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

#include "swfdec_abc_object.h"
#include "swfdec_abc_function.h"
#include "swfdec_abc_internal.h"
#include "swfdec_abc_multiname.h"
#include "swfdec_abc_scope_chain.h"
#include "swfdec_abc_traits.h"
#include "swfdec_as_context.h"
#include "swfdec_debug.h"

enum {
  PROP_0,
  PROP_TRAITS
};

G_DEFINE_TYPE (SwfdecAbcObject, swfdec_abc_object, SWFDEC_TYPE_AS_OBJECT)

static void
swfdec_abc_object_get_property (GObject *object, guint param_id, GValue *value, 
    GParamSpec * pspec)
{
  SwfdecAbcObject *abc = SWFDEC_ABC_OBJECT (object);

  switch (param_id) {
    case PROP_TRAITS:
      g_value_set_object (value, abc->traits);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
swfdec_abc_object_set_property (GObject *object, guint param_id, const GValue *value, 
    GParamSpec * pspec)
{
  SwfdecAbcObject *abc = SWFDEC_ABC_OBJECT (object);

  switch (param_id) {
    case PROP_TRAITS:
      abc->traits = g_value_get_object (value);
      g_object_ref (abc->traits);
      /* traits must exist and be resolved - it's your job to ensure this */
      g_assert (abc->traits != NULL);
      g_assert (abc->traits->resolved);
      /* we use the traits' context here, our context might not be assigned yet */
      if (abc->traits->n_slots)
	abc->slots = swfdec_as_context_new (swfdec_gc_object_get_context (abc->traits),
	   SwfdecAsValue, abc->traits->n_slots);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
swfdec_abc_object_mark (SwfdecGcObject *gcobject)
{
  SwfdecAbcObject *object = SWFDEC_ABC_OBJECT (gcobject);

  swfdec_gc_object_mark (object->traits);

  SWFDEC_GC_OBJECT_CLASS (swfdec_abc_object_parent_class)->mark (gcobject);
}

static void
swfdec_abc_object_dispose (GObject *gobject)
{
  SwfdecAbcObject *object = SWFDEC_ABC_OBJECT (gobject);
  SwfdecAsContext *context = swfdec_gc_object_get_context (object);

  if (object->slots) {
    swfdec_as_context_free (context, sizeof (SwfdecAsValue) * object->traits->n_slots, object->slots);
    object->slots = NULL;
  }
  g_object_unref (object->traits);
  swfdec_abc_scope_chain_unref (context, object->scope);

  G_OBJECT_CLASS (swfdec_abc_object_parent_class)->dispose (gobject);
}

static void
swfdec_abc_object_class_init (SwfdecAbcObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_abc_object_dispose;
  object_class->get_property = swfdec_abc_object_get_property;
  object_class->set_property = swfdec_abc_object_set_property;

  g_object_class_install_property (object_class, PROP_TRAITS,
      g_param_spec_object ("traits", "traits", "traits describing this object",
	  SWFDEC_TYPE_ABC_TRAITS, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gc_class->mark = swfdec_abc_object_mark;
}

static void
swfdec_abc_object_init (SwfdecAbcObject *function)
{
}

SwfdecAbcObject *
swfdec_abc_object_new (SwfdecAbcTraits *traits, SwfdecAbcScopeChain *scope)
{
  SwfdecAbcObject *object;

  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), NULL);

  if (!swfdec_abc_traits_resolve (traits))
    return NULL;

  object = g_object_new (traits->type_func (), 
      "context", swfdec_gc_object_get_context (traits),
      "traits", traits, NULL);
  object->scope = swfdec_abc_scope_chain_ref (scope);

  return object;
}

gboolean
swfdec_abc_object_get_variable (SwfdecAsContext *context, const SwfdecAsValue *object,
    const SwfdecAbcMultiname *mn, SwfdecAsValue *value)
{
  SwfdecAbcTraits *traits;
  const SwfdecAbcTrait *trait;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), FALSE);
  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (mn != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  traits = swfdec_as_value_to_traits (context, object);
  trait = swfdec_abc_traits_find_trait_multi (traits, mn);
  if (!trait) {
    if (!SWFDEC_AS_VALUE_IS_OBJECT (object) || traits->sealed ||
	!swfdec_abc_multiname_contains_namespace (mn, context->public_ns)) {
      swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_REFERENCE_ERROR, 
	  "Property %s not found on %s and there is no default value.", 
	  mn->name, traits->name);
      SWFDEC_AS_VALUE_SET_UNDEFINED (value);
      return FALSE;
    }
    return swfdec_as_object_get_variable (SWFDEC_AS_VALUE_GET_OBJECT (object),
	mn->name, value);
  } else if (trait == SWFDEC_ABC_TRAIT_AMBIGUOUS) {
    swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_REFERENCE_ERROR, 
	"%s is ambiguous; Found more than one matching binding.", mn->name);
    SWFDEC_AS_VALUE_SET_UNDEFINED (value);
    return FALSE;
  } else {
    switch (SWFDEC_ABC_BINDING_GET_TYPE (trait->type)) {
      case SWFDEC_ABC_TRAIT_SLOT:
      case SWFDEC_ABC_TRAIT_CONST:
	{
	  SwfdecAbcObject *o = SWFDEC_ABC_OBJECT (SWFDEC_AS_VALUE_GET_OBJECT (object));
	  guint slot = SWFDEC_ABC_BINDING_GET_ID (trait->type);
	  g_assert (slot < o->traits->n_slots);
	  *value = o->slots[slot];
	}
	break;
      case SWFDEC_ABC_TRAIT_METHOD:
	SWFDEC_FIXME ("implement method getters");
	SWFDEC_AS_VALUE_SET_UNDEFINED (value);
	break;
      case SWFDEC_ABC_TRAIT_GET:
      case SWFDEC_ABC_TRAIT_GETSET:
	{
	  SwfdecAbcObject *o = SWFDEC_ABC_OBJECT (SWFDEC_AS_VALUE_GET_OBJECT (object));
	  guint slot = SWFDEC_ABC_BINDING_GET_ID (trait->type);
	  return swfdec_abc_function_call (o->traits->methods[slot], o->scope,
	      o, 0, NULL, value);
	}
      case SWFDEC_ABC_TRAIT_SET:
	swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_REFERENCE_ERROR, 
	    "Illegal read of write-only property %s on %s.", mn->name, traits->name);
	SWFDEC_AS_VALUE_SET_UNDEFINED (value);
	return FALSE;
      case SWFDEC_ABC_TRAIT_NONE:
      case SWFDEC_ABC_TRAIT_ITRAMP:
      default:
	g_assert_not_reached ();
    }
  }
  return TRUE;
}

