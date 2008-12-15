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

#include <stdlib.h>

#include "swfdec_net_stream_video.h"
#include "swfdec_cached_video.h"
#include "swfdec_debug.h"
#include "swfdec_font.h"
#include "swfdec_player_internal.h"
#include "swfdec_renderer_internal.h"
#include "swfdec_swf_decoder.h"

/*** VIDEO PROVIDER INTERFACE ***/

static cairo_surface_t *
swfdec_net_stream_video_get_image (SwfdecVideoProvider *prov,
    SwfdecRenderer *renderer, guint *width, guint *height)
{
  SwfdecNetStreamVideo *video = SWFDEC_NET_STREAM_VIDEO (prov);
  cairo_surface_t *surface;

  if (video->decoder == NULL)
    return NULL;

  surface = swfdec_video_decoder_get_image (video->decoder, renderer);
  if (surface == NULL)
    return NULL;

  *width = swfdec_video_decoder_get_width (video->decoder);
  *height = swfdec_video_decoder_get_height (video->decoder);
  return surface;
}

static void
swfdec_net_stream_video_get_size (SwfdecVideoProvider *prov, guint *width, guint *height)
{
  SwfdecNetStreamVideo *video = SWFDEC_NET_STREAM_VIDEO (prov);

  if (video->decoder) {
    *width = swfdec_video_decoder_get_width (video->decoder);
    *height = swfdec_video_decoder_get_height (video->decoder);
  } else {
    *width = 0;
    *height = 0;
  }
}

static void
swfdec_net_stream_video_video_provider_init (SwfdecVideoProviderInterface *iface)
{
  iface->get_image = swfdec_net_stream_video_get_image;
  iface->get_size = swfdec_net_stream_video_get_size;
}

/*** SWFDEC_NET_STREAM_VIDEO ***/

enum {
  PROP_0,
  PROP_PLAYING
};

G_DEFINE_TYPE_WITH_CODE (SwfdecNetStreamVideo, swfdec_net_stream_video, SWFDEC_TYPE_GC_OBJECT,
    G_IMPLEMENT_INTERFACE (SWFDEC_TYPE_VIDEO_PROVIDER, swfdec_net_stream_video_video_provider_init))

static void
swfdec_net_stream_video_get_property (GObject *object, guint param_id, GValue *value, 
    GParamSpec * pspec)
{
  SwfdecNetStreamVideo *video = SWFDEC_NET_STREAM_VIDEO (object);
  
  switch (param_id) {
    case PROP_PLAYING:
      g_value_set_boolean (value, video->playing);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
swfdec_net_stream_video_dispose (GObject *object)
{
  SwfdecNetStreamVideo *video = SWFDEC_NET_STREAM_VIDEO (object);

  if (video->decoder != NULL) {
    g_object_unref (video->decoder);
    video->decoder = NULL;
  }
  if (video->next) {
    g_queue_foreach (video->next, (GFunc) swfdec_rtmp_packet_free, NULL);
    g_queue_free (video->next);
    video->next = NULL;
  }
  if (video->timeout.callback) {
    swfdec_player_remove_timeout (SWFDEC_PLAYER (swfdec_gc_object_get_context (video)),
	&video->timeout);
    video->timeout.callback = NULL;
  }

  G_OBJECT_CLASS (swfdec_net_stream_video_parent_class)->dispose (object);
}

static void
swfdec_net_stream_video_class_init (SwfdecNetStreamVideoClass * g_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (g_class);

  object_class->dispose = swfdec_net_stream_video_dispose;
  object_class->get_property = swfdec_net_stream_video_get_property;

  g_object_class_install_property (object_class, PROP_PLAYING,
      g_param_spec_boolean ("playing", "playing", "TRUE when the video is playing",
	  FALSE, G_PARAM_READABLE));
}

static void
swfdec_net_stream_video_init (SwfdecNetStreamVideo *video)
{
  video->next = g_queue_new ();
  video->buffer_time = 100;
}

SwfdecNetStreamVideo *
swfdec_net_stream_video_new (SwfdecPlayer *player)
{
  SwfdecNetStreamVideo *ret;

  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), NULL);

  ret = g_object_new (SWFDEC_TYPE_NET_STREAM_VIDEO, "context", player, NULL);

  return ret;
}

void
swfdec_net_stream_video_clear (SwfdecNetStreamVideo *video)
{
  g_return_if_fail (SWFDEC_IS_NET_STREAM_VIDEO (video));

  video->time = 0;
  g_queue_foreach (video->next, (GFunc) swfdec_rtmp_packet_free, NULL);
  g_queue_clear (video->next);
  video->next_length = 0;
  video->playing = FALSE;
  if (video->timeout.callback) {
    swfdec_player_remove_timeout (SWFDEC_PLAYER (swfdec_gc_object_get_context (video)),
	&video->timeout);
    video->timeout.callback = NULL;
  }
  /* FIXME: clear decoder, too? currently it's needed for getting the current image */
}

