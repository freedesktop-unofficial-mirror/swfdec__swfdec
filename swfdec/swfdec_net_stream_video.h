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

#ifndef _SWFDEC_NET_STREAM_VIDEO_H_
#define _SWFDEC_NET_STREAM_VIDEO_H_

#include <swfdec/swfdec_gc_object.h>
#include <swfdec/swfdec_net_stream.h>
#include <swfdec/swfdec_player_internal.h>
#include <swfdec/swfdec_rtmp_header.h>
#include <swfdec/swfdec_video_decoder.h>
#include <swfdec/swfdec_video_provider.h>

G_BEGIN_DECLS

//typedef struct _SwfdecNetStreamVideo SwfdecNetStreamVideo;
typedef struct _SwfdecNetStreamVideoClass SwfdecNetStreamVideoClass;

#define SWFDEC_TYPE_NET_STREAM_VIDEO                    (swfdec_net_stream_video_get_type())
#define SWFDEC_IS_NET_STREAM_VIDEO(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_NET_STREAM_VIDEO))
#define SWFDEC_IS_NET_STREAM_VIDEO_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_NET_STREAM_VIDEO))
#define SWFDEC_NET_STREAM_VIDEO(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_NET_STREAM_VIDEO, SwfdecNetStreamVideo))
#define SWFDEC_NET_STREAM_VIDEO_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_NET_STREAM_VIDEO, SwfdecNetStreamVideoClass))

struct _SwfdecNetStreamVideo {
  SwfdecGcObject		object;

  gulong			buffer_time;	/* time to buffer before starting to play */
  GQueue *			next;		/* queue of pending packets */
  gulong			next_length;	/* number of milliseconds in the next queue */
  gboolean			playing;	/* TRUE if we're currently playing */
  SwfdecTimeout			timeout;	/* time the next image should be decoded */
  SwfdecVideoDecoder *		decoder;	/* the current decoder */
};

struct _SwfdecNetStreamVideoClass {
  SwfdecGcObjectClass		object_class;
};

GType			swfdec_net_stream_video_get_type	(void);

SwfdecNetStreamVideo *	swfdec_net_stream_video_new		(SwfdecPlayer *		player);

void			swfdec_net_stream_video_push		(SwfdecNetStreamVideo *	video,
								 const SwfdecRtmpHeader *header,
								 SwfdecBuffer *		buffer);


G_END_DECLS
#endif
