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

#include "swfdec_rtmp_control_channel.h"

#include "swfdec_debug.h"
#include "swfdec_rtmp_socket.h"
#include "swfdec_utils.h"

/*** SwfdecRtmpControlChannel ***/

G_DEFINE_TYPE (SwfdecRtmpControlChannel, swfdec_rtmp_control_channel, SWFDEC_TYPE_RTMP_CHANNEL)

static void
swfdec_rtmp_control_channel_push (SwfdecRtmpControlChannel *control, 
    SwfdecRtmpPacket *packet)
{
  gboolean empty = g_queue_is_empty (control->send_packets);

  g_queue_push_tail (control->send_packets, packet);

  if (empty)
    swfdec_rtmp_channel_send (SWFDEC_RTMP_CHANNEL (control));
}

static void
swfdec_rtmp_control_channel_handle_chunk_size (SwfdecRtmpChannel *channel, SwfdecBuffer *buffer)
{
  SwfdecBits bits;

  swfdec_bits_init (&bits, buffer);
  channel->conn->read_size = swfdec_bits_get_bu32 (&bits);
  SWFDEC_INFO ("setting read chunk size to %u", channel->conn->read_size);
  if (swfdec_bits_left (&bits)) {
    SWFDEC_FIXME ("%u bytes left after chunk size", swfdec_bits_left (&bits) / 8);
  }
}

static void
swfdec_rtmp_control_channel_handle_ping (SwfdecRtmpChannel *channel, SwfdecBuffer *buffer)
{
  SwfdecBits bits;
  guint type, target;

  swfdec_bits_init (&bits, buffer);
  type = swfdec_bits_get_bu16 (&bits);
  target = swfdec_bits_get_bu32 (&bits);
  SWFDEC_FIXME ("handle ping type %u for target %u", type, target);
}

static void
swfdec_rtmp_control_channel_handle_server_bandwidth (SwfdecRtmpChannel *channel,
    const SwfdecRtmpHeader *org_header, SwfdecBuffer *buffer)
{
  SwfdecRtmpControlChannel *control = SWFDEC_RTMP_CONTROL_CHANNEL (channel);
  SwfdecBits bits;
  SwfdecBots *bots;
  guint new_bandwidth;
  SwfdecRtmpPacket *packet;
  GTimeVal tv;
  long diff;

  swfdec_bits_init (&bits, buffer);
  new_bandwidth = swfdec_bits_get_bu32 (&bits);
  SWFDEC_INFO ("new server bandwidth set: %u => %u", 
      control->server_bandwidth, new_bandwidth);
  control->server_bandwidth = new_bandwidth;

  /* I guess this is for telling the server to throttle if we know our bandwidth is smaller */
  bots = swfdec_bots_new ();
  swfdec_bots_put_bu32 (bots, new_bandwidth);
  buffer = swfdec_bots_close (bots);
  /* send diff between the timestamp that the server sent and our current time.
   * FIXME: Is that correct? */
  swfdec_rtmp_channel_get_time (channel, &tv);
  diff = swfdec_time_val_diff (&channel->timestamp, &tv);
  packet = swfdec_rtmp_packet_new (SWFDEC_RTMP_PACKET_SERVER_BANDWIDTH,
      org_header->timestamp - diff, buffer);
  swfdec_buffer_unref (buffer);

  swfdec_rtmp_control_channel_push (control, packet);
}

static void
swfdec_rtmp_control_channel_handle_client_bandwidth (SwfdecRtmpChannel *channel, SwfdecBuffer *buffer)
{
  SwfdecRtmpControlChannel *control = SWFDEC_RTMP_CONTROL_CHANNEL (channel);
  SwfdecBits bits;
  guint magic;

  swfdec_bits_init (&bits, buffer);
  control->client_bandwidth = swfdec_bits_get_bu32 (&bits);
  magic = swfdec_bits_get_u8 (&bits);
  SWFDEC_INFO ("client bandwidth is %u, magic value set to %u", 
      control->client_bandwidth, magic);
}

static void
swfdec_rtmp_control_channel_receive (SwfdecRtmpChannel *channel,
    const SwfdecRtmpHeader *header, SwfdecBuffer *buffer)
{
  switch ((guint) header->type) {
    case SWFDEC_RTMP_PACKET_SIZE:
      swfdec_rtmp_control_channel_handle_chunk_size (channel, buffer);
      break;
    case SWFDEC_RTMP_PACKET_PING:
      swfdec_rtmp_control_channel_handle_ping (channel, buffer);
      break;
    case SWFDEC_RTMP_PACKET_SERVER_BANDWIDTH:
      swfdec_rtmp_control_channel_handle_server_bandwidth (channel, header, buffer);
      break;
    case SWFDEC_RTMP_PACKET_CLIENT_BANDWIDTH:
      swfdec_rtmp_control_channel_handle_client_bandwidth (channel, buffer);
      break;
    default:
      SWFDEC_FIXME ("what to do with header type %u?", header->type);
      break;
  }
}

static SwfdecRtmpPacket *
swfdec_rtmp_control_channel_send (SwfdecRtmpChannel *channel)
{
  SwfdecRtmpControlChannel *control = SWFDEC_RTMP_CONTROL_CHANNEL (channel);

  return g_queue_pop_head (control->send_packets);
}

static void
swfdec_rtmp_control_channel_dispose (GObject *object)
{
  SwfdecRtmpControlChannel *control = SWFDEC_RTMP_CONTROL_CHANNEL (object);

  if (control->send_packets) {
    g_queue_foreach (control->send_packets, (GFunc) swfdec_rtmp_packet_free, NULL);
    g_queue_free (control->send_packets);
    control->send_packets = NULL;
  }

  G_OBJECT_CLASS (swfdec_rtmp_control_channel_parent_class)->dispose (object);
}

static void
swfdec_rtmp_control_channel_class_init (SwfdecRtmpControlChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecRtmpChannelClass *channel_class = SWFDEC_RTMP_CHANNEL_CLASS (klass);

  object_class->dispose = swfdec_rtmp_control_channel_dispose;

  channel_class->receive = swfdec_rtmp_control_channel_receive;
  channel_class->send = swfdec_rtmp_control_channel_send;
}

static void
swfdec_rtmp_control_channel_init (SwfdecRtmpControlChannel *control)
{
  control->send_packets = g_queue_new ();
}

SwfdecRtmpChannel *
swfdec_rtmp_control_channel_new (SwfdecRtmpConnection *conn)
{
  g_return_val_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn), NULL);

  return g_object_new (SWFDEC_TYPE_RTMP_CONTROL_CHANNEL, "connection", conn, NULL);
}
