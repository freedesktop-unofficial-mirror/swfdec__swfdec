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

#include <string.h>
#include "swfdec_as_internal.h"
#include "swfdec_as_strings.h"
#include "swfdec_debug.h"
#include "swfdec_player_internal.h"

/*** AS CODE ***/

SWFDEC_AS_NATIVE (666, 1, get_scaleMode)
void
get_scaleMode (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  SwfdecPlayer *player = SWFDEC_PLAYER (cx);

  switch (player->scale_mode) {
    case SWFDEC_SCALE_SHOW_ALL:
      SWFDEC_AS_VALUE_SET_STRING (ret, SWFDEC_AS_STR_showAll);
      break;
    case SWFDEC_SCALE_NO_BORDER:
      SWFDEC_AS_VALUE_SET_STRING (ret, SWFDEC_AS_STR_noBorder);
      break;
    case SWFDEC_SCALE_EXACT_FIT:
      SWFDEC_AS_VALUE_SET_STRING (ret, SWFDEC_AS_STR_exactFit);
      break;
    case SWFDEC_SCALE_NONE:
      SWFDEC_AS_VALUE_SET_STRING (ret, SWFDEC_AS_STR_noScale);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

SWFDEC_AS_NATIVE (666, 2, set_scaleMode)
void
set_scaleMode (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  SwfdecPlayer *player = SWFDEC_PLAYER (cx);
  const char *s;
  SwfdecScaleMode mode;

  if (argc == 0)
    return;
  s = swfdec_as_value_to_string (cx, &argv[0]);
  if (g_ascii_strcasecmp (s, SWFDEC_AS_STR_noBorder) == 0) {
    mode = SWFDEC_SCALE_NO_BORDER;
  } else if (g_ascii_strcasecmp (s, SWFDEC_AS_STR_exactFit) == 0) {
    mode = SWFDEC_SCALE_EXACT_FIT;
  } else if (g_ascii_strcasecmp (s, SWFDEC_AS_STR_noScale) == 0) {
    mode = SWFDEC_SCALE_NONE;
  } else {
    mode = SWFDEC_SCALE_SHOW_ALL;
  }
  swfdec_player_set_scale_mode (player, mode);
}

SWFDEC_AS_NATIVE (666, 3, get_align)
void
get_align (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  SwfdecPlayer *player = SWFDEC_PLAYER (cx);
  char s[5];
  guint i = 0;

  if (player->align_flags & SWFDEC_ALIGN_FLAG_LEFT)
    s[i++] = 'L';
  if (player->align_flags & SWFDEC_ALIGN_FLAG_TOP)
    s[i++] = 'T';
  if (player->align_flags & SWFDEC_ALIGN_FLAG_RIGHT)
    s[i++] = 'R';
  if (player->align_flags & SWFDEC_ALIGN_FLAG_BOTTOM)
    s[i++] = 'B';
  s[i] = 0;
  SWFDEC_AS_VALUE_SET_STRING (ret, swfdec_as_context_get_string (cx, s));
}

SWFDEC_AS_NATIVE (666, 4, set_align)
void
set_align (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  SwfdecPlayer *player = SWFDEC_PLAYER (cx);
  guint flags = 0;
  const char *s;

  if (argc == 0)
    return;

  s = swfdec_as_value_to_string (cx, &argv[0]);
  if (strchr (s, 'l') || strchr (s, 'L'))
    flags |= SWFDEC_ALIGN_FLAG_LEFT;
  if (strchr (s, 't') || strchr (s, 'T'))
    flags |= SWFDEC_ALIGN_FLAG_TOP;
  if (strchr (s, 'r') || strchr (s, 'R'))
    flags |= SWFDEC_ALIGN_FLAG_RIGHT;
  if (strchr (s, 'b') || strchr (s, 'B'))
    flags |= SWFDEC_ALIGN_FLAG_BOTTOM;

  if (flags != player->align_flags) {
    player->align_flags = flags;
    g_object_notify (G_OBJECT (player), "alignment");
  }
}

SWFDEC_AS_NATIVE (666, 5, get_width)
void
get_width (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  SwfdecPlayer *player = SWFDEC_PLAYER (cx);

  SWFDEC_AS_VALUE_SET_INT (ret, player->internal_width);
}

SWFDEC_AS_NATIVE (666, 7, get_height)
void
get_height (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  SwfdecPlayer *player = SWFDEC_PLAYER (cx);

  SWFDEC_AS_VALUE_SET_INT (ret, player->internal_height);
}

/* FIXME: do this smarter */
SWFDEC_AS_NATIVE (666, 6, set_width)
void
set_width (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
}

SWFDEC_AS_NATIVE (666, 8, set_height)
void
set_height (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
}

