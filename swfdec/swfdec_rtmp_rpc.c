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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "swfdec_rtmp_rpc.h"

#include "swfdec_amf.h"
#include "swfdec_as_strings.h"
#include "swfdec_debug.h"
#include "swfdec_rtmp_handshake.h"
#include "swfdec_rtmp_packet.h"
#include "swfdec_rtmp_socket.h"
#include "swfdec_utils.h"

SwfdecBuffer *
swfdec_rtmp_rpc_encode (SwfdecAsContext *context, SwfdecAsValue name,
    guint reply_id, SwfdecAsValue special, guint argc, const SwfdecAsValue *argv)
{
  SwfdecAmfContext *cx;
  SwfdecBuffer *buffer;
  SwfdecBots *bots;
  guint i;

  cx = swfdec_amf_context_new (context);
  bots = swfdec_bots_new ();
  swfdec_amf_encode (cx, bots, name);
  swfdec_amf_encode (cx, bots, swfdec_as_value_from_number (context, reply_id));
  swfdec_amf_encode (cx, bots, special);
  for (i = 0; i < argc; i++) {
    swfdec_amf_encode (cx, bots, argv[i]);
  }
  buffer = swfdec_bots_close (bots);
  swfdec_amf_context_free (cx);

  return buffer;
}

static guint
swfdec_rtmp_rpc_update_last_send (SwfdecRtmpRpc *rpc)
{
  GTimeVal tv;
  long diff;

  swfdec_as_context_get_time (swfdec_gc_object_get_context (rpc->conn), &tv);
  diff = swfdec_time_val_diff (&rpc->last_send, &tv);
  rpc->last_send = tv;
  return diff;
}

static void
swfdec_rtmp_rpc_do_send (SwfdecRtmpRpc *rpc, SwfdecAsValue name, 
    guint id, SwfdecAsValue special, guint argc, const SwfdecAsValue *argv)
{
  SwfdecRtmpPacket *packet;
  SwfdecBuffer *buffer;
  gboolean empty;

  buffer = swfdec_rtmp_rpc_encode (swfdec_gc_object_get_context (rpc->conn),
      name, id, special, argc, argv);

  packet = swfdec_rtmp_packet_new (SWFDEC_RTMP_PACKET_INVOKE,
      swfdec_rtmp_rpc_update_last_send (rpc), buffer);
  swfdec_buffer_unref (buffer);
  empty = g_queue_is_empty (rpc->packets);
  g_queue_push_tail (rpc->packets, packet);
}

static void
swfdec_rtmp_rpc_receive_reply (SwfdecRtmpRpc *rpc,
    SwfdecAmfContext *cx, SwfdecBits *bits)
{
  SwfdecAsObject *reply_to;
  SwfdecAsValue val[2], tmp;
  guint id, i;

  if (!swfdec_amf_decode (cx, bits, &tmp)) {
    SWFDEC_ERROR ("could not decode reply id");
    return;
  }
  id = swfdec_as_value_to_integer (swfdec_gc_object_get_context (rpc->conn), tmp);
  
  for (i = 0; swfdec_bits_left (bits) && i < 2; i++) {
    if (!swfdec_amf_decode (cx, bits, &val[i])) {
      SWFDEC_ERROR ("could not decode reply value");
      return;
    }
  }
  if (swfdec_bits_left (bits)) {
    SWFDEC_FIXME ("more than 2 values in a reply?");
  }

  if (id == 1 && rpc->conn->handshake) {
    SwfdecRtmpConnection *conn = rpc->conn;

    /* FIXME: Do something with the result value */

    if (i >= 2) {
      swfdec_rtmp_connection_on_status (conn, val[1]);
    } else {
      SWFDEC_ERROR ("no 2nd argument in connect reply");
    }

    swfdec_rtmp_handshake_free (conn->handshake);
    conn->handshake = NULL;
    swfdec_rtmp_socket_send (conn->socket);
  } else {
    if (!SWFDEC_AS_VALUE_IS_NULL (val[0])) {
      SWFDEC_FIXME ("first argument in reply is not null?");
    }
    reply_to = g_hash_table_lookup (rpc->pending, 
	GUINT_TO_POINTER (id));
    if (reply_to == NULL) {
      SWFDEC_ERROR ("no object to send a reply to");
      return;
    }
    g_hash_table_steal (rpc->pending, GUINT_TO_POINTER (id));
    swfdec_as_object_call (reply_to, SWFDEC_AS_STR_onResult, 1, &val[1], NULL);
  }
}

