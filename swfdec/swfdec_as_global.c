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

#include "swfdec_as_global.h"

#include <math.h>

#include "swfdec_as_context.h"
#include "swfdec_as_initialize.h"
#include "swfdec_as_internal.h"
#include "swfdec_as_strings.h"
#include "swfdec_debug.h"
#include "swfdec_internal.h"

G_DEFINE_TYPE (SwfdecAsGlobal, swfdec_as_global, SWFDEC_TYPE_AS_OBJECT)

static void
swfdec_as_global_mark (SwfdecGcObject *object)
{
  SwfdecAsGlobal *global = SWFDEC_AS_GLOBAL (object);

  swfdec_gc_object_mark (global->Function);
  swfdec_gc_object_mark (global->Function_prototype);
  swfdec_gc_object_mark (global->Object);
  swfdec_gc_object_mark (global->Object_prototype);

  SWFDEC_GC_OBJECT_CLASS (swfdec_as_global_parent_class)->mark (object);
}

static GObject *
swfdec_as_global_constructor (GType type, guint n_construct_properties,
    GObjectConstructParam *construct_properties)
{
  GObject *object;
  SwfdecAsContext *context;
  SwfdecAsObject *global;
  SwfdecAsValue val;

  object = G_OBJECT_CLASS (swfdec_as_global_parent_class)->constructor (type, 
      n_construct_properties, construct_properties);

  global = SWFDEC_AS_OBJECT (object);
  context = swfdec_gc_object_get_context (object);

  context->global = global;

  /* init the two internal functions */
  /* FIXME: remove them for normal contexts? */
  swfdec_player_preinit_global (context);

  SWFDEC_AS_VALUE_SET_NUMBER (&val, NAN);
  swfdec_as_object_set_variable (global, SWFDEC_AS_STR_NaN, &val);
  SWFDEC_AS_VALUE_SET_NUMBER (&val, HUGE_VAL);
  swfdec_as_object_set_variable (global, SWFDEC_AS_STR_Infinity, &val);

  swfdec_as_function_init_context (context);
  swfdec_as_object_init_context (context);

  /* run init script */
  swfdec_as_context_run_init_script (context, swfdec_as_initialize, sizeof (swfdec_as_initialize), 8);
  return object;
}

static void
swfdec_as_global_class_init (SwfdecAsGlobalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->constructor = swfdec_as_global_constructor;

  gc_class->mark = swfdec_as_global_mark;
}

static void
swfdec_as_global_init (SwfdecAsGlobal *global)
{
}

