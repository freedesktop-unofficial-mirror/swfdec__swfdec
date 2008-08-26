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

#include "swfdec_abc_traits.h"

#include <string.h>

#include "swfdec_abc_pool.h"
#include "swfdec_abc_function.h"
#include "swfdec_abc_global.h"
#include "swfdec_abc_internal.h"
#include "swfdec_abc_multiname.h"
#include "swfdec_abc_namespace.h"
#include "swfdec_abc_object.h"
#include "swfdec_abc_value.h"
#include "swfdec_as_context.h"
#include "swfdec_as_strings.h"
#include "swfdec_debug.h"

const SwfdecAbcTrait swfdec_abc_trait_ambiguous = { SWFDEC_ABC_TRAIT_NONE, 0,
  NULL, SWFDEC_AS_STR_EMPTY, FALSE, FALSE, };

G_DEFINE_TYPE (SwfdecAbcTraits, swfdec_abc_traits, SWFDEC_TYPE_GC_OBJECT)

static void
swfdec_abc_traits_dispose (GObject *object)
{
  SwfdecAbcTraits *traits = SWFDEC_ABC_TRAITS (object);
  SwfdecAsContext *context = swfdec_gc_object_get_context (traits);

  if (traits->n_traits) {
    swfdec_as_context_free (context, traits->n_traits * sizeof (SwfdecAbcTrait), traits->traits);
    traits->traits = NULL;
    traits->n_traits = 0;
  }
  if (traits->resolved) {
    if (traits->n_slots)
      swfdec_as_context_free (context, traits->n_slots * sizeof (SwfdecAsValue), traits->slots);
    if (traits->n_methods)
      swfdec_as_context_free (context, traits->n_methods * sizeof (SwfdecAsFunction *), traits->methods);
  }

  G_OBJECT_CLASS (swfdec_abc_traits_parent_class)->dispose (object);
}

static void
swfdec_abc_traits_mark (SwfdecGcObject *object)
{
  SwfdecAbcTraits *traits = SWFDEC_ABC_TRAITS (object);
  guint i;

  swfdec_gc_object_mark (traits->ns);
  swfdec_as_string_mark (traits->name);
  if (traits->base)
    swfdec_gc_object_mark (traits->base);
  if (traits->construct)
    swfdec_gc_object_mark (traits->construct);
  if (traits->protected_ns)
    swfdec_gc_object_mark (traits->protected_ns);
  if (traits->pool)
    swfdec_gc_object_mark (traits->pool);
  if (traits->resolved) {
    for (i = 0; i < traits->n_slots; i++) {
      swfdec_as_value_mark (&traits->slots[i]);
    }
  }

  SWFDEC_GC_OBJECT_CLASS (swfdec_abc_traits_parent_class)->mark (object);
}

static void
swfdec_abc_traits_class_init (SwfdecAbcTraitsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_abc_traits_dispose;

  gc_class->mark = swfdec_abc_traits_mark;
}

static void
swfdec_abc_traits_init (SwfdecAbcTraits *traits)
{
  traits->type_func = swfdec_abc_object_get_type;
}

gboolean
swfdec_abc_traits_allow_early_binding (SwfdecAbcTraits *traits)
{
  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), FALSE);

  do {
    if (traits->n_slots == 0 ||
	traits->base == NULL)
      return TRUE;
    if (traits->base->pool != traits->pool)
      return FALSE;
  } while ((traits = traits->base) != NULL);
  g_return_val_if_reached (FALSE);
}

