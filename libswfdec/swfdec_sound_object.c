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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include "swfdec_sound_object.h"
#include "swfdec_as_context.h"
#include "swfdec_as_internal.h"
#include "swfdec_as_native_function.h"
#include "swfdec_as_object.h"
#include "swfdec_as_strings.h"
#include "swfdec_audio_event.h"
#include "swfdec_debug.h"
#include "swfdec_internal.h"
#include "swfdec_player_internal.h"
#include "swfdec_resource.h"

/*** SwfdecSoundObject ***/

G_DEFINE_TYPE (SwfdecSoundObject, swfdec_sound_object, SWFDEC_TYPE_AS_OBJECT)

static void
swfdec_sound_object_mark (SwfdecAsObject *object)
{
  SwfdecSoundObject *sound = SWFDEC_SOUND_OBJECT (object);

  swfdec_as_object_mark (SWFDEC_AS_OBJECT (sound->target));

  SWFDEC_AS_OBJECT_CLASS (swfdec_sound_object_parent_class)->mark (object);
}

static void
swfdec_sound_object_dispose (GObject *object)
{
  SwfdecSoundObject *sound = SWFDEC_SOUND_OBJECT (object);

  if (sound->attached) {
    g_object_unref (sound->attached);
    sound->attached = NULL;
  }

  G_OBJECT_CLASS (swfdec_sound_object_parent_class)->dispose (object);
}

static void
swfdec_sound_object_class_init (SwfdecSoundObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecAsObjectClass *asobject_class = SWFDEC_AS_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_sound_object_dispose;

  asobject_class->mark = swfdec_sound_object_mark;
}

static void
swfdec_sound_object_init (SwfdecSoundObject *sound)
{
}

static SwfdecSound *
swfdec_sound_object_get_sound (SwfdecSoundObject *sound, const char *name)
{
  if (sound->target == NULL)
    return NULL;

  return swfdec_resource_get_export (sound->target->resource, name);
}

/*** AS CODE ***/

SWFDEC_AS_NATIVE (500, 7, swfdec_sound_object_attachSound)
void
swfdec_sound_object_attachSound (SwfdecAsContext *cx, SwfdecAsObject *object, guint argc, 
    SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  SwfdecSoundObject *sound;
  const char *name;
  SwfdecSound *new;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_SOUND_OBJECT, &sound, "s", &name);

  new = swfdec_sound_object_get_sound (sound, name);
  if (new) {
    if (sound->attached)
      g_object_unref (sound->attached);
    sound->attached = g_object_ref (new);
  }
}

SWFDEC_AS_NATIVE (500, 8, swfdec_sound_object_start)
void
swfdec_sound_object_start (SwfdecAsContext *cx, SwfdecAsObject *object, guint argc, 
    SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  SwfdecSoundObject *sound;
  double offset;
  int loops;
  SwfdecAudio *audio;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_SOUND_OBJECT, &sound, "|ni", &offset, &loops);

  if (sound->attached == NULL) {
    SWFDEC_INFO ("no sound attached when calling Sound.start()");
    return;
  }
  if (argc < 2 || loops < 0)
    loops = 1;
  if (offset < 0 || !isfinite (offset))
    offset = 0;

  audio = swfdec_audio_event_new (SWFDEC_PLAYER (cx), sound->attached, offset / 44100, loops);
  g_object_unref (audio);
}

typedef struct {
  SwfdecMovie *movie;
  SwfdecSound *sound;
} RemoveData;

static gboolean
swfdec_sound_object_should_stop (SwfdecAudio *audio, gpointer datap)
{
  RemoveData *data = datap;
  SwfdecAudioEvent *event;

  if (!SWFDEC_IS_AUDIO_EVENT (audio))
    return FALSE;
  event = SWFDEC_AUDIO_EVENT (audio);
  if (data->sound != NULL && event->sound != data->sound)
    return FALSE;
  /* FIXME: also check the movie is identical */
  return TRUE;
}

SWFDEC_AS_NATIVE (500, 6, swfdec_sound_object_stop)
void
swfdec_sound_object_stop (SwfdecAsContext *cx, SwfdecAsObject *object, guint argc, 
    SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  SwfdecSoundObject *sound;
  const char *name;
  RemoveData data;

  SWFDEC_AS_CHECK (SWFDEC_TYPE_SOUND_OBJECT, &sound, "|s", &name);

  if (sound->global) {
    data.movie = NULL;
  } else {
    data.movie = sound->target;
  }
  if (argc > 0) {
    data.sound = swfdec_sound_object_get_sound (sound, name);
    if (data.sound == NULL)
      return;
  } else if (sound->attached) {
    data.sound = sound->attached;
  } else {
    data.sound = NULL;
  }
  swfdec_player_stop_sounds (SWFDEC_PLAYER (cx), swfdec_sound_object_should_stop, &data);
}

SWFDEC_AS_CONSTRUCTOR (500, 16, swfdec_sound_object_construct, swfdec_sound_object_get_type)
void
swfdec_sound_object_construct (SwfdecAsContext *cx, SwfdecAsObject *object, guint argc, 
    SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  SwfdecSoundObject *sound;
  SwfdecPlayer *player;
    
  if (!swfdec_as_context_is_constructing (cx))
    return;

  sound = SWFDEC_SOUND_OBJECT (object);
  player = SWFDEC_PLAYER (cx);

  if (argc == 0 || SWFDEC_AS_VALUE_IS_UNDEFINED (&argv[0])) {
    sound->global = TRUE;
    /* FIXME: what is the target for global sounds? Problem:
     * We use the target in attachSound to look up the sound object to attach.
     * But I'm not sure what is used for global sounds.
     * So we just use a random one that looks good for now. */
    sound->target = player->roots->data;
  } else {
    sound->target = swfdec_player_get_movie_from_value (player, &argv[0]);
  }
}

