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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "swfdec_rtmp_handshake.h"

#include <string.h>

#include "swfdec_as_internal.h"
#include "swfdec_as_strings.h"
#include "swfdec_debug.h"
#include "swfdec_player_internal.h"
#include "swfdec_rtmp_rpc.h"
#include "swfdec_rtmp_socket.h"
#include "swfdec_utils.h"

static SwfdecBuffer *
swfdec_rtmp_handshake_create (SwfdecAsContext *context)
{
  SwfdecBots *bots;
  GTimeVal tv;
  guint i, x;
  
  swfdec_as_context_get_time (context, &tv);
  x = swfdec_time_val_diff (&context->start_time, &tv);

  bots = swfdec_bots_new ();
  swfdec_bots_prepare_bytes (bots, 1 + 1536);
  swfdec_bots_put_u8 (bots, 3);
  swfdec_bots_put_bu32 (bots, x);
  swfdec_bots_put_bu32 (bots, 0);
  for (i = 0; i < 1528 / 2; i++) {
    x = (x * 0xB8CD75 + 1) & 0xFF;
    swfdec_bots_put_bu16 (bots, x);
  }
  g_assert (swfdec_bots_get_bytes (bots) == 1537);
  return swfdec_bots_close (bots);
}

SwfdecRtmpHandshake *
swfdec_rtmp_handshake_new (SwfdecRtmpConnection *conn)
{
  SwfdecRtmpHandshake *shake;

  g_return_val_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn), NULL);

  shake = g_slice_new0 (SwfdecRtmpHandshake);
  shake->conn = conn;
  
  shake->next_buffer = swfdec_rtmp_handshake_create (swfdec_gc_object_get_context (conn));
  shake->initial = swfdec_buffer_new_subbuffer (shake->next_buffer, 1, 1536);

  return shake;
}

void
swfdec_rtmp_handshake_free (SwfdecRtmpHandshake *shake)
{
  g_return_if_fail (shake != NULL);

  if (shake->next_buffer)
    swfdec_buffer_unref (shake->next_buffer);
  if (shake->initial)
    swfdec_buffer_unref (shake->initial);

  g_slice_free (SwfdecRtmpHandshake, shake);
}

SwfdecBuffer *
swfdec_rtmp_handshake_next_buffer (SwfdecRtmpHandshake *shake)
{
  SwfdecBuffer *buffer;

  g_return_val_if_fail (shake != NULL, NULL);

  if (shake->next_buffer == NULL)
    return NULL;

  buffer = shake->next_buffer;
  shake->next_buffer = NULL;

  return buffer;
}

static SwfdecBuffer *
swfdec_rtmp_handshake_create_connect (SwfdecRtmpHandshake *shake)
{
  SwfdecRtmpConnection *conn;
  SwfdecAsContext *cx;
  /* send connect command. Equivalent to:
   * nc.call ("connect", null, { ... }); */
  SwfdecAsObject *o;
  SwfdecAsValue val;
  const SwfdecURL *url;

  conn = shake->conn;
  cx = swfdec_gc_object_get_context (conn);
  o = swfdec_as_object_new_empty (cx);

  /* app */
  SWFDEC_AS_VALUE_SET_STRING (&val, swfdec_as_context_get_string (cx, 
	swfdec_url_get_path (conn->url) ? swfdec_url_get_path (conn->url) : ""));
  swfdec_as_object_set_variable (o, SWFDEC_AS_STR_app, &val);

  /* swfUrl */
  /* FIXME: which URL do we display here actually? */
  url = SWFDEC_PLAYER (cx)->priv->url;
  if (swfdec_url_has_protocol (url, "file")) {
    const char *s = swfdec_url_get_path (url);
    g_assert (s); /* files must have a path */
    s = strrchr (s, '/');
    g_assert (s); /* a full path even */
    SWFDEC_AS_VALUE_SET_STRING (&val, swfdec_as_context_give_string (cx, 
	  g_strconcat ("file://", s + 1, NULL)));
  } else {
    SWFDEC_AS_VALUE_SET_STRING (&val, swfdec_as_context_get_string (cx, 
	swfdec_url_get_url (SWFDEC_PLAYER (cx)->priv->url)));
  }
  swfdec_as_object_set_variable (o, SWFDEC_AS_STR_swfUrl, &val);

  /* tcUrl */
  SWFDEC_AS_VALUE_SET_STRING (&val, swfdec_as_context_get_string (cx, 
	swfdec_url_get_url (conn->url)));
  swfdec_as_object_set_variable (o, SWFDEC_AS_STR_tcUrl, &val);

  /* pageUrl */
  SWFDEC_AS_VALUE_SET_UNDEFINED (&val);
  swfdec_as_object_set_variable (o, SWFDEC_AS_STR_pageUrl, &val);

  /* flashVer */
  SWFDEC_AS_VALUE_SET_STRING (&val, swfdec_as_context_get_string (cx, 
	SWFDEC_PLAYER (cx)->priv->system->version));
  swfdec_as_object_set_variable (o, SWFDEC_AS_STR_flashVer, &val);

  /* fpad */
  val = SWFDEC_AS_VALUE_TRUE;
  swfdec_as_object_set_variable (o, SWFDEC_AS_STR_fpad, &val);

  /* FIXME: reverse engineer the values used here */
  /* audioCodecs */
  val = swfdec_as_value_from_number (cx, 615);
  swfdec_as_object_set_variable (o, SWFDEC_AS_STR_audioCodecs, &val);

  /* FIXME: reverse engineer the values used here */
  /* videoCodecs */
  val = swfdec_as_value_from_number (cx, 124);
  swfdec_as_object_set_variable (o, SWFDEC_AS_STR_videoCodecs, &val);

  /* videoFunction */
  val = swfdec_as_value_from_number (cx, 1);
  swfdec_as_object_set_variable (o, SWFDEC_AS_STR_videoFunction, &val);

  return swfdec_rtmp_rpc_encode (cx, SWFDEC_AS_VALUE_FROM_STRING (SWFDEC_AS_STR_connect),
      1, SWFDEC_AS_VALUE_FROM_OBJECT (o), 0, NULL);
}

