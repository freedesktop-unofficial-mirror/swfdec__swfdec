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

#include <string.h>
#include "swfdec_net_stream_audio.h"
#include "swfdec_debug.h"
#include "swfdec_sprite.h"
#include "swfdec_tag.h"


G_DEFINE_TYPE (SwfdecNetStreamAudio, swfdec_net_stream_audio, SWFDEC_TYPE_AUDIO_STREAM)

static void
swfdec_net_stream_audio_dispose (GObject *object)
{
  SwfdecNetStreamAudio *stream = SWFDEC_NET_STREAM_AUDIO (object);

  if (stream->queue != NULL) {
    /* pop eventual NULL buffer indicating end of data */
    if (g_queue_peek_tail (stream->queue) == NULL)
      g_queue_pop_tail (stream->queue);
    g_queue_foreach (stream->queue, (GFunc) swfdec_buffer_unref, NULL);
    g_queue_free (stream->queue);
    stream->queue = NULL;
  }

  G_OBJECT_CLASS (swfdec_net_stream_audio_parent_class)->dispose (object);
}

static void
swfdec_net_stream_audio_clear (SwfdecAudio *audio)
{
  SwfdecNetStreamAudio *stream = SWFDEC_NET_STREAM_AUDIO (audio);

  /* pop eventual NULL buffer indicating end of data */
  if (g_queue_peek_tail (stream->queue) == NULL)
    g_queue_pop_tail (stream->queue);
  g_queue_foreach (stream->queue, (GFunc) swfdec_buffer_unref, NULL);
  g_queue_clear (stream->queue);

  SWFDEC_AUDIO_CLASS (swfdec_net_stream_audio_parent_class)->clear (audio);
}

static SwfdecBuffer *
swfdec_net_stream_audio_pull (SwfdecAudioStream *audio)
{
  SwfdecNetStreamAudio *stream = SWFDEC_NET_STREAM_AUDIO (audio);
  SwfdecBuffer *buffer, *result;
  SwfdecAudioFormat format;
  SwfdecBits bits;
  guint codec;

  if (g_queue_is_empty (stream->queue))
    return NULL;

  buffer = g_queue_pop_head (stream->queue);
  if (buffer == NULL) {
    swfdec_audio_stream_done (audio);
    return NULL;
  }

  swfdec_bits_init (&bits, buffer);
  codec = swfdec_bits_getbits (&bits, 4);
  format = swfdec_audio_format_parse (&bits);
  swfdec_audio_stream_use_decoder (audio, codec, format);
  result = swfdec_bits_get_buffer (&bits, -1);
  swfdec_buffer_unref (buffer);
  return result;
}

static void
swfdec_net_stream_audio_class_init (SwfdecNetStreamAudioClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecAudioClass *audio_class = SWFDEC_AUDIO_CLASS (klass);
  SwfdecAudioStreamClass *stream_class = SWFDEC_AUDIO_STREAM_CLASS (klass);

  object_class->dispose = swfdec_net_stream_audio_dispose;

  audio_class->clear = swfdec_net_stream_audio_clear;

  stream_class->pull = swfdec_net_stream_audio_pull;
}

static void
swfdec_net_stream_audio_init (SwfdecNetStreamAudio *stream)
{
  stream->queue = g_queue_new ();
}

SwfdecNetStreamAudio *
swfdec_net_stream_audio_new (SwfdecPlayer *player)
{
  SwfdecNetStreamAudio *stream;

  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), NULL);

  stream = g_object_new (SWFDEC_TYPE_NET_STREAM_AUDIO, NULL);

  return stream;
}

void
swfdec_net_stream_audio_push (SwfdecNetStreamAudio *audio, SwfdecBuffer *buffer)
{
  g_return_if_fail (SWFDEC_IS_NET_STREAM_AUDIO (audio));

  if (buffer && buffer->length < 2) {
    SWFDEC_WARNING ("buffer too small, ignoring");
    swfdec_buffer_unref (buffer);
    return;
  };
  if (!g_queue_is_empty (audio->queue) && g_queue_peek_tail (audio->queue) == NULL) {
    SWFDEC_ERROR ("pushing data onto an audio stream that is done. Ignoring.");
    if (buffer)
      swfdec_buffer_unref (buffer);
    return;
  }
  g_queue_push_tail (audio->queue, buffer);
}
