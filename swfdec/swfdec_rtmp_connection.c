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

#include <string.h>
#include "swfdec_rtmp_connection.h"

/*** SwfdecRtmpConnection ***/

G_DEFINE_TYPE (SwfdecRtmpConnection, swfdec_rtmp_connection, SWFDEC_TYPE_GC_OBJECT)

static void
swfdec_rtmp_connection_dispose (GObject *object)
{
  SwfdecRtmpConnection *conn = SWFDEC_RTMP_CONNECTION (object);

  if (conn->socket) {
    g_object_unref (conn->socket);
    conn->socket = NULL;
  }

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

