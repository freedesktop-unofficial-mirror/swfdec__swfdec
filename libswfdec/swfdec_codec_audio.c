/* Swfdec
 * Copyright (C) 2006-2007 Benjamin Otte <otte@gnome.org>
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

#include "swfdec_codec_audio.h"
#include "swfdec_debug.h"
#include "swfdec_internal.h"

/*** UNCOMPRESSED SOUND ***/

typedef struct {
  SwfdecAudioDecoder	decoder;
  SwfdecBufferQueue *	queue;		/* queue collecting output buffers */
} SwfdecAudioDecoderUncompressed;

static void
swfdec_audio_decoder_uncompressed_decode_8bit (SwfdecAudioDecoder *decoder, 
    SwfdecBuffer *buffer)
{
  SwfdecBuffer *ret;
  gint16 *out;
  guint8 *in;
  guint i;

  if (buffer == NULL)
    return;

  ret = swfdec_buffer_new_and_alloc (buffer->length * 2);
  out = (gint16 *) ret->data;
  in = buffer->data;
  for (i = 0; i < buffer->length; i++) {
    *out = ((gint16) *in << 8) ^ (-1);
    out++;
    in++;
  }
  swfdec_buffer_queue_push (((SwfdecAudioDecoderUncompressed *) decoder)->queue, ret);
}

static void
swfdec_audio_decoder_uncompressed_decode_16bit (SwfdecAudioDecoder *decoder, 
    SwfdecBuffer *buffer)
{
  if (buffer == NULL)
    return;

  swfdec_buffer_ref (buffer);
  swfdec_buffer_queue_push (((SwfdecAudioDecoderUncompressed *) decoder)->queue, buffer);
}

static SwfdecBuffer *
swfdec_audio_decoder_uncompressed_pull (SwfdecAudioDecoder *decoder)
{
  SwfdecAudioDecoderUncompressed *dec = (SwfdecAudioDecoderUncompressed *) decoder;
  
  return swfdec_buffer_queue_pull_buffer (dec->queue);
}

static void
swfdec_audio_decoder_uncompressed_free (SwfdecAudioDecoder *decoder)
{
  SwfdecAudioDecoderUncompressed *dec = (SwfdecAudioDecoderUncompressed *) decoder;

  swfdec_buffer_queue_unref (dec->queue);
  g_free (dec);
}

static SwfdecAudioDecoder *
swfdec_audio_decoder_uncompressed_new (SwfdecAudioCodec type, SwfdecAudioFormat format)
{
  SwfdecAudioDecoderUncompressed *dec;

  if (type != SWFDEC_AUDIO_CODEC_UNDEFINED &&
      type != SWFDEC_AUDIO_CODEC_UNCOMPRESSED)
    return NULL;
  if (type == SWFDEC_AUDIO_CODEC_UNDEFINED) {
    SWFDEC_WARNING ("endianness of audio unknown, assuming little endian");
  }
  dec = g_new (SwfdecAudioDecoderUncompressed, 1);
  dec->decoder.format = format;
  if (swfdec_audio_format_is_16bit (format))
    dec->decoder.push = swfdec_audio_decoder_uncompressed_decode_16bit;
  else
    dec->decoder.push = swfdec_audio_decoder_uncompressed_decode_8bit;
  dec->decoder.pull = swfdec_audio_decoder_uncompressed_pull;
  dec->decoder.free = swfdec_audio_decoder_uncompressed_free;
  dec->queue = swfdec_buffer_queue_new ();

  return &dec->decoder;
}

/*** PUBLIC API ***/

static SwfdecAudioDecoder *
swfdec_audio_decoder_builtin_new (SwfdecAudioCodec codec, SwfdecAudioFormat format)
{
  SwfdecAudioDecoder *ret;

  ret = swfdec_audio_decoder_uncompressed_new (codec, format);
  if (ret == NULL)
    ret = swfdec_audio_decoder_adpcm_new (codec, format);

  return ret;
}

struct {
  const char *		name;
  SwfdecAudioDecoder *	(* func) (SwfdecAudioCodec, SwfdecAudioFormat);
} audio_codecs[] = {
  { "builtin",	swfdec_audio_decoder_builtin_new },
#ifdef HAVE_MAD
  { "mad",	swfdec_audio_decoder_mad_new },
#endif
#ifdef HAVE_GST
  { "gst",	swfdec_audio_decoder_gst_new },
#endif
#ifdef HAVE_FFMPEG
  { "ffmpeg",	swfdec_audio_decoder_ffmpeg_new },
#endif
  { NULL, }
};

