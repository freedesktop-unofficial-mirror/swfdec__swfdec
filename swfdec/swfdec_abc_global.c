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

#include "swfdec_abc_global.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "swfdec_abc_class.h"
#include "swfdec_abc_file.h"
#include "swfdec_abc_function.h"
#include "swfdec_abc_initialize.h"
#include "swfdec_abc_internal.h"
#include "swfdec_abc_native.h"
#include "swfdec_abc_script.h"
#include "swfdec_abc_traits.h"
#include "swfdec_as_context.h"
#include "swfdec_as_strings.h"
#include "swfdec_debug.h"

typedef struct _SwfdecAbcGlobalScript SwfdecAbcGlobalScript;
struct _SwfdecAbcGlobalScript {
  SwfdecAbcNamespace *	ns;
  const char *		name;
  SwfdecAbcScript *	script;
};

enum {
  PROP_0,
  PROP_TRAITS
};

G_DEFINE_TYPE (SwfdecAbcGlobal, swfdec_abc_global, SWFDEC_TYPE_ABC_OBJECT)

static GObject *
swfdec_abc_global_constructor (GType type, guint n_construct_properties,
    GObjectConstructParam *construct_properties)
{
  GObject *object;
  SwfdecAsContext *context;
  SwfdecAbcGlobal *global;
  SwfdecAbcTraits *traits;
  SwfdecAsValue val;
  SwfdecBits bits;

  object = G_OBJECT_CLASS (swfdec_abc_global_parent_class)->constructor (type, 
      n_construct_properties, construct_properties);

  global = SWFDEC_ABC_GLOBAL (object);
  context = swfdec_gc_object_get_context (object);

  context->global = SWFDEC_AS_OBJECT (global);

  /* create builtin traits */
  global->void_traits = g_object_new (SWFDEC_TYPE_ABC_TRAITS, "context", context, NULL);
  global->void_traits->pool = NULL;
  global->void_traits->ns = context->public_ns;
  global->void_traits->name = SWFDEC_AS_STR_void;
  global->void_traits->final = TRUE;
  global->void_traits->resolved = TRUE;
  global->null_traits = g_object_new (SWFDEC_TYPE_ABC_TRAITS, "context", context, NULL);
  global->null_traits->pool = NULL;
  global->null_traits->ns = context->public_ns;
  global->null_traits->name = SWFDEC_AS_STR_null;
  global->null_traits->final = TRUE;
  global->null_traits->resolved = TRUE;
  swfdec_abc_global_add_traits (global, global->void_traits);
  swfdec_abc_global_add_traits (global, global->null_traits);

  /* init default pool */
  swfdec_bits_init_data (&bits, swfdec_abc_initialize, sizeof (swfdec_abc_initialize));
  global->file = swfdec_abc_file_new_trusted (context, &bits, 
      swfdec_abc_natives, G_N_ELEMENTS (swfdec_abc_natives));
  /* must work and not throw exceptions */
  g_assert (global->file);

  /* set proper traits here */
  traits = global->file->main->traits;
  if (!swfdec_abc_traits_resolve (traits)) {
    g_assert_not_reached ();
  }
  g_object_unref (SWFDEC_ABC_OBJECT (global)->traits);
  SWFDEC_ABC_OBJECT (global)->traits = traits;
  g_object_ref (traits);
  g_assert (SWFDEC_ABC_OBJECT (global)->slots == NULL);
  if (traits->n_slots)
    SWFDEC_ABC_OBJECT (global)->slots = swfdec_as_context_new (context,
       SwfdecAsValue, traits->n_slots);

  /* run main script */
  global->file->main->global = SWFDEC_ABC_OBJECT (global);
  SWFDEC_AS_VALUE_SET_OBJECT (&val, SWFDEC_AS_OBJECT (global));
  swfdec_abc_function_call (traits->construct, NULL, 0, &val, &val);
  return object;
}

