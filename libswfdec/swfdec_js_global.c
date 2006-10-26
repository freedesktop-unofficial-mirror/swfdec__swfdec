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

#include <string.h>
#include "swfdec_js.h"
#include "swfdec_debug.h"
#include "swfdec_player_internal.h"

JSBool
swfdec_js_eval (JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  if (JSVAL_IS_STRING (argv[0])) {
    const char *bytes = swfdec_js_to_string (cx, argv[0]);
    char *slash = NULL;
    JSBool ret;
    if (bytes == NULL)
      return JS_FALSE;
    if (bytes[0] == '\0') {
      *rval = OBJECT_TO_JSVAL (obj);
    } else {
      if (strchr (bytes, '/')) {
	slash = swfdec_js_slash_to_dot (bytes);
	bytes = slash;
      }
      /* FIXME: better filename/lineno information */
      ret = JS_EvaluateScript (cx, obj, bytes, strlen (bytes), NULL, 0, rval);
      if (bytes == slash)
	g_free (slash);
      if (!ret)
	return JS_FALSE;
    }
  } else {
    *rval = argv[0];
  }
  return JS_TRUE;
}

static JSBool
swfdec_js_trace (JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  SwfdecPlayer *player = JS_GetContextPrivate (cx);
  const char *bytes;

  bytes = swfdec_js_to_string (cx, argv[0]);
  if (bytes == NULL)
    return JS_TRUE;

  /* FIXME: accumulate and emit after JS handling? */
  g_signal_emit_by_name (player, "trace", bytes);
  return JS_TRUE;
}

static JSBool
swfdec_js_random (JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  gint32 max, result;

  if (!JS_ValueToECMAInt32 (cx, argv[0], &max))
    return JS_FALSE;
  
  result = g_random_int_range (0, max);

  return JS_NewNumberValue(cx, result, rval);
}

static JSFunctionSpec global_methods[] = {
  { "eval",		swfdec_js_eval,		1, 0, 0 },
  { "random",		swfdec_js_random,	1, 0, 0 },
  { "trace",     	swfdec_js_trace,	1, 0, 0 },
  { NULL, NULL, 0, 0, 0 }
};

void
swfdec_js_add_globals (SwfdecPlayer *player)
{
  if (!JS_DefineFunctions (player->jscx, player->jsobj, global_methods)) {
    SWFDEC_ERROR ("failed to initialize global methods");
  }
}

