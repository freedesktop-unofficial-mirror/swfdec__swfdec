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

#include <swfdec/swfdec_as_types.h>
#include <swfdec/swfdec_gc_object.h>

#ifndef _SWFDEC_ABC_TRAITS_H_
#define _SWFDEC_ABC_TRAITS_H_

G_BEGIN_DECLS

typedef enum {
  SWFDEC_ABC_TRAIT_SLOT = 0,
  SWFDEC_ABC_TRAIT_METHOD = 1,
  SWFDEC_ABC_TRAIT_GETTER = 2,
  SWFDEC_ABC_TRAIT_SETTER = 3,
  SWFDEC_ABC_TRAIT_CLASS = 4,
  SWFDEC_ABC_TRAIT_FUNCTION = 5,
  SWFDEC_ABC_TRAIT_CONST = 6
} SwfdecAbcTraitType;

/* forward declaration */
typedef struct _SwfdecAbcFunction SwfdecAbcFunction;

typedef struct _SwfdecAbcTrait SwfdecAbcTrait;
typedef struct _SwfdecAbcTraits SwfdecAbcTraits;
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
  gboolean			final:1;	/* TRUE if trait is final */
  gboolean			override:1;	/* TRUE if trait overrides a base trait */
  guint				id;		/* id of data for type */
  guint				id_data;	/* relevant data for id */
};

struct _SwfdecAbcTraits {
  SwfdecGcObject		object;

  SwfdecAbcNamespace *		ns;		/* namespace of type we represent */
  const char *			name;		/* name of type we represent */
  SwfdecAbcTraits *		base;		/* parent type traits */
  SwfdecAbcFunction *		construct;	/* constructor for objects of these traits or NULL */
  SwfdecAbcNamespace *		protected_ns;	/* protected namespace */

  SwfdecAbcTrait *		traits;		/* the traits we have */
  guint				n_traits;	/* number of traits */

  gboolean			sealed:1;	/* cannot add properties to object */
  gboolean			final:1;	/* no derived traits */
  gboolean			interface:1;	/* traits describe an interface */
};

struct _SwfdecAbcTraitsClass {
  SwfdecGcObjectClass	object_class;
};

GType		swfdec_abc_traits_get_type	(void) G_GNUC_CONST;


G_END_DECLS

#endif
