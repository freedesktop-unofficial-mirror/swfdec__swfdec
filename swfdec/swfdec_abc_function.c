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
#include "swfdec_as_strings.h" /* swfdec_abc_function_describe */
#include "swfdec_debug.h"

/*** LIBFFI ***/

static ffi_type *machine_types_ffi[] = {
  [SWFDEC_ABC_POINTER] = &ffi_type_pointer,
  [SWFDEC_ABC_INT] = &ffi_type_sint,
  [SWFDEC_ABC_UINT] = &ffi_type_uint,
  [SWFDEC_ABC_DOUBLE] = &ffi_type_double,
  [SWFDEC_ABC_STRING] = &ffi_type_pointer,
  [SWFDEC_ABC_VOID] = &ffi_type_void
};

static ffi_type *
swfdec_abc_ffi_traits_to_type (SwfdecAbcTraits *traits)
{
  if (traits == NULL)
    return &ffi_type_pointer;

  return machine_types_ffi[traits->machine_type];
}

static guint
swfdec_abc_function_native_n_arguments (SwfdecAbcFunction *fun)
{
  /* this pointer */
  guint n_args = 1;
  /* arguments */
  n_args += fun->n_args;
  /* add context as first arg if we're not passing an object */
  if (fun->args[0].traits->machine_type != SWFDEC_ABC_POINTER)
    n_args++;
  /* append arguments: guint argc, SwfdecAsValue **argv */
  if (fun->need_rest || fun->need_arguments)
    n_args += 2;
  /* append argument: SwfdecAsValue *retval */
  if (fun->return_traits == NULL)
    n_args++;
  return n_args;
}

static gboolean
swfdec_abc_function_ffi_verify (SwfdecAbcFunction *fun)
{
  guint i, n_args;
  ffi_type **args;
  ffi_type *ret;
  ffi_status status;
  
  /* prepare variables */
  n_args = swfdec_abc_function_native_n_arguments (fun);
  args = g_new (ffi_type *, n_args);

  /* set argument types */
  if (fun->args[0].traits->machine_type != SWFDEC_ABC_POINTER)
    args[0] = &ffi_type_pointer;
  for (i = 0; i <= fun->n_args; i++) {
    args[i] = swfdec_abc_ffi_traits_to_type (fun->args[i].traits);
  }
  if (fun->args[0].traits->machine_type != SWFDEC_ABC_POINTER) {
    args--;
    i++;
  }
  if (fun->need_rest || fun->need_arguments) {
    args[i++] = &ffi_type_uint;
    args[i++] = &ffi_type_pointer;
  }
  if (fun->return_traits == NULL) {
    ret = &ffi_type_void;
    args[i++] = &ffi_type_pointer;
  } else {
    ret = swfdec_abc_ffi_traits_to_type (fun->return_traits);
  }
  g_assert (i == n_args);

  /* prepare the cif */
  status = ffi_prep_cif (&fun->cif, FFI_DEFAULT_ABI, n_args, ret, args);
  //g_free (args);
  if (status != FFI_OK) {
    /* We don't have typedefs and we use the default abi, so this must not happen */
    g_assert_not_reached ();
  }

  return TRUE;
}

/* NB: @val must have been coerced to @traits */
static gpointer
swfdec_abc_ffi_get_address (SwfdecAbcTraits *traits, SwfdecAsValue *val)
{
  g_assert (traits != NULL);

  /* make sure the value is NULL; */
  if (SWFDEC_AS_VALUE_IS_NULL (val))
    val->value.object = NULL;
  /* FIXME: Is this correct, or do we need to do stuff like:
   * if (SWFDEC_AS_VALUE_IS_BOOLEAN (val)) return &val.values.boolean
   */
  return &val->value;
}

