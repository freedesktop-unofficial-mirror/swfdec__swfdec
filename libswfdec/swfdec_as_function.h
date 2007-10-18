/* Swfdec
 * Copyright (C) 2007 Benjamin Otte <otte@gnome.org>
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

#ifndef _SWFDEC_AS_FUNCTION_H_
#define _SWFDEC_AS_FUNCTION_H_

#include <libswfdec/swfdec_as_object.h>
#include <libswfdec/swfdec_as_types.h>
#include <libswfdec/swfdec_script.h>

G_BEGIN_DECLS

typedef struct _SwfdecAsFunctionClass SwfdecAsFunctionClass;

#define SWFDEC_TYPE_AS_FUNCTION                    (swfdec_as_function_get_type())
#define SWFDEC_IS_AS_FUNCTION(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_AS_FUNCTION))
#define SWFDEC_IS_AS_FUNCTION_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_AS_FUNCTION))
#define SWFDEC_AS_FUNCTION(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_AS_FUNCTION, SwfdecAsFunction))
#define SWFDEC_AS_FUNCTION_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_AS_FUNCTION, SwfdecAsFunctionClass))
#define SWFDEC_AS_FUNCTION_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_AS_FUNCTION, SwfdecAsFunctionClass))

/* FIXME: do two obejcts, one for scripts and one for native? */
struct _SwfdecAsFunction {
  /*< private >*/
  SwfdecAsObject	object;

  gpointer		priv;		/* currently only contains the security context or NULL for any */
};

struct _SwfdecAsFunctionClass {
  SwfdecAsObjectClass	object_class;

  /* return a frame that calls this function or NULL if uncallable */
  SwfdecAsFrame *	(* call)			(SwfdecAsFunction *	function);
};

GType			swfdec_as_function_get_type	(void);

void			swfdec_as_function_call		(SwfdecAsFunction *	function,
							 SwfdecAsObject *	thisp,
							 guint			n_args,
							 const SwfdecAsValue *	args,
							 SwfdecAsValue *	return_value);


G_END_DECLS
#endif
