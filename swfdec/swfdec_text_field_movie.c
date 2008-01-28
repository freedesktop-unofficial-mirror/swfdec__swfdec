/* Swfdec
 * Copyright (C) 2006 Benjamin Otte <otte@gnome.org>
 *               2007 Pekka Lampila <pekka.lampila@iki.fi>
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
#include <math.h>
#include <pango/pangocairo.h>

#include "swfdec_text_field_movie.h"
#include "swfdec_as_context.h"
#include "swfdec_as_strings.h"
#include "swfdec_as_interpret.h"
#include "swfdec_debug.h"
#include "swfdec_player_internal.h"
#include "swfdec_resource.h"
#include "swfdec_sandbox.h"
#include "swfdec_text_format.h"
#include "swfdec_xml.h"

G_DEFINE_TYPE (SwfdecTextFieldMovie, swfdec_text_field_movie, SWFDEC_TYPE_MOVIE)

#define EXTRA_MARGIN 2
#define BULLET_MARGIN 36

static void
swfdec_text_field_movie_update_extents (SwfdecMovie *movie,
    SwfdecRect *extents)
{
  swfdec_rect_union (extents, extents,
      &SWFDEC_GRAPHIC (SWFDEC_TEXT_FIELD_MOVIE (movie)->text)->extents);
}

static void
swfdec_text_field_movie_invalidate (SwfdecMovie *movie, const cairo_matrix_t *matrix, gboolean last)
{
  SwfdecRect rect;
  
  swfdec_rect_transform (&rect, 
    &SWFDEC_GRAPHIC (SWFDEC_TEXT_FIELD_MOVIE (movie)->text)->extents, matrix);
  swfdec_player_invalidate (SWFDEC_PLAYER (SWFDEC_AS_OBJECT (movie)->context), &rect);
}

static void
swfdec_text_field_movie_ensure_asterisks (SwfdecTextFieldMovie *text,
    guint length)
{
  g_return_if_fail (SWFDEC_IS_TEXT_FIELD_MOVIE (text));

  if (text->asterisks_length >= length)
    return;

  if (text->asterisks != NULL)
    g_free (text->asterisks);

  text->asterisks = g_malloc (length + 1);
  memset (text->asterisks, '*', length);
  text->asterisks[length] = 0;
  text->asterisks_length = length;
}

static void
swfdec_text_paragraph_add_attribute (SwfdecParagraph *paragraph,
    PangoAttribute *attr)
{
  paragraph->attrs = g_slist_prepend (paragraph->attrs, attr);
}

static void
swfdec_text_paragraph_add_block (SwfdecParagraph *paragraph, int index_,
    SwfdecTextFormat *format)
{
  gint32 length, i;
  SwfdecBlock *block;
  SwfdecAsValue val;

  block = g_new0 (SwfdecBlock, 1);

  block->index_ = index_;

  switch (format->align) {
    case SWFDEC_TEXT_ALIGN_LEFT:
      block->align = PANGO_ALIGN_LEFT;
      block->justify = FALSE;
      break;
    case SWFDEC_TEXT_ALIGN_RIGHT:
      block->align = PANGO_ALIGN_RIGHT;
      block->justify = FALSE;
      break;
    case SWFDEC_TEXT_ALIGN_CENTER:
      block->align = PANGO_ALIGN_CENTER;
      block->justify = FALSE;
      break;
    case SWFDEC_TEXT_ALIGN_JUSTIFY:
      block->align = PANGO_ALIGN_LEFT;
      block->justify = TRUE;
      break;
    default:
      g_assert_not_reached ();
  }
  block->leading = format->leading * 20 * PANGO_SCALE;
  block->block_indent = format->block_indent * 20;
  block->left_margin = format->left_margin * 20;
  block->right_margin = format->right_margin * 20;

  if (format->tab_stops != NULL) {
    length = swfdec_as_array_get_length (format->tab_stops);
    block->tab_stops = pango_tab_array_new (length, TRUE);
    for (i = 0; i < length; i++) {
      swfdec_as_array_get_value (format->tab_stops, i, &val);
      g_assert (SWFDEC_AS_VALUE_IS_NUMBER (&val));
      pango_tab_array_set_tab (block->tab_stops, i, PANGO_TAB_LEFT,
	  SWFDEC_AS_VALUE_GET_NUMBER (&val) * 20);
    }
  } else {
    block->tab_stops = NULL;
  }

  paragraph->blocks = g_slist_prepend (paragraph->blocks, block);
}

static void
swfdec_text_field_movie_generate_paragraph (SwfdecTextFieldMovie *text,
    SwfdecParagraph *paragraph, guint start_index, guint length)
{
  SwfdecTextFormat *format, *format_prev;
  guint index_;
  GSList *iter;
  PangoAttribute *attr_bold, *attr_color, *attr_font, *attr_italic,
		 *attr_letter_spacing, *attr_size, *attr_underline;
  // TODO: kerning, display

  g_assert (SWFDEC_IS_TEXT_FIELD_MOVIE (text));
  g_assert (paragraph != NULL);
  g_assert (start_index + length <= text->input->len);

  paragraph->index_ = start_index;
  paragraph->length = length;
  if (text->input->str[start_index + length - 1] == '\n' ||
      text->input->str[start_index + length - 1] == '\r') {
    paragraph->newline = TRUE;
  } else {
    paragraph->newline = FALSE;
  }

  paragraph->blocks = NULL;
  paragraph->attrs = NULL;

  g_assert (text->formats != NULL);
  for (iter = text->formats; iter->next != NULL &&
      ((SwfdecFormatIndex *)(iter->next->data))->index_ <= start_index;
      iter = iter->next);

  index_ = start_index;
  format = ((SwfdecFormatIndex *)(iter->data))->format;

  // Paragraph formats
  paragraph->bullet = format->bullet;
  paragraph->indent = format->indent * 20 * PANGO_SCALE;

  // Add new block
  swfdec_text_paragraph_add_block (paragraph, 0, format);

  // Open attributes
  attr_bold = pango_attr_weight_new (
      (format->bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL));
  attr_bold->start_index = 0;

  attr_color = pango_attr_foreground_new (SWFDEC_COLOR_R (format->color) * 255,
      SWFDEC_COLOR_G (format->color) * 255,
      SWFDEC_COLOR_B (format->color) * 255);
  attr_color->start_index = 0;

  // FIXME: embed fonts
  attr_font = pango_attr_family_new (format->font);
  attr_font->start_index = 0;

  attr_italic = pango_attr_style_new (
      (format->italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL));
  attr_italic->start_index = 0;

  attr_letter_spacing = pango_attr_letter_spacing_new (
      format->letter_spacing * 20 * PANGO_SCALE);
  attr_letter_spacing->start_index = 0;

  attr_size =
    pango_attr_size_new_absolute (MAX (format->size, 1) * 20 * PANGO_SCALE);
  attr_size->start_index = 0;

  attr_underline = pango_attr_underline_new (
      (format->underline ? PANGO_UNDERLINE_SINGLE : PANGO_UNDERLINE_NONE));
  attr_underline->start_index = 0;

  for (iter = iter->next;
      iter != NULL &&
      ((SwfdecFormatIndex *)(iter->data))->index_ < start_index + length;
      iter = iter->next)
  {
    format_prev = format;
    index_ = ((SwfdecFormatIndex *)(iter->data))->index_;
    format = ((SwfdecFormatIndex *)(iter->data))->format;

    // Add new block if necessary
    if (format_prev->align != format->align ||
       format_prev->bullet != format->bullet ||
       format_prev->indent != format->indent ||
       format_prev->leading != format->leading ||
       format_prev->block_indent != format->block_indent ||
       format_prev->left_margin != format->left_margin)
    {
      swfdec_text_paragraph_add_block (paragraph, index_ - start_index,
	  format);
    }

    // Change attributes if necessary
    if (format_prev->bold != format->bold) {
      attr_bold->end_index = index_ - start_index;
      swfdec_text_paragraph_add_attribute (paragraph, attr_bold);

      attr_bold = pango_attr_weight_new (
	  (format->bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL));
      attr_bold->start_index = index_ - start_index;
    }

    if (format_prev->color != format->color) {
      attr_color->end_index = index_ - start_index;
      swfdec_text_paragraph_add_attribute (paragraph, attr_color);

      attr_color = pango_attr_foreground_new (
	  SWFDEC_COLOR_R (format->color) * 255,
	  SWFDEC_COLOR_G (format->color) * 255,
	  SWFDEC_COLOR_B (format->color) * 255);
      attr_color->start_index = index_ - start_index;
    }

    if (format_prev->font != format->font) {
      attr_font->end_index = index_ - start_index;
      swfdec_text_paragraph_add_attribute (paragraph, attr_font);

      // FIXME: embed fonts
      attr_font = pango_attr_family_new (format->font);
      attr_font->start_index = index_ - start_index;
    }

    if (format_prev->italic != format->italic) {
      attr_italic->end_index = index_ - start_index;
      swfdec_text_paragraph_add_attribute (paragraph, attr_italic);

      attr_italic = pango_attr_style_new (
	  (format->italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL));
      attr_italic->start_index = index_ - start_index;
    }

    if (format_prev->letter_spacing != format->letter_spacing) {
      attr_letter_spacing->end_index = index_ - start_index;
      swfdec_text_paragraph_add_attribute (paragraph, attr_letter_spacing);

      attr_letter_spacing = pango_attr_letter_spacing_new (
	  format->letter_spacing * 20 * PANGO_SCALE);
      attr_letter_spacing->start_index = index_ - start_index;
    }

    if (format_prev->size != format->size) {
      attr_size->end_index = index_ - start_index;
      swfdec_text_paragraph_add_attribute (paragraph, attr_size);

      attr_size = pango_attr_size_new_absolute (
	  MAX (1, format->size) * 20 * PANGO_SCALE);
      attr_size->start_index = index_ - start_index;
    }

    if (format_prev->underline != format->underline) {
      attr_underline->end_index = index_ - start_index;
      swfdec_text_paragraph_add_attribute (paragraph, attr_underline);

      attr_underline = pango_attr_underline_new (
	  (format->underline ? PANGO_UNDERLINE_SINGLE : PANGO_UNDERLINE_NONE));
      attr_underline->start_index = index_ - start_index;
    }
  }

  // Close attributes
  attr_bold->end_index = length;
  swfdec_text_paragraph_add_attribute (paragraph, attr_bold);
  attr_bold = NULL;

  attr_color->end_index = length;
  swfdec_text_paragraph_add_attribute (paragraph, attr_color);
  attr_color = NULL;

  attr_font->end_index = length;
  swfdec_text_paragraph_add_attribute (paragraph, attr_font);
  attr_font = NULL;

  attr_italic->end_index = length;
  swfdec_text_paragraph_add_attribute (paragraph, attr_italic);
  attr_italic = NULL;

  attr_letter_spacing->end_index = length;
  swfdec_text_paragraph_add_attribute (paragraph, attr_letter_spacing);
  attr_letter_spacing = NULL;

  attr_size->end_index = length;
  swfdec_text_paragraph_add_attribute (paragraph, attr_size);
  attr_size = NULL;

  attr_underline->end_index = length;
  swfdec_text_paragraph_add_attribute (paragraph, attr_underline);
  attr_underline = NULL;

  // reverse blocks since we use prepend to add them
  paragraph->blocks = g_slist_reverse (paragraph->blocks);
}

static SwfdecParagraph *
swfdec_text_field_movie_get_paragraphs (SwfdecTextFieldMovie *text, int *num)
{
  GArray *paragraphs;
  SwfdecParagraph paragraph;
  const char *p, *end;
  guint max_length;

  g_assert (SWFDEC_IS_TEXT_FIELD_MOVIE (text));

  paragraphs = g_array_new (TRUE, TRUE, sizeof (SwfdecParagraph));

  max_length = 0;
  p = text->input->str;
  while (*p != '\0')
  {
    end = strpbrk (p, "\r\n");
    if (end == NULL) {
      end = strchr (p, '\0');
    } else {
      end++;
    }

    if ((guint) (end - p) > max_length)
      max_length = end - p;

    swfdec_text_field_movie_generate_paragraph (text, &paragraph,
	p - text->input->str, end - p);
    paragraphs = g_array_append_val (paragraphs, paragraph);

    p = end;
  }

  if (num != NULL)
    *num = paragraphs->len;

  if (text->text->password)
    swfdec_text_field_movie_ensure_asterisks (text, max_length);

  return (SwfdecParagraph *) (void *) g_array_free (paragraphs, FALSE);
}

static void
swfdec_text_field_movie_free_paragraphs (SwfdecParagraph *paragraphs)
{
  GSList *iter;
  int i;

  g_return_if_fail (paragraphs != NULL);

  for (i = 0; paragraphs[i].blocks != NULL; i++)
  {
    for (iter = paragraphs[i].blocks; iter != NULL; iter = iter->next) {
      if (((SwfdecBlock *)(iter->data))->tab_stops)
	pango_tab_array_free (((SwfdecBlock *)(iter->data))->tab_stops);
      g_free (iter->data);
    }
    g_slist_free (paragraphs[i].blocks);

    for (iter = paragraphs[i].attrs; iter != NULL; iter = iter->next) {
      pango_attribute_destroy ((PangoAttribute *)(iter->data));
    }
    g_slist_free (paragraphs[i].attrs);
  }
  g_free (paragraphs);
}

/*
 * Rendering
 */
