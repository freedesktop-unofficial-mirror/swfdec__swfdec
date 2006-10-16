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

#include "swfdec_js.h"
#include "swfdec_debug.h"
#include "swfdec_player_internal.h"

static JSBool
swfdec_js_mouse_show (JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  SwfdecPlayer *player = JS_GetContextPrivate (cx);

  g_assert (player);
  if (!player->mouse_visible) {
    player->mouse_visible = TRUE;
    g_object_notify (G_OBJECT (player), "mouse-visible");
  }
  return JS_TRUE;
}

static JSBool
swfdec_js_mouse_hide (JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  SwfdecPlayer *player = JS_GetContextPrivate (cx);

  g_assert (player);
  if (player->mouse_visible) {
    player->mouse_visible = FALSE;
    g_object_notify (G_OBJECT (player), "mouse-visible");
  }
  return JS_TRUE;
}

static JSFunctionSpec mouse_methods[] = {
    {"show",		swfdec_js_mouse_show,	0, 0, 0 },
    {"hide",		swfdec_js_mouse_hide,	0, 0, 0 },
    {0,0,0,0,0}
};

static JSClass mouse_class = {
    "Mouse",
    0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

void
swfdec_js_add_mouse (SwfdecPlayer *player)
{
  JSObject *mouse;
  
  mouse = JS_DefineObject(player->jscx, player->jsobj, "Mouse", &mouse_class, NULL, 0);
  if (!mouse || 
      !JS_DefineFunctions(player->jscx, mouse, mouse_methods)) {
    SWFDEC_ERROR ("failed to initialize Mouse object");
  }
}

