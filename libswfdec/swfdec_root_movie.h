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

#ifndef _SWFDEC_ROOT_MOVIE_H_
#define _SWFDEC_ROOT_MOVIE_H_

#include <libswfdec/swfdec_movie.h>
#include <libswfdec/swfdec_player.h>

G_BEGIN_DECLS

//typedef struct _SwfdecRootMovie SwfdecRootMovie;
typedef struct _SwfdecRootMovieClass SwfdecRootMovieClass;

#define SWFDEC_TYPE_ROOT_MOVIE                    (swfdec_root_movie_get_type())
#define SWFDEC_IS_ROOT_MOVIE(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_ROOT_MOVIE))
#define SWFDEC_IS_ROOT_MOVIE_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_ROOT_MOVIE))
#define SWFDEC_ROOT_MOVIE(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_ROOT_MOVIE, SwfdecRootMovie))
#define SWFDEC_ROOT_MOVIE_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_ROOT_MOVIE, SwfdecRootMovieClass))

struct _SwfdecRootMovie
{
  SwfdecMovie		movie;

  gboolean		error;		/* we're in error */
  SwfdecPlayer *	player;		/* player we're played in */
  SwfdecLoader *	loader;		/* the loader providing data for the decoder */
  SwfdecDecoder *	decoder;	/* decoder that decoded all the stuff used by us */
  guint			unnamed_count;	/* variable used for naming unnamed movies */
};

struct _SwfdecRootMovieClass
{
  SwfdecMovieClass	movie_class;
};

GType		swfdec_root_movie_get_type	(void);

void		swfdec_root_movie_parse		(SwfdecRootMovie *	movie);


G_END_DECLS
#endif
