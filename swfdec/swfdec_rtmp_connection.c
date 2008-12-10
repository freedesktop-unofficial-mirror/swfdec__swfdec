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

#include "swfdec_rtmp_connection.h"

#include <string.h>

#include "swfdec_as_strings.h"
#include "swfdec_bots.h"
#include "swfdec_debug.h"
#include "swfdec_sandbox.h"
#include "swfdec_rtmp_handshake.h"
#include "swfdec_rtmp_rpc.h"
#include "swfdec_rtmp_socket.h"
#include "swfdec_rtmp_stream.h"
#include "swfdec_utils.h"

/*** SwfdecRtmpStream ***/

static void
swfdec_rtmp_connection_push_control (SwfdecRtmpConnection *conn,
    SwfdecRtmpPacket *packet)
{
  g_return_if_fail (packet->header.channel == 2);

  if (g_queue_is_empty (conn->control_packets)) {
    g_queue_push_tail (conn->control_packets, NULL);
    swfdec_rtmp_connection_send (conn, packet);
  } else {
    g_queue_push_tail (conn->control_packets, packet);
  }
}

static void
swfdec_rtmp_connection_handle_chunk_size (SwfdecRtmpConnection *conn, SwfdecBuffer *buffer)
{
  SwfdecBits bits;
  guint new_size;

  swfdec_bits_init (&bits, buffer);
  new_size = swfdec_bits_get_bu32 (&bits);
  SWFDEC_INFO ("altering read chunk size %u => %u", conn->read_size, new_size);
  conn->read_size = new_size;
  if (swfdec_bits_left (&bits)) {
    SWFDEC_FIXME ("%u bytes left after chunk size", swfdec_bits_left (&bits) / 8);
  }
}

enum {
  SWFDEC_RTMP_PING_SYNC = 0,
  SWFDEC_RTMP_PING_FLUSH = 1,
  SWFDEC_RTMP_PING_BUFFERTIME = 3,
  SWFDEC_RTMP_PING_CLEAR = 4,
  SWFDEC_RTMP_PING_PING = 6,
  SWFDEC_RTMP_PING_PONG = 7
};

static void
swfdec_rtmp_connection_handle_ping (SwfdecRtmpConnection *conn, SwfdecBuffer *buffer)
{
  SwfdecBits bits;
  guint type, target;
  SwfdecRtmpStream *stream;

  swfdec_bits_init (&bits, buffer);
  type = swfdec_bits_get_bu16 (&bits);
  target = swfdec_bits_get_bu32 (&bits);
  switch (type) {
    case SWFDEC_RTMP_PING_SYNC:
      stream = swfdec_rtmp_connection_get_stream (conn, target);
      if (stream) {
	swfdec_rtmp_stream_sync (stream);
      } else {
	SWFDEC_ERROR ("got sync ping for stream %u, but no such stream", target);
      }
      break;
    case SWFDEC_RTMP_PING_FLUSH:
      stream = swfdec_rtmp_connection_get_stream (conn, target);
      if (stream) {
	swfdec_rtmp_stream_flush (stream);
      } else {
	SWFDEC_ERROR ("got flush ping for stream %u, but no such stream", target);
      }
      break;
    case SWFDEC_RTMP_PING_CLEAR:
      stream = swfdec_rtmp_connection_get_stream (conn, target);
      if (stream) {
	swfdec_rtmp_stream_clear (stream);
      } else {
	SWFDEC_ERROR ("got clear ping for stream %u, but no such stream", target);
      }
      break;
    case SWFDEC_RTMP_PING_BUFFERTIME:
    case SWFDEC_RTMP_PING_PING:
    case SWFDEC_RTMP_PING_PONG:
    default:
      SWFDEC_FIXME ("handle ping type %u for target %u", type, target);
      break;
  }
}