/**
 * swfdec_audio_decoder_new:
 * @format: #SwfdecAudioCodec to decode
 *
 * Creates a decoder suitable for decoding @format. If no decoder is available
 * for the given for mat, %NULL is returned.
 *
 * Returns: a new decoder or %NULL
 **/
SwfdecAudioDecoder *
swfdec_audio_decoder_new (SwfdecAudioCodec codec, SwfdecAudioFormat format)
{
  SwfdecAudioDecoder *ret;
  const char *list;

  g_return_val_if_fail (SWFDEC_IS_AUDIO_FORMAT (format), NULL);

  list = g_getenv ("SWFDEC_CODEC_AUDIO");
  if (list == NULL)
    list = g_getenv ("SWFDEC_CODEC");
  if (list == NULL) {
    guint i;
    ret = NULL;
    for (i = 0; audio_codecs[i].name != NULL; i++) {
      ret = audio_codecs[i].func (codec, format);
      if (ret)
	break;
    }
  } else {
    char **split = g_strsplit (list, ":", -1);
    guint i, j;
    ret = NULL;
    SWFDEC_LOG ("codecs limited to \"%s\"", list);
    for (i = 0; split[i] != NULL && ret == NULL; i++) {
      for (j = 0; audio_codecs[j].name != NULL; j++) {
	if (g_ascii_strcasecmp (audio_codecs[j].name, split[i]) != 0)
	  continue;
	ret = audio_codecs[j].func (codec, format);
	if (ret)
	  break;
      }
    }
    g_strfreev (split);
  }

  if (ret) {
    ret->codec = codec;
    g_return_val_if_fail (SWFDEC_IS_AUDIO_FORMAT (ret->format), NULL);
    g_return_val_if_fail (ret->push, NULL);
    g_return_val_if_fail (ret->pull, NULL);
    g_return_val_if_fail (ret->free, NULL);
  } else {
    SWFDEC_ERROR ("no suitable decoder for audio codec %u", codec);
    return NULL;
  }
  return ret;
}

/**
 * swfdec_audio_decoder_free:
 * @decoder: a #SwfdecAudioDecoder
 *
 * Frees the given decoder. When finishing decoding, be sure to pass a %NULL
 * buffer to swfdec_audio_decoder_push() first to flush the decoder. See that
 * function for details.
 **/
void
swfdec_audio_decoder_free (SwfdecAudioDecoder *decoder)
{
  g_return_if_fail (decoder != NULL);

  decoder->free (decoder);
}

/**
 * swfdec_audio_decoder_get_format:
 * @decoder: a #SwfdecAudioDecoder
 *
 * Queries the format that is used by the decoder for its produced output.
 *
 * Returns: the format of the decoded data
 **/
SwfdecAudioFormat
swfdec_audio_decoder_get_format	(SwfdecAudioDecoder *decoder)
{
  g_return_val_if_fail (decoder != NULL, 0);

  return decoder->format;
}

/**
 * swfdec_audio_decoder_push:
 * @decoder: a #SwfdecAudioDecoder
 * @buffer: a #SwfdecBuffer to process or %NULL to flush
 *
 * Pushes a new buffer into the decoding pipeline. After this the results can
 * be queried using swfdec_audio_decoder_pull(). Some decoders may not decode
 * all available data immediately. So when you are done decoding, you may want
 * to flush the decoder. Flushing can be achieved by passing %NULL as the 
 * @buffer argument. Do this when you are finished decoding.
 **/
void
swfdec_audio_decoder_push (SwfdecAudioDecoder *decoder, SwfdecBuffer *buffer)
{
  g_return_if_fail (decoder != NULL);

  decoder->push (decoder, buffer);
}

/**
 * swfdec_audio_decoder_pull:
 * @decoder: a #SwfdecAudioDecoder
 *
 * Gets the next buffer of decoded audio data. Since some decoders do not
 * produce one output buffer per input buffer, any number of buffers may be
 * available after calling swfdec_audio_decoder_push(), even none. When no more
 * buffers are available, this function returns %NULL. You need to provide more
 * input in then. A simple decoding pipeline would look like this:
 * <informalexample><programlisting>do {
 *   input = next_input_buffer ();
 *   swfdec_audio_decoder_push (decoder, input);
 *   while ((output = swfdec_audio_decoder_pull (decoder))) {
 *     ... process output ...
 *   }
 * } while (input != NULL); </programlisting></informalexample>
 *
 * Returns: the next buffer or %NULL if no more buffers are available.
 **/
SwfdecBuffer *
swfdec_audio_decoder_pull (SwfdecAudioDecoder *decoder)
{
  g_return_val_if_fail (decoder != NULL, NULL);

  return decoder->pull (decoder);
}

