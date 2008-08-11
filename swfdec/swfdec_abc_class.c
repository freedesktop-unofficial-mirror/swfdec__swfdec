/* Swfdec
 * Copyright (C) 2007 Benjamin Otte <otte@gnome.org>
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

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "swfdec_abc_class.h"
#include "swfdec_abc_function.h"
#include "swfdec_as_context.h"
#include "swfdec_debug.h"

G_DEFINE_TYPE (SwfdecAbcClass, swfdec_abc_class, SWFDEC_TYPE_ABC_OBJECT)

static void
swfdec_abc_class_dispose (GObject *object)
{
  //SwfdecAbcClass *class = SWFDEC_ABC_CLASS (object);

  G_OBJECT_CLASS (swfdec_abc_class_parent_class)->dispose (object);
}

static void
swfdec_abc_class_mark (SwfdecGcObject *object)
{
  //SwfdecAbcClass *class = SWFDEC_ABC_CLASS (object);

  SWFDEC_GC_OBJECT_CLASS (swfdec_abc_class_parent_class)->mark (object);
}

static void
swfdec_abc_class_class_init (SwfdecAbcClassClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_abc_class_dispose;

  gc_class->mark = swfdec_abc_class_mark;
}

static void
swfdec_abc_class_init (SwfdecAbcClass *class)
{
}

