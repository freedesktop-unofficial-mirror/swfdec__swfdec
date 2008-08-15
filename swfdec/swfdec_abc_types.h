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

#ifndef _SWFDEC_ABC_TYPES_H_
#define _SWFDEC_ABC_TYPES_H_

#include <glib-object.h>

G_BEGIN_DECLS

enum {
  SWFDEC_ABC_TYPE_OBJECT = 0,
  SWFDEC_ABC_TYPE_CLASS = 1,
  SWFDEC_ABC_TYPE_NAMESPACE = 2,
  SWFDEC_ABC_TYPE_FUNCTION = 3,
  SWFDEC_ABC_TYPE_METHOD_CLOSURE = 4,
  SWFDEC_ABC_TYPE_ERROR = 5,
  SWFDEC_ABC_TYPE_DEFINITION_ERROR = 6,
  SWFDEC_ABC_TYPE_EVAL_ERROR = 7,
  SWFDEC_ABC_TYPE_RANGE_ERROR = 8,
  SWFDEC_ABC_TYPE_REFERENCE_ERROR = 9,
  SWFDEC_ABC_TYPE_SECURITY_ERROR = 10,
  SWFDEC_ABC_TYPE_SYNTAX_ERROR = 11,
  SWFDEC_ABC_TYPE_TYPE_ERROR = 12,
  SWFDEC_ABC_TYPE_URI_ERROR = 13,
  SWFDEC_ABC_TYPE_VERIFY_ERROR = 14,
  SWFDEC_ABC_TYPE_UNINITIALIZED_ERROR = 15,
  SWFDEC_ABC_TYPE_ARGUMENT_ERROR = 16,
  SWFDEC_ABC_TYPE_STRING = 17,
  SWFDEC_ABC_TYPE_BOOLEAN = 18,
  SWFDEC_ABC_TYPE_NUMBER = 19,
  SWFDEC_ABC_TYPE_INT = 20,
  SWFDEC_ABC_TYPE_UINT = 21,
  SWFDEC_ABC_TYPE_MATH = 22,
  SWFDEC_ABC_TYPE_ARRAY = 23,
  SWFDEC_ABC_TYPE_REGEXP = 24,
  SWFDEC_ABC_TYPE_DATE = 25,
  SWFDEC_ABC_TYPE_XML = 26,
  SWFDEC_ABC_TYPE_XML_LIST = 27,
  SWFDEC_ABC_TYPE_QNAME = 28
};
#define SWFDEC_ABC_N_TYPES 29

typedef struct _SwfdecAbcClass SwfdecAbcClass;
typedef struct _SwfdecAbcFile SwfdecAbcFile;
typedef struct _SwfdecAbcFunction SwfdecAbcFunction;
typedef struct _SwfdecAbcMultiname SwfdecAbcMultiname;
typedef struct _SwfdecAbcObject SwfdecAbcObject;
typedef struct _SwfdecAbcScript SwfdecAbcScript;
typedef struct _SwfdecAbcTraits SwfdecAbcTraits;


G_END_DECLS
#endif