static void
swfdec_rtmp_rpc_receive_call (SwfdecRtmpRpc *rpc, SwfdecAmfContext *cx, 
    SwfdecAsValue val, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (rpc->conn);
  const char *name;
  guint id, i;
  SwfdecAsValue *args;

  name = swfdec_as_value_to_string (context, val);
  if (!swfdec_amf_decode (cx, bits, &val)) {
    SWFDEC_ERROR ("could not decode reply id");
    return;
  }
  id = swfdec_as_value_to_integer (context, val);
  
  args = NULL;
  for (i = 0; swfdec_bits_left (bits); i++) {
    if ((i % 4) == 0)
      args = g_realloc (args, sizeof (SwfdecAsValue) * (i + 4));

    if (!swfdec_amf_decode (cx, bits, &args[i])) {
      SWFDEC_ERROR ("could not decode argument %u", i);
      return;
    }
  }
  swfdec_as_object_call (rpc->target, name, i, args, &val);
  g_free (args);

  /* send reply */
  if (id) {
    swfdec_rtmp_rpc_do_send (rpc, SWFDEC_AS_VALUE_FROM_STRING (SWFDEC_AS_STR__result), 
	id, val, 0, NULL);
  }
}

void
swfdec_rtmp_rpc_receive (SwfdecRtmpRpc *rpc,
    const SwfdecRtmpHeader *header, SwfdecBuffer *buffer)
{
  SwfdecAmfContext *cx;
  SwfdecAsContext *context;
  SwfdecAsValue val;
  SwfdecBits bits;

  context = swfdec_gc_object_get_context (rpc->conn);
  cx = swfdec_amf_context_new (context);
  g_assert (context->global);

  switch ((guint) header->type) {
    case SWFDEC_RTMP_PACKET_INVOKE:
      swfdec_bits_init (&bits, buffer);
      if (!swfdec_amf_decode (cx, &bits, &val)) {
	SWFDEC_ERROR ("could not decode call name");
	break;
      }
      if (SWFDEC_AS_VALUE_IS_STRING (val) && 
	  SWFDEC_AS_VALUE_GET_STRING (val) == SWFDEC_AS_STR__result) {
	swfdec_rtmp_rpc_receive_reply (rpc, cx, &bits);
      } else {
	swfdec_rtmp_rpc_receive_call (rpc, cx, val, &bits);
      }
      if (swfdec_bits_left (&bits)) {
	SWFDEC_FIXME ("%u bytes left after invoke on channel %u (stream %u)",
	    swfdec_bits_left (&bits) / 8, header->channel, header->stream);
      }
      break;
    default:
      SWFDEC_FIXME ("channel %u: what to do with header type %u?", 
	  header->channel, header->type);
      break;
  }
  swfdec_amf_context_free (cx);
}

SwfdecRtmpPacket *
swfdec_rtmp_rpc_pop (SwfdecRtmpRpc *rpc)
{
  g_return_val_if_fail (rpc != NULL, NULL);

  return g_queue_pop_head (rpc->packets);
}

void
swfdec_rtmp_rpc_send (SwfdecRtmpRpc *rpc, SwfdecAsValue name, 
    SwfdecAsObject *reply_to, guint argc, const SwfdecAsValue *argv)
{
  guint id;

  g_return_if_fail (rpc != NULL);
  g_return_if_fail (argc == 0 || argv != NULL);

  if (reply_to) {
    id = ++rpc->id;
    g_hash_table_insert (rpc->pending, GUINT_TO_POINTER (id), reply_to);
  } else {
    id = 0;
  }
  swfdec_rtmp_rpc_do_send (rpc, name, id, SWFDEC_AS_VALUE_NULL, argc, argv);
}

SwfdecRtmpRpc *
swfdec_rtmp_rpc_new (SwfdecRtmpConnection *conn, SwfdecAsObject *target)
{
  SwfdecRtmpRpc *rpc;

  g_return_val_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn), NULL);
  g_return_val_if_fail (target != NULL, NULL);

  rpc = g_slice_new (SwfdecRtmpRpc);
  rpc->conn = conn;
  rpc->target = target;
  rpc->pending = g_hash_table_new (g_direct_hash, g_direct_equal);
  rpc->packets = g_queue_new ();
  swfdec_as_context_get_time (swfdec_gc_object_get_context (conn), &rpc->last_send);

  return rpc;
}

void
swfdec_rtmp_rpc_mark (SwfdecRtmpRpc *rpc)
{
  GHashTableIter iter;
  gpointer value;

  g_return_if_fail (rpc != NULL);

  for (g_hash_table_iter_init (&iter, rpc->pending);
       g_hash_table_iter_next (&iter, NULL, &value);) {
    swfdec_as_object_mark (value);
  }

  swfdec_as_object_mark (rpc->target);
}

void
swfdec_rtmp_rpc_free (SwfdecRtmpRpc *rpc)
{
  g_return_if_fail (rpc != NULL);

  if (rpc->pending) {
    g_hash_table_destroy (rpc->pending);
    rpc->pending = NULL;
  }
  if (rpc->packets) {
    g_queue_foreach (rpc->packets, (GFunc) swfdec_rtmp_packet_free, NULL);
    g_queue_free (rpc->packets);
    rpc->packets = NULL;
  }
  g_slice_free (SwfdecRtmpRpc, rpc);
}

