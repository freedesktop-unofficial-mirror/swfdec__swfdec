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

#include "swfdec_rtmp_rpc_channel.h"

#include "swfdec_amf.h"
#include "swfdec_as_strings.h"
#include "swfdec_debug.h"
#include "swfdec_rtmp_handshake_channel.h"
#include "swfdec_rtmp_socket.h"
#include "swfdec_sandbox.h"
#include "swfdec_utils.h"

/*** SwfdecRtmpRpcChannel ***/

G_DEFINE_TYPE (SwfdecRtmpRpcChannel, swfdec_rtmp_rpc_channel, SWFDEC_TYPE_RTMP_CHANNEL)

static guint
swfdec_rtmp_rpc_channel_update_last_send (SwfdecRtmpChannel *channel)
{
  GTimeVal tv;
  long diff;

  swfdec_rtmp_channel_get_time (channel, &tv);
  diff = swfdec_time_val_diff (&channel->timestamp, &tv);
  channel->timestamp = tv;
  return diff;
}

static void
swfdec_rtmp_rpc_channel_do_send (SwfdecRtmpRpcChannel *rpc, SwfdecAsValue name, 
    guint id, SwfdecAsValue special, guint argc, const SwfdecAsValue *argv)
{
  SwfdecRtmpChannel *channel;
  SwfdecAsContext *context;
  SwfdecRtmpHeader header;
  SwfdecAmfContext *cx;
  SwfdecBuffer *buffer;
  SwfdecBots *bots;
  guint i;

  channel = SWFDEC_RTMP_CHANNEL (rpc);
  context = swfdec_gc_object_get_context (channel->conn);

  /* prepare buffer to encode */
  cx = swfdec_amf_context_new (context);
  bots = swfdec_bots_new ();
  swfdec_amf_encode (cx, bots, name);
  swfdec_amf_encode (cx, bots, swfdec_as_value_from_number (context, id));
  swfdec_amf_encode (cx, bots, special);
  for (i = 0; i < argc; i++) {
    swfdec_amf_encode (cx, bots, argv[i]);
  }
  buffer = swfdec_bots_close (bots);
  swfdec_amf_context_free (cx);

  header.channel = channel->id;
  header.timestamp = swfdec_rtmp_rpc_channel_update_last_send (channel);
  header.size = buffer->length;
  header.type = SWFDEC_RTMP_PACKET_INVOKE;
  header.stream = 0;

  swfdec_rtmp_channel_send (channel, &header, buffer);
}

static void
swfdec_rtmp_rpc_channel_receive_reply (SwfdecRtmpChannel *channel, 
    SwfdecAmfContext *cx, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (channel->conn);
  SwfdecAsObject *reply_to;
  SwfdecAsValue val[2], tmp;
  guint id, i;

  if (!swfdec_amf_decode (cx, bits, &tmp)) {
    SWFDEC_ERROR ("could not decode reply id");
    return;
  }
  id = swfdec_as_value_to_integer (context, tmp);
  
  for (i = 0; swfdec_bits_left (bits) && i < 2; i++) {
    if (!swfdec_amf_decode (cx, bits, &val[i])) {
      SWFDEC_ERROR ("could not decode reply value");
      return;
    }
  }
  if (swfdec_bits_left (bits)) {
    SWFDEC_FIXME ("more than 2 values in a reply?");
  }

  if (id == 1 && SWFDEC_IS_RTMP_HANDSHAKE_CHANNEL (channel->conn->channels[0])) {
    swfdec_rtmp_handshake_channel_connected (SWFDEC_RTMP_HANDSHAKE_CHANNEL (channel->conn->channels[0]),
	  i, val);
  } else {
    if (!SWFDEC_AS_VALUE_IS_NULL (val[0])) {
      SWFDEC_FIXME ("first argument in reply is not null?");
    }
    reply_to = g_hash_table_lookup (SWFDEC_RTMP_RPC_CHANNEL (channel)->pending, 
	GUINT_TO_POINTER (id));
    if (reply_to == NULL) {
      SWFDEC_ERROR ("no object to send a reply to");
      return;
    }
    g_hash_table_steal (SWFDEC_RTMP_RPC_CHANNEL (channel)->pending, GUINT_TO_POINTER (id));
    swfdec_as_object_call (reply_to, SWFDEC_AS_STR_onResult, 1, &val[1], NULL);
  }
}

static void
swfdec_rtmp_rpc_channel_receive_call (SwfdecRtmpChannel *channel, 
    SwfdecAmfContext *cx, SwfdecAsValue val, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (channel->conn);
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
  swfdec_as_relay_call (SWFDEC_AS_RELAY (channel->conn), name,
      i, args, &val);
  g_free (args);

  /* send reply */
  if (id) {
    swfdec_rtmp_rpc_channel_do_send (SWFDEC_RTMP_RPC_CHANNEL (channel),
	SWFDEC_AS_VALUE_FROM_STRING (SWFDEC_AS_STR__result), id, val, 0, NULL);
  }
}

