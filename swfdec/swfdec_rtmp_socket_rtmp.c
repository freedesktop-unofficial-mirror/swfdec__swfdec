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

#include <string.h>

#include "swfdec_bots.h"
#include "swfdec_debug.h"
#include "swfdec_loader_internal.h"
#include "swfdec_player_internal.h"
#include "swfdec_stream_target.h"

/*** SwfdecStreamTarget ***/

static SwfdecPlayer *
swfdec_rtmp_socket_rtmp_stream_target_get_player (SwfdecStreamTarget *target)
{
  SwfdecRtmpSocket *socket = SWFDEC_RTMP_SOCKET (target);

  return SWFDEC_PLAYER (swfdec_gc_object_get_context (socket->conn));
}

static void
swfdec_rtmp_socket_rtmp_do_send (SwfdecRtmpSocketRtmp *rtmp)
{
  while (rtmp->next != NULL) {
    gsize written = swfdec_socket_send (SWFDEC_SOCKET (rtmp->socket), rtmp->next);
    if (written == rtmp->next->length) {
      swfdec_buffer_unref (rtmp->next);
      rtmp->next = swfdec_rtmp_socket_next_buffer (SWFDEC_RTMP_SOCKET (rtmp));
    } else {
      SwfdecBuffer *buffer = swfdec_buffer_new_subbuffer (rtmp->next,
	  written, rtmp->next->length - written);
      swfdec_buffer_unref (rtmp->next);
      rtmp->next = buffer;
      break;
    }
  }
}

static void
swfdec_rtmp_socket_rtmp_stream_target_open (SwfdecStreamTarget *target, SwfdecStream *stream)
{
  SwfdecRtmpSocketRtmp *rtmp = SWFDEC_RTMP_SOCKET_RTMP (target);
  SwfdecRtmpSocket *sock = SWFDEC_RTMP_SOCKET (target);

  swfdec_rtmp_connection_set_connected (sock->conn, 
      swfdec_as_context_get_string (swfdec_gc_object_get_context (sock->conn),
	swfdec_url_get_url (sock->conn->url)));
  rtmp->next = swfdec_rtmp_socket_next_buffer (sock);
  swfdec_rtmp_socket_rtmp_do_send (rtmp);
}

static gboolean
swfdec_rtmp_socket_rtmp_stream_target_parse (SwfdecStreamTarget *target, SwfdecStream *stream)
{
  swfdec_rtmp_socket_receive (SWFDEC_RTMP_SOCKET (target), 
      swfdec_stream_get_queue (stream));
  return FALSE;
}

static void
swfdec_rtmp_socket_rtmp_ensure_closed (SwfdecRtmpSocketRtmp *rtmp)
{
  if (rtmp->socket == NULL)
    return;

  swfdec_stream_ensure_closed (rtmp->socket);
  swfdec_stream_set_target (rtmp->socket, NULL);
  g_object_unref (rtmp->socket);
  rtmp->socket = NULL;

  if (rtmp->url) {
    swfdec_url_free (rtmp->url);
    rtmp->url = NULL;
  }
  if (rtmp->next) {
    swfdec_buffer_unref (rtmp->next);
    rtmp->next = NULL;
  }
}

static void
swfdec_rtmp_socket_rtmp_stream_target_close (SwfdecStreamTarget *target, SwfdecStream *stream)
{
  SWFDEC_FIXME ("anything to do now?");
  swfdec_rtmp_socket_rtmp_ensure_closed (SWFDEC_RTMP_SOCKET_RTMP (target));
}

static void
swfdec_rtmp_socket_rtmp_stream_target_error (SwfdecStreamTarget *target, SwfdecStream *stream)
{
  swfdec_rtmp_connection_error (SWFDEC_RTMP_SOCKET (target)->conn,
      "error from socket used by RTMP socket");
}

static void
swfdec_rtmp_socket_rtmp_stream_target_writable (SwfdecStreamTarget *target, SwfdecStream *stream)
{
  SwfdecRtmpSocketRtmp *rtmp = SWFDEC_RTMP_SOCKET_RTMP (target);

  g_assert (rtmp->next);
  swfdec_rtmp_socket_rtmp_do_send (rtmp);
}

