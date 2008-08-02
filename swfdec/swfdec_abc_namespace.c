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

#include "swfdec_abc_namespace.h"
#include "swfdec_debug.h"

void
swfdec_abc_namespace_init (SwfdecAbcNamespace *ns, SwfdecAbcNamespaceType type,
    const char *prefix, const char *uri)
{
  g_return_if_fail (ns != NULL);
  g_return_if_fail (uri != NULL);

  ns->type = type;
  ns->prefix = prefix;
  ns->uri = uri;
}

gboolean
swfdec_abc_namespace_equal (const SwfdecAbcNamespace *a,
    const SwfdecAbcNamespace *b)
{
  g_return_val_if_fail (a != NULL, FALSE);
  g_return_val_if_fail (b != NULL, FALSE);

  if (a->prefix == NULL || b->prefix == NULL)
    return a == b;

  return a->uri == b->uri;
}
