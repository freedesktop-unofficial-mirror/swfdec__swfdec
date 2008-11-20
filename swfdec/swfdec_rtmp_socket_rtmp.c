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

static SwfdecBuffer *
swfdec_rtmp_create_initial_buffer (SwfdecAsContext *context)
{
  SwfdecBots *bots;
  GTimeVal tv;
  guint i, x;
  
  swfdec_as_context_get_time (context, &tv);
  /* we assume here that swfdec_as_context_get_time always returns a tv > start_time */
  x = tv.tv_sec - context->start_time.tv_sec;
  x *= 1000;
  x += (tv.tv_usec - context->start_time.tv_usec) / 1000;

  bots = swfdec_bots_new ();
  swfdec_bots_prepare_bytes (bots, 1 + 1536);
  swfdec_bots_put_u8 (bots, 3);
  swfdec_bots_put_bu32 (bots, x);
  swfdec_bots_put_bu32 (bots, 0);
  for (i = 0; i < 1528 / 2; i++) {
    x = (x * 0xB8CD75 + 1) & 0xFF;
    swfdec_bots_put_bu16 (bots, x);
  }
  g_assert (swfdec_bots_get_bytes (bots) == 1537);
  return swfdec_bots_close (bots);
}

static void
swfdec_rtmp_socket_rtmp_stream_target_open (SwfdecStreamTarget *target, SwfdecStream *stream)
{
  SwfdecRtmpSocketRtmp *rtmp = SWFDEC_RTMP_SOCKET_RTMP (target);
  SwfdecBuffer *send;

  send = swfdec_rtmp_create_initial_buffer (
      swfdec_gc_object_get_context (SWFDEC_RTMP_SOCKET (rtmp)->conn));
  rtmp->ping = g_slist_prepend (rtmp->ping, swfdec_buffer_new_subbuffer (send, 1, 1536));
  swfdec_socket_send (SWFDEC_SOCKET (stream), send);
}

static gboolean
swfdec_rtmp_socket_rtmp_stream_target_parse (SwfdecStreamTarget *target, SwfdecStream *stream)
{
  SwfdecRtmpSocketRtmp *rtmp = SWFDEC_RTMP_SOCKET_RTMP (target);
  SwfdecBufferQueue *queue = swfdec_stream_get_queue (stream);

  if (rtmp->ping) {
    if (g_slist_find (rtmp->ping, NULL)) {
      /* first ever reply */
      SwfdecBuffer *send, *test, *compare;
      guint first_byte;
      if (swfdec_buffer_queue_get_depth (queue) < 1536 * 2 + 1)
	return FALSE;
      compare = swfdec_buffer_queue_pull (queue, 1);
      first_byte = compare->data[0];
      swfdec_buffer_unref (compare);
      send = swfdec_buffer_queue_pull (queue, 1536);
      compare = swfdec_buffer_queue_pull (queue, 1536);
      test = rtmp->ping->data;
      rtmp->ping = g_slist_remove (rtmp->ping, test);
      if (first_byte != 3 || memcmp (test->data, compare->data, 1536) != 0) {
	swfdec_rtmp_socket_error (SWFDEC_RTMP_SOCKET (rtmp),
	    "handshake data is wrong, closing connection");
	return TRUE;
      }
      swfdec_buffer_unref (test);
      swfdec_buffer_unref (compare);
      g_assert (rtmp->ping->data == NULL);
      rtmp->ping = g_slist_remove (rtmp->ping, NULL);
      /* send back 1536 bytes buffer */
      swfdec_socket_send (rtmp->socket, send);
      /* send connect command */
      send = rtmp->ping->data;
      rtmp->ping = g_slist_remove (rtmp->ping, send);
      swfdec_socket_send (rtmp->socket, send);
    } else {
      /* reply to connect command */
      GSList *walk;
      for (walk = rtmp->ping; walk; walk = walk->next) {
	SwfdecBuffer *send = walk->data;
	swfdec_socket_send (rtmp->socket, send);
      }
      g_slist_free (rtmp->ping);
      rtmp->ping = NULL;
    }
  }

  swfdec_rtmp_connection_receive (SWFDEC_RTMP_SOCKET (rtmp)->conn, queue);
  return TRUE;
}

static void
swfdec_rtmp_socket_rtmp_ensure_closed (SwfdecRtmpSocketRtmp *rtmp)
{
  if (rtmp->socket == NULL)
    return;

  swfdec_stream_ensure_closed (SWFDEC_STREAM (rtmp->socket));
  swfdec_stream_set_target (SWFDEC_STREAM (rtmp->socket), NULL);
  g_object_unref (rtmp->socket);
  rtmp->socket = NULL;

  if (rtmp->url) {
    swfdec_url_free (rtmp->url);
    rtmp->url = NULL;
  }
  if (rtmp->ping) {
    GSList *walk;
    for (walk = rtmp->ping; walk; walk = walk->next) {
      if (walk->data)
	swfdec_buffer_unref (walk->data);
    }
    g_slist_free (rtmp->ping);
    rtmp->ping = NULL;
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
  rtmp->socket = swfdec_player_create_socket (player, 
      swfdec_url_get_host (rtmp->url) ? swfdec_url_get_host (rtmp->url) : "localhost",
      swfdec_url_get_port (rtmp->url) ? swfdec_url_get_port (rtmp->url) : 1935);
  swfdec_stream_set_target (SWFDEC_STREAM (rtmp->socket), SWFDEC_STREAM_TARGET (rtmp));
  rtmp->ping = g_slist_prepend (rtmp->ping, NULL); /* magic value */
}

static void
swfdec_rtmp_socket_rtmp_close (SwfdecRtmpSocket *sock)
{
  SwfdecRtmpSocketRtmp *rtmp = SWFDEC_RTMP_SOCKET_RTMP (sock);

  swfdec_rtmp_socket_rtmp_ensure_closed (rtmp);
}

static void
swfdec_rtmp_socket_rtmp_send (SwfdecRtmpSocket *sock, SwfdecBuffer *data)
{
  SwfdecRtmpSocketRtmp *rtmp = SWFDEC_RTMP_SOCKET_RTMP (sock);

  if (rtmp->ping) {
    rtmp->ping = g_slist_append (rtmp->ping, data);
  } else {
    swfdec_socket_send (rtmp->socket, data);
  }
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