static void
swfdec_rtmp_connection_handle_server_bandwidth (SwfdecRtmpConnection *conn,
    const SwfdecRtmpHeader *org_header, SwfdecBuffer *buffer)
{
  SwfdecBits bits;
  SwfdecBots *bots;
  guint new_bandwidth;
  SwfdecRtmpPacket *packet;
  GTimeVal tv;
  long diff;

  swfdec_bits_init (&bits, buffer);
  new_bandwidth = swfdec_bits_get_bu32 (&bits);
  SWFDEC_INFO ("new server bandwidth set: %u => %u", 
      conn->server_bandwidth, new_bandwidth);
  conn->server_bandwidth = new_bandwidth;

  /* I guess this is for telling the server to throttle if we know our bandwidth is smaller */
  bots = swfdec_bots_new ();
  swfdec_bots_put_bu32 (bots, new_bandwidth);
  buffer = swfdec_bots_close (bots);
  /* send diff between the timestamp that the server sent and our current time.
   * FIXME: Is that correct? */
  swfdec_as_context_get_time (swfdec_gc_object_get_context (conn), &tv);
  diff = swfdec_time_val_diff (&conn->connect_time, &tv);
  packet = swfdec_rtmp_packet_new (2, 0, SWFDEC_RTMP_PACKET_SERVER_BANDWIDTH,
      org_header->timestamp - diff, buffer);
  swfdec_buffer_unref (buffer);

  swfdec_rtmp_connection_push_control (conn, packet);
}

static void
swfdec_rtmp_connection_handle_client_bandwidth (SwfdecRtmpConnection *conn, SwfdecBuffer *buffer)
{
  SwfdecBits bits;
  guint magic;

  swfdec_bits_init (&bits, buffer);
  conn->client_bandwidth = swfdec_bits_get_bu32 (&bits);
  magic = swfdec_bits_get_u8 (&bits);
  SWFDEC_INFO ("client bandwidth is %u, magic value set to %u", 
      conn->client_bandwidth, magic);
}

static void
swfdec_rtmp_connection_rtmp_stream_receive (SwfdecRtmpStream *stream,
    const SwfdecRtmpHeader *header, SwfdecBuffer *buffer)
{
  SwfdecRtmpConnection *conn = SWFDEC_RTMP_CONNECTION (stream);

  switch ((guint) header->type) {
    case SWFDEC_RTMP_PACKET_SIZE:
      swfdec_rtmp_connection_handle_chunk_size (conn, buffer);
      break;
    case SWFDEC_RTMP_PACKET_PING:
      swfdec_rtmp_connection_handle_ping (conn, buffer);
      break;
    case SWFDEC_RTMP_PACKET_SERVER_BANDWIDTH:
      swfdec_rtmp_connection_handle_server_bandwidth (conn, header, buffer);
      break;
    case SWFDEC_RTMP_PACKET_CLIENT_BANDWIDTH:
      swfdec_rtmp_connection_handle_client_bandwidth (conn, buffer);
      break;
    case SWFDEC_RTMP_PACKET_NOTIFY:
      swfdec_sandbox_use (conn->sandbox);
      swfdec_rtmp_rpc_notify (conn->rpc, buffer);
      swfdec_sandbox_unuse (conn->sandbox);
      break;
    case SWFDEC_RTMP_PACKET_INVOKE:
      swfdec_sandbox_use (conn->sandbox);
      if (swfdec_rtmp_rpc_receive (conn->rpc, buffer)) {
	SwfdecRtmpPacket *packet = swfdec_rtmp_rpc_pop (conn->rpc, FALSE);
	if (packet) {
	  packet->header.channel = 3;
	  packet->header.stream = 0;
	  swfdec_rtmp_connection_send (conn, packet);
	}
      }
      swfdec_sandbox_unuse (conn->sandbox);
      break;
    default:
      SWFDEC_FIXME ("what to do with header type %u (channel %u)?", header->type,
	  header->channel);
      break;
  }
}

