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

#ifndef _SWFDEC_SANDBOX_ABC_H_
#define _SWFDEC_SANDBOX_ABC_H_

#include <swfdec/swfdec_abc_global.h>
#include <swfdec/swfdec_sandbox.h>
#include <swfdec/swfdec_url.h>

G_BEGIN_DECLS

typedef struct _SwfdecSandboxAbc SwfdecSandboxAbc;
typedef struct _SwfdecSandboxAbcClass SwfdecSandboxAbcClass;

#define SWFDEC_TYPE_SANDBOX_ABC                    (swfdec_sandbox_abc_get_type())
#define SWFDEC_IS_SANDBOX_ABC(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_SANDBOX_ABC))
#define SWFDEC_IS_SANDBOX_ABC_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_SANDBOX_ABC))
#define SWFDEC_SANDBOX_ABC(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_SANDBOX_ABC, SwfdecSandboxAbc))
#define SWFDEC_SANDBOX_ABC_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_SANDBOX_ABC, SwfdecSandboxAbcClass))
#define SWFDEC_SANDBOX_ABC_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_SANDBOX_ABC, SwfdecSandboxAbcClass))

struct _SwfdecSandboxAbc
{
  SwfdecAbcGlobal	global;

  SwfdecSandboxType	type;			/* type of this sandbox_abc */
  SwfdecURL *		url;			/* URL this sandbox_abc acts for */
};

struct _SwfdecSandboxAbcClass
{
  SwfdecAbcGlobalClass 	global_class;
};

GType			swfdec_sandbox_abc_get_type		(void);


G_END_DECLS
#endif
