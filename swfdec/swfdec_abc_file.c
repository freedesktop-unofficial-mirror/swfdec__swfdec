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

#include "swfdec_abc_file.h"
#include "swfdec_abc_internal.h"
#include "swfdec_abc_traits.h"
#include "swfdec_as_strings.h"
#include "swfdec_debug.h"

/*** SWFDEC_ABC_FILE ***/

G_DEFINE_TYPE (SwfdecAbcFile, swfdec_abc_file, SWFDEC_TYPE_GC_OBJECT)

static void
swfdec_abc_file_dispose (GObject *object)
{
  SwfdecAbcFile *file = SWFDEC_ABC_FILE (object);
  SwfdecAsContext *context = swfdec_gc_object_get_context (file);

  if (file->n_ints) {
    swfdec_as_context_free (context, file->n_ints * sizeof (int), file->ints);
  }
  if (file->n_uints) {
    swfdec_as_context_free (context, file->n_uints * sizeof (guint), file->uints);
  }
  if (file->n_doubles) {
    swfdec_as_context_free (context, file->n_doubles * sizeof (double), file->doubles);
  }
  if (file->n_strings) {
    swfdec_as_context_free (context, file->n_strings * sizeof (const char *), file->strings);
  }
  if (file->n_namespaces) {
    swfdec_as_context_free (context, file->n_namespaces * sizeof (SwfdecAbcNamespace *),
	file->namespaces);
  }
  if (file->n_nssets) {
    guint i;
    for (i = 0; i < file->n_nssets; i++) {
      swfdec_abc_ns_set_free (file->nssets[i]);
    }
    swfdec_as_context_free (context, file->n_nssets * sizeof (SwfdecAbcNsSet *),
	file->nssets);
  }
  if (file->n_multinames) {
    swfdec_as_context_free (context, file->n_multinames * sizeof (SwfdecAbcMultiname),
	file->multinames);
  }
  if (file->n_functions) {
    swfdec_as_context_free (context, file->n_functions * sizeof (SwfdecAbcFunction *),
	file->functions);
  }
  if (file->n_classes) {
    swfdec_as_context_free (context, file->n_classes * sizeof (SwfdecAbcFunction *),
	file->classes);
  }

  G_OBJECT_CLASS (swfdec_abc_file_parent_class)->dispose (object);
}

static void
swfdec_abc_file_mark (SwfdecGcObject *object)
{
  SwfdecAbcFile *file = SWFDEC_ABC_FILE (object);
  guint i;

  for (i = 0; i < file->n_strings; i++) {
    swfdec_as_string_mark (file->strings[i]);
  }
  for (i = 1; i < file->n_namespaces; i++) {
    swfdec_gc_object_mark (file->namespaces[i]);
  }
  for (i = 1; i < file->n_functions; i++) {
    if (file->functions[i])
      swfdec_gc_object_mark (file->functions[i]);
  }
  for (i = 1; i < file->n_classes; i++) {
    if (file->classes[i])
      swfdec_gc_object_mark (file->classes[i]);
  }

  SWFDEC_GC_OBJECT_CLASS (swfdec_abc_file_parent_class)->mark (object);
}

static void
swfdec_abc_file_class_init (SwfdecAbcFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_abc_file_dispose;

  gc_class->mark = swfdec_abc_file_mark;
}

static void
swfdec_abc_file_init (SwfdecAbcFile *date)
{
}

/*** PARSING ***/

#define READ_U30(x, bits) G_STMT_START{ \
  x = swfdec_bits_get_vu32 (bits); \
  if (x >= (1 << 30)) \
    return FALSE; \
}G_STMT_END;
#define THROW(file, ...) G_STMT_START{ \
  swfdec_as_context_throw_abc (swfdec_gc_object_get_context (file), \
      SWFDEC_ABC_ERROR_VERIFY, __VA_ARGS__); \
  return FALSE; \
}G_STMT_END

