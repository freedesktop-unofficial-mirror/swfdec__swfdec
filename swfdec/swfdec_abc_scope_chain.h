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

#ifndef _SWFDEC_ABC_SCOPE_CHAIN_H_
#define _SWFDEC_ABC_SCOPE_CHAIN_H_

#include <swfdec/swfdec_abc_types.h>
#include <swfdec/swfdec_as_context.h>

G_BEGIN_DECLS

//typedef struct _SwfdecAbcScopeChain SwfdecAbcScopeChain;
typedef struct _SwfdecAbcScopeEntry SwfdecAbcScopeEntry;

struct _SwfdecAbcScopeEntry {
  SwfdecAsValue		value;		/* the scope value */
  gboolean		with;		/* with scope or normal scope? */
};

struct _SwfdecAbcScopeChain {
  guint			refcount;
  SwfdecAbcScopeChain *	base;		/* base scope chain (for super calls) - FIXME: Want vtables instead? */
  guint			n_entries;	/* number of entries in following array */
  SwfdecAbcScopeEntry	entries[1];	/* array of entries */
};

SwfdecAbcScopeChain *	swfdec_abc_scope_chain_new	(SwfdecAsContext *		context,
							 SwfdecAbcScopeChain *		base,
							 SwfdecAsValue *		start,
							 SwfdecAsValue *		end,
							 SwfdecAsValue *		with);
SwfdecAbcScopeChain *	swfdec_abc_scope_chain_ref	(SwfdecAbcScopeChain *		chain);
void			swfdec_abc_scope_chain_unref	(SwfdecAsContext *		context,
							 SwfdecAbcScopeChain *		chain);

void			swfdec_abc_scope_chain_mark	(const SwfdecAbcScopeChain *	chain);

G_END_DECLS
#endif
