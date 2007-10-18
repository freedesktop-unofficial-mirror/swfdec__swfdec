/* Swfdec
 * Copyright (C) 2006 Benjamin Otte <otte@gnome.org>
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
#include "swfdec_event.h"
#include "swfdec_as_internal.h"
#include "swfdec_as_strings.h"
#include "swfdec_debug.h"
#include "swfdec_player_internal.h"
#include "swfdec_script_internal.h"

typedef struct _SwfdecEvent SwfdecEvent;

struct _SwfdecEvent {
  guint	conditions;
  guint8	key;
  SwfdecScript *script;
};

struct _SwfdecEventList {
  SwfdecPlayer *	player;
  guint			refcount;
  GArray *		events;
};

/**
 * swfdec_event_type_get_name:
 * @type: a #SwfdecEventType
 *
 * Gets the name for the event as a refcounted string or %NULL if the
 * given clip event has no associated event.
 *
 * Returns: The name of the event or %NULL if none.
 **/
const char *
swfdec_event_type_get_name (SwfdecEventType type)
{
  switch (type) {
    case SWFDEC_EVENT_LOAD:
      return SWFDEC_AS_STR_onLoad;
    case SWFDEC_EVENT_ENTER:
      return SWFDEC_AS_STR_onEnterFrame;
    case SWFDEC_EVENT_UNLOAD:
      return SWFDEC_AS_STR_onUnload;
    case SWFDEC_EVENT_MOUSE_MOVE:
      return SWFDEC_AS_STR_onMouseMove;
    case SWFDEC_EVENT_MOUSE_DOWN:
      return SWFDEC_AS_STR_onMouseDown;
    case SWFDEC_EVENT_MOUSE_UP:
      return SWFDEC_AS_STR_onMouseUp;
    case SWFDEC_EVENT_KEY_UP:
      return SWFDEC_AS_STR_onKeyUp;
    case SWFDEC_EVENT_KEY_DOWN:
      return SWFDEC_AS_STR_onKeyDown;
    case SWFDEC_EVENT_DATA:
      return SWFDEC_AS_STR_onData;
    case SWFDEC_EVENT_INITIALIZE:
      return NULL;
    case SWFDEC_EVENT_PRESS:
      return SWFDEC_AS_STR_onPress;
    case SWFDEC_EVENT_RELEASE:
      return SWFDEC_AS_STR_onRelease;
    case SWFDEC_EVENT_RELEASE_OUTSIDE:
      return SWFDEC_AS_STR_onReleaseOutside;
    case SWFDEC_EVENT_ROLL_OVER:
      return SWFDEC_AS_STR_onRollOver;
    case SWFDEC_EVENT_ROLL_OUT:
      return SWFDEC_AS_STR_onRollOut;
    case SWFDEC_EVENT_DRAG_OVER:
      return SWFDEC_AS_STR_onDragOver;
    case SWFDEC_EVENT_DRAG_OUT:
      return SWFDEC_AS_STR_onDragOut;
    case SWFDEC_EVENT_KEY_PRESS:
      return NULL;
    case SWFDEC_EVENT_CONSTRUCT:
      return SWFDEC_AS_STR_onConstruct;
    default:
      g_assert_not_reached ();
      return NULL;
  }
}

SwfdecEventList *
swfdec_event_list_new (SwfdecPlayer *player)
{
  SwfdecEventList *list;

  g_return_val_if_fail (SWFDEC_IS_PLAYER (player), NULL);

  list = g_new0 (SwfdecEventList, 1);
  list->player = player;
  list->refcount = 1;
  list->events = g_array_new (FALSE, FALSE, sizeof (SwfdecEvent));

  return list;
}

/* FIXME: this is a bit nasty because of modifying */
SwfdecEventList *
swfdec_event_list_copy (SwfdecEventList *list)
{
  g_return_val_if_fail (list != NULL, NULL);
  g_return_val_if_fail (list->refcount > 0, NULL);

  list->refcount++;

  return list;
}

void
swfdec_event_list_free (SwfdecEventList *list)
{
  guint i;

  g_return_if_fail (list != NULL);
  g_return_if_fail (list->refcount > 0);

  list->refcount--;
  if (list->refcount > 0)
    return;

  for (i = 0; i < list->events->len; i++) {
    SwfdecEvent *event = &g_array_index (list->events, SwfdecEvent, i);
    swfdec_script_unref (event->script);
  }
  g_array_free (list->events, TRUE);
  g_free (list);
}