gboolean
swfdec_rtmp_handshake_receive (SwfdecRtmpHandshake *shake,
    SwfdecBufferQueue *queue)
{
  SwfdecRtmpConnection *conn;
  SwfdecRtmpHeader header;
  SwfdecBuffer *buffer;
  SwfdecBots *bots;
  guint i;

  g_return_val_if_fail (shake != NULL, FALSE);
  g_return_val_if_fail (queue != NULL, FALSE);

  if (shake->next_buffer != NULL)
    return TRUE;

  if (shake->initial == NULL)
    return FALSE;

  if (swfdec_buffer_queue_get_depth (queue) < 1536 * 2 + 1)
    return TRUE;

  conn = shake->conn;

  /* check first byte is 0x3 */
  buffer = swfdec_buffer_queue_pull (queue, 1);
  if (buffer->data[0] != 0x3) {
    swfdec_rtmp_connection_error (conn,
	"handshake data is wrong, closing connection");
    swfdec_buffer_unref (buffer);
    return FALSE;
  }
  swfdec_buffer_unref (buffer);

  /* send back next 1536 bytes verbatim */
  bots = swfdec_bots_new ();
  buffer = swfdec_buffer_queue_pull (queue, 1536);
  swfdec_bots_put_buffer (bots, buffer);
  swfdec_buffer_unref (buffer);

  /* compare last 1536 bytes to be equal to initial handshake */
  buffer = swfdec_buffer_queue_pull (queue, 1536);
  if (memcmp (buffer->data, shake->initial->data, 1536) != 0) {
    swfdec_rtmp_connection_error (conn,
	"handshake reply packet is wrong, closing connection");
    swfdec_buffer_unref (buffer);
    swfdec_bots_free (bots);
    return FALSE;
  }
  swfdec_buffer_unref (buffer);
  swfdec_buffer_unref (shake->initial);
  shake->initial = NULL;

  /* send connect command */
  buffer = swfdec_rtmp_handshake_create_connect (shake);
  header.channel = 3;
  header.type = SWFDEC_RTMP_PACKET_INVOKE;
  header.timestamp = 0;
  header.size = buffer->length;
  header.stream = 0;
  swfdec_rtmp_header_write (&header, bots, SWFDEC_RTMP_HEADER_12_BYTES);
  for (i = 0; i < buffer->length; i += SWFDEC_RTMP_BLOCK_SIZE) {
    if (i > 0)
      swfdec_bots_put_u8 (bots, 0xC3);
    swfdec_bots_put_data (bots, buffer->data + i,
	MIN (SWFDEC_RTMP_BLOCK_SIZE, buffer->length - i));
  }

  shake->next_buffer = swfdec_bots_close (bots);
  swfdec_rtmp_socket_send (conn->socket);
  return TRUE;
}