static gboolean
swfdec_abc_object_set_variable_full (SwfdecAsContext *context, const SwfdecAsValue *object,
    gboolean init, const SwfdecAbcMultiname *mn, const SwfdecAsValue *value)
{
  SwfdecAbcTraits *traits;
  const SwfdecAbcTrait *trait;

  traits = swfdec_as_value_to_traits (context, object);
  trait = swfdec_abc_traits_find_trait_multi (traits, mn);
  if (trait == NULL) {
    if (!SWFDEC_AS_VALUE_IS_OBJECT (object) || traits->sealed ||
	!swfdec_abc_multiname_contains_namespace (mn, context->public_ns)) {
      swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_REFERENCE_ERROR,
	  "Cannot create property %s on %s.", mn->name, traits->name);
      return FALSE;
    }
    swfdec_as_object_set_variable (SWFDEC_AS_VALUE_GET_OBJECT (object),
	mn->name, value);
  } else if (trait == SWFDEC_ABC_TRAIT_AMBIGUOUS) {
    swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_REFERENCE_ERROR,
	"%s is ambiguous; Found more than one matching binding.", mn->name);
    return FALSE;
  } else {
    SwfdecAbcTraitType type = SWFDEC_ABC_BINDING_GET_TYPE (trait->type);
    if (init && type == SWFDEC_ABC_TRAIT_CONST)
      type = SWFDEC_ABC_TRAIT_SLOT;
    switch (type) {
      case SWFDEC_ABC_TRAIT_SLOT:
	{
	  SwfdecAbcObject *o = SWFDEC_ABC_OBJECT (SWFDEC_AS_VALUE_GET_OBJECT (object));
	  SwfdecAsValue tmp = *value;
	  guint slot = SWFDEC_ABC_BINDING_GET_ID (trait->type);
	  //if (!swfdec_abc_traits_coerce (trait->traits, &tmp))
	  //  return FALSE;
	  g_assert (slot < o->traits->n_slots);
	  o->slots[slot] = tmp;
	}
	break;
      case SWFDEC_ABC_TRAIT_CONST:
      case SWFDEC_ABC_TRAIT_GET:
	swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_REFERENCE_ERROR,
	    "Illegal write to read-only property %s on %s.", mn->name, traits->name);
	return FALSE;
      case SWFDEC_ABC_TRAIT_METHOD:
	SWFDEC_FIXME ("implement method getters");
	break;
      case SWFDEC_ABC_TRAIT_SET:
      case SWFDEC_ABC_TRAIT_GETSET:
	{
	  SwfdecAbcObject *o = SWFDEC_ABC_OBJECT (SWFDEC_AS_VALUE_GET_OBJECT (object));
	  SwfdecAsValue retval, tmp;
	  guint slot = SWFDEC_ABC_BINDING_GET_ID (trait->type);
	  /* Need to do this as argv gets coerced */
	  tmp = *value;
	  return swfdec_abc_function_call (o->traits->methods[slot + 1], o->scope,
	      o, 1, &tmp, &retval);
	}
      case SWFDEC_ABC_TRAIT_NONE:
      case SWFDEC_ABC_TRAIT_ITRAMP:
      default:
	g_assert_not_reached ();
    }
  }
  return TRUE;
}

gboolean
swfdec_abc_object_set_variable (SwfdecAsContext *context, const SwfdecAsValue *object,
    const SwfdecAbcMultiname *mn, const SwfdecAsValue *value)
{
  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), FALSE);
  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (mn != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  return swfdec_abc_object_set_variable_full (context, object, FALSE, mn, value);
}

gboolean
swfdec_abc_object_init_variable (SwfdecAsContext *context, const SwfdecAsValue *object,
    const SwfdecAbcMultiname *mn, const SwfdecAsValue *value)
{
  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), FALSE);
  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (mn != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  return swfdec_abc_object_set_variable_full (context, object, TRUE, mn, value);
}