static PangoAttrList *
swfdec_text_field_movie_paragraph_get_attr_list (
    const SwfdecParagraph *paragraph, guint index_,
    const SwfdecColorTransform *trans)
{
  PangoAttrList *attr_list;
  GSList *iter;

  attr_list = pango_attr_list_new ();

  for (iter = paragraph->attrs; iter != NULL; iter = iter->next)
  {
    PangoAttribute *attr;

    if (((PangoAttribute *)iter->data)->end_index <= index_)
      continue;

    attr = pango_attribute_copy ((PangoAttribute *)iter->data);

    if (attr->klass->type == PANGO_ATTR_FOREGROUND && trans != NULL &&
	!swfdec_color_transform_is_identity (trans))
    {
      SwfdecColor color;
      PangoColor color_p;

      color_p = ((PangoAttrColor *)attr)->color;

      color = SWFDEC_COLOR_COMBINE (color_p.red >> 8, color_p.green >> 8,
	  color_p.blue >> 8, 255);
      color = swfdec_color_apply_transform (color, trans);

      color_p.red = SWFDEC_COLOR_R (color) << 8;
      color_p.green = SWFDEC_COLOR_G (color) << 8;
      color_p.blue = SWFDEC_COLOR_B (color) << 8;
    }

    attr->start_index =
      (attr->start_index > index_ ? attr->start_index - index_ : 0);
    attr->end_index = attr->end_index - index_;
    pango_attr_list_insert (attr_list, attr);
  }

  return attr_list;
}