static gboolean
swfdec_abc_file_parse_constants (SwfdecAbcFile *file, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (file);
  guint i;

  SWFDEC_LOG ("parsing ABC block");
  /* read all integers */
  READ_U30 (file->n_ints, bits);
  if (file->n_ints) {
    file->ints = swfdec_as_context_try_new (context, int, file->n_ints);
    if (file->ints == NULL) {
      file->n_ints = 0;
      return FALSE;
    }
    for (i = 1; i < file->n_ints && swfdec_bits_left (bits); i++) {
      file->ints[i] = swfdec_bits_get_vs32 (bits);
      SWFDEC_LOG ("  int %u: %d", i, file->ints[i]);
    }
  }

  /* read all unsigned integers */
  READ_U30 (file->n_uints, bits);
  if (file->n_uints) {
    file->uints = swfdec_as_context_try_new (context, guint, file->n_uints);
    if (file->uints == NULL) {
      file->n_uints = 0;
      return FALSE;
    }
    for (i = 1; i < file->n_uints && swfdec_bits_left (bits); i++) {
      file->uints[i] = swfdec_bits_get_vu32 (bits);
      SWFDEC_LOG ("  uint %u: %u", i, file->uints[i]);
    }
  }

  /* read all doubles */
  READ_U30 (file->n_doubles, bits);
  if (file->n_doubles) {
    file->doubles = swfdec_as_context_try_new (context, double, file->n_doubles);
    if (file->doubles == NULL) {
      file->n_doubles = 0;
      return FALSE;
    }
    for (i = 1; i < file->n_doubles && swfdec_bits_left (bits); i++) {
      file->doubles[i] = swfdec_bits_get_ldouble (bits);
      SWFDEC_LOG ("  double %u: %g", i, file->doubles[i]);
    }
  }

  /* read all strings */
  READ_U30 (file->n_strings, bits);
  if (file->n_strings) {
    file->strings = swfdec_as_context_try_new (context, const char *, file->n_strings);
    if (file->strings == NULL) {
      file->n_strings = 0;
      return FALSE;
    }
    for (i = 1; i < file->n_strings; i++) {
      guint len;
      char *s;
      READ_U30 (len, bits);
      s = swfdec_bits_get_string_length (bits, len, 9);
      if (s == NULL)
	return FALSE;
      file->strings[i] = swfdec_as_context_give_string (context, s);
      SWFDEC_LOG ("  string %u: %s", i, file->strings[i]);
    }
  }

  /* read all namespaces */
  READ_U30 (file->n_namespaces, bits);
  if (file->n_namespaces) {
    file->namespaces = swfdec_as_context_try_new (context, SwfdecAbcNamespace *, file->n_namespaces);
    if (file->namespaces == NULL) {
      file->n_namespaces = 0;
      return FALSE;
    }
    for (i = 1; i < file->n_namespaces; i++) {
      SwfdecAbcNamespaceType type;
      guint id;
      switch (swfdec_bits_get_u8 (bits)) {
	case 0x05:
	  type = SWFDEC_ABC_NAMESPACE_PRIVATE;
	  break;
	case 0x08:
	case 0x16:
	  type = SWFDEC_ABC_NAMESPACE_PUBLIC;
	  break;
	case 0x17:
	  type = SWFDEC_ABC_NAMESPACE_PACKAGE;
	  break;
	case 0x18:
	  type = SWFDEC_ABC_NAMESPACE_PROTECTED;
	  break;
	case 0x19:
	  type = SWFDEC_ABC_NAMESPACE_EXPLICIT;
	  break;
	case 0x1A:
	  type = SWFDEC_ABC_NAMESPACE_STATIC_PROTECTED;
	  break;
	default:
	  return FALSE;
      }
      READ_U30 (id, bits);
      if (id == 0) {
	SWFDEC_LOG ("  namespace %u: undefined", i);
	file->namespaces[i] = (type == SWFDEC_ABC_NAMESPACE_PRIVATE ? 
	    swfdec_abc_namespace_new : swfdec_as_context_get_namespace) (context,
	    type, NULL, SWFDEC_AS_STR_undefined);
      } else if (id < file->n_strings) {
	SWFDEC_LOG ("  namespace %u: %s", i, file->strings[id]);
	file->namespaces[i] = (type == SWFDEC_ABC_NAMESPACE_PRIVATE ? 
	    swfdec_abc_namespace_new : swfdec_as_context_get_namespace) (context,
	    type, NULL, file->strings[id]);
      } else {
	return FALSE;
      }
    }
  }

  /* read all namespace sets */
  READ_U30 (file->n_nssets, bits);
  if (file->n_nssets) {
    file->nssets = swfdec_as_context_try_new (context, SwfdecAbcNsSet *, file->n_nssets);
    if (file->nssets == NULL) {
      file->n_nssets = 0;
      return FALSE;
    }
    for (i = 0; i < file->n_nssets; i++) {
      file->nssets[i] = swfdec_abc_ns_set_new ();
    }
    for (i = 1; i < file->n_nssets; i++) {
      guint j, len;
      READ_U30 (len, bits);
      for (j = 0; j < len; j++) {
	guint ns;
	READ_U30 (ns, bits);
	if (ns == 0 || ns >= file->n_namespaces)
	  return FALSE;
	swfdec_abc_ns_set_add (file->nssets[i], &file->namespaces[ns]);
      }
      SWFDEC_LOG ("  ns set %u: %u namespaces", i, len);
    }
  }

  /* read all multinames */
  READ_U30 (file->n_multinames, bits);
  if (file->n_multinames) {
    guint nsid, nameid;
    file->multinames = swfdec_as_context_try_new (context, SwfdecAbcMultiname, file->n_multinames);
    if (file->multinames == NULL) {
      file->n_multinames = 0;
      return FALSE;
    }
    for (i = 1; i < file->n_multinames; i++) {
      switch (swfdec_bits_get_u8 (bits)) {
	case 0x0D:
	  SWFDEC_FIXME ("implement attributes");
	case 0x07:
	  READ_U30 (nsid, bits);
	  READ_U30 (nameid, bits);
	  if (nameid >= file->n_strings || nsid >= file->n_namespaces)
	    return FALSE;
	  swfdec_abc_multiname_init (&file->multinames[i],
	      nameid == 0 ? SWFDEC_ABC_MULTINAME_ANY : file->strings[nameid],
	      nsid == 0 ? SWFDEC_ABC_MULTINAME_ANY : file->namespaces[nsid],
	      NULL);
	  break;
	case 0x10:
	  SWFDEC_FIXME ("implement attributes");
	case 0x0F:
	  READ_U30 (nameid, bits);
	  if (nameid >= file->n_strings)
	    return FALSE;
	  swfdec_abc_multiname_init (&file->multinames[i],
	      nameid == 0 ? SWFDEC_ABC_MULTINAME_ANY : file->strings[nameid],
	      NULL, NULL);
	  break;
	case 0x12:
	  SWFDEC_FIXME ("implement attributes");
	case 0x11:
	  swfdec_abc_multiname_init (&file->multinames[i],
	      NULL, NULL, NULL);
	  break;
	case 0x0E:
	  SWFDEC_FIXME ("implement attributes");
	case 0x09:
	  READ_U30 (nameid, bits);
	  READ_U30 (nsid, bits);
	  if (nameid >= file->n_strings || nsid >= file->n_nssets || nsid == 0)
	    return FALSE;
	  swfdec_abc_multiname_init (&file->multinames[i],
	      nameid == 0 ? SWFDEC_ABC_MULTINAME_ANY : file->strings[nameid],
	      NULL, file->nssets[nsid]);
	  break;
	case 0x1C:
	  SWFDEC_FIXME ("implement attributes");
	case 0x1B:
	  READ_U30 (nsid, bits);
	  if (nsid >= file->n_nssets || nsid == 0)
	    return FALSE;
	  swfdec_abc_multiname_init (&file->multinames[i],
	      NULL, NULL, file->nssets[nsid]);
	  break;
	default:
	  SWFDEC_ERROR ("invalid multiname type");
	  return FALSE;
      }
      SWFDEC_LOG ("  multiname %u: %s::%s", i, file->multinames[i].ns == NULL ? 
	  (file->multinames[i].nsset ? "[SET]" : "[RUNTIME]") : 
	  file->multinames[i].ns == SWFDEC_ABC_MULTINAME_ANY ? "[*]" : file->multinames[i].ns->uri,
	  file->multinames[i].name == NULL ? "[RUNTIME]" : 
	  file->multinames[i].name == SWFDEC_ABC_MULTINAME_ANY ? "[*]" : file->multinames[i].name);
    }
  }

  return TRUE;
}

