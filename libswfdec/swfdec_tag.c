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

#include <zlib.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "swfdec_tag.h"
#include "swfdec_bits.h"
#include "swfdec_button.h"
#include "swfdec_debug.h"
#include "swfdec_text_field.h"
#include "swfdec_filter.h"
#include "swfdec_font.h"
#include "swfdec_image.h"
#include "swfdec_morphshape.h"
#include "swfdec_movie.h" /* for SwfdecContent */
#include "swfdec_pattern.h"
#include "swfdec_player_internal.h"
#include "swfdec_script_internal.h"
#include "swfdec_shape.h"
#include "swfdec_sound.h"
#include "swfdec_sprite.h"
#include "swfdec_text.h"
#include "swfdec_video.h"

static int
tag_func_end (SwfdecSwfDecoder * s, guint tag)
{
  return SWFDEC_STATUS_OK;
}

static int
tag_func_protect (SwfdecSwfDecoder * s, guint tag)
{
  if (s->protection) {
    SWFDEC_INFO ("This file is really protected.");
    g_free (s->password);
    s->password = NULL;
  }
  s->protection = TRUE;
  if (swfdec_bits_left (&s->b)) {
    /* FIXME: What's this for? */
    swfdec_bits_get_u16 (&s->b);
    s->password = swfdec_bits_get_string (&s->b);
  }
  return SWFDEC_STATUS_OK;
}

static int
tag_func_frame_label (SwfdecSwfDecoder * s, guint tag)
{
  SwfdecSpriteFrame *frame = &s->parse_sprite->frames[s->parse_sprite->parse_frame];
  
  if (frame->label) {
    SWFDEC_WARNING ("frame %d already has a label (%s)", s->parse_sprite->parse_frame, frame->label);
    g_free (frame->label);
  }
  frame->label = swfdec_bits_get_string (&s->b);
  SWFDEC_LOG ("frame %d named %s", s->parse_sprite->parse_frame, frame->label);

  return SWFDEC_STATUS_OK;
}


/* text */

int
tag_func_define_text (SwfdecSwfDecoder * s, guint tag)
{
  SwfdecBits *bits = &s->b;
  int id;
  int n_glyph_bits;
  int n_advance_bits;
  SwfdecText *text = NULL;
  SwfdecTextGlyph glyph = { 0 };

  id = swfdec_bits_get_u16 (bits);
  text = swfdec_swf_decoder_create_character (s, id, SWFDEC_TYPE_TEXT);
  if (!text)
    return SWFDEC_STATUS_OK;

  glyph.color = 0xffffffff;

  swfdec_bits_get_rect (bits, &SWFDEC_GRAPHIC (text)->extents);
  swfdec_bits_get_matrix (bits, &text->transform, &text->transform_inverse);
  swfdec_bits_syncbits (bits);
  n_glyph_bits = swfdec_bits_get_u8 (bits);
  n_advance_bits = swfdec_bits_get_u8 (bits);

  //printf("  n_glyph_bits = %d\n", n_glyph_bits);
  //printf("  n_advance_bits = %d\n", n_advance_bits);

  while (swfdec_bits_peekbits (bits, 8) != 0) {
    int type;

    type = swfdec_bits_getbit (bits);
    if (type == 0) {
      /* glyph record */
      int n_glyphs;
      int i;

      n_glyphs = swfdec_bits_getbits (bits, 7);
      if (glyph.font == NULL)
	SWFDEC_ERROR ("no font for %d glyphs", n_glyphs);
      for (i = 0; i < n_glyphs; i++) {
        glyph.glyph = swfdec_bits_getbits (bits, n_glyph_bits);

	if (glyph.font != NULL)
	  g_array_append_val (text->glyphs, glyph);
        glyph.x += swfdec_bits_getsbits (bits, n_advance_bits);
      }
    } else {
      /* state change */
      int reserved;
      int has_font;
      int has_color;
      int has_y_offset;
      int has_x_offset;

      reserved = swfdec_bits_getbits (bits, 3);
      has_font = swfdec_bits_getbit (bits);
      has_color = swfdec_bits_getbit (bits);
      has_y_offset = swfdec_bits_getbit (bits);
      has_x_offset = swfdec_bits_getbit (bits);
      if (has_font) {
        glyph.font = swfdec_swf_decoder_get_character (s, swfdec_bits_get_u16 (bits));
        //printf("  font = %d\n",font);
      }
      if (has_color) {
        if (tag == SWFDEC_TAG_DEFINETEXT) {
          glyph.color = swfdec_bits_get_color (bits);
        } else {
          glyph.color = swfdec_bits_get_rgba (bits);
        }
        //printf("  color = %08x\n",glyph.color);
      }
      if (has_x_offset) {
        glyph.x = swfdec_bits_get_s16 (bits);
      }
      if (has_y_offset) {
        glyph.y = swfdec_bits_get_s16 (bits);
      }
      if (has_font) {
        glyph.height = swfdec_bits_get_u16 (bits);
      }
    }
    swfdec_bits_syncbits (bits);
  }
  swfdec_bits_get_u8 (bits);

  return SWFDEC_STATUS_OK;
}

