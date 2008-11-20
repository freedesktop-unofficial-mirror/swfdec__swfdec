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

#ifndef _SWFDEC_RTMP_HEADER_H_
#define _SWFDEC_RTMP_HEADER_H_

#include <swfdec/swfdec.h>
#include <swfdec/swfdec_bits.h>
#include <swfdec/swfdec_bots.h>

G_BEGIN_DECLS


typedef enum {
  SWFDEC_RTMP_PACKET_SIZE		= 0x01,
  SWFDEC_RTMP_PACKET_CONTINUE		= 0x03,
  SWFDEC_RTMP_PACKET_PING		= 0x04,
  SWFDEC_RTMP_PACKET_SERVER_BANDWIDTH	= 0x05,
  SWFDEC_RTMP_PACKET_CLIENT_BANDWIDTH	= 0x06,
  SWFDEC_RTMP_PACKET_AUDIO		= 0x08,
  SWFDEC_RTMP_PACKET_VIDEO		= 0x09,
  SWFDEC_RTMP_PACKET_FLEX_STREAM	= 0x0F,
  SWFDEC_RTMP_PACKET_FLEX_OBJECT	= 0x10,
  SWFDEC_RTMP_PACKET_FLEX_MESSAGE	= 0x11,
  SWFDEC_RTMP_PACKET_NOTIFY		= 0x12,
  SWFDEC_RTMP_PACKET_SHARED_OBJECT	= 0x13,
  SWFDEC_RTMP_PACKET_INVOKE		= 0x14,
  SWFDEC_RTMP_PACKET_FLV		= 0x16,
} SwfdecRtmpPacketType;

typedef enum {
  SWFDEC_RTMP_HEADER_12_BYTES = 0,
  SWFDEC_RTMP_HEADER_8_BYTES = 1,
  SWFDEC_RTMP_HEADER_4_BYTES = 2,
  SWFDEC_RTMP_HEADER_1_BYTE = 3
} SwfdecRtmpHeaderSize;

typedef struct _SwfdecRtmpHeader SwfdecRtmpHeader;

struct _SwfdecRtmpHeader {
  guint			channel;	/* channel to send this header to */
  guint			timestamp;	/* timestamp associated with this header */
  guint			size;		/* size of message in bytes */
  SwfdecRtmpPacketType	type;		/* type of message */
  guint			stream;		/* id of stream associated with message */
};

#define SWFDEC_RTMP_HEADER_INVALID { (guint) -1, 0, 0, 0, 0 }

gsize			swfdec_rtmp_header_size_get	(SwfdecRtmpHeaderSize		size);

void			swfdec_rtmp_header_invalidate	(SwfdecRtmpHeader *	  	header);
#define swfdec_rtmp_header_copy(dest, src) *(dest) = *(src)

void			swfdec_rtmp_header_read		(SwfdecRtmpHeader *		header,
							 SwfdecBits *			bits);
void			swfdec_rtmp_header_write      	(const SwfdecRtmpHeader *     	header,
							 SwfdecBots *			bots,
							 SwfdecRtmpHeaderSize 		size);

SwfdecRtmpHeaderSize	swfdec_rtmp_header_diff		(const SwfdecRtmpHeader *       a,
							 const SwfdecRtmpHeader *	b);


G_END_DECLS
#endif