gboolean
swfdec_abc_traits_resolve (SwfdecAbcTraits *traits)
{
  SwfdecAsContext *context;
  SwfdecAbcTraits *base;
  SwfdecAbcPool *pool;
  guint i;

  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), FALSE);
  
  if (traits->resolved)
    return TRUE;

  context = swfdec_gc_object_get_context (traits);
  base = traits->base;
  pool = traits->pool;

  if (base && !swfdec_abc_traits_resolve (base))
    return FALSE;
  /* resolve interfaces here */

  if (traits->n_slots) {
    traits->slots = swfdec_as_context_new (context, SwfdecAsValue, traits->n_slots);
    if (base && base->n_slots)
      memcpy (traits->slots, base->slots, base->n_slots * sizeof (SwfdecAsValue));
  }
  if (traits->n_methods) {
    traits->methods = swfdec_as_context_new (context, SwfdecAbcFunction *, traits->n_methods);
    if (base && base->n_methods)
      memcpy (traits->methods, base->methods, base->n_methods * sizeof (SwfdecAsFunction *));
  }

  for (i = 0; i < traits->n_traits; i++) {
    SwfdecAbcTrait *trait = &traits->traits[i];
    guint slot = SWFDEC_ABC_BINDING_GET_ID (trait->type);

    switch (SWFDEC_ABC_BINDING_GET_TYPE (trait->type)) {
      case SWFDEC_ABC_TRAIT_NONE:
	continue;
      case SWFDEC_ABC_TRAIT_SLOT:
      case SWFDEC_ABC_TRAIT_CONST:
	if (trait->traits_type) {
	  if (trait->traits_type >= pool->n_multinames) {
	    swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_VERIFY_ERROR,
		"Cpool index %u is out of range %u.", trait->traits_type, pool->n_multinames);
	    goto fail;
	  }
	  trait->traits = swfdec_abc_global_get_traits_for_multiname (
	      SWFDEC_ABC_GLOBAL (context->global), &pool->multinames[trait->traits_type]);
	  if (trait->traits == NULL) {
	    swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_VERIFY_ERROR,
		"Class %s could not be found.", pool->multinames[trait->traits_type].name);
	    goto fail;
	  } else if (trait->traits == SWFDEC_ABC_VOID_TRAITS (context)) {
	    swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_VERIFY_ERROR,
		"Type void may only be used as a function return type.");
	    goto fail;
	  }
	} else {
	  trait->traits = NULL;
	}
	if (trait->default_type == G_MAXUINT) {
	  /* magic value for classes */
	  g_assert (trait->traits == NULL);
	  trait->traits = pool->classes[trait->default_index];
	  SWFDEC_AS_VALUE_SET_NULL (&traits->slots[slot]);
	} else if (trait->default_index == 0) {
	  SWFDEC_AS_VALUE_SET_UNDEFINED (&traits->slots[slot]);
	} else {
	  if (!swfdec_abc_pool_get_constant (pool, &traits->slots[slot],
		trait->default_type, trait->default_index))
	    goto fail;
	}
	if (trait->traits && !swfdec_abc_traits_coerce (trait->traits, &traits->slots[slot])) {
	  swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_VERIFY_ERROR,
	      "Illegal default value for type %s.", trait->traits->name);
	  goto fail;
	}
	continue;
      case SWFDEC_ABC_TRAIT_METHOD:
      case SWFDEC_ABC_TRAIT_GET:
      case SWFDEC_ABC_TRAIT_GETSET:
	traits->methods[slot] = pool->functions[trait->default_index];
	if (!swfdec_abc_function_resolve (traits->methods[slot]))
	  goto fail;
	if (SWFDEC_ABC_BINDING_GET_TYPE (trait->type) != SWFDEC_ABC_TRAIT_GETSET)
	  break;
      case SWFDEC_ABC_TRAIT_SET:
	slot++;
	traits->methods[slot] = pool->functions[trait->default_type];
	if (!swfdec_abc_function_resolve (traits->methods[slot]))
	  goto fail;
	break;
      case SWFDEC_ABC_TRAIT_ITRAMP:
      default:
	g_assert_not_reached ();
	break;
    }
  }

  /* check all overrides are fine */
  if (base) {
    for (i = 0; i < base->n_methods; i++) {
      if (base->methods[i] != NULL && 
	  base->methods[i] != traits->methods[i] &&
	  (traits->methods[i] == NULL ||
	   !swfdec_abc_function_is_override (traits->methods[i], base->methods[i]))) {
	/* nice error message */
	swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_VERIFY_ERROR,
	    "Illegal override of %s in %s.", traits->name, traits->name);
	goto fail;
      }
    }
  }

  /* FIXME: interface override verification goes here */

  traits->resolved = TRUE;
  return TRUE;