static SwfdecAbcTraits *
swfdec_abc_file_parse_traits (SwfdecAbcFile *file, SwfdecAbcTraits *traits, SwfdecBits *bits)
{
  return NULL;
}

static gboolean
swfdec_abc_file_parse_methods (SwfdecAbcFile *file, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (file);
  guint i;

  READ_U30 (file->n_functions, bits);
  if (file->n_functions) {
    gboolean param_names, optional;
    file->functions = swfdec_as_context_try_new (context, SwfdecAbcFunction *, file->n_functions);
    if (file->functions == NULL) {
      file->n_functions = 0;
      return FALSE;
    }
    for (i = 1; i < file->n_functions; i++) {
      guint id, j, len;
      SwfdecAbcFunction *fun = file->functions[i] = 
	g_object_new (SWFDEC_TYPE_ABC_FUNCTION, "context", context, NULL);
      READ_U30 (len, bits);
      if (len) {
	if (!swfdec_as_context_try_use_mem (context, len * sizeof (SwfdecAbcFunctionArgument)))
	  return FALSE;
	fun->args = g_new0 (SwfdecAbcFunctionArgument, len);
	fun->n_args = len;
      }
      READ_U30 (id, bits);
      if (id == 0) {
	fun->return_type = NULL;
      } else if (id < file->n_multinames) {
	fun->return_type = &file->multinames[id];
      } else {
	return FALSE;
      }
      for (j = 0; j < fun->n_args; j++) {
	READ_U30 (id, bits);
	if (id == 0) {
	  fun->args[j].type = NULL;
	} else if (id < file->n_multinames) {
	  fun->args[j].type = &file->multinames[id];
	} else {
	  return FALSE;
	}
      }
      READ_U30 (id, bits);
      if (id > file->n_strings) {
	return FALSE;
      } else if (id > 0) {
	fun->name = file->strings[id];
      }
      SWFDEC_LOG ("  function %u: %s (%u args)", i, fun->name ? fun->name : "[unnamed]", fun->n_args);
      param_names = swfdec_bits_getbit (bits);
      fun->set_dxns = swfdec_bits_getbit (bits);
      /* ignored = */ swfdec_bits_getbits (bits, 2);
      optional = swfdec_bits_getbit (bits);
      fun->need_rest = swfdec_bits_getbit (bits);
      fun->need_activation = swfdec_bits_getbit (bits);
      fun->need_arguments = swfdec_bits_getbit (bits);
      if (optional) {
	READ_U30 (len, bits);
	if (len == 0 || len > fun->n_args)
	  return FALSE;
	for (j = fun->n_args - len; j < fun->n_args; j++) {
	  READ_U30 (id, bits);
	  fun->args[j].default_index = id;
	  fun->args[j].default_type = swfdec_bits_get_u8 (bits);
	}
      }
      if (param_names) {
	/* Tamarin doesn't parse this, so we must not error out here */
	for (j = 0; j < fun->n_args; j++) {
	  id = swfdec_bits_get_vu32 (bits);
	  if (id > 0 && id < file->n_strings)
	    fun->args[j].name = file->strings[i];
	}
      }
    }
  }

  return TRUE;
}

