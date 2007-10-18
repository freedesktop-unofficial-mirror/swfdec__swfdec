/* Swfdec
 * Copyright (C) 2007 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "swfdec_as_context.h"
#include "swfdec_as_array.h"
#include "swfdec_as_frame_internal.h"
#include "swfdec_as_function.h"
#include "swfdec_as_initialize.h"
#include "swfdec_as_internal.h"
#include "swfdec_as_interpret.h"
#include "swfdec_as_native_function.h"
#include "swfdec_as_object.h"
#include "swfdec_as_stack.h"
#include "swfdec_as_strings.h"
#include "swfdec_as_types.h"
#include "swfdec_debug.h"
#include "swfdec_internal.h" /* for swfdec_player_preinit_global() */
#include "swfdec_script.h"

/*** GARBAGE COLLECTION DOCS ***/

#define SWFDEC_AS_GC_MARK (1 << 0)		/* only valid during GC */
#define SWFDEC_AS_GC_ROOT (1 << 1)		/* for objects: rooted, for strings: static */

/**
 * SECTION:Internals
 * @title: Internals and garbage collection
 * @short_description: understanding internals such as garbage collection
 * @see_also: #SwfdecAsContext
 *
 * This section deals with the internals of the Swfdec Actionscript engine. You
 * should probably read this first when trying to write code with it. If you're
 * just trying to use Swfdec for creating Flash content, you can probably skip
 * this section.
 *
 * First, I'd like to note that the Swfdec script engine has to be modeled very 
 * closely after the existing Flash players. So if there are some behaviours
 * that seem stupid at first sight, it might very well be that it was chosen for
 * a very particular reason. Now on to the features.
 *
 * The Swfdec script engine tries to imitate Actionscript as good as possible.
 * Actionscript is similar to Javascript, but not equal. Depending on the 
 * version of the script executed it might be more or less similar. An important
 * example is that Flash in versions up to 6 did case-insensitive variable 
 * lookups.
 *
 * The script engine starts its life when it is initialized via 
 * swfdec_as_context_startup (). At that point, the basic objects are created.
 * After this function has been called, you can start executing code. All code
 * execution happens by creating a new #SwfdecAsFrame and then calling 
 * swfdec_as_context_run() to execute it. This function is the single entry 
 * point for code execution. Convenience functions exist that make executing 
 * code easy, most notably swfdec_as_object_run() and 
 * swfdec_as_object_call().
 *
 * It is also easily possible to extend the environment by adding new objects.
 * In fact, without doing so, the environment is pretty bare as it just contains
 * the basic Math, String, Number, Array, Date and Boolean objects. This is done
 * by adding #SwfdecAsNative functions to the environment. The easy way
 * to do this is via swfdec_as_object_add_function().
 *
 * The Swfdec script engine is dynamically typed and knows about different types
 * of values. See #SwfdecAsValue for the different values. Memory management is
 * done using a mark and sweep garbage collector. You can initiate a garbage 
 * collection cycle by calling swfdec_as_context_gc() or 
 * swfdec_as_context_maybe_gc(). You should do this regularly to avoid excessive
 * memory use. The #SwfdecAsContext will then collect the objects and strings it
 * is keeping track of. If you want to use an object or string in the script 
 * engine, you'll have to add it first via swfdec_as_object_add() or
 * swfdec_as_context_get_string() and swfdec_as_context_give_string(), 
 * respectively.
 *
 * Garbage-collected strings are special in Swfdec in that they are unique. This
 * means the same string exists exactly once. Because of this you can do 
 * equality comparisons using == instead of strcmp. It also means that if you 
 * forget to add a string to the context before using it, your app will most 
 * likely not work, since the string will not compare equal to any other string.
 *
 * When a garbage collection cycle happens, all reachable objects and strings 
 * are marked and all unreachable ones are freed. This is done by calling the
 * context class's mark function which will mark all reachable objects. This is
 * usually called the "root set". For any reachable object, the object's mark
 * function is called so that the object in turn can mark all objects it can 
 * reach itself. Marking is done via functions described below.
 */

/*** GTK-DOC ***/

/**
 * SECTION:SwfdecAsContext
 * @title: SwfdecAsContext
 * @short_description: the main script engine context
 * @see_also: SwfdecPlayer
 *
 * A #SwfdecAsContext provides the main execution environment for Actionscript
 * execution. It provides the objects typically available in ECMAScript and
 * manages script execution, garbage collection etc. #SwfdecPlayer is a
 * subclass of the context that implements Flash specific objects on top of it.
 * However, it is possible to use the context for completely different functions
 * where a sandboxed scripting environment is needed. An example is the Swfdec 
 * debugger.
 * <note>The Actionscript engine is similar, but not equal to Javascript. It
 * is not very different, but it is different.</note>
 */

/**
 * SwfdecAsContext
 *
 * This is the main object ued to hold the state of a script engine. All members 
 * are private and should not be accessed.
 *
 * Subclassing this structure to get scripting support in your own appliation is
 * encouraged.
 */

/**
 * SwfdecAsContextState
 * @SWFDEC_AS_CONTEXT_NEW: the context is not yet initialized, 
 *                         swfdec_as_context_startup() needs to be called.
 * @SWFDEC_AS_CONTEXT_RUNNING: the context is running normally
 * @SWFDEC_AS_CONTEXT_INTERRUPTED: the context has been interrupted by a 
 *                             debugger
 * @SWFDEC_AS_CONTEXT_ABORTED: the context has aborted execution due to a 
 *                         fatal error
 *
 * The state of the context describes what operations are possible on the context.
 * It will be in the state @SWFDEC_AS_CONTEXT_STATE_RUNNING almost all the time. If
 * it is in the state @SWFDEC_AS_CONTEXT_STATE_ABORTED, it will not work anymore and
 * every operation on it will instantly fail.
 */

