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
#include "swfdec_abc_script.h"
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
	file->instances);
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

  if (file->main)
    swfdec_gc_object_mark (file->main);

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
  if (x >= (1 << 30)) { \
    SWFDEC_ERROR ("invalid U30 value %u\n", x); \
    return FALSE; \
  } \
}G_STMT_END;
#define THROW(file, ...) G_STMT_START{ \
  swfdec_as_context_throw_abc (swfdec_gc_object_get_context (file), \
      SWFDEC_ABC_ERROR_VERIFY, __VA_ARGS__); \
  return FALSE; \
}G_STMT_END

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
swfdec_abc_file_parse_constants (SwfdecAbcFile *file, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (file);
  guint i;

  SWFDEC_LOG ("parsing ABC block");
  /* read all integers */
  READ_U30 (file->n_ints, bits);
  SWFDEC_LOG ("%u integers", file->n_ints);
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
  SWFDEC_LOG ("%u unsigned integers", file->n_uints);
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
  SWFDEC_LOG ("%u doubles", file->n_doubles);
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
  SWFDEC_LOG ("%u strings", file->n_strings);
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
  SWFDEC_LOG ("%u namespaces", file->n_namespaces);
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
	  THROW (file, "Cpool entry %u is wrong type.", i);
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
	THROW (file, "Cpool index %u is out of range %u.", id, file->n_strings);
      }
    }
  }

  /* read all namespace sets */
  READ_U30 (file->n_nssets, bits);
  SWFDEC_LOG ("%u namespace sets", file->n_nssets);
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
  SWFDEC_LOG ("%u multinames", file->n_multinames);
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
	  if (nsid >= file->n_namespaces)
	    THROW (file, "Cpool index %u is out of range %u.", nsid, file->n_namespaces);
	  READ_U30 (nameid, bits);
	  if (nameid >= file->n_strings)
	    THROW (file, "Cpool index %u is out of range %u.", nameid, file->n_strings);
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
	    THROW (file, "Cpool index %u is out of range %u.", nameid, file->n_strings);
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
	  if (nameid >= file->n_strings)
	    THROW (file, "Cpool index %u is out of range %u.", nameid, file->n_strings);
	  READ_U30 (nsid, bits);
	  if (nsid >= file->n_nssets)
	    THROW (file, "Cpool index %u is out of range %u.", nsid, file->n_nssets);
	  swfdec_abc_multiname_init (&file->multinames[i],
	      nameid == 0 ? SWFDEC_ABC_MULTINAME_ANY : file->strings[nameid],
	      NULL, file->nssets[nsid]);
	  break;
	case 0x1C:
	  SWFDEC_FIXME ("implement attributes");
	case 0x1B:
	  READ_U30 (nsid, bits);
	  if (nsid >= file->n_nssets)
	    THROW (file, "Cpool index %u is out of range %u.", nsid, file->n_nssets);
	  swfdec_abc_multiname_init (&file->multinames[i],
	      NULL, NULL, file->nssets[nsid]);
	  break;
	default:
	  THROW (file, "Cpool entry %u is wrong type.", i);
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

static gboolean
swfdec_abc_file_parse_method (SwfdecAbcFile *file, SwfdecBits *bits, SwfdecAbcTraits *traits, SwfdecAbcFunction **ret)
{
  guint id;

  READ_U30 (id, bits);
  if (id >= file->n_functions) {
    THROW (file, "Method_info %u exceeds method_count=%u.", id, file->n_functions);
  } else if (file->functions[id] == NULL) {
    THROW (file, "MethodInfo-%u referenced before definition.", id);
  } else if (traits) {
    if (!swfdec_abc_function_bind (file->functions[id], traits)) {
      THROW (file, "Function %s has already been bound to %s.", file->functions[id]->name,
	  file->functions[id]->bound_traits->name);
    }
    g_assert (traits->construct == NULL);
    traits->construct = file->functions[id];
    SWFDEC_LOG ("binding method %u to traits %s", id, traits->name);
  } else {
    SWFDEC_LOG ("found method %u", id);
  }
  if (ret)
    *ret = file->functions[id];
  return TRUE;
}

