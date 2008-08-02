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

#ifndef _SWFDEC_ABC_NAMESPACE_H_
#define _SWFDEC_ABC_NAMESPACE_H_

G_BEGIN_DECLS

typedef enum {
  SWFDEC_ABC_NAMESPACE_PUBLIC = 0,
  SWFDEC_ABC_NAMESPACE_PROTECTED = 1,
  SWFDEC_ABC_NAMESPACE_PACKAGE = 2,
  SWFDEC_ABC_NAMESPACE_PRIVATE = 3,
  SWFDEC_ABC_NAMESPACE_EXPLICIT = 4,
  SWFDEC_ABC_NAMESPACE_STATIC_PROTECTED = 5
} SwfdecAbcNamespaceType;

//typedef struct _SwfdecAbcNamespace SwfdecAbcNamespace;
typedef struct _SwfdecAbcNamespaceClass SwfdecAbcNamespaceClass;

#define SWFDEC_TYPE_ABC_NAMESPACE                    (swfdec_abc_namespace_get_type())
#define SWFDEC_IS_ABC_NAMESPACE(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_ABC_NAMESPACE))
#define SWFDEC_IS_ABC_NAMESPACE_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_ABC_NAMESPACE))
#define SWFDEC_ABC_NAMESPACE(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_ABC_NAMESPACE, SwfdecAbcNamespace))
#define SWFDEC_ABC_NAMESPACE_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_ABC_NAMESPACE, SwfdecAbcNamespaceClass))
#define SWFDEC_ABC_NAMESPACE_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_ABC_NAMESPACE, SwfdecAbcNamespaceClass))

struct _SwfdecAbcNamespace {
  SwfdecGcObject		object;

  SwfdecAbcNamespaceType	type;
  const char *			prefix;	/* prefix of namespace or NULL */
  const char *			uri;	/* uri */
};

struct _SwfdecAbcNamespaceClass {
  SwfdecGcObjectClass	object_class;
};

GType		swfdec_abc_namespace_get_type	(void) G_GNUC_CONST;

SwfdecAbcNamespace *
		swfdec_abc_namespace_new	(SwfdecAsContext *		context,
						 SwfdecAbcNamespaceType		type,
						 const char *			prefix,
						 const char *			uri);

gboolean	swfdec_abc_namespace_equal	(const SwfdecAbcNamespace *	a,
						 const SwfdecAbcNamespace *	b);


G_END_DECLS

#endif
