/* Swfdec
 * Copyright (C) 2007 Benjamin Otte <otte@gnome.org>
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

#include <string.h>
#include "swfdec_security.h"
#include "swfdec_debug.h"


G_DEFINE_ABSTRACT_TYPE (SwfdecSecurity, swfdec_security, G_TYPE_OBJECT)

static void
swfdec_security_class_init (SwfdecSecurityClass *klass)
{
}

static void
swfdec_security_init (SwfdecSecurity *security)
{
}

/**
 * swfdec_security_allow:
 * @guard: security guarding an operation
 * @key: security available
 *
 * Asks @guard to check if the given @key allows accessing it. If so, a
 * key for accessing the operation is returned.
 *
 * Returns: %NULL if access was not granted, otherwise the new security 
 *          priviliges for accessing the operation. Use g_object_unref() after
 *          use.
 **/
SwfdecSecurity *
swfdec_security_allow (SwfdecSecurity *guard, SwfdecSecurity *key)
{
  SwfdecSecurityClass *klass;

  g_return_val_if_fail (SWFDEC_IS_SECURITY (guard), NULL);
  g_return_val_if_fail (SWFDEC_IS_SECURITY (key), NULL);

  klass = SWFDEC_SECURITY_GET_CLASS (guard);
  g_return_val_if_fail (klass->allow, NULL);
  return klass->allow (guard, key);
}

/**
 * swfdec_security_allow_url:
 * @guard: security that is in effect
 * @url: URL that should be accessed
 *
 * Asks @guard to check if the given @url may be accessed.
 *
 * Returns: %TRUE if @url may be accessed.
 **/
gboolean
swfdec_security_allow_url (SwfdecSecurity *guard, const SwfdecURL *url)
{
  SwfdecSecurityClass *klass;

  g_return_val_if_fail (SWFDEC_IS_SECURITY (guard), FALSE);
  g_return_val_if_fail (url != NULL, FALSE);

  klass = SWFDEC_SECURITY_GET_CLASS (guard);
  g_return_val_if_fail (klass->allow_url, FALSE);
  return klass->allow_url (guard, url);
}