static int
swfdec_text_field_movie_layout_get_last_line_baseline (PangoLayout *playout)
{
  int baseline;
  PangoLayoutIter *iter;

  g_return_val_if_fail (playout != NULL, 0);

  iter = pango_layout_get_iter (playout);
  while (!pango_layout_iter_at_last_line (iter))
    pango_layout_iter_next_line (iter);

  baseline = pango_layout_iter_get_baseline (iter) / PANGO_SCALE;

  pango_layout_iter_free (iter);

  return baseline;
}

static void
swfdec_text_field_movie_attr_list_get_ascent_descent (PangoAttrList *attr_list,
    guint pos, int *ascent, int *descent)
{
  PangoAttrIterator *attr_iter;
  PangoFontDescription *desc;
  PangoFontMap *fontmap;
  PangoFont *font;
  PangoFontMetrics *metrics;
  PangoContext *pcontext;
  int end;

  if (ascent != NULL)
    *ascent = 0;
  if (descent != NULL)
    *descent = 0;

  g_return_if_fail (attr_list != NULL);

  attr_iter = pango_attr_list_get_iterator (attr_list);
  pango_attr_iterator_range (attr_iter, NULL, &end);
  while ((guint)end < pos && pango_attr_iterator_next (attr_iter)) {
    pango_attr_iterator_range (attr_iter, NULL, &end);
  }
  desc = pango_font_description_new ();
  pango_attr_iterator_get_font (attr_iter, desc, NULL, NULL);
  fontmap = pango_cairo_font_map_get_default ();
  pcontext =
    pango_cairo_font_map_create_context (PANGO_CAIRO_FONT_MAP (fontmap));
  font = pango_font_map_load_font (fontmap, pcontext, desc);
  metrics = pango_font_get_metrics (font, NULL);

  if (ascent != NULL)
    *ascent = pango_font_metrics_get_ascent (metrics) / PANGO_SCALE;
  if (descent != NULL)
    *descent = pango_font_metrics_get_descent (metrics) / PANGO_SCALE;

  g_object_unref (pcontext);
  pango_font_metrics_unref (metrics);
  pango_font_description_free (desc);
  pango_attr_iterator_destroy (attr_iter);
}