static gboolean
swfdec_abc_function_ffi_call (SwfdecAbcFunction *fun)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (fun);
  SwfdecAsFrame *frame = context->frame;
  SwfdecAsValue rval = { 0, };
  const SwfdecAsValue *restp;
  guint i, rest, n_args;
  gpointer *args;
  gpointer ret, rvalp;

  /* compute number of arguments */
  n_args = swfdec_abc_function_native_n_arguments (fun);
  /* FIXME: I don't want a malloc here */
  args = g_new (gpointer, n_args);

  if (fun->args[0].traits->machine_type != SWFDEC_ABC_POINTER) {
    args[0] = &context;
    args++;
  }
  for (i = 0; i <= MIN (frame->argc, fun->n_args); i++) {
    if (fun->args[i].traits == NULL) {
      gconstpointer *tmp = g_newa (gconstpointer, 1);
      *tmp = &frame->argv[i];
      args[i] = &tmp;
    } else {
      args[i] = swfdec_abc_ffi_get_address (fun->args[i].traits, (gpointer) &frame->argv[i]);
    }
  }
  for (; i <= fun->n_args; i++) {
    if (fun->args[i].traits == NULL) {
      gpointer *tmp = g_newa (gpointer, 1);
      *tmp = &fun->args[i].default_value;
      args[i] = &tmp;
    } else {
      args[i] = swfdec_abc_ffi_get_address (fun->args[i].traits, 
	  &fun->args[i].default_value);
    }
  }
  if (fun->args[0].traits->machine_type != SWFDEC_ABC_POINTER) {
    args--;
    i++;
  }
  if (fun->need_rest) {
    if (frame->argc <= fun->n_args) {
      rest = 0;
      args[i++] = &rest;
      args[i++] = &rval.value.object; /* should be a NULL value */
    } else {
      rest = frame->argc - fun->n_args;
      restp = &frame->argv[fun->n_args];
      args[i++] = &rest;
      args[i++] = &restp;
    }
  } else if (fun->need_arguments) {
    args[i++] = &frame->argc;
    args[i++] = &frame->argv;
  }
  if (fun->return_traits == NULL) {
    rvalp = &rval;
    args[i++] = &rvalp;
    ret = NULL;
  } else {
    ret = swfdec_abc_ffi_get_address (fun->return_traits, &rval);
  }
  g_assert (i == n_args);

  ffi_call (&fun->cif, FFI_FN (fun->native), ret, args);
  g_free (args);
  if ((SWFDEC_AS_VALUE_IS_OBJECT (&rval) && rval.value.object == NULL) ||
      (SWFDEC_AS_VALUE_IS_STRING (&rval) && rval.value.string == NULL) ||
      (SWFDEC_AS_VALUE_IS_NAMESPACE (&rval) && rval.value.ns == NULL)) {
    SWFDEC_AS_VALUE_SET_NULL (&rval);
  }
  swfdec_as_frame_return (frame, &rval);
  return context->exception ? FALSE : TRUE;
}

G_DEFINE_TYPE (SwfdecAbcFunction, swfdec_abc_function, SWFDEC_TYPE_GC_OBJECT)

static void
swfdec_abc_function_dispose (GObject *object)
{
  SwfdecAbcFunction *fun = SWFDEC_ABC_FUNCTION (object);
  SwfdecAsContext *context = swfdec_gc_object_get_context (object);

  if (fun->args) {
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
      } else if (traits == SWFDEC_ABC_VOID_TRAITS (context)) {
	swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_VERIFY_ERROR,
	    "Type void may only be used as a function return type.");
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
  static gboolean shut_up = FALSE;

  g_return_val_if_fail (SWFDEC_IS_ABC_FUNCTION (fun), FALSE);
  
  if (!swfdec_abc_function_resolve (fun))
    return FALSE;

  if (fun->native)
    return swfdec_abc_function_ffi_verify (fun);

  if (!shut_up) {
    SWFDEC_FIXME ("i can has verify?");
    shut_up = TRUE;
  }
  fun->verified = TRUE;
  return TRUE;
}

gboolean
swfdec_abc_function_is_override (SwfdecAbcFunction *fun, SwfdecAbcFunction *base)
{
  guint i;

  g_return_val_if_fail (SWFDEC_IS_ABC_FUNCTION (fun), FALSE);
  g_return_val_if_fail (fun->resolved, FALSE);
  g_return_val_if_fail (SWFDEC_IS_ABC_FUNCTION (base), FALSE);
  g_return_val_if_fail (base->resolved, FALSE);

  if (fun->return_traits != base->return_traits) {
    SWFDEC_WARNING ("return types don't match: %s => %s", 
	base->return_traits->name, fun->return_traits->name);
    return FALSE;
  }

  if (fun->n_args != base->n_args) {
    SWFDEC_WARNING ("argument count doesn't match: %u => %u", 
	base->n_args, fun->n_args);
    return FALSE;
  }

  if (!swfdec_abc_traits_is_traits (fun->args[0].traits, base->args[0].traits)) {
    SWFDEC_WARNING ("this traits %s aren't subtraits of %s",
	fun->args[0].traits->name, base->args[0].traits->name);
    return FALSE;
  }

  for (i = 1; i <= fun->n_args; i++) {
    if (fun->args[i].traits !=  base->args[i].traits) {
      SWFDEC_WARNING ("traits for argument %u don't match: %s => %s",
	  i, base->args[i].traits->name, fun->args[i].traits->name);
      return FALSE;
    }
  }

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
    return swfdec_abc_function_ffi_call (fun);
  } else if (fun->code) {
    return swfdec_abc_interpret (fun, scope);
  } else {
    /* no code and no native function, probably a missing stub */
    SwfdecAsValue rval = { 0, };
    char *desc = swfdec_abc_function_describe (fun);
    SWFDEC_STUB (desc);
    g_free (desc);
    if (fun->return_traits && !swfdec_abc_traits_coerce (fun->return_traits, &rval))
      return FALSE;
    swfdec_as_frame_return (&frame, &rval);
    return TRUE;
  }
}