/*** RUNNING STATE ***/

/**
 * swfdec_as_context_abort:
 * @context: a #SwfdecAsContext
 * @reason: a string describing why execution was aborted
 *
 * Aborts script execution in @context. Call this functon if the script engine 
 * encountered a fatal error and cannot continue. A possible reason for this is
 * an out-of-memory condition.
 **/
void
swfdec_as_context_abort (SwfdecAsContext *context, const char *reason)
{
  g_return_if_fail (context);

  SWFDEC_ERROR ("%s", reason);
  context->state = SWFDEC_AS_CONTEXT_ABORTED;
}

/*** MEMORY MANAGEMENT ***/

/**
 * swfdec_as_context_use_mem:
 * @context: a #SwfdecAsContext
 * @bytes: number of bytes to use
 *
 * Registers @bytes additional bytes as in use by the @context. This function
 * keeps track of the memory that script code consumes. If too many memory is 
 * in use, this function may decide to stop the script engine with an out of 
 * memory error.
 *
 * Returns: %TRUE if the memory could be allocated. %FALSE on OOM.
 **/
gboolean
swfdec_as_context_use_mem (SwfdecAsContext *context, gsize bytes)
{
  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), FALSE);
  g_return_val_if_fail (bytes > 0, FALSE);

  if (context->state == SWFDEC_AS_CONTEXT_ABORTED)
    return FALSE;
  
  context->memory += bytes;
  context->memory_since_gc += bytes;
  SWFDEC_LOG ("+%4"G_GSIZE_FORMAT" bytes, total %7"G_GSIZE_FORMAT" (%7"G_GSIZE_FORMAT" since GC)",
      bytes, context->memory, context->memory_since_gc);
  /* FIXME: Don't foget to abort on OOM */
  return TRUE;
}

/**
 * swfdec_as_context_unuse_mem:
 * @context: a #SwfdecAsContext
 * @bytes: number of bytes to release
 *
 * Releases a number of bytes previously allocated using 
 * swfdec_as_context_use_mem(). See that function for details.
 **/
void
swfdec_as_context_unuse_mem (SwfdecAsContext *context, gsize bytes)
{
  g_return_if_fail (SWFDEC_IS_AS_CONTEXT (context));
  g_return_if_fail (bytes > 0);
  g_return_if_fail (context->memory >= bytes);

  context->memory -= bytes;
  SWFDEC_LOG ("-%4"G_GSIZE_FORMAT" bytes, total %7"G_GSIZE_FORMAT" (%7"G_GSIZE_FORMAT" since GC)",
      bytes, context->memory, context->memory_since_gc);
}

/*** GC ***/

static gboolean
swfdec_as_context_remove_strings (gpointer key, gpointer value, gpointer data)
{
  SwfdecAsContext *context = data;
  char *string;

  string = (char *) value;
  /* it doesn't matter that rooted strings aren't destroyed, they're constant */
  if (string[0] & SWFDEC_AS_GC_ROOT) {
    SWFDEC_LOG ("rooted: %s", (char *) key);
    return FALSE;
  } else if (string[0] & SWFDEC_AS_GC_MARK) {
    SWFDEC_LOG ("marked: %s", (char *) key);
    string[0] &= ~SWFDEC_AS_GC_MARK;
    return FALSE;
  } else {
    gsize len;
    SWFDEC_LOG ("deleted: %s", (char *) key);
    len = (strlen ((char *) key) + 2);
    swfdec_as_context_unuse_mem (context, len);
    g_slice_free1 (len, value);
    return TRUE;
  }
}

static gboolean
swfdec_as_context_remove_objects (gpointer key, gpointer value, gpointer debugger)
{
  SwfdecAsObject *object;

  object = key;
  /* we only check for mark here, not root, since this works on destroy, too */
  if (object->flags & SWFDEC_AS_GC_MARK) {
    object->flags &= ~SWFDEC_AS_GC_MARK;
    SWFDEC_LOG ("%s: %s %p", (object->flags & SWFDEC_AS_GC_ROOT) ? "rooted" : "marked",
	G_OBJECT_TYPE_NAME (object), object);
    return FALSE;
  } else {
    SWFDEC_LOG ("deleted: %s %p", G_OBJECT_TYPE_NAME (object), object);
    if (debugger) {
      SwfdecAsDebuggerClass *klass = SWFDEC_AS_DEBUGGER_GET_CLASS (debugger);
      if (klass->remove)
	klass->remove (debugger, object->context, object);
    }
    swfdec_as_object_collect (object);
    return TRUE;
  }
}

static void
swfdec_as_context_collect (SwfdecAsContext *context)
{
  SWFDEC_INFO (">> collecting garbage");
  /* NB: This functions is called without GC from swfdec_as_context_dispose */
  g_hash_table_foreach_remove (context->strings, 
    swfdec_as_context_remove_strings, context);
  g_hash_table_foreach_remove (context->objects, 
    swfdec_as_context_remove_objects, context->debugger);
  SWFDEC_INFO (">> done collecting garbage");
}

/**
 * swfdec_as_object_mark:
 * @object: a #SwfdecAsObject
 *
 * Mark @object as being in use. Calling this function is only valid during
 * the marking phase of garbage collection.
 **/
void
swfdec_as_object_mark (SwfdecAsObject *object)
{
  SwfdecAsObjectClass *klass;

  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));

  if (object->flags & SWFDEC_AS_GC_MARK)
    return;
  object->flags |= SWFDEC_AS_GC_MARK;
  klass = SWFDEC_AS_OBJECT_GET_CLASS (object);
  g_assert (klass->mark);
  klass->mark (object);
}

