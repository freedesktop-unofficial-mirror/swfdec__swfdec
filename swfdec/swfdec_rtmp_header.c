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

#include "swfdec_rtmp_header.h"

gsize
swfdec_rtmp_header_size_get (SwfdecRtmpHeaderSize size)
{
  static const gsize sizes[] = { 12, 8, 4, 1 };

  return sizes[size];
}

void
swfdec_rtmp_header_invalidate (SwfdecRtmpHeader *header)
{
  g_return_if_fail (header != NULL);

  header->channel = (guint) -1;
}

void
swfdec_rtmp_header_read (SwfdecRtmpHeader *header, SwfdecBits *bits)
{
  SwfdecRtmpHeaderSize size;

  g_return_if_fail (header != NULL);
  g_return_if_fail (bits != NULL);

  size = swfdec_bits_getbits (bits, 2);
  header->channel = swfdec_bits_getbits (bits, 6);
  if (size == SWFDEC_RTMP_HEADER_1_BYTE)
    return;
  header->timestamp = swfdec_bits_get_bu24 (bits);
  if (size == SWFDEC_RTMP_HEADER_4_BYTES)
    return;
  header->size = swfdec_bits_get_bu24 (bits);
  header->type = swfdec_bits_get_u8 (bits);
  if (size == SWFDEC_RTMP_HEADER_8_BYTES)
    return;
  header->stream = swfdec_bits_get_u32 (bits);
}

void
swfdec_rtmp_header_write (const SwfdecRtmpHeader *header, SwfdecBots *bots,
    SwfdecRtmpHeaderSize size)
{
  g_return_if_fail (header != NULL);
  g_return_if_fail (bots != NULL);

  swfdec_bots_put_bits (bots, size, 2);
  swfdec_bots_put_bits (bots, header->channel, 6);
  if (size == SWFDEC_RTMP_HEADER_1_BYTE)
    return;
  swfdec_bots_put_bu24 (bots, header->timestamp);
  if (size == SWFDEC_RTMP_HEADER_4_BYTES)
    return;
  swfdec_bots_put_bu24 (bots, header->size);
  swfdec_bots_put_u8 (bots, header->type);
  if (size == SWFDEC_RTMP_HEADER_8_BYTES)
    return;
  swfdec_bots_put_u32 (bots, header->stream);
}


SwfdecRtmpHeaderSize
swfdec_rtmp_header_diff (const SwfdecRtmpHeader *a, const SwfdecRtmpHeader *b)
{
  g_return_val_if_fail (a != NULL, SWFDEC_RTMP_HEADER_12_BYTES);
  g_return_val_if_fail (b != NULL, SWFDEC_RTMP_HEADER_12_BYTES);

  if (a->channel != b->channel || a->stream != b->stream)
    return SWFDEC_RTMP_HEADER_12_BYTES;

  if (a->size != b->size || a->type != b->type)
    return SWFDEC_RTMP_HEADER_8_BYTES;

  if (a->timestamp != b->timestamp)
    return SWFDEC_RTMP_HEADER_4_BYTES;

  return SWFDEC_RTMP_HEADER_1_BYTE;
}

