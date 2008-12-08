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
#include "swfdec_rtmp_control_channel.h"
#include "swfdec_rtmp_handshake_channel.h"
#include "swfdec_rtmp_rpc_channel.h"
#include "swfdec_rtmp_socket.h"
#include "swfdec_rtmp_stream.h"

/*** SwfdecRtmpStream ***/

static void
swfdec_rtmp_connection_rtmp_stream_receive (SwfdecRtmpStream *stream,
    const SwfdecRtmpHeader *header, SwfdecBuffer *buffer)
{
  SwfdecRtmpConnection *conn = SWFDEC_RTMP_CONNECTION (stream);
  SwfdecRtmpChannel *channel = swfdec_rtmp_connection_get_channel (conn, header->channel);
  SwfdecRtmpChannelClass *klass;
  
  if (channel == NULL) {
    SWFDEC_FIXME ("woot, no channel %u", header->channel);
    return;
  }
  klass = SWFDEC_RTMP_CHANNEL_GET_CLASS (channel);
  klass->receive (channel, header, buffer);
}

static void
swfdec_rtmp_connection_rtmp_stream_init (SwfdecRtmpStreamInterface *iface)
{
  iface->receive = swfdec_rtmp_connection_rtmp_stream_receive;
}

/*** SwfdecRtmpConnection ***/

G_DEFINE_TYPE_WITH_CODE (SwfdecRtmpConnection, swfdec_rtmp_connection, SWFDEC_TYPE_AS_RELAY,
    G_IMPLEMENT_INTERFACE (SWFDEC_TYPE_RTMP_STREAM, swfdec_rtmp_connection_rtmp_stream_init))

static void
swfdec_rtmp_connection_mark (SwfdecGcObject *object)
{
  SwfdecRtmpConnection *conn = SWFDEC_RTMP_CONNECTION (object);
  GList *walk;

  for (walk = conn->channels; walk; walk = walk->next) {
    SwfdecRtmpChannel *channel = walk->data;
    SwfdecRtmpChannelClass *klass = SWFDEC_RTMP_CHANNEL_GET_CLASS (channel);
    if (klass->mark)
      klass->mark (channel);
  }

  SWFDEC_GC_OBJECT_CLASS (swfdec_rtmp_connection_parent_class)->mark (object);
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

  conn->read_size = SWFDEC_RTMP_BLOCK_SIZE;
  conn->write_size = SWFDEC_RTMP_BLOCK_SIZE;

  swfdec_rtmp_register_stream (conn, 0, SWFDEC_RTMP_STREAM (conn));
}

void
swfdec_rtmp_connection_connect (SwfdecRtmpConnection *conn, const SwfdecURL *url)
{
  SwfdecRtmpChannel *channel;

  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));

  swfdec_rtmp_connection_close (conn);

  if (url) {
    conn->socket = swfdec_rtmp_socket_new (conn, url);
    conn->url = swfdec_url_copy (url);
  } else {
    SWFDEC_FIXME ("handle NULL urls in connect()");
  }

  if (conn->error)
    return;

  conn->handshake = swfdec_rtmp_handshake_channel_new (conn);
  channel = swfdec_rtmp_control_channel_new (conn);
  swfdec_rtmp_channel_register (channel, 2, 0);
  g_object_unref (channel);
  channel = swfdec_rtmp_rpc_channel_new (conn);
  swfdec_rtmp_channel_register (channel, 3, 0);
  g_object_unref (channel);
  conn->last_send = conn->channels;

  swfdec_rtmp_handshake_channel_start (SWFDEC_RTMP_HANDSHAKE_CHANNEL (
	swfdec_rtmp_connection_get_handshake_channel (conn)));
}

void
swfdec_rtmp_connection_close (SwfdecRtmpConnection *conn)
{
  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));

  while (conn->channels)
    swfdec_rtmp_channel_unregister (conn->channels->data);

  if (conn->socket) {
    g_object_unref (conn->socket);
    conn->socket = NULL;
  }
  if (conn->url) {
    swfdec_url_free (conn->url);
    conn->url = NULL;
  }
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

SwfdecRtmpChannel *
swfdec_rtmp_connection_get_channel (SwfdecRtmpConnection *conn, guint id)
{
  SwfdecRtmpChannel *channel;
  GList *walk;

  g_return_val_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn), NULL);
  g_return_val_if_fail (conn->channels != NULL, NULL);

  for (walk = conn->channels; walk; walk = walk->next) {
    channel = walk->data;
    if (channel->channel_id < id)
      continue;
    if (channel->channel_id == id)
      return channel;
    return NULL;
  }

  return NULL;
}

void
swfdec_rtmp_register_stream (SwfdecRtmpConnection *conn,
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
swfdec_rtmp_unregister_stream (SwfdecRtmpConnection *conn, guint id)
{
  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));

  if (!g_hash_table_remove (conn->streams, GUINT_TO_POINTER (id))) {
    g_assert_not_reached ();
  }
}
