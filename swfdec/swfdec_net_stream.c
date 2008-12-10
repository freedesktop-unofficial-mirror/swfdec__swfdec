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

#include "swfdec_net_stream.h"

#include "swfdec_as_frame_internal.h"
#include "swfdec_as_internal.h"
#include "swfdec_as_strings.h"
#include "swfdec_debug.h"
#include "swfdec_net_stream_video.h"
#include "swfdec_sandbox.h"
#include "swfdec_rtmp_rpc.h"
#include "swfdec_rtmp_stream.h"

#define SWFDEC_NET_STREAM_RPC_CHANNEL(stream) ((((stream)->stream - 1) * 5 % 65592) + 8)
#define SWFDEC_NET_STREAM_VIDEO_CHANNEL(stream) ((((stream)->stream - 1) * 5 % 65592) + 5)
#define SWFDEC_NET_STREAM_AUDIO_CHANNEL(stream) ((((stream)->stream - 1) * 5 % 65592) + 6)

static void
swfdec_net_stream_onstatus (SwfdecNetStream *stream, const char *code)
{
  SwfdecAsValue val;
  SwfdecAsObject *object;
  SwfdecAsContext *cx;

  cx = swfdec_gc_object_get_context (stream);
  swfdec_sandbox_use (stream->conn->sandbox);

  object = swfdec_as_object_new (cx, SWFDEC_AS_STR_Object, NULL);
  SWFDEC_INFO ("emitting onStatus for %s", code);
  SWFDEC_AS_VALUE_SET_STRING (&val, code);
  swfdec_as_object_set_variable (object, SWFDEC_AS_STR_code, &val);
  SWFDEC_AS_VALUE_SET_STRING (&val, SWFDEC_AS_STR_level);
  swfdec_as_object_set_variable (object, SWFDEC_AS_STR_level, &val);

  SWFDEC_AS_VALUE_SET_OBJECT (&val, object);
  if (!swfdec_as_relay_call (SWFDEC_AS_RELAY (stream),
        SWFDEC_AS_STR_onStatus, 1, &val, NULL)) {
#if 0
    // if it's an error message and the stream object didn't have onStatus
    // handler, call System.onStatus
    if (level == SWFDEC_AS_STR_error) {
      SwfdecAsValue system;

      swfdec_as_object_get_variable (cx->global,
          SWFDEC_AS_STR_System, &system);
      if (SWFDEC_AS_VALUE_IS_COMPOSITE (system) &&
	  (object = SWFDEC_AS_VALUE_GET_COMPOSITE (system)) != NULL) {
        swfdec_as_object_call (object, SWFDEC_AS_STR_onStatus, 1, &val, NULL);
      }
    }
#endif
  }
  swfdec_sandbox_unuse (stream->conn->sandbox);
}

static void
swfdec_net_stream_rtmp_stream_receive (SwfdecRtmpStream *rtmp_stream,
    const SwfdecRtmpHeader *header, SwfdecBuffer *buffer)
{
  SwfdecNetStream *stream = SWFDEC_NET_STREAM (rtmp_stream);

  switch ((guint) header->type) {
    case SWFDEC_RTMP_PACKET_VIDEO:
      swfdec_net_stream_video_push (stream->video, header, buffer);
      break;
    case SWFDEC_RTMP_PACKET_NOTIFY:
      swfdec_sandbox_use (stream->conn->sandbox);
      swfdec_rtmp_rpc_notify (stream->rpc, buffer);
      swfdec_sandbox_unuse (stream->conn->sandbox);
      break;
    case SWFDEC_RTMP_PACKET_INVOKE:
      swfdec_sandbox_use (stream->conn->sandbox);
      if (swfdec_rtmp_rpc_receive (stream->rpc, buffer)) {
	SwfdecRtmpPacket *packet = swfdec_rtmp_rpc_pop (stream->rpc, FALSE);
	if (packet) {
	  packet->header.channel = SWFDEC_NET_STREAM_RPC_CHANNEL (stream);
	  packet->header.stream = stream->stream;
	  swfdec_rtmp_connection_send (stream->conn, packet);
	}
      }
      swfdec_sandbox_unuse (stream->conn->sandbox);
      break;
    default:
      SWFDEC_FIXME ("what to do with header type %u (channel %u, stream %u)?",
	  header->type, header->channel, header->stream);
      break;
  }
}

