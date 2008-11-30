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

#include "swfdec_as_strings.h"
#include "swfdec_bots.h"
#include "swfdec_debug.h"
#include "swfdec_rtmp_control_channel.h"
#include "swfdec_rtmp_handshake_channel.h"
#include "swfdec_rtmp_rpc_channel.h"
#include "swfdec_rtmp_socket.h"

/*** SwfdecRtmpConnection ***/

G_DEFINE_TYPE (SwfdecRtmpConnection, swfdec_rtmp_connection, SWFDEC_TYPE_AS_RELAY)

static void
swfdec_rtmp_connection_mark (SwfdecGcObject *object)
{
  SwfdecRtmpConnection *conn = SWFDEC_RTMP_CONNECTION (object);
  guint i;

  for (i = 0; i < 64; i++) {
    if (conn->channels[i]) {
      SwfdecRtmpChannelClass *klass = SWFDEC_RTMP_CHANNEL_GET_CLASS (conn->channels[i]);
      if (klass->mark)
	klass->mark (conn->channels[i]);
    }
  }

  SWFDEC_GC_OBJECT_CLASS (swfdec_rtmp_connection_parent_class)->mark (object);
}

static void
swfdec_rtmp_connection_dispose (GObject *object)
{
  SwfdecRtmpConnection *conn = SWFDEC_RTMP_CONNECTION (object);

  swfdec_rtmp_connection_close (conn);
  g_assert (conn->socket == NULL);

  g_free (conn->error);
  conn->error = NULL;

  G_OBJECT_CLASS (swfdec_rtmp_connection_parent_class)->dispose (object);
}

static void
swfdec_rtmp_connection_class_init (SwfdecRtmpConnectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_rtmp_connection_dispose;

  gc_class->mark = swfdec_rtmp_connection_mark;
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
    conn->url = swfdec_url_copy (url);
  } else {
    SWFDEC_FIXME ("handle NULL urls in connect()");
  }

  swfdec_rtmp_connection_register_channel (conn, 0, SWFDEC_TYPE_RTMP_HANDSHAKE_CHANNEL);
  swfdec_rtmp_connection_register_channel (conn, 2, SWFDEC_TYPE_RTMP_CONTROL_CHANNEL);
  swfdec_rtmp_connection_register_channel (conn, 3, SWFDEC_TYPE_RTMP_RPC_CHANNEL);
  swfdec_rtmp_handshake_channel_start (SWFDEC_RTMP_HANDSHAKE_CHANNEL (conn->channels[0]));
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
    conn->channels[i] = NULL;
  }

  if (conn->socket) {
    g_object_unref (conn->socket);
    conn->socket = NULL;
  }
  if (conn->url) {
    swfdec_url_free (conn->url);
    conn->url = NULL;
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
  swfdec_as_context_get_time (swfdec_gc_object_get_context (conn), 
      &conn->channels[id]->start_time);

  return conn->channels[id];
}

void
swfdec_rtmp_connection_error (SwfdecRtmpConnection *conn, const char *error, ...)
{
  va_list args;

  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));
  g_return_if_fail (error != NULL);

  va_start (args, error);
  swfdec_rtmp_connection_errorv (conn, error, args);
  va_end (args);
}

void
swfdec_rtmp_connection_errorv (SwfdecRtmpConnection *conn, const char *error, va_list args)
{
  char *real_error;

  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));
  g_return_if_fail (error != NULL);

  real_error = g_strdup_vprintf (error, args);
  if (conn->error) {
    SWFDEC_ERROR ("another error in rtmp socket: %s", real_error);
    g_free (real_error);
    return;
  }

  SWFDEC_ERROR ("error in rtmp socket: %s", real_error);
  conn->error = real_error;
  swfdec_rtmp_connection_close (conn);
}

void
swfdec_rtmp_connection_on_status (SwfdecRtmpConnection *conn, SwfdecAsValue value)
{
  g_return_if_fail (SWFDEC_IS_RTMP_CONNECTION (conn));

  swfdec_as_relay_call (SWFDEC_AS_RELAY (conn), SWFDEC_AS_STR_onStatus, 1, &value, NULL);
}
