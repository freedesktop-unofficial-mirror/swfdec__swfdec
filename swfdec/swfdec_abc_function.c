/* Swfdec
 * Copyright (C) 2008 Benjamin Otte <otte@gnome.org>
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

#include "swfdec_abc_function.h"

#include "swfdec_abc_file.h"
#include "swfdec_abc_global.h"
#include "swfdec_abc_internal.h"
#include "swfdec_abc_interpret.h"
#include "swfdec_abc_traits.h"
#include "swfdec_as_context.h"
#include "swfdec_as_frame_internal.h"
#include "swfdec_debug.h"

G_DEFINE_TYPE (SwfdecAbcFunction, swfdec_abc_function, SWFDEC_TYPE_GC_OBJECT)

static void
swfdec_abc_function_dispose (GObject *object)
{
  SwfdecAbcFunction *fun = SWFDEC_ABC_FUNCTION (object);
  SwfdecAsContext *context = swfdec_gc_object_get_context (object);

  if (fun->n_args) {
    swfdec_as_context_free (context, (fun->n_args + 1) * sizeof (SwfdecAbcFunctionArgument), fun->args);
    fun->args = NULL;
    fun->n_args = 0;
  }
  if (fun->n_exceptions) {
    swfdec_as_context_free (context, fun->n_exceptions * sizeof (SwfdecAbcException), fun->exceptions);
    fun->exceptions = NULL;
    fun->n_exceptions = 0;
  }
  if (fun->code) {
    swfdec_buffer_unref (fun->code);
    fun->code = NULL;
  }

  G_OBJECT_CLASS (swfdec_abc_function_parent_class)->dispose (object);
}

static void
swfdec_abc_function_class_init (SwfdecAbcFunctionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_abc_function_dispose;
}

static void
swfdec_abc_function_init (SwfdecAbcFunction *function)
{
}

gboolean
swfdec_abc_function_bind (SwfdecAbcFunction *fun, SwfdecAbcTraits *traits)
{
  g_return_val_if_fail (SWFDEC_IS_ABC_FUNCTION (fun), FALSE);
  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), FALSE);

  if (fun->bound_traits)
    return FALSE;

  fun->bound_traits = traits;
  fun->args[0].traits = traits;
  return TRUE;
}

gboolean
swfdec_abc_function_is_native (SwfdecAbcFunction *fun)
{
  g_return_val_if_fail (SWFDEC_IS_ABC_FUNCTION (fun), FALSE);
  
  return fun->native != NULL;
}

gboolean
swfdec_abc_function_resolve (SwfdecAbcFunction *fun)
{
  SwfdecAsContext *context;
  SwfdecAbcTraits *traits;
  SwfdecAbcGlobal *global;
  guint i;

  g_return_val_if_fail (SWFDEC_IS_ABC_FUNCTION (fun), FALSE);
  
  if (fun->resolved)
    return TRUE;

  context = swfdec_gc_object_get_context (fun);
  global = SWFDEC_ABC_GLOBAL (context->global);

  if (fun->return_type == NULL) {
    fun->return_traits = NULL;
  } else {
    traits = swfdec_abc_global_get_traits_for_multiname (global,
	fun->return_type);
    if (traits == NULL) {
      swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_VERIFY_ERROR,
	  "Class %s could not be found.", fun->return_type->name);
      return FALSE;
    }
    fun->return_traits = traits;
  }

  if (fun->bound_traits == NULL)
    fun->bound_traits = SWFDEC_ABC_OBJECT_TRAITS (context);

  if (fun->args[0].traits == NULL)
    fun->args[0].traits = SWFDEC_ABC_OBJECT_TRAITS (context);
  /* do we need the SET_UNDEFINED? */
  SWFDEC_AS_VALUE_SET_UNDEFINED (&fun->args[0].default_value);
  for (i = 1; i <= fun->n_args; i++) {
    if (fun->args[i].type == NULL) {
      fun->args[i].traits = NULL;
    } else {
      traits = swfdec_abc_global_get_traits_for_multiname (global,
	  fun->args[i].type);
      if (traits == NULL) {
	swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_VERIFY_ERROR,
	    "Class %s could not be found.", fun->args[i].type->name);
	return FALSE;
      }
      fun->args[i].traits = traits;
    }
    if (fun->args[i].default_index == 0) {
      SWFDEC_AS_VALUE_SET_UNDEFINED (&fun->args[i].default_value);
    } else {
      if (!swfdec_abc_file_get_constant (fun->pool, &fun->args[i].default_value,
	    fun->args[i].default_type, fun->args[i].default_index))
	return FALSE;
    }
    if (fun->args[i].traits && !swfdec_abc_traits_coerce (fun->args[i].traits, &fun->args[i].default_value)) {
      swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_VERIFY_ERROR,
	  "Illegal default value for type %s.", fun->args[i].traits->name);
      return FALSE;
    }
  }

  fun->resolved = TRUE;
  return TRUE;
}