static void
swfdec_abc_function_append_name_for_traits (GString *string, SwfdecAbcTraits *traits)
{
  if (traits == NULL) {
    g_string_append (string, "const SwfdecAsValue *");
    return;
  }

  switch (traits->machine_type) {
    case SWFDEC_ABC_POINTER:
      g_string_append (string, "SwfdecAbc");
      g_string_append (string, traits->name);
      g_string_append (string, "*");
      break;
    case SWFDEC_ABC_INT:
      if (traits->name == SWFDEC_AS_STR_Boolean)
	g_string_append (string, "gboolean");
      else
	g_string_append (string, "int");
      break;
    case SWFDEC_ABC_UINT:
      g_string_append (string, "guint");
      break;
    case SWFDEC_ABC_DOUBLE:
      g_string_append (string, "double");
      break;
    case SWFDEC_ABC_STRING:
      g_string_append (string, "const char*");
      break;
    case SWFDEC_ABC_VOID:
      g_string_append (string, "void");
      break;
    default:
      g_assert_not_reached ();
  }
}

char *
swfdec_abc_function_describe (SwfdecAbcFunction *fun)
{
  SwfdecAbcTraits *traits;
  GString *name, *retname;
  guint i, id;

  g_assert (fun->resolved);

  /* find id of our function in pool */
  for (id = 0; id < fun->pool->n_functions; id++) {
    if (fun->pool->functions[id] == fun)
      break;
  }

  traits = fun->bound_traits;
  name = g_string_new ("");
  if (fun->return_traits == NULL) {
    retname = g_string_new ("void");
  } else {
    retname = g_string_new ("");
    swfdec_abc_function_append_name_for_traits (retname, fun->return_traits);
  }
  if (traits == NULL) {
    /* we're likely a lambda function */
    g_string_append_printf (name, "function %u: %s [lambda]", id, retname->str);
  } else if (traits->construct == fun) {
    /* we're the constructor */
    g_string_append_printf (name, "function %u: %s %s", id, 
	retname->str, traits->name);
  } else {
    /* we're method, getter or setter, find out what */
    do {
      for (i = 0; i < traits->n_traits; i++) {
	SwfdecAbcTrait *trait = &traits->traits[i];
	guint slot = SWFDEC_ABC_BINDING_GET_ID (trait->type);
	switch (SWFDEC_ABC_BINDING_GET_TYPE (trait->type)) {
	  case SWFDEC_ABC_TRAIT_METHOD:
	    if (traits->methods[slot] == fun) {
	      g_string_append_printf (name, "method %u %s %s.%s", id,
		  retname->str, traits->name, trait->name);
	      goto out;
	    }
	    break;
	  case SWFDEC_ABC_TRAIT_GET:
	  case SWFDEC_ABC_TRAIT_SET:
	  case SWFDEC_ABC_TRAIT_GETSET:
	    if (traits->methods[slot] == fun) {
	      g_string_append_printf (name, "getter %u %s %s.%s", id,
		  retname->str, traits->name, trait->name);
	      goto out;
	    } else if (traits->methods[slot + 1] == fun) {
	      g_string_append_printf (name, "setter %u %s %s.%s", id,
		  retname->str, traits->name, trait->name);
	      goto out;
	    }
	    break;
	  case SWFDEC_ABC_TRAIT_NONE:
	  case SWFDEC_ABC_TRAIT_SLOT:
	  case SWFDEC_ABC_TRAIT_CONST:
	  case SWFDEC_ABC_TRAIT_ITRAMP:
	  default:
	  break;
	}
      }
      traits = traits->base;
    } while (traits);
    /* huh? */
    g_string_append_printf (name, "function %u %s %s", id, retname->str,
	fun->bound_traits->name);
  }
out:
  /* append arguments */
  g_string_append (name, " (");
  if (fun->args[0].traits->machine_type != SWFDEC_ABC_POINTER)
    g_string_append (name, "SwfdecAsContext *cx, ");
  for (i = 0; i <= fun->n_args; i++) {
    if (i > 0)
      g_string_append (name, ", ");
    swfdec_abc_function_append_name_for_traits (name, fun->args[i].traits);
    if (i > 0)
      g_string_append_printf (name, " arg%u", i);
    else
      g_string_append (name, " thisp");
  }
  if (fun->need_rest) {
    g_string_append (name, ", guint argc, SwfdecAsValue *rest");
  } else if (fun->need_arguments) {
    g_string_append (name, ", guint argc, SwfdecAsValue *argv");
  }
  if (fun->return_type == NULL) {
    g_string_append (name, ", SwfdecAsValue *ret");
  }
  g_string_append (name, ")");

  g_string_free (retname, TRUE);
  return g_string_free (name, FALSE);
}

