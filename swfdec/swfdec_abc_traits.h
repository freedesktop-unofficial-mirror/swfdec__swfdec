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

#include <swfdec/swfdec_abc_types.h>
#include <swfdec/swfdec_as_types.h>
#include <swfdec/swfdec_gc_object.h>

#ifndef _SWFDEC_ABC_TRAITS_H_
#define _SWFDEC_ABC_TRAITS_H_

G_BEGIN_DECLS

typedef enum {
  SWFDEC_ABC_POINTER = 0,
  SWFDEC_ABC_INT,
  SWFDEC_ABC_UINT,
  SWFDEC_ABC_DOUBLE,
  SWFDEC_ABC_STRING,
  SWFDEC_ABC_VOID
} SwfdecAbcMachineType;

/* NB: The indexes for the binding types have a lot of magic associated with 
 * them, so be sure to update this magic. Examples:
 * (type & 6) == 2 => slot or const
 * type > 4 => accessor
 * type | SWFDEC_ABC_TRAIT_GET => make getter out of any accessor
 */
typedef enum {
  SWFDEC_ABC_TRAIT_NONE = 0,
  SWFDEC_ABC_TRAIT_METHOD = 1,
  SWFDEC_ABC_TRAIT_SLOT = 2,
  SWFDEC_ABC_TRAIT_CONST = 3,
  SWFDEC_ABC_TRAIT_ITRAMP = 4,
  SWFDEC_ABC_TRAIT_GET = 5,
  SWFDEC_ABC_TRAIT_SET = 6,
  SWFDEC_ABC_TRAIT_GETSET = 7,
} SwfdecAbcTraitType;

typedef guint SwfdecAbcBinding;

#define SWFDEC_ABC_BINDING_NEW(type, id) ((id) << 3 | (type))
#define SWFDEC_ABC_BINDING_NONE SWFDEC_ABC_BINDING_NEW (SWFDEC_ABC_TRAIT_NONE, 0)
#define SWFDEC_ABC_BINDING_GET_ID(bind) ((bind) >> 3)
#define SWFDEC_ABC_BINDING_GET_TYPE(bind) ((SwfdecAbcTraitType) ((bind) & 7))
#define SWFDEC_ABC_BINDING_IS_TYPE(bind, type) (((bind) & 7) == (type))
#define SWFDEC_ABC_BINDING_IS_ACCESSOR(bind) (SWFDEC_ABC_BINDING_GET_TYPE (bind) > 4)

typedef struct _SwfdecAbcTrait SwfdecAbcTrait;
//typedef struct _SwfdecAbcTraits SwfdecAbcTraits;
typedef struct _SwfdecAbcTraitsClass SwfdecAbcTraitsClass;

#define SWFDEC_TYPE_ABC_TRAITS                    (swfdec_abc_traits_get_type())
#define SWFDEC_IS_ABC_TRAITS(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_ABC_TRAITS))
#define SWFDEC_IS_ABC_TRAITS_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_ABC_TRAITS))
#define SWFDEC_ABC_TRAITS(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_ABC_TRAITS, SwfdecAbcTraits))
#define SWFDEC_ABC_TRAITS_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_ABC_TRAITS, SwfdecAbcTraitsClass))
#define SWFDEC_ABC_TRAITS_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_ABC_TRAITS, SwfdecAbcTraitsClass))

struct _SwfdecAbcTrait {
  SwfdecAbcTraitType		type;		/* type of trait */
  gsize				offset;		/* offset into allocated memory or 0 if not verified */
  SwfdecAbcNamespace *		ns;		/* namespace described by this trait */
  const char *			name;		/* name described by this trait */
  gboolean			final:1;	/* TRUE if trait is final */
  gboolean			override:1;	/* TRUE if trait overrides a base trait */
  union {
    guint			traits_type;	/* if not resolved: index into multiname array of pool */
    SwfdecAbcTraits *		traits;		/* if resolved: traits describing this trait */
  };
  guint				default_index;	/* if not resolved: index into array */
  guint	  			default_type;	/* if not resolved: for slots the type of array to index, for methods 
						   and getters unused, for setters index into function array (yes, i know this sucks) */
};
extern const SwfdecAbcTrait swfdec_abc_trait_ambiguous;
#define SWFDEC_ABC_TRAIT_AMBIGUOUS (&swfdec_abc_trait_ambiguous)

struct _SwfdecAbcTraits {
  SwfdecGcObject		object;

  SwfdecAbcPool *		pool;		/* pool that defined this traits */
  SwfdecAbcNamespace *		ns;		/* namespace of type we represent */
  const char *			name;		/* name of type we represent */
  SwfdecAbcTraits *		base;		/* parent type traits */
  SwfdecAbcTraits *		instance_traits;/* traits for instances we construct or NULL */
  SwfdecAbcFunction *		construct;	/* constructor for objects of these traits or NULL */
  SwfdecAbcNamespace *		protected_ns;	/* protected namespace */
  GType				(* type_func)	(void);	/* get_type function for the object type we create */
  SwfdecAbcMachineType		machine_type;	/* how this type is represented */

  SwfdecAbcTrait *		traits;		/* the traits we have */
  guint				n_traits;	/* number of traits */

  guint				n_slots;	/* number of slots */
  guint				n_methods;	/* number of methods */
  SwfdecAsValue *		slots;		/* if resolved: n_slots default values */
  SwfdecAbcFunction **		methods;	/* if resolved: n_methods methods */

  gboolean			sealed:1;	/* cannot add properties to object */
  gboolean			final:1;	/* no derived traits */
  gboolean			interface:1;	/* traits describe an interface */
  gboolean			resolved:1;	/* types are resolved */
};

struct _SwfdecAbcTraitsClass {
  SwfdecGcObjectClass	object_class;
};

GType			swfdec_abc_traits_get_type		(void) G_GNUC_CONST;

gboolean		swfdec_abc_traits_allow_early_binding	(SwfdecAbcTraits *	traits);
gboolean		swfdec_abc_traits_resolve		(SwfdecAbcTraits *	traits);

const SwfdecAbcTrait *	swfdec_abc_traits_get_trait		(SwfdecAbcTraits *	traits,
								 SwfdecAbcNamespace *	ns,
								 const char *		name);
const SwfdecAbcTrait *	swfdec_abc_traits_find_trait		(SwfdecAbcTraits *	traits,
								 SwfdecAbcNamespace *	ns,
								 const char *		name);
const SwfdecAbcTrait *	swfdec_abc_traits_find_trait_multi	(SwfdecAbcTraits *	traits,
								 const SwfdecAbcMultiname *multi);

gboolean		swfdec_abc_traits_is_traits		(SwfdecAbcTraits *	traits,
								 SwfdecAbcTraits *	parent);
gboolean		swfdec_abc_traits_coerce		(SwfdecAbcTraits *	traits,
								 SwfdecAsValue *	val);
/* FIXME: kinda wrong header */
SwfdecAbcTraits *	swfdec_as_value_to_traits		(SwfdecAsContext *	context,
								 const SwfdecAsValue *	value);
gboolean		swfdec_as_value_is_traits		(const SwfdecAsValue *	value,
								 SwfdecAbcTraits *	traits);


G_END_DECLS

#endif
