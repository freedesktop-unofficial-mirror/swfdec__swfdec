/* Swfdec
 * Copyright (C) 2006 Benjamin Otte <otte@gnome.org>
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

#include "swfdec_graphic_movie.h"
#include "swfdec_button.h"
#include "swfdec_debug.h"
#include "swfdec_text_field.h"
#include "swfdec_movie.h"
#include "swfdec_shape.h"
#include "swfdec_sprite.h"
#include "swfdec_swf_decoder.h"
#include "swfdec_resource.h"
#include "swfdec_text.h"

G_DEFINE_TYPE (SwfdecGraphicMovie, swfdec_graphic_movie, SWFDEC_TYPE_MOVIE)

static void
swfdec_graphic_movie_update_extents (SwfdecMovie *movie,
    SwfdecRect *extents)
{
  swfdec_rect_union (extents, extents, 
      &movie->graphic->extents);
}

static void
swfdec_graphic_movie_render (SwfdecMovie *movie, cairo_t *cr, 
    const SwfdecColorTransform *trans, const SwfdecRect *inval)
{
  swfdec_graphic_render (movie->graphic, cr, trans, inval);
}

static gboolean
swfdec_graphic_movie_mouse_in (SwfdecMovie *movie, double x, double y)
{
  return swfdec_graphic_mouse_in (movie->graphic, x, y);
}

static void
swfdec_graphic_movie_replace (SwfdecMovie *movie, SwfdecGraphic *graphic)
{
  if (SWFDEC_IS_SHAPE (graphic) ||
      SWFDEC_IS_TEXT (graphic)) {
    /* nothing to do here, please move along */
  } else if (SWFDEC_IS_SPRITE (graphic) ||
      SWFDEC_IS_BUTTON (graphic) ||
      SWFDEC_IS_TEXT_FIELD (graphic)) {
    SWFDEC_INFO ("can't replace with scriptable objects");
    return;
  } else {
    SWFDEC_FIXME ("Can we replace with %s objects?", G_OBJECT_TYPE_NAME (graphic));
    return;
  }
  if (movie->graphic == graphic)
    return;
  SWFDEC_LOG ("replacing %u with %u", SWFDEC_CHARACTER (movie->graphic)->id,
      SWFDEC_CHARACTER (graphic)->id);
  swfdec_movie_queue_update (movie, SWFDEC_MOVIE_INVALID_CONTENTS);
  g_object_unref (movie->graphic);
  movie->graphic = g_object_ref (graphic);
}

static void
swfdec_graphic_movie_class_init (SwfdecGraphicMovieClass * g_class)
{
  SwfdecMovieClass *movie_class = SWFDEC_MOVIE_CLASS (g_class);

  movie_class->update_extents = swfdec_graphic_movie_update_extents;
  movie_class->replace = swfdec_graphic_movie_replace;
  movie_class->render = swfdec_graphic_movie_render;
  movie_class->mouse_in = swfdec_graphic_movie_mouse_in;
}

static void
swfdec_graphic_movie_init (SwfdecGraphicMovie * graphic_movie)
{
}

