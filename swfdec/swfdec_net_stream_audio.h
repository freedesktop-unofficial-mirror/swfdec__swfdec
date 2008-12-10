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

#ifndef _SWFDEC_NET_STREAM_AUDIO_H_
#define _SWFDEC_NET_STREAM_AUDIO_H_

#include <swfdec/swfdec_audio_stream.h>
#include <swfdec/swfdec_net_stream.h>

G_BEGIN_DECLS

//typedef struct _SwfdecNetStreamAudio SwfdecNetStreamAudio;
typedef struct _SwfdecNetStreamAudioClass SwfdecNetStreamAudioClass;

#define SWFDEC_TYPE_NET_STREAM_AUDIO                    (swfdec_net_stream_audio_get_type())
#define SWFDEC_IS_NET_STREAM_AUDIO(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_NET_STREAM_AUDIO))
#define SWFDEC_IS_NET_STREAM_AUDIO_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_NET_STREAM_AUDIO))
#define SWFDEC_NET_STREAM_AUDIO(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_NET_STREAM_AUDIO, SwfdecNetStreamAudio))
#define SWFDEC_NET_STREAM_AUDIO_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_NET_STREAM_AUDIO, SwfdecNetStreamAudioClass))
#define SWFDEC_NET_STREAM_AUDIO_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_NET_STREAM_AUDIO, SwfdecNetStreamAudioClass))

struct _SwfdecNetStreamAudio
{
  SwfdecAudioStream	stream;

  GQueue *		queue;		/* not yet decoded buffers (NULL at the end means we're done) */
};

struct _SwfdecNetStreamAudioClass
{
  SwfdecAudioStreamClass stream_class;
};

GType	  		swfdec_net_stream_audio_get_type	(void);

SwfdecNetStreamAudio *	swfdec_net_stream_audio_new		(SwfdecPlayer *		player);

void			swfdec_net_stream_audio_push		(SwfdecNetStreamAudio *	audio,
								 SwfdecBuffer *		buffer);


G_END_DECLS
#endif
