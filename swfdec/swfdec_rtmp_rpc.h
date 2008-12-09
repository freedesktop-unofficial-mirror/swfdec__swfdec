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

#ifndef _SWFDEC_RTMP_RPC_H_
#define _SWFDEC_RTMP_RPC_H_

#include <swfdec/swfdec_rtmp_connection.h>
#include <swfdec/swfdec_rtmp_packet.h>

G_BEGIN_DECLS


struct _SwfdecRtmpRpc {
  SwfdecRtmpConnection *	conn;		/* connection to use */
  SwfdecAsRelay *		target;		/* object to call received calls on */
  guint				id;		/* last id used for RPC call */
  GHashTable *			pending;	/* int => SwfdecAsObject mapping of calls having pending replies */
  GQueue *			packets;	/* outstanding SwfdecRtmpPackets */
  gboolean			packet_pending;	/* if a packet is known to be pending */
  GTimeVal			last_send;	/* time the last call was sent */
};

SwfdecRtmpRpc *		swfdec_rtmp_rpc_new		(SwfdecRtmpConnection *	conn,
							 SwfdecAsRelay *	target);
void			swfdec_rtmp_rpc_free		(SwfdecRtmpRpc *	rpc);
void			swfdec_rtmp_rpc_mark		(SwfdecRtmpRpc *	rpc);

SwfdecBuffer *		swfdec_rtmp_rpc_encode		(SwfdecAsContext *	context,
							 SwfdecAsValue		name,
							 guint			reply_id,
							 SwfdecAsValue		special,
							 guint			argc,
							 const SwfdecAsValue *	argv);
							 
SwfdecRtmpPacket *	swfdec_rtmp_rpc_pop		(SwfdecRtmpRpc *	rpc,
							 gboolean		pull_if_pending);
void			swfdec_rtmp_rpc_send		(SwfdecRtmpRpc *	rpc,
							 SwfdecAsValue		name,
							 SwfdecAsObject *	reply_to,
							 guint			argc,
							 const SwfdecAsValue *	argv);
gboolean		swfdec_rtmp_rpc_receive		(SwfdecRtmpRpc *	rpc,
							 SwfdecBuffer *		buffer);
void			swfdec_rtmp_rpc_notify		(SwfdecRtmpRpc *	rpc,
							 SwfdecBuffer *		buffer);


G_END_DECLS
#endif
