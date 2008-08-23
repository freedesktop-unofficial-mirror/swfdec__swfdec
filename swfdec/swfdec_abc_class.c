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
#include <string.h>
#include <math.h>

#include "swfdec_abc_class.h"

#include "swfdec_abc_function.h"
#include "swfdec_abc_global.h"
#include "swfdec_abc_internal.h"
#include "swfdec_abc_scope_chain.h"
#include "swfdec_abc_traits.h"
#include "swfdec_as_context.h"
#include "swfdec_debug.h"

G_DEFINE_TYPE (SwfdecAbcClass, swfdec_abc_class, SWFDEC_TYPE_ABC_OBJECT)

static GObject *
swfdec_abc_class_constructor (GType type, guint n_construct_properties,
    GObjectConstructParam *construct_properties)
{
  SwfdecAbcClass *classp;
  GObject *object;
  SwfdecAsContext *context;

  object = G_OBJECT_CLASS (swfdec_abc_class_parent_class)->constructor (type, 
      n_construct_properties, construct_properties);

  classp = SWFDEC_ABC_CLASS (object);
  context = swfdec_gc_object_get_context (classp);
  if (classp->prototype == NULL && 
      SWFDEC_ABC_GLOBAL (context->global)->classes[SWFDEC_ABC_TYPE_OBJECT]) {
    classp->prototype = swfdec_abc_object_new_from_class (
	SWFDEC_ABC_GET_OBJECT_CLASS (context));
  }

  return object;
}

static void
swfdec_abc_class_dispose (GObject *object)
{
  SwfdecAbcClass *classp = SWFDEC_ABC_CLASS (object);
  SwfdecAsContext *context = swfdec_gc_object_get_context (classp);

  swfdec_abc_scope_chain_unref (context, classp->instance_scope);

  G_OBJECT_CLASS (swfdec_abc_class_parent_class)->dispose (object);
}

static void
swfdec_abc_class_mark (SwfdecGcObject *object)
{
  SwfdecAbcClass *classp = SWFDEC_ABC_CLASS (object);

  swfdec_abc_scope_chain_mark (classp->instance_scope);

  SWFDEC_GC_OBJECT_CLASS (swfdec_abc_class_parent_class)->mark (object);
}

static gboolean
swfdec_abc_class_call (SwfdecAbcObject *object, guint argc, 
    SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  /* Explicit coercion of a class object. */
  if (argc != 1) {
    swfdec_as_context_throw_abc (swfdec_gc_object_get_context (object),
      SWFDEC_ABC_TYPE_ARGUMENT_ERROR, "Argument count mismatch on class coercion.  Expected 1, got %u.",
      argc);
    return FALSE;
  }

  *ret = argv[1];
  return swfdec_abc_traits_coerce (object->traits->instance_traits, ret);
}

static gboolean
swfdec_abc_class_construct (SwfdecAbcObject *object, guint argc, 
    SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  SwfdecAbcClass *classp = SWFDEC_ABC_CLASS (object);
  SwfdecAbcObject *result;

  result = swfdec_abc_object_new_from_class (classp);
  SWFDEC_AS_OBJECT (result)->prototype = SWFDEC_AS_OBJECT (classp->prototype);
  SWFDEC_AS_VALUE_SET_OBJECT (&argv[0], SWFDEC_AS_OBJECT (result));
  if (!swfdec_abc_function_call (object->traits->instance_traits->construct,
	classp->instance_scope, argc, argv, ret))
    return FALSE;
  SWFDEC_AS_VALUE_SET_OBJECT (ret, SWFDEC_AS_OBJECT (result));
  return TRUE;
}

static void
swfdec_abc_class_class_init (SwfdecAbcClassClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);
  SwfdecAbcObjectClass *abc_class = SWFDEC_ABC_OBJECT_CLASS (klass);

  object_class->constructor = swfdec_abc_class_constructor;
  object_class->dispose = swfdec_abc_class_dispose;

  gc_class->mark = swfdec_abc_class_mark;

  abc_class->call = swfdec_abc_class_call;
  abc_class->construct = swfdec_abc_class_construct;
}

static void
swfdec_abc_class_init (SwfdecAbcClass *class)
{
}

/*** ABC CODE ***/

void swfdec_abc_class_get_prototype (SwfdecAbcClass *classp, SwfdecAsValue *ret);
SWFDEC_ABC_NATIVE (29, swfdec_abc_class_get_prototype)
void
swfdec_abc_class_get_prototype (SwfdecAbcClass *classp, SwfdecAsValue *ret)
{
  if (classp->prototype)
    SWFDEC_AS_VALUE_SET_OBJECT (ret, SWFDEC_AS_OBJECT (classp->prototype));
  else
    SWFDEC_AS_VALUE_SET_NULL (ret);
}