int
tag_func_define_sprite (SwfdecSwfDecoder * s, guint define_sprite_tag)
{
  SwfdecBits parse;
  int id;
  SwfdecSprite *sprite;
  int ret;
  guint tag = 1;

  parse = s->b;

  id = swfdec_bits_get_u16 (&parse);
  sprite = swfdec_swf_decoder_create_character (s, id, SWFDEC_TYPE_SPRITE);
  if (!sprite)
    return SWFDEC_STATUS_OK;

  SWFDEC_LOG ("  ID: %d", id);

  swfdec_sprite_set_n_frames (sprite, swfdec_bits_get_u16 (&parse), SWFDEC_DECODER (s)->rate);

  s->parse_sprite = sprite;
  while (swfdec_bits_left (&parse)) {
    int x;
    guint tag_len;
    SwfdecTagFunc func;

    x = swfdec_bits_get_u16 (&parse);
    tag = (x >> 6) & 0x3ff;
    tag_len = x & 0x3f;
    if (tag_len == 0x3f) {
      tag_len = swfdec_bits_get_u32 (&parse);
    }
    SWFDEC_INFO ("sprite parsing at %td, tag %d %s, length %d",
        parse.buffer ? parse.ptr - parse.buffer->data : 0, tag,
        swfdec_swf_decoder_get_tag_name (tag), tag_len);

    if (tag_len == 0) {
      swfdec_bits_init_data (&s->b, NULL, 0);
    } else {
      swfdec_bits_init_bits (&s->b, &parse, tag_len);
    }

    func = swfdec_swf_decoder_get_tag_func (tag);
    if (tag == 0) {
      break;
    } else if (func == NULL) {
      SWFDEC_WARNING ("tag function not implemented for %d %s",
          tag, swfdec_swf_decoder_get_tag_name (tag));
    } else if ((swfdec_swf_decoder_get_tag_flag (tag) & 1) == 0) {
      SWFDEC_ERROR ("invalid tag %d %s during DefineSprite",
          tag, swfdec_swf_decoder_get_tag_name (tag));
    } else if (s->parse_sprite->parse_frame < s->parse_sprite->n_frames) {
      ret = func (s, tag);

      if (swfdec_bits_left (&s->b)) {
        SWFDEC_WARNING ("early parse finish (%d bytes)", 
	    swfdec_bits_left (&s->b) / 8);
      }
    } else {
      SWFDEC_ERROR ("data after last frame");
    }
  }

  /* sanity check the sprite */
  if (s->parse_sprite->n_frames != s->parse_sprite->parse_frame) {
    SWFDEC_INFO ("not enough frames in sprite %u (have %u, want %u), filling up with empty frames",
	id, s->parse_sprite->parse_frame, s->parse_sprite->n_frames);
    s->parse_sprite->parse_frame = s->parse_sprite->n_frames;
  }

  s->b = parse;
  /* this assumes that no recursive DefineSprite happens and we check it doesn't */
  s->parse_sprite = s->main_sprite;
  SWFDEC_LOG ("done parsing this sprite");

  return SWFDEC_STATUS_OK;
}

#define CONTENT_IN_FRAME(content, frame) \
  ((content)->sequence->start <= frame && \
   (content)->sequence->end > frame)
