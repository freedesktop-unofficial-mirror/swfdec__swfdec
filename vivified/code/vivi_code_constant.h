/* Vivified
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

#ifndef _VIVI_CODE_CONSTANT_H_
#define _VIVI_CODE_CONSTANT_H_

#include <vivified/code/vivi_code_value.h>

G_BEGIN_DECLS


typedef struct _ViviCodeConstant ViviCodeConstant;
typedef struct _ViviCodeConstantClass ViviCodeConstantClass;

#define VIVI_TYPE_CODE_CONSTANT                    (vivi_code_constant_get_type())
#define VIVI_IS_CODE_CONSTANT(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIVI_TYPE_CODE_CONSTANT))
#define VIVI_IS_CODE_CONSTANT_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), VIVI_TYPE_CODE_CONSTANT))
#define VIVI_CODE_CONSTANT(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIVI_TYPE_CODE_CONSTANT, ViviCodeConstant))
#define VIVI_CODE_CONSTANT_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), VIVI_TYPE_CODE_CONSTANT, ViviCodeConstantClass))
#define VIVI_CODE_CONSTANT_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), VIVI_TYPE_CODE_CONSTANT, ViviCodeConstantClass))

struct _ViviCodeConstant
{
  ViviCodeValue		parent;

  SwfdecAsValue		value;
};

struct _ViviCodeConstantClass
{
  ViviCodeValueClass  	value_class;
};

GType			vivi_code_constant_get_type   	(void);

ViviCodeValue *		vivi_code_constant_new_null	(void);
ViviCodeValue *		vivi_code_constant_new_undefined(void);
ViviCodeValue *		vivi_code_constant_new_string	(const char *		string);
#define vivi_code_constant_new_int vivi_code_constant_new_number
ViviCodeValue *		vivi_code_constant_new_number	(double			number);
ViviCodeValue *		vivi_code_constant_new_boolean	(gboolean		boolean);

char *			vivi_code_constant_get_variable_name
							(ViviCodeConstant *	constant);
SwfdecAsValueType	vivi_code_constant_get_value_type
							(ViviCodeConstant *	constant);
double			vivi_code_constant_get_number	(ViviCodeConstant *	constant);
const char *		vivi_code_constant_get_string	(ViviCodeConstant *	constant);

G_END_DECLS
#endif