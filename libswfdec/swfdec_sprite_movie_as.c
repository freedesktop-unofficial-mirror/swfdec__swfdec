/* Swfdec
 * Copyright (C) 2003-2006 David Schleef <ds@schleef.org>
 *		 2005-2006 Eric Anholt <eric@anholt.net>
 *		 2006-2007 Benjamin Otte <otte@gnome.org>
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

#include "swfdec_movie.h"
#include "swfdec_as_internal.h"
#include "swfdec_as_strings.h"
#include "swfdec_bits.h"
#include "swfdec_debug.h"
#include "swfdec_decoder.h"
#include "swfdec_internal.h"
#include "swfdec_player_internal.h"
#include "swfdec_sprite.h"
#include "swfdec_sprite_movie.h"
#include "swfdec_swf_decoder.h"
#include "swfdec_resource.h"
#include "swfdec_as_internal.h"

SWFDEC_AS_NATIVE (900, 12, swfdec_sprite_movie_play)
void
swfdec_sprite_movie_play (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecSpriteMovie *movie;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_SPRITE_MOVIE, (gpointer)&movie, "");

  movie->playing = TRUE;
}

SWFDEC_AS_NATIVE (900, 13, swfdec_sprite_movie_stop)
void
swfdec_sprite_movie_stop (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecSpriteMovie *movie;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_SPRITE_MOVIE, (gpointer)&movie, "");

  movie->playing = FALSE;
}

SWFDEC_AS_NATIVE (900, 7, swfdec_sprite_movie_getBytesLoaded)
void
swfdec_sprite_movie_getBytesLoaded (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecMovie *movie;
  SwfdecResource *resource;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_MOVIE, (gpointer)&movie, "");

  resource = swfdec_movie_get_own_resource (movie);
  if (resource) {
    SWFDEC_AS_VALUE_SET_INT (rval, resource->decoder->bytes_loaded);
  } else {
    SWFDEC_AS_VALUE_SET_INT (rval, 0);
  }
}

SWFDEC_AS_NATIVE (900, 6, swfdec_sprite_movie_getBytesTotal)
void
swfdec_sprite_movie_getBytesTotal (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecMovie *movie;
  SwfdecResource *resource;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_MOVIE, (gpointer)&movie, "");

  resource = swfdec_movie_get_own_resource (movie);
  if (resource) {
    SWFDEC_AS_VALUE_SET_INT (rval, resource->decoder->bytes_total);
  } else {
    SWFDEC_AS_VALUE_SET_INT (rval, 0);
  }
}

// No ASnative number
static void
swfdec_sprite_movie_getNextHighestDepth (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecMovie *movie;
  int depth;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_MOVIE, (gpointer)&movie, "");

  if (movie->list) {
    depth = SWFDEC_MOVIE (g_list_last (movie->list)->data)->depth + 1;
    if (depth < 0)
      depth = 0;
  } else {
    depth = 0;
  }
  SWFDEC_AS_VALUE_SET_INT (rval, depth);
}

static void
swfdec_sprite_movie_do_goto (SwfdecSpriteMovie *movie, SwfdecAsValue *target)
{
  int frame;

  g_return_if_fail (SWFDEC_IS_SPRITE_MOVIE (movie));
  g_return_if_fail (SWFDEC_IS_AS_VALUE (target));

  if (SWFDEC_AS_VALUE_IS_STRING (target)) {
    const char *label = SWFDEC_AS_VALUE_GET_STRING (target);
    frame = swfdec_sprite_get_frame (movie->sprite, label);
    /* FIXME: nonexisting frames? */
    if (frame == -1)
      return;
    frame++;
  } else {
    frame = swfdec_as_value_to_integer (SWFDEC_AS_OBJECT (movie)->context, target);
  }
  /* FIXME: how to handle overflow? */
  frame = CLAMP (frame, 1, (int) movie->n_frames);

  swfdec_sprite_movie_goto (movie, frame);
}

SWFDEC_AS_NATIVE (900, 16, swfdec_sprite_movie_gotoAndPlay)
void
swfdec_sprite_movie_gotoAndPlay (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecSpriteMovie *movie;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_SPRITE_MOVIE, (gpointer)&movie, "");
  
  swfdec_sprite_movie_do_goto (movie, &argv[0]);
  movie->playing = TRUE;
}

SWFDEC_AS_NATIVE (900, 17, swfdec_sprite_movie_gotoAndStop)
void
swfdec_sprite_movie_gotoAndStop (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecSpriteMovie *movie;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_SPRITE_MOVIE, (gpointer)&movie, "");
  
  swfdec_sprite_movie_do_goto (movie, &argv[0]);
  movie->playing = FALSE;
}

