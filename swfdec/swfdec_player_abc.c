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

#include "swfdec_player_internal.h"

#include "swfdec_abc_internal.h"
#include "swfdec_resource.h"

void swfdec_player_abc_trace (SwfdecAsObject *global, guint argc, SwfdecAsValue *argv);
SWFDEC_ABC_FLASH (0, swfdec_player_abc_trace)
void
swfdec_player_abc_trace (SwfdecAsObject *global, guint argc, SwfdecAsValue *argv)
{
  SwfdecAsContext *cx = swfdec_gc_object_get_context (global);
  GString *string = g_string_new ("");
  guint i;

  for (i = 1; i <= argc; i++) {
    const char *s = swfdec_as_value_to_string (cx, &argv[i]);
    if (i > 1)
      g_string_append_c (string, ' ');
    g_string_append (string, s);
  }
  g_signal_emit_by_name (cx, "trace", string->str);
  g_string_free (string, TRUE);
}

void swfdec_player_abc_fscommand (SwfdecAsObject *system, const char *command, const char *target);
SWFDEC_ABC_FLASH (203, swfdec_player_abc_fscommand)
void
swfdec_player_abc_fscommand (SwfdecAsObject *system, const char *command, const char *target)
{
  SwfdecAsContext *cx = swfdec_gc_object_get_context (system);
  /* FIXME: emit here or (as in older Flash) later? */
  g_signal_emit_by_name (cx, "fscommand", command, target);
}
