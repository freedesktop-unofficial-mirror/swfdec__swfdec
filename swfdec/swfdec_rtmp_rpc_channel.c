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

#include "swfdec_rtmp_rpc_channel.h"

#include "swfdec_amf.h"
#include "swfdec_debug.h"

/*** SwfdecRtmpRpcChannel ***/

G_DEFINE_TYPE (SwfdecRtmpRpcChannel, swfdec_rtmp_rpc_channel, SWFDEC_TYPE_RTMP_CHANNEL)

static void
swfdec_rtmp_rpc_channel_receive (SwfdecRtmpChannel *channel,
    const SwfdecRtmpHeader *header, SwfdecBuffer *buffer)
{
  switch ((guint) header->type) {
    default:
      SWFDEC_FIXME ("what to do with header type %u?", header->type);
      break;
  }
  swfdec_buffer_unref (buffer);
}

static void
swfdec_rtmp_rpc_channel_dispose (GObject *object)
{
  //SwfdecRtmpRpcChannel *conn = SWFDEC_RTMP_RPC_CHANNEL (object);

  G_OBJECT_CLASS (swfdec_rtmp_rpc_channel_parent_class)->dispose (object);
}

static void
swfdec_rtmp_rpc_channel_class_init (SwfdecRtmpRpcChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecRtmpChannelClass *channel_class = SWFDEC_RTMP_CHANNEL_CLASS (klass);

  object_class->dispose = swfdec_rtmp_rpc_channel_dispose;

  channel_class->receive = swfdec_rtmp_rpc_channel_receive;
}

static void
swfdec_rtmp_rpc_channel_init (SwfdecRtmpRpcChannel *rpc)
{
}

void
swfdec_rtmp_rpc_channel_send (SwfdecRtmpRpcChannel *rpc,
    SwfdecAsValue name, SwfdecAsObject *return_object, 
    guint argc, const SwfdecAsValue *argv)
{
  SwfdecRtmpChannel *channel;
  SwfdecRtmpHeader header;
  SwfdecAsContext *cx;
  SwfdecBots *bots;
  SwfdecBuffer *buffer;
  guint i;

  g_return_if_fail (SWFDEC_IS_RTMP_RPC_CHANNEL (rpc));
  g_return_if_fail (argc == 0 || argv != NULL);

  channel = SWFDEC_RTMP_CHANNEL (rpc);
  cx = swfdec_gc_object_get_context (channel->conn);

  /* prepare buffer to encode */
  bots = swfdec_bots_new ();
  swfdec_amf_encode (cx, bots, name);
  if (return_object) {
    swfdec_amf_encode (cx, bots, swfdec_as_value_from_number (cx, ++rpc->id));
  } else {
    swfdec_amf_encode (cx, bots, swfdec_as_value_from_number (cx, 0));
  }
  if (argc == 0) {
    swfdec_amf_encode (cx, bots, SWFDEC_AS_VALUE_NULL);
  } else {
    for (i = 0; i < argc; i++) {
      swfdec_amf_encode (cx, bots, argv[i]);
    }
  }
  buffer = swfdec_bots_close (bots);

  /* prepare header */
  header.channel = channel->id;
  header.timestamp = 0;
  header.size = buffer->length;
  header.type = SWFDEC_RTMP_PACKET_INVOKE;
  header.stream = 0;

  swfdec_rtmp_channel_send (channel, &header, buffer);
}

