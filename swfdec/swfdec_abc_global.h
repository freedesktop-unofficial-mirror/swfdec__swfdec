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

#include <swfdec/swfdec_abc_multiname.h>
#include <swfdec/swfdec_abc_object.h>
#include <swfdec/swfdec_abc_types.h>

G_BEGIN_DECLS

/* CAREFUL: These macros cause SEGVs before the global->file is set */
#define SWFDEC_ABC_BUILTIN_TRAITS(context, type) \
  swfdec_abc_global_get_builtin_traits (SWFDEC_ABC_GLOBAL ((context)->global), type)
#define SWFDEC_ABC_OBJECT_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_OBJECT)
#define SWFDEC_ABC_CLASS_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_CLASS)
#define SWFDEC_ABC_NAMESPACE_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_NAMESPACE)
#define SWFDEC_ABC_FUNCTION_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_FUNCTION)
#define SWFDEC_ABC_METHOD_CLOSURE_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_METHOD_CLOSURE)
#define SWFDEC_ABC_ERROR_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_ERROR)
#define SWFDEC_ABC_DEFINITION_ERROR_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_DEFINITION_ERROR)
#define SWFDEC_ABC_EVAL_ERROR_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_EVAL_ERROR)
#define SWFDEC_ABC_RANGE_ERROR_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_RANGE_ERROR)
#define SWFDEC_ABC_REFERENCE_ERROR_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_REFERENCE_ERROR)
#define SWFDEC_ABC_SECURITY_ERROR_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_SECURITY_ERROR)
#define SWFDEC_ABC_SYNTAX_ERROR_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_SYNTAX_ERROR)
#define SWFDEC_ABC_TYPE_ERROR_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_TYPE_ERROR)
#define SWFDEC_ABC_URI_ERROR_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_URI_ERROR)
#define SWFDEC_ABC_VERIFY_ERROR_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_VERIFY_ERROR)
#define SWFDEC_ABC_UNINITIALIZED_ERROR_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_UNINITIALIZED_ERROR)
#define SWFDEC_ABC_ARGUMENT_ERROR_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_ARGUMENT_ERROR)
#define SWFDEC_ABC_STRING_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_STRING)
#define SWFDEC_ABC_BOOLEAN_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_BOOLEAN)
#define SWFDEC_ABC_NUMBER_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_NUMBER)
#define SWFDEC_ABC_INT_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_INT)
#define SWFDEC_ABC_UINT_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_UINT)
#define SWFDEC_ABC_MATH_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_MATH)
#define SWFDEC_ABC_ARRAY_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_ARRAY)
#define SWFDEC_ABC_REGEXP_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_REGEXP)
#define SWFDEC_ABC_DATE_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_DATE)
#define SWFDEC_ABC_XML_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_XML)
#define SWFDEC_ABC_XML_LIST_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_XML_LIST)
#define SWFDEC_ABC_QNAME_TRAITS(context) SWFDEC_ABC_BUILTIN_TRAITS(context, SWFDEC_ABC_TYPE_QNAME)
#define SWFDEC_ABC_VOID_TRAITS(context) (SWFDEC_ABC_GLOBAL ((context)->global)->void_traits)
#define SWFDEC_ABC_NULL_TRAITS(context) (SWFDEC_ABC_GLOBAL ((context)->global)->null_traits)

typedef struct _SwfdecAbcGlobal SwfdecAbcGlobal;
typedef struct _SwfdecAbcGlobalClass SwfdecAbcGlobalClass;

#define SWFDEC_TYPE_ABC_GLOBAL                    (swfdec_abc_global_get_type())
#define SWFDEC_IS_ABC_GLOBAL(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_ABC_GLOBAL))
#define SWFDEC_IS_ABC_GLOBAL_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_ABC_GLOBAL))
#define SWFDEC_ABC_GLOBAL(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_ABC_GLOBAL, SwfdecAbcGlobal))
#define SWFDEC_ABC_GLOBAL_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_ABC_GLOBAL, SwfdecAbcGlobalClass))
#define SWFDEC_ABC_GLOBAL_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_ABC_GLOBAL, SwfdecAbcGlobalClass))

struct _SwfdecAbcGlobal {
  SwfdecAbcObject	object;

  SwfdecAbcFile	*	file;		/* file that read the default objects */
  SwfdecAbcTraits *	void_traits;	/* the void traits */
  SwfdecAbcTraits *	null_traits;	/* the null traits */
  GPtrArray *		traits;		/* list of all traits - FIXME: needs fast index by ns/name */
  GArray *		scripts;	/* list of all scripts - FIXME: needs fast index by ns/name */
};

struct _SwfdecAbcGlobalClass {
  SwfdecAbcObjectClass	object_class;
};

GType			swfdec_abc_global_get_type	(void);

void			swfdec_abc_global_new		(SwfdecAsContext *		context);

SwfdecAbcTraits *	swfdec_abc_global_get_builtin_traits 
							(SwfdecAbcGlobal *		global,
							 guint				id);
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
							 SwfdecAbcScript *		script,
							 gboolean			override);

gboolean		swfdec_abc_global_get_script_variable
							(SwfdecAbcGlobal *		global,
							 const SwfdecAbcMultiname *	mn,
							 SwfdecAsValue *		value);
							


G_END_DECLS
#endif
