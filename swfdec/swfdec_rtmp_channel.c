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

#include "swfdec_rtmp_channel.h"

#include "swfdec_debug.h"
#include "swfdec_rtmp_socket.h"

/*** SwfdecRtmpChannel ***/

G_DEFINE_ABSTRACT_TYPE (SwfdecRtmpChannel, swfdec_rtmp_channel, G_TYPE_OBJECT)

static void
swfdec_rtmp_channel_dispose (GObject *object)
{
  SwfdecRtmpChannel *channel = SWFDEC_RTMP_CHANNEL (object);

  if (channel->recv_queue) {
    swfdec_buffer_queue_unref (channel->recv_queue);
    channel->recv_queue = NULL;
  }
  if (channel->send_queue) {
    swfdec_buffer_queue_unref (channel->send_queue);
    channel->send_queue = NULL;
  }

  G_OBJECT_CLASS (swfdec_rtmp_channel_parent_class)->dispose (object);
}

static void
swfdec_rtmp_channel_class_init (SwfdecRtmpChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_rtmp_channel_dispose;
}

static void
swfdec_rtmp_channel_init (SwfdecRtmpChannel *channel)
{
  swfdec_rtmp_header_invalidate (&channel->recv_cache);
  channel->recv_queue = swfdec_buffer_queue_new ();
  swfdec_rtmp_header_invalidate (&channel->send_cache);
  channel->send_queue = swfdec_buffer_queue_new ();
  channel->block_size = 128;
}

void
swfdec_rtmp_channel_send (SwfdecRtmpChannel *channel,
    const SwfdecRtmpHeader *header, SwfdecBuffer *data)
{
  SwfdecBots *bots;
  gsize i;

  g_return_if_fail (SWFDEC_IS_RTMP_CHANNEL (channel));
  g_return_if_fail (header != NULL);
  g_return_if_fail (data != NULL);

  bots = swfdec_bots_new ();
  swfdec_rtmp_header_write (header, bots,
      swfdec_rtmp_header_diff (header, &channel->send_cache));
  swfdec_rtmp_header_copy (&channel->send_cache, header);

  for (i = 0; i < data->length; i += channel->block_size) {
    if (i != 0) {
      /* write a continuation header */
      bots = swfdec_bots_new ();
      swfdec_rtmp_header_write (header, bots, SWFDEC_RTMP_HEADER_1_BYTE);
    }
    swfdec_bots_put_data (bots, data->data + i, MIN (channel->block_size, data->length - i));
    swfdec_buffer_queue_push (channel->send_queue, swfdec_bots_close (bots));
  }
  
  swfdec_rtmp_socket_send (channel->conn->socket);
}

gboolean
swfdec_rtmp_channel_receive (SwfdecRtmpChannel *channel, SwfdecBufferQueue *queue,
    SwfdecRtmpHeaderSize header_size)
{
  SwfdecRtmpHeader header;
  SwfdecBuffer *buffer;
  SwfdecBits bits;
  gsize size, remaining_size;

  size = swfdec_rtmp_header_size_get (header_size);
  if (size > 4 && swfdec_buffer_queue_get_depth (channel->recv_queue)) {
    SWFDEC_ERROR ("received new command, but old command not processed yet, dropping old command");
    swfdec_buffer_queue_flush (channel->recv_queue, swfdec_buffer_queue_get_depth (channel->recv_queue));
  }

  buffer = swfdec_buffer_queue_peek (queue, size);
  if (buffer == NULL)
    return FALSE;

  swfdec_bits_init (&bits, buffer);
  swfdec_rtmp_header_copy (&header, &channel->recv_cache);
  swfdec_rtmp_header_read (&header, &bits);
  swfdec_buffer_unref (buffer);
  remaining_size = header.size - swfdec_buffer_queue_get_depth (channel->recv_queue);
  remaining_size = MIN (remaining_size, channel->block_size);
  if (swfdec_buffer_queue_get_depth (queue) < size + remaining_size)
    return FALSE;

  swfdec_rtmp_header_copy (&channel->recv_cache, &header);
  swfdec_buffer_queue_flush (queue, size);
  buffer = swfdec_buffer_queue_pull (queue, remaining_size);
  swfdec_buffer_queue_push (channel->recv_queue, buffer);

  size = swfdec_buffer_queue_get_depth (channel->recv_queue);
  g_assert (header.size >= size);
  if (header.size == size) {
    SwfdecRtmpChannelClass *klass;
    buffer = swfdec_buffer_queue_pull (channel->recv_queue, size);
    klass = SWFDEC_RTMP_CHANNEL_GET_CLASS (channel);
    klass->receive (channel, &header, buffer);
  }
  return TRUE;
}

