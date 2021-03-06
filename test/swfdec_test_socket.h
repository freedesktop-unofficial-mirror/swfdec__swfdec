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

#ifndef _SWFDEC_TEST_SOCKET_H_
#define _SWFDEC_TEST_SOCKET_H_

#include "swfdec_test_plugin.h"
#include "swfdec_test_test.h"
#include <swfdec/swfdec.h>

G_BEGIN_DECLS


typedef struct _SwfdecTestSocket SwfdecTestSocket;
typedef struct _SwfdecTestSocketClass SwfdecTestSocketClass;

#define SWFDEC_TYPE_TEST_SOCKET                    (swfdec_test_socket_get_type())
#define SWFDEC_IS_TEST_SOCKET(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_TEST_SOCKET))
#define SWFDEC_IS_TEST_SOCKET_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_TEST_SOCKET))
#define SWFDEC_TEST_SOCKET(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_TEST_SOCKET, SwfdecTestSocket))
#define SWFDEC_TEST_SOCKET_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_TEST_SOCKET, SwfdecTestSocketClass))
#define SWFDEC_TEST_SOCKET_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_TEST_SOCKET, SwfdecTestSocketClass))

struct _SwfdecTestSocket
{
  SwfdecAsRelay			relay;

  SwfdecTestPluginSocket *	plugin;		/* the socket we manage or NULL if closed */
  SwfdecTestTest *		test;		/* the test that spawned us */
};

struct _SwfdecTestSocketClass
{
  SwfdecAsRelayClass		relay_class;
};

GType			swfdec_test_socket_get_type	(void);

SwfdecTestSocket *	swfdec_test_socket_new		(SwfdecTestTest *		test,
							 SwfdecTestPluginSocket *	sock);
void			swfdec_test_socket_close	(SwfdecTestSocket *		sock);

G_END_DECLS
#endif
