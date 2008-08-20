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

#ifndef _SWFDEC_SANDBOX_AS_H_
#define _SWFDEC_SANDBOX_AS_H_

#include <swfdec/swfdec_as_global.h>
#include <swfdec/swfdec_sandbox.h>
#include <swfdec/swfdec_url.h>
#include <swfdec/swfdec_types.h>

G_BEGIN_DECLS

typedef struct _SwfdecSandboxAs SwfdecSandboxAs;
typedef struct _SwfdecSandboxAsClass SwfdecSandboxAsClass;

#define SWFDEC_TYPE_SANDBOX_AS                    (swfdec_sandbox_as_get_type())
#define SWFDEC_IS_SANDBOX_AS(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_SANDBOX_AS))
#define SWFDEC_IS_SANDBOX_AS_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_SANDBOX_AS))
#define SWFDEC_SANDBOX_AS(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_SANDBOX_AS, SwfdecSandboxAs))
#define SWFDEC_SANDBOX_AS_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_SANDBOX_AS, SwfdecSandboxAsClass))
#define SWFDEC_SANDBOX_AS_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_SANDBOX_AS, SwfdecSandboxAsClass))

struct _SwfdecSandboxAs
{
  SwfdecAsGlobal	global;

  SwfdecSandboxType	type;			/* type of this sandbox_as */
  SwfdecURL *		url;			/* URL this sandbox_as acts for */
  guint			flash_version;		/* Flash version */

  /* global player objects */
  SwfdecAsObject *	MovieClip;		/* MovieClip object */
  SwfdecAsObject *	Video;			/* Video object */
};

struct _SwfdecSandboxAsClass
{
  SwfdecAsGlobalClass 	global_class;
};

GType			swfdec_sandbox_as_get_type		(void);


G_END_DECLS
#endif
