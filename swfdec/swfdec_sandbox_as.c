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

#include "swfdec_sandbox_as.h"
#include "swfdec_abc_file.h"
#include "swfdec_abc_native.h"
#include "swfdec_as_internal.h"
#include "swfdec_debug.h"
#include "swfdec_initialize.h"
#include "swfdec_internal.h"
#include "swfdec_player_internal.h"

enum {
  PROP_0,
  PROP_TYPE,
  PROP_URL,
  PROP_VERSION,
};

G_DEFINE_TYPE_WITH_CODE (SwfdecSandboxAs, swfdec_sandbox_as, SWFDEC_TYPE_AS_GLOBAL,
    G_IMPLEMENT_INTERFACE (SWFDEC_TYPE_SANDBOX, NULL))

static void
swfdec_sandbox_as_mark (SwfdecGcObject *object)
{
  SwfdecSandboxAs *sandbox = SWFDEC_SANDBOX_AS (object);

  swfdec_gc_object_mark (sandbox->MovieClip);
  swfdec_gc_object_mark (sandbox->Video);

  SWFDEC_GC_OBJECT_CLASS (swfdec_sandbox_as_parent_class)->mark (object);
}

static GObject *
swfdec_sandbox_as_constructor (GType type, guint n_construct_properties,
    GObjectConstructParam *construct_properties)
{
  GObject *object;
  SwfdecAsContext *context;
  SwfdecPlayer *player;
  SwfdecSandboxAs *sandbox;

  object = G_OBJECT_CLASS (swfdec_sandbox_as_parent_class)->constructor (type, 
      n_construct_properties, construct_properties);

  sandbox = SWFDEC_SANDBOX_AS (object);
  context = swfdec_gc_object_get_context (object);
  player = SWFDEC_PLAYER (context);

  swfdec_sprite_movie_init_context (player);
  swfdec_video_movie_init_context (player);
  swfdec_net_stream_init_context (player);

  swfdec_as_context_run_init_script (context, swfdec_initialize, 
      sizeof (swfdec_initialize), sandbox->flash_version);

  return object;
}

static void
swfdec_sandbox_as_get_property (GObject *object, guint param_id, GValue *value, 
    GParamSpec * pspec)
{
  SwfdecSandboxAs *sandbox = SWFDEC_SANDBOX_AS (object);

  switch (param_id) {
    case PROP_TYPE:
      g_value_set_enum (value, sandbox->type);
      break;
    case PROP_URL:
      g_value_set_boxed (value, sandbox->url);
      break;
    case PROP_VERSION:
      g_value_set_uint (value, sandbox->flash_version);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
swfdec_sandbox_as_set_property (GObject *object, guint param_id, const GValue *value, 
    GParamSpec * pspec)
{
  SwfdecSandboxAs *sandbox = SWFDEC_SANDBOX_AS (object);

  switch (param_id) {
    case PROP_TYPE:
      sandbox->type = g_value_get_enum (value);
      break;
    case PROP_URL:
      sandbox->url = g_value_dup_boxed (value);
      g_assert (sandbox->url);
      break;
    case PROP_VERSION:
      sandbox->flash_version = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
swfdec_sandbox_as_dispose (GObject *object)
{
  SwfdecSandboxAs *sandbox = SWFDEC_SANDBOX_AS (object);

  swfdec_url_free (sandbox->url);

  G_OBJECT_CLASS (swfdec_sandbox_as_parent_class)->dispose (object);
}

static void
swfdec_sandbox_as_class_init (SwfdecSandboxAsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->constructor = swfdec_sandbox_as_constructor;
  object_class->dispose = swfdec_sandbox_as_dispose;
  object_class->get_property = swfdec_sandbox_as_get_property;
  object_class->set_property = swfdec_sandbox_as_set_property;

  g_object_class_override_property (object_class, PROP_TYPE, "sandbox-type");
  g_object_class_override_property (object_class, PROP_URL, "url");
  g_object_class_install_property (object_class, PROP_VERSION, 
      g_param_spec_uint ("version", "version", "Flash version of this sandbox",
	0, 255, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gc_class->mark = swfdec_sandbox_as_mark;
}

static void
swfdec_sandbox_as_init (SwfdecSandboxAs *sandbox_as)
{
}