static SwfdecLayout *
swfdec_text_field_movie_get_layouts (SwfdecTextFieldMovie *text, int *num,
    cairo_t *cr, const SwfdecParagraph *paragraphs,
    const SwfdecColorTransform *trans)
{
  GArray *layouts;
  guint i;
  SwfdecParagraph *paragraphs_free;

  g_assert (SWFDEC_IS_TEXT_FIELD_MOVIE (text));

  if (cr == NULL)
    cr = text->cr;

  if (paragraphs == NULL) {
    paragraphs_free = swfdec_text_field_movie_get_paragraphs (text, NULL);
    paragraphs = paragraphs_free;
  } else {
    paragraphs_free = NULL;
  }

  layouts = g_array_new (TRUE, TRUE, sizeof (SwfdecLayout));

  for (i = 0; paragraphs[i].blocks != NULL; i++)
  {
    GSList *iter;
    guint skip;

    skip = 0;
    for (iter = paragraphs[i].blocks; iter != NULL; iter = iter->next)
    {
      SwfdecLayout layout;
      PangoLayout *playout;
      PangoAttrList *attr_list;
      SwfdecBlock *block;
      int width;
      guint length;
      gboolean end_of_paragraph;

      block = (SwfdecBlock *)iter->data;
      if (iter->next != NULL) {
	length =
	  ((SwfdecBlock *)(iter->next->data))->index_ - block->index_;
      } else {
	length = paragraphs[i].length - block->index_;
	if (paragraphs[i].newline)
	  length -= 1;
      }

      if (skip > length) {
	skip -= length;
	continue;
      }

      // create layout
      playout = layout.layout = pango_cairo_create_layout (cr);

      // set rendering position
      layout.offset_x = block->left_margin + block->block_indent;
      if (paragraphs[i].bullet)
	layout.offset_x += SWFDEC_DOUBLE_TO_TWIPS (BULLET_MARGIN);

      width = SWFDEC_MOVIE (text)->original_extents.x1 -
	SWFDEC_MOVIE (text)->original_extents.x0 - block->right_margin -
	layout.offset_x;

      if (block->index_ == 0 && paragraphs[i].indent < 0) {
	// limit negative indent to not go over leftMargin + blockIndent
	int indent = MAX (paragraphs[i].indent / PANGO_SCALE,
	    -(block->left_margin + block->block_indent));
	layout.offset_x += indent;
	width += -indent;
      }

      if (text->text->word_wrap) {
	pango_layout_set_wrap (playout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_width (playout, width * PANGO_SCALE);
	pango_layout_set_alignment (playout, block->align);
	pango_layout_set_justify (playout, block->justify);
      } else {
	pango_layout_set_width (playout, -1);
      }

      // set paragraph styles
      if (block->index_ == 0) {
	pango_layout_set_indent (playout, paragraphs[i].indent);
	layout.bullet = paragraphs[i].bullet;
      } else {
	pango_layout_set_indent (playout, 0);
	layout.bullet = FALSE;
      }

      // set block styles
      pango_layout_set_spacing (playout, block->leading);
      if (block->tab_stops != NULL)
	pango_layout_set_tabs (playout, block->tab_stops);

      // set text attributes
      attr_list = swfdec_text_field_movie_paragraph_get_attr_list (
	  &paragraphs[i], block->index_ + skip, trans);
      pango_layout_set_attributes (playout, attr_list);

      if (text->text->password) {
	pango_layout_set_text (playout, text->asterisks, paragraphs[i].length -
	    block->index_ - skip - (paragraphs[i].newline ? 1 : 0));
      } else {
	pango_layout_set_text (playout,
	    text->input->str + paragraphs[i].index_ + block->index_ + skip,
	    paragraphs[i].length - block->index_ - skip -
	    (paragraphs[i].newline ? 1 : 0));
      }

      end_of_paragraph = TRUE;
      if (iter->next != NULL && text->text->word_wrap)
      {
	PangoLayoutLine *line;
	int line_num;
	guint skip_new;

	pango_layout_index_to_line_x (playout, length - skip, FALSE, &line_num,
	    NULL);
	if (line_num < pango_layout_get_line_count (playout) - 1) {
	  end_of_paragraph = FALSE;
	  line = pango_layout_get_line_readonly (playout, line_num);
	  skip_new = line->start_index + line->length - (length - skip);
	  pango_layout_set_text (playout,
	      text->input->str + paragraphs[i].index_ + block->index_ + skip,
	      length - skip + skip_new);
	  skip = skip_new;
	}
      }
      else
      {
	if (!text->text->word_wrap && block->align != PANGO_ALIGN_LEFT) {
	  int line_width;
	  pango_layout_get_pixel_size (playout, &line_width, 0);
	  if (line_width < width) {
	    if (block->align == PANGO_ALIGN_RIGHT) {
	      layout.offset_x += width - line_width;
	    } else if (block->align == PANGO_ALIGN_CENTER) {
	      layout.offset_x += (width - line_width) / 2;
	    } else {
	      g_assert_not_reached ();
	    }
	  }
	}

	skip = 0;
      }

      pango_layout_get_pixel_size (playout, &layout.width, &layout.height);
      layout.width += layout.offset_x + block->right_margin;
      layout.last_line_offset_y = 0;

      // figure out if we need to add extra height because of the size of the
      // line break character
      if (end_of_paragraph && paragraphs[i].newline)
      {
	int ascent, descent;

	swfdec_text_field_movie_attr_list_get_ascent_descent (attr_list,
	    paragraphs[i].length - block->index_ - skip, &ascent, &descent);

	if (ascent + descent > layout.height) {
	  int baseline =
	    swfdec_text_field_movie_layout_get_last_line_baseline (playout);
	  layout.last_line_offset_y = ascent - baseline;
	  layout.height = ascent + descent;
	}
      }

      pango_attr_list_unref (attr_list);

      // add leading to last line too
      layout.height += block->leading / PANGO_SCALE;

      layouts = g_array_append_val (layouts, layout);

      if (!text->text->word_wrap)
	break;
    }
  }

  if (paragraphs_free != NULL) {
    swfdec_text_field_movie_free_paragraphs (paragraphs_free);
    paragraphs_free = NULL;
    paragraphs = NULL;
  }

  if (num != NULL)
    *num = layouts->len;

  return (SwfdecLayout *) (void *) g_array_free (layouts, FALSE);
}

static void
swfdec_text_field_movie_free_layouts (SwfdecLayout *layouts)
{
  int i;

  g_return_if_fail (layouts != NULL);

  for (i = 0; layouts[i].layout != NULL; i++) {
    g_object_unref (layouts[i].layout);
  }

  g_free (layouts);
}

static void
swfdec_text_field_movie_render (SwfdecMovie *movie, cairo_t *cr,
    const SwfdecColorTransform *trans, const SwfdecRect *inval)
{
  SwfdecTextFieldMovie *text_movie;
  SwfdecTextField *text;
  SwfdecLayout *layouts;
  SwfdecRect limit;
  SwfdecColor color;
  SwfdecParagraph *paragraphs;
  int i, y, x, linenum;
  gboolean first;

  g_return_if_fail (SWFDEC_IS_TEXT_FIELD_MOVIE (movie));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (trans != NULL);
  g_return_if_fail (inval != NULL);

  /* textfields don't mask */
  if (swfdec_color_transform_is_mask (trans))
    return;

  text_movie = SWFDEC_TEXT_FIELD_MOVIE (movie);
  text = SWFDEC_TEXT_FIELD (movie->graphic);

  paragraphs = swfdec_text_field_movie_get_paragraphs (text_movie, NULL);

  swfdec_rect_intersect (&limit, &movie->original_extents, inval);

  cairo_rectangle (cr, limit.x0, limit.y0, limit.x1 - limit.x0,
      limit.y1 - limit.y0);
  cairo_clip (cr);

  if (text->background) {
    cairo_rectangle (cr, limit.x0, limit.y0, limit.x1 - limit.x0,
	limit.y1 - limit.y0);
    color = swfdec_color_apply_transform (text_movie->background_color, trans);
    // always use full alpha
    swfdec_color_set_source (cr, color | SWFDEC_COLOR_COMBINE (0, 0, 0, 255));
    cairo_fill (cr);
  }

  if (text->border) {
    // FIXME: border should be partly outside the extents and should not be
    // scaled, but always be 1 pixel width
    cairo_rectangle (cr, movie->original_extents.x0 +
	SWFDEC_DOUBLE_TO_TWIPS (1), movie->original_extents.y0,
	movie->original_extents.x1 - movie->original_extents.x0 -
	SWFDEC_DOUBLE_TO_TWIPS (1), movie->original_extents.y1 -
	movie->original_extents.y0 - SWFDEC_DOUBLE_TO_TWIPS (1));
    color = swfdec_color_apply_transform (text_movie->border_color, trans);
    // always use full alpha
    swfdec_color_set_source (cr, color | SWFDEC_COLOR_COMBINE (0, 0, 0, 255));
    cairo_set_line_width (cr, SWFDEC_DOUBLE_TO_TWIPS (1));
    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
    cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
    cairo_stroke (cr);
  }

  layouts = swfdec_text_field_movie_get_layouts (text_movie, NULL, cr,
      paragraphs, trans);

  first = TRUE;
  linenum = 0;
  x = movie->original_extents.x0 + EXTRA_MARGIN +
    MIN (text_movie->hscroll, text_movie->hscroll_max);
  y = movie->original_extents.y0 + EXTRA_MARGIN;

  for (i = 0; layouts[i].layout != NULL && y < limit.y1; i++)
  {
    SwfdecLayout *layout = &layouts[i];
    PangoLayoutIter *iter_line;
    PangoLayoutLine *line;
    PangoRectangle rect;
    int skipped;

    iter_line = pango_layout_get_iter (layout->layout);

    if (layout->bullet && linenum + 1 >=
	MIN (text_movie->scroll, text_movie->scroll_max)) {
      PangoColor color_p;
      PangoAttribute *attr;
      PangoAttrIterator *attr_iter;

      pango_layout_iter_get_line_extents (iter_line, NULL, &rect);
      pango_extents_to_pixels (NULL, &rect);

      cairo_new_sub_path (cr);

      // get current color
      attr_iter = pango_attr_list_get_iterator (
	  pango_layout_get_attributes (layout->layout));
      attr = pango_attr_iterator_get (attr_iter, PANGO_ATTR_FOREGROUND);
      color_p = ((PangoAttrColor *)attr)->color;
      color = SWFDEC_COLOR_COMBINE (color_p.red >> 8, color_p.green >> 8,
	  color_p.blue >> 8, 255);
      color = swfdec_color_apply_transform (color, trans);
      pango_attr_iterator_destroy (attr_iter);

      swfdec_color_set_source (cr, color);

      cairo_arc (cr, x + layout->offset_x +
	  pango_layout_get_indent (layout->layout) -
	  SWFDEC_DOUBLE_TO_TWIPS (BULLET_MARGIN) / 2,
	  y + rect.height / 2, rect.height / 8, 20, 2 * M_PI);
      cairo_fill (cr);
    }

    skipped = 0;
    do {
      if (++linenum < MIN (text_movie->scroll, text_movie->scroll_max))
	continue;

      pango_layout_iter_get_line_extents (iter_line, NULL, &rect);
      pango_extents_to_pixels (NULL, &rect);

      if (linenum == MIN (text_movie->scroll, text_movie->scroll_max))
	skipped = rect.y;

      if (!first && y + rect.y + rect.height > movie->original_extents.y1)
	break;

      first = FALSE;

      if (y + rect.y > limit.y1)
	break;

      if (y + rect.y + rect.height < limit.y0 ||
	  x + layout->offset_x + rect.x > limit.x1 ||
	  x + layout->offset_x + rect.x + rect.width < limit.x0)
	continue;

      cairo_move_to (cr, x, y);

      if (pango_layout_iter_at_last_line (iter_line))
	cairo_rel_move_to (cr, 0, layout->last_line_offset_y);
      cairo_rel_move_to (cr, layout->offset_x + rect.x,
	  pango_layout_iter_get_baseline (iter_line) / PANGO_SCALE - skipped);

      line = pango_layout_iter_get_line_readonly (iter_line);
      pango_cairo_show_layout_line (cr, line);
    } while (pango_layout_iter_next_line (iter_line));

    if (linenum >= MIN (text_movie->scroll, text_movie->scroll_max)) {
      y += layout->height - skipped;
      skipped = 0;
    }

    pango_layout_iter_free (iter_line);
  }

  swfdec_text_field_movie_free_layouts (layouts);

  swfdec_text_field_movie_free_paragraphs (paragraphs);
}

void
swfdec_text_field_movie_update_scroll (SwfdecTextFieldMovie *text,
    gboolean check_limits)
{
  SwfdecLayout *layouts;
  int i, num, y, visible, all, height;
  double width, width_max;

  g_return_if_fail (SWFDEC_IS_TEXT_FIELD_MOVIE (text));

  layouts = swfdec_text_field_movie_get_layouts (text, &num, NULL, NULL, NULL);

  width = SWFDEC_MOVIE (text)->original_extents.x1 -
    SWFDEC_MOVIE (text)->original_extents.x0;
  height = SWFDEC_MOVIE (text)->original_extents.y1 -
    SWFDEC_MOVIE (text)->original_extents.y0;

  width_max = width;
  y = 0;
  all = 0;
  visible = 0;

  for (i = num - 1; i >= 0; i--)
  {
    SwfdecLayout *layout = &layouts[i];
    PangoLayoutIter *iter_line;
    PangoRectangle rect;

    if (layouts[i].width > width_max)
      width_max = layouts[i].width;

    y += layout->height;

    iter_line = pango_layout_get_iter (layout->layout);

    do {
      pango_layout_iter_get_line_extents (iter_line, NULL, &rect);
      pango_extents_to_pixels (NULL, &rect);

      if (y - rect.y <= height)
	visible++;

      all++;
    } while (pango_layout_iter_next_line (iter_line));

    pango_layout_iter_free (iter_line);
  }

  swfdec_text_field_movie_free_layouts (layouts);
  layouts = NULL;

  if (text->scroll_max != all - visible + 1) {
    text->scroll_max = all - visible + 1;
    text->scroll_changed = TRUE;
  }
  if (text->hscroll_max != SWFDEC_TWIPS_TO_DOUBLE (width_max - width)) {
    text->hscroll_max = SWFDEC_TWIPS_TO_DOUBLE (width_max - width);
    text->scroll_changed = TRUE;
  }

  if (check_limits) {
    if (text->scroll != CLAMP(text->scroll, 1, text->scroll_max)) {
      text->scroll = CLAMP(text->scroll, 1, text->scroll_max);
      text->scroll_changed = TRUE;
    }
    if (text->scroll_bottom != text->scroll + (visible > 0 ? visible - 1 : 0))
    {
      text->scroll_bottom = text->scroll + (visible > 0 ? visible - 1 : 0);
      text->scroll_changed = TRUE;
    }
    if (text->hscroll != CLAMP(text->hscroll, 0, text->hscroll_max)) {
      text->hscroll = CLAMP(text->hscroll, 0, text->hscroll_max);
      text->scroll_changed = TRUE;
    }
  } else {
    if (text->scroll_bottom < text->scroll ||
	text->scroll_bottom > text->scroll_max + visible - 1) {
      text->scroll_bottom = text->scroll;
      text->scroll_changed = TRUE;
    }
  }
}

void
swfdec_text_field_movie_get_text_size (SwfdecTextFieldMovie *text, int *width,
    int *height)
{
  SwfdecLayout *layouts;
  int i;

  if (width != NULL)
    *width = 0;
  if (height != NULL)
    *height = 0;

  g_return_if_fail (SWFDEC_IS_TEXT_FIELD_MOVIE (text));
  g_return_if_fail (width != NULL || height != NULL);

  layouts = swfdec_text_field_movie_get_layouts (text, NULL, NULL, NULL, NULL);

  for (i = 0; layouts[i].layout != NULL; i++) {
    if (!text->text->word_wrap) {
      if (width != NULL && layouts[i].width > *width)
	*width = layouts[i].width;
    }

    if (height != NULL)
      *height += layouts[i].height;
  }

  // align to get integer amount after TWIPS_TO_DOUBLE
  if (width != NULL && *width % SWFDEC_TWIPS_SCALE_FACTOR != 0)
    *width += SWFDEC_TWIPS_SCALE_FACTOR - *width % SWFDEC_TWIPS_SCALE_FACTOR;
  if (height != NULL && *height % SWFDEC_TWIPS_SCALE_FACTOR != 0)
    *height += SWFDEC_TWIPS_SCALE_FACTOR - *height % SWFDEC_TWIPS_SCALE_FACTOR;

  swfdec_text_field_movie_free_layouts (layouts);
}

gboolean
swfdec_text_field_movie_auto_size (SwfdecTextFieldMovie *text)
{
  SwfdecGraphic *graphic;
  int height, width, diff;

  g_return_val_if_fail (SWFDEC_IS_TEXT_FIELD_MOVIE (text), FALSE);

  graphic = SWFDEC_GRAPHIC (text->text);

  if (text->text->auto_size == SWFDEC_AUTO_SIZE_NONE)
    return FALSE;

  swfdec_text_field_movie_get_text_size (text, &width, &height);
  width += SWFDEC_DOUBLE_TO_TWIPS (2 * EXTRA_MARGIN);
  height += SWFDEC_DOUBLE_TO_TWIPS (2 * EXTRA_MARGIN);

  if ((text->text->word_wrap ||
	graphic->extents.x1 - graphic->extents.x0 == width) &&
      graphic->extents.y1 - graphic->extents.y0 == height)
    return FALSE;

  swfdec_movie_invalidate_next (SWFDEC_MOVIE (text));

  if (!text->text->word_wrap && graphic->extents.x1 -
      graphic->extents.x0 != width)
  {
    switch (text->text->auto_size) {
      case SWFDEC_AUTO_SIZE_LEFT:
	graphic->extents.x1 = graphic->extents.x0 + width;
	break;
      case SWFDEC_AUTO_SIZE_RIGHT:
	graphic->extents.x0 = graphic->extents.x1 - width;
	break;
      case SWFDEC_AUTO_SIZE_CENTER:
	diff = (graphic->extents.x1 - graphic->extents.x0) - width;
	graphic->extents.x0 += floor (diff / 2.0);
	graphic->extents.x1 = graphic->extents.x0 + width;
	break;
      case SWFDEC_AUTO_SIZE_NONE:
      default:
	g_return_val_if_reached (FALSE);
    }
  }

  if (graphic->extents.y1 - graphic->extents.y0 != height)
  {
    graphic->extents.y1 = graphic->extents.y0 + height;
  }

  swfdec_movie_queue_update (SWFDEC_MOVIE (text),
      SWFDEC_MOVIE_INVALID_EXTENTS);

  return TRUE;
}

static void
swfdec_text_field_movie_dispose (GObject *object)
{
  SwfdecTextFieldMovie *text;
  GSList *iter;

  text = SWFDEC_TEXT_FIELD_MOVIE (object);

  if (text->asterisks != NULL) {
    g_free (text->asterisks);
    text->asterisks = NULL;
    text->asterisks_length = 0;
  }

  if (SWFDEC_IS_STYLESHEET (text->style_sheet)) {
    g_signal_handlers_disconnect_matched (text->style_sheet, 
	G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, text);
    g_object_remove_weak_pointer (G_OBJECT (text->style_sheet), 
	(gpointer) &text->style_sheet);
  }
  text->style_sheet = NULL;

  for (iter = text->formats; iter != NULL; iter = iter->next) {
    g_free (text->formats->data);
    text->formats->data = NULL;
  }
  g_slist_free (text->formats);
  text->formats = NULL;

  g_string_free (text->input, TRUE);
  text->input = NULL;

  cairo_destroy (text->cr);
  text->cr = NULL;
  cairo_surface_destroy (text->surface);
  text->surface = NULL;

  G_OBJECT_CLASS (swfdec_text_field_movie_parent_class)->dispose (object);
}

static void
swfdec_text_field_movie_mark (SwfdecAsObject *object)
{
  SwfdecTextFieldMovie *text;
  GSList *iter;

  text = SWFDEC_TEXT_FIELD_MOVIE (object);

  if (text->variable != NULL)
    swfdec_as_string_mark (text->variable);
  swfdec_as_object_mark (SWFDEC_AS_OBJECT (text->format_new));
  for (iter = text->formats; iter != NULL; iter = iter->next) {
    swfdec_as_object_mark (
	SWFDEC_AS_OBJECT (((SwfdecFormatIndex *)(iter->data))->format));
  }
  swfdec_as_object_mark (SWFDEC_AS_OBJECT (text->format_new));
  if (text->style_sheet != NULL)
    swfdec_as_object_mark (text->style_sheet);
  if (text->style_sheet_input != NULL)
    swfdec_as_string_mark (text->style_sheet_input);
  if (text->restrict_ != NULL)
    swfdec_as_string_mark (text->restrict_);

  SWFDEC_AS_OBJECT_CLASS (swfdec_text_field_movie_parent_class)->mark (object);
}

static void
swfdec_text_field_movie_init_movie (SwfdecMovie *movie)
{
  SwfdecTextFieldMovie *text = SWFDEC_TEXT_FIELD_MOVIE (movie);
  SwfdecAsContext *cx;
  SwfdecAsValue val;
  gboolean needs_unuse;

  needs_unuse = swfdec_sandbox_try_use (movie->resource->sandbox);

  cx = SWFDEC_AS_OBJECT (movie)->context;

  swfdec_text_field_movie_init_properties (cx);

  swfdec_as_object_get_variable (cx->global, SWFDEC_AS_STR_TextField, &val);
  if (SWFDEC_AS_VALUE_IS_OBJECT (&val)) {
    swfdec_as_object_set_constructor (SWFDEC_AS_OBJECT (movie),
	SWFDEC_AS_VALUE_GET_OBJECT (&val));
  }

  // listen self
  SWFDEC_AS_VALUE_SET_OBJECT (&val, SWFDEC_AS_OBJECT (movie));
  swfdec_as_object_call (SWFDEC_AS_OBJECT (movie), SWFDEC_AS_STR_addListener,
      1, &val, NULL);

  // format
  text->format_new =
    SWFDEC_TEXT_FORMAT (swfdec_text_format_new_no_properties (cx));
  if (!text->format_new)
    goto out;

  swfdec_text_format_set_defaults (text->format_new);
  text->format_new->color = text->text->color;
  text->format_new->align = text->text->align;
  if (text->text->font != NULL)  {
    text->format_new->font =
      swfdec_as_context_get_string (cx, text->text->font);
  }
  text->format_new->size = text->text->size / 20;
  text->format_new->left_margin = text->text->left_margin / 20;
  text->format_new->right_margin = text->text->right_margin / 20;
  text->format_new->indent = text->text->indent / 20;
  text->format_new->leading = text->text->leading / 20;

  text->border_color = SWFDEC_COLOR_COMBINE (0, 0, 0, 0);
  text->background_color = SWFDEC_COLOR_COMBINE (255, 255, 255, 0);

  // text
  if (text->text->input != NULL) {
    swfdec_text_field_movie_set_text (text,
	swfdec_as_context_get_string (cx, text->text->input),
	text->text->html);
  } else {
    swfdec_text_field_movie_set_text (text, SWFDEC_AS_STR_EMPTY,
	text->text->html);
  }

  // variable
  if (text->text->variable != NULL) {
    swfdec_text_field_movie_set_listen_variable (text,
	swfdec_as_context_get_string (cx, text->text->variable));
  }

out:
  if (needs_unuse)
    swfdec_sandbox_unuse (movie->resource->sandbox);
}

static void
swfdec_text_field_movie_finish_movie (SwfdecMovie *movie)
{
  SwfdecTextFieldMovie *text = SWFDEC_TEXT_FIELD_MOVIE (movie);

  swfdec_text_field_movie_set_listen_variable (text, NULL);
}

static void
swfdec_text_field_movie_iterate (SwfdecMovie *movie)
{
  SwfdecTextFieldMovie *text = SWFDEC_TEXT_FIELD_MOVIE (movie);

  if (text->scroll_changed) {
    SwfdecAsValue argv[2];

    SWFDEC_FIXME ("I'm pretty sure this is swfdec_player_add_action()'d");
    SWFDEC_AS_VALUE_SET_STRING (&argv[0], SWFDEC_AS_STR_onScroller);
    SWFDEC_AS_VALUE_SET_OBJECT (&argv[1], SWFDEC_AS_OBJECT (movie));
    swfdec_sandbox_use (movie->resource->sandbox);
    swfdec_as_object_call (SWFDEC_AS_OBJECT (movie),
	SWFDEC_AS_STR_broadcastMessage, 2, argv, NULL);
    swfdec_sandbox_unuse (movie->resource->sandbox);

    /* FIXME: unset this before or after emitting the event? */
    text->scroll_changed = FALSE;
  }
}

static void
swfdec_text_field_movie_class_init (SwfdecTextFieldMovieClass * g_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (g_class);
  SwfdecAsObjectClass *asobject_class = SWFDEC_AS_OBJECT_CLASS (g_class);
  SwfdecMovieClass *movie_class = SWFDEC_MOVIE_CLASS (g_class);

  object_class->dispose = swfdec_text_field_movie_dispose;

  asobject_class->mark = swfdec_text_field_movie_mark;

  movie_class->init_movie = swfdec_text_field_movie_init_movie;
  movie_class->finish_movie = swfdec_text_field_movie_finish_movie;
  movie_class->iterate_start = swfdec_text_field_movie_iterate;
  movie_class->update_extents = swfdec_text_field_movie_update_extents;
  movie_class->render = swfdec_text_field_movie_render;
  movie_class->invalidate = swfdec_text_field_movie_invalidate;
}

static void
swfdec_text_field_movie_init (SwfdecTextFieldMovie *text)
{
  text->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 1);
  text->cr = cairo_create (text->surface);

  text->input = g_string_new ("");
  text->scroll = 1;
  text->mouse_wheel_enabled = TRUE;
}

