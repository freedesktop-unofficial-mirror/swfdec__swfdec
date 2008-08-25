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

#include "swfdec_abc_value.h"

#include <math.h>
#include <string.h>

#include "swfdec_abc_namespace.h"
#include "swfdec_abc_object.h"
#include "swfdec_abc_traits.h"
#include "swfdec_as_context.h"
#include "swfdec_as_strings.h"
#include "swfdec_debug.h"

gboolean
swfdec_abc_value_to_boolean (SwfdecAsContext *context, const SwfdecAsValue *value)
{
  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), FALSE);
  g_return_val_if_fail (SWFDEC_IS_AS_VALUE (value), FALSE);

  switch (SWFDEC_AS_VALUE_GET_TYPE (value)) {
    case SWFDEC_AS_TYPE_UNDEFINED:
    case SWFDEC_AS_TYPE_NULL:
      return FALSE;
    case SWFDEC_AS_TYPE_BOOLEAN:
      return SWFDEC_AS_VALUE_GET_BOOLEAN (value);
    case SWFDEC_AS_TYPE_NUMBER:
      {
	double d = SWFDEC_AS_VALUE_GET_NUMBER (value);
	return !isnan (d) && d != 0.0;
      }
    case SWFDEC_AS_TYPE_STRING:
      return SWFDEC_AS_VALUE_GET_STRING (value) != SWFDEC_AS_STR_EMPTY;
    case SWFDEC_AS_TYPE_OBJECT:
    case SWFDEC_AS_TYPE_NAMESPACE:
      return TRUE;
    case SWFDEC_AS_TYPE_INT:
    default:
      g_assert_not_reached ();
      return FALSE;
  }
}

int
swfdec_abc_value_to_integer (SwfdecAsContext *context, const SwfdecAsValue *value)
{
  double d;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), 0);
  g_return_val_if_fail (SWFDEC_IS_AS_VALUE (value), 0);

  d = swfdec_abc_value_to_number (context, value);

  if (!isfinite (d))
    return 0;
  if (d < 0) {
    d = fmod (-d, 4294967296.0);
    return - (guint) d;
  } else {
    d = fmod (d, 4294967296.0);
    return (guint) d;
  }
}

double
swfdec_abc_value_to_number (SwfdecAsContext *context, const SwfdecAsValue *value)
{
  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), 0.0);
  g_return_val_if_fail (SWFDEC_IS_AS_VALUE (value), 0.0);

  switch (SWFDEC_AS_VALUE_GET_TYPE (value)) {
    case SWFDEC_AS_TYPE_UNDEFINED:
    case SWFDEC_AS_TYPE_NULL:
      return NAN;
    case SWFDEC_AS_TYPE_BOOLEAN:
      return SWFDEC_AS_VALUE_GET_BOOLEAN (value) ? 1.0 : 0.0;
    case SWFDEC_AS_TYPE_NUMBER:
      return SWFDEC_AS_VALUE_GET_NUMBER (value);
    case SWFDEC_AS_TYPE_STRING:
      return swfdec_as_string_to_number (context, SWFDEC_AS_VALUE_GET_STRING (value));
    case SWFDEC_AS_TYPE_OBJECT:
      {
	SwfdecAsValue tmp;
	if (!swfdec_abc_object_default_value (
	      SWFDEC_ABC_OBJECT (SWFDEC_AS_VALUE_GET_OBJECT (value)), &tmp))
	  return NAN;
	return swfdec_abc_value_to_number (context, &tmp);
      }
    case SWFDEC_AS_TYPE_NAMESPACE:
      return swfdec_as_string_to_number (context, SWFDEC_AS_VALUE_GET_NAMESPACE (value)->uri);
    case SWFDEC_AS_TYPE_INT:
    default:
      g_assert_not_reached ();
      return FALSE;
  }
}

gboolean
swfdec_abc_value_to_primitive (SwfdecAsValue *dest, const SwfdecAsValue *src)
{
  g_return_val_if_fail (SWFDEC_IS_AS_VALUE (dest), FALSE);
  g_return_val_if_fail (SWFDEC_IS_AS_VALUE (src), FALSE);

  if (SWFDEC_AS_VALUE_IS_OBJECT (src)) {
    return swfdec_abc_object_default_value
      (SWFDEC_ABC_OBJECT (SWFDEC_AS_VALUE_GET_OBJECT (src)), dest);
  } else if (dest != src) {
    *dest = *src;
  }
  return TRUE;
}