static void
swfdec_net_stream_video_decode_one (SwfdecNetStreamVideo *video, SwfdecBuffer *buffer)
{
  SwfdecBits bits;
  guint frametype, codec;

  swfdec_bits_init (&bits, buffer);
  frametype = swfdec_bits_getbits (&bits, 4);
  codec = swfdec_bits_getbits (&bits, 4);
  if (video->decoder == NULL) {
    video->decoder = swfdec_video_decoder_new (codec);
  } else if (swfdec_video_decoder_get_codec (video->decoder) != codec) {
    SWFDEC_WARNING ("codec change from %u to %u", 
	swfdec_video_decoder_get_codec (video->decoder), codec);
    g_object_unref (video->decoder);
    video->decoder = swfdec_video_decoder_new (codec);
  }
  
  if (codec == SWFDEC_VIDEO_CODEC_H264) {
    guint type = swfdec_bits_get_u8 (&bits);
    /* composition_time_offset = */ swfdec_bits_get_bu24 (&bits);
    switch (type) {
      case 0:
	buffer = swfdec_bits_get_buffer (&bits, -1);
	if (buffer) {
	  swfdec_video_decoder_set_codec_data (video->decoder, buffer);
	  swfdec_buffer_unref (buffer);
	}
	break;
      case 1:
	buffer = swfdec_bits_get_buffer (&bits, -1);
	if (buffer) {
	  swfdec_video_decoder_decode (video->decoder, buffer);
	} else {
	  SWFDEC_ERROR ("no data in H264 buffer?");
	}
	swfdec_video_provider_new_image (SWFDEC_VIDEO_PROVIDER (video));
	break;
      case 2:
	break;
      default:
	SWFDEC_ERROR ("H264 data type %u not supported", type);
	break;
    }
  } else if (codec == SWFDEC_VIDEO_CODEC_VP6 ||
      codec == SWFDEC_VIDEO_CODEC_VP6_ALPHA) {
    /* FIXME: This is somewhat nasty as we modify values in the decoder 
     * directly. I know the current decoders don't mind, but if we expose 
     * the decoder API... */
    guint wsub, hsub;
    wsub = swfdec_bits_getbits (&bits, 4);
    hsub = swfdec_bits_getbits (&bits, 4);
    buffer = swfdec_bits_get_buffer (&bits, -1);
    swfdec_video_decoder_decode (video->decoder, buffer);
    swfdec_buffer_unref (buffer);
    if (hsub >= video->decoder->height || wsub >= video->decoder->width) {
      SWFDEC_ERROR ("can't reduce size by more than available");
      video->decoder->width = 0;
      video->decoder->height = 0;
    } else {
      video->decoder->width -= wsub;
      video->decoder->height -= hsub;
    }
    swfdec_video_provider_new_image (SWFDEC_VIDEO_PROVIDER (video));
  } else {
    buffer = swfdec_bits_get_buffer (&bits, -1);
    swfdec_video_decoder_decode (video->decoder, buffer);
    swfdec_buffer_unref (buffer);
    swfdec_video_provider_new_image (SWFDEC_VIDEO_PROVIDER (video));
  }
}

static void swfdec_net_stream_video_decode (SwfdecNetStreamVideo *video);
static void
swfdec_net_stream_video_timeout (SwfdecTimeout *timeout)
{
  SwfdecNetStreamVideo *video = SWFDEC_NET_STREAM_VIDEO ((void *) ((guint8 *) timeout - G_STRUCT_OFFSET (SwfdecNetStreamVideo, timeout)));
  SwfdecRtmpPacket *packet;

  video->timeout.callback = NULL;
  /* subtract time, so swfdec_net_stream_video_decode() can do its work */
  packet = g_queue_peek_head (video->next);
  g_assert (packet);
  video->timeout.timestamp -= SWFDEC_TICKS_PER_SECOND * packet->header.timestamp / 1000;
  swfdec_net_stream_video_decode (video);
}

static void
swfdec_net_stream_video_decode (SwfdecNetStreamVideo *video)
{
  SwfdecPlayer *player = SWFDEC_PLAYER (swfdec_gc_object_get_context (video));
  SwfdecRtmpPacket *packet;

  for (;;) {
    packet = g_queue_pop_head (video->next);
    if (packet == NULL) {
      video->playing = FALSE;
      g_object_notify (G_OBJECT (video), "playing");
      return;
    }
    video->timeout.timestamp += SWFDEC_TICKS_PER_SECOND * packet->header.timestamp / 1000;
    if (player->priv->time < video->timeout.timestamp) {
      g_queue_push_head (video->next, packet);
      video->timeout.callback = swfdec_net_stream_video_timeout;
      swfdec_player_add_timeout (player, &video->timeout);
      return;
    }
    swfdec_net_stream_video_decode_one (video, packet->buffer);
    video->next_length -= packet->header.timestamp;
    video->time += packet->header.timestamp;
    swfdec_rtmp_packet_free (packet);
  }
}

static void
swfdec_net_stream_video_start (SwfdecNetStreamVideo *video)
{
  video->time = 0;
  video->playing = TRUE;
  video->timeout.timestamp = SWFDEC_PLAYER (swfdec_gc_object_get_context (video))->priv->time;
  g_object_notify (G_OBJECT (video), "playing");
}

void
swfdec_net_stream_video_push (SwfdecNetStreamVideo *video,
    const SwfdecRtmpHeader *header, SwfdecBuffer *buffer)
{
  SwfdecRtmpPacket *packet;

  g_return_if_fail (SWFDEC_IS_NET_STREAM_VIDEO (video));
  g_return_if_fail (buffer != NULL);

  packet = swfdec_rtmp_packet_new (header->channel, header->stream,
      header->type, header->timestamp, buffer);
  g_queue_push_tail (video->next, packet);
  video->next_length += header->timestamp;
  if (!video->playing && video->next_length >= video->buffer_time) {
    swfdec_net_stream_video_start (video);
    swfdec_net_stream_video_decode (video);
  }
}

