/* Swfdec
 * Copyright (C) 2007-2008 Benjamin Otte <otte@gnome.org>
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

#include "swfdec_sandbox.h"

#include "swfdec_debug.h"
#include "swfdec_loader_internal.h"
#include "swfdec_player_internal.h"
#include "swfdec_sandbox_abc.h"
#include "swfdec_sandbox_as.h"

/* enumerations from "swfdec_as_context.h" */
GType
swfdec_sandbox_type_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { SWFDEC_SANDBOX_NONE, "SWFDEC_SANDBOX_NONE", "none" },
      { SWFDEC_SANDBOX_REMOTE, "SWFDEC_SANDBOX_REMOTE", "remote" },
      { SWFDEC_SANDBOX_LOCAL_FILE, "SWFDEC_SANDBOX_LOCAL_FILE", "local-file" },
      { SWFDEC_SANDBOX_LOCAL_NETWORK, "SWFDEC_SANDBOX_LOCAL_NETWORK", "local-network" },
      { SWFDEC_SANDBOX_TRUSTED, "SWFDEC_SANDBOX_TRUSTED", "trusted" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static (g_intern_static_string ("SwfdecSandboxType"), values);
  }
  return etype;
}

#define SWFDEC_FLASH_TO_AS_VERSION(ver) ((ver) < 7 ? 1 : 2)
static void
swfdec_sandbox_class_init (gpointer g_class, gpointer class_data)
{
  g_object_interface_install_property (g_class,
      g_param_spec_enum ("sandbox-type", "sandbox type", "type of sandbox",
	  SWFDEC_TYPE_SANDBOX_TYPE, SWFDEC_SANDBOX_NONE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_interface_install_property (g_class,
      g_param_spec_boxed ("url", "url", "host url handled by this sandbox",
	  SWFDEC_TYPE_URL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

GType
swfdec_sandbox_get_type (void)
{
  static GType sandbox_type = 0;
  
  if (!sandbox_type) {
    static const GTypeInfo sandbox_info = {
      sizeof (SwfdecSandboxInterface),
      NULL,
      NULL,
      swfdec_sandbox_class_init,
      NULL,
      NULL,
      0,
      0,
      NULL,
    };
    
    sandbox_type = g_type_register_static (G_TYPE_INTERFACE,
        "SwfdecSandbox", &sandbox_info, 0);
    g_type_interface_add_prerequisite (sandbox_type, SWFDEC_TYPE_AS_OBJECT);
  }
  
  return sandbox_type;
}

/**
 * swfdec_sandbox_compute_sandbox_type:
 * @url: the url to check
 * @network: %TRUE if network access is possible
 *
 * Checks what sandbox type belongs to the given @url and @network.
 *
 * Returns: The computed #SwfdecSandboxType.
 **/
static SwfdecSandboxType
swfdec_sandbox_compute_sandbox_type (const SwfdecURL *url, gboolean network)
{
  g_return_val_if_fail (url != NULL, SWFDEC_SANDBOX_NONE);

  if (swfdec_url_is_local (url)) {
    return network ? SWFDEC_SANDBOX_LOCAL_NETWORK : SWFDEC_SANDBOX_LOCAL_FILE;
  } else {
    return SWFDEC_SANDBOX_REMOTE;
  }
}

static SwfdecSandbox *
swfdec_sandbox_get (SwfdecPlayer *player, const SwfdecURL *url,
    guint as_version, guint flash_version, gboolean allow_network)
{
  SwfdecPlayerPrivate *priv;
  SwfdecSandbox *sandbox;
  SwfdecURL *real;
  GSList *walk;

  priv = player->priv;
  real = swfdec_url_new_components (swfdec_url_get_protocol (url),
      swfdec_url_get_host (url), swfdec_url_get_port (url), NULL, NULL);

  for (walk = priv->sandboxes; walk; walk = walk->next) {
    sandbox = walk->data;
    if ((as_version < 3 && SWFDEC_IS_SANDBOX_AS (sandbox) &&
	 SWFDEC_FLASH_TO_AS_VERSION (SWFDEC_SANDBOX_AS (sandbox)->flash_version) == as_version) ||
	(as_version > 2 && SWFDEC_IS_SANDBOX_ABC (sandbox))) {
      SwfdecURL *sandbox_url = swfdec_sandbox_get_url (sandbox);
      if (swfdec_url_equal (sandbox_url, real)) {
	swfdec_url_free (sandbox_url);
	break;
      }
      swfdec_url_free (sandbox_url);
    }
  }

  if (walk) {
    /* FIXME: Do we need to check globally for network/file, not per as_version? */
    if (swfdec_sandbox_get_sandbox_type (sandbox) != 
	swfdec_sandbox_compute_sandbox_type (real, allow_network))
      sandbox = NULL;
  } else {
    SwfdecAsContext *context = SWFDEC_AS_CONTEXT (player);

    sandbox = g_object_new (as_version > 2 ? SWFDEC_TYPE_SANDBOX_ABC : SWFDEC_TYPE_SANDBOX_AS,
	"context", context, "sandbox-type", swfdec_sandbox_compute_sandbox_type (real, allow_network),
	"url", real, as_version < 3 ? "version" : NULL, flash_version, NULL);
    priv->sandboxes = g_slist_append (priv->sandboxes, sandbox);
    swfdec_sandbox_unuse (sandbox);
  }

  swfdec_url_free (real);
  return sandbox;
}

/**
 * swfdec_sandbox_abc_get:
 * @player: a #SwfdecPlayer
 * @flash_version: The Flash version for looking up the sandbox
 * @url: the URL this player refers to
 * @allow_network: %TRUE to allow network access, %FALSE to only allow local 
 *                 file access. See the documentation of the use_network flag 
 *                 of the SWF FileAttributes tag for what that means.
 *
 * Checks if a sandbox is already in use for a given URL and if so, returns it.
 * Otherwise a new sandbox is created, initialized and returned.
 * Note that the given url must be a HTTP, HTTPS or a FILE url.
 *
 * Returns: the sandbox corresponding to the given URL or %NULL if no such 
 *          sandbox is allowed.
 */
SwfdecSandbox *
swfdec_sandbox_abc_get (SwfdecPlayer *player, const SwfdecURL *url,
    guint flash_version, gboolean allow_network)
{
  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), NULL);
  g_return_val_if_fail (url != NULL, NULL);

  return swfdec_sandbox_get (player, url, 3, flash_version, allow_network);
}


/**
 * swfdec_sandbox_as_get:
 * @player: a #SwfdecPlayer
 * @url: the URL this player refers to
 * @flash_version: The Flash version for looking up the sandbox
 * @allow_network: %TRUE to allow network access, %FALSE to only allow local 
 *                 file access. See the documentation of the use_network flag 
 *                 of the SWF FileAttributes tag for what that means.
 *
 * Checks if a sandbox is already in use for a given URL and if so, returns it.
 * Otherwise a new sandbox is created, initialized and returned.
 * Note that the given url must be a HTTP, HTTPS or a FILE url.
 *
 * Returns: the sandbox corresponding to the given URL or %NULL if no such 
 *          sandbox is allowed.
 **/
SwfdecSandbox *
swfdec_sandbox_as_get (SwfdecPlayer *player, const SwfdecURL *url,
    guint flash_version, gboolean allow_network)
{
  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), NULL);
  g_return_val_if_fail (url != NULL, NULL);

  return swfdec_sandbox_get (player, url, SWFDEC_FLASH_TO_AS_VERSION (flash_version), flash_version, allow_network);
}

SwfdecSandboxType
swfdec_sandbox_get_sandbox_type (SwfdecSandbox *sandbox)
{
  SwfdecSandboxType type = SWFDEC_SANDBOX_NONE;

  g_return_val_if_fail (SWFDEC_IS_SANDBOX (sandbox), SWFDEC_SANDBOX_NONE);

  g_object_get (sandbox, "sandbox-type", &type, NULL);
  return type;
}

SwfdecURL *
swfdec_sandbox_get_url (SwfdecSandbox *sandbox)
{
  SwfdecURL *url;

  g_return_val_if_fail (SWFDEC_IS_SANDBOX (sandbox), NULL);

  g_object_get (sandbox, "url", &url, NULL);
  return url;
}

/**
 * swfdec_sandbox_use:
 * @sandbox: the sandbox to use when executing scripts
 *
 * Sets @sandbox to be used for scripts that are going to be executed next. No
 * sandbox may be set yet. You must unset the sandbox with 
 * swfdec_sandbox_unuse() after calling your script.
 **/
void
swfdec_sandbox_use (SwfdecSandbox *sandbox)
{
  SwfdecAsContext *context;

  g_return_if_fail (SWFDEC_IS_SANDBOX (sandbox));
  g_return_if_fail (swfdec_sandbox_get_sandbox_type (sandbox) != SWFDEC_SANDBOX_NONE);
  g_return_if_fail (swfdec_gc_object_get_context (sandbox)->global == NULL);

  context = swfdec_gc_object_get_context (sandbox);
  context->global = SWFDEC_AS_OBJECT (sandbox);
}

/**
 * swfdec_sandbox_try_use:
 * @sandbox: the sandbox to use
 *
 * Makes sure a sandbox is in use. If no sandbox is in use currently, use the
 * given @sandbox. This function is intended for cases where code can be called
 * from both inside scripts with a sandbox already set or outside with no 
 * sandbox in use.
 *
 * Returns: %TRUE if the new sandbox will be used. You need to call 
 *          swfdec_sandbox_unuse() afterwards. %FALSE if a sandbox is already in
 *          use.
 **/
gboolean
swfdec_sandbox_try_use (SwfdecSandbox *sandbox)
{
  g_return_val_if_fail (SWFDEC_IS_SANDBOX (sandbox), FALSE);
  g_return_val_if_fail (swfdec_sandbox_get_sandbox_type (sandbox) != SWFDEC_SANDBOX_NONE, FALSE);

  if (swfdec_gc_object_get_context (sandbox)->global)
    return FALSE;

  swfdec_sandbox_use (sandbox);
  return TRUE;
}

/**
 * swfdec_sandbox_unuse:
 * @sandbox: a #SwfdecSandbox
 *
 * Unsets the sandbox as the current sandbox for executing scripts.
 **/
void
swfdec_sandbox_unuse (SwfdecSandbox *sandbox)
{
  SwfdecAsContext *context;

  g_return_if_fail (SWFDEC_IS_SANDBOX (sandbox));
  g_return_if_fail (swfdec_gc_object_get_context (sandbox)->global == SWFDEC_AS_OBJECT (sandbox));

  context = swfdec_gc_object_get_context (sandbox);
  context->global = NULL;
}

