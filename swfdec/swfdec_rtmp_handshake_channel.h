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

#ifndef _SWFDEC_RTMP_HANDSHAKE_CHANNEL_H_
#define _SWFDEC_RTMP_HANDSHAKE_CHANNEL_H_

#include <swfdec/swfdec_rtmp_channel.h>

G_BEGIN_DECLS


typedef struct _SwfdecRtmpHandshakeChannel SwfdecRtmpHandshakeChannel;
typedef struct _SwfdecRtmpHandshakeChannelClass SwfdecRtmpHandshakeChannelClass;

#define SWFDEC_TYPE_RTMP_HANDSHAKE_CHANNEL                    (swfdec_rtmp_handshake_channel_get_type())
#define SWFDEC_IS_RTMP_HANDSHAKE_CHANNEL(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_RTMP_HANDSHAKE_CHANNEL))
#define SWFDEC_IS_RTMP_HANDSHAKE_CHANNEL_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_RTMP_HANDSHAKE_CHANNEL))
#define SWFDEC_RTMP_HANDSHAKE_CHANNEL(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_RTMP_HANDSHAKE_CHANNEL, SwfdecRtmpHandshakeChannel))
#define SWFDEC_RTMP_HANDSHAKE_CHANNEL_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_RTMP_HANDSHAKE_CHANNEL, SwfdecRtmpHandshakeChannelClass))
#define SWFDEC_RTMP_HANDSHAKE_CHANNEL_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_RTMP_HANDSHAKE_CHANNEL, SwfdecRtmpHandshakeChannelClass))

struct _SwfdecRtmpHandshakeChannel {
  SwfdecRtmpChannel		channel;

  SwfdecBuffer *		initial;	/* initial buffer that was sent */
  SwfdecBuffer *		reply;		/* reply that was gotten */
};

struct _SwfdecRtmpHandshakeChannelClass {
  SwfdecRtmpChannelClass      	channel_class;
};

GType		swfdec_rtmp_handshake_channel_get_type	(void);

void		swfdec_rtmp_handshake_channel_start	(SwfdecRtmpHandshakeChannel *	  channel);
gboolean	swfdec_rtmp_handshake_channel_receive	(SwfdecRtmpHandshakeChannel *	  channel,
							 SwfdecBufferQueue *		  queue);

G_END_DECLS
#endif
