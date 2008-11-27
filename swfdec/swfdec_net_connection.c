/* Swfdec
 * Copyright (C) 2007-2008 Benjamin Otte <otte@gnome.org>
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

#include "swfdec_amf.h"
#include "swfdec_as_context.h"
#include "swfdec_as_frame_internal.h"
#include "swfdec_as_internal.h"
#include "swfdec_as_object.h"
#include "swfdec_as_strings.h"
#include "swfdec_bots.h"
#include "swfdec_debug.h"
#include "swfdec_internal.h"
#include "swfdec_player_internal.h"
#include "swfdec_rtmp_rpc_channel.h"
#include "swfdec_sandbox.h"

/*** AS CODE ***/

SWFDEC_AS_NATIVE (2100, 0, swfdec_net_connection_do_connect)
void
swfdec_net_connection_do_connect (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecRtmpConnection *conn;
  SwfdecAsValue val;
  SwfdecURL *url;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_RTMP_CONNECTION, &conn, "v", &val);

  if (SWFDEC_AS_VALUE_IS_STRING (val)) {
    url = swfdec_player_create_url (SWFDEC_PLAYER (cx), SWFDEC_AS_VALUE_GET_STRING (val));
  } else if (SWFDEC_AS_VALUE_IS_NULL (val)) {
    url = NULL;
  } else {
    SWFDEC_FIXME ("untested argument to NetConnection.connect: type %u",
	SWFDEC_AS_VALUE_GET_TYPE (val));
    url = NULL;
  }
  swfdec_rtmp_connection_connect (conn, url);
  
  if (url)
    swfdec_url_free (url);
}

SWFDEC_AS_NATIVE (2100, 1, swfdec_net_connection_do_close)
void
swfdec_net_connection_do_close (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetConnection.close");
}

SWFDEC_AS_NATIVE (2100, 2, swfdec_net_connection_do_call)
void
swfdec_net_connection_do_call (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecRtmpConnection *conn;
  SwfdecAsObject *ret_cb = NULL;
  SwfdecBots *bots;
  SwfdecBuffer *buffer;
  SwfdecAsValue name;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_RTMP_CONNECTION, &conn, "v|O", &name, &ret_cb);

  bots = swfdec_bots_new ();
  swfdec_amf_encode (cx, bots, name);

  buffer = swfdec_bots_close (bots);
  swfdec_rtmp_rpc_channel_send (SWFDEC_RTMP_RPC_CHANNEL (
	swfdec_rtmp_connection_get_rpc_channel (conn)), name,
      ret_cb, MAX (2, argc) - 2, argv + 2);
}

SWFDEC_AS_NATIVE (2100, 3, swfdec_net_connection_do_addHeader)
void
swfdec_net_connection_do_addHeader (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetConnection.addHeader");
}

SWFDEC_AS_NATIVE (2100, 4, swfdec_net_connection_get_connectedProxyType)
void
swfdec_net_connection_get_connectedProxyType (SwfdecAsContext *cx,
    SwfdecAsObject *object, guint argc, SwfdecAsValue *argv,
    SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetConnection.connectedProxyType (get)");
}

SWFDEC_AS_NATIVE (2100, 5, swfdec_net_connection_get_usingTLS)
void
swfdec_net_connection_get_usingTLS (SwfdecAsContext *cx,
    SwfdecAsObject *object, guint argc, SwfdecAsValue *argv,
    SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetConnection.usingTLS (get)");
}

SWFDEC_AS_NATIVE (2100, 200, swfdec_net_connection_construct)
void
swfdec_net_connection_construct (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecRtmpConnection *conn;
  SwfdecAsObject *o;

  SWFDEC_AS_CHECK (0, NULL, "o", &o);

  if (!cx->frame->next || !cx->frame->next->construct)
    return;
  if (o->movie) {
    SWFDEC_FIXME ("you managed to call SwfdecNetConnetion's constructor from a movie. Congrats, but what now?");
    return;
  }

  conn = g_object_new (SWFDEC_TYPE_RTMP_CONNECTION, "context", cx, NULL);
  conn->sandbox = swfdec_sandbox_get (SWFDEC_PLAYER (cx));
  swfdec_as_object_set_relay (o, SWFDEC_AS_RELAY (conn));
}