static const char *
swfdec_event_list_condition_name (guint conditions)
{
  if (conditions & SWFDEC_EVENT_LOAD)
    return "Load";
  if (conditions & SWFDEC_EVENT_ENTER)
    return "Enter";
  if (conditions & SWFDEC_EVENT_UNLOAD)
    return "Unload";
  if (conditions & SWFDEC_EVENT_MOUSE_MOVE)
    return "MouseMove";
  if (conditions & SWFDEC_EVENT_MOUSE_DOWN)
    return "MouseDown";
  if (conditions & SWFDEC_EVENT_MOUSE_UP)
    return "MouseUp";
  if (conditions & SWFDEC_EVENT_KEY_UP)
    return "KeyUp";
  if (conditions & SWFDEC_EVENT_KEY_DOWN)
    return "KeyDown";
  if (conditions & SWFDEC_EVENT_DATA)
    return "Data";
  if (conditions & SWFDEC_EVENT_INITIALIZE)
    return "Initialize";
  if (conditions & SWFDEC_EVENT_PRESS)
    return "Press";
  if (conditions & SWFDEC_EVENT_RELEASE)
    return "Release";
  if (conditions & SWFDEC_EVENT_RELEASE_OUTSIDE)
    return "ReleaseOutside";
  if (conditions & SWFDEC_EVENT_ROLL_OVER)
    return "RollOver";
  if (conditions & SWFDEC_EVENT_ROLL_OUT)
    return "RollOut";
  if (conditions & SWFDEC_EVENT_DRAG_OVER)
    return "DragOver";
  if (conditions & SWFDEC_EVENT_DRAG_OUT)
    return "DragOut";
  if (conditions & SWFDEC_EVENT_KEY_PRESS)
    return "KeyPress";
  if (conditions & SWFDEC_EVENT_CONSTRUCT)
    return "Construct";
  return "No Event";
}

void
swfdec_event_list_parse (SwfdecEventList *list, SwfdecBits *bits, int version,
    guint conditions, guint8 key, const char *description)
{
  SwfdecEvent event;
  char *name;

  g_return_if_fail (list != NULL);
  g_return_if_fail (list->refcount == 1);
  g_return_if_fail (description != NULL);

  event.conditions = conditions;
  event.key = key;
  name = g_strconcat (description, ".", 
      swfdec_event_list_condition_name (conditions), NULL);
  event.script = swfdec_script_new_from_bits (bits, name, version);
  g_free (name);
  if (event.script) 
    g_array_append_val (list->events, event);
}

void
swfdec_event_list_execute (SwfdecEventList *list, SwfdecAsObject *object, 
    SwfdecSecurity *sec, guint condition, guint8 key)
{
  guint i;

  g_return_if_fail (list != NULL);
  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));
  g_return_if_fail (SWFDEC_IS_SECURITY (sec));

  /* FIXME: Do we execute all events if the event list is gone already? */
  /* need to ref here because followup code could free all references to the list */
  list = swfdec_event_list_copy (list);
  for (i = 0; i < list->events->len; i++) {
    SwfdecEvent *event = &g_array_index (list->events, SwfdecEvent, i);
    if ((event->conditions & condition) &&
	event->key == key) {
      SWFDEC_LOG ("executing script for event %u on scriptable %p", condition, object);
      swfdec_as_object_run_with_security (object, event->script, sec);
    }
  }
  swfdec_event_list_free (list);
}

gboolean
swfdec_event_list_has_conditions (SwfdecEventList *list, SwfdecAsObject *object,
    guint condition, guint8 key)
{
  guint i;
  const char *name;

  g_return_val_if_fail (list != NULL, FALSE);
  g_return_val_if_fail (SWFDEC_IS_AS_OBJECT (object), FALSE);

  for (i = 0; i < list->events->len; i++) {
    SwfdecEvent *event = &g_array_index (list->events, SwfdecEvent, i);
    if ((event->conditions & condition) &&
	event->key == key)
      return TRUE;
  }
  name = swfdec_event_type_get_name (condition);
  if (name)
    return swfdec_as_object_has_function (object, name);
  return FALSE;
}

