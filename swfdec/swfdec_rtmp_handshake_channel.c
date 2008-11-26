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

#include "swfdec_rtmp_handshake_channel.h"

#include <string.h>

#include "swfdec_as_strings.h"
#include "swfdec_debug.h"
#include "swfdec_player_internal.h"
#include "swfdec_rtmp_rpc_channel.h"
#include "swfdec_rtmp_socket.h"

/*** SwfdecRtmpHandshakeChannel ***/

G_DEFINE_TYPE (SwfdecRtmpHandshakeChannel, swfdec_rtmp_handshake_channel, SWFDEC_TYPE_RTMP_CHANNEL)

static SwfdecBuffer *
swfdec_rtmp_handshake_create (SwfdecAsContext *context)
{
  SwfdecBots *bots;
  GTimeVal tv;
  guint i, x;
  
  swfdec_as_context_get_time (context, &tv);
  /* we assume here that swfdec_as_context_get_time always returns a tv > start_time */
  x = tv.tv_sec - context->start_time.tv_sec;
  x *= 1000;
  x += (tv.tv_usec - context->start_time.tv_usec) / 1000;

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

static void
swfdec_rtmp_handshake_channel_push_connect (SwfdecRtmpHandshakeChannel *shake)
{
  SwfdecRtmpConnection *conn;
  SwfdecAsContext *cx;
  /* send connect command. Equivalent to:
   * nc.call ("connect", null, { ... }); */
  SwfdecAsObject *o;
  SwfdecAsValue val;

  conn = SWFDEC_RTMP_CHANNEL (shake)->conn;
  cx = swfdec_gc_object_get_context (conn);
  o = swfdec_as_object_new_empty (cx);

  /* app */
  SWFDEC_AS_VALUE_SET_STRING (&val, swfdec_as_context_get_string (cx, 
	swfdec_url_get_path (conn->url) ? swfdec_url_get_path (conn->url) : ""));
  swfdec_as_object_set_variable (o, SWFDEC_AS_STR_app, &val);

  /* swfUrl */
  /* FIXME: which URL do we display here actually? */
  SWFDEC_AS_VALUE_SET_STRING (&val, swfdec_as_context_get_string (cx, 
      swfdec_url_get_url (SWFDEC_PLAYER (cx)->priv->url)));
  swfdec_as_object_set_variable (o, SWFDEC_AS_STR_swfUrl, &val);

  /* tcUrl */
  SWFDEC_AS_VALUE_SET_STRING (&val, swfdec_as_context_get_string (cx, 
	swfdec_url_get_url (conn->url)));
  swfdec_as_object_set_variable (o, SWFDEC_AS_STR_tcUrl, &val);

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

  val = SWFDEC_AS_VALUE_FROM_OBJECT (o);
  swfdec_rtmp_rpc_channel_send (SWFDEC_RTMP_RPC_CHANNEL (
	swfdec_rtmp_connection_get_rpc_channel (conn)), 
      SWFDEC_AS_VALUE_FROM_STRING (SWFDEC_AS_STR_connect), o,
      1, &val);
}

void
swfdec_rtmp_handshake_channel_start (SwfdecRtmpHandshakeChannel *shake)
{
  SwfdecRtmpChannel *channel;
  SwfdecRtmpConnection *conn;
  SwfdecBuffer *buffer;

  g_return_if_fail (SWFDEC_IS_RTMP_HANDSHAKE_CHANNEL (shake));
  g_return_if_fail (shake->initial == NULL);
  g_return_if_fail (shake->reply == NULL);

  channel = SWFDEC_RTMP_CHANNEL (shake);
  conn = channel->conn;

  buffer = swfdec_rtmp_handshake_create (swfdec_gc_object_get_context (conn));
  shake->initial = swfdec_buffer_new_subbuffer (buffer, 1, 1536);
  swfdec_buffer_queue_push (channel->send_queue, buffer);
  swfdec_rtmp_socket_send (conn->socket);
  swfdec_rtmp_handshake_channel_push_connect (shake);
}

/* FIXME: This is a rather large hack where we only send the connect command 
 * but not anything else until we got a reply to the connect command */
static void
swfdec_rtmp_handshake_channel_redirect_connect (SwfdecRtmpHandshakeChannel *shake)
{
  SwfdecRtmpConnection *conn = SWFDEC_RTMP_CHANNEL (shake)->conn;
  SwfdecRtmpChannel *rpc = swfdec_rtmp_connection_get_rpc_channel (conn);
  SwfdecRtmpHeader header;
  SwfdecBuffer *buffer;
  SwfdecBits bits;

  buffer = swfdec_buffer_queue_pull_buffer (rpc->send_queue);
  swfdec_bits_init (&bits, buffer);
  swfdec_rtmp_header_read (&header, &bits);
  header.size -= (buffer->length - 12);
  swfdec_buffer_queue_push (SWFDEC_RTMP_CHANNEL (shake)->send_queue, buffer);
  while (header.size > 0) {
    buffer = swfdec_buffer_queue_pull_buffer (rpc->send_queue);
    g_assert (header.size >= buffer->length - 1);
    header.size -= buffer->length - 1;
    swfdec_buffer_queue_push (SWFDEC_RTMP_CHANNEL (shake)->send_queue, buffer);
  }
}

gboolean
swfdec_rtmp_handshake_channel_receive (SwfdecRtmpHandshakeChannel *shake,
    SwfdecBufferQueue *queue)
{
  SwfdecRtmpConnection *conn;
  SwfdecBuffer *buffer;

  g_return_val_if_fail (SWFDEC_IS_RTMP_HANDSHAKE_CHANNEL (shake), FALSE);
  g_return_val_if_fail (queue != NULL, FALSE);

  if (shake->initial == NULL || shake->reply != NULL) 
    return FALSE;

  if (swfdec_buffer_queue_get_depth (queue) < 1536 * 2 + 1)
    return FALSE;

  conn = SWFDEC_RTMP_CHANNEL (shake)->conn;

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
  shake->reply = swfdec_buffer_queue_pull (queue, 1536);
  swfdec_buffer_queue_push (SWFDEC_RTMP_CHANNEL (shake)->send_queue, swfdec_buffer_ref (shake->reply));

  /* compare last 1536 bytes to be equal to initial handshake */
  buffer = swfdec_buffer_queue_pull (queue, 1536);
  if (memcmp (buffer->data, shake->initial->data, 1536) != 0) {
    swfdec_rtmp_connection_error (conn,
	"handshake reply packet is wrong, closing connection");
    swfdec_buffer_unref (buffer);
    return FALSE;
  }
  swfdec_buffer_unref (buffer);

  /* send connect command */
  swfdec_rtmp_handshake_channel_redirect_connect (shake);

  swfdec_rtmp_socket_send (conn->socket);
  return FALSE;
}

static void
swfdec_rtmp_handshake_channel_dont_receive (SwfdecRtmpChannel *channel,
    const SwfdecRtmpHeader *header, SwfdecBuffer *buffer)
{
  g_critical ("This function should never be called");
}

static void
swfdec_rtmp_handshake_channel_dispose (GObject *object)
{
  SwfdecRtmpHandshakeChannel *shake = SWFDEC_RTMP_HANDSHAKE_CHANNEL (object);

  if (shake->initial) {
    swfdec_buffer_unref (shake->initial);
    shake->initial = NULL;
  }
  if (shake->reply) {
    swfdec_buffer_unref (shake->reply);
    shake->reply = NULL;
  }

  G_OBJECT_CLASS (swfdec_rtmp_handshake_channel_parent_class)->dispose (object);
}

static void
swfdec_rtmp_handshake_channel_class_init (SwfdecRtmpHandshakeChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecRtmpChannelClass *channel_class = SWFDEC_RTMP_CHANNEL_CLASS (klass);

  object_class->dispose = swfdec_rtmp_handshake_channel_dispose;

  channel_class->receive = swfdec_rtmp_handshake_channel_dont_receive;
}

static void
swfdec_rtmp_handshake_channel_init (SwfdecRtmpHandshakeChannel *command)
{
}

