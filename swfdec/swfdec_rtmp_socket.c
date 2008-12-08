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

#include <string.h>

#include "swfdec_debug.h"
#include "swfdec_player_internal.h"
#include "swfdec_rtmp_channel.h"
#include "swfdec_rtmp_handshake.h"
#include "swfdec_rtmp_stream.h"
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

  if (G_UNLIKELY (conn->handshake))
    return swfdec_rtmp_handshake_next_buffer (conn->handshake);

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
  SwfdecRtmpPacket *packet;
  SwfdecRtmpHeader header;
  SwfdecBuffer *buffer;
  SwfdecBits bits;
  guint i, remaining, header_size;

  g_return_if_fail (SWFDEC_IS_RTMP_SOCKET (sock));
  g_return_if_fail (queue != NULL);

  conn = sock->conn;

  if (G_UNLIKELY (conn->handshake)) {
    if (swfdec_rtmp_handshake_receive (conn->handshake, queue))
      return;
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
    packet = g_hash_table_lookup (conn->incoming, GUINT_TO_POINTER (i));
    if (packet) {
      swfdec_rtmp_header_copy (&header, &packet->header);
    } else {
      swfdec_rtmp_header_invalidate (&header);
    }
    swfdec_rtmp_header_read (&header, &bits);
    swfdec_buffer_unref (buffer);

    /* read the data chunk */
    remaining = header.size;
    if (packet && packet->buffer)
      remaining -= packet->buffer->length;
    remaining = MIN (remaining, conn->read_size);
    if (header_size + remaining > swfdec_buffer_queue_get_depth (queue))
      return;

    if (packet == NULL) {
      packet = swfdec_rtmp_packet_new_empty ();
      g_hash_table_insert (conn->incoming, GUINT_TO_POINTER (i), packet);
    } else if (header_size >= 4 && packet->buffer != NULL) {
      SWFDEC_ERROR ("not a continuation header, but old command not finished yet, dropping old command");
      swfdec_buffer_unref (packet->buffer);
      packet->buffer = NULL;
    }
    if (packet->buffer == NULL) {
      packet->buffer = swfdec_buffer_new (header.size);
      /* we store the actual size of the buffer in packet->header.size, and 
       * use length to count how much data we already received */
      packet->buffer->length = 0;
    }
    swfdec_rtmp_header_copy (&packet->header, &header);

    swfdec_buffer_queue_flush (queue, header_size);
    buffer = swfdec_buffer_queue_pull (queue, remaining);
    g_assert (buffer);
    /* we allocate the buffer so it's big enough */
    memcpy (packet->buffer->data + packet->buffer->length, buffer->data, remaining);
    packet->buffer->length += remaining;
    swfdec_buffer_unref (buffer);

    if (packet->buffer->length == header.size) {
      SwfdecRtmpStream *stream = g_hash_table_lookup (conn->streams, GUINT_TO_POINTER (header.stream));

      if (stream) {
	swfdec_rtmp_stream_receive (stream, packet);
      } else {
	SWFDEC_FIXME ("packet (type %u) for unknown stream %u", header.type, header.stream);
      }
      swfdec_buffer_unref (packet->buffer);
      packet->buffer = NULL;
    }
  } while (TRUE);
}

