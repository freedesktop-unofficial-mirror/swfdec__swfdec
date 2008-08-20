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

#ifndef __SWFDEC_SANDBOX_H__
#define __SWFDEC_SANDBOX_H__

#include <swfdec/swfdec.h>
#include <swfdec/swfdec_types.h>

G_BEGIN_DECLS


typedef enum {
  SWFDEC_SANDBOX_NONE,
  SWFDEC_SANDBOX_REMOTE,
  SWFDEC_SANDBOX_LOCAL_FILE,
  SWFDEC_SANDBOX_LOCAL_NETWORK,
  SWFDEC_SANDBOX_TRUSTED
} SwfdecSandboxType;

#define SWFDEC_TYPE_SANDBOX_TYPE           (swfdec_sandbox_type_get_type ())

#define SWFDEC_TYPE_SANDBOX                (swfdec_sandbox_get_type ())
#define SWFDEC_SANDBOX(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_SANDBOX, SwfdecSandbox))
#define SWFDEC_IS_SANDBOX(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_SANDBOX))
#define SWFDEC_SANDBOX_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), SWFDEC_TYPE_SANDBOX, SwfdecSandboxInterface))

//typedef struct _SwfdecSandbox SwfdecSandbox;
typedef struct _SwfdecSandboxInterface SwfdecSandboxInterface;

struct _SwfdecSandboxInterface {
  GTypeInterface	parent;
};

GType			swfdec_sandbox_type_get_type	(void) G_GNUC_CONST;
GType			swfdec_sandbox_get_type		(void) G_GNUC_CONST;

SwfdecSandbox *		swfdec_sandbox_as_get		(SwfdecPlayer *	  	player,
							 const SwfdecURL *	url,
							 guint			flash_version,
							 gboolean		allow_network);
SwfdecSandbox *		swfdec_sandbox_abc_get		(SwfdecPlayer *		player,
							 const SwfdecURL *	url,
							 guint			flash_version,
							 gboolean		allow_network);

SwfdecSandboxType	swfdec_sandbox_get_sandbox_type	(SwfdecSandbox *	sandbox);
SwfdecURL *		swfdec_sandbox_get_url		(SwfdecSandbox *	sandbox);

void			swfdec_sandbox_use		(SwfdecSandbox *	sandbox);
gboolean		swfdec_sandbox_try_use		(SwfdecSandbox *	sandbox);
void			swfdec_sandbox_unuse		(SwfdecSandbox *	sandbox);


G_END_DECLS

#endif /* __SWFDEC_SANDBOX_H__ */