SWFDEC_AS_NATIVE (900, 14, swfdec_sprite_movie_nextFrame)
void
swfdec_sprite_movie_nextFrame (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecSpriteMovie *movie;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_SPRITE_MOVIE, (gpointer)&movie, "");
  
  swfdec_sprite_movie_goto (movie, movie->frame + 1);
  movie->playing = FALSE;
}

SWFDEC_AS_NATIVE (900, 15, swfdec_sprite_movie_prevFrame)
void
swfdec_sprite_movie_prevFrame (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecSpriteMovie *movie;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_SPRITE_MOVIE, (gpointer)&movie, "");
  
  swfdec_sprite_movie_goto (movie, movie->frame - 1);
  movie->playing = FALSE;
}

SWFDEC_AS_NATIVE (900, 4, swfdec_sprite_movie_hitTest)
void
swfdec_sprite_movie_hitTest (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecMovie *movie;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_MOVIE, (gpointer)&movie, "");
  
  if (argc == 1) {
    SwfdecMovie *other;
    SwfdecRect movie_rect, other_rect;

    other = swfdec_player_get_movie_from_value (SWFDEC_PLAYER (cx), &argv[0]);
    if (other == NULL) {
      SWFDEC_AS_VALUE_SET_BOOLEAN (rval, FALSE);
      return;
    }
    swfdec_movie_update (movie);
    swfdec_movie_update (other);
    movie_rect = movie->extents;
    if (movie->parent)
      swfdec_movie_rect_local_to_global (movie->parent, &movie_rect);
    other_rect = other->extents;
    if (other->parent)
      swfdec_movie_rect_local_to_global (other->parent, &other_rect);
    SWFDEC_AS_VALUE_SET_BOOLEAN (rval, swfdec_rect_intersect (NULL, &movie_rect, &other_rect));
  } else if (argc >= 2) {
    double x, y;
    gboolean shape, ret;

    x = swfdec_as_value_to_number (cx, &argv[0]) * SWFDEC_TWIPS_SCALE_FACTOR;
    y = swfdec_as_value_to_number (cx, &argv[1]) * SWFDEC_TWIPS_SCALE_FACTOR;
    shape = (argc >= 3 && swfdec_as_value_to_boolean (cx, &argv[2]));

    swfdec_movie_global_to_local (movie, &x, &y);

    if (shape) {
      ret = swfdec_movie_mouse_in (movie, x, y);
    } else {
      ret = swfdec_rect_contains (&movie->original_extents, x, y);
    }
    SWFDEC_AS_VALUE_SET_BOOLEAN (rval, ret);
  }
}

SWFDEC_AS_NATIVE (900, 20, swfdec_sprite_movie_startDrag)
void
swfdec_sprite_movie_startDrag (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecMovie *movie;
  SwfdecPlayer *player = SWFDEC_PLAYER (cx);
  gboolean center = FALSE;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_MOVIE, (gpointer)&movie, "");

  if (argc > 0) {
    center = swfdec_as_value_to_boolean (cx, &argv[0]);
  }
  if (argc >= 5) {
    SwfdecRect rect;
    rect.x0 = swfdec_as_value_to_number (cx, &argv[1]);
    rect.y0 = swfdec_as_value_to_number (cx, &argv[2]);
    rect.x1 = swfdec_as_value_to_number (cx, &argv[3]);
    rect.y1 = swfdec_as_value_to_number (cx, &argv[4]);
    swfdec_rect_scale (&rect, &rect, SWFDEC_TWIPS_SCALE_FACTOR);
    swfdec_player_set_drag_movie (player, movie, center, &rect);
  } else {
    swfdec_player_set_drag_movie (player, movie, center, NULL);
  }
}

SWFDEC_AS_NATIVE (900, 21, swfdec_sprite_movie_stopDrag)
void
swfdec_sprite_movie_stopDrag (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  // FIXME: Should this work when called on non-movie objects or not?

  swfdec_player_set_drag_movie (SWFDEC_PLAYER (cx), NULL, FALSE, NULL);
}

SWFDEC_AS_NATIVE (900, 1, swfdec_sprite_movie_swapDepths)
void
swfdec_sprite_movie_swapDepths (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecMovie *movie;
  SwfdecMovie *other;
  int depth;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_MOVIE, (gpointer)&movie, "");

  if (SWFDEC_AS_VALUE_IS_OBJECT (&argv[0])) {
    other = (SwfdecMovie *) SWFDEC_AS_VALUE_GET_OBJECT (&argv[0]);
    if (!SWFDEC_IS_MOVIE (other) ||
	other->parent != movie->parent)
      return;
    depth = other->depth;
  } else {
    depth = swfdec_as_value_to_integer (cx, &argv[0]);
    other = swfdec_movie_find (movie->parent, depth);
  }
  if (other)
    swfdec_movie_set_depth (other, movie->depth);
  swfdec_movie_set_depth (movie, depth);
}

