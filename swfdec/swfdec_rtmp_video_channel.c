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

#include "swfdec_rtmp_video_channel.h"

#include "swfdec_debug.h"
#include "swfdec_rtmp_socket.h"
#include "swfdec_video_provider.h"

/*** VIDEO PROVIDER INTERFACE ***/

static cairo_surface_t *
swfdec_rtmp_video_channel_get_image (SwfdecVideoProvider *prov,
    SwfdecRenderer *renderer, guint *width, guint *height)
{
  return NULL;
}

static void
swfdec_rtmp_video_channel_get_size (SwfdecVideoProvider *prov, guint *width, guint *height)
{
}

static void
swfdec_rtmp_video_channel_video_provider_init (SwfdecVideoProviderInterface *iface)
{
  iface->get_image = swfdec_rtmp_video_channel_get_image;
  iface->get_size = swfdec_rtmp_video_channel_get_size;
}

/*** SwfdecRtmpVideoChannel ***/

G_DEFINE_TYPE_WITH_CODE (SwfdecRtmpVideoChannel, swfdec_rtmp_video_channel, SWFDEC_TYPE_RTMP_CHANNEL,
    G_IMPLEMENT_INTERFACE (SWFDEC_TYPE_VIDEO_PROVIDER, swfdec_rtmp_video_channel_video_provider_init))

static void
swfdec_rtmp_video_channel_receive (SwfdecRtmpChannel *channel,
    const SwfdecRtmpHeader *header, SwfdecBuffer *buffer)
{
}

static SwfdecRtmpPacket *
swfdec_rtmp_video_channel_send (SwfdecRtmpChannel *channel)
{
  return NULL;
}

static void
swfdec_rtmp_video_channel_dispose (GObject *object)
{
  //SwfdecRtmpVideoChannel *conn = SWFDEC_RTMP_VIDEO_CHANNEL (object);

  G_OBJECT_CLASS (swfdec_rtmp_video_channel_parent_class)->dispose (object);
}

static void
swfdec_rtmp_video_channel_class_init (SwfdecRtmpVideoChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecRtmpChannelClass *channel_class = SWFDEC_RTMP_CHANNEL_CLASS (klass);

  object_class->dispose = swfdec_rtmp_video_channel_dispose;

  channel_class->receive = swfdec_rtmp_video_channel_receive;
  channel_class->send = swfdec_rtmp_video_channel_send;
}

static void
swfdec_rtmp_video_channel_init (SwfdecRtmpVideoChannel *command)
{
}

SwfdecRtmpChannel *
swfdec_rtmp_video_channel_new (SwfdecRtmpConnection *conn)
{
  g_return_val_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn), NULL);

  return g_object_new (SWFDEC_TYPE_RTMP_VIDEO_CHANNEL, "connection", conn, NULL);
}