static guint
swfdec_button_remove_duplicates (SwfdecButton *button, int depth, guint states)
{
  GList *walk;
  guint taken = 0;
  guint i;

  /* 1) find out which states are already taken */
  for (walk = button->records; walk; walk = walk->next) {
    SwfdecContent *cur = walk->data;
    if (cur->depth != depth)
      continue;
    for (i = 0; i < 4; i++) {
      if (CONTENT_IN_FRAME (cur, i))
	taken |= (1 << i);
    }
  }
  /* 2) mark states that overlap */
  taken &= states;
  /* 3) remove the overlapping states */
  if (taken) {
    SWFDEC_ERROR ("overlapping contents in button, removing for depth %u and states %u",
	depth, taken);
    states &= ~taken;
  }
  return states;
}

static void
swfdec_button_append_content (SwfdecButton *button, guint states, SwfdecContent *content)
{
  guint i;
  SwfdecContent *cur = NULL;

  states = swfdec_button_remove_duplicates (button, content->depth, states);

  for (i = 0; i < 4; i++) {
    if (!cur && (states & 1)) {
      cur = content;
      if (content->end != G_MAXUINT) {
	/* need a copy here */
	cur = swfdec_content_new (content->depth);
	*cur = *content;
	cur->sequence = cur;
      }
      cur->start = i;
      button->records = g_list_append (button->records, cur);
    }
    if (cur && !(states & 1)) {
      cur->end = i;
      cur = NULL;
    }
    states >>= 1;
  }
  if (cur) {
    SwfdecRect rect;
    cur->end = 4;
    swfdec_rect_transform (&rect, &content->graphic->extents, &cur->transform);
    swfdec_rect_union (&SWFDEC_GRAPHIC (button)->extents,
	&SWFDEC_GRAPHIC (button)->extents, &rect);
  }
  if (content->end == G_MAXUINT) {
    SWFDEC_ERROR ("button record for graphic %u is not used in any state, discarding", 
	SWFDEC_CHARACTER (content->graphic)->id);
    swfdec_content_free (content);
  }
}

