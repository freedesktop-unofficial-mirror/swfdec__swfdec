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

#include "swfdec_rtmp_packet.h"

SwfdecRtmpPacket *
swfdec_rtmp_packet_new_empty (void) 
{
  return g_slice_new0 (SwfdecRtmpPacket);
}

SwfdecRtmpPacket *
swfdec_rtmp_packet_new (SwfdecRtmpPacketType type, guint timestamp,
    SwfdecBuffer *buffer)
{
  SwfdecRtmpPacket *packet;

  g_return_val_if_fail (buffer != NULL, NULL);

  packet = swfdec_rtmp_packet_new_empty ();
  packet->header.type = type;
  packet->header.timestamp = timestamp;
  packet->buffer = swfdec_buffer_ref (buffer);

  return packet;
}

void
swfdec_rtmp_packet_free (SwfdecRtmpPacket *packet)
{
  g_return_if_fail (packet != NULL);

  if (packet->buffer)
    swfdec_buffer_unref (packet->buffer);
  g_slice_free (SwfdecRtmpPacket, packet);
}