void
swfdec_text_field_movie_set_text_format (SwfdecTextFieldMovie *text,
    SwfdecTextFormat *format, guint start_index, guint end_index)
{
  SwfdecFormatIndex *findex, *findex_new, *findex_prev;
  guint findex_end_index;
  GSList *iter, *next;

  g_return_if_fail (SWFDEC_IS_TEXT_FIELD_MOVIE (text));
  g_return_if_fail (SWFDEC_IS_TEXT_FORMAT (format));
  g_return_if_fail (start_index < end_index);
  g_return_if_fail (end_index <= text->input->len);

  g_assert (text->formats != NULL);
  g_assert (text->formats->data != NULL);
  g_assert (((SwfdecFormatIndex *)text->formats->data)->index_ == 0);

  findex = NULL;
  for (iter = text->formats; iter != NULL &&
      ((SwfdecFormatIndex *)iter->data)->index_ < end_index;
      iter = next)
  {
    findex_prev = findex;
    next = iter->next;
    findex = iter->data;
    if (iter->next != NULL) {
      findex_end_index =
	((SwfdecFormatIndex *)iter->next->data)->index_;
    } else {
      findex_end_index = text->input->len;
    }

    if (findex_end_index <= start_index)
      continue;

    if (swfdec_text_format_equal_or_undefined (findex->format, format))
      continue;

    if (findex_end_index > end_index) {
      findex_new = g_new (SwfdecFormatIndex, 1);
      findex_new->index_ = end_index;
      findex_new->format = swfdec_text_format_copy (findex->format);
      if (findex_new->format == NULL) {
	g_free (findex_new);
	break;
      }

      iter = g_slist_insert (iter, findex_new, 1);
    }

    if (findex->index_ < start_index) {
      findex_new = g_new (SwfdecFormatIndex, 1);
      findex_new->index_ = start_index;
      findex_new->format = swfdec_text_format_copy (findex->format);
      if (findex_new->format == NULL) {
	g_free (findex_new);
	break;
      }
      swfdec_text_format_add (findex_new->format, format);

      iter = g_slist_insert (iter, findex_new, 1);
      findex = findex_new;
    } else {
      swfdec_text_format_add (findex->format, format);

      // if current format now equals previous one, remove current
      if (findex_prev != NULL &&
	  swfdec_text_format_equal (findex->format, findex_prev->format)) {
	text->formats = g_slist_remove (text->formats, findex);
	findex = findex_prev;
      }
    }

    // if current format now equals the next one, remove current
    if (findex_end_index <= end_index && next != NULL &&
	swfdec_text_format_equal (findex->format,
	  ((SwfdecFormatIndex *)next->data)->format))
    {
      ((SwfdecFormatIndex *)next->data)->index_ = findex->index_;
      text->formats = g_slist_remove (text->formats, findex);
      findex = findex_prev;
    }
  }
}

