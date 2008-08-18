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

#ifndef _SWFDEC_ABC_METHOD_H_
#define _SWFDEC_ABC_METHOD_H_

#include <swfdec/swfdec_abc_object.h>
#include <swfdec/swfdec_abc_scope_chain.h>
#include <swfdec/swfdec_abc_types.h>

G_BEGIN_DECLS

typedef struct _SwfdecAbcMethod SwfdecAbcMethod;
typedef struct _SwfdecAbcMethodClass SwfdecAbcMethodClass;

#define SWFDEC_TYPE_ABC_METHOD                    (swfdec_abc_method_get_type())
#define SWFDEC_IS_ABC_METHOD(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_ABC_METHOD))
#define SWFDEC_IS_ABC_METHOD_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_ABC_METHOD))
#define SWFDEC_ABC_METHOD(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_ABC_METHOD, SwfdecAbcMethod))
#define SWFDEC_ABC_METHOD_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_ABC_METHOD, SwfdecAbcMethodClass))
#define SWFDEC_ABC_METHOD_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_ABC_METHOD, SwfdecAbcMethodClass))

struct _SwfdecAbcMethod {
  SwfdecAbcObject	object;

  SwfdecAbcFunction *	function;	/* the function we call */
  SwfdecAbcScopeChain *	scope;		/* scope chain */
};

struct _SwfdecAbcMethodClass {
  SwfdecAbcObjectClass	object_class;
};

GType			swfdec_abc_method_get_type	(void);

SwfdecAbcMethod *	swfdec_abc_method_new		(SwfdecAbcFunction *	function,
							 SwfdecAbcScopeChain *	chain);

void			swfdec_abc_method_call		(SwfdecAbcMethod *	method,
							 guint			argc,
							 SwfdecAsValue *	argv,
							 SwfdecAsValue *	ret);


G_END_DECLS
#endif