static gboolean
swfdec_abc_file_parse_traits (SwfdecAbcFile *file, SwfdecAbcTraits *traits, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (file);
  SwfdecAbcTraits *base = traits->base;
  guint i, n_slots, n_methods, n_traits;
  gboolean early, metadata;

  READ_U30 (n_traits, bits);
  SWFDEC_LOG ("%u traits", n_traits);
  if (n_traits) {
    traits->traits = swfdec_as_context_try_new (context, SwfdecAbcTrait, n_traits);
    if (traits->traits == NULL)
      return FALSE;
    traits->n_traits = n_traits;
  }

  /* copy protected traits from base class into new protected namespace */
  if (base && base->protected_ns && traits->protected_ns) {
    SWFDEC_FIXME ("copy protected slots");
  }

  if (base) {
    n_slots = base->n_slots;
    n_methods = base->n_methods;
  } else {
    n_slots = 0;
    n_methods = 0;
  }

  early = base ? swfdec_abc_traits_allow_early_binding (traits) : TRUE;
  SWFDEC_LOG ("  %sallowing early binding", early ? "" : "NOT ");

  for (i = 0; i < n_traits; i++) {
    SwfdecAbcTrait *trait = &traits->traits[i];
    guint type;
    SWFDEC_LOG ("trait %u", i);
    if (!swfdec_abc_file_parse_qname (file, bits, &trait->ns, &trait->name))
      return FALSE;
    SWFDEC_LOG ("  name: %s::%s", trait->ns->uri, trait->name);

    /* reserved = */ swfdec_bits_getbit (bits);
    metadata = swfdec_bits_getbit (bits);
    SWFDEC_LOG ("  metadata: %d", metadata);
    trait->override = swfdec_bits_getbit (bits);
    SWFDEC_LOG ("  override: %d", trait->override);
    trait->final = swfdec_bits_getbit (bits);
    SWFDEC_LOG ("  final: %d", trait->final);
    type = swfdec_bits_getbits (bits, 4);
    SWFDEC_LOG ("  type: %u", type);
    switch (type) {
      case 0: /* slot */
      case 6: /* const */
      case 4: /* class */
	{
          guint slot;
	  READ_U30 (slot, bits);
	  if (!early)
	    slot = 0;
	  if (slot == 0) {
	    slot = n_slots;
	    n_slots++;
	    SWFDEC_LOG ("  slot: %u (automatic)", slot);
	  } else {
	    if (slot > n_traits || slot == 0)
	      THROW (file, "The ABC data is corrupt, attempt to read out of bounds.");
	    if (slot > n_slots)
	      n_slots = slot;
	    slot--;
	    if (base && slot < base->n_slots)
	      THROW (file, "Illegal override of %s in %s.", trait->name, traits->name);
	    SWFDEC_LOG ("  slot: %u", slot);
	  }

	  if (swfdec_abc_traits_get_trait (traits, trait->ns, trait->name))
	    THROW (file, "The ABC data is corrupt, attempt to read out of bounds.");
				
	  /* FIXME: if (script) addNamedScript(ns, name, script); */

	  if (type == 4) {
	    guint id;
	    READ_U30 (id, bits);
	    if (id >= file->n_classes)
	      THROW (file, "ClassInfo %u exceeds class_count=%u.", id, file->n_functions);
	    if (file->classes[id] == NULL)
	      THROW (file, "ClassInfo-%u is referenced before definition.", id);

	    /* if (script) addNamedTraits(ns, name, cinit->declaringTraits->itraits); */
	    
	    trait->type = SWFDEC_ABC_BINDING_NEW (SWFDEC_ABC_TRAIT_CONST, slot);
	  } else {
	    g_assert (type == 0 || type == 6);
	    READ_U30 (trait->slot.type, bits);
	    READ_U30 (trait->slot.default_index, bits);
	    if (trait->slot.default_index) {
	      trait->slot.default_type = swfdec_bits_get_u8 (bits);
	    }
	    trait->type = SWFDEC_ABC_BINDING_NEW (type == 0 ? SWFDEC_ABC_TRAIT_SLOT : SWFDEC_ABC_TRAIT_CONST, slot);
	  }
	}
	break;
      case 1: /* method */
      case 2: /* getter */
      case 3: /* setter */
	{
	  guint ignore;
	  SwfdecAbcBinding bind;
	  const SwfdecAbcTrait *found;
	  SwfdecAbcFunction *fun;
	  READ_U30 (ignore, bits);
	  SWFDEC_LOG ("  display id = %u (ignored)", ignore);
	  if (!swfdec_abc_file_parse_method (file, bits, NULL, &fun))
	    return FALSE;
	  if (!swfdec_abc_function_bind (fun, traits)) {
	    THROW (file, "Function %s has already been bound to %s.", fun->name,
		fun->bound_traits->name);
	  }

#if 0
	  // only export one name for an accessor 
	  if (script && !domain->getNamedScript(name,ns))
		  addNamedScript(ns, name, script);
#endif
	  
	  if (traits->base && (found = swfdec_abc_traits_find_trait (traits->base,
	      trait->ns == traits->protected_ns ? traits->base->protected_ns: trait->ns, 
	      trait->name))) {
	    bind = found->type;
	    SWFDEC_LOG ("  found method %u of type %u to override", 
		SWFDEC_ABC_BINDING_GET_ID (bind), SWFDEC_ABC_BINDING_GET_TYPE (bind));
	  } else {
	    bind = SWFDEC_ABC_BINDING_NONE;
	  }

	  if (type == 1) { 
	    /* method */
	    if (bind == SWFDEC_ABC_BINDING_NONE) {
	      if (trait->override)
		THROW (file, "Illegal override of %s in %s.", trait->name, traits->name);
	      trait->type = SWFDEC_ABC_BINDING_NEW (SWFDEC_ABC_TRAIT_METHOD, n_methods);
	      SWFDEC_LOG ("  method: %u (new)", n_methods);
	      n_methods++;
	    } else if (SWFDEC_ABC_BINDING_IS_TYPE (bind, SWFDEC_ABC_TRAIT_METHOD)) {
	      if (!trait->override)
		THROW (file, "Illegal override of %s in %s.", trait->name, traits->name);
	      trait->type = SWFDEC_ABC_BINDING_NEW (SWFDEC_ABC_TRAIT_METHOD, 
		  SWFDEC_ABC_BINDING_GET_ID (bind));
	      SWFDEC_LOG ("  method: %u (override)", SWFDEC_ABC_BINDING_GET_ID (bind));
	    } else {
	      THROW (file, "The ABC data is corrupt, attempt to read out of bounds.");
	    }
	  } else {
	    SwfdecAbcTraitType ttype = type == 2 ? SWFDEC_ABC_TRAIT_GET : SWFDEC_ABC_TRAIT_SET;
	    /* getter or setter */

	    /* check override */
	    if (bind == SWFDEC_ABC_BINDING_NONE) {
	      if (trait->override)
		THROW (file, "Illegal override of %s in %s.", trait->name, traits->name);
	    } else if (SWFDEC_ABC_BINDING_IS_ACCESSOR (bind)) {
	      if ((trait->override ? TRUE : FALSE) != ((SWFDEC_ABC_BINDING_GET_TYPE (bind) & ttype) == ttype))
		THROW (file, "Illegal override of %s in %s.", trait->name, traits->name);
	      SWFDEC_LOG ("  method: %u (override)", SWFDEC_ABC_BINDING_GET_ID (bind));
	    } else {
	      THROW (file, "The ABC data is corrupt, attempt to read out of bounds.");
	    }

	    /* try to find own trait */
	    found = swfdec_abc_traits_get_trait (traits, trait->ns, trait->name);
	    if (found) {
	      /* FIXME: This is kinda (read: very) hacky */
	      ((SwfdecAbcTrait *) found)->type = SWFDEC_ABC_BINDING_NEW (SWFDEC_ABC_TRAIT_GETSET, 
		  SWFDEC_ABC_BINDING_GET_ID (found->type));
	      SWFDEC_LOG ("  method: %u (reuse)", SWFDEC_ABC_BINDING_GET_ID (found->type));
	    } else if (bind == SWFDEC_ABC_BINDING_NONE) {
	      trait->type = SWFDEC_ABC_BINDING_NEW (ttype, n_methods);
	      SWFDEC_LOG ("  method: %u (new)", n_methods);
	      n_methods += 2;
	    } else {
	      trait->type = SWFDEC_ABC_BINDING_NEW (SWFDEC_ABC_BINDING_GET_TYPE (bind) | ttype,
		  SWFDEC_ABC_BINDING_GET_ID (bind));
	    }
	  }
	}
        break;
      default:
	THROW (file, "Unsupported traits kind=%u.", type);
    }

    if (metadata) {
      guint ignore, j, count;
      READ_U30 (count, bits);
  
      SWFDEC_INFO ("parse metadata, in particular \"NeedsDxns\"");
      for (j = 0; j < count; j++) {
	READ_U30 (ignore, bits);
      }
    }

    traits->n_slots = n_slots;
    traits->n_methods = n_methods;
  }

  return TRUE;
}

