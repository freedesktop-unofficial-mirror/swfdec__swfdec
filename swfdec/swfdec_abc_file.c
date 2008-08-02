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

/*** SWFDEC_ABC_FILE ***/

G_DEFINE_TYPE (SwfdecAbcFile, swfdec_abc_file, SWFDEC_TYPE_GC_OBJECT)

static void
swfdec_abc_file_dispose (GObject *object)
{
  SwfdecAbcFile *file = SWFDEC_ABC_FILE (object);
  SwfdecAsContext *context = swfdec_gc_object_get_context (file);

  if (file->n_ints) {
    swfdec_as_context_unuse_mem (context, file->n_ints * sizeof (int));
    g_free (file->ints);
  }
  if (file->n_uints) {
    swfdec_as_context_unuse_mem (context, file->n_uints * sizeof (guint));
    g_free (file->uints);
  }
  if (file->n_doubles) {
    swfdec_as_context_unuse_mem (context, file->n_doubles * sizeof (double));
    g_free (file->doubles);
  }

  G_OBJECT_CLASS (swfdec_abc_file_parent_class)->dispose (object);
}

static void
swfdec_abc_file_class_init (SwfdecAbcFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_abc_file_dispose;
}

static void
swfdec_abc_file_init (SwfdecAbcFile *date)
{
}

/*** PARSING ***/

#define READ_U30(x, bits) G_STMT_START{ \
  x = swfdec_bits_get_vu32 (bits); \
  if (x >= (1 << 30)) \
    return FALSE; \
}G_STMT_END;

static gboolean
swfdec_abc_file_parse_46 (SwfdecAbcFile *file, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (file);
  guint i;

  SWFDEC_LOG ("parsing ABC block");
  /* read all integers */
  READ_U30 (file->n_ints, bits);
  if (file->n_ints) {
    if (!swfdec_as_context_try_use_mem (context, file->n_ints * sizeof (int))) {
      file->n_ints = 0;
      return FALSE;
    }
    file->ints = g_new0 (int, file->n_ints);
    for (i = 1; i < file->n_ints && swfdec_bits_left (bits); i++) {
      file->ints[i] = swfdec_bits_get_vs32 (bits);
      SWFDEC_LOG ("  int %u: %d", i, file->ints[i]);
    }
  }

  /* read all unsigned integers */
  READ_U30 (file->n_uints, bits);
  if (file->n_uints) {
    if (!swfdec_as_context_try_use_mem (context, file->n_uints * sizeof (guint))) {
      file->n_uints = 0;
      return FALSE;
    }
    file->uints = g_new0 (guint, file->n_uints);
    for (i = 1; i < file->n_uints && swfdec_bits_left (bits); i++) {
      file->uints[i] = swfdec_bits_get_vu32 (bits);
      SWFDEC_LOG ("  uint %u: %u", i, file->uints[i]);
    }
  }

  /* read all doubles */
  READ_U30 (file->n_doubles, bits);
  if (file->n_doubles) {
    if (!swfdec_as_context_try_use_mem (context, file->n_doubles * sizeof (double))) {
      file->n_doubles = 0;
      return FALSE;
    }
    file->doubles = g_new0 (double, file->n_doubles);
    for (i = 1; i < file->n_doubles && swfdec_bits_left (bits); i++) {
      file->doubles[i] = swfdec_bits_get_double (bits);
      SWFDEC_LOG ("  double %u: %g", i, file->doubles[i]);
    }
  }

  return TRUE;
}

SwfdecAbcFile *
swfdec_abc_file_new (SwfdecAsContext *context, SwfdecBits *bits)
{
  SwfdecAbcFile *file;
  guint major, minor;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), NULL);
  g_return_val_if_fail (bits != NULL, NULL);

  file = g_object_new (SWFDEC_TYPE_ABC_FILE, "context", context, NULL);

  minor = swfdec_bits_get_u16 (bits);
  major = swfdec_bits_get_u16 (bits);
  SWFDEC_LOG ("  major version: %u", major);
  SWFDEC_LOG ("  minor version: %u", minor);
  if (major == 46 && minor == 16) {
    if (!swfdec_abc_file_parse_46 (file, bits)) {
      swfdec_as_context_throw_abc (context, SWFDEC_ABC_ERROR_VERIFY,
	  "The ABC data is corrupt, attempt to read out of bounds.");
      return NULL;
    }
  } else {
    swfdec_as_context_throw_abc (context, SWFDEC_ABC_ERROR_VERIFY,
	"Not an ABC file.  major_version=%u minor_version=%u.", major, minor);
    return NULL;
  }

  return file;
}
