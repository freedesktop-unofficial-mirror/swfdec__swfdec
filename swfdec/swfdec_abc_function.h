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
#include <swfdec/swfdec_buffer.h>

G_BEGIN_DECLS

//typedef struct _SwfdecAbcFunction SwfdecAbcFunction;
typedef struct _SwfdecAbcFunctionClass SwfdecAbcFunctionClass;
typedef struct _SwfdecAbcFunctionArgument SwfdecAbcFunctionArgument;
typedef struct _SwfdecAbcException SwfdecAbcException;

#define SWFDEC_TYPE_ABC_FUNCTION                    (swfdec_abc_function_get_type())
#define SWFDEC_IS_ABC_FUNCTION(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_ABC_FUNCTION))
#define SWFDEC_IS_ABC_FUNCTION_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_ABC_FUNCTION))
#define SWFDEC_ABC_FUNCTION(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_ABC_FUNCTION, SwfdecAbcFunction))
#define SWFDEC_ABC_FUNCTION_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_ABC_FUNCTION, SwfdecAbcFunctionClass))
#define SWFDEC_ABC_FUNCTION_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_ABC_FUNCTION, SwfdecAbcFunctionClass))

struct _SwfdecAbcFunctionArgument {
  union {
    SwfdecAbcMultiname *type;			/* multiname if not resolved */
    SwfdecAbcTraits *	traits;			/* traits pointer if resolved */
  };
  const char *		name;			/* name of argument or NULL if not given or read error */
  union {
    struct {
      guint		default_index;		/* index of default value if not resolved */
      guint8		default_type;		/* type of default value if not resolved */
    };
    SwfdecAsValue	default_value;		/* default value if resolved */
  };
};

struct _SwfdecAbcException {
  guint			from;
  guint			to;
  guint			target;
  guint			type;
  guint			var;
};

struct _SwfdecAbcFunction {
  SwfdecGcObject	object;

  const char *		name;			/* gc'd name of the function or NULL if unnamed */
  gboolean		verified:1;		/* function body has been verified */
  gboolean		resolved:1;		/* function arguments and return type have been resolved */
  gboolean		need_arguments:1;	/* needs arguments object */
  gboolean		need_rest:1;		/* needs rest object */
  gboolean		need_activation:1;	/* needs activation object */
  gboolean		set_dxns:1;		/* sets dynamic xml namespace */

  union {
    SwfdecAbcMultiname *return_type;		/* multiname if not resolved */
    SwfdecAbcTraits *	return_traits;		/* traits pointer if resolved */
  };
  guint			n_args;			/* number of arguments */
  guint			min_args;     		/* minimum required number of arguments */
  SwfdecAbcFunctionArgument *args;		/* n_args arguments */ 

  SwfdecAbcTraits *	bound_traits;		/* traits of objects we construct or NULL */
  
  /* native functions only */
  GCallback		native;			/* SwfdecAsNative for now - will become native when we can marhsal */
  /* functions with body only */
  SwfdecBuffer *	code;			/* the code to be executed */
  guint			n_stack;		/* max number of values on stack */
  guint			n_scope;		/* number of scope values necessary */
  guint			n_locals;		/* number of local variables */
  SwfdecAbcTraits *	activation;		/* traits of activation object */
  guint			n_exceptions;		/* number of exceptions */
  SwfdecAbcException *	exceptions;		/* the exceptions */
};

struct _SwfdecAbcFunctionClass {
  SwfdecGcObjectClass	object_class;
};

GType			swfdec_abc_function_get_type	(void);

gboolean		swfdec_abc_function_bind	(SwfdecAbcFunction *	fun,
							 SwfdecAbcTraits *	traits);
gboolean		swfdec_abc_function_is_native	(SwfdecAbcFunction *	fun);
gboolean		swfdec_abc_function_resolve	(SwfdecAbcFunction *	fun);
gboolean		swfdec_abc_function_verify	(SwfdecAbcFunction *	fun);


G_END_DECLS
#endif