/**
 * swfdec_as_string_mark:
 * @string: a garbage-collected string
 *
 * Mark @string as being in use. Calling this function is only valid during
 * the marking phase of garbage collection.
 **/
void
swfdec_as_string_mark (const char *string)
{
  char *str;

  g_return_if_fail (string != NULL);

  str = (char *) string - 1;
  if (*str == 0)
    *str = SWFDEC_AS_GC_MARK;
}

/**
 * swfdec_as_value_mark:
 * @value: a #SwfdecAsValue
 *
 * Mark @value as being in use. This is just a convenience function that calls
 * the right marking function depending on the value's type. Calling this 
 * function is only valid during the marking phase of garbage collection.
 **/
void
swfdec_as_value_mark (SwfdecAsValue *value)
{
  g_return_if_fail (SWFDEC_IS_AS_VALUE (value));

  if (SWFDEC_AS_VALUE_IS_OBJECT (value)) {
    swfdec_as_object_mark (SWFDEC_AS_VALUE_GET_OBJECT (value));
  } else if (SWFDEC_AS_VALUE_IS_STRING (value)) {
    swfdec_as_string_mark (SWFDEC_AS_VALUE_GET_STRING (value));
  }
}

static void
swfdec_as_context_mark_roots (gpointer key, gpointer value, gpointer data)
{
  SwfdecAsObject *object = key;

  if ((object->flags & (SWFDEC_AS_GC_MARK | SWFDEC_AS_GC_ROOT)) == SWFDEC_AS_GC_ROOT)
    swfdec_as_object_mark (object);
}

static void
swfdec_as_context_do_mark (SwfdecAsContext *context)
{
  swfdec_as_object_mark (context->global);
  swfdec_as_object_mark (context->Function);
  swfdec_as_object_mark (context->Function_prototype);
  swfdec_as_object_mark (context->Object);
  swfdec_as_object_mark (context->Object_prototype);
  g_hash_table_foreach (context->objects, swfdec_as_context_mark_roots, NULL);
}

/**
 * swfdec_as_context_gc:
 * @context: a #SwfdecAsContext
 *
 * Calls the Swfdec Gargbage collector and reclaims any unused memory. You 
 * should call this function or swfdec_as_context_maybe_gc() regularly.
 * <warning>Calling the GC during execution of code or initialization is not
 *          allowed.</warning>
 **/
void
swfdec_as_context_gc (SwfdecAsContext *context)
{
  SwfdecAsContextClass *klass;

  g_return_if_fail (SWFDEC_IS_AS_CONTEXT (context));
  g_return_if_fail (context->frame == NULL);
  g_return_if_fail (context->state == SWFDEC_AS_CONTEXT_RUNNING);

  if (context->state == SWFDEC_AS_CONTEXT_ABORTED)
    return;
  SWFDEC_INFO ("invoking the garbage collector");
  klass = SWFDEC_AS_CONTEXT_GET_CLASS (context);
  g_assert (klass->mark);
  klass->mark (context);
  swfdec_as_context_collect (context);
  context->memory_since_gc = 0;
}

static gboolean
swfdec_as_context_needs_gc (SwfdecAsContext *context)
{
  return context->memory_since_gc >= context->memory_until_gc;
}

/**
 * swfdec_as_context_maybe_gc:
 * @context: a #SwfdecAsContext
 *
 * Calls the garbage collector if necessary. It's a good idea to call this
 * function regularly instead of swfdec_as_context_gc() as it only does collect
 * garage as needed. For example, #SwfdecPlayer calls this function after every
 * frame advancement.
 **/
void
swfdec_as_context_maybe_gc (SwfdecAsContext *context)
{
  g_return_if_fail (SWFDEC_IS_AS_CONTEXT (context));
  g_return_if_fail (context->state == SWFDEC_AS_CONTEXT_RUNNING);
  g_return_if_fail (context->frame == NULL);

  if (swfdec_as_context_needs_gc (context))
    swfdec_as_context_gc (context);
}

/*** SWFDEC_AS_CONTEXT ***/

enum {
  TRACE,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_DEBUGGER,
  PROP_UNTIL_GC
};

G_DEFINE_TYPE (SwfdecAsContext, swfdec_as_context, G_TYPE_OBJECT)
static guint signals[LAST_SIGNAL] = { 0, };

