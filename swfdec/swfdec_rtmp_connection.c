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

#include "swfdec_rtmp_connection.h"

#include <string.h>

#include "swfdec_debug.h"
#include "swfdec_rtmp_socket.h"

/*** SwfdecRtmpConnection ***/

G_DEFINE_TYPE (SwfdecRtmpConnection, swfdec_rtmp_connection, SWFDEC_TYPE_AS_RELAY)

static void
swfdec_rtmp_connection_dispose (GObject *object)
{
  SwfdecRtmpConnection *conn = SWFDEC_RTMP_CONNECTION (object);

  swfdec_rtmp_connection_close (conn);
  g_assert (conn->socket == NULL);

  G_OBJECT_CLASS (swfdec_rtmp_connection_parent_class)->dispose (object);
}

static void
swfdec_rtmp_connection_class_init (SwfdecRtmpConnectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_rtmp_connection_dispose;
}

static void
swfdec_rtmp_connection_init (SwfdecRtmpConnection *rtmp_connection)
{
}

void
swfdec_rtmp_connection_connect (SwfdecRtmpConnection *conn, const char *url)
{
  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));

  swfdec_rtmp_connection_close (conn);

  if (url) {
    conn->socket = swfdec_rtmp_socket_new (conn, url);
  } else {
    SWFDEC_FIXME ("handle NULL urls in connect()");
  }
}

void
swfdec_rtmp_connection_close (SwfdecRtmpConnection *conn)
{
  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));

  if (conn->socket) {
    g_object_unref (conn->socket);
    conn->socket = NULL;
  }
}

