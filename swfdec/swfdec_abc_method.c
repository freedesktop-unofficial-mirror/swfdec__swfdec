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

#include "swfdec_abc_method.h"
#include "swfdec_abc_function.h"
#include "swfdec_abc_internal.h"
#include "swfdec_abc_traits.h"
#include "swfdec_debug.h"

G_DEFINE_TYPE (SwfdecAbcMethod, swfdec_abc_method, SWFDEC_TYPE_ABC_OBJECT)

static void
swfdec_abc_method_dispose (GObject *object)
{
  SwfdecAbcMethod *method = SWFDEC_ABC_METHOD (object);

  swfdec_abc_scope_chain_unref (swfdec_gc_object_get_context (method),
      method->scope);

  G_OBJECT_CLASS (swfdec_abc_method_parent_class)->dispose (object);
}

static void
swfdec_abc_method_mark (SwfdecGcObject *object)
{
  SwfdecAbcMethod *method = SWFDEC_ABC_METHOD (object);

  swfdec_gc_object_mark (method->function);
  swfdec_abc_scope_chain_mark (method->scope);

  SWFDEC_GC_OBJECT_CLASS (swfdec_abc_method_parent_class)->mark (object);
}

static void
swfdec_abc_method_class_init (SwfdecAbcMethodClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_abc_method_dispose;

  gc_class->mark = swfdec_abc_method_mark;
}

static void
swfdec_abc_method_init (SwfdecAbcMethod *method)
{
}

SwfdecAbcMethod *
swfdec_abc_method_new (SwfdecAbcFunction *function, SwfdecAbcScopeChain *chain)
{
  SwfdecAbcMethod *method;

  g_return_val_if_fail (SWFDEC_IS_ABC_FUNCTION (function), NULL);

  if (!swfdec_abc_traits_resolve (function->bound_traits))
    return NULL;

  method = g_object_new (SWFDEC_TYPE_ABC_METHOD, 
      "context", swfdec_gc_object_get_context (function), 
      "traits", function->bound_traits, NULL);
  method->function = function;
  method->scope = swfdec_abc_scope_chain_ref (chain);

  return method;
}

void
swfdec_abc_method_call (SwfdecAbcMethod *method, 
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  g_return_if_fail (SWFDEC_IS_ABC_METHOD (method));
  g_return_if_fail (argv != NULL);
  g_return_if_fail (ret != NULL);

  swfdec_abc_function_call (method->function, method->scope, argc, argv, ret);
}

