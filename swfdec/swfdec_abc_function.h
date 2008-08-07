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

#ifndef _SWFDEC_ABC_FUNCTION_H_
#define _SWFDEC_ABC_FUNCTION_H_

#include <swfdec/swfdec_abc_types.h>
#include <swfdec/swfdec_as_function.h>

G_BEGIN_DECLS

//typedef struct _SwfdecAbcFunction SwfdecAbcFunction;
typedef struct _SwfdecAbcFunctionClass SwfdecAbcFunctionClass;
typedef struct _SwfdecAbcFunctionArgument SwfdecAbcFunctionArgument;

#define SWFDEC_TYPE_ABC_FUNCTION                    (swfdec_abc_function_get_type())
#define SWFDEC_IS_ABC_FUNCTION(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_ABC_FUNCTION))
#define SWFDEC_IS_ABC_FUNCTION_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_ABC_FUNCTION))
#define SWFDEC_ABC_FUNCTION(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_ABC_FUNCTION, SwfdecAbcFunction))
#define SWFDEC_ABC_FUNCTION_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_ABC_FUNCTION, SwfdecAbcFunctionClass))
#define SWFDEC_ABC_FUNCTION_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_ABC_FUNCTION, SwfdecAbcFunctionClass))

struct _SwfdecAbcFunctionArgument {
  gpointer		type;			/* trait pointer if verified, multiname otherwise */
  const char *		name;			/* name of argument or NULL if not given or read error */
  guint			default_index;		/* index of default value */
  guint8		default_type;		/* type of default value */
};

struct _SwfdecAbcFunction {
  SwfdecAsFunction	function;

  const char *		name;			/* gc'd name of the function or NULL if unnamed */
  gboolean		verified:1;		/* function body has been verified */
  gboolean		need_arguments:1;	/* needs arguments object */
  gboolean		need_rest:1;		/* needs rest object */
  gboolean		need_activation:1;	/* needs activation object */
  gboolean		set_dxns:1;		/* sets dynamic xml namespace */

  gpointer		return_type;		/* trait pointer if verified, multiname otherwise */
  guint			n_args;			/* number of arguments */
  SwfdecAbcFunctionArgument *args;		/* n_args arguments */ 

  SwfdecAbcTraits *	bound_traits;		/* traits of objects we construct or NULL */
};

struct _SwfdecAbcFunctionClass {
  SwfdecAsFunctionClass	function_class;
};

GType			swfdec_abc_function_get_type	(void);

gboolean		swfdec_abc_function_bind	(SwfdecAbcFunction *	fun,
							 SwfdecAbcTraits *	traits);


G_END_DECLS
#endif