static gboolean
swfdec_abc_file_skip_metadata (SwfdecAbcFile *file, SwfdecBits *bits)
{
  guint i, ignore, count;

  READ_U30 (ignore, bits);
  READ_U30 (count, bits);
  for (i = 0; i < count; i++) {
    READ_U30 (ignore, bits); /* key */
    READ_U30 (ignore, bits); /* value */
  }
  return TRUE;
}

static gboolean
swfdec_abc_file_parse_qname (SwfdecAbcFile *file, SwfdecBits *bits, SwfdecAbcNamespace **ns, const char **name)
{
  SwfdecAbcMultiname *mn;
  guint id;

  READ_U30 (id, bits);
  if (id == 0 || id >= file->n_multinames)
    THROW (file, "Cpool index %u is out of range %u.", id, file->n_multinames);
  mn = &file->multinames[id];
  if (!swfdec_abc_multiname_is_qualified (mn))
    THROW (file, "Cpool entry %u is wrong type.", id);

  *ns = mn->ns;
  *name = mn->name;
  return TRUE;
}

static gboolean
swfdec_abc_file_parse_instance (SwfdecAbcFile *file, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (file);
  SwfdecAbcNamespace *protected_ns;
  SwfdecAbcTraits *traits;
  guint id, i, n;

  traits = g_object_new (SWFDEC_TYPE_ABC_TRAITS, "context", context, NULL);
  if (!swfdec_abc_file_parse_qname (file, bits, &traits->ns, &traits->name))
    return FALSE;
  
  /* reserved = */ swfdec_bits_getbits (bits, 4);
  traits->protected_ns = swfdec_bits_getbit (bits);
  traits->interface = swfdec_bits_getbit (bits);
  traits->final = swfdec_bits_getbit (bits);
  traits->sealed = swfdec_bits_getbit (bits);
  READ_U30 (id, bits);
  /* id == 0 means no base traits */
  if (id >= file->n_multinames) {
    THROW (file, "Cpool index %u is out of range %u.", id, file->n_multinames);
  } else if (id > 0) {
    SwfdecAbcTraits *base = swfdec_abc_global_get_traits_for_multiname (file->global,
	&file->multinames[id]);
    if (base == NULL)
      return FALSE;
    if (base->final) {
      /* FIXME: check for CLASS and FUNCTION traits */
      THROW (file, "Class %s cannot extend final base class.", file->multinames[id].name);
    }
    if (base->interface || traits->interface) {
      THROW (file, "Class %s cannot extend %s.", file->multinames[id].name, base->name);
    }
  } 
  if (traits->protected_ns) {
    READ_U30 (id, bits);
    if (id == 0) {
      protected_ns = NULL;
    } else if (id < file->n_namespaces) {
      protected_ns = file->namespaces[id];
    } else {
      THROW (file, "Cpool index %u is out of range %u.", id, file->n_multinames);
    }
  } else {
    protected_ns = NULL;
  }
  READ_U30 (n, bits);
  if (n > 0) {
    SWFDEC_FIXME ("implement interface parsing");
    for (i = 0; i < n; i++) {
      READ_U30 (id, bits);
    }
  } 
  READ_U30 (id, bits);
  if (id >= file->n_functions) {
    THROW (file, "Method_info %u exceeds method_count=%u.", id, file->n_functions);
  } else if (file->functions[id] == NULL) {
    THROW (file, "MethodInfo-%u referenced before definition.", id);
  } else {
    if (!swfdec_abc_function_bind (file->functions[id], traits)) {
      THROW (file, "Function %s has already been bound to %s.", file->functions[id]->name,
	  file->functions[id]->construct_traits->name);
    }
  }
  swfdec_abc_file_parse_traits (file, traits, bits);
  swfdec_abc_global_add_traits (file->global, traits);

  return TRUE;
}

