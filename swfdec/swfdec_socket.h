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

#ifndef _SWFDEC_SOCKET_H_
#define _SWFDEC_SOCKET_H_

#include <glib-object.h>
#include <swfdec/swfdec_stream.h>
#include <swfdec/swfdec_player.h>

G_BEGIN_DECLS

typedef struct _SwfdecSocket SwfdecSocket;
typedef struct _SwfdecSocketClass SwfdecSocketClass;

#define SWFDEC_TYPE_SOCKET                    (swfdec_socket_get_type())
#define SWFDEC_IS_SOCKET(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_SOCKET))
#define SWFDEC_IS_SOCKET_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_SOCKET))
#define SWFDEC_SOCKET(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_SOCKET, SwfdecSocket))
#define SWFDEC_SOCKET_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_SOCKET, SwfdecSocketClass))
#define SWFDEC_SOCKET_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_SOCKET, SwfdecSocketClass))

struct _SwfdecSocket
{
  SwfdecStream		stream;
};

struct _SwfdecSocketClass
{
  /*< private >*/
  SwfdecStreamClass   	stream_class;

  /*< public >*/
  void			(* connect)		(SwfdecSocket *	socket,
						 SwfdecPlayer *	player,
						 const char *	hostname,
						 guint		port);

  gsize			(* send)		(SwfdecSocket *	socket,
						 SwfdecBuffer *	buffer);
};

GType		swfdec_socket_get_type		(void);

void		swfdec_socket_signal_writable	(SwfdecSocket *	sock);


G_END_DECLS
#endif
