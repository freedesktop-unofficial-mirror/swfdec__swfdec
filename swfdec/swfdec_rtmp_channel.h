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

#ifndef _SWFDEC_RTMP_CHANNEL_H_
#define _SWFDEC_RTMP_CHANNEL_H_

#include <swfdec/swfdec_rtmp_connection.h>
#include <swfdec/swfdec_rtmp_header.h>

G_BEGIN_DECLS


typedef struct _SwfdecRtmpChannelClass SwfdecRtmpChannelClass;

#define SWFDEC_TYPE_RTMP_CHANNEL                    (swfdec_rtmp_channel_get_type())
#define SWFDEC_IS_RTMP_CHANNEL(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_RTMP_CHANNEL))
#define SWFDEC_IS_RTMP_CHANNEL_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_RTMP_CHANNEL))
#define SWFDEC_RTMP_CHANNEL(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_RTMP_CHANNEL, SwfdecRtmpChannel))
#define SWFDEC_RTMP_CHANNEL_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_RTMP_CHANNEL, SwfdecRtmpChannelClass))
#define SWFDEC_RTMP_CHANNEL_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_RTMP_CHANNEL, SwfdecRtmpChannelClass))

struct _SwfdecRtmpChannel {
  GObject			object;

  SwfdecRtmpConnection *	conn;		/* Connection this channel belongs to (holds reference) */
  guint				id;		/* id (0-63) inside connection */
  GTimeVal			start_time;	/* context time when starting this channel */
  SwfdecRtmpHeader		recv_cache;	/* cached header info for receiving data */
  SwfdecBufferQueue *		recv_queue;	/* Queue of semi-assembled packages when receiving */
  SwfdecRtmpHeader		send_cache;	/* cached header info for sending data */
  SwfdecBufferQueue *		send_queue;	/* Queue of outgoing waiting for delivery */
  guint				block_size;	/* maximum size for a single message block (FIXME: is this constant?) */
};

struct _SwfdecRtmpChannelClass {
  GObjectClass			object_class;

  void				(* mark)			(SwfdecRtmpChannel *	channel);
  void				(* receive)			(SwfdecRtmpChannel *	channel,
								 const SwfdecRtmpHeader *header,
								 SwfdecBuffer *		buffer);
};

GType			swfdec_rtmp_channel_get_type		(void);

void			swfdec_rtmp_channel_send		(SwfdecRtmpChannel *	channel,
								 const SwfdecRtmpHeader *header,
								 SwfdecBuffer *		data);
gboolean		swfdec_rtmp_channel_receive		(SwfdecRtmpChannel *	channel,
								 SwfdecBufferQueue *	queue,
								 SwfdecRtmpHeaderSize	header_size);

guint			swfdec_rtmp_channel_get_lifetime	(SwfdecRtmpChannel *	channel);



G_END_DECLS
#endif