SWFDEC_AS_NATIVE (901, 0, swfdec_sprite_movie_createEmptyMovieClip)
void
swfdec_sprite_movie_createEmptyMovieClip (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecMovie *movie, *parent;
  int depth;
  const char *name;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_MOVIE, &parent, "si", &name, &depth);

  movie = swfdec_movie_find (parent, depth);
  if (movie)
    swfdec_movie_remove (movie);
  movie = swfdec_movie_new (SWFDEC_PLAYER (cx), depth, parent, parent->resource, NULL, name);
  swfdec_movie_initialize (movie);
  SWFDEC_AS_VALUE_SET_OBJECT (rval, SWFDEC_AS_OBJECT (movie));
}

static void
swfdec_sprite_movie_copy_props (SwfdecMovie *target, SwfdecMovie *src)
{
  target->matrix = src->matrix;
  target->color_transform = src->color_transform;
  swfdec_movie_queue_update (target, SWFDEC_MOVIE_INVALID_MATRIX);
}

static gboolean
swfdec_sprite_movie_foreach_copy_properties (SwfdecAsObject *object,
    const char *variable, SwfdecAsValue *value, guint flags, gpointer data)
{
  SwfdecAsObject *target = data;

  g_return_val_if_fail (SWFDEC_IS_AS_OBJECT (target), FALSE);

  swfdec_as_object_set_variable (target, variable, value);

  return TRUE;
}

static void
swfdec_sprite_movie_init_from_object (SwfdecMovie *movie,
    SwfdecAsObject *initObject)
{
  g_return_if_fail (SWFDEC_IS_MOVIE (movie));
  g_return_if_fail (initObject == NULL || SWFDEC_IS_AS_OBJECT (initObject));

  if (initObject != NULL) {
    swfdec_as_object_foreach (initObject,
	swfdec_sprite_movie_foreach_copy_properties, SWFDEC_AS_OBJECT (movie));
  }

  swfdec_movie_initialize (movie);
}

SWFDEC_AS_NATIVE (900, 0, swfdec_sprite_movie_attachMovie)
void
swfdec_sprite_movie_attachMovie (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecMovie *movie;
  SwfdecMovie *ret;
  SwfdecAsObject *initObject;
  const char *name, *export;
  int depth;
  SwfdecGraphic *sprite;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_MOVIE, (gpointer)&movie, "ss", &export, &name);

  if (argc > 3 && SWFDEC_AS_VALUE_IS_OBJECT (&argv[3])) {
    initObject = SWFDEC_AS_VALUE_GET_OBJECT ((&argv[3]));
  } else {
    initObject = NULL;
  }
  sprite = swfdec_resource_get_export (movie->resource, export);
  if (!SWFDEC_IS_SPRITE (sprite)) {
    if (sprite == NULL) {
      SWFDEC_WARNING ("no symbol with name %s exported", export);
    } else {
      SWFDEC_WARNING ("can only use attachMovie with sprites");
    }
    return;
  }
  depth = swfdec_as_value_to_integer (cx, &argv[2]);
  if (swfdec_depth_classify (depth) == SWFDEC_DEPTH_CLASS_EMPTY)
    return;
  ret = swfdec_movie_find (movie, depth);
  if (ret)
    swfdec_movie_remove (ret);
  ret = swfdec_movie_new (SWFDEC_PLAYER (object->context), depth, movie, movie->resource, sprite, name);
  SWFDEC_LOG ("attached %s (%u) as %s to depth %u", export, SWFDEC_CHARACTER (sprite)->id,
      ret->name, ret->depth);
  /* run init and construct */
  swfdec_sprite_movie_init_from_object (ret, initObject);
  SWFDEC_AS_VALUE_SET_OBJECT (rval, SWFDEC_AS_OBJECT (ret));
}

SWFDEC_AS_NATIVE (900, 18, swfdec_sprite_movie_duplicateMovieClip)
void
swfdec_sprite_movie_duplicateMovieClip (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecMovie *movie;
  SwfdecMovie *new;
  const char *name;
  int depth;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_MOVIE, (gpointer)&movie, "si", &name, &depth);

  if (swfdec_depth_classify (depth) == SWFDEC_DEPTH_CLASS_EMPTY)
    return;
  new = swfdec_movie_duplicate (movie, name, depth);
  if (new == NULL)
    return;
  swfdec_sprite_movie_copy_props (new, movie);
  if (SWFDEC_IS_SPRITE_MOVIE (new)) {
    g_queue_push_tail (SWFDEC_PLAYER (cx)->init_queue, new);
    swfdec_movie_queue_script (new, SWFDEC_EVENT_LOAD);
    swfdec_movie_run_construct (new);
  }
  swfdec_movie_initialize (new);
  SWFDEC_LOG ("duplicated %s as %s to depth %u", movie->name, new->name, new->depth);
  SWFDEC_AS_VALUE_SET_OBJECT (rval, SWFDEC_AS_OBJECT (new));
}

