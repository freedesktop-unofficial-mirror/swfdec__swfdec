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

#include "swfdec_abc_file.h"
#include "swfdec_abc_internal.h"
#include "swfdec_debug.h"

G_DEFINE_TYPE (SwfdecAbcFile, swfdec_abc_file, G_TYPE_OBJECT)

/*
 * Class functions
 */

static void
swfdec_abc_file_class_init (SwfdecAbcFileClass *klass)
{
}

static void
swfdec_abc_file_init (SwfdecAbcFile *date)
{
}

/*** PARSING ***/

static gboolean
swfdec_abc_file_parse_46 (SwfdecAbcFile *file, SwfdecBits *bits)
{
  return TRUE;
}

SwfdecAbcFile *
swfdec_abc_file_new (SwfdecAsContext *context, SwfdecBits *bits)
{
  SwfdecAbcFile *file;
  guint major, minor;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), NULL);
  g_return_val_if_fail (bits != NULL, NULL);

  file = g_object_new (SWFDEC_TYPE_ABC_FILE, NULL);
  file->context = context;

  minor = swfdec_bits_get_u16 (bits);
  major = swfdec_bits_get_u16 (bits);
  SWFDEC_LOG ("  major version: %u", major);
  SWFDEC_LOG ("  minor version: %u", minor);
  if (major == 46 && minor == 16) {
    if (!swfdec_abc_file_parse_46 (file, bits)) {
      swfdec_as_context_throw_abc (context, SWFDEC_ABC_ERROR_VERIFY,
	  "The ABC data is corrupt, attempt to read out of bounds.");
      goto fail;
    }
  } else {
    swfdec_as_context_throw_abc (context, SWFDEC_ABC_ERROR_VERIFY,
	"Not an ABC file.  major_version=%u minor_version=%u.", major, minor);
    goto fail;
  }

  return file;

fail:
  g_object_unref (file);
  return NULL;
}