static int
tag_func_define_button_2 (SwfdecSwfDecoder * s, guint tag)
{
  SwfdecBits bits;
  int id, reserved;
  guint length;
  SwfdecButton *button;
  char *script_name;

  id = swfdec_bits_get_u16 (&s->b);
  button = swfdec_swf_decoder_create_character (s, id, SWFDEC_TYPE_BUTTON);
  if (!button)
    return SWFDEC_STATUS_OK;

  SWFDEC_LOG ("  ID: %d", id);

  reserved = swfdec_bits_getbits (&s->b, 7);
  button->menubutton = swfdec_bits_getbit (&s->b) ? TRUE : FALSE;
  length = swfdec_bits_get_u16 (&s->b);

  SWFDEC_LOG ("  reserved = %d", reserved);
  SWFDEC_LOG ("  menu = %d", button->menubutton);
  SWFDEC_LOG ("  length of region = %d", length);

  if (length)
    swfdec_bits_init_bits (&bits, &s->b, length > 2 ? length - 2 : 0);
  else
    swfdec_bits_init_bits (&bits, &s->b, swfdec_bits_left (&s->b) / 8);
  while (swfdec_bits_peek_u8 (&bits)) {
    guint character;
    guint depth;
    guint states;
    gboolean has_blend_mode, has_filters;
    SwfdecContent *content;

    if (s->version >= 8) {
      reserved = swfdec_bits_getbits (&bits, 2);
      has_blend_mode = swfdec_bits_getbit (&bits);
      has_filters = swfdec_bits_getbit (&bits);
      SWFDEC_LOG ("  reserved = %d", reserved);
      SWFDEC_LOG ("  has_blend_mode = %d", has_blend_mode);
      SWFDEC_LOG ("  has_filters = %d", has_filters);
    } else {
      reserved = swfdec_bits_getbits (&bits, 4);
      has_blend_mode = 0;
      has_filters = 0;
      SWFDEC_LOG ("  reserved = %d", reserved);
    }
    states = swfdec_bits_getbits (&bits, 4);
    character = swfdec_bits_get_u16 (&bits);
    depth = swfdec_bits_get_u16 (&bits);

    SWFDEC_LOG ("  states: %s%s%s%scharacter=%u layer=%u",
        states & (1 << SWFDEC_BUTTON_HIT) ? "HIT " : "", 
	states & (1 << SWFDEC_BUTTON_DOWN) ? "DOWN " : "", 
        states & (1 << SWFDEC_BUTTON_OVER) ? "OVER " : "",
	states & (1 << SWFDEC_BUTTON_UP) ? "UP " : "", 
	character, depth);
    content = swfdec_content_new (depth);

    swfdec_bits_get_matrix (&bits, &content->transform, NULL);
    content->has_transform = TRUE;
    SWFDEC_LOG ("matrix: %g %g  %g %g   %g %g",
	content->transform.xx, content->transform.yy, 
	content->transform.xy, content->transform.yx,
	content->transform.x0, content->transform.y0);
    swfdec_bits_get_color_transform (&bits, &content->color_transform);
    content->has_color_transform = TRUE;

    content->graphic = swfdec_swf_decoder_get_character (s, character);
    if (has_blend_mode) {
      content->blend_mode = swfdec_bits_get_u8 (&bits);
      SWFDEC_LOG ("  blend mode = %u", content->blend_mode);
    }
    if (has_filters) {
      GSList *list = swfdec_filter_parse (SWFDEC_DECODER (s)->player, &bits);
      g_slist_free (list);
    }
    if (!SWFDEC_IS_GRAPHIC (content->graphic)) {
      SWFDEC_ERROR ("id %u does not reference a graphic, ignoring", character);
      swfdec_content_free (content);
    } else {
      swfdec_button_append_content (button, states, content);
    }
  }
  swfdec_bits_get_u8 (&bits);
  if (swfdec_bits_left (&bits)) {
    SWFDEC_WARNING ("%u bytes left when parsing button records", swfdec_bits_left (&bits) / 8);
  }

  script_name = g_strdup_printf ("Button%u", SWFDEC_CHARACTER (button)->id);
  while (length != 0) {
    guint condition, key;

    length = swfdec_bits_get_u16 (&s->b);
    if (length)
      swfdec_bits_init_bits (&bits, &s->b, length > 2 ? length - 2 : 0);
    else
      swfdec_bits_init_bits (&bits, &s->b, swfdec_bits_left (&s->b) / 8);
    condition = swfdec_bits_get_u16 (&bits);
    key = condition >> 9;
    condition &= 0x1FF;

    SWFDEC_LOG (" length = %d", length);

    if (button->events == NULL)
      button->events = swfdec_event_list_new (SWFDEC_DECODER (s)->player);
    SWFDEC_LOG ("  new event for condition %u (key %u)", condition, key);
    swfdec_event_list_parse (button->events, &bits, s->version, condition, key,
	script_name);
    if (swfdec_bits_left (&bits)) {
      SWFDEC_WARNING ("%u bytes left after parsing script", swfdec_bits_left (&bits) / 8);
    }
  }
  g_free (script_name);

  return SWFDEC_STATUS_OK;
}

