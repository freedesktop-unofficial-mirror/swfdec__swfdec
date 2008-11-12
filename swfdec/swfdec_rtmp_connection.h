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

#ifndef _SWFDEC_RTMP_CONNECTION_H_
#define _SWFDEC_RTMP_CONNECTION_H_

#include <swfdec/swfdec.h>
#include <swfdec/swfdec_as_relay.h>

G_BEGIN_DECLS

/* forward declarations */
typedef struct _SwfdecRtmpSocket SwfdecRtmpSocket;
typedef struct _SwfdecRtmpStream SwfdecRtmpStream;

typedef struct _SwfdecRtmpConnection SwfdecRtmpConnection;
typedef struct _SwfdecRtmpConnectionClass SwfdecRtmpConnectionClass;

#define SWFDEC_TYPE_RTMP_CONNECTION                    (swfdec_rtmp_connection_get_type())
#define SWFDEC_IS_RTMP_CONNECTION(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_RTMP_CONNECTION))
#define SWFDEC_IS_RTMP_CONNECTION_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_RTMP_CONNECTION))
#define SWFDEC_RTMP_CONNECTION(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_RTMP_CONNECTION, SwfdecRtmpConnection))
#define SWFDEC_RTMP_CONNECTION_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_RTMP_CONNECTION, SwfdecRtmpConnectionClass))
#define SWFDEC_RTMP_CONNECTION_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_RTMP_CONNECTION, SwfdecRtmpConnectionClass))

struct _SwfdecRtmpConnection {
  SwfdecAsRelay		relay;

  SwfdecRtmpSocket *	socket;		/* socket we're using for read/write */
  /* FIXME: make this a GArray for size savings? Are 256 or 512 bytes really worth it? */
  SwfdecRtmpStream *	streams[64];	/* the streams we're using */
};

struct _SwfdecRtmpConnectionClass {
  SwfdecAsRelayClass	relay_class;
};

GType			swfdec_rtmp_connection_get_type	(void);

void			swfdec_rtmp_connection_connect	(SwfdecRtmpConnection *	conn,
							 const char *		url);
void			swfdec_rtmp_connection_close	(SwfdecRtmpConnection *	conn);


G_END_DECLS
#endif