static SwfdecRtmpPacket *
swfdec_net_stream_rtmp_stream_sent (SwfdecRtmpStream *rtmp_stream,
    const SwfdecRtmpPacket *packet)
{
  SwfdecNetStream *stream = SWFDEC_NET_STREAM (rtmp_stream);
  SwfdecRtmpPacket *result;

  if (packet->header.channel == SWFDEC_NET_STREAM_RPC_CHANNEL (stream)) {
    result = swfdec_rtmp_rpc_pop (stream->rpc, TRUE);
    if (result) {
      result->header.channel = SWFDEC_NET_STREAM_RPC_CHANNEL (stream);
      result->header.stream = stream->stream;
    }
  } else {
    result = NULL;
    g_assert_not_reached ();
  }

  return result;
}

static void
swfdec_net_stream_rtmp_stream_sync (SwfdecRtmpStream *stream)
{
  SWFDEC_FIXME ("implement");
}

static void
swfdec_net_stream_rtmp_stream_flush (SwfdecRtmpStream *rtmp_stream)
{
  SwfdecNetStream *stream = SWFDEC_NET_STREAM (rtmp_stream);

  swfdec_net_stream_onstatus (stream, SWFDEC_AS_STR_NetStream_Buffer_Flush);
}

static void
swfdec_net_stream_rtmp_stream_clear (SwfdecRtmpStream *stream)
{
  SWFDEC_FIXME ("implement");
}

static void
swfdec_net_stream_rtmp_stream_init (SwfdecRtmpStreamInterface *iface)
{
  iface->receive = swfdec_net_stream_rtmp_stream_receive;
  iface->sent = swfdec_net_stream_rtmp_stream_sent;
  iface->sync = swfdec_net_stream_rtmp_stream_sync;
  iface->flush = swfdec_net_stream_rtmp_stream_flush;
  iface->clear = swfdec_net_stream_rtmp_stream_clear;
}

/*** NET STREAM ***/

G_DEFINE_TYPE_WITH_CODE (SwfdecNetStream, swfdec_net_stream, SWFDEC_TYPE_AS_RELAY,
    G_IMPLEMENT_INTERFACE (SWFDEC_TYPE_RTMP_STREAM, swfdec_net_stream_rtmp_stream_init))

static void
swfdec_net_stream_mark (SwfdecGcObject *object)
{
  SwfdecNetStream *stream = SWFDEC_NET_STREAM (object);

  swfdec_gc_object_mark (stream->conn);
  swfdec_rtmp_rpc_mark (stream->rpc);
  swfdec_gc_object_mark (stream->video);

  SWFDEC_GC_OBJECT_CLASS (swfdec_net_stream_parent_class)->mark (object);
}

static void
swfdec_net_stream_video_buffer_status (SwfdecNetStreamVideo *video, GParamSpec *pspec,
    SwfdecNetStream* stream)
{
  swfdec_net_stream_onstatus (stream, video->playing ?
      SWFDEC_AS_STR_NetStream_Buffer_Full : SWFDEC_AS_STR_NetStream_Buffer_Empty);
}

static void
swfdec_net_stream_dispose (GObject *object)
{
  SwfdecNetStream *stream = SWFDEC_NET_STREAM (object);

  if (stream->rpc) {
    swfdec_rtmp_rpc_free (stream->rpc);
    stream->rpc = NULL;
  }
  g_signal_handlers_disconnect_by_func (stream->video, 
      swfdec_net_stream_video_buffer_status, stream);
  g_object_unref (stream->video);

  G_OBJECT_CLASS (swfdec_net_stream_parent_class)->dispose (object);
}

static void
swfdec_net_stream_class_init (SwfdecNetStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_net_stream_dispose;

  gc_class->mark = swfdec_net_stream_mark;
}

static void
swfdec_net_stream_init (SwfdecNetStream *stream)
{
}

/*** AS CODE ***/