fail:
  if (traits->n_slots) {
    swfdec_as_context_free (context, traits->n_slots * sizeof (SwfdecAsValue), traits->slots);
    traits->slots = NULL;
  }
  if (traits->n_methods) {
    swfdec_as_context_free (context, traits->n_methods * sizeof (SwfdecAsFunction *), traits->methods);
    traits->methods = NULL;
  }
  return FALSE;
}

const SwfdecAbcTrait *
swfdec_abc_traits_get_trait (SwfdecAbcTraits *traits,
    SwfdecAbcNamespace *ns, const char *name)
{
  guint i;

  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), NULL);
  g_return_val_if_fail (SWFDEC_IS_ABC_NAMESPACE (ns), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  for (i = 0; i < traits->n_traits; i++) {
    SwfdecAbcTrait *trait = &traits->traits[i];

    if (trait->type == SWFDEC_ABC_TRAIT_NONE)
      continue;

    if (trait->ns == ns && trait->name == name)
      return trait;
  }
  return NULL;
}

const SwfdecAbcTrait *
swfdec_abc_traits_find_trait (SwfdecAbcTraits *traits,
    SwfdecAbcNamespace *ns, const char *name)
{
  const SwfdecAbcTrait *trait;

  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), NULL);
  g_return_val_if_fail (SWFDEC_IS_ABC_NAMESPACE (ns), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  do {
    trait = swfdec_abc_traits_get_trait (traits, ns, name);
    if (trait)
      return trait;
    traits = traits->base;
  } while (traits);
  return NULL;
}

const SwfdecAbcTrait *
swfdec_abc_traits_find_trait_multi (SwfdecAbcTraits *traits,
    const SwfdecAbcMultiname *mn)
{
  const SwfdecAbcTrait *trait, *found;
  guint i;

  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), NULL);
  g_return_val_if_fail (mn != NULL, NULL);

  found = NULL;
  for (i = 0; i < swfdec_abc_multiname_get_n_namespaces (mn); i++) {
    trait = swfdec_abc_traits_find_trait (traits, 
	swfdec_abc_multiname_get_namespace (mn, i), mn->name);
    if (trait == NULL)
      continue;
    if (found == NULL)
      found = trait;
    else
      return SWFDEC_ABC_TRAIT_AMBIGUOUS;
  }
  return found;
}

gboolean
swfdec_abc_traits_is_traits (SwfdecAbcTraits *traits, SwfdecAbcTraits *parent)
{
  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), FALSE);
  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (parent), FALSE);

  if (parent->interface) {
    SWFDEC_FIXME ("implement");
  } else {
    do {
      if (traits == parent)
	return TRUE;
      traits = traits->base;
    } while (traits != NULL);
  }
  return FALSE;
}

SwfdecAbcTraits *
swfdec_abc_value_to_traits (SwfdecAsContext *context, const SwfdecAsValue *value)
{
  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), NULL);
  g_return_val_if_fail (value != NULL, NULL);

  switch (value->type) {
    case SWFDEC_AS_TYPE_UNDEFINED:
      return SWFDEC_ABC_VOID_TRAITS (context);
    case SWFDEC_AS_TYPE_NULL:
      return SWFDEC_ABC_NULL_TRAITS (context);
    case SWFDEC_AS_TYPE_BOOLEAN:
      return SWFDEC_ABC_BOOLEAN_TRAITS (context);
    case SWFDEC_AS_TYPE_NUMBER:
      /* FIXME: support uint and int traits */
      return SWFDEC_ABC_NUMBER_TRAITS (context);
    case SWFDEC_AS_TYPE_STRING:
      return SWFDEC_ABC_STRING_TRAITS (context);
    case SWFDEC_AS_TYPE_NAMESPACE:
      return SWFDEC_ABC_NAMESPACE_TRAITS (context);
    case SWFDEC_AS_TYPE_OBJECT:
      {
	SwfdecAsObject *o = SWFDEC_AS_VALUE_GET_OBJECT (value);
	if (!SWFDEC_IS_ABC_OBJECT (o)) {
	  SWFDEC_ERROR ("wtf? is_traits on a non-abc object?");
	  return SWFDEC_ABC_OBJECT_TRAITS (context);
	}
	return SWFDEC_ABC_OBJECT (o)->traits;
      }
    case SWFDEC_AS_TYPE_INT:
    default:
      g_assert_not_reached ();
      return NULL;
  }
}

