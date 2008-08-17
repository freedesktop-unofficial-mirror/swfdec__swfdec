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

#include "swfdec_abc_script.h"
#include "swfdec_abc_method.h"
#include "swfdec_abc_traits.h"
#include "swfdec_as_context.h"
#include "swfdec_debug.h"

G_DEFINE_TYPE (SwfdecAbcScript, swfdec_abc_script, SWFDEC_TYPE_GC_OBJECT)

static void
swfdec_abc_script_mark (SwfdecGcObject *object)
{
  SwfdecAbcScript *script = SWFDEC_ABC_SCRIPT (object);

  if (script->global)
    swfdec_gc_object_mark (script->global);
  swfdec_gc_object_mark (script->traits);

  SWFDEC_GC_OBJECT_CLASS (swfdec_abc_script_parent_class)->mark (object);
}

static void
swfdec_abc_script_class_init (SwfdecAbcScriptClass *klass)
{
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  gc_class->mark = swfdec_abc_script_mark;
}

static void
swfdec_abc_script_init (SwfdecAbcScript *script)
{
}

SwfdecAbcObject *
swfdec_abc_script_get_global (SwfdecAbcScript *script)
{
  g_return_val_if_fail (SWFDEC_IS_ABC_SCRIPT (script), NULL);

  if (script->global == NULL) {
    SwfdecAbcTraits* traits = script->traits;
    SwfdecAbcMethod *method;
    SwfdecAsValue ret;
    
    swfdec_abc_traits_resolve (traits);
    script->global = swfdec_abc_object_new (traits, NULL);
    method = swfdec_abc_method_new (traits->construct, NULL);
    swfdec_abc_method_call (method, script->global, 0, NULL, &ret);
  }
  return script->global;
}

