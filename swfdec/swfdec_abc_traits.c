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

#include "swfdec_abc_traits.h"
#include "swfdec_abc_multiname.h"
#include "swfdec_abc_namespace.h"
#include "swfdec_abc_object.h"
#include "swfdec_as_context.h"
#include "swfdec_as_strings.h"
#include "swfdec_debug.h"

const SwfdecAbcTrait swfdec_abc_trait_ambiguous = { SWFDEC_ABC_TRAIT_NONE, 0,
  NULL, SWFDEC_AS_STR_EMPTY, FALSE, FALSE, };

G_DEFINE_TYPE (SwfdecAbcTraits, swfdec_abc_traits, SWFDEC_TYPE_GC_OBJECT)

static void
swfdec_abc_traits_dispose (GObject *object)
{
  SwfdecAbcTraits *traits = SWFDEC_ABC_TRAITS (object);
  SwfdecAsContext *context = swfdec_gc_object_get_context (traits);

  if (traits->n_traits) {
    swfdec_as_context_free (context, traits->n_traits * sizeof (SwfdecAbcTrait), traits->traits);
    traits->traits = NULL;
    traits->n_traits = 0;
  }

  G_OBJECT_CLASS (swfdec_abc_traits_parent_class)->dispose (object);
}

static void
swfdec_abc_traits_mark (SwfdecGcObject *object)
{
  SwfdecAbcTraits *traits = SWFDEC_ABC_TRAITS (object);

  swfdec_gc_object_mark (traits->ns);
  swfdec_as_string_mark (traits->name);
  if (traits->base)
    swfdec_gc_object_mark (traits->base);
  if (traits->construct)
    swfdec_gc_object_mark (traits->construct);
  if (traits->protected_ns)
    swfdec_gc_object_mark (traits->protected_ns);

  SWFDEC_GC_OBJECT_CLASS (swfdec_abc_traits_parent_class)->mark (object);
}

static void
swfdec_abc_traits_class_init (SwfdecAbcTraitsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_abc_traits_dispose;

  gc_class->mark = swfdec_abc_traits_mark;
}

static void
swfdec_abc_traits_init (SwfdecAbcTraits *traits)
{
  traits->type_func = swfdec_abc_object_get_type;
}

gboolean
swfdec_abc_traits_allow_early_binding (SwfdecAbcTraits *traits)
{
  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), FALSE);

  do {
    if (traits->n_slots == 0 ||
	traits->base == NULL)
      return TRUE;
    if (traits->base->pool != traits->pool)
      return FALSE;
  } while ((traits = traits->base) != NULL);
  g_return_val_if_reached (FALSE);
}

gboolean
swfdec_abc_traits_resolve (SwfdecAbcTraits *traits)
{
  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), FALSE);
  
  if (traits->resolved)
    return TRUE;

  SWFDEC_FIXME ("resolve traits");
  traits->resolved = TRUE;
  return TRUE;
}

const SwfdecAbcTrait *
swfdec_abc_traits_get_trait (SwfdecAbcTraits *traits,
    SwfdecAbcNamespace *ns, const char *name)
{
  guint i;

  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), NULL);
  g_return_val_if_fail (SWFDEC_IS_ABC_NAMESPACE (ns), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  for (i = 0; i < traits->n_traits; i++) {
    SwfdecAbcTrait *trait = &traits->traits[i];

    if (trait->type == SWFDEC_ABC_TRAIT_NONE)
      continue;

    if (trait->ns == ns && trait->name == name)
      return trait;
  }
  return NULL;
}

const SwfdecAbcTrait *
swfdec_abc_traits_find_trait (SwfdecAbcTraits *traits,
    SwfdecAbcNamespace *ns, const char *name)
{
  const SwfdecAbcTrait *trait;

  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), NULL);
  g_return_val_if_fail (SWFDEC_IS_ABC_NAMESPACE (ns), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  do {
    trait = swfdec_abc_traits_get_trait (traits, ns, name);
    if (trait)
      return trait;
    traits = traits->base;
  } while (traits);
  return NULL;
}

const SwfdecAbcTrait *
swfdec_abc_traits_find_trait_multi (SwfdecAbcTraits *traits,
    const SwfdecAbcMultiname *mn)
{
  const SwfdecAbcTrait *trait, *found;
  guint i;

  g_return_val_if_fail (SWFDEC_IS_ABC_TRAITS (traits), NULL);
  g_return_val_if_fail (mn != NULL, NULL);

  found = NULL;
  for (i = 0; i < swfdec_abc_multiname_get_n_namespaces (mn); i++) {
    trait = swfdec_abc_traits_find_trait (traits, 
	swfdec_abc_multiname_get_namespace (mn, i), traits->name);
    if (trait == NULL)
      continue;
    if (found == NULL)
      found = trait;
    else
      return SWFDEC_ABC_TRAIT_AMBIGUOUS;
  }
  return found;
}

