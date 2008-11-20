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

/*** SwfdecRtmpControlChannel ***/

G_DEFINE_TYPE (SwfdecRtmpControlChannel, swfdec_rtmp_control_channel, SWFDEC_TYPE_RTMP_CHANNEL)

static void
swfdec_rtmp_control_channel_receive (SwfdecRtmpChannel *channel,
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
swfdec_rtmp_control_channel_dispose (GObject *object)
{
  //SwfdecRtmpControlChannel *conn = SWFDEC_RTMP_CONTROL_CHANNEL (object);

  G_OBJECT_CLASS (swfdec_rtmp_control_channel_parent_class)->dispose (object);
}

static void
swfdec_rtmp_control_channel_class_init (SwfdecRtmpControlChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecRtmpChannelClass *channel_class = SWFDEC_RTMP_CHANNEL_CLASS (klass);

  object_class->dispose = swfdec_rtmp_control_channel_dispose;

  channel_class->receive = swfdec_rtmp_control_channel_receive;
}

static void
swfdec_rtmp_control_channel_init (SwfdecRtmpControlChannel *command)
{
}