static void
swfdec_rtmp_socket_rtmp_stream_target_init (SwfdecStreamTargetInterface *iface)
{
  iface->get_player = swfdec_rtmp_socket_rtmp_stream_target_get_player;
  iface->open = swfdec_rtmp_socket_rtmp_stream_target_open;
  iface->parse = swfdec_rtmp_socket_rtmp_stream_target_parse;
  iface->error = swfdec_rtmp_socket_rtmp_stream_target_error;
  iface->close = swfdec_rtmp_socket_rtmp_stream_target_close;
  iface->writable = swfdec_rtmp_socket_rtmp_stream_target_writable;
}

/*** SwfdecRtmpSocketRtmp ***/

G_DEFINE_TYPE_WITH_CODE (SwfdecRtmpSocketRtmp, swfdec_rtmp_socket_rtmp, SWFDEC_TYPE_RTMP_SOCKET,
    G_IMPLEMENT_INTERFACE (SWFDEC_TYPE_STREAM_TARGET, swfdec_rtmp_socket_rtmp_stream_target_init))

static void
swfdec_rtmp_socket_rtmp_dispose (GObject *object)
{
  SwfdecRtmpSocketRtmp *rtmp = SWFDEC_RTMP_SOCKET_RTMP (object);

  swfdec_rtmp_socket_rtmp_ensure_closed (rtmp);

  G_OBJECT_CLASS (swfdec_rtmp_socket_rtmp_parent_class)->dispose (object);
}

static void
swfdec_rtmp_socket_rtmp_open (SwfdecRtmpSocket *sock, const SwfdecURL *url)
{
  SwfdecPlayer *player = SWFDEC_PLAYER (swfdec_gc_object_get_context (sock->conn));
  SwfdecRtmpSocketRtmp *rtmp = SWFDEC_RTMP_SOCKET_RTMP (sock);

  rtmp->url = swfdec_url_copy (url);
  rtmp->socket = SWFDEC_STREAM (swfdec_player_create_socket (player, 
      swfdec_url_get_host (rtmp->url) ? swfdec_url_get_host (rtmp->url) : "localhost",
      swfdec_url_get_port (rtmp->url) ? swfdec_url_get_port (rtmp->url) : 1935));
  swfdec_stream_set_target (rtmp->socket, SWFDEC_STREAM_TARGET (rtmp));
}

static void
swfdec_rtmp_socket_rtmp_close (SwfdecRtmpSocket *sock)
{
  SwfdecRtmpSocketRtmp *rtmp = SWFDEC_RTMP_SOCKET_RTMP (sock);

  swfdec_rtmp_socket_rtmp_ensure_closed (rtmp);
}

static void
swfdec_rtmp_socket_rtmp_send (SwfdecRtmpSocket *sock)
{
  SwfdecRtmpSocketRtmp *rtmp = SWFDEC_RTMP_SOCKET_RTMP (sock);

  /* we're already sending something, the writable callback will trigger */
  if (rtmp->next != NULL)
    return;

  if (!swfdec_stream_is_open (rtmp->socket))
    return;

  rtmp->next = swfdec_rtmp_socket_next_buffer (sock);
  swfdec_rtmp_socket_rtmp_do_send (rtmp);
}

static void
swfdec_rtmp_socket_rtmp_class_init (SwfdecRtmpSocketRtmpClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecRtmpSocketClass *socket_class = SWFDEC_RTMP_SOCKET_CLASS (klass);

  object_class->dispose = swfdec_rtmp_socket_rtmp_dispose;

  socket_class->open = swfdec_rtmp_socket_rtmp_open;
  socket_class->close = swfdec_rtmp_socket_rtmp_close;
  socket_class->send = swfdec_rtmp_socket_rtmp_send;
}

static void
swfdec_rtmp_socket_rtmp_init (SwfdecRtmpSocketRtmp *sock)
{
}

