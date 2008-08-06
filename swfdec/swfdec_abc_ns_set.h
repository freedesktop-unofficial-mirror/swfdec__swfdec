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

#ifndef _SWFDEC_ABC_NS_SET_H_
#define _SWFDEC_ABC_NS_SET_H_

G_BEGIN_DECLS


typedef GPtrArray SwfdecAbcNsSet;

#define swfdec_abc_ns_set_new() g_ptr_array_new ()
#define swfdec_abc_ns_set_free(set) g_ptr_array_free (set, TRUE)

#define swfdec_abc_ns_set_add(set, ns) g_ptr_array_add (set, ns)
gboolean	swfdec_abc_ns_set_contains (const SwfdecAbcNsSet *	set,
					    const SwfdecAbcNamespace *	ns);
#define swfdec_abc_ns_set_get_n_namespaces(set) ((set)->len)
#define swfdec_abc_ns_set_get_namespace(set, i) (g_ptr_array_index (set, i))


G_END_DECLS

#endif