const char *
swfdec_abc_value_to_string (SwfdecAsContext *context, const SwfdecAsValue *value)
{
  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), SWFDEC_AS_STR_EMPTY);
  g_return_val_if_fail (SWFDEC_IS_AS_VALUE (value), SWFDEC_AS_STR_EMPTY);

  switch (SWFDEC_AS_VALUE_GET_TYPE (value)) {
    case SWFDEC_AS_TYPE_UNDEFINED:
      return SWFDEC_AS_STR_undefined;
    case SWFDEC_AS_TYPE_NULL:
      return SWFDEC_AS_STR_null;
    case SWFDEC_AS_TYPE_BOOLEAN:
      return SWFDEC_AS_VALUE_GET_BOOLEAN (value) ? SWFDEC_AS_STR_true : SWFDEC_AS_STR_false;
    case SWFDEC_AS_TYPE_NUMBER:
      return swfdec_as_double_to_string (context, SWFDEC_AS_VALUE_GET_NUMBER (value));
    case SWFDEC_AS_TYPE_STRING:
      return SWFDEC_AS_VALUE_GET_STRING (value);
    case SWFDEC_AS_TYPE_OBJECT:
      {
	SwfdecAsValue tmp;
	if (!swfdec_abc_object_to_string (
	      SWFDEC_ABC_OBJECT (SWFDEC_AS_VALUE_GET_OBJECT (value)), &tmp))
	  return SWFDEC_AS_STR_EMPTY;
	return swfdec_abc_value_to_string (context, &tmp);
      }
    case SWFDEC_AS_TYPE_NAMESPACE:
      return SWFDEC_AS_VALUE_GET_NAMESPACE (value)->uri;
    case SWFDEC_AS_TYPE_INT:
    default:
      g_assert_not_reached ();
      return FALSE;
  }
}

const char *
swfdec_abc_value_get_type_name (const SwfdecAsValue *value)
{
  g_return_val_if_fail (SWFDEC_IS_AS_VALUE (value), NULL);

  switch (value->type) {
    case SWFDEC_AS_TYPE_STRING:
      return SWFDEC_AS_STR_String;
    case SWFDEC_AS_TYPE_UNDEFINED:
      return SWFDEC_AS_STR_void;
    case SWFDEC_AS_TYPE_BOOLEAN:
      return SWFDEC_AS_STR_Boolean;
    case SWFDEC_AS_TYPE_NULL:
      return SWFDEC_AS_STR_null;
    case SWFDEC_AS_TYPE_NUMBER:
      return SWFDEC_AS_STR_Number;
    case SWFDEC_AS_TYPE_OBJECT:
      {
	SwfdecAsObject *object = SWFDEC_AS_VALUE_GET_OBJECT (value);
	if (SWFDEC_IS_ABC_OBJECT (object))
	  return SWFDEC_ABC_OBJECT (object)->traits->name;
	return SWFDEC_AS_STR_Object;
      }
    case SWFDEC_AS_TYPE_NAMESPACE:
      return SWFDEC_AS_STR_Namespace;
    case SWFDEC_AS_TYPE_INT:
    default:
      g_assert_not_reached ();
      return NULL;
  }
}

/**
 * swfdec_abc_value_compare:
 * @context: the context the values beliong to
 * @lval: the left-side value of the comparison
 * @rval: the right-side value of the comparison
 *
 * Does the ABC lower-or-equal comparison, as in @lval <= @rval. Be aware that
 * this function can throw an exception.
 *
 * Returns: see #SwfdecAbcComparison
 **/
SwfdecAbcComparison
swfdec_abc_value_compare (SwfdecAsContext *context, const SwfdecAsValue *lval,
    const SwfdecAsValue *rval)
{
  SwfdecAsValue ltmp, rtmp;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), SWFDEC_ABC_COMPARE_UNDEFINED);
  g_return_val_if_fail (lval != NULL, SWFDEC_ABC_COMPARE_UNDEFINED);
  g_return_val_if_fail (rval != NULL, SWFDEC_ABC_COMPARE_UNDEFINED);

  if (!swfdec_abc_value_to_primitive (&ltmp, lval) ||
      !swfdec_abc_value_to_primitive (&rtmp, rval))
    return SWFDEC_ABC_COMPARE_THROWN;

  if (SWFDEC_AS_VALUE_IS_STRING (&ltmp) && SWFDEC_AS_VALUE_IS_STRING (&rtmp)) {
    int comp = strcmp (SWFDEC_AS_VALUE_GET_STRING (&ltmp), 
	SWFDEC_AS_VALUE_GET_STRING (&rtmp));
    return (comp < 0) ? SWFDEC_ABC_COMPARE_LOWER : 
      (comp > 0 ? SWFDEC_ABC_COMPARE_GREATER : SWFDEC_ABC_COMPARE_EQUAL);
  } else {
    double l = swfdec_as_value_to_number (context, &ltmp);
    double r = swfdec_as_value_to_number (context, &rtmp);
    if (isnan (l) || isnan (r))
      return SWFDEC_ABC_COMPARE_UNDEFINED;

    return (l < r) ? SWFDEC_ABC_COMPARE_LOWER : 
      (l > r ? SWFDEC_ABC_COMPARE_GREATER : SWFDEC_ABC_COMPARE_EQUAL);
  }
}

