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

#ifndef _SWFDEC_ABC_OBJECT_H_
#define _SWFDEC_ABC_OBJECT_H_

#include <swfdec/swfdec_as_object.h>
#include <swfdec/swfdec_abc_types.h>

G_BEGIN_DECLS

//typedef struct _SwfdecAbcObject SwfdecAbcObject;
typedef struct _SwfdecAbcObjectClass SwfdecAbcObjectClass;

#define SWFDEC_TYPE_ABC_OBJECT                    (swfdec_abc_object_get_type())
#define SWFDEC_IS_ABC_OBJECT(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_ABC_OBJECT))
#define SWFDEC_IS_ABC_OBJECT_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_ABC_OBJECT))
#define SWFDEC_ABC_OBJECT(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_ABC_OBJECT, SwfdecAbcObject))
#define SWFDEC_ABC_OBJECT_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_ABC_OBJECT, SwfdecAbcObjectClass))
#define SWFDEC_ABC_OBJECT_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_ABC_OBJECT, SwfdecAbcObjectClass))

struct _SwfdecAbcObject {
  SwfdecAsObject	object;

  SwfdecAbcTraits *	traits;		/* the traits that created us - we have a ref so it stays alive until we dispose */
  SwfdecAbcScopeChain *	scope;		/* the scope chain to use for slot functions */
  SwfdecAsValue *	slots;		/* traits->n_slots values */
};

struct _SwfdecAbcObjectClass {
  SwfdecAsObjectClass	object_class;

  gboolean		(* call)			(SwfdecAbcObject *		object,
							 guint				argc,
							 SwfdecAsValue *		argv,
							 SwfdecAsValue *		ret);
};

GType			swfdec_abc_object_get_type	(void);

SwfdecAbcObject *	swfdec_abc_object_new		(SwfdecAbcTraits *		traits,
							 SwfdecAbcScopeChain *		scopes);
SwfdecAbcObject *	swfdec_abc_object_new_from_class(SwfdecAbcClass *		classp);

gboolean		swfdec_abc_object_call		(SwfdecAbcObject *		object,
							 guint				argc,
							 SwfdecAsValue *		argv,
							 SwfdecAsValue *		ret);

gboolean		swfdec_abc_object_get_variable	(SwfdecAsContext *		context,
							 const SwfdecAsValue *		object,
							 const SwfdecAbcMultiname *	mn,
							 SwfdecAsValue *		value);
gboolean		swfdec_abc_object_set_variable	(SwfdecAsContext *		context,
							 const SwfdecAsValue *		object,
							 const SwfdecAbcMultiname *	mn,
							 const SwfdecAsValue *		value);
gboolean		swfdec_abc_object_init_variable	(SwfdecAsContext *		context,
							 const SwfdecAsValue *		object,
							 const SwfdecAbcMultiname *	mn,
							 const SwfdecAsValue *		value);
gboolean		swfdec_abc_object_call_variable	(SwfdecAsContext *		context,
							 const SwfdecAsValue *		object,
							 const SwfdecAbcMultiname *	mn,
							 guint				argc,
							 SwfdecAsValue *		argv,
							 SwfdecAsValue *		ret);


G_END_DECLS
#endif
