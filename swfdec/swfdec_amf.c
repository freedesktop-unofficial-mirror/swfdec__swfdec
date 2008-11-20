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

#ifndef HAVE_CONFIG_H
#include "config.h"
#endif

#include "swfdec_amf.h"
#include "swfdec_as_array.h"
#include "swfdec_as_date.h"
#include "swfdec_as_internal.h"
#include "swfdec_as_strings.h"
#include "swfdec_debug.h"

/*** decoding ***/

static gboolean
swfdec_amf_decode_boolean (SwfdecAsContext *context, SwfdecBits *bits, SwfdecAsValue *val)
{
  SWFDEC_AS_VALUE_SET_BOOLEAN (val, swfdec_bits_get_u8 (bits) ? TRUE : FALSE);
  return TRUE;
}

static gboolean
swfdec_amf_decode_number (SwfdecAsContext *context, SwfdecBits *bits, SwfdecAsValue *val)
{
  *val = swfdec_as_value_from_number (context, swfdec_bits_get_bdouble (bits));
  return TRUE;
}

static gboolean
swfdec_amf_decode_string (SwfdecAsContext *context, SwfdecBits *bits, SwfdecAsValue *val)
{
  guint len = swfdec_bits_get_bu16 (bits);
  char *s;
  
  /* FIXME: the supplied version is likely incorrect */
  s = swfdec_bits_get_string_length (bits, len, context->version);
  if (s == NULL)
    return FALSE;
  SWFDEC_AS_VALUE_SET_STRING (val, swfdec_as_context_give_string (context, s));
  return TRUE;
}

static gboolean
swfdec_amf_decode_properties (SwfdecAsContext *context, SwfdecBits *bits, SwfdecAsObject *object)
{
  while (swfdec_bits_left (bits)) {
    SwfdecAsValue val;
    const char *name;

    if (!swfdec_amf_decode_string (context, bits, &val))
      return FALSE;
    name = SWFDEC_AS_VALUE_GET_STRING (val);
    /* FIXME: can we integrate this into swfdec_amf_decode() somehow? */
    if (swfdec_bits_peek_u8 (bits) == SWFDEC_AMF_END_OBJECT)
      break;
    if (!swfdec_amf_decode (context, bits, &val))
      goto error;
    swfdec_as_object_set_variable (object, name, &val);
  }
  /* no more bytes seems to end automatically */
  return TRUE;

error:
  return FALSE;
}

static gboolean
swfdec_amf_decode_object (SwfdecAsContext *context, SwfdecBits *bits, SwfdecAsValue *val)
{
  SwfdecAsObject *object;
  
  object = swfdec_as_object_new (context, SWFDEC_AS_STR_Object, NULL);
  if (!swfdec_amf_decode_properties (context, bits, object))
    return FALSE;
  SWFDEC_AS_VALUE_SET_OBJECT (val, object);
  return TRUE;
}

static gboolean
swfdec_amf_decode_mixed_array (SwfdecAsContext *context, SwfdecBits *bits, SwfdecAsValue *val)
{
  guint len;
  SwfdecAsObject *array;
  
  len = swfdec_bits_get_bu32 (bits);
  array = swfdec_as_array_new (context);
  if (!swfdec_amf_decode_properties (context, bits, array))
    return FALSE;
  SWFDEC_AS_VALUE_SET_OBJECT (val, array);
  return TRUE;
}

static gboolean
swfdec_amf_decode_array (SwfdecAsContext *context, SwfdecBits *bits, SwfdecAsValue *val)
{
  guint i, len;
  SwfdecAsObject *array;
  
  len = swfdec_bits_get_bu32 (bits);
  array = swfdec_as_array_new (context);
  for (i = 0; i < len; i++) {
    SwfdecAsValue tmp;
    if (!swfdec_amf_decode (context, bits, &tmp))
      goto fail;
    swfdec_as_array_push (array, &tmp);
  }

  SWFDEC_AS_VALUE_SET_OBJECT (val, array);
  return TRUE;

fail:
  return FALSE;
}

// FIXME: untested
static gboolean
swfdec_amf_decode_date (SwfdecAsContext *context, SwfdecBits *bits, SwfdecAsValue *val)
{
  SwfdecAsDate *date;
  SwfdecAsObject *object;

  object = swfdec_as_object_new (context, SWFDEC_AS_STR_Date, NULL);
  date = SWFDEC_AS_DATE (object->relay);
  date->milliseconds = swfdec_bits_get_bdouble (bits);
  date->utc_offset = swfdec_bits_get_bu16 (bits);
  if (date->utc_offset > 12 * 60)
    date->utc_offset -= 12 * 60;
  SWFDEC_AS_VALUE_SET_OBJECT (val, object);

  return TRUE;
}

static gboolean
swfdec_amf_decode_null (SwfdecAsContext *context, SwfdecBits *bits, SwfdecAsValue *val)
{
  SWFDEC_AS_VALUE_SET_NULL (val);
  return TRUE;
}

static gboolean
swfdec_amf_decode_undefined (SwfdecAsContext *context, SwfdecBits *bits, SwfdecAsValue *val)
{
  SWFDEC_AS_VALUE_SET_UNDEFINED (val);
  return TRUE;
}

typedef gboolean (* SwfdecAmfParseFunc) (SwfdecAsContext *cx, SwfdecBits *bits, SwfdecAsValue *val);

