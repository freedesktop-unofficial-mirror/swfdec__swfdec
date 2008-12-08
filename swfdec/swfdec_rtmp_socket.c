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

#include "swfdec_rtmp_socket.h"

#include "swfdec_debug.h"
#include "swfdec_rtmp_handshake_channel.h"
#include "swfdec_player_internal.h"
/* socket implementations for swfdec_rtmp_socket_new() */
#include "swfdec_rtmp_socket_rtmp.h"

/*** SwfdecRtmpSocket ***/

G_DEFINE_TYPE (SwfdecRtmpSocket, swfdec_rtmp_socket, G_TYPE_OBJECT)

static void
swfdec_rtmp_socket_dispose (GObject *object)
{
  //SwfdecRtmpSocket *sock = SWFDEC_RTMP_SOCKET (object);

  G_OBJECT_CLASS (swfdec_rtmp_socket_parent_class)->dispose (object);
}

static void
swfdec_rtmp_socket_class_init (SwfdecRtmpSocketClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_rtmp_socket_dispose;
}

static void
swfdec_rtmp_socket_init (SwfdecRtmpSocket *sock)
{
}

static void
swfdec_rtmp_socket_open (SwfdecRtmpSocket *sock, const SwfdecURL *url)
{
  SwfdecRtmpSocketClass *klass;

  g_return_if_fail (SWFDEC_IS_RTMP_SOCKET (sock));
  g_return_if_fail (url != NULL);

  klass = SWFDEC_RTMP_SOCKET_GET_CLASS (sock);
  klass->open (sock, url);
}

SwfdecRtmpSocket *
swfdec_rtmp_socket_new (SwfdecRtmpConnection *conn, const SwfdecURL *url)
{
  SwfdecRtmpSocket *sock;
  SwfdecPlayer *player;
  const char *protocol;
  
  g_return_val_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn), NULL);
  g_return_val_if_fail (url != NULL, NULL);

  player = SWFDEC_PLAYER (swfdec_gc_object_get_context (conn));
  protocol = swfdec_url_get_protocol (url);
  if (g_str_equal (protocol, "rtmp")) {
    sock = g_object_new (SWFDEC_TYPE_RTMP_SOCKET_RTMP, NULL);
  } else {
    sock = g_object_new (SWFDEC_TYPE_RTMP_SOCKET, NULL);
  }

  sock->conn = conn;
  if (G_OBJECT_TYPE (sock) == SWFDEC_TYPE_RTMP_SOCKET) {
    swfdec_rtmp_connection_error (conn, "RTMP protocol %s is not supported", protocol);
  } else {
    swfdec_rtmp_socket_open (sock, url);
  }
  return sock;
}

void
swfdec_rtmp_socket_send (SwfdecRtmpSocket *socket)
{
  SwfdecRtmpSocketClass *klass;

  g_return_if_fail (SWFDEC_IS_RTMP_SOCKET (socket));

  klass = SWFDEC_RTMP_SOCKET_GET_CLASS (socket);
  klass->send (socket);
}

/**
 * swfdec_rtmp_socket_next_buffer:
 * @socket: the socket that wants to send a buffer
 *
 * Checks if there are any pending buffers to be sent. If so, the next buffer
 * to be processed is returned.
 *
 * Returns: The next buffer to send or %NULL
 **/
SwfdecBuffer *
swfdec_rtmp_socket_next_buffer (SwfdecRtmpSocket *socket)
{
  SwfdecRtmpConnection *conn;
  SwfdecRtmpChannel *channel;
  SwfdecBuffer *buffer;
  GList *walk;

  g_return_val_if_fail (SWFDEC_IS_RTMP_SOCKET (socket), NULL);

  conn = socket->conn;

  if (G_UNLIKELY (swfdec_rtmp_connection_get_handshake_channel (conn))) {
    return swfdec_buffer_queue_pull_buffer (swfdec_rtmp_connection_get_handshake_channel (conn)->send_queue);
  }

  walk = conn->last_send;
  g_assert (walk);
  do {
    walk = walk->next ? walk->next : conn->channels;
    channel = walk->data;
    buffer = swfdec_rtmp_channel_next_buffer (channel);
    if (buffer != NULL) {
      conn->last_send = walk;
      return buffer;
    }
  } while (walk != conn->last_send);
  return NULL;
}

void
swfdec_rtmp_socket_receive (SwfdecRtmpSocket *sock, SwfdecBufferQueue *queue)
{
  SwfdecRtmpConnection *conn;
  SwfdecRtmpChannel *channel;
  SwfdecRtmpHeader header;
  SwfdecBuffer *buffer;
  SwfdecBits bits;
  guint i, remaining, header_size;

  g_return_if_fail (SWFDEC_IS_RTMP_SOCKET (sock));
  g_return_if_fail (queue != NULL);

  conn = sock->conn;

  if (G_UNLIKELY (swfdec_rtmp_connection_get_handshake_channel (conn))) {
    SwfdecRtmpHandshakeChannel *shake = SWFDEC_RTMP_HANDSHAKE_CHANNEL (
	swfdec_rtmp_connection_get_handshake_channel (conn));
    if (shake->reply == NULL) {
      while (swfdec_rtmp_handshake_channel_receive (shake, queue));
      return;
    }
  }

  do {
    /* determine size of header */
    buffer = swfdec_buffer_queue_peek (queue, 1);
    if (buffer == NULL)
      break;
    header_size = swfdec_rtmp_header_peek_size (buffer->data[0]);
    swfdec_buffer_unref (buffer);

    /* read header */
    buffer = swfdec_buffer_queue_peek (queue, header_size);
    if (buffer == NULL)
      break;
    swfdec_bits_init (&bits, buffer);
    i = swfdec_rtmp_header_peek_channel (&bits);
    channel = swfdec_rtmp_connection_get_channel (conn, i);
    if (channel == NULL) {
      swfdec_rtmp_connection_error (conn,
	  "message on unknown channel %u, what now?", i);
      return;
    }
    if (header_size >= 4 && swfdec_buffer_queue_get_depth (channel->recv_queue)) {
      SWFDEC_ERROR ("not a continuation header, but old command not finished yet, dropping old command");
      swfdec_buffer_queue_flush (channel->recv_queue, swfdec_buffer_queue_get_depth (channel->recv_queue));
    }
    swfdec_rtmp_header_copy (&header, &channel->recv_cache);
    swfdec_rtmp_header_read (&header, &bits);
    swfdec_buffer_unref (buffer);

    /* read the data chunk */
    remaining = header.size - swfdec_buffer_queue_get_depth (channel->recv_queue);
    remaining = MIN (remaining, SWFDEC_RTMP_BLOCK_SIZE);
    if (header_size + remaining > swfdec_buffer_queue_get_depth (queue))
      return;
    swfdec_buffer_queue_flush (queue, header_size);
    buffer = swfdec_buffer_queue_pull (queue, remaining);
    g_assert (buffer);
    swfdec_buffer_queue_push (channel->recv_queue, buffer);
    swfdec_rtmp_header_copy (&channel->recv_cache, &header);

    /* process the buffer if it's received completely */
    buffer = swfdec_buffer_queue_pull (channel->recv_queue, header.size);
    if (buffer) {
      SwfdecRtmpChannelClass *klass = SWFDEC_RTMP_CHANNEL_GET_CLASS (channel);

      g_assert (swfdec_buffer_queue_get_depth (channel->recv_queue) == 0);
      klass->receive (channel, &header, buffer);
      swfdec_buffer_unref (buffer);
    }
  } while (TRUE);
}