SwfdecTextFormat *
swfdec_text_field_movie_get_text_format (SwfdecTextFieldMovie *text,
    guint start_index, guint end_index)
{
  SwfdecTextFormat *format;
  GSList *iter;

  g_assert (SWFDEC_IS_TEXT_FIELD_MOVIE (text));
  g_assert (start_index < end_index);
  g_assert (end_index <= text->input->len);

  g_assert (text->formats != NULL);
  g_assert (text->formats->data != NULL);
  g_assert (((SwfdecFormatIndex *)text->formats->data)->index_ == 0);

  format = NULL;
  for (iter = text->formats; iter != NULL &&
      ((SwfdecFormatIndex *)iter->data)->index_ < end_index;
      iter = iter->next)
  {
    if (iter->next != NULL &&
	((SwfdecFormatIndex *)iter->next->data)->index_ <= start_index)
      continue;

    if (format == NULL) {
      swfdec_text_format_init_properties (SWFDEC_AS_OBJECT (text)->context);
      format =
	swfdec_text_format_copy (((SwfdecFormatIndex *)iter->data)->format);
    } else {
      swfdec_text_format_remove_different (format,
	  ((SwfdecFormatIndex *)iter->data)->format);
    }
  }

  return format;
}

static void
swfdec_text_field_movie_parse_listen_variable (SwfdecTextFieldMovie *text,
    const char *variable, SwfdecAsObject **object, const char **name)
{
  SwfdecAsContext *cx;
  SwfdecAsObject *parent;
  const char *p1, *p2;

  g_return_if_fail (SWFDEC_IS_TEXT_FIELD_MOVIE (text));
  g_return_if_fail (variable != NULL);
  g_return_if_fail (object != NULL);
  g_return_if_fail (name != NULL);

  *object = NULL;
  *name = NULL;

  if (SWFDEC_MOVIE (text)->parent == NULL)
    return;

  g_assert (SWFDEC_IS_AS_OBJECT (SWFDEC_MOVIE (text)->parent));
  cx = SWFDEC_AS_OBJECT (text)->context;
  parent = SWFDEC_AS_OBJECT (SWFDEC_MOVIE (text)->parent);

  p1 = strrchr (variable, '.');
  p2 = strrchr (variable, ':');
  if (p1 == NULL && p2 == NULL) {
    *object = parent;
    *name = variable;
  } else {
    if (p1 == NULL || (p2 != NULL && p2 > p1))
      p1 = p2;
    if (strlen (p1) == 1)
      return;
    *object = swfdec_action_lookup_object (cx, parent, variable, p1);
    if (*object == NULL)
      return;
    *name = swfdec_as_context_get_string (cx, p1 + 1);
  }
}

