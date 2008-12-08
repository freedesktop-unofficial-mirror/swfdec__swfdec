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
#include "swfdec_debug.h"
#include "swfdec_rtmp_rpc_channel.h"
#include "swfdec_rtmp_video_channel.h"

/*** NET STREAM ***/

G_DEFINE_TYPE (SwfdecNetStream, swfdec_net_stream, SWFDEC_TYPE_AS_RELAY)

static void
swfdec_net_stream_mark (SwfdecGcObject *object)
{
  SwfdecNetStream *stream = SWFDEC_NET_STREAM (object);

  swfdec_gc_object_mark (stream->conn);
  /* no need to handle the channels, the connection manages them */

  SWFDEC_GC_OBJECT_CLASS (swfdec_net_stream_parent_class)->mark (object);
}

static void
swfdec_net_stream_dispose (GObject *object)
{
  SwfdecNetStream *stream = SWFDEC_NET_STREAM (object);

  swfdec_rtmp_channel_unregister (stream->rpc_channel);
  g_object_unref (stream->rpc_channel);
  swfdec_rtmp_channel_unregister (stream->video_channel);
  g_object_unref (stream->video_channel);
  swfdec_rtmp_channel_unregister (stream->audio_channel);
  g_object_unref (stream->audio_channel);

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
swfdec_net_stream_init (SwfdecNetStream *net_stream)
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

SWFDEC_AS_NATIVE (2101, 200, swfdec_net_stream_construct)
void
swfdec_net_stream_construct (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecNetStream *stream;
  SwfdecAsObject *o, *oconn;
  SwfdecRtmpConnection *conn;

  SWFDEC_AS_CHECK (0, NULL, "oo", &o, &oconn);

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
  swfdec_as_object_set_relay (o, SWFDEC_AS_RELAY (stream));
  stream->rpc_channel = swfdec_rtmp_rpc_channel_new (conn);
  swfdec_rtmp_rpc_channel_set_target (SWFDEC_RTMP_RPC_CHANNEL (stream->rpc_channel),
      swfdec_as_relay_get_as_object (SWFDEC_AS_RELAY (stream)));
  stream->video_channel = swfdec_rtmp_video_channel_new (conn);
  /* FIXME: new class for audio plz */
  stream->audio_channel = swfdec_rtmp_rpc_channel_new (conn);
}

SWFDEC_AS_NATIVE (2101, 201, swfdec_net_stream_onCreate)
void
swfdec_net_stream_onCreate (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecNetStream *stream;
  SwfdecAsObject *o;
  guint stream_id, channel_id;

  SWFDEC_AS_CHECK (0, NULL, "oi", &o, &stream_id);

  if (!SWFDEC_IS_NET_STREAM (o->relay))
    return;
  stream = SWFDEC_NET_STREAM (o->relay);

  stream->stream_id = stream_id;
  channel_id = 4 + ((stream_id - 1) * 5);
  swfdec_rtmp_channel_register (stream->rpc_channel, channel_id, stream_id);
  swfdec_rtmp_channel_register (stream->video_channel, channel_id + 1, stream_id);
  swfdec_rtmp_channel_register (stream->audio_channel, channel_id + 2, stream_id);
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

  swfdec_rtmp_rpc_channel_send (SWFDEC_RTMP_RPC_CHANNEL (
	stream->rpc_channel), name,
      ret_cb, MAX (3, argc) - 3, argv + 3);
}