SWFDEC_AS_NATIVE (2101, 0, swfdec_net_stream_close)
void
swfdec_net_stream_close (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.close");
}

SWFDEC_AS_NATIVE (2101, 1, swfdec_net_stream_attachAudio)
void
swfdec_net_stream_attachAudio(SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.attachAudio");
}

SWFDEC_AS_NATIVE (2101, 2, swfdec_net_stream_attachVideo)
void
swfdec_net_stream_attachVideo (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.attachVideo");
}

SWFDEC_AS_NATIVE (2101, 3, swfdec_net_stream_send)
void
swfdec_net_stream_send (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.send");
}

SWFDEC_AS_NATIVE (2101, 4, swfdec_net_stream_setBufferTime)
void
swfdec_net_stream_setBufferTime (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.setBufferTime");
}

SWFDEC_AS_NATIVE (2101, 5, swfdec_net_stream_get_checkPolicyFile)
void
swfdec_net_stream_get_checkPolicyFile (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.checkPolicyFile (get)");
}

SWFDEC_AS_NATIVE (2101, 6, swfdec_net_stream_set_checkPolicyFile)
void
swfdec_net_stream_set_checkPolicyFile (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.checkPolicyFile (set)");
}

static void
swfdec_net_stream_get_audiocodec (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.audiocodec (get)");
}

static void
swfdec_net_stream_get_bufferLength (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.bufferLength (get)");
}

static void
swfdec_net_stream_get_bufferTime (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.bufferTime (get)");
}

static void
swfdec_net_stream_get_bytesLoaded (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.bytesLoaded (get)");
}

static void
swfdec_net_stream_get_bytesTotal (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.bytesTotal (get)");
}

static void
swfdec_net_stream_get_currentFps (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.currentFps (get)");
}

static void
swfdec_net_stream_get_decodedFrames (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.decodedFrames (get)");
}

static void
swfdec_net_stream_get_liveDelay (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.liveDelay (get)");
}

static void
swfdec_net_stream_get_time (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecNetStream *stream;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_NET_STREAM, &stream, "");

  *rval = swfdec_as_value_from_number (cx, stream->video->time / 1000.);
}

static void
swfdec_net_stream_get_videocodec (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.videocodec (get)");
}

static void
swfdec_net_stream_install_properties (SwfdecAsObject *object)
{
  object = object->prototype;
  if (object == NULL)
    return;

  swfdec_as_object_add_native_variable (object, SWFDEC_AS_STR_audiocodec,
      swfdec_net_stream_get_audiocodec, NULL);
  swfdec_as_object_add_native_variable (object, SWFDEC_AS_STR_bufferLength,
      swfdec_net_stream_get_bufferLength, NULL);
  swfdec_as_object_add_native_variable (object, SWFDEC_AS_STR_bufferTime,
      swfdec_net_stream_get_bufferTime, NULL);
  swfdec_as_object_add_native_variable (object, SWFDEC_AS_STR_bytesLoaded,
      swfdec_net_stream_get_bytesLoaded, NULL);
  swfdec_as_object_add_native_variable (object, SWFDEC_AS_STR_bytesTotal,
      swfdec_net_stream_get_bytesTotal, NULL);
  swfdec_as_object_add_native_variable (object, SWFDEC_AS_STR_currentFps,
      swfdec_net_stream_get_currentFps, NULL);
  swfdec_as_object_add_native_variable (object, SWFDEC_AS_STR_decodedFrames,
      swfdec_net_stream_get_decodedFrames, NULL);
  swfdec_as_object_add_native_variable (object, SWFDEC_AS_STR_liveDelay,
      swfdec_net_stream_get_liveDelay, NULL);
  swfdec_as_object_add_native_variable (object, SWFDEC_AS_STR_time,
      swfdec_net_stream_get_time, NULL);
  swfdec_as_object_add_native_variable (object, SWFDEC_AS_STR_videocodec,
      swfdec_net_stream_get_videocodec, NULL);
}