void
swfdec_text_field_movie_set_listen_variable_text (SwfdecTextFieldMovie *text,
    const char *value)
{
  SwfdecAsObject *object;
  const char *name;

  g_return_if_fail (SWFDEC_IS_TEXT_FIELD_MOVIE (text));
  g_return_if_fail (text->variable != NULL);
  g_return_if_fail (value != NULL);

  swfdec_text_field_movie_parse_listen_variable (text, text->variable,
      &object, &name);
  if (object != NULL) {
    SwfdecAsValue val;
    SWFDEC_AS_VALUE_SET_STRING (&val, value);
    swfdec_as_object_set_variable (object, name, &val);
  }
}

static void
swfdec_text_field_movie_variable_listener_callback (SwfdecAsObject *object,
    const char *name, const SwfdecAsValue *val)
{
  SwfdecTextFieldMovie *text;

  g_return_if_fail (SWFDEC_IS_TEXT_FIELD_MOVIE (object));

  text = SWFDEC_TEXT_FIELD_MOVIE (object);
  swfdec_text_field_movie_set_text (text,
      swfdec_as_value_to_string (object->context, val), text->text->html);
}

void
swfdec_text_field_movie_set_listen_variable (SwfdecTextFieldMovie *text,
    const char *value)
{
  SwfdecAsObject *object;
  const char *name;

  // FIXME: case-insensitive when v < 7?
  if (text->variable == value)
    return;

  if (text->variable != NULL) {
    swfdec_text_field_movie_parse_listen_variable (text, text->variable,
	&object, &name);
    if (object != NULL && SWFDEC_IS_MOVIE (object)) {
      swfdec_movie_remove_variable_listener (SWFDEC_MOVIE (object),
	  SWFDEC_AS_OBJECT (text), name,
	  swfdec_text_field_movie_variable_listener_callback);
    }
  }

  text->variable = value;

  if (value != NULL) {
    SwfdecAsValue val;

    swfdec_text_field_movie_parse_listen_variable (text, value, &object,
	&name);
    if (object != NULL && swfdec_as_object_get_variable (object, name, &val)) {
      swfdec_text_field_movie_set_text (text,
	  swfdec_as_value_to_string (SWFDEC_AS_OBJECT (text)->context, &val),
	  text->text->html);
    }
    if (object != NULL && SWFDEC_IS_MOVIE (object)) {
      swfdec_movie_add_variable_listener (SWFDEC_MOVIE (object),
	  SWFDEC_AS_OBJECT (text), name,
	  swfdec_text_field_movie_variable_listener_callback);
    }
  }
}