gboolean
swfdec_abc_value_is_traits (const SwfdecAsValue *value, SwfdecAbcTraits *traits)
{
  SwfdecAsContext *context;
  SwfdecAbcTraits *vtraits;

  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), FALSE);

  context = swfdec_gc_object_get_context (traits);
  vtraits = swfdec_abc_value_to_traits (context, value);
  return swfdec_abc_traits_is_traits (vtraits, traits);
}

gboolean
swfdec_abc_traits_coerce (SwfdecAbcTraits *traits, SwfdecAsValue *val)
{
  SwfdecAsContext *context;

  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), FALSE);
  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), FALSE);

  context = swfdec_gc_object_get_context (traits);

  if (traits == SWFDEC_ABC_BOOLEAN_TRAITS (context)) {
    SWFDEC_AS_VALUE_SET_BOOLEAN (val,
	swfdec_abc_value_to_boolean (context, val));
    return TRUE;
  } else if (traits == SWFDEC_ABC_NUMBER_TRAITS (context)) {
    SWFDEC_AS_VALUE_SET_NUMBER (val,
	swfdec_abc_value_to_number (context, val));
    return TRUE;
  } else if (traits == SWFDEC_ABC_INT_TRAITS (context)) {
    SWFDEC_AS_VALUE_SET_INT (val,
	swfdec_abc_value_to_integer (context, val));
    return TRUE;
  } else if (traits == SWFDEC_ABC_UINT_TRAITS (context)) {
    SWFDEC_AS_VALUE_SET_NUMBER (val,
	(guint) swfdec_abc_value_to_integer (context, val));
    return TRUE;
  } else if (traits == SWFDEC_ABC_NUMBER_TRAITS (context)) {
    SWFDEC_AS_VALUE_SET_NUMBER (val,
	(guint) swfdec_abc_value_to_integer (context, val));
    return TRUE;
  } else if (traits == SWFDEC_ABC_STRING_TRAITS (context)) {
    if (SWFDEC_AS_VALUE_IS_UNDEFINED (val) || SWFDEC_AS_VALUE_IS_NULL (val)) {
      SWFDEC_AS_VALUE_SET_NULL (val);
    } else {
      SWFDEC_AS_VALUE_SET_STRING (val, 
	  swfdec_abc_value_to_string (context, val));
    }
    return TRUE;
  } else if (traits == SWFDEC_ABC_OBJECT_TRAITS (context)) {
    if (SWFDEC_AS_VALUE_IS_UNDEFINED (val)) {
      SWFDEC_AS_VALUE_SET_NULL (val);
    }
    return TRUE;
  }

  if (SWFDEC_AS_VALUE_IS_UNDEFINED (val) || SWFDEC_AS_VALUE_IS_NULL (val)) {
    if (traits == SWFDEC_ABC_VOID_TRAITS (context)) {
      SWFDEC_AS_VALUE_SET_UNDEFINED (val);
    } else {
      SWFDEC_AS_VALUE_SET_NULL (val);
    }
    return TRUE;
  }

  return swfdec_abc_value_is_traits (val, traits);
}

SwfdecAbcTraits *
swfdec_abc_traits_get_slot_traits (SwfdecAbcTraits *traits, guint slot)
{
  SwfdecAbcTrait *trait;
  guint i, slot_id, const_id;

  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), NULL);
  g_return_val_if_fail (traits->resolved, NULL);
  g_return_val_if_fail (slot < traits->n_slots, NULL);

  while (traits->base && traits->base->n_slots > slot)
    traits = traits->base;

  const_id = SWFDEC_ABC_BINDING_NEW (SWFDEC_ABC_TRAIT_CONST, slot);
  slot_id = SWFDEC_ABC_BINDING_NEW (SWFDEC_ABC_TRAIT_SLOT, slot);
  for (i = 0; i < traits->n_traits; i++) {
    trait = &traits->traits[i];
    if (trait->type == slot_id || trait->type == const_id)
      return trait->traits;
  }

  g_assert_not_reached ();
  return NULL;
}