static void
swfdec_rtmp_rpc_channel_receive (SwfdecRtmpChannel *channel,
    const SwfdecRtmpHeader *header, SwfdecBuffer *buffer)
{
  SwfdecAsContext *context;
  SwfdecAmfContext *cx;
  SwfdecAsValue val;
  SwfdecBits bits;

  if (header->stream != 0) {
    SWFDEC_FIXME ("not stream 0, but stream %u here?!", header->stream);
  }
  context = swfdec_gc_object_get_context (channel->conn);
  cx = swfdec_amf_context_new (context);
  swfdec_sandbox_use (channel->conn->sandbox);
  switch ((guint) header->type) {
    case SWFDEC_RTMP_PACKET_INVOKE:
      swfdec_bits_init (&bits, buffer);
      if (!swfdec_amf_decode (cx, &bits, &val)) {
	SWFDEC_ERROR ("could not decode call name");
	break;
      }
      if (SWFDEC_AS_VALUE_IS_STRING (val) && 
	  SWFDEC_AS_VALUE_GET_STRING (val) == SWFDEC_AS_STR__result) {
	swfdec_rtmp_rpc_channel_receive_reply (channel, cx, &bits);
      } else {
	swfdec_rtmp_rpc_channel_receive_call (channel, cx, val, &bits);
      }
      break;
    default:
      SWFDEC_FIXME ("what to do with header type %u?", header->type);
      break;
  }
  swfdec_sandbox_unuse (channel->conn->sandbox);
  swfdec_amf_context_free (cx);
}

static void
swfdec_rtmp_rpc_channel_mark (SwfdecRtmpChannel *channel)
{
  SwfdecRtmpRpcChannel *rpc = SWFDEC_RTMP_RPC_CHANNEL (channel);
  GHashTableIter iter;
  gpointer value;

  for (g_hash_table_iter_init (&iter, rpc->pending);
       g_hash_table_iter_next (&iter, NULL, &value);) {
    swfdec_as_object_mark (value);
  }
}

static void
swfdec_rtmp_rpc_channel_dispose (GObject *object)
{
  SwfdecRtmpRpcChannel *rpc = SWFDEC_RTMP_RPC_CHANNEL (object);

  if (rpc->pending) {
    g_hash_table_destroy (rpc->pending);
    rpc->pending = NULL;
  }

  G_OBJECT_CLASS (swfdec_rtmp_rpc_channel_parent_class)->dispose (object);
}

static void
swfdec_rtmp_rpc_channel_class_init (SwfdecRtmpRpcChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecRtmpChannelClass *channel_class = SWFDEC_RTMP_CHANNEL_CLASS (klass);

  object_class->dispose = swfdec_rtmp_rpc_channel_dispose;

  channel_class->mark = swfdec_rtmp_rpc_channel_mark;
  channel_class->receive = swfdec_rtmp_rpc_channel_receive;
}

static void
swfdec_rtmp_rpc_channel_init (SwfdecRtmpRpcChannel *rpc)
{
  rpc->pending = g_hash_table_new (g_direct_hash, g_direct_equal);
}

void
swfdec_rtmp_rpc_channel_send_connect (SwfdecRtmpRpcChannel *rpc,
    SwfdecAsValue connect)
{
  swfdec_rtmp_rpc_channel_do_send (rpc, SWFDEC_AS_VALUE_FROM_STRING (SWFDEC_AS_STR_connect),
      ++rpc->id, connect, 0, NULL);
}

void
swfdec_rtmp_rpc_channel_send (SwfdecRtmpRpcChannel *rpc,
    SwfdecAsValue name, SwfdecAsObject *reply_to, 
    guint argc, const SwfdecAsValue *argv)
{
  guint id;

  g_return_if_fail (SWFDEC_IS_RTMP_RPC_CHANNEL (rpc));
  g_return_if_fail (argc == 0 || argv != NULL);

  if (reply_to) {
    id = ++rpc->id;
    g_hash_table_insert (rpc->pending, GUINT_TO_POINTER (id), reply_to);
  } else {
    id = 0;
  }
  swfdec_rtmp_rpc_channel_do_send (rpc, name, id, SWFDEC_AS_VALUE_NULL, argc, argv);
}

SwfdecRtmpChannel *
swfdec_rtmp_rpc_channel_new (SwfdecRtmpConnection *conn)
{
  g_return_val_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn), NULL);

  return g_object_new (SWFDEC_TYPE_RTMP_RPC_CHANNEL, "connection", conn, NULL);
}
