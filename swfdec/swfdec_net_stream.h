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

#ifndef _SWFDEC_NET_STREAM_H_
#define _SWFDEC_NET_STREAM_H_

#include <swfdec/swfdec_rtmp_connection.h>

G_BEGIN_DECLS

/* forward decls */
typedef struct _SwfdecNetStreamVideo SwfdecNetStreamVideo;

typedef struct _SwfdecNetStream SwfdecNetStream;
typedef struct _SwfdecNetStreamClass SwfdecNetStreamClass;

#define SWFDEC_TYPE_NET_STREAM                    (swfdec_net_stream_get_type())
#define SWFDEC_IS_NET_STREAM(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_NET_STREAM))
#define SWFDEC_IS_NET_STREAM_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_NET_STREAM))
#define SWFDEC_NET_STREAM(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_NET_STREAM, SwfdecNetStream))
#define SWFDEC_NET_STREAM_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_NET_STREAM, SwfdecNetStreamClass))
#define SWFDEC_NET_STREAM_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_NET_STREAM, SwfdecNetStreamClass))

struct _SwfdecNetStream {
  SwfdecAsRelay			relay;

  SwfdecRtmpConnection *	conn;		/* the connection in use */
  SwfdecRtmpRpc *		rpc;		/* rpc */
  guint				stream;		/* id of this stream */
  SwfdecNetStreamVideo *	video;		/* video object */
};

struct _SwfdecNetStreamClass {
  SwfdecAsRelayClass		relay_class;
};

GType			swfdec_net_stream_get_type		(void);


G_END_DECLS
#endif
