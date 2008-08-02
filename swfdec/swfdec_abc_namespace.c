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
#include "swfdec_as_context.h"
#include "swfdec_debug.h"

G_DEFINE_TYPE (SwfdecAbcNamespace, swfdec_abc_namespace, SWFDEC_TYPE_GC_OBJECT)

static void
swfdec_abc_namespace_dispose (GObject *object)
{
  //SwfdecAbcNamespace *ns = SWFDEC_ABC_NAMESPACE (object);

  G_OBJECT_CLASS (swfdec_abc_namespace_parent_class)->dispose (object);
}

static void
swfdec_abc_namespace_mark (SwfdecGcObject *object)
{
  SwfdecAbcNamespace *ns = SWFDEC_ABC_NAMESPACE (object);

  if (ns->prefix)
    swfdec_as_string_mark (ns->prefix);
  swfdec_as_string_mark (ns->uri);

  SWFDEC_GC_OBJECT_CLASS (swfdec_abc_namespace_parent_class)->mark (object);
}

static void
swfdec_abc_namespace_class_init (SwfdecAbcNamespaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_abc_namespace_dispose;

  gc_class->mark = swfdec_abc_namespace_mark;
}

static void
swfdec_abc_namespace_init (SwfdecAbcNamespace *date)
{
}

SwfdecAbcNamespace *
swfdec_abc_namespace_new (SwfdecAsContext *context, SwfdecAbcNamespaceType type,
    const char *prefix, const char *uri)
{
  SwfdecAbcNamespace *ns;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), NULL);
  g_return_val_if_fail (uri != NULL, NULL);

  ns = g_object_new (SWFDEC_TYPE_ABC_NAMESPACE, "context", context, NULL);

  ns->type = type;
  ns->prefix = prefix;
  ns->uri = uri;

  return ns;
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
