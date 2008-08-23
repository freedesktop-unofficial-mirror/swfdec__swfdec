/* Swfdec
 * Copyright (C) 2007-2008 Benjamin Otte <otte@gnome.org>
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

#include "swfdec_sandbox_abc.h"

#include "swfdec_abc_pool.h"
#include "swfdec_abc_native.h"
#include "swfdec_bits.h"
#include "swfdec_debug.h"
#include "swfdec_initialize_abc.h"

enum {
  PROP_0,
  PROP_TYPE,
  PROP_URL,
};

G_DEFINE_TYPE_WITH_CODE (SwfdecSandboxAbc, swfdec_sandbox_abc, SWFDEC_TYPE_ABC_GLOBAL,
    G_IMPLEMENT_INTERFACE (SWFDEC_TYPE_SANDBOX, NULL))

static void
swfdec_sandbox_abc_mark (SwfdecGcObject *object)
{
  //SwfdecSandboxAbc *sandbox = SWFDEC_SANDBOX_ABC (object);

  SWFDEC_GC_OBJECT_CLASS (swfdec_sandbox_abc_parent_class)->mark (object);
}

static GObject *
swfdec_sandbox_abc_constructor (GType type, guint n_construct_properties,
    GObjectConstructParam *construct_properties)
{
  GObject *object;
  SwfdecAsContext *context;
  SwfdecPlayer *player;
  SwfdecBits bits;

  object = G_OBJECT_CLASS (swfdec_sandbox_abc_parent_class)->constructor (type, 
      n_construct_properties, construct_properties);

  context = swfdec_gc_object_get_context (object);
  player = SWFDEC_PLAYER (context);

  swfdec_bits_init_data (&bits, swfdec_initialize_abc, sizeof (swfdec_initialize_abc));
  swfdec_abc_pool_new_trusted (context, &bits, swfdec_abc_natives_flash,
      G_N_ELEMENTS (swfdec_abc_natives_flash));

  return object;
}

static void
swfdec_sandbox_abc_get_property (GObject *object, guint param_id, GValue *value, 
    GParamSpec * pspec)
{
  SwfdecSandboxAbc *sandbox = SWFDEC_SANDBOX_ABC (object);

  switch (param_id) {
    case PROP_TYPE:
      g_value_set_enum (value, sandbox->type);
      break;
    case PROP_URL:
      g_value_set_boxed (value, sandbox->url);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
swfdec_sandbox_abc_set_property (GObject *object, guint param_id, const GValue *value, 
    GParamSpec * pspec)
{
  SwfdecSandboxAbc *sandbox = SWFDEC_SANDBOX_ABC (object);

  switch (param_id) {
    case PROP_TYPE:
      sandbox->type = g_value_get_enum (value);
      break;
    case PROP_URL:
      sandbox->url = g_value_dup_boxed (value);
      g_assert (sandbox->url);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
swfdec_sandbox_abc_dispose (GObject *object)
{
  SwfdecSandboxAbc *sandbox = SWFDEC_SANDBOX_ABC (object);

  swfdec_url_free (sandbox->url);

  G_OBJECT_CLASS (swfdec_sandbox_abc_parent_class)->dispose (object);
}

static void
swfdec_sandbox_abc_class_init (SwfdecSandboxAbcClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->constructor = swfdec_sandbox_abc_constructor;
  object_class->dispose = swfdec_sandbox_abc_dispose;
  object_class->get_property = swfdec_sandbox_abc_get_property;
  object_class->set_property = swfdec_sandbox_abc_set_property;

  g_object_class_override_property (object_class, PROP_TYPE, "sandbox-type");
  g_object_class_override_property (object_class, PROP_URL, "url");

  gc_class->mark = swfdec_sandbox_abc_mark;
}

static void
swfdec_sandbox_abc_init (SwfdecSandboxAbc *sandbox_abc)
{
}

