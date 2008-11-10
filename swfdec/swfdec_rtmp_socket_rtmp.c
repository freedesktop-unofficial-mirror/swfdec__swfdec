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

#include "swfdec_rtmp_socket_rtmp.h"

#include "swfdec_debug.h"
#include "swfdec_loader_internal.h"
#include "swfdec_stream_target.h"

/*** SwfdecStreamTarget ***/

static SwfdecPlayer *
swfdec_rtmp_socket_rtmp_stream_target_get_player (SwfdecStreamTarget *target)
{
  SwfdecRtmpSocket *socket = SWFDEC_RTMP_SOCKET (target);

  return SWFDEC_PLAYER (swfdec_gc_object_get_context (socket->conn));
}

static void
swfdec_rtmp_socket_rtmp_stream_target_open (SwfdecStreamTarget *target, SwfdecStream *stream)
{
  SwfdecRtmpSocketRtmp *rtmp = SWFDEC_RTMP_SOCKET_RTMP (target);
  SwfdecBuffer *send;

  send = swfdec_buffer_new (1 + 1536);
  send->data[0] = 3;
  rtmp->ping = swfdec_buffer_ref (send);
  swfdec_socket_send (SWFDEC_SOCKET (stream), send);
}

static gboolean
swfdec_rtmp_socket_rtmp_stream_target_parse (SwfdecStreamTarget *target, SwfdecStream *stream)
{
  SwfdecRtmpSocketRtmp *rtmp = SWFDEC_RTMP_SOCKET_RTMP (target);
  SwfdecBufferQueue *queue = swfdec_stream_get_queue (stream);

  if (rtmp->ping) {
    if (swfdec_buffer_queue_get_depth (queue) < 1536 * 2)
      return FALSE;
    g_assert_not_reached ();
  }
  return TRUE;
}

static void
swfdec_rtmp_socket_rtmp_stream_target_close (SwfdecStreamTarget *target, SwfdecStream *stream)
{
  //SwfdecRtmpSocket *sock = SWFDEC_RTMP_SOCKET (target);

  SWFDEC_FIXME ("anything to do now?");
}

static void
swfdec_rtmp_socket_rtmp_stream_target_error (SwfdecStreamTarget *target, SwfdecStream *stream)
{
  SwfdecRtmpSocket *sock = SWFDEC_RTMP_SOCKET (target);

  swfdec_rtmp_socket_error (sock, "error from socket used by RTMP socket");
}

static void
swfdec_rtmp_socket_rtmp_stream_target_init (SwfdecStreamTargetInterface *iface)
{
  iface->get_player = swfdec_rtmp_socket_rtmp_stream_target_get_player;
  iface->open = swfdec_rtmp_socket_rtmp_stream_target_open;
  iface->parse = swfdec_rtmp_socket_rtmp_stream_target_parse;
  iface->error = swfdec_rtmp_socket_rtmp_stream_target_error;
  iface->close = swfdec_rtmp_socket_rtmp_stream_target_close;
}

/*** SwfdecRtmpSocketRtmp ***/

G_DEFINE_TYPE_WITH_CODE (SwfdecRtmpSocketRtmp, swfdec_rtmp_socket_rtmp, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (SWFDEC_TYPE_STREAM_TARGET, swfdec_rtmp_socket_rtmp_stream_target_init))

static void
swfdec_rtmp_socket_rtmp_dispose (GObject *object)
{
  //SwfdecRtmpSocketRtmp *sock = SWFDEC_RTMP_SOCKET_RTMP (object);

  G_OBJECT_CLASS (swfdec_rtmp_socket_rtmp_parent_class)->dispose (object);
}

static void
swfdec_rtmp_socket_rtmp_class_init (SwfdecRtmpSocketRtmpClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_rtmp_socket_rtmp_dispose;
}

static void
swfdec_rtmp_socket_rtmp_init (SwfdecRtmpSocketRtmp *sock)
{
}

