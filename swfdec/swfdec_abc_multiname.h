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

#include <swfdec/swfdec_abc_namespace.h>
#include <swfdec/swfdec_abc_ns_set.h>

#ifndef _SWFDEC_ABC_MULTINAME_H_
#define _SWFDEC_ABC_MULTINAME_H_

G_BEGIN_DECLS

#define SWFDEC_ABC_MULTINAME_ANY GUINT_TO_POINTER (1)

typedef struct _SwfdecAbcMultiname SwfdecAbcMultiname;
struct _SwfdecAbcMultiname {
  const char *		name;		/* gc-collected name, MULTINAME_ANY or NULL if unknown */
  SwfdecAbcNamespace *	ns;		/* namespace, MULTINAME_ANY or NULL if none */
  SwfdecAbcNsSet *	nsset;		/* namespace set or NULL - must not be set if ns is set */
};

void			swfdec_abc_multiname_init		(SwfdecAbcMultiname *		multi,
								 const char *			name,
								 SwfdecAbcNamespace *		ns,
								 SwfdecAbcNsSet *		set);

#define swfdec_abc_multiname_is_qualified(mn) ((mn)->name > (const char *) SWFDEC_ABC_MULTINAME_ANY \
    && (mn)->ns > (SwfdecAbcNamespace *) SWFDEC_ABC_MULTINAME_ANY)
#define swfdec_abc_multiname_is_binding(mn) ((mn)->name > (const char *) SWFDEC_ABC_MULTINAME_ANY \
    && ((mn)->nsset || (mn)->ns > (SwfdecAbcNamespace *) SWFDEC_ABC_MULTINAME_ANY))
#define swfdec_abc_multiname_get_n_namespaces(mn) ((mn)->nsset ? \
    swfdec_abc_ns_set_get_n_namespaces ((mn)->nsset) : 1)
SwfdecAbcNamespace *	swfdec_abc_multiname_get_namespace	(const SwfdecAbcMultiname *	mn,
								 guint				i);


G_END_DECLS

#endif
