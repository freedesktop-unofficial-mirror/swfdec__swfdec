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

#ifndef _SWFDEC_ABC_INTERNAL_H_
#define _SWFDEC_ABC_INTERNAL_H_

#include <swfdec/swfdec_abc_global.h>
#include <swfdec/swfdec_abc_namespace.h>
#include <swfdec/swfdec_as_context.h>

G_BEGIN_DECLS

#define SWFDEC_ABC_NATIVE(id, func) void func (SwfdecAsContext *cx, \
    SwfdecAsObject *object, guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret);
#define SWFDEC_ABC_FLASH(id, func) void func (SwfdecAsContext *cx, \
    SwfdecAsObject *object, guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret);

/* This header contains all private symbols of Abc handling that should not be 
 * exported in the API */

/* swfdec_as_context.c */
void			swfdec_as_context_throw_abc		(SwfdecAsContext *	context,
								 guint			type,
								 const char *		format,
								 ...) G_GNUC_PRINTF (3, 4);
void			swfdec_as_context_throw_abcv		(SwfdecAsContext *	context,
								 guint			type,
								 const char *		format,
								 va_list		args);

SwfdecAbcNamespace *	swfdec_as_context_get_namespace		(SwfdecAsContext *	context,
								 SwfdecAbcNamespaceType	type,
								 const char *		prefix,
								 const char *		uri);
#define swfdec_as_context_get_abc_variable(context, mn, value) \
  swfdec_abc_global_get_script_variable (SWFDEC_ABC_GLOBAL (context->global), mn, value)


G_END_DECLS
#endif