static const SwfdecAmfParseFunc parse_funcs[SWFDEC_AMF_N_TYPES] = {
  [SWFDEC_AMF_NUMBER] = swfdec_amf_decode_number,
  [SWFDEC_AMF_BOOLEAN] = swfdec_amf_decode_boolean,
  [SWFDEC_AMF_STRING] = swfdec_amf_decode_string,
  [SWFDEC_AMF_OBJECT] = swfdec_amf_decode_object,
  [SWFDEC_AMF_MIXED_ARRAY] = swfdec_amf_decode_mixed_array,
  [SWFDEC_AMF_ARRAY] = swfdec_amf_decode_array,
  [SWFDEC_AMF_DATE] = swfdec_amf_decode_date,
  [SWFDEC_AMF_NULL] = swfdec_amf_decode_null,
  [SWFDEC_AMF_UNDEFINED] = swfdec_amf_decode_undefined,
#if 0
  SWFDEC_AMF_MOVIECLIP = 4,
  SWFDEC_AMF_REFERENCE = 7,
  SWFDEC_AMF_END_OBJECT = 9,
  SWFDEC_AMF_BIG_STRING = 12,
  SWFDEC_AMF_RECORDSET = 14,
  SWFDEC_AMF_XML = 15,
  SWFDEC_AMF_CLASS = 16,
  SWFDEC_AMF_FLASH9 = 17,
#endif
};

gboolean
swfdec_amf_decode (SwfdecAsContext *context, SwfdecBits *bits, 
    SwfdecAsValue *rval)
{
  SwfdecAmfParseFunc func;
  guint type;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), FALSE);
  g_return_val_if_fail (bits != NULL, FALSE);
  g_return_val_if_fail (rval != NULL, FALSE);

  type = swfdec_bits_get_u8 (bits);
  if (type >= SWFDEC_AMF_N_TYPES ||
      (func = parse_funcs[type]) == NULL) {
    SWFDEC_ERROR ("no parse func for AMF type %u", type);
    return FALSE;
  }
  return func (context, bits, rval);
}

/*** encoding ***/

gboolean
swfdec_amf_encode (SwfdecAsContext *context,  SwfdecBots *bots,
    SwfdecAsValue val)
{
  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), FALSE);
  g_return_val_if_fail (bots != NULL, FALSE);

  switch (SWFDEC_AS_VALUE_GET_TYPE (val)) {
    case SWFDEC_AS_TYPE_UNDEFINED:
      swfdec_bots_put_u8 (bots, SWFDEC_AMF_UNDEFINED);
      break;
    case SWFDEC_AS_TYPE_NULL:
      swfdec_bots_put_u8 (bots, SWFDEC_AMF_NULL);
      break;
    case SWFDEC_AS_TYPE_BOOLEAN:
      swfdec_bots_put_u8 (bots, SWFDEC_AMF_BOOLEAN);
      swfdec_bots_put_u8 (bots, SWFDEC_AS_VALUE_GET_BOOLEAN (val) ? 1 : 0);
      break;
    case SWFDEC_AS_TYPE_NUMBER:
      swfdec_bots_put_u8 (bots, SWFDEC_AMF_NUMBER);
      swfdec_bots_put_bdouble (bots, SWFDEC_AS_VALUE_GET_NUMBER (val));
      break;
    case SWFDEC_AS_TYPE_STRING:
      {
	const char *s = SWFDEC_AS_VALUE_GET_STRING (val);
	gsize len = SWFDEC_AS_VALUE_STRLEN (val);
	if (len > G_MAXUINT32) {
	  SWFDEC_ERROR ("string is more than 2^32 bytes, clamping");
	  len = G_MAXUINT32;
	}
	if (len > G_MAXUINT16) {
	  swfdec_bots_put_u8 (bots, SWFDEC_AMF_BIG_STRING);
	  swfdec_bots_put_bu32 (bots, len);
	} else {
	  swfdec_bots_put_u8 (bots, SWFDEC_AMF_STRING);
	  swfdec_bots_put_bu16 (bots, len);
	}
	swfdec_bots_put_data (bots, (const guchar *) s, len);
      }
      break;
    case SWFDEC_AS_TYPE_OBJECT:
      {
	SwfdecAsObject *object = SWFDEC_AS_VALUE_GET_OBJECT (val);
	if (object->array) {
	  SWFDEC_ERROR ("implement encoding of arrays?");
	  return FALSE;
	} else {
	  GSList *walk, *list;
	  swfdec_bots_put_u8 (bots, SWFDEC_AMF_OBJECT);
	  list = swfdec_as_object_enumerate (object);
	  for (walk = list; walk; walk = walk->next) {
	    const char *name = walk->data;
	    gsize len = SWFDEC_AS_VALUE_STRLEN (SWFDEC_AS_VALUE_FROM_STRING (name));
	    SwfdecAsObject *o;
	    SwfdecAsValue *tmp;
	    if (len > G_MAXUINT16) {
	      SWFDEC_ERROR ("property name too long, calmping");
	      len = G_MAXUINT16;
	    }
	    swfdec_bots_put_bu16 (bots, len);
	    swfdec_bots_put_data (bots, (const guchar *) name, len);
	    o = object;
	    while ((tmp = swfdec_as_object_peek_variable (o, name)) == NULL) {
	      o = o->prototype;
	      g_assert (o);
	    }
	    swfdec_amf_encode (context, bots, *tmp);
	  }
	  g_slist_free (list);
	  swfdec_bots_put_u16 (bots, 0); /* property name */
	  swfdec_bots_put_u8 (bots, SWFDEC_AMF_END_OBJECT);
	}
      }
      break;
    case SWFDEC_AS_TYPE_MOVIE:
      /* MovieClip objects are unsupported */
      swfdec_bots_put_u8 (bots, SWFDEC_AMF_UNDEFINED);
      break;
    case SWFDEC_AS_TYPE_INT:
    default:
      g_assert_not_reached ();
      return FALSE;
  }
  return TRUE;
}

