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

#include "swfdec_abc_scope_chain.h"

#include <string.h>

#include "swfdec_debug.h"

/* NB: n_entries must be at least 1 */
#define SWFDEC_ABC_SCOPE_CHAIN_SIZE(n_entries) \
  (sizeof (SwfdecAbcScopeChain) + sizeof (SwfdecAbcScopeEntry) * n_entries)

SwfdecAbcScopeChain *
swfdec_abc_scope_chain_new (SwfdecAsContext *context, const SwfdecAbcScopeChain *base,
    SwfdecAsValue *start, SwfdecAsValue *end, SwfdecAsValue *with)
{
  SwfdecAbcScopeChain *chain;
  SwfdecAbcScopeEntry *cur;
  SwfdecAsValue *min;
  guint n_entries;
  gsize size;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), NULL);
  g_return_val_if_fail (start <= end, NULL);
  g_return_val_if_fail (start <= with, NULL);

  if (start == end)
    return swfdec_abc_scope_chain_ref (chain);

  n_entries = base ? base->n_entries : 0;
  n_entries += end - start;
  if (n_entries == 0)
    return NULL;

  size = SWFDEC_ABC_SCOPE_CHAIN_SIZE (n_entries);
  swfdec_as_context_use_mem (context, size);
  if (swfdec_as_context_is_aborted (context)) {
    swfdec_as_context_unuse_mem (context, size);
    return NULL;
  }

  chain = g_slice_alloc0 (size);
  chain->refcount = 1;
  chain->n_entries = n_entries;
  if (base) {
    memcpy (chain->entries, base->entries, base->n_entries * sizeof (SwfdecAbcScopeEntry));
    cur = &chain->entries[base->n_entries];
  } else {
    cur = &chain->entries[0];
  }
  min = MIN (end, with);
  for (;start < min; start++) {
    cur->value = *start;
    cur->with = FALSE;
    cur++;
  }
  for (;start < end; start++) {
    cur->value = *start;
    cur->with = TRUE;
    cur++;
  }

  return chain;
}

SwfdecAbcScopeChain *
swfdec_abc_scope_chain_ref (SwfdecAbcScopeChain *chain)
{
  if (chain == NULL)
    return NULL;

  chain->refcount++;
  return chain;
}

void
swfdec_abc_scope_chain_unref (SwfdecAsContext *context, SwfdecAbcScopeChain *chain)
{
  gsize size;
  
  g_return_if_fail (SWFDEC_IS_AS_CONTEXT (context));

  if (chain == NULL)
    return;
  chain->refcount--;
  if (chain->refcount > 0)
    return;
  size = SWFDEC_ABC_SCOPE_CHAIN_SIZE (chain->n_entries);
  swfdec_as_context_unuse_mem (context, size);
  g_slice_free1 (size, chain);
}

void
swfdec_abc_scope_chain_mark (const SwfdecAbcScopeChain *chain)
{
  guint i;

  if (chain == NULL)
    return;

  for (i = 0; i < chain->n_entries; i++) {
    swfdec_as_value_mark (&chain->entries[i].value);
  }
}