static int
tag_func_define_button (SwfdecSwfDecoder * s, guint tag)
{
  SwfdecBits *bits = &s->b;
  int id;
  SwfdecButton *button;
  char *script_name;

  id = swfdec_bits_get_u16 (bits);
  button = swfdec_swf_decoder_create_character (s, id, SWFDEC_TYPE_BUTTON);
  if (!button)
    return SWFDEC_STATUS_OK;

  SWFDEC_LOG ("  ID: %d", id);

  while (swfdec_bits_peek_u8 (bits)) {
    int reserved;
    guint character;
    guint depth;
    guint states;
    SwfdecContent *content;

    swfdec_bits_syncbits (bits);
    reserved = swfdec_bits_getbits (bits, 4);
    states = swfdec_bits_getbits (bits, 4);
    character = swfdec_bits_get_u16 (bits);
    depth = swfdec_bits_get_u16 (bits);

    SWFDEC_LOG ("  reserved = %d", reserved);
    SWFDEC_LOG ("states: %s%s%s%scharacter=%u layer=%u",
        states & (1 << SWFDEC_BUTTON_HIT) ? "HIT " : "", 
	states & (1 << SWFDEC_BUTTON_DOWN) ? "DOWN " : "", 
        states & (1 << SWFDEC_BUTTON_OVER) ? "OVER " : "",
	states & (1 << SWFDEC_BUTTON_UP) ? "UP " : "", 
	character, depth);
    content = swfdec_content_new (depth);

    swfdec_bits_get_matrix (bits, &content->transform, NULL);
    content->has_transform = TRUE;
    SWFDEC_LOG ("matrix: %g %g  %g %g   %g %g",
	content->transform.xx, content->transform.yy, 
	content->transform.xy, content->transform.yx,
	content->transform.x0, content->transform.y0);

    content->graphic = swfdec_swf_decoder_get_character (s, character);
    if (!SWFDEC_IS_GRAPHIC (content->graphic)) {
      SWFDEC_ERROR ("id %u does not reference a graphic, ignoring", character);
      swfdec_content_free (content);
    } else {
      swfdec_button_append_content (button, states, content);
    }
  }
  swfdec_bits_get_u8 (bits);

  script_name = g_strdup_printf ("Button%u", SWFDEC_CHARACTER (button)->id);
  button->events = swfdec_event_list_new (SWFDEC_DECODER (s)->player);
  swfdec_event_list_parse (button->events, &s->b, s->version, 
      SWFDEC_BUTTON_OVER_UP_TO_OVER_DOWN, 0, script_name);
  g_free (script_name);

  return SWFDEC_STATUS_OK;
}

static int
tag_func_file_attributes (SwfdecSwfDecoder *s, guint tag)
{
  if (swfdec_bits_getbits (&s->b, 3))
    SWFDEC_INFO ("reserved bits (1) aren't 0");
  s->has_metadata = swfdec_bits_getbit (&s->b);
  SWFDEC_LOG ("  has metadata: %d", s->has_metadata);
  if (swfdec_bits_getbits (&s->b, 3))
    SWFDEC_INFO ("reserved bits (2) aren't 0");
  s->use_network = swfdec_bits_getbit (&s->b);
  SWFDEC_LOG ("  use network: %d", s->use_network);
  if (swfdec_bits_getbits (&s->b, 24))
    SWFDEC_INFO ("reserved bits (3) aren't 0");
  /* initialize default security if it wasn't initialized yet */

  return SWFDEC_STATUS_OK;
}

static int
tag_func_export_assets (SwfdecSwfDecoder * s, guint tag)
{
  SwfdecBits *bits = &s->b;
  guint count, i;

  count = swfdec_bits_get_u16 (bits);
  SWFDEC_LOG ("exporting %u assets", count);
  for (i = 0; i < count && swfdec_bits_left (bits); i++) {
    guint id;
    SwfdecCharacter *object;
    char *name;
    id = swfdec_bits_get_u16 (bits);
    object = swfdec_swf_decoder_get_character (s, id);
    name = swfdec_bits_get_string_with_version (bits, s->version);
    if (object == NULL) {
      SWFDEC_ERROR ("cannot export id %u as %s, id wasn't found", id, name);
      g_free (name);
    } else if (name == NULL) {
      SWFDEC_ERROR ("cannot export id %u, no name was given", id);
    } else {
      SwfdecRootExportData *data = g_new (SwfdecRootExportData, 1);
      data->name = name;
      data->character = object;
      SWFDEC_LOG ("exporting %s %u as %s", G_OBJECT_TYPE_NAME (object), id, name);
      g_object_ref (object);
      swfdec_swf_decoder_add_root_action (s, SWFDEC_ROOT_ACTION_EXPORT, data);
    }
  }

  return SWFDEC_STATUS_OK;
}

