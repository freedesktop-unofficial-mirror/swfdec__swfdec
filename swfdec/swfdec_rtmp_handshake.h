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

#ifndef _SWFDEC_RTMP_HANDSHAKE_H_
#define _SWFDEC_RTMP_HANDSHAKE_H_

#include <swfdec/swfdec_rtmp_connection.h>

G_BEGIN_DECLS


struct _SwfdecRtmpHandshake {
  SwfdecRtmpConnection *      	conn;		/* connection we use */

  SwfdecBuffer *		next_buffer;	/* buffer we want to send */
  SwfdecBuffer *		initial;	/* initial buffer that was sent */
};

GType			swfdec_rtmp_handshake_get_type	(void);

SwfdecRtmpHandshake *	swfdec_rtmp_handshake_new		(SwfdecRtmpConnection *	conn);
void			swfdec_rtmp_handshake_free		(SwfdecRtmpHandshake *	shake);

SwfdecBuffer *		swfdec_rtmp_handshake_next_buffer	(SwfdecRtmpHandshake *	shake);
gboolean		swfdec_rtmp_handshake_receive		(SwfdecRtmpHandshake *	shake,
								 SwfdecBufferQueue *	queue);


G_END_DECLS
#endif
