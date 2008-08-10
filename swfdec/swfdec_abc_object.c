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

#include "swfdec_abc_object.h"
#include "swfdec_as_context.h"
#include "swfdec_debug.h"

G_DEFINE_ABSTRACT_TYPE (SwfdecAbcObject, swfdec_abc_object, SWFDEC_TYPE_AS_OBJECT)

static void
swfdec_abc_object_mark (SwfdecGcObject *gcobject)
{
  SwfdecAbcObject *object = SWFDEC_ABC_OBJECT (gcobject);

  swfdec_gc_object_mark (object->traits);

  SWFDEC_GC_OBJECT_CLASS (swfdec_abc_object_parent_class)->mark (gcobject);
}

static void
swfdec_abc_object_class_init (SwfdecAbcObjectClass *klass)
{
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  gc_class->mark = swfdec_abc_object_mark;
}

static void
swfdec_abc_object_init (SwfdecAbcObject *function)
{
}

SwfdecAbcObject *
swfdec_abc_object_new (SwfdecAsContext *context, SwfdecAbcTraits *traits)
{
  SwfdecAbcObject *object;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), NULL);
  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), NULL);

  if (!swfdec_abc_traits_resolve (traits))
    return NULL;

  object = g_object_new (traits->type_func (), "context", context, NULL);
  object->traits = traits;

  return object;
}

