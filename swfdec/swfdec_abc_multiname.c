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

#include "swfdec_abc_multiname.h"

#include <string.h>

#include "swfdec_abc_internal.h"
#include "swfdec_as_context.h"
#include "swfdec_debug.h"

void
swfdec_abc_multiname_init (SwfdecAbcMultiname *multi, SwfdecAbcNamespace *ns,
    const char *name)
{
  g_return_if_fail (multi != NULL);
  g_return_if_fail (ns == NULL || ns == SWFDEC_ABC_MULTINAME_ANY || SWFDEC_IS_ABC_NAMESPACE (ns));

  multi->name = name;
  multi->ns = ns;
  multi->nsset = NULL;
}

void
swfdec_abc_multiname_init_set (SwfdecAbcMultiname *multi, SwfdecAbcNsSet *set, 
    const char *name)
{
  g_return_if_fail (multi != NULL);
  g_return_if_fail (set != NULL);

  multi->name = name;
  multi->ns = NULL;
  multi->nsset = set;
}

/**
 * swfdec_abc_multiname_init_from_string:
 * @multi: the multiname to be initialized
 * @context: context to use for interning the strings
 * @string: a String in the form [namespace.]name to intern
 *
 * extracts the name and namespace out of a dot-seperated @string and sets them
 * on the given multiname.
 **/
void
swfdec_abc_multiname_init_from_string (SwfdecAbcMultiname *multi,
    SwfdecAsContext *context, const char *string)
{
  const char *dot;

  g_return_if_fail (multi != NULL);
  g_return_if_fail (SWFDEC_IS_AS_CONTEXT (context));
  g_return_if_fail (string != NULL);

  dot = strrchr (string, '.');
  if (dot != NULL) {
    char *s = g_strndup (string, dot - string);
    multi->ns = swfdec_as_context_get_namespace (context, SWFDEC_ABC_NAMESPACE_PUBLIC,
	NULL, swfdec_as_context_give_string (context, s));
    string = dot + 1;
  } else {
    multi->ns = context->public_ns;
  }
  multi->name = swfdec_as_context_get_string (context, string);
  multi->nsset = NULL;
}

SwfdecAbcNamespace *
swfdec_abc_multiname_get_namespace (const SwfdecAbcMultiname *mn, guint i)
{
  g_return_val_if_fail (mn != NULL, NULL);
  g_return_val_if_fail (i < swfdec_abc_multiname_get_n_namespaces (mn), NULL);

  if (mn->nsset) {
    return swfdec_abc_ns_set_get_namespace (mn->nsset, i);
  } else {
    g_assert (mn->ns > (SwfdecAbcNamespace *) SWFDEC_ABC_MULTINAME_ANY);
    return mn->ns;
  }
}

gboolean
swfdec_abc_multiname_contains_namespace	(const SwfdecAbcMultiname *mn,
    SwfdecAbcNamespace *ns)
{
  g_return_val_if_fail (mn != NULL, FALSE);
  g_return_val_if_fail (SWFDEC_IS_ABC_NAMESPACE (ns), FALSE);

  if (mn->nsset)
    return swfdec_abc_ns_set_contains (mn->nsset, ns);
  else
    return mn->ns == ns;
}