static void
swfdec_as_context_get_property (GObject *object, guint param_id, GValue *value, 
    GParamSpec * pspec)
{
  SwfdecAsContext *context = SWFDEC_AS_CONTEXT (object);

  
  switch (param_id) {
    case PROP_DEBUGGER:
      g_value_set_object (value, context->debugger);
      break;
    case PROP_UNTIL_GC:
      g_value_set_ulong (value, (gulong) context->memory_until_gc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
swfdec_as_context_set_property (GObject *object, guint param_id, const GValue *value, 
    GParamSpec * pspec)
{
  SwfdecAsContext *context = SWFDEC_AS_CONTEXT (object);

  
  switch (param_id) {
    case PROP_DEBUGGER:
      context->debugger = SWFDEC_AS_DEBUGGER (g_value_dup_object (value));
      break;
    case PROP_UNTIL_GC:
      context->memory_until_gc = g_value_get_ulong (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
swfdec_as_context_dispose (GObject *object)
{
  SwfdecAsContext *context = SWFDEC_AS_CONTEXT (object);

  while (context->stack)
    swfdec_as_stack_pop_segment (context);
  swfdec_as_context_collect (context);
  if (context->memory != 0) {
    g_critical ("%zu bytes of memory left over\n", context->memory);
  }
  g_assert (g_hash_table_size (context->objects) == 0);
  g_hash_table_destroy (context->objects);
  g_hash_table_destroy (context->strings);
  g_rand_free (context->rand);
  if (context->debugger) {
    g_object_unref (context->debugger);
    context->debugger = NULL;
  }

  G_OBJECT_CLASS (swfdec_as_context_parent_class)->dispose (object);
}

static void
swfdec_as_context_class_init (SwfdecAsContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_as_context_dispose;
  object_class->get_property = swfdec_as_context_get_property;
  object_class->set_property = swfdec_as_context_set_property;

  g_object_class_install_property (object_class, PROP_DEBUGGER,
      g_param_spec_object ("debugger", "debugger", "debugger used in this player",
	  SWFDEC_TYPE_AS_DEBUGGER, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_UNTIL_GC,
      g_param_spec_ulong ("memory-until-gc", "memory until gc", 
	  "amount of bytes that need to be allocated before garbage collection triggers",
	  0, G_MAXULONG, 8 * 1024 * 1024, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * SwfdecAsContext::trace:
   * @context: the #SwfdecAsContext affected
   * @text: the debugging string
   *
   * Emits a debugging string while running. The effect of calling any swfdec 
   * functions on the emitting @context is undefined.
   */
  signals[TRACE] = g_signal_new ("trace", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__STRING,
      G_TYPE_NONE, 1, G_TYPE_STRING);

  klass->mark = swfdec_as_context_do_mark;
}

static void
swfdec_as_context_init (SwfdecAsContext *context)
{
  const char *s;

  context->strings = g_hash_table_new (g_str_hash, g_str_equal);
  context->objects = g_hash_table_new (g_direct_hash, g_direct_equal);

  for (s = swfdec_as_strings; *s == '\2'; s += strlen (s) + 1) {
    g_hash_table_insert (context->strings, (char *) s + 1, (char *) s);
  }
  g_assert (*s == 0);
  context->global = swfdec_as_object_new_empty (context);
  context->rand = g_rand_new ();
  g_get_current_time (&context->start_time);
}

/*** STRINGS ***/

static const char *
swfdec_as_context_create_string (SwfdecAsContext *context, const char *string, gsize len)
{
  char *new;
  
  if (!swfdec_as_context_use_mem (context, sizeof (char) * (2 + len)))
    return SWFDEC_AS_STR_EMPTY;

  new = g_slice_alloc (2 + len);
  memcpy (&new[1], string, len);
  new[len + 1] = 0;
  new[0] = 0; /* GC flags */
  g_hash_table_insert (context->strings, new + 1, new);

  return new + 1;
}

/**
 * swfdec_as_context_get_string:
 * @context: a #SwfdecAsContext
 * @string: a sting that is not garbage-collected
 *
 * Gets the garbage-collected version of @string. You need to call this function
 * for every not garbage-collected string that you want to use in Swfdecs script
 * interpreter.
 *
 * Returns: the garbage-collected version of @string
 **/
const char *
swfdec_as_context_get_string (SwfdecAsContext *context, const char *string)
{
  const char *ret;
  gsize len;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), NULL);
  g_return_val_if_fail (string != NULL, NULL);

  if (g_hash_table_lookup_extended (context->strings, string, (gpointer) &ret, NULL))
    return ret;

  len = strlen (string);
  return swfdec_as_context_create_string (context, string, len);
}

/**
 * swfdec_as_context_give_string:
 * @context: a #SwfdecAsContext
 * @string: string to make refcounted
 *
 * Takes ownership of @string and returns a refcounted version of the same 
 * string. This function is the same as swfdec_as_context_get_string(), but 
 * takes ownership of @string.
 *
 * Returns: A refcounted string
 **/
const char *
swfdec_as_context_give_string (SwfdecAsContext *context, char *string)
{
  const char *ret;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), NULL);
  g_return_val_if_fail (string != NULL, NULL);

  ret = swfdec_as_context_get_string (context, string);
  g_free (string);
  return ret;
}

/**
 * swfdec_as_context_is_constructing:
 * @context: a #SwfdecAsConstruct
 *
 * Determines if the contexxt is currently constructing. This information is
 * used by various constructors to do different things when they are 
 * constructing and when they are not. The Boolean, Number and String functions
 * for example setup the newly constructed objects when constructing but only
 * cast the provided argument when being called.
 *
 * Returns: %TRUE if the currently executing frame is a constructor
 **/
gboolean
swfdec_as_context_is_constructing (SwfdecAsContext *context)
{
  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), FALSE);

  return context->frame && context->frame->construct;
}

/**
 * swfdec_as_context_get_frame:
 * @context: a #SwfdecAsContext
 *
 * This is a debugging function. It gets the topmost stack frame that is 
 * currently executing. If no function is executing, %NULL is returned. You can
 * easily get a backtrace with code like this:
 * |[for (frame = swfdec_as_context_get_frame (context); frame != NULL; 
 *     frame = swfdec_as_frame_get_next (frame)) {
 *   char *s = swfdec_as_object_get_debug (SWFDEC_AS_OBJECT (frame));
 *   g_print ("%s\n", s);
 *   g_free (s);
 * }]|
 *
 * Returns: the currently executing frame or %NULL if none
 **/
SwfdecAsFrame *
swfdec_as_context_get_frame (SwfdecAsContext *context)
{
  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), NULL);

  return context->frame;
}

/**
 * swfdec_as_context_get_time:
 * @context: a #SwfdecAsContext
 * @tv: a #GTimeVal to be set to the context's time
 *
 * This function queries the time to be used inside this context. By default,
 * this is the same as g_get_current_time(), but it may be overwriten to allow
 * things such as slower or faster playback.
 **/
void
swfdec_as_context_get_time (SwfdecAsContext *context, GTimeVal *tv)
{
  SwfdecAsContextClass *klass;

  g_return_if_fail (SWFDEC_IS_AS_CONTEXT (context));
  g_return_if_fail (tv != NULL);

  klass = SWFDEC_AS_CONTEXT_GET_CLASS (context);
  if (klass->get_time)
    klass->get_time (context, tv);
  else
    g_get_current_time (tv);
}

/**
 * swfdec_as_context_run:
 * @context: a #SwfdecAsContext
 *
 * Continues running the script engine. Executing code in this engine works
 * in 2 steps: First, you push the frame to be executed onto the stack, then
 * you call this function to execute it. So this function is the single entry
 * point to script execution. This might be helpful when debugging your 
 * application. 
 * <note>A lot of convenience functions like swfdec_as_object_run() call this 
 * function automatically.</note>
 **/
void
swfdec_as_context_run (SwfdecAsContext *context)
{
  SwfdecAsFrame *frame, *last_frame;
  SwfdecScript *script;
  const SwfdecActionSpec *spec;
  SwfdecActionExec exec;
  const guint8 *startpc, *pc, *endpc, *nextpc, *exitpc;
#ifndef G_DISABLE_ASSERT
  SwfdecAsValue *check;
#endif
  guint action, len;
  const guint8 *data;
  int version;
  guint original_version;
  void (* step) (SwfdecAsDebugger *debugger, SwfdecAsContext *context);
  gboolean check_block; /* some opcodes avoid a scope check */

  g_return_if_fail (SWFDEC_IS_AS_CONTEXT (context));
  if (context->frame == NULL || context->state == SWFDEC_AS_CONTEXT_ABORTED)
    return;

  if (context->debugger) {
    SwfdecAsDebuggerClass *klass = SWFDEC_AS_DEBUGGER_GET_CLASS (context->debugger);
    step = klass->step;
  } else {
    step = NULL;
  }

  last_frame = context->last_frame;
  context->last_frame = context->frame->next;
  original_version = context->version;
start:
  /* setup data */
  frame = context->frame;
  if (frame == context->last_frame)
    goto out;
  if (context->call_depth > 256) {
    /* we've exceeded our maximum call depth, throw an error and abort */
    swfdec_as_context_abort (context, "Stack overflow");
    return;
  }
  /* if security is NULL, the function may not be called */
  if (frame->security == NULL) {
    SWFDEC_WARNING ("insufficient right to call %s", frame->function_name);
    swfdec_as_frame_return (frame, NULL);
    goto start;
  }
  if (SWFDEC_IS_AS_NATIVE_FUNCTION (frame->function)) {
    SwfdecAsNativeFunction *native = SWFDEC_AS_NATIVE_FUNCTION (frame->function);
    SwfdecAsValue rval = { 0, };
    if (frame->argc >= native->min_args && 
	(native->type == 0 || 
	 g_type_is_a (G_OBJECT_TYPE (frame->thisp), native->type))) {
      SwfdecAsValue *argv;
      /* accumulate argv */
      if (frame->argc == 0 || frame->argv != NULL) {
	/* FIXME FIXME FIXME: no casting here please! */
	argv = (SwfdecAsValue *) frame->argv;
      } else {
	SwfdecAsStack *stack;
	SwfdecAsValue *cur;
	guint i;
	if (frame->argc > 128) {
	  SWFDEC_FIXME ("allow calling native functions with more than 128 args (this one has %u)",
	      frame->argc);
	  frame->argc = 128;
	}
	argv = g_new (SwfdecAsValue, frame->argc);
	stack = context->stack;
	cur = context->cur;
	for (i = 0; i < frame->argc; i++) {
	  if (cur <= &stack->elements[0]) {
	    stack = stack->next;
	    cur = &stack->elements[stack->used_elements];
	  }
	  cur--;
	  argv[i] = *cur;
	}
      }
      native->native (context, frame->thisp, frame->argc, 
	  argv, &rval);
      if (argv != frame->argv)
	g_free (argv);
    }
    swfdec_as_frame_return (frame, &rval);
    goto start;
  }
  g_assert (frame->script);
  g_assert (frame->target);
  script = frame->script;
  version = SWFDEC_AS_EXTRACT_SCRIPT_VERSION (script->version);
  context->version = script->version;
  startpc = script->buffer->data;
  endpc = startpc + script->buffer->length;
  exitpc = script->exit;
  pc = frame->pc;
  check_block = TRUE;

  while (context->state < SWFDEC_AS_CONTEXT_ABORTED) {
    if (check_block && (pc < frame->block_start || pc >= frame->block_end)) {
      SWFDEC_LOG ("code exited block");
      swfdec_as_frame_check_block (frame);
      pc = frame->pc;
      if (frame != context->frame)
	goto start;
    }
    if (pc == exitpc) {
      swfdec_as_frame_return (frame, NULL);
      goto start;
    }
    if (pc < startpc || pc >= endpc) {
      SWFDEC_ERROR ("pc %p not in valid range [%p, %p) anymore", pc, startpc, endpc);
      goto error;
    }

    /* decode next action */
    action = *pc;
    if (action == 0) {
      swfdec_as_frame_return (frame, NULL);
      goto start;
    }
    /* invoke debugger if there is one */
    if (step) {
      frame->pc = pc;
      (* step) (context->debugger, context);
      if (frame != context->frame || 
	  frame->pc != pc) {
	goto start;
      }
    }
    /* prepare action */
    spec = swfdec_as_actions + action;
    if (action & 0x80) {
      if (pc + 2 >= endpc) {
	SWFDEC_ERROR ("action %u length value out of range", action);
	goto error;
      }
      data = pc + 3;
      len = pc[1] | pc[2] << 8;
      if (data + len > endpc) {
	SWFDEC_ERROR ("action %u length %u out of range", action, len);
	goto error;
      }
      nextpc = pc + 3 + len;
    } else {
      data = NULL;
      len = 0;
      nextpc = pc + 1;
    }
    /* check action is valid */
    exec = spec->exec[version];
    if (!exec) {
      guint real_version;
      for (real_version = version + 1; !exec && 
	  real_version <= SWFDEC_AS_MAX_SCRIPT_VERSION - SWFDEC_AS_MIN_SCRIPT_VERSION; 
	  real_version++) {
	exec = spec->exec[real_version];
      }
      if (!exec) {
	SWFDEC_WARNING ("cannot interpret action %3u 0x%02X %s for version %u, skipping it", action,
	    action, spec->name ? spec->name : "Unknown", script->version);
	frame->pc = pc = nextpc;
	check_block = TRUE;
	continue;
      }
      SWFDEC_WARNING ("cannot interpret action %3u 0x%02X %s for version %u, using version %u instead", 
	  action, action, spec->name ? spec->name : "Unknown", script->version, 
	  script->version + real_version - version);
    }
    if (spec->remove > 0) {
      if (spec->add > spec->remove)
	swfdec_as_stack_ensure_free (context, spec->add - spec->remove);
      swfdec_as_stack_ensure_size (context, spec->remove);
    } else {
      if (spec->add > 0)
	swfdec_as_stack_ensure_free (context, spec->add);
    }
    if (context->state > SWFDEC_AS_CONTEXT_RUNNING) {
      SWFDEC_WARNING ("context not running anymore, aborting");
      goto error;
    }
#ifndef G_DISABLE_ASSERT
    check = (spec->add >= 0 && spec->remove >= 0) ? context->cur + spec->add - spec->remove : NULL;
#endif
    /* execute action */
    exec (context, action, data, len);
    /* adapt the pc if the action did not, otherwise, leave it alone */
    /* FIXME: do this via flag? */
    if (frame->pc == pc) {
      frame->pc = pc = nextpc;
      check_block = TRUE;
    } else {
      pc = frame->pc;
      check_block = FALSE;
    }
    if (frame == context->frame) {
#ifndef G_DISABLE_ASSERT
      if (check != NULL && check != context->cur) {
	g_error ("action %s was supposed to change the stack by %d (+%d -%d), but it changed by %td",
	    spec->name, spec->add - spec->remove, spec->add, spec->remove,
	    context->cur - check + spec->add - spec->remove);
      }
#endif
    } else {
      /* someone called/returned from a function, reread variables */
      goto start;
    }
  }

error:
  while (context->frame != context->last_frame)
    swfdec_as_frame_return (context->frame, NULL);
out:
  context->last_frame = last_frame;
  context->version = original_version;
  return;
}

/*** EVAL ***/

static char *
swfdec_as_slash_to_dot (const char *slash_str)
{
  const char *cur = slash_str;
  GString *str = g_string_new ("");

  if (*cur == '/') {
    g_string_append (str, "_root");
  } else {
    goto start;
  }
  while (cur && *cur == '/') {
    cur++;
start:
    if (str->len > 0)
      g_string_append_c (str, '.');
    if (cur[0] == '.' && cur[1] == '.') {
      g_string_append (str, "_parent");
      cur += 2;
    } else {
      char *slash = strchr (cur, '/');
      if (slash) {
	g_string_append_len (str, cur, slash - cur);
	cur = slash;
      } else {
	g_string_append (str, cur);
	cur = NULL;
      }
    }
    /* cur should now point to the slash */
  }
  if (cur) {
    if (*cur != '\0')
      goto fail;
  }
  SWFDEC_DEBUG ("parsed slash-notated string \"%s\" into dot notation \"%s\"",
      slash_str, str->str);
  return g_string_free (str, FALSE);

fail:
  SWFDEC_WARNING ("failed to parse slash-notated string \"%s\" into dot notation", slash_str);
  g_string_free (str, TRUE);
  return NULL;
}

static void
swfdec_as_context_eval_get_property (SwfdecAsContext *cx, 
    SwfdecAsObject *obj, const char *name, SwfdecAsValue *ret)
{
  if (obj) {
    swfdec_as_object_get_variable (obj, name, ret);
  } else {
    if (cx->frame) {
      swfdec_as_frame_get_variable (cx->frame, name, ret);
    } else {
      SWFDEC_WARNING ("eval called without a frame");
      swfdec_as_object_get_variable (cx->global, name, ret);
    }
  }
}

static void
swfdec_as_context_eval_set_property (SwfdecAsContext *cx, 
    SwfdecAsObject *obj, const char *name, const SwfdecAsValue *ret)
{
  if (obj == NULL) {
    if (cx->frame == NULL) {
      SWFDEC_ERROR ("no frame in eval_set?");
      return;
    }
    swfdec_as_frame_set_variable (cx->frame, name, ret);
  } else {
    swfdec_as_object_set_variable (obj, name, ret);
  }
}

static void
swfdec_as_context_eval_internal (SwfdecAsContext *cx, SwfdecAsObject *obj, const char *str,
        SwfdecAsValue *val, gboolean set)
{
  SwfdecAsValue cur;
  char **varlist;
  guint i;

  SWFDEC_LOG ("eval called with \"%s\" on %p", str, obj);
  if (strchr (str, '/')) {
    char *work = swfdec_as_slash_to_dot (str);
    if (!work) {
      SWFDEC_AS_VALUE_SET_UNDEFINED (val);
      return;
    }
    varlist = g_strsplit (work, ".", -1);
    g_free (work);
  } else {
    varlist = g_strsplit (str, ".", -1);
  }
  for (i = 0; varlist[i] != NULL; i++) {
    const char *dot = swfdec_as_context_get_string (cx, varlist[i]);
    if (varlist[i+1] != NULL) {
      swfdec_as_context_eval_get_property (cx, obj, dot, &cur);
      if (!SWFDEC_AS_VALUE_IS_OBJECT (&cur)) {
	SWFDEC_AS_VALUE_SET_UNDEFINED (&cur);
	break;
      }
      obj = SWFDEC_AS_VALUE_GET_OBJECT (&cur);
    } else {
      if (set) {
	swfdec_as_context_eval_set_property (cx, obj, dot, val);
      } else {
	swfdec_as_context_eval_get_property (cx, obj, dot, &cur);
      }
      goto finish;
    }
  }
  if (obj == NULL) {
    if (cx->frame)
      obj = cx->frame->target;
    else
      obj = cx->global;
  }
  g_assert (obj != NULL);
  SWFDEC_AS_VALUE_SET_OBJECT (&cur, obj);

finish:
  g_strfreev (varlist);
  *val = cur;
}

/**
 * swfdec_as_context_eval:
 * @context: a #SwfdecAsContext
 * @obj: #SwfdecAsObject to use as source for evaluating or NULL for the 
 *       current frame's scope
 * @str: The string to evaluate
 * @val: location for the return value
 *
 * This function works like the Actionscript eval function used on @obj.
 * It handles both slash-style and dot-style notation. If an error occured
 * during evaluation, the return value will be the undefined value.
 **/
void
swfdec_as_context_eval (SwfdecAsContext *context, SwfdecAsObject *obj, const char *str, 
    SwfdecAsValue *val)
{
  g_return_if_fail (SWFDEC_IS_AS_CONTEXT (context));
  g_return_if_fail (obj == NULL || SWFDEC_IS_AS_OBJECT (obj));
  g_return_if_fail (str != NULL);
  g_return_if_fail (val != NULL);

  swfdec_as_context_eval_internal (context, obj, str, val, FALSE);
}

/**
 * swfdec_as_context_eval_set:
 * @context: a #SwfdecAsContext
 * @obj: #SwfdecAsObject to use as source for evaluating or NULL for the 
 *       default object.
 * @str: The string to evaluate
 * @val: the value to set the variable to
 *
 * Sets the variable referenced by @str to @val. If @str does not reference 
 * a valid property, nothing happens.
 **/
void
swfdec_as_context_eval_set (SwfdecAsContext *context, SwfdecAsObject *obj, const char *str,
    const SwfdecAsValue *val)
{
  g_return_if_fail (SWFDEC_IS_AS_CONTEXT (context));
  g_return_if_fail (obj == NULL || SWFDEC_IS_AS_OBJECT (obj));
  g_return_if_fail (str != NULL);
  g_return_if_fail (val != NULL);

  swfdec_as_context_eval_internal (context, obj, str, (SwfdecAsValue *) val, TRUE);
}

/*** AS CODE ***/

static void
swfdec_as_context_ASSetPropFlags_set_one_flag (SwfdecAsObject *object,
    const char *s, guint *flags)
{
  swfdec_as_object_unset_variable_flags (object, s, flags[1]);
  swfdec_as_object_set_variable_flags (object, s, flags[0]);
}

static gboolean
swfdec_as_context_ASSetPropFlags_foreach (SwfdecAsObject *object,
    const char *s, SwfdecAsValue *val, guint cur_flags, gpointer data)
{
  guint *flags = data;

  /* shortcut if the flags already match */
  if (cur_flags == ((cur_flags &~ flags[1]) | flags[0]))
    return TRUE;

  swfdec_as_context_ASSetPropFlags_set_one_flag (object, s, flags);
  return TRUE;
}

static void
swfdec_as_context_ASSetPropFlags (SwfdecAsContext *cx, SwfdecAsObject *object, 
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *retval)
{
  guint flags[2]; /* flags and mask - array so we can pass it as data pointer */
  SwfdecAsObject *obj;

  if (!SWFDEC_AS_VALUE_IS_OBJECT (&argv[0]))
    return;
  obj = SWFDEC_AS_VALUE_GET_OBJECT (&argv[0]);
  flags[0] = swfdec_as_value_to_integer (cx, &argv[2]);
  flags[1] = (argc > 3) ? swfdec_as_value_to_integer (cx, &argv[3]) : 0;

  if (flags[0] == 0 && flags[1] == 0) {
    // we should add autosizing length attribute here
    SWFDEC_FIXME ("ASSetPropFlags to set special length attribute not implemented");
    return;
  }

  if (SWFDEC_AS_VALUE_IS_NULL (&argv[1])) {
    swfdec_as_object_foreach (obj, swfdec_as_context_ASSetPropFlags_foreach, flags);
  } else {
    char **split =
      g_strsplit (swfdec_as_value_to_string (cx, &argv[1]), ",", -1);
    guint i;
    for (i = 0; split[i]; i++) {
      swfdec_as_context_ASSetPropFlags_set_one_flag (obj, 
	  swfdec_as_context_get_string (cx, split[i]), flags);
    }
    g_strfreev (split); 
  }
}

SWFDEC_AS_NATIVE (200, 19, swfdec_as_context_isFinite)
void
swfdec_as_context_isFinite (SwfdecAsContext *cx, SwfdecAsObject *object, 
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *retval)
{
  double d;

  if (argc < 1)
    return;

  d = swfdec_as_value_to_number (cx, &argv[0]);
  SWFDEC_AS_VALUE_SET_BOOLEAN (retval, isfinite (d) ? TRUE : FALSE);
}

SWFDEC_AS_NATIVE (200, 18, swfdec_as_context_isNaN)
void
swfdec_as_context_isNaN (SwfdecAsContext *cx, SwfdecAsObject *object, 
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *retval)
{
  double d;

  if (argc < 1)
    return;

  d = swfdec_as_value_to_number (cx, &argv[0]);
  SWFDEC_AS_VALUE_SET_BOOLEAN (retval, isnan (d) ? TRUE : FALSE);
}

SWFDEC_AS_NATIVE (100, 2, swfdec_as_context_parseInt)
void
swfdec_as_context_parseInt (SwfdecAsContext *cx, SwfdecAsObject *object, 
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *retval)
{
  const char *s;
  char *tail;
  int radix;
  gint64 i;

  if (argc < 1)
    return;

  s = swfdec_as_value_to_string (cx, &argv[0]);

  if (argc >= 2) {
    radix = swfdec_as_value_to_integer (cx, &argv[1]);

    if (radix < 2 || radix > 36) {
      SWFDEC_AS_VALUE_SET_NUMBER (retval, NAN);
      return;
    }

    // special case, strtol parses things that we shouldn't parse
    if (radix == 16) {
      const char *end = s + strspn (s, " \t\r\n");
      if (end != s && end[0] == '0' && end[1] == 'x') {
	SWFDEC_AS_VALUE_SET_NUMBER (retval, 0);
	return;
      }
    }
  } else {
    radix = 0;
  }

  // special case
  if ((s[0] == '-' || s[0] == '+') && s[1] == '0' && s[2] == 'x') {
    SWFDEC_AS_VALUE_SET_NUMBER (retval, NAN);
    return;
  }

  if (s[0] == '0' && s[1] == 'x') {
    s = s + 2;
    i = g_ascii_strtoll (s, &tail, (radix != 0 ? radix : 16));
  } else {
    i = g_ascii_strtoll (s, &tail, (radix != 0 ? radix : 10));
  }

  if (tail == s) {
    SWFDEC_AS_VALUE_SET_NUMBER (retval, NAN);
    return;
  }

  if (i > G_MAXINT32 || i < G_MININT32) {
    SWFDEC_AS_VALUE_SET_NUMBER (retval, i);
  } else {
    SWFDEC_AS_VALUE_SET_INT (retval, i);
  }
}

SWFDEC_AS_NATIVE (100, 3, swfdec_as_context_parseFloat)
void
swfdec_as_context_parseFloat (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *retval)
{
  char *s, *p, *tail;
  double d;

  if (argc < 1)
    return;

  // we need to remove everything after x or I, since strtod parses hexadecimal
  // numbers and Infinity
  s = g_strdup (swfdec_as_value_to_string (cx, &argv[0]));
  if ((p = strpbrk (s, "xI")) != NULL) {
    *p = '\0';
  }

  d = g_ascii_strtod (s, &tail);

  if (tail == s) {
    SWFDEC_AS_VALUE_SET_NUMBER (retval, NAN);
  } else {
    SWFDEC_AS_VALUE_SET_NUMBER (retval, d);
  }

  g_free (s);
}
static void
swfdec_as_context_init_global (SwfdecAsContext *context, guint version)
{
  SwfdecAsValue val;

  swfdec_as_object_add_function (context->global, SWFDEC_AS_STR_ASSetPropFlags, 0, 
      swfdec_as_context_ASSetPropFlags, 3);
  SWFDEC_AS_VALUE_SET_NUMBER (&val, NAN);
  swfdec_as_object_set_variable (context->global, SWFDEC_AS_STR_NaN, &val);
  SWFDEC_AS_VALUE_SET_NUMBER (&val, HUGE_VAL);
  swfdec_as_object_set_variable (context->global, SWFDEC_AS_STR_Infinity, &val);
}

void
swfdec_as_context_run_init_script (SwfdecAsContext *context, const guint8 *data, 
    gsize length, guint version)
{
  g_return_if_fail (SWFDEC_IS_AS_CONTEXT (context));
  g_return_if_fail (data != NULL);
  g_return_if_fail (length > 0);

  if (version > 4) {
    SwfdecBits bits;
    SwfdecScript *script;
    swfdec_bits_init_data (&bits, data, length);
    script = swfdec_script_new_from_bits (&bits, "init", version);
    if (script == NULL) {
      g_warning ("script passed to swfdec_as_context_run_init_script is invalid");
      return;
    }
    swfdec_as_object_run (context->global, script);
    swfdec_script_unref (script);
  } else {
    SWFDEC_LOG ("not running init script, since version is <= 4");
  }
}

/**
 * swfdec_as_context_startup:
 * @context: a #SwfdecAsContext
 * @version: Flash version to use
 *
 * Starts up the context. This function must be called before any Actionscript
 * is called on @context. The version is responsible for deciding which native
 * functions and properties are available in the context.
 **/
void
swfdec_as_context_startup (SwfdecAsContext *context, guint version)
{
  g_return_if_fail (SWFDEC_IS_AS_CONTEXT (context));
  g_return_if_fail (context->state == SWFDEC_AS_CONTEXT_NEW);

  if (!swfdec_as_stack_push_segment (context))
    return;
  context->version = version;
  /* init the two internal functions */
  /* FIXME: remove them for normal contexts? */
  swfdec_player_preinit_global (context, version);
  /* get the necessary objects up to define objects and functions sanely */
  swfdec_as_function_init_context (context, version);
  swfdec_as_object_init_context (context, version);
  /* define the global object and other important ones */
  swfdec_as_context_init_global (context, version);

  /* run init script */
  swfdec_as_context_run_init_script (context, swfdec_as_initialize, sizeof (swfdec_as_initialize), 8);

  if (context->state == SWFDEC_AS_CONTEXT_NEW)
    context->state = SWFDEC_AS_CONTEXT_RUNNING;
}

