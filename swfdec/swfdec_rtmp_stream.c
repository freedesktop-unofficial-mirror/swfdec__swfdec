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

#include "swfdec_rtmp_stream.h"
#include "swfdec_debug.h"
#include "swfdec_loader_internal.h"
#include "swfdec_player_internal.h"

static void
swfdec_rtmp_stream_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;

  if (G_UNLIKELY (!initialized)) {
    initialized = TRUE;
  }
}

GType
swfdec_rtmp_stream_get_type (void)
{
  static GType rtmp_stream_type = 0;
  
  if (!rtmp_stream_type) {
    static const GTypeInfo rtmp_stream_info = {
      sizeof (SwfdecRtmpStreamInterface),
      swfdec_rtmp_stream_base_init,
      NULL,
      NULL,
      NULL,
      NULL,
      0,
      0,
      NULL,
    };
    
    rtmp_stream_type = g_type_register_static (G_TYPE_INTERFACE,
        "SwfdecRtmpStream", &rtmp_stream_info, 0);
    g_type_interface_add_prerequisite (rtmp_stream_type, G_TYPE_OBJECT);
  }
  
  return rtmp_stream_type;
}

void
swfdec_rtmp_stream_receive (SwfdecRtmpStream *stream, const SwfdecRtmpPacket *packet)
{
  SwfdecRtmpStreamInterface *iface;
  
  g_return_if_fail (SWFDEC_IS_RTMP_STREAM (stream));
  g_return_if_fail (packet != NULL);

  iface = SWFDEC_RTMP_STREAM_GET_INTERFACE (stream);
  g_assert (iface->receive != NULL);
  return iface->receive (stream, &packet->header, packet->buffer);
}

SwfdecRtmpPacket *
swfdec_rtmp_stream_sent (SwfdecRtmpStream *stream, const SwfdecRtmpPacket *packet)
{
  SwfdecRtmpStreamInterface *iface;
  
  g_return_val_if_fail (SWFDEC_IS_RTMP_STREAM (stream), NULL);
  g_return_val_if_fail (packet != NULL, NULL);

  iface = SWFDEC_RTMP_STREAM_GET_INTERFACE (stream);
  return iface->sent (stream, packet);
}
