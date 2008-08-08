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

#ifndef _SWFDEC_ABC_GLOBAL_H_
#define _SWFDEC_ABC_GLOBAL_H_

#include <swfdec/swfdec_as_object.h>
#include <swfdec/swfdec_abc_multiname.h>
#include <swfdec/swfdec_abc_traits.h>

G_BEGIN_DECLS

typedef struct _SwfdecAbcGlobal SwfdecAbcGlobal;
typedef struct _SwfdecAbcGlobalClass SwfdecAbcGlobalClass;

#define SWFDEC_TYPE_ABC_GLOBAL                    (swfdec_abc_global_get_type())
#define SWFDEC_IS_ABC_GLOBAL(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_ABC_GLOBAL))
#define SWFDEC_IS_ABC_GLOBAL_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_ABC_GLOBAL))
#define SWFDEC_ABC_GLOBAL(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_ABC_GLOBAL, SwfdecAbcGlobal))
#define SWFDEC_ABC_GLOBAL_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_ABC_GLOBAL, SwfdecAbcGlobalClass))
#define SWFDEC_ABC_GLOBAL_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_ABC_GLOBAL, SwfdecAbcGlobalClass))

struct _SwfdecAbcGlobal {
  SwfdecAsObject	object;

  GPtrArray *		traits;		/* list of all traits - FIXME: needs fast index by ns/name */
  GArray *		scripts;	/* list of all scripts - FIXME: needs fast index by ns/name */
};

struct _SwfdecAbcGlobalClass {
  SwfdecAsObjectClass	object_class;
};

GType			swfdec_abc_global_get_type	(void);

SwfdecAsObject *	swfdec_abc_global_new		(SwfdecAsContext *		context);

void			swfdec_abc_global_add_traits	(SwfdecAbcGlobal *		global,
							 SwfdecAbcTraits *		traits);
SwfdecAbcTraits *	swfdec_abc_global_get_traits	(SwfdecAbcGlobal *		global,
							 SwfdecAbcNamespace *		ns,
							 const char *			name);
SwfdecAbcTraits *	swfdec_abc_global_get_traits_for_multiname
							(SwfdecAbcGlobal *		global,
							 const SwfdecAbcMultiname *	mn);
void	  		swfdec_abc_global_add_script	(SwfdecAbcGlobal *		global,
							 SwfdecAbcNamespace *		ns,
							 const char *			name,
							 SwfdecAbcFunction *		script,
							 gboolean			override);


G_END_DECLS
#endif