SWFDEC_AS_NATIVE (2101, 200, swfdec_net_stream_construct)
void
swfdec_net_stream_construct (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecNetStream *stream;
  SwfdecAsObject *o, *oconn;
  SwfdecRtmpConnection *conn;

  SWFDEC_AS_CHECK (0, NULL, "oo", &o, &oconn);

  swfdec_net_stream_install_properties (o);

  if (!cx->frame->next || !cx->frame->next->construct)
    return;
  if (!SWFDEC_IS_RTMP_CONNECTION (oconn->relay))
    return;
  conn = SWFDEC_RTMP_CONNECTION (oconn->relay);
  if (o->movie) {
    SWFDEC_FIXME ("you managed to call SwfdecNetStream's constructor from a movie. Congrats, but what now?");
    return;
  }

  stream = g_object_new (SWFDEC_TYPE_NET_STREAM, "context", cx, NULL);
  stream->conn = conn;
  stream->rpc = swfdec_rtmp_rpc_new (conn, SWFDEC_AS_RELAY (stream));
  stream->video = swfdec_net_stream_video_new (SWFDEC_PLAYER (cx));
  g_object_ref (stream->video);
  g_signal_connect (stream->video, "notify::playing", 
      G_CALLBACK (swfdec_net_stream_video_buffer_status), stream);
  swfdec_as_context_get_time (cx, &stream->rpc->last_send);
  swfdec_as_object_set_relay (o, SWFDEC_AS_RELAY (stream));
}

static void
swfdec_net_stream_send_buffer_time (SwfdecNetStream *stream)
{
  SwfdecRtmpPacket *packet;
  SwfdecBots *bots;
  SwfdecBuffer *buffer;

  bots = swfdec_bots_new ();
  swfdec_bots_put_bu16 (bots, 3);
  swfdec_bots_put_bu32 (bots, stream->stream);
  swfdec_bots_put_bu32 (bots, stream->video->buffer_time);
  buffer = swfdec_bots_close (bots);

  packet = swfdec_rtmp_packet_new (2, 0, SWFDEC_RTMP_PACKET_PING, 0, buffer);
  swfdec_buffer_unref (buffer);
  swfdec_rtmp_connection_queue_control_packet (stream->conn, packet);
}

SWFDEC_AS_NATIVE (2101, 201, swfdec_net_stream_onCreate)
void
swfdec_net_stream_onCreate (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecNetStream *stream;
  SwfdecAsObject *o;
  guint stream_id;
  SwfdecRtmpPacket *packet;

  SWFDEC_AS_CHECK (0, NULL, "oi", &o, &stream_id);

  if (!SWFDEC_IS_NET_STREAM (o->relay))
    return;
  stream = SWFDEC_NET_STREAM (o->relay);

  stream->stream = stream_id;
  swfdec_rtmp_connection_register_stream (stream->conn, 
      stream_id, SWFDEC_RTMP_STREAM (stream));

  packet = swfdec_rtmp_rpc_pop (stream->rpc, FALSE);
  if (packet) {
    packet->header.channel = SWFDEC_NET_STREAM_RPC_CHANNEL (stream);
    packet->header.stream = stream->stream;
    swfdec_rtmp_connection_send (stream->conn, packet);
  }
  swfdec_net_stream_send_buffer_time (stream);
}

SWFDEC_AS_NATIVE (2101, 202, swfdec_net_stream_send_connection)
void
swfdec_net_stream_send_connection (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecNetStream *stream;
  SwfdecAsObject *o, *ret_cb = NULL;
  SwfdecAsValue name;

  SWFDEC_AS_CHECK (0, NULL, "ov|O", &o, &name, &ret_cb);

  if (!SWFDEC_IS_NET_STREAM (o->relay))
    return;
  stream = SWFDEC_NET_STREAM (o->relay);

  swfdec_rtmp_rpc_send (stream->rpc, name, ret_cb, MAX (3, argc) - 3, argv + 3);
  /* FIXME: This should be done by some smart API */
  if (stream->stream) {
    SwfdecRtmpPacket *packet = swfdec_rtmp_rpc_pop (stream->rpc, FALSE);
    if (packet) {
      packet->header.channel = SWFDEC_NET_STREAM_RPC_CHANNEL (stream);
      packet->header.stream = stream->stream;
      swfdec_rtmp_connection_send (stream->conn, packet);
    }
  }
}