SWFDEC_AS_NATIVE (900, 19, swfdec_sprite_movie_removeMovieClip)
void
swfdec_sprite_movie_removeMovieClip (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecMovie *movie;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_MOVIE, (gpointer)&movie, "");

  if (swfdec_depth_classify (movie->depth) == SWFDEC_DEPTH_CLASS_DYNAMIC)
    swfdec_movie_remove (movie);
}

SWFDEC_AS_NATIVE (900, 10, swfdec_sprite_movie_getDepth)
void
swfdec_sprite_movie_getDepth (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  SwfdecMovie *movie;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_MOVIE, (gpointer)&movie, "");

  SWFDEC_AS_VALUE_SET_INT (rval, movie->depth);
}

SWFDEC_AS_NATIVE (900, 5, swfdec_sprite_movie_getBounds)
void
swfdec_sprite_movie_getBounds (SwfdecAsContext *cx, SwfdecAsObject *object,
        guint argc, SwfdecAsValue *argv, SwfdecAsValue *rval)
{
  int x0, x1, y0, y1;
  SwfdecAsValue val;
  SwfdecAsObject *obj;
  SwfdecMovie *movie;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_MOVIE, (gpointer)&movie, "");

  obj= swfdec_as_object_new_empty (cx);
  if (obj== NULL)
    return;

  swfdec_movie_update (movie);
  if (swfdec_rect_is_empty (&movie->extents)) {
    x0 = x1 = y0 = y1 = 0x7FFFFFF;
  } else {
    SwfdecRect rect = movie->extents;
    if (argc > 0) {
      SwfdecMovie *other = swfdec_player_get_movie_from_value (
	  SWFDEC_PLAYER (cx), &argv[0]);
      if (other) {
	if (movie->parent)
	  swfdec_movie_rect_local_to_global (movie->parent, &rect);
	swfdec_movie_rect_global_to_local (other, &rect);
      } else {
	SWFDEC_FIXME ("what's getBounds relative to invalid?");
      }
    }
    x0 = rect.x0;
    y0 = rect.y0;
    x1 = rect.x1;
    y1 = rect.y1;
  }
  SWFDEC_AS_VALUE_SET_NUMBER (&val, SWFDEC_TWIPS_TO_DOUBLE (x0));
  swfdec_as_object_set_variable (obj, SWFDEC_AS_STR_xMin, &val);
  SWFDEC_AS_VALUE_SET_NUMBER (&val, SWFDEC_TWIPS_TO_DOUBLE (y0));
  swfdec_as_object_set_variable (obj, SWFDEC_AS_STR_yMin, &val);
  SWFDEC_AS_VALUE_SET_NUMBER (&val, SWFDEC_TWIPS_TO_DOUBLE (x1));
  swfdec_as_object_set_variable (obj, SWFDEC_AS_STR_xMax, &val);
  SWFDEC_AS_VALUE_SET_NUMBER (&val, SWFDEC_TWIPS_TO_DOUBLE (y1));
  swfdec_as_object_set_variable (obj, SWFDEC_AS_STR_yMax, &val);

  SWFDEC_AS_VALUE_SET_OBJECT (rval, obj);
}

void
swfdec_sprite_movie_init_context (SwfdecPlayer *player, guint version)
{
  SwfdecAsContext *context = SWFDEC_AS_CONTEXT (player);
  SwfdecAsValue val;
  SwfdecAsObject *proto;

  player->MovieClip = SWFDEC_AS_OBJECT (swfdec_as_object_add_function (context->global, 
      SWFDEC_AS_STR_MovieClip, 0, NULL, 0));
  if (player->MovieClip == NULL)
    return;
  proto = swfdec_as_object_new (context);
  if (!proto)
    return;
  SWFDEC_AS_VALUE_SET_OBJECT (&val, proto);
  swfdec_as_object_set_variable_and_flags (player->MovieClip,
      SWFDEC_AS_STR_prototype, &val, SWFDEC_AS_VARIABLE_HIDDEN |
      SWFDEC_AS_VARIABLE_PERMANENT);
  /* now add all the functions */
  swfdec_as_object_add_function (proto, SWFDEC_AS_STR_getNextHighestDepth, SWFDEC_TYPE_SPRITE_MOVIE, 
      swfdec_sprite_movie_getNextHighestDepth, 0);
};