static SwfdecRtmpPacket *
swfdec_rtmp_connection_rtmp_stream_sent (SwfdecRtmpStream *stream,
    const SwfdecRtmpPacket *packet)
{
  SwfdecRtmpConnection *conn = SWFDEC_RTMP_CONNECTION (stream);
  SwfdecRtmpPacket *result = NULL;

  switch (packet->header.channel) {
    case 2:
      {
	GList *list;
	if (g_queue_pop_head (conn->control_packets) != NULL) {
	  g_assert_not_reached ();
	}
	list = g_queue_peek_head_link (conn->control_packets);
	if (list) {
	  result = list->data;
	  list->data = NULL;
	}
      }
      break;
    case 3:
      result = swfdec_rtmp_rpc_pop (conn->rpc, TRUE);
      if (result) {
	result->header.channel = 3;
	result->header.stream = 0;
      }
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return result;
}

static void
swfdec_rtmp_connection_rtmp_stream_sync (SwfdecRtmpStream *stream)
{
  SWFDEC_FIXME ("implement");
}

static void
swfdec_rtmp_connection_rtmp_stream_flush (SwfdecRtmpStream *stream)
{
  SWFDEC_FIXME ("implement");
}

static void
swfdec_rtmp_connection_rtmp_stream_clear (SwfdecRtmpStream *stream)
{
  SWFDEC_FIXME ("implement");
}

static void
swfdec_rtmp_connection_rtmp_stream_init (SwfdecRtmpStreamInterface *iface)
{
  iface->receive = swfdec_rtmp_connection_rtmp_stream_receive;
  iface->sent = swfdec_rtmp_connection_rtmp_stream_sent;
  iface->sync = swfdec_rtmp_connection_rtmp_stream_sync;
  iface->flush = swfdec_rtmp_connection_rtmp_stream_flush;
  iface->clear = swfdec_rtmp_connection_rtmp_stream_clear;
}

/*** SwfdecRtmpConnection ***/

G_DEFINE_TYPE_WITH_CODE (SwfdecRtmpConnection, swfdec_rtmp_connection, SWFDEC_TYPE_AS_RELAY,
    G_IMPLEMENT_INTERFACE (SWFDEC_TYPE_RTMP_STREAM, swfdec_rtmp_connection_rtmp_stream_init))

static void
swfdec_rtmp_connection_mark (SwfdecGcObject *object)
{
  SwfdecRtmpConnection *conn = SWFDEC_RTMP_CONNECTION (object);

  swfdec_rtmp_rpc_mark (conn->rpc);

  SWFDEC_GC_OBJECT_CLASS (swfdec_rtmp_connection_parent_class)->mark (object);
}

/* This function is necessary because we use packet->buffer->length as our write counter */
static void
swfdec_rtmp_connection_packets_free (gpointer packetp)
{
  SwfdecRtmpPacket *packet = packetp;

  packet->buffer->length = packet->header.size;
  swfdec_rtmp_packet_free (packet);
}

static void
swfdec_rtmp_connection_dispose (GObject *object)
{
  SwfdecRtmpConnection *conn = SWFDEC_RTMP_CONNECTION (object);

  swfdec_rtmp_connection_close (conn);
  g_assert (conn->socket == NULL);

  g_free (conn->error);
  conn->error = NULL;

  if (conn->incoming) {
    g_hash_table_destroy (conn->incoming);
    conn->incoming = NULL;
  }
  if (conn->streams) {
    g_hash_table_destroy (conn->streams);
    conn->streams = NULL;
  }
  if (conn->packets) {
    g_queue_foreach (conn->packets, (GFunc) swfdec_rtmp_connection_packets_free, NULL);
    g_queue_free (conn->packets);
    conn->packets = NULL;
  }
  if (conn->control_packets) {
    g_queue_foreach (conn->control_packets, (GFunc) swfdec_rtmp_packet_free, NULL);
    g_queue_free (conn->control_packets);
    conn->control_packets = NULL;
  }
  if (conn->rpc) {
    swfdec_rtmp_rpc_free (conn->rpc);
    conn->rpc = NULL;
  }

  G_OBJECT_CLASS (swfdec_rtmp_connection_parent_class)->dispose (object);
}

static void
swfdec_rtmp_connection_class_init (SwfdecRtmpConnectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_rtmp_connection_dispose;

  gc_class->mark = swfdec_rtmp_connection_mark;
}

static void
swfdec_rtmp_connection_init (SwfdecRtmpConnection *conn)
{
  conn->incoming = g_hash_table_new_full (g_direct_hash, g_direct_equal, 
      NULL, (GDestroyNotify) swfdec_rtmp_packet_free);
  conn->streams = g_hash_table_new (g_direct_hash, g_direct_equal);
  conn->packets = g_queue_new ();

  conn->control_packets = g_queue_new ();
  conn->rpc = swfdec_rtmp_rpc_new (conn, SWFDEC_AS_RELAY (conn));

  conn->read_size = SWFDEC_RTMP_BLOCK_SIZE;
  conn->write_size = SWFDEC_RTMP_BLOCK_SIZE;

  swfdec_rtmp_connection_register_stream (conn, 0, SWFDEC_RTMP_STREAM (conn));
}

void
swfdec_rtmp_connection_connect (SwfdecRtmpConnection *conn, const SwfdecURL *url)
{
  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));

  swfdec_rtmp_connection_close (conn);

  if (url) {
    conn->socket = swfdec_rtmp_socket_new (conn, url);
    conn->url = swfdec_url_copy (url);
  } else {
    swfdec_rtmp_connection_error (conn, "handle NULL urls in connect()");
  }

  if (conn->error)
    return;

  swfdec_as_context_get_time (swfdec_gc_object_get_context (conn), &conn->connect_time);
  conn->rpc->last_send = conn->connect_time;
  conn->handshake = swfdec_rtmp_handshake_new (conn);
  swfdec_rtmp_socket_send (conn->socket);
}

