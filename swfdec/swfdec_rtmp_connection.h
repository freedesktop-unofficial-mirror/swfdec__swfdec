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
#include <swfdec/swfdec_rtmp_header.h>
#include <swfdec/swfdec_rtmp_packet.h>
#include <swfdec/swfdec_types.h>

G_BEGIN_DECLS


/* size of a packet block */
#define SWFDEC_RTMP_BLOCK_SIZE 128

/* forward declarations */
typedef struct _SwfdecRtmpHandshake SwfdecRtmpHandshake;
typedef struct _SwfdecRtmpRpc SwfdecRtmpRpc;
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
  SwfdecAsRelay			relay;

  SwfdecURL *			url;		/* URL in use by this connection */
  SwfdecSandbox *		sandbox;	/* sandbox we execute functions in or NULL */
  SwfdecRtmpSocket *		socket;		/* socket we're using for read/write */
  SwfdecRtmpHandshake *		handshake;	/* structure used for doing initial handshake or NULL */
  char *			error;		/* NULL or debug string for error message */
  GHashTable *			incoming;	/* channel id => incoming packets */
  GHashTable *			streams;	/* stream id => stream */
  GQueue *			packets;	/* queue of packets in send order */
  GTimeVal			connect_time;	/* time at which this connectioon was opened */

  GQueue *			control_packets;/* packets waiting to be sent on queue 2 */
  SwfdecRtmpRpc *		rpc;		/* queue for rpc */

  guint				read_size;	/* size of a block of data when reading */
  guint				write_size;	/* size of a block of data when writing */
  guint				server_bandwidth; /* ??? */
  guint				client_bandwidth; /* ??? */
};

struct _SwfdecRtmpConnectionClass {
  SwfdecAsRelayClass		relay_class;
};

GType			swfdec_rtmp_connection_get_type		(void);

#define swfdec_rtmp_connection_is_connected(conn) ((conn)->socket != NULL && (conn)->error == NULL)
void			swfdec_rtmp_connection_connect	  	(SwfdecRtmpConnection *	conn,
								 const SwfdecURL *	url);
void			swfdec_rtmp_connection_close		(SwfdecRtmpConnection *	conn);

void			swfdec_rtmp_connection_receive		(SwfdecRtmpConnection *	conn,
								 SwfdecBufferQueue *	queue);
void			swfdec_rtmp_connection_send		(SwfdecRtmpConnection *	conn,
								 SwfdecRtmpPacket *	packet);
void			swfdec_rtmp_connection_queue_control_packet
								(SwfdecRtmpConnection *	conn,
								 SwfdecRtmpPacket *	packet);

void			swfdec_rtmp_connection_set_connected	(SwfdecRtmpConnection *	conn,
								 const char *		url);
void			swfdec_rtmp_connection_error		(SwfdecRtmpConnection *	conn,
								 const char *		error,
								 ...) G_GNUC_PRINTF (2, 3);
void			swfdec_rtmp_connection_errorv		(SwfdecRtmpConnection *	conn,
								 const char *		error,
								 va_list		args) G_GNUC_PRINTF (2, 0);
void			swfdec_rtmp_connection_on_status	(SwfdecRtmpConnection *	conn,
								 SwfdecAsValue		value);

void			swfdec_rtmp_connection_register_stream	(SwfdecRtmpConnection *	conn,
								 guint			id,
								 SwfdecRtmpStream *	stream);
void			swfdec_rtmp_connection_unregister_stream(SwfdecRtmpConnection *	conn,
								 guint			id);
SwfdecRtmpStream *	swfdec_rtmp_connection_get_stream	(SwfdecRtmpConnection *	conn,
								 guint			id);


G_END_DECLS
#endif
