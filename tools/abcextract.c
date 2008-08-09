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

#include <stdlib.h>

#include <swfdec/swfdec.h>
#include <swfdec/swfdec_abc_file.h>
#include <swfdec/swfdec_abc_global.h>
#include <swfdec/swfdec_bits.h>

static void
extract (SwfdecAsContext *cx, SwfdecBuffer *buffer, gsize offset)
{
  SwfdecAbcFile *file;
  SwfdecBits bits;

  g_print ("extracting data at offset %"G_GSIZE_FORMAT"...\n", offset);
  swfdec_bits_init (&bits, buffer);
  swfdec_bits_skip_bytes (&bits, offset);
  file = swfdec_abc_file_new (cx, &bits);
  if (file) {
    g_print ("  SUCCESS (%u bytes)\n", bits.ptr - buffer->data - offset);
  } else {
    g_print ("  failed\n");
  }
}

int
main (int argc, char **argv)
{
  char **filenames = NULL;
  const GOptionEntry entries[] = {
    { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, "<INPUT FILE>" },
    { NULL }
  };
  GOptionContext *context;
  GPtrArray *offsets;
  SwfdecAsContext *cx;
  GError *err;
  SwfdecBuffer *buffer;
  gsize i;

  swfdec_init ();

  context = g_option_context_new ("Run a Flash file trying to crash Swfdec");
  g_option_context_add_main_entries (context, entries, NULL);

  if (g_option_context_parse (context, &argc, &argv, &err) == FALSE) {
    g_printerr ("Couldn't parse command-line options: %s\n", err->message);
    g_error_free (err);
    return 1;
  }

  if (filenames == NULL || filenames[0] == NULL) {
    g_printerr ("usage: %s FILENAME\n", argv[0]);
    return 1;
  }

  buffer = swfdec_buffer_new_from_file (filenames[0], &err);
  if (buffer == NULL) {
    g_printerr ("%s\n", err->message);
    g_error_free (err);
    return 1;
  }

  cx = g_object_new (SWFDEC_TYPE_AS_CONTEXT, NULL);
  cx->global = swfdec_abc_global_new (cx);
  offsets = g_ptr_array_new ();
  for (i = 0; i < buffer->length - 3; i++) {
    if (buffer->data[i] != 0x10 ||
        buffer->data[i + 1] != 0x00 ||
        buffer->data[i + 2] != 0x2E ||
        buffer->data[i + 3] != 0x00)
      continue;
    g_print ("found data (index %u) at offset %u\n", offsets->len, i);
    g_ptr_array_add (offsets, GSIZE_TO_POINTER (i));
  }

  if (filenames[1] == NULL) {
    for (i = 0; i < offsets->len; i++) {
      extract (cx, buffer, GPOINTER_TO_SIZE (g_ptr_array_index (offsets, i)));
    }
  } else {
    for (i = 1; filenames[i]; i++) {
      guint id = strtoul (filenames[i], NULL, 10);
      if (id >= offsets->len) {
	g_printerr ("Cannot extract data index %u, only %u indexes available\n",
	    id, offsets->len);
	return 1;
      }
      extract (cx, buffer, GPOINTER_TO_SIZE (g_ptr_array_index (offsets, id)));
    }
  }

  g_object_unref (cx);
  g_strfreev (filenames);
  return 0;
}
