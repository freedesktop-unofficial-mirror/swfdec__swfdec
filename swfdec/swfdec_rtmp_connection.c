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

#include "swfdec_bots.h"
#include "swfdec_debug.h"
#include "swfdec_rtmp_control_channel.h"
#include "swfdec_rtmp_rpc_channel.h"
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
swfdec_rtmp_connection_connect (SwfdecRtmpConnection *conn, const SwfdecURL *url)
{
  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));

  swfdec_rtmp_connection_close (conn);

  if (url) {
    conn->socket = swfdec_rtmp_socket_new (conn, url);
  } else {
    SWFDEC_FIXME ("handle NULL urls in connect()");
  }

  swfdec_rtmp_connection_register_channel (conn, 2, SWFDEC_TYPE_RTMP_CONTROL_CHANNEL);
  swfdec_rtmp_connection_register_channel (conn, 3, SWFDEC_TYPE_RTMP_RPC_CHANNEL);
}

void
swfdec_rtmp_connection_close (SwfdecRtmpConnection *conn)
{
  guint i;

  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));

  for (i = 0; i < 64; i++) {
    if (conn->channels[i] == NULL)
      continue;
    g_object_unref (conn->channels[i]);
  }

  if (conn->socket) {
    g_object_unref (conn->socket);
    conn->socket = NULL;
  }
}

SwfdecRtmpChannel *
swfdec_rtmp_connection_register_channel	(SwfdecRtmpConnection *conn, int id,
    GType channel_type)
{
  g_return_val_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn), NULL);
  g_return_val_if_fail (id >= -1 && id < 64, NULL);
  g_return_val_if_fail (g_type_is_a (channel_type, SWFDEC_TYPE_RTMP_CHANNEL), NULL);

  if (id < 0) {
    /* FIXME: do we give out channels 0 and 1? */
    /* we can start at 4, because 2 and 3 are reserved */
    for (id = 4; id < 64; id++) {
      if (conn->channels[id] == NULL)
	break;
    }
    if (id == 64) {
      SWFDEC_ERROR ("all channels in use, what now?");
      return NULL;
    }
  }

  conn->channels[id] = g_object_new (channel_type, NULL);
  conn->channels[id]->conn = conn;
  conn->channels[id]->id = id;

  return conn->channels[id];
}

void
swfdec_rtmp_connection_receive (SwfdecRtmpConnection *conn, SwfdecBufferQueue *queue)
{
  SwfdecBuffer *buffer;
  SwfdecBits bits;
  SwfdecRtmpHeaderSize header_size;
  guint channel;

  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));
  g_return_if_fail (queue != NULL);

  do {
    buffer = swfdec_buffer_queue_peek (queue, 1);
    if (buffer == NULL)
      break;
    swfdec_bits_init (&bits, buffer);
    header_size = swfdec_bits_getbits (&bits, 2);
    channel = swfdec_bits_getbits (&bits, 6);
    swfdec_buffer_unref (buffer);
    if (conn->channels[channel] == NULL) {
      SWFDEC_FIXME ("message on unknown channel %u, what now?", channel);
      break;
    }
  } while (swfdec_rtmp_channel_receive (conn->channels[channel], queue, header_size));
}

