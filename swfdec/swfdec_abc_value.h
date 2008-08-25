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

#ifndef _SWFDEC_ABC_VALUE_H_
#define _SWFDEC_ABC_VALUE_H_

#include <swfdec/swfdec_abc_types.h>
#include <swfdec/swfdec_as_types.h>

G_BEGIN_DECLS


typedef enum {
  SWFDEC_ABC_COMPARE_UNDEFINED,
  SWFDEC_ABC_COMPARE_THROWN,
  SWFDEC_ABC_COMPARE_LOWER,
  SWFDEC_ABC_COMPARE_EQUAL,
  SWFDEC_ABC_COMPARE_GREATER
} SwfdecAbcComparison;

gboolean		swfdec_abc_value_to_boolean	(SwfdecAsContext *	context,
							 const SwfdecAsValue *	value);
int			swfdec_abc_value_to_integer	(SwfdecAsContext *	context,
							 const SwfdecAsValue *	value);
double			swfdec_abc_value_to_number	(SwfdecAsContext *	context,
							 const SwfdecAsValue *	value);
SwfdecAsObject *	swfdec_abc_value_to_object	(SwfdecAsContext *	context,
							 const SwfdecAsValue *	value);
gboolean		swfdec_abc_value_to_primitive	(SwfdecAsValue *	dest,
							 const SwfdecAsValue *	src);
const char *		swfdec_abc_value_to_string	(SwfdecAsContext *	context,
							 const SwfdecAsValue *	value);
const char *		swfdec_abc_value_get_type_name	(const SwfdecAsValue *	value);

SwfdecAbcComparison	swfdec_abc_value_compare	(SwfdecAsContext *	context,
							 const SwfdecAsValue *	lval,
							 const SwfdecAsValue *	rval);


G_END_DECLS
#endif
