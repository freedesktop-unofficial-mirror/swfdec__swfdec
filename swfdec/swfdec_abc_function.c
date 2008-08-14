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

#include "swfdec_abc_function.h"
#include "swfdec_abc_traits.h"
#include "swfdec_as_context.h"
#include "swfdec_debug.h"

G_DEFINE_TYPE (SwfdecAbcFunction, swfdec_abc_function, SWFDEC_TYPE_GC_OBJECT)

static void
swfdec_abc_function_dispose (GObject *object)
{
  SwfdecAbcFunction *fun = SWFDEC_ABC_FUNCTION (object);
  SwfdecAsContext *context = swfdec_gc_object_get_context (object);

  if (fun->n_args) {
    swfdec_as_context_free (context, fun->n_args * sizeof (SwfdecAbcFunctionArgument), fun->args);
    fun->args = NULL;
    fun->n_args = 0;
  }
  if (fun->n_exceptions) {
    swfdec_as_context_free (context, fun->n_exceptions * sizeof (SwfdecAbcException), fun->exceptions);
    fun->exceptions = NULL;
    fun->n_exceptions = 0;
  }
  if (fun->code) {
    swfdec_buffer_unref (fun->code);
    fun->code = NULL;
  }

  G_OBJECT_CLASS (swfdec_abc_function_parent_class)->dispose (object);
}

static void
swfdec_abc_function_class_init (SwfdecAbcFunctionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_abc_function_dispose;
}

static void
swfdec_abc_function_init (SwfdecAbcFunction *function)
{
}

gboolean
swfdec_abc_function_bind (SwfdecAbcFunction *fun, SwfdecAbcTraits *traits)
{
  g_return_val_if_fail (SWFDEC_IS_ABC_FUNCTION (fun), FALSE);
  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), FALSE);

  if (fun->bound_traits)
    return FALSE;

  fun->bound_traits = traits;
  return TRUE;
}

gboolean
swfdec_abc_function_is_native (SwfdecAbcFunction *fun)
{
  g_return_val_if_fail (SWFDEC_IS_ABC_FUNCTION (fun), FALSE);
  
  return fun->native != NULL;
}

gboolean
swfdec_abc_function_resolve (SwfdecAbcFunction *fun)
{
  g_return_val_if_fail (SWFDEC_IS_ABC_FUNCTION (fun), FALSE);
  
  SWFDEC_FIXME ("i can has resolve?");
  fun->resolved = TRUE;
  return TRUE;
}

gboolean
swfdec_abc_function_verify (SwfdecAbcFunction *fun)
{
  g_return_val_if_fail (SWFDEC_IS_ABC_FUNCTION (fun), FALSE);
  g_return_val_if_fail (fun->resolved, FALSE);
  
  SWFDEC_FIXME ("i can has verify?");
  fun->verified = TRUE;
  return TRUE;
}

