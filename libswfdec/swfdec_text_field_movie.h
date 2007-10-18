/* Swfdec
 * Copyright (C) 2006 Benjamin Otte <otte@gnome.org>
 *                    Pekka Lampila <pekka.lampila@iki.fi>
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

#ifndef _SWFDEC_TEXT_FIELD_MOVIE_H_
#define _SWFDEC_TEXT_FIELD_MOVIE_H_

#include <libswfdec/swfdec_movie.h>
#include <libswfdec/swfdec_text_field.h>
#include <libswfdec/swfdec_style_sheet.h>
#include <libswfdec/swfdec_text_format.h>

G_BEGIN_DECLS


typedef struct _SwfdecTextFieldMovie SwfdecTextFieldMovie;
typedef struct _SwfdecTextFieldMovieClass SwfdecTextFieldMovieClass;

#define SWFDEC_TYPE_TEXT_FIELD_MOVIE                    (swfdec_text_field_movie_get_type())
#define SWFDEC_IS_TEXT_FIELD_MOVIE(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_TEXT_FIELD_MOVIE))
#define SWFDEC_IS_TEXT_FIELD_MOVIE_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_TEXT_FIELD_MOVIE))
#define SWFDEC_TEXT_FIELD_MOVIE(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_TEXT_FIELD_MOVIE, SwfdecTextFieldMovie))
#define SWFDEC_TEXT_FIELD_MOVIE_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_TEXT_FIELD_MOVIE, SwfdecTextFieldMovieClass))

typedef struct {
  guint			index;
  SwfdecTextFormat *	format;
} SwfdecFormatIndex;

struct _SwfdecTextFieldMovie {
  SwfdecMovie		movie;

  SwfdecTextField *	text;		/* the text_field object we render */

  const char *		text_input;
  const char *		text_display;

  const char *		variable;

  SwfdecTextFormat *	format_new;
  GSList *		formats;

  gboolean		condense_white;
  gboolean		embed_fonts;
  SwfdecStyleSheet *	style_sheet;

  gboolean		mouse_wheel_enabled;
  const char *		restrict_;

  SwfdecColor		border_color;
  SwfdecColor		background_color;

  // FIXME: Temporary using image surface, until there is a way to get cairo_t
  // outside the rendering functions
  cairo_surface_t *	surface;
  cairo_t *		cr;

  /* for rendering */
  SwfdecParagraph *	paragraphs;
};

struct _SwfdecTextFieldMovieClass {
  SwfdecMovieClass	movie_class;
};

GType		swfdec_text_field_movie_get_type		(void);

void		swfdec_text_field_movie_set_text		(SwfdecTextFieldMovie *	movie,
							 const char *		str,
							 gboolean		html);
void		swfdec_text_field_movie_format_changed	(SwfdecTextFieldMovie *	text);
void		swfdec_text_field_movie_set_text_format	(SwfdecTextFieldMovie *	text,
							 SwfdecTextFormat *	format,
							 guint			start_index,
							 guint			end_index);
void		swfdec_text_field_movie_set_scroll	(SwfdecTextFieldMovie *	text,
							 int			value);
const char *	swfdec_text_field_movie_get_html_text	(SwfdecTextFieldMovie *		text);
void		swfdec_text_field_movie_set_listen_variable (SwfdecTextFieldMovie *	text,
							 const char *			value);
void		swfdec_text_field_movie_set_listen_variable_text (SwfdecTextFieldMovie		*text,
							 const char *			value);

/* implemented in swfdec_text_field_movie_as.c */
void		swfdec_text_field_movie_init_properties	(SwfdecAsContext *	cx);

/* implemented in swfdec_html_parser.c */
void		swfdec_text_field_movie_html_parse	(SwfdecTextFieldMovie *	text, 
							 const char *		str);

G_END_DECLS
#endif
