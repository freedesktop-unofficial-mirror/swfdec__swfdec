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

#include "swfdec_as_internal.h"
#include "swfdec_debug.h"

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
  SWFDEC_STUB ("NetStream.construct (internal)");
}

SWFDEC_AS_NATIVE (2101, 201, swfdec_net_stream_onCreate)
void
swfdec_net_stream_onCreate (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.onCreate (internal)");
}

SWFDEC_AS_NATIVE (2101, 202, swfdec_net_stream_send_connection)
void
swfdec_net_stream_send_connection (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SWFDEC_STUB ("NetStream.send_connection (internal)");
}