static int
tag_func_do_init_action (SwfdecSwfDecoder * s, guint tag)
{
  SwfdecBits *bits = &s->b;
  guint id;
  SwfdecSprite *sprite;
  char *name;

  id = swfdec_bits_get_u16 (bits);
  SWFDEC_LOG ("  id = %u", id);
  sprite = swfdec_swf_decoder_get_character (s, id);
  if (!SWFDEC_IS_SPRITE (sprite)) {
    SWFDEC_ERROR ("character %u is not a sprite", id);
    return SWFDEC_STATUS_OK;
  }
  if (sprite->init_action != NULL) {
    SWFDEC_ERROR ("sprite %u already has an init action", id);
    return SWFDEC_STATUS_OK;
  }
  name = g_strdup_printf ("InitAction %u", id);
  sprite->init_action = swfdec_script_new_from_bits (bits, name, s->version);
  g_free (name);
  if (sprite->init_action) {
    swfdec_script_ref (sprite->init_action);
    swfdec_swf_decoder_add_root_action (s, SWFDEC_ROOT_ACTION_INIT_SCRIPT, sprite->init_action);
  }

  return SWFDEC_STATUS_OK;
}

static int
tag_func_enqueue (SwfdecSwfDecoder *s, guint tag)
{
  SwfdecBuffer *buffer;

  buffer = swfdec_bits_get_buffer (&s->b, -1);
  SWFDEC_LOG ("queueing %s tag for sprite %u", swfdec_swf_decoder_get_tag_name (tag),
      SWFDEC_CHARACTER (s->parse_sprite)->id);
  swfdec_sprite_add_action (s->parse_sprite, tag, buffer);

  return SWFDEC_STATUS_OK;
}

static int
tag_func_show_frame (SwfdecSwfDecoder * s, guint tag)
{
  SWFDEC_DEBUG("show_frame %d of id %d", s->parse_sprite->parse_frame,
      SWFDEC_CHARACTER (s->parse_sprite)->id);

  s->parse_sprite->parse_frame++;
  if (s->parse_sprite->parse_frame < s->parse_sprite->n_frames) {
    SwfdecSpriteFrame *old = &s->parse_sprite->frames[s->parse_sprite->parse_frame - 1];
    SwfdecSpriteFrame *new = &s->parse_sprite->frames[s->parse_sprite->parse_frame];
    if (old->sound_head)
      new->sound_head = g_object_ref (old->sound_head);
  }
  tag_func_enqueue (s, tag);

  return SWFDEC_STATUS_IMAGE;
}

static int
tag_func_do_action (SwfdecSwfDecoder * s, guint tag)
{
  SwfdecScript *script;
  SwfdecBits bits;
  char *name;

  name = g_strdup_printf ("Sprite%u_Frame%u", SWFDEC_CHARACTER (s->parse_sprite)->id,
      s->parse_sprite->parse_frame);
  bits = s->b;
  script = swfdec_script_new_from_bits (&bits, name, s->version);
  g_free (name);
  if (script) {
    swfdec_swf_decoder_add_script (s, script);
    tag_func_enqueue (s, tag);
  }

  return SWFDEC_STATUS_OK;
}