static void
swfdec_abc_global_get_property (GObject *object, guint param_id, GValue *value, 
    GParamSpec * pspec)
{
  switch (param_id) {
    case PROP_TRAITS:
      g_value_set_object (value, SWFDEC_ABC_OBJECT (object)->traits);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
swfdec_abc_global_set_property (GObject *object, guint param_id, const GValue *value, 
    GParamSpec * pspec)
{
  switch (param_id) {
    case PROP_TRAITS:
      /* big cheat here: we ignore traits during creation, so asserts dont trigger.
       * We'll assign proper traits later in the constructor */
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
swfdec_abc_global_dispose (GObject *object)
{
  SwfdecAbcGlobal *global = SWFDEC_ABC_GLOBAL (object);

  g_ptr_array_free (global->traits, TRUE);
  g_array_free (global->scripts, TRUE);

  G_OBJECT_CLASS (swfdec_abc_global_parent_class)->dispose (object);
}

static void
swfdec_abc_global_mark (SwfdecGcObject *object)
{
  SwfdecAbcGlobal *global = SWFDEC_ABC_GLOBAL (object);
  guint i;

  g_ptr_array_foreach (global->traits, (GFunc) swfdec_gc_object_mark, NULL);
  for (i = 0; i < SWFDEC_ABC_N_TYPES; i++) {
    if (global->classes[i])
      swfdec_gc_object_mark (global->classes[i]);
  }

  SWFDEC_GC_OBJECT_CLASS (swfdec_abc_global_parent_class)->mark (object);
}

static void
swfdec_abc_global_class_init (SwfdecAbcGlobalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->constructor = swfdec_abc_global_constructor;
  object_class->dispose = swfdec_abc_global_dispose;
  object_class->get_property = swfdec_abc_global_get_property;
  object_class->set_property = swfdec_abc_global_set_property;

  g_object_class_override_property (object_class, PROP_TRAITS, "traits");

  gc_class->mark = swfdec_abc_global_mark;
}

static void
swfdec_abc_global_init (SwfdecAbcGlobal *global)
{
  global->traits = g_ptr_array_new ();
  global->scripts = g_array_new (FALSE, FALSE, sizeof (SwfdecAbcGlobalScript));
}

void
swfdec_abc_global_add_traits (SwfdecAbcGlobal *global, SwfdecAbcTraits *traits)
{
  g_return_if_fail (SWFDEC_IS_ABC_GLOBAL (global));
  g_return_if_fail (SWFDEC_IS_ABC_TRAITS (traits));
  g_return_if_fail (swfdec_abc_global_get_traits (global, traits->ns, traits->name) == NULL);

  g_ptr_array_add (global->traits, traits);
}

SwfdecAbcTraits *
swfdec_abc_global_get_traits (SwfdecAbcGlobal *global, SwfdecAbcNamespace *ns,
    const char *name)
{
  guint i;

  g_return_val_if_fail (SWFDEC_IS_ABC_GLOBAL (global), NULL);
  g_return_val_if_fail (SWFDEC_IS_ABC_NAMESPACE (ns), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  for (i = 0; i < global->traits->len; i++) {
    SwfdecAbcTraits *traits = g_ptr_array_index (global->traits, i);
    if (traits->ns == ns && traits->name == name)
      return traits;
  }
  return NULL;
}

SwfdecAbcTraits *
swfdec_abc_global_get_traits_for_multiname (SwfdecAbcGlobal *global,
    const SwfdecAbcMultiname *mn)
{
  SwfdecAbcTraits *ret, *tmp;
  guint i;

  g_return_val_if_fail (SWFDEC_IS_ABC_GLOBAL (global), NULL);
  g_return_val_if_fail (mn != NULL, NULL);

  if (!swfdec_abc_multiname_is_binding (mn))
    return NULL;

  ret = NULL;
  for (i = 0; i < swfdec_abc_multiname_get_n_namespaces (mn); i++) {
    tmp = swfdec_abc_global_get_traits (global, 
	swfdec_abc_multiname_get_namespace (mn, i), mn->name);
    if (tmp) {
      if (ret == NULL) {
	ret = tmp;
      } else {
	SWFDEC_FIXME ("throw ambigious namespace error here");
	return NULL;
      }
    }
  }
  return ret;
}

static SwfdecAbcGlobalScript *
swfdec_abc_global_find_script (SwfdecAbcGlobal *global, SwfdecAbcNamespace *ns,
    const char *name)
{
  guint i;

  for (i = 0; i < global->scripts->len; i++) {
    SwfdecAbcGlobalScript *script = &g_array_index (global->scripts, SwfdecAbcGlobalScript, i);
    if (script->ns == ns && script->name == name)
      return script;
  }
  return NULL;
}

void
swfdec_abc_global_add_script (SwfdecAbcGlobal *global, SwfdecAbcNamespace *ns,
    const char *name, SwfdecAbcScript *script, gboolean override)
{
  SwfdecAbcGlobalScript *found;

  g_return_if_fail (SWFDEC_IS_ABC_GLOBAL (global));
  g_return_if_fail (SWFDEC_IS_ABC_NAMESPACE (ns));
  g_return_if_fail (name != NULL);
  g_return_if_fail (SWFDEC_IS_ABC_SCRIPT (script));

  found = swfdec_abc_global_find_script (global, ns, name);
  if (found) {
    if (override)
      found->script = script;
  } else {
    SwfdecAbcGlobalScript add = { ns, name, script };
    g_array_append_val (global->scripts, add);
  }
}

SwfdecAbcTraits *
swfdec_abc_global_get_builtin_traits (SwfdecAbcGlobal *global, guint id)
{
  g_return_val_if_fail (SWFDEC_IS_ABC_GLOBAL (global), NULL);
  g_return_val_if_fail (global->file, NULL);
  g_return_val_if_fail (id < SWFDEC_ABC_N_TYPES, NULL);

  return global->file->instances[id];
}

SwfdecAbcClass *
swfdec_abc_global_get_builtin_class (SwfdecAbcGlobal *global, guint id)
{
  SwfdecAbcTraits *traits;
  SwfdecAbcMultiname mn;
  SwfdecAsValue val;

  g_return_val_if_fail (SWFDEC_IS_ABC_GLOBAL (global), NULL);
  g_return_val_if_fail (global->file, NULL);
  g_return_val_if_fail (id < SWFDEC_ABC_N_TYPES, NULL);

  traits = global->file->instances[id];

  swfdec_abc_multiname_init (&mn, traits->name, traits->ns, NULL);
  SWFDEC_AS_VALUE_SET_OBJECT (&val, SWFDEC_AS_OBJECT (global));
  if (!swfdec_abc_object_get_variable (swfdec_gc_object_get_context (global),
	&val, &mn, &val))
    g_assert_not_reached ();
  g_assert (SWFDEC_AS_VALUE_IS_OBJECT (&val));
  g_assert (SWFDEC_IS_ABC_CLASS (SWFDEC_AS_VALUE_GET_OBJECT (&val)));
  global->classes[id] = SWFDEC_ABC_CLASS (SWFDEC_AS_VALUE_GET_OBJECT (&val));

  return global->classes[id];
}

SwfdecAbcScript *
swfdec_abc_global_get_script_multi (SwfdecAbcGlobal *global,
    const SwfdecAbcMultiname *mn)
{
  SwfdecAbcGlobalScript *script, *found;
  guint i;

  found = NULL;
  for (i = 0; i < swfdec_abc_multiname_get_n_namespaces (mn); i++) {
    script = swfdec_abc_global_find_script (global,
	swfdec_abc_multiname_get_namespace (mn, i), mn->name);
    if (script == NULL)
      continue;
    if (found == NULL) {
      found = script;
    } else {
      SWFDEC_FIXME ("handle ambiguity error somehow here");
    }
  }
  return found ? found->script : NULL;
}

gboolean
swfdec_abc_global_get_script_variable (SwfdecAbcGlobal *global,
    const SwfdecAbcMultiname *mn, SwfdecAsValue *value)
{
  SwfdecAsContext *context;
  SwfdecAbcScript *script;
  SwfdecAsValue val;

  g_return_val_if_fail (SWFDEC_IS_ABC_GLOBAL (global), FALSE);
  g_return_val_if_fail (mn != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  context = swfdec_gc_object_get_context (global);
  script = swfdec_abc_global_get_script_multi (global, mn);
  if (script) {
    SWFDEC_AS_VALUE_SET_OBJECT (&val,
	SWFDEC_AS_OBJECT (swfdec_abc_script_get_global (script)));
  } else {
    SWFDEC_AS_VALUE_SET_OBJECT (&val, SWFDEC_AS_OBJECT (global));
  }
  return swfdec_abc_object_get_variable (context, &val, mn, value);
}

