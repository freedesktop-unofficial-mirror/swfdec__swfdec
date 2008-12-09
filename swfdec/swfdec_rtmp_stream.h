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

#ifndef __SWFDEC_RTMP_STREAM_H__
#define __SWFDEC_RTMP_STREAM_H__

#include <swfdec/swfdec_rtmp_connection.h>
#include <swfdec/swfdec_rtmp_packet.h>

G_BEGIN_DECLS


#define SWFDEC_TYPE_RTMP_STREAM                (swfdec_rtmp_stream_get_type ())
#define SWFDEC_RTMP_STREAM(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_RTMP_STREAM, SwfdecRtmpStream))
#define SWFDEC_IS_RTMP_STREAM(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_RTMP_STREAM))
#define SWFDEC_RTMP_STREAM_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), SWFDEC_TYPE_RTMP_STREAM, SwfdecRtmpStreamInterface))

typedef struct _SwfdecRtmpStreamInterface SwfdecRtmpStreamInterface;

struct _SwfdecRtmpStreamInterface {
  GTypeInterface	parent;

  /* mandatory vfunc */
  void			(* receive)      	(SwfdecRtmpStream *		stream,
						 const SwfdecRtmpHeader *	header,
						 SwfdecBuffer *			buffer);
  SwfdecRtmpPacket *	(* sent)		(SwfdecRtmpStream *		stream,
						 const SwfdecRtmpPacket *	packet);
};

GType			swfdec_rtmp_stream_get_type	(void) G_GNUC_CONST;

void			swfdec_rtmp_stream_receive	(SwfdecRtmpStream *	stream,
							 const SwfdecRtmpPacket *packet);
SwfdecRtmpPacket *	swfdec_rtmp_stream_sent		(SwfdecRtmpStream *	stream,
						  	 const SwfdecRtmpPacket *packet);


G_END_DECLS

#endif /* __SWFDEC_RTMP_STREAM_H__ */
