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
#include "swfdec_abc_internal.h"
#include "swfdec_abc_multiname.h"
#include "swfdec_abc_traits.h"
#include "swfdec_as_context.h"
#include "swfdec_debug.h"

enum {
  PROP_0,
  PROP_TRAITS
};

G_DEFINE_ABSTRACT_TYPE (SwfdecAbcObject, swfdec_abc_object, SWFDEC_TYPE_AS_OBJECT)

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
swfdec_abc_object_new (SwfdecAbcTraits *traits)
{
  SwfdecAbcObject *object;

  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), NULL);

  if (!swfdec_abc_traits_resolve (traits))
    return NULL;

  object = g_object_new (traits->type_func (), 
      "context", swfdec_gc_object_get_context (traits),
      "traits", traits, NULL);

  return object;
}

gboolean
swfdec_abc_object_get_variable (SwfdecAbcObject *object, const SwfdecAbcMultiname *mn,
    SwfdecAsValue *value)
{
  const SwfdecAbcTrait *trait;

  g_return_val_if_fail (SWFDEC_IS_ABC_OBJECT (object), FALSE);
  g_return_val_if_fail (mn != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  trait = swfdec_abc_traits_find_trait_multi (object->traits, mn);
  if (!trait) {
    if (object->traits->sealed ||
	!swfdec_abc_multiname_contains_namespace (mn,
	  swfdec_gc_object_get_context (object)->public_ns)) {
      swfdec_as_context_throw_abc (swfdec_gc_object_get_context (object),
	  SWFDEC_ABC_ERROR_REFERENCE, "Property %s not found on %s and there is no default value.", 
	  mn->name, object->traits->name);
      SWFDEC_AS_VALUE_SET_UNDEFINED (value);
      return FALSE;
    }
    return swfdec_as_object_get_variable (SWFDEC_AS_OBJECT (object),
	mn->name, value);
  } else if (trait == SWFDEC_ABC_TRAIT_AMBIGUOUS) {
    swfdec_as_context_throw_abc (swfdec_gc_object_get_context (object),
	SWFDEC_ABC_ERROR_REFERENCE, "%s is ambiguous; Found more than one matching binding.", mn->name);
    SWFDEC_AS_VALUE_SET_UNDEFINED (value);
    return FALSE;
  } else {
    switch (SWFDEC_ABC_BINDING_GET_TYPE (trait->type)) {
      case SWFDEC_ABC_TRAIT_SLOT:
      case SWFDEC_ABC_TRAIT_CONST:
	{
	  guint slot = SWFDEC_ABC_BINDING_GET_ID (trait->type);
	  g_assert (slot < object->traits->n_slots);
	  *value = object->slots[slot];
	}
	break;
      case SWFDEC_ABC_TRAIT_METHOD:
      case SWFDEC_ABC_TRAIT_GET:
      case SWFDEC_ABC_TRAIT_GETSET:
	SWFDEC_FIXME ("implement vtables");
	SWFDEC_AS_VALUE_SET_UNDEFINED (value);
	break;
      case SWFDEC_ABC_TRAIT_SET:
	swfdec_as_context_throw_abc (swfdec_gc_object_get_context (object),
	    SWFDEC_ABC_ERROR_REFERENCE, "Illegal read of write-only property %s on %s.", mn->name, object->traits->name);
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

