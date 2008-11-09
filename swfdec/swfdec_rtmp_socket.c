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

#include "swfdec_rtmp_socket.h"

#include "swfdec_debug.h"
#include "swfdec_player_internal.h"
/* socket implementations for swfdec_rtmp_socket_new() */
#include "swfdec_rtmp_socket_rtmp.h"

/*** SwfdecRtmpSocket ***/

G_DEFINE_TYPE (SwfdecRtmpSocket, swfdec_rtmp_socket, G_TYPE_OBJECT)

static void
swfdec_rtmp_socket_dispose (GObject *object)
{
  SwfdecRtmpSocket *sock = SWFDEC_RTMP_SOCKET (object);

  g_free (sock->error);
  sock->error = NULL;

  G_OBJECT_CLASS (swfdec_rtmp_socket_parent_class)->dispose (object);
}

static void
swfdec_rtmp_socket_class_init (SwfdecRtmpSocketClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_rtmp_socket_dispose;
}

static void
swfdec_rtmp_socket_init (SwfdecRtmpSocket *sock)
{
}

static void
swfdec_rtmp_socket_open (SwfdecRtmpSocket *sock, const char *url)
{
  SwfdecRtmpSocketClass *klass;

  g_return_if_fail (SWFDEC_IS_RTMP_SOCKET (sock));
  g_return_if_fail (url != NULL);

  klass = SWFDEC_RTMP_SOCKET_GET_CLASS (sock);
  klass->open (sock, url);
}

SwfdecRtmpSocket *
swfdec_rtmp_socket_new (SwfdecRtmpConnection *conn, const char *url)
{
  SwfdecRtmpSocket *sock;
  SwfdecPlayer *player;
  SwfdecURL *parse;
  const char *protocol;
  
  g_return_val_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn), NULL);
  g_return_val_if_fail (url != NULL, NULL);

  player = SWFDEC_PLAYER (swfdec_gc_object_get_context (conn));
  parse = swfdec_player_create_url (player, url);
  protocol = swfdec_url_get_protocol (parse);
  if (g_str_equal (protocol, "rtmp")) {
    sock = g_object_new (SWFDEC_TYPE_RTMP_SOCKET_RTMP, NULL);
  } else {
    sock = g_object_new (SWFDEC_TYPE_RTMP_SOCKET, NULL);
  }

  sock->conn = conn;
  if (G_OBJECT_TYPE (sock) == SWFDEC_TYPE_RTMP_SOCKET) {
    swfdec_rtmp_socket_error (sock, "RTMP protocol %s is not supported", protocol);
  } else {
    swfdec_rtmp_socket_open (sock, url);
  }
  swfdec_url_free (parse);
  return sock;
}

void
swfdec_rtmp_socket_receive (SwfdecRtmpSocket *socket, SwfdecBuffer *data)
{
  g_return_if_fail (SWFDEC_IS_RTMP_SOCKET (socket));
  g_return_if_fail (data != NULL);

  SWFDEC_FIXME ("implement");
}

void
swfdec_rtmp_socket_error (SwfdecRtmpSocket *sock, const char *error, ...)
{
  va_list args;

  g_return_if_fail (SWFDEC_IS_RTMP_SOCKET (sock));
  g_return_if_fail (error != NULL);

  va_start (args, error);
  swfdec_rtmp_socket_errorv (sock, error, args);
  va_end (args);
}

void
swfdec_rtmp_socket_errorv (SwfdecRtmpSocket *sock, const char *error, va_list args)
{
  char *real_error;

  g_return_if_fail (SWFDEC_IS_RTMP_SOCKET (sock));
  g_return_if_fail (error != NULL);

  real_error = g_strdup_vprintf (error, args);
  if (sock->error) {
    SWFDEC_ERROR ("another error in rtmp socket: %s", real_error);
    g_free (real_error);
    return;
  }

  SWFDEC_ERROR ("error in rtmp socket: %s", real_error);
  sock->error = real_error;
}