struct tag_func_struct
{
  const char *name;
  SwfdecTagFunc func;
  int flag;
};
static struct tag_func_struct tag_funcs[] = {
  [SWFDEC_TAG_END] = {"End", tag_func_end, SWFDEC_TAG_DEFINE_SPRITE },
  [SWFDEC_TAG_SHOWFRAME] = {"ShowFrame", tag_func_show_frame, SWFDEC_TAG_DEFINE_SPRITE },
  [SWFDEC_TAG_DEFINESHAPE] = {"DefineShape", tag_define_shape, 0},
  [SWFDEC_TAG_FREECHARACTER] = {"FreeCharacter", NULL, 0},
  [SWFDEC_TAG_PLACEOBJECT] = {"PlaceObject", NULL, SWFDEC_TAG_DEFINE_SPRITE },
  [SWFDEC_TAG_REMOVEOBJECT] = {"RemoveObject", tag_func_enqueue, SWFDEC_TAG_DEFINE_SPRITE },
  [SWFDEC_TAG_DEFINEBITSJPEG] = {"DefineBitsJPEG", tag_func_define_bits_jpeg, 0},
  [SWFDEC_TAG_DEFINEBUTTON] = {"DefineButton", tag_func_define_button, 0},
  [SWFDEC_TAG_JPEGTABLES] = {"JPEGTables", swfdec_image_jpegtables, 0},
  [SWFDEC_TAG_SETBACKGROUNDCOLOR] =
      {"SetBackgroundColor", tag_func_set_background_color, 0},
  [SWFDEC_TAG_DEFINEFONT] = {"DefineFont", tag_func_define_font, 0},
  [SWFDEC_TAG_DEFINETEXT] = {"DefineText", tag_func_define_text, 0},
  [SWFDEC_TAG_DOACTION] = {"DoAction", tag_func_do_action, SWFDEC_TAG_DEFINE_SPRITE },
  [SWFDEC_TAG_DEFINEFONTINFO] = {"DefineFontInfo", tag_func_define_font_info, 0},
  [SWFDEC_TAG_DEFINESOUND] = {"DefineSound", tag_func_define_sound, 0},
  [SWFDEC_TAG_STARTSOUND] = {"StartSound", tag_func_enqueue, SWFDEC_TAG_DEFINE_SPRITE },
  [SWFDEC_TAG_DEFINEBUTTONSOUND] =
      {"DefineButtonSound", tag_func_define_button_sound, 0},
  [SWFDEC_TAG_SOUNDSTREAMHEAD] = {"SoundStreamHead", tag_func_sound_stream_head, SWFDEC_TAG_DEFINE_SPRITE },
  [SWFDEC_TAG_SOUNDSTREAMBLOCK] = {"SoundStreamBlock", tag_func_sound_stream_block, SWFDEC_TAG_DEFINE_SPRITE },
  [SWFDEC_TAG_DEFINEBITSLOSSLESS] =
      {"DefineBitsLossless", tag_func_define_bits_lossless, 0},
  [SWFDEC_TAG_DEFINEBITSJPEG2] = {"DefineBitsJPEG2", tag_func_define_bits_jpeg_2, 0},
  [SWFDEC_TAG_DEFINESHAPE2] = {"DefineShape2", tag_define_shape, 0},
  [SWFDEC_TAG_DEFINEBUTTONCXFORM] = {"DefineButtonCXForm", NULL, 0},
  [SWFDEC_TAG_PROTECT] = {"Protect", tag_func_protect, 0},
  [SWFDEC_TAG_PLACEOBJECT2] = {"PlaceObject2", tag_func_enqueue, SWFDEC_TAG_DEFINE_SPRITE },
  [SWFDEC_TAG_REMOVEOBJECT2] = {"RemoveObject2", tag_func_enqueue, SWFDEC_TAG_DEFINE_SPRITE },
  [SWFDEC_TAG_DEFINESHAPE3] = {"DefineShape3", tag_define_shape_3, 0},
  [SWFDEC_TAG_DEFINETEXT2] = {"DefineText2", tag_func_define_text, 0},
  [SWFDEC_TAG_DEFINEBUTTON2] = {"DefineButton2", tag_func_define_button_2, 0},
  [SWFDEC_TAG_DEFINEBITSJPEG3] = {"DefineBitsJPEG3", tag_func_define_bits_jpeg_3, 0},
  [SWFDEC_TAG_DEFINEBITSLOSSLESS2] =
      {"DefineBitsLossless2", tag_func_define_bits_lossless_2, 0},
  [SWFDEC_TAG_DEFINEEDITTEXT] = {"DefineEditText", tag_func_define_edit_text, 0},
  [SWFDEC_TAG_DEFINEMOVIE] = {"DefineMovie", NULL, 0},
  [SWFDEC_TAG_DEFINESPRITE] = {"DefineSprite", tag_func_define_sprite, 0},
  [SWFDEC_TAG_NAMECHARACTER] = {"NameCharacter", NULL, 0},
  [SWFDEC_TAG_SERIALNUMBER] = {"SerialNumber", NULL, 0},
  [SWFDEC_TAG_GENERATORTEXT] = {"GeneratorText", NULL, 0},
  [SWFDEC_TAG_FRAMELABEL] = {"FrameLabel", tag_func_frame_label, SWFDEC_TAG_DEFINE_SPRITE },
  [SWFDEC_TAG_SOUNDSTREAMHEAD2] = {"SoundStreamHead2", tag_func_sound_stream_head, SWFDEC_TAG_DEFINE_SPRITE },
  [SWFDEC_TAG_DEFINEMORPHSHAPE] =
      {"DefineMorphShape", tag_define_morph_shape, 0},
  [SWFDEC_TAG_DEFINEFONT2] = {"DefineFont2", tag_func_define_font_2, 0},
  [SWFDEC_TAG_TEMPLATECOMMAND] = {"TemplateCommand", NULL, 0},
  [SWFDEC_TAG_GENERATOR3] = {"Generator3", NULL, 0},
  [SWFDEC_TAG_EXTERNALFONT] = {"ExternalFont", NULL, 0},
  [SWFDEC_TAG_EXPORTASSETS] = {"ExportAssets", tag_func_export_assets, 0},
  [SWFDEC_TAG_IMPORTASSETS] = {"ImportAssets", NULL, 0},
  [SWFDEC_TAG_ENABLEDEBUGGER] = {"EnableDebugger", NULL, 0},
  [SWFDEC_TAG_DOINITACTION] = {"DoInitAction", tag_func_do_init_action, SWFDEC_TAG_DEFINE_SPRITE },
  [SWFDEC_TAG_DEFINEVIDEOSTREAM] = {"DefineVideoStream", tag_func_define_video, 0},
  [SWFDEC_TAG_VIDEOFRAME] = {"VideoFrame", tag_func_video_frame, SWFDEC_TAG_DEFINE_SPRITE },
  [SWFDEC_TAG_DEFINEFONTINFO2] = {"DefineFontInfo2", tag_func_define_font_info, 0},
  [SWFDEC_TAG_MX4] = {"MX4", NULL, 0},
  [SWFDEC_TAG_ENABLEDEBUGGER2] = {"EnableDebugger2", NULL, 0},
  [SWFDEC_TAG_SCRIPTLIMITS] = {"ScriptLimits", NULL, 0},
  [SWFDEC_TAG_SETTABINDEX] = {"SetTabIndex", NULL, 0},
  [SWFDEC_TAG_FILEATTRIBUTES] = {"FileAttributes", tag_func_file_attributes, SWFDEC_TAG_FIRST_ONLY },
  [SWFDEC_TAG_PLACEOBJECT3] = {"PlaceObject3", tag_func_enqueue, SWFDEC_TAG_DEFINE_SPRITE },
  [SWFDEC_TAG_IMPORTASSETS2] = {"ImportAssets2", NULL, 0},
  [SWFDEC_TAG_DEFINEFONTALIGNZONES] = {"DefineFontAlignZones", NULL, 0},
  [SWFDEC_TAG_CSMTEXTSETTINGS] = {"CSMTextSettings", NULL, 0},
  [SWFDEC_TAG_DEFINEFONT3] = {"DefineFont3", tag_func_define_font_3, 0},
  [SWFDEC_TAG_AVM2DECL] = {"AVM2Decl", NULL, 0},
  [SWFDEC_TAG_METADATA] = {"Metadata", NULL, 0},
  [SWFDEC_TAG_DEFINESCALINGGRID] = {"DefineScalingGrid", NULL, 0},
  [SWFDEC_TAG_AVM2ACTION] = {"AVM2Action", NULL, 0},
  [SWFDEC_TAG_DEFINESHAPE4] = {"DefineShape4", tag_define_shape_4, 0},
  [SWFDEC_TAG_DEFINEMORPHSHAPE2] = {"DefineMorphShape2", NULL, 0},
};

static const int n_tag_funcs = sizeof (tag_funcs) / sizeof (tag_funcs[0]);

const char *
swfdec_swf_decoder_get_tag_name (int tag)
{
  if (tag >= 0 && tag < n_tag_funcs) {
    if (tag_funcs[tag].name) {
      return tag_funcs[tag].name;
    }
  }
  
  return "unknown";
}

SwfdecTagFunc
swfdec_swf_decoder_get_tag_func (int tag)
{ 
  if (tag >= 0 && tag < n_tag_funcs) {
    if (tag_funcs[tag].func) {
      return tag_funcs[tag].func;
    }
  }
  
  return NULL;
}

int
swfdec_swf_decoder_get_tag_flag (int tag)
{
  if (tag >= 0 && tag < n_tag_funcs) {
    return tag_funcs[tag].flag;
  }
  
  return 0;
}
