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

#ifndef _SWFDEC_RTMP_SOCKET_RTMP_H_
#define _SWFDEC_RTMP_SOCKET_RTMP_H_

#include <swfdec/swfdec_rtmp_socket.h>

G_BEGIN_DECLS


typedef struct _SwfdecRtmpSocketRtmp SwfdecRtmpSocketRtmp;
typedef struct _SwfdecRtmpSocketRtmpClass SwfdecRtmpSocketRtmpClass;

#define SWFDEC_TYPE_RTMP_SOCKET_RTMP                    (swfdec_rtmp_socket_rtmp_get_type())
#define SWFDEC_IS_RTMP_SOCKET_RTMP(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_RTMP_SOCKET_RTMP))
#define SWFDEC_IS_RTMP_SOCKET_RTMP_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_RTMP_SOCKET_RTMP))
#define SWFDEC_RTMP_SOCKET_RTMP(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_RTMP_SOCKET_RTMP, SwfdecRtmpSocketRtmp))
#define SWFDEC_RTMP_SOCKET_RTMP_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_RTMP_SOCKET_RTMP, SwfdecRtmpSocketRtmpClass))
#define SWFDEC_RTMP_SOCKET_RTMP_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_RTMP_SOCKET_RTMP, SwfdecRtmpSocketRtmpClass))

struct _SwfdecRtmpSocketRtmp {
  SwfdecRtmpSocket	parent_socket;

  SwfdecStream *	socket;		/* the socket we use */
  SwfdecURL *		url;		/* the URL we're opening */
  SwfdecBuffer *	next;		/* buffer we're sending next - potentially a partial buffer */
};

struct _SwfdecRtmpSocketRtmpClass {
  SwfdecRtmpSocketClass	parent_socket_class;
};

GType			swfdec_rtmp_socket_rtmp_get_type	(void);



G_END_DECLS
#endif
