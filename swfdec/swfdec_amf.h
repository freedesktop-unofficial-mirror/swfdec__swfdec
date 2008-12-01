/* Swfdec
 * Copyright (C) 2007-2008 Benjamin Otte <otte@gnome.org>
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

#ifndef __SWFDEC_AMF_H__
#define __SWFDEC_AMF_H__

#include <swfdec/swfdec_as_context.h>
#include <swfdec/swfdec_bits.h>
#include <swfdec/swfdec_bots.h>
#include <swfdec/swfdec_xml.h>

G_BEGIN_DECLS

typedef enum {
  SWFDEC_AMF_NUMBER = 0,
  SWFDEC_AMF_BOOLEAN = 1,
  SWFDEC_AMF_STRING = 2,
  SWFDEC_AMF_OBJECT = 3,
  SWFDEC_AMF_MOVIECLIP = 4,
  SWFDEC_AMF_NULL = 5,
  SWFDEC_AMF_UNDEFINED = 6,
  SWFDEC_AMF_REFERENCE = 7,
  SWFDEC_AMF_SPARSE_ARRAY = 8,
  SWFDEC_AMF_END_OBJECT = 9,
  SWFDEC_AMF_DENSE_ARRAY = 10,
  SWFDEC_AMF_DATE = 11,
  SWFDEC_AMF_BIG_STRING = 12,
  SWFDEC_AMF_UNSUPPORTED = 13,
  SWFDEC_AMF_RECORDSET = 14,
  SWFDEC_AMF_XML = 15,
  SWFDEC_AMF_CLASS = 16,
  SWFDEC_AMF_FLASH9 = 17
} SwfdecAmfType;

typedef struct _SwfdecAmfContext SwfdecAmfContext;

SwfdecAmfContext *	swfdec_amf_context_new		(SwfdecAsContext *	context);
void			swfdec_amf_context_free		(SwfdecAmfContext *	context);

gboolean		swfdec_amf_decode		(SwfdecAmfContext *	context,
							 SwfdecBits *		bits, 
							 SwfdecAsValue *	rval);

gboolean		swfdec_amf_encode		(SwfdecAmfContext *	context, 
							 SwfdecBots *		bots,
							 SwfdecAsValue		val);


G_END_DECLS

#endif
