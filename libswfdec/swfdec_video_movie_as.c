/* Swfdec
 * Copyright (C) 2007 Benjamin Otte <otte@gnome.org>
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

#include "swfdec_video.h"
#include "swfdec_as_strings.h"
#include "swfdec_debug.h"
#include "swfdec_internal.h"
#include "swfdec_net_stream.h"
#include "swfdec_player_internal.h"

static void
swfdec_video_attach_video (SwfdecAsContext *cx, SwfdecAsObject *obj, guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecVideoMovie *video = SWFDEC_VIDEO_MOVIE (obj);
  SwfdecNetStream *stream;

  if (!SWFDEC_AS_VALUE_IS_OBJECT (&argv[0]) ||
      !SWFDEC_IS_NET_STREAM (stream = (SwfdecNetStream *) SWFDEC_AS_VALUE_GET_OBJECT (&argv[0]))) {
    SWFDEC_WARNING ("calling attachVideo without a NetStream object");
    swfdec_video_movie_set_input (video, NULL);
    return;
  }
  swfdec_video_movie_set_input (video, &stream->input);
}

static void
swfdec_video_clear (SwfdecAsContext *cx, SwfdecAsObject *obj, guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecVideoMovie *video = SWFDEC_VIDEO_MOVIE (obj);

  swfdec_video_movie_clear (video);
}

void
swfdec_video_movie_init_context (SwfdecPlayer *player, guint version)
{
  SwfdecAsContext *context;
  SwfdecAsObject *video, *proto;
  SwfdecAsValue val;

  g_return_if_fail (SWFDEC_IS_PLAYER (player));

  context = SWFDEC_AS_CONTEXT (player);
  video = SWFDEC_AS_OBJECT (swfdec_as_object_add_function (context->global, 
      SWFDEC_AS_STR_Video, 0, NULL, 0));
  if (video == NULL)
    return;
  player->Video = video;
  proto = swfdec_as_object_new_empty (context);
  if (proto == NULL)
    return;
  /* set the right properties on the Video object */
  SWFDEC_AS_VALUE_SET_OBJECT (&val, proto);
  swfdec_as_object_set_variable_and_flags (video, SWFDEC_AS_STR_prototype, &val,
      SWFDEC_AS_VARIABLE_HIDDEN | SWFDEC_AS_VARIABLE_PERMANENT);
  /* set the right properties on the Video.prototype object */
  if (version >= 6) {
    swfdec_as_object_add_function (proto, SWFDEC_AS_STR_attachVideo,
	SWFDEC_TYPE_VIDEO_MOVIE, swfdec_video_attach_video, 1);
    swfdec_as_object_add_function (proto, SWFDEC_AS_STR_clear,
	SWFDEC_TYPE_VIDEO_MOVIE, swfdec_video_clear, 0);
  }
  SWFDEC_AS_VALUE_SET_OBJECT (&val, video);
  swfdec_as_object_set_variable_and_flags (proto, SWFDEC_AS_STR_constructor,
      &val, SWFDEC_AS_VARIABLE_HIDDEN | SWFDEC_AS_VARIABLE_PERMANENT);
  SWFDEC_AS_VALUE_SET_OBJECT (&val, context->Object_prototype);
  swfdec_as_object_set_variable_and_flags (proto, SWFDEC_AS_STR___proto__, &val,
      SWFDEC_AS_VARIABLE_HIDDEN | SWFDEC_AS_VARIABLE_PERMANENT);
}