gboolean
swfdec_abc_function_verify (SwfdecAbcFunction *fun)
{
  g_return_val_if_fail (SWFDEC_IS_ABC_FUNCTION (fun), FALSE);
  
  if (!swfdec_abc_function_resolve (fun))
    return FALSE;

  SWFDEC_FIXME ("i can has verify?");
  fun->verified = TRUE;
  return TRUE;
}

gboolean
swfdec_abc_function_is_override (SwfdecAbcFunction *fun, SwfdecAbcFunction *base)
{
  g_return_val_if_fail (SWFDEC_IS_ABC_FUNCTION (fun), FALSE);
  g_return_val_if_fail (fun->resolved, FALSE);
  g_return_val_if_fail (SWFDEC_IS_ABC_FUNCTION (base), FALSE);
  g_return_val_if_fail (base->resolved, FALSE);

  SWFDEC_FIXME ("implement");
  return TRUE;
}

gboolean
swfdec_abc_function_call (SwfdecAbcFunction *fun, SwfdecAbcScopeChain *scope,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  SwfdecAsFrame frame = { NULL, };
  SwfdecAsContext *context;
  guint i;

  g_return_val_if_fail (SWFDEC_IS_ABC_FUNCTION (fun), FALSE);
  g_return_val_if_fail (argv != NULL, FALSE);
  g_return_val_if_fail (ret != NULL, FALSE);

  context = swfdec_gc_object_get_context (fun);

  if (argc < fun->min_args || (argc > fun->n_args &&
	!fun->need_arguments && ! fun->need_rest)) {
    swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_ARGUMENT_ERROR, 
      "Argument count mismatch on %s. Expected %u, got %u.", 
      fun->name ? fun->name : "[unnamed]", fun->n_args, fun->min_args);
    return FALSE;
  }

  if (!swfdec_abc_function_resolve (fun))
    return FALSE;

  for (i = 0; i <= MIN (argc, fun->n_args); i++) {
    if (!swfdec_abc_traits_coerce (fun->args[i].traits, &argv[i])) {
      swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_TYPE_ERROR, 
	"Type Coercion failed: cannot convert %s to %s.",
	swfdec_as_value_get_type_name (&argv[i]), fun->args[i].traits->name);
      return FALSE;
    }
  }

  if (!swfdec_abc_function_verify (fun))
    return FALSE;

  swfdec_as_frame_init_native (&frame, context);
  /* HACK! */
  frame.function = (SwfdecAsFunction *) fun;
  if (SWFDEC_AS_VALUE_IS_OBJECT (&argv[0])) {
    frame.thisp = SWFDEC_AS_VALUE_GET_OBJECT (&argv[0]);
  }
  frame.argc = argc;
  frame.argv = argv;
  frame.return_value = ret;
  frame.target = frame.next ? frame.next->original_target : frame.thisp;
  frame.original_target = frame.target;
  /* FIXME: coerce arguments */
  if (fun->native) {
    SwfdecAsValue rval;
    ((SwfdecAbcNative) fun->native) (context, argc, argv, &rval);
    swfdec_as_frame_return (&frame, &rval);
    return context->exception ? FALSE : TRUE;
  } else {
    return swfdec_abc_interpret (fun, scope);
  }
}