void
swfdec_rtmp_connection_close (SwfdecRtmpConnection *conn)
{
  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));

  if (!swfdec_rtmp_connection_is_connected (conn))
    return;

  g_object_unref (conn->socket);
  conn->socket = NULL;
  swfdec_url_free (conn->url);
  conn->url = NULL;
}

void
swfdec_rtmp_connection_error (SwfdecRtmpConnection *conn, const char *error, ...)
{
  va_list args;

  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));
  g_return_if_fail (error != NULL);

  va_start (args, error);
  swfdec_rtmp_connection_errorv (conn, error, args);
  va_end (args);
}

void
swfdec_rtmp_connection_errorv (SwfdecRtmpConnection *conn, const char *error, va_list args)
{
  char *real_error;

  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));
  g_return_if_fail (error != NULL);

  real_error = g_strdup_vprintf (error, args);
  if (conn->error) {
    SWFDEC_ERROR ("another error in rtmp socket: %s", real_error);
    g_free (real_error);
    return;
  }

  SWFDEC_ERROR ("error in rtmp socket: %s", real_error);
  conn->error = real_error;
  swfdec_rtmp_connection_close (conn);
}

void
swfdec_rtmp_connection_on_status (SwfdecRtmpConnection *conn, SwfdecAsValue value)
{
  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));

  swfdec_as_relay_call (SWFDEC_AS_RELAY (conn), SWFDEC_AS_STR_onStatus, 1, &value, NULL);
}

void
swfdec_rtmp_connection_register_stream (SwfdecRtmpConnection *conn,
    guint id, SwfdecRtmpStream *stream)
{
  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));
  g_return_if_fail (SWFDEC_IS_RTMP_STREAM (stream));

  if (g_hash_table_lookup (conn->streams, GUINT_TO_POINTER (id))) {
    SWFDEC_FIXME ("stream %u is already registered, ignoring new request",
	id);
    return;
  }

  g_hash_table_insert (conn->streams, GUINT_TO_POINTER (id), stream);
}

void
swfdec_rtmp_connection_unregister_stream (SwfdecRtmpConnection *conn, guint id)
{
  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));

  if (!g_hash_table_remove (conn->streams, GUINT_TO_POINTER (id))) {
    g_assert_not_reached ();
  }
}

SwfdecRtmpStream *
swfdec_rtmp_connection_get_stream (SwfdecRtmpConnection *conn, guint id)
{
  g_return_val_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn), NULL);

  return g_hash_table_lookup (conn->streams, GUINT_TO_POINTER (id));
}

void
swfdec_rtmp_connection_send (SwfdecRtmpConnection *conn, SwfdecRtmpPacket *packet)
{
  gboolean send;

  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));
  g_return_if_fail (packet != NULL);
  /* FIXME: I'd like a g_return_if_fail (packet->channel is not already sent),
   * but that requires a g_queue_find_custom () and that's slow */

  g_assert (packet->header.size == packet->buffer->length);
  packet->buffer->length = 0;
  send = g_queue_is_empty (conn->packets);
  g_queue_push_tail (conn->packets, packet);
  SWFDEC_LOG ("pushed channel %u - %u packets now", packet->header.channel, 
      g_queue_get_length (conn->packets));
  if (send)
    swfdec_rtmp_socket_send (conn->socket);
}