const char *
swfdec_text_field_movie_get_text (SwfdecTextFieldMovie *text)
{
  char *str, *p;

  str = g_strdup (text->input->str);

  // if input was orginally html, remove all \r
  if (text->input_html) {
    p = str;
    while ((p = strchr (p, '\r')) != NULL) {
      memmove (p, p + 1, strlen (p));
    }
  }

  // change all \n to \r
  p = str;
  while ((p = strchr (p, '\n')) != NULL) {
    *p = '\r';
  }

  return swfdec_as_context_give_string (SWFDEC_AS_OBJECT (text)->context, str);
}

void
swfdec_text_field_movie_replace_text (SwfdecTextFieldMovie *text,
    guint start_index, guint end_index, const char *str)
{
  SwfdecFormatIndex *findex;
  GSList *iter, *prev;
  gboolean first;

  g_return_if_fail (SWFDEC_IS_TEXT_FIELD_MOVIE (text));
  g_return_if_fail (end_index <= text->input->len);
  g_return_if_fail (start_index <= end_index);
  g_return_if_fail (str != NULL);

  /* if there was a style sheet set when setting the text, modifications are
   * not allowed */
  if (text->style_sheet_input)
    return;

  first = TRUE;
  prev = NULL;
  for (iter = text->formats; iter != NULL; iter = iter->next)
  {
    findex = iter->data;

    if (findex->index_ >= start_index) {
      if (end_index == text->input->len ||(iter->next != NULL &&
	   ((SwfdecFormatIndex *)iter->next->data)->index_ <= end_index))
      {
	g_free (iter->data);
	text->formats = g_slist_remove (text->formats, iter->data);
	iter = (prev != NULL ? prev : text->formats);
      }
      else
      {
	findex->index_ += strlen (str) - (end_index - start_index);
	if (first) {
	  findex->index_ -= strlen (str);
	  first = FALSE;
	}
      }
    }
    prev = iter;
  }

  if (end_index == text->input->len) {
    if (SWFDEC_AS_OBJECT (text)->context->version < 8) {
      SWFDEC_FIXME ("replaceText to the end of the TextField might use wrong text format on version 7");
    }
    findex = g_new0 (SwfdecFormatIndex, 1);
    findex->index_ = start_index;
    findex->format = swfdec_text_format_copy (
	((SwfdecFormatIndex *)text->formats->data)->format);
    text->formats = g_slist_append (text->formats, findex);
  }

  text->input = g_string_erase (text->input, start_index,
      end_index - start_index);
  text->input = g_string_insert (text->input, start_index, str);

  swfdec_movie_invalidate_last (SWFDEC_MOVIE (text));
  swfdec_text_field_movie_auto_size (text);
  swfdec_text_field_movie_update_scroll (text, TRUE);
}

void
swfdec_text_field_movie_set_text (SwfdecTextFieldMovie *text, const char *str,
    gboolean html)
{
  SwfdecFormatIndex *block;
  GSList *iter;

  g_return_if_fail (SWFDEC_IS_TEXT_FIELD_MOVIE (text));
  g_return_if_fail (str != NULL);

  if (text->format_new == NULL) {
    text->input = g_string_truncate (text->input, 0);
    return;
  }

  // remove old formatting info
  iter = text->formats;
  while (iter) {
    g_free (iter->data);
    iter = g_slist_next (iter);
  }
  g_slist_free (text->formats);
  text->formats = NULL;

  // add the default style
  if (html && SWFDEC_AS_OBJECT (text)->context->version < 8)
    swfdec_text_format_set_defaults (text->format_new);
  block = g_new (SwfdecFormatIndex, 1);
  block->index_ = 0;
  g_assert (SWFDEC_IS_TEXT_FORMAT (text->format_new));
  block->format = swfdec_text_format_copy (text->format_new);
  if (block->format == NULL) {
    g_free (block);
    text->input = g_string_truncate (text->input, 0);
    return;
  }
  text->formats = g_slist_prepend (text->formats, block);

  text->input_html = html;

  if (SWFDEC_AS_OBJECT (text)->context->version >= 7 &&
      text->style_sheet != NULL)
  {
    text->style_sheet_input = str;
    swfdec_text_field_movie_html_parse (text, str);
  }
  else
  {
    text->style_sheet_input = NULL;
    if (html) {
      swfdec_text_field_movie_html_parse (text, str);
    } else {
      text->input = g_string_assign (text->input, str);
    }
  }

  swfdec_movie_invalidate_last (SWFDEC_MOVIE (text));
  swfdec_text_field_movie_auto_size (text);
  swfdec_text_field_movie_update_scroll (text, TRUE);
}