static gboolean
swfdec_abc_file_parse_methods (SwfdecAbcFile *file, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (file);
  guint i;

  READ_U30 (file->n_functions, bits);
  SWFDEC_LOG ("%u methods", file->n_functions);
  if (file->n_functions) {
    gboolean param_names, optional, native;
    file->functions = swfdec_as_context_try_new (context, SwfdecAbcFunction *, file->n_functions);
    if (file->functions == NULL) {
      file->n_functions = 0;
      return FALSE;
    }
    for (i = 0; i < file->n_functions; i++) {
      guint id, j, len;
      SwfdecAbcFunction *fun = file->functions[i] = 
	g_object_new (SWFDEC_TYPE_ABC_FUNCTION, "context", context, NULL);
      READ_U30 (len, bits);
      SWFDEC_LOG ("  function %u:", i);
      if (len) {
	fun->args = swfdec_as_context_try_new (context, SwfdecAbcFunctionArgument, len);
	if (fun->args == NULL)
	  return FALSE;
	fun->n_args = len;
      }
      READ_U30 (id, bits);
      if (id == 0) {
	fun->return_type = NULL;
      } else if (id < file->n_multinames) {
	fun->return_type = &file->multinames[id];
      } else {
	THROW (file, "Cpool index %u is out of range %u.", id, file->n_multinames);
      }
      SWFDEC_LOG ("    return type: %s", fun->return_type ? ((SwfdecAbcMultiname *) fun->return_type)->name : "void");
      for (j = 0; j < fun->n_args; j++) {
	READ_U30 (id, bits);
	if (id == 0) {
	  fun->args[j].type = NULL;
	} else if (id < file->n_multinames) {
	  fun->args[j].type = &file->multinames[id];
	} else {
	  THROW (file, "Cpool index %u is out of range %u.", id, file->n_multinames);
	}
	SWFDEC_LOG ("    argument %u: type %s", j, fun->args[j].type ? ((SwfdecAbcMultiname *) fun->args[j].type)->name : "void");
      }
      READ_U30 (id, bits);
      if (id > file->n_strings) {
	THROW (file, "Cpool index %u is out of range %u.", id, file->n_strings);
      } else if (id > 0) {
	fun->name = file->strings[id];
      }
      param_names = swfdec_bits_getbit (bits);
      fun->set_dxns = swfdec_bits_getbit (bits);
      native = swfdec_bits_getbit (bits);
      /* ignored = */ swfdec_bits_getbit (bits);
      optional = swfdec_bits_getbit (bits);
      fun->need_rest = swfdec_bits_getbit (bits);
      fun->need_activation = swfdec_bits_getbit (bits);
      fun->need_arguments = swfdec_bits_getbit (bits);
      if (native) {
	SWFDEC_FIXME ("supposed to load native method %u here", i);
      }
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
  guint i, j, ignore, count, count2;

  READ_U30 (count, bits);
  for (i = 0; i < count; i++) {
    READ_U30 (ignore, bits);
    READ_U30 (count2, bits);
    for (j = 0; j < count2; j++) {
      READ_U30 (ignore, bits); /* key */
      READ_U30 (ignore, bits); /* value */
    }
  }
  return TRUE;
}

static gboolean
swfdec_abc_file_parse_instance (SwfdecAbcFile *file, guint instance_id, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (file);
  gboolean protected_ns;
  SwfdecAbcTraits *traits;
  guint id, i, n;

  SWFDEC_LOG ("instance: %u", instance_id);
  traits = g_object_new (SWFDEC_TYPE_ABC_TRAITS, "context", context, NULL);
  traits->pool = file;
  if (!swfdec_abc_file_parse_qname (file, bits, &traits->ns, &traits->name))
    return FALSE;
  SWFDEC_LOG ("  name: %s::%s", traits->ns->uri, traits->name);
  
  READ_U30 (id, bits);
  /* id == 0 means no base traits */
  if (id >= file->n_multinames) {
    THROW (file, "Cpool index %u is out of range %u.", id, file->n_multinames);
  } else if (id > 0) {
    SwfdecAbcTraits *base = swfdec_abc_global_get_traits_for_multiname (file->global,
	&file->multinames[id]);
    if (base == NULL)
      THROW (file, "Class %s could not be found.", file->multinames[id].name);
    if (base->final) {
      /* FIXME: check for CLASS and FUNCTION traits */
      THROW (file, "Class %s cannot extend final base class.", file->multinames[id].name);
    }
    if (base->interface || traits->interface) {
      THROW (file, "Class %s cannot extend %s.", file->multinames[id].name, base->name);
    }
    traits->base = base;
    SWFDEC_LOG ("  base: %s::%s", base->ns->uri, base->name);
  } else {
    SWFDEC_LOG ("  no base type");
  }
  /* reserved = */ swfdec_bits_getbits (bits, 4);
  protected_ns = swfdec_bits_getbit (bits);
  traits->interface = swfdec_bits_getbit (bits);
  traits->final = swfdec_bits_getbit (bits);
  traits->sealed = swfdec_bits_getbit (bits);
  if (protected_ns) {
    READ_U30 (id, bits);
    if (id == 0) {
      /* traits->protected_ns = NULL; */
    } else if (id < file->n_namespaces) {
      traits->protected_ns = file->namespaces[id];
    } else if (i != 0) {
      THROW (file, "Cpool index %u is out of range %u.", id, file->n_multinames);
    }
  }
  READ_U30 (n, bits);
  if (n > 0) {
    SWFDEC_FIXME ("implement interface parsing");
    for (i = 0; i < n; i++) {
      READ_U30 (id, bits);
    }
  } 
  if (!swfdec_abc_file_parse_method (file, bits, traits, NULL))
    return FALSE;

  if (!swfdec_abc_file_parse_traits (file, traits, bits))
    return FALSE;
  swfdec_abc_global_add_traits (file->global, traits);
  file->instances[instance_id] = traits;

  return TRUE;
}

static gboolean
swfdec_abc_file_parse_instances (SwfdecAbcFile *file, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (file);
  guint i;

  READ_U30 (file->n_classes, bits);
  SWFDEC_LOG ("%u classes", file->n_classes);
  if (file->n_classes) {
    file->instances = swfdec_as_context_try_new (context, SwfdecAbcFunction *, file->n_classes);
    file->classes = swfdec_as_context_try_new (context, SwfdecAbcFunction *, file->n_classes);
    if (file->instances == NULL || file->classes == NULL) {
      if (file->instances) {
	swfdec_as_context_free (context, sizeof (SwfdecAbcFunction *) * file->n_classes, file->instances);
	file->instances = NULL;
      }
      if (file->classes) {
	swfdec_as_context_free (context, sizeof (SwfdecAbcFunction *) * file->n_classes, file->classes);
	file->classes = NULL;
      }
      file->n_classes = 0;
      return FALSE;
    }
    for (i = 0; i < file->n_classes; i++) {
      if (!swfdec_abc_file_parse_instance (file, i, bits))
	return FALSE;
    }
  }
  return TRUE;
}

static gboolean
swfdec_abc_file_parse_classes (SwfdecAbcFile *file, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (file);
  guint i;

  if (file->n_classes) {
    for (i = 0; i < file->n_classes; i++) {
      SwfdecAbcTraits *traits;

      SWFDEC_LOG ("class %u", i);
      traits = g_object_new (SWFDEC_TYPE_ABC_TRAITS, "context", context, NULL);
      traits->pool = file;

      traits->ns = file->instances[i]->ns;
      traits->name = file->instances[i]->name;
      traits->final = TRUE;
      traits->sealed = FALSE;

      if (!swfdec_abc_file_parse_method (file, bits, traits, NULL))
	return FALSE;

      if (!swfdec_abc_file_parse_traits (file, traits, bits))
	return FALSE;

      traits->instance_traits = file->instances[i];
      file->classes[i] = traits;
    }
  }
  return TRUE;
}

static gboolean
swfdec_abc_file_parse_scripts (SwfdecAbcFile *file, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (file);
  SwfdecAbcScript *script = NULL;
  SwfdecAbcTraits *traits;
  SwfdecAbcFunction *fun;
  guint i, j, n_scripts;

  READ_U30 (n_scripts, bits);
  SWFDEC_LOG ("%u scripts ", n_scripts);
  for (i = 0; i < n_scripts; i++) {
    SWFDEC_LOG ("script %u", i);
    traits = g_object_new (SWFDEC_TYPE_ABC_TRAITS, "context", context, NULL);
    traits->pool = file;
    traits->final = TRUE;
    traits->ns = context->public_ns;
    traits->name = SWFDEC_AS_STR_global;
    /* FIXME: traits->base = OBJECT_TYPE */
    if (!swfdec_abc_file_parse_method (file, bits, traits, &fun))
      return FALSE;
    traits->construct = fun;
    if (!swfdec_abc_file_parse_traits (file, traits, bits))
      return FALSE;
    script = g_object_new (SWFDEC_TYPE_ABC_SCRIPT, "context", context, NULL);
    script->traits = traits;
    for (j = 0; j < traits->n_traits; j++) {
      SwfdecAbcTrait *trait = &traits->traits[j];
      if (trait->type == SWFDEC_ABC_TRAIT_NONE)
	continue;
      swfdec_abc_global_add_script (file->global, trait->ns, trait->name, script, 
	  i + 1 == n_scripts);
    }
  }
  if (script == 0)
    THROW (file, "No entry point was found.");
  file->main = script;
  return TRUE;
}

static gboolean
swfdec_abc_file_parse_bodies (SwfdecAbcFile *file, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (file);
  SwfdecAbcTraits *traits;
  SwfdecAbcFunction *fun;
  guint i, j, max_scope, min_scope, n_bodies, size;

  READ_U30 (n_bodies, bits);
  SWFDEC_LOG ("%u bodies", n_bodies);
  for (i = 0; i < n_bodies; i++) {
    SWFDEC_LOG ("body %u", i);
    if (!swfdec_abc_file_parse_method (file, bits, NULL, &fun))
      return FALSE;
    if (swfdec_abc_function_is_native (fun))
      THROW (file, "Native method %u has illegal method body.", fun - file->functions[0]);
    if (fun->bound_traits && fun->bound_traits->interface)
      THROW (file, "Interface method %u has illegal method body.", fun - file->functions[0]);
    if (fun->code != NULL)
      THROW (file, "Method %u has a duplicate method body.", fun - file->functions[0]);

    READ_U30 (fun->stack, bits);
    SWFDEC_LOG ("  stack: %u", fun->stack);
    READ_U30 (fun->locals, bits);
    if (fun->n_args >= fun->locals)
      THROW (file, "The ABC data is corrupt, attempt to read out of bounds.");
    SWFDEC_LOG ("  locals: %u", fun->locals);
    READ_U30 (min_scope, bits);
    READ_U30 (max_scope, bits);
    if (min_scope > max_scope)
      THROW (file, "The ABC data is corrupt, attempt to read out of bounds.");
    fun->scope = max_scope - min_scope;
    SWFDEC_LOG ("  scope: %u (%u - %u)", fun->scope, min_scope, max_scope);
    READ_U30 (size, bits);
    if (size == 0)
      THROW (file, "Invalid code_length=0.");
    SWFDEC_LOG ("  code: %u bytes", size);
    fun->code = swfdec_bits_get_buffer (bits, size);
    if (fun->code == NULL)
      THROW (file, "The ABC data is corrupt, attempt to read out of bounds.");

    READ_U30 (fun->n_exceptions, bits);
    SWFDEC_LOG ("  exceptions: %u", fun->n_exceptions);
    if (fun->n_exceptions) {
      fun->exceptions = swfdec_as_context_try_new (context, SwfdecAbcException, fun->n_exceptions);
      if (fun->exceptions == NULL) {
	fun->n_exceptions = 0;
	return FALSE;
      }
      for (j = 0; j < fun->n_exceptions; j++) {
	SwfdecAbcException *ex = &fun->exceptions[j];
	READ_U30 (ex->from, bits);
	READ_U30 (ex->to, bits);
	READ_U30 (ex->target, bits);
	READ_U30 (ex->type, bits);
	READ_U30 (ex->var, bits);
      }
    }

    traits = g_object_new (SWFDEC_TYPE_ABC_TRAITS, "context", context, NULL);
    traits->pool = file;
    traits->final = TRUE;
    traits->ns = context->public_ns;
    traits->name = SWFDEC_AS_STR_EMPTY;
    /* FIXME: traits->base = OBJECT_TYPE */
    if (!swfdec_abc_file_parse_traits (file, traits, bits))
      return FALSE;
    fun->activation = traits;
  }
  return TRUE;
}

static gboolean
swfdec_abc_file_parse (SwfdecAbcFile *file, SwfdecBits *bits)
{
  return swfdec_abc_file_parse_constants (file, bits) &&
      swfdec_abc_file_parse_methods (file, bits) &&
      swfdec_abc_file_skip_metadata (file, bits) &&
      swfdec_abc_file_parse_instances (file, bits) &&
      swfdec_abc_file_parse_classes (file, bits) &&
      swfdec_abc_file_parse_scripts (file, bits) &&
      swfdec_abc_file_parse_bodies (file, bits);
}

SwfdecAbcFile *
swfdec_abc_file_new (SwfdecAsContext *context, SwfdecBits *bits)
{
  SwfdecAbcFile *file;
  guint major, minor;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), NULL);
  g_return_val_if_fail (SWFDEC_IS_ABC_GLOBAL (context->global), NULL);
  g_return_val_if_fail (bits != NULL, NULL);

  file = g_object_new (SWFDEC_TYPE_ABC_FILE, "context", context, NULL);
  file->global = SWFDEC_ABC_GLOBAL (context->global);

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
