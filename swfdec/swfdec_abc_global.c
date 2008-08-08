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

#include "swfdec_abc_global.h"
#include "swfdec_abc_function.h"
#include "swfdec_as_context.h"
#include "swfdec_debug.h"

typedef struct _SwfdecAbcGlobalScript SwfdecAbcGlobalScript;
struct _SwfdecAbcGlobalScript {
  SwfdecAbcNamespace *	ns;
  const char *		name;
  SwfdecAbcFunction *	script;
};

G_DEFINE_TYPE (SwfdecAbcGlobal, swfdec_abc_global, SWFDEC_TYPE_AS_OBJECT)

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

  g_ptr_array_foreach (global->traits, (GFunc) swfdec_gc_object_mark, NULL);

  SWFDEC_GC_OBJECT_CLASS (swfdec_abc_global_parent_class)->mark (object);
}

static void
swfdec_abc_global_class_init (SwfdecAbcGlobalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_abc_global_dispose;

  gc_class->mark = swfdec_abc_global_mark;
}

static void
swfdec_abc_global_init (SwfdecAbcGlobal *global)
{
  global->traits = g_ptr_array_new ();
  global->scripts = g_array_new (FALSE, FALSE, sizeof (SwfdecAbcGlobalScript));
}

SwfdecAsObject *
swfdec_abc_global_new (SwfdecAsContext *context)
{
  SwfdecAbcGlobal *global;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), NULL);

  global = g_object_new (SWFDEC_TYPE_ABC_GLOBAL, "context", context, NULL);

  return SWFDEC_AS_OBJECT (global);
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
    const char *name, SwfdecAbcFunction *script, gboolean override)
{
  SwfdecAbcGlobalScript *found;

  g_return_if_fail (SWFDEC_IS_ABC_GLOBAL (global));
  g_return_if_fail (SWFDEC_IS_ABC_NAMESPACE (ns));
  g_return_if_fail (name != NULL);
  g_return_if_fail (SWFDEC_IS_ABC_FUNCTION (script));

  found = swfdec_abc_global_find_script (global, ns, name);
  if (found) {
    if (override)
      found->script = script;
  } else {
    SwfdecAbcGlobalScript add = { ns, name, script };
    g_array_append_val (global->scripts, add);
  }
}