static gboolean
swfdec_abc_file_parse_instances (SwfdecAbcFile *file, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (file);
  guint i;

  READ_U30 (file->n_classes, bits);
  if (file->n_classes) {
    file->classes = swfdec_as_context_try_new (context, SwfdecAbcFunction *, file->n_classes);
    if (file->classes == NULL) {
      file->n_classes = 0;
      return FALSE;
    }
    for (i = 1; i < file->n_classes; i++) {
      if (!swfdec_abc_file_parse_instance (file, bits))
	return FALSE;
    }
  }
  return TRUE;
}

static gboolean
swfdec_abc_file_parse (SwfdecAbcFile *file, SwfdecBits *bits)
{
  if (swfdec_abc_file_parse_constants (file, bits) &&
      swfdec_abc_file_parse_methods (file, bits) &&
      swfdec_abc_file_skip_metadata (file, bits) &&
      swfdec_abc_file_parse_instances (file, bits))
    return TRUE;

  return FALSE;
}

SwfdecAbcFile *
swfdec_abc_file_new (SwfdecAsContext *context, SwfdecBits *bits)
{
  SwfdecAbcFile *file;
  guint major, minor;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), NULL);
  g_return_val_if_fail (bits != NULL, NULL);

  file = g_object_new (SWFDEC_TYPE_ABC_FILE, "context", context, NULL);
  file->global = g_object_new (SWFDEC_TYPE_ABC_GLOBAL, "context", context, NULL);

  minor = swfdec_bits_get_u16 (bits);
  major = swfdec_bits_get_u16 (bits);
  SWFDEC_LOG ("  major version: %u", major);
  SWFDEC_LOG ("  minor version: %u", minor);
  if (major == 46 && minor == 16) {
    if (!swfdec_abc_file_parse (file, bits)) {
      swfdec_as_context_throw_abc (context, SWFDEC_ABC_ERROR_VERIFY,
	  "The ABC data is corrupt, attempt to read out of bounds.");
      return NULL;
    }
  } else {
    swfdec_as_context_throw_abc (context, SWFDEC_ABC_ERROR_VERIFY,
	"Not an ABC file.  major_version=%u minor_version=%u.", major, minor);
    return NULL;
  }

  return file;
}
