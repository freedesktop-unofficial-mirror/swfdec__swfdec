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

#include "swfdec_abc_pool.h"

#include <math.h>

#include "swfdec_abc_function.h"
#include "swfdec_abc_internal.h"
#include "swfdec_abc_script.h"
#include "swfdec_abc_traits.h"
#include "swfdec_as_frame.h" /* default stub */
#include "swfdec_as_strings.h"
#include "swfdec_debug.h"

/*** SWFDEC_ABC_POOL ***/

G_DEFINE_TYPE (SwfdecAbcPool, swfdec_abc_pool, SWFDEC_TYPE_GC_OBJECT)

static void
swfdec_abc_pool_dispose (GObject *object)
{
  SwfdecAbcPool *pool = SWFDEC_ABC_POOL (object);
  SwfdecAsContext *context = swfdec_gc_object_get_context (pool);

  if (pool->n_ints) {
    swfdec_as_context_free (context, pool->n_ints * sizeof (int), pool->ints);
  }
  if (pool->n_uints) {
    swfdec_as_context_free (context, pool->n_uints * sizeof (guint), pool->uints);
  }
  if (pool->n_doubles) {
    swfdec_as_context_free (context, pool->n_doubles * sizeof (double), pool->doubles);
  }
  if (pool->n_strings) {
    swfdec_as_context_free (context, pool->n_strings * sizeof (const char *), pool->strings);
  }
  if (pool->n_namespaces) {
    swfdec_as_context_free (context, pool->n_namespaces * sizeof (SwfdecAbcNamespace *),
	pool->namespaces);
  }
  if (pool->n_nssets) {
    guint i;
    for (i = 0; i < pool->n_nssets; i++) {
      swfdec_abc_ns_set_free (pool->nssets[i]);
    }
    swfdec_as_context_free (context, pool->n_nssets * sizeof (SwfdecAbcNsSet *),
	pool->nssets);
  }
  if (pool->n_multinames) {
    swfdec_as_context_free (context, pool->n_multinames * sizeof (SwfdecAbcMultiname),
	pool->multinames);
  }
  if (pool->n_functions) {
    swfdec_as_context_free (context, pool->n_functions * sizeof (SwfdecAbcFunction *),
	pool->functions);
  }
  if (pool->n_classes) {
    swfdec_as_context_free (context, pool->n_classes * sizeof (SwfdecAbcFunction *),
	pool->instances);
    swfdec_as_context_free (context, pool->n_classes * sizeof (SwfdecAbcFunction *),
	pool->classes);
  }

  G_OBJECT_CLASS (swfdec_abc_pool_parent_class)->dispose (object);
}

static void
swfdec_abc_pool_mark (SwfdecGcObject *object)
{
  SwfdecAbcPool *pool = SWFDEC_ABC_POOL (object);
  guint i;

  for (i = 0; i < pool->n_strings; i++) {
    swfdec_as_string_mark (pool->strings[i]);
  }
  for (i = 1; i < pool->n_namespaces; i++) {
    swfdec_gc_object_mark (pool->namespaces[i]);
  }
  for (i = 1; i < pool->n_functions; i++) {
    if (pool->functions[i])
      swfdec_gc_object_mark (pool->functions[i]);
  }
  for (i = 1; i < pool->n_classes; i++) {
    if (pool->classes[i])
      swfdec_gc_object_mark (pool->classes[i]);
  }

  if (pool->main)
    swfdec_gc_object_mark (pool->main);

  SWFDEC_GC_OBJECT_CLASS (swfdec_abc_pool_parent_class)->mark (object);
}

static void
swfdec_abc_pool_class_init (SwfdecAbcPoolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_abc_pool_dispose;

  gc_class->mark = swfdec_abc_pool_mark;
}

static void
swfdec_abc_pool_init (SwfdecAbcPool *date)
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
#define THROW(pool, ...) G_STMT_START{ \
  swfdec_as_context_throw_abc (swfdec_gc_object_get_context (pool), \
      SWFDEC_ABC_TYPE_VERIFY_ERROR, __VA_ARGS__); \
  return FALSE; \
}G_STMT_END

static gboolean
swfdec_abc_pool_parse_qname (SwfdecAbcPool *pool, SwfdecBits *bits, SwfdecAbcNamespace **ns, const char **name)
{
  SwfdecAbcMultiname *mn;
  guint id;

  READ_U30 (id, bits);
  if (id == 0 || id >= pool->n_multinames)
    THROW (pool, "Cpool index %u is out of range %u.", id, pool->n_multinames);
  mn = &pool->multinames[id];
  if (!swfdec_abc_multiname_is_qualified (mn))
    THROW (pool, "Cpool entry %u is wrong type.", id);

  *ns = mn->ns;
  *name = mn->name;
  return TRUE;
}

static gboolean
swfdec_abc_pool_parse_constants (SwfdecAbcPool *pool, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (pool);
  guint i;

  SWFDEC_LOG ("parsing ABC block");
  /* read all integers */
  READ_U30 (pool->n_ints, bits);
  SWFDEC_LOG ("%u integers", pool->n_ints);
  if (pool->n_ints) {
    pool->ints = swfdec_as_context_try_new (context, int, pool->n_ints);
    if (pool->ints == NULL) {
      pool->n_ints = 0;
      return FALSE;
    }
    for (i = 1; i < pool->n_ints && swfdec_bits_left (bits); i++) {
      pool->ints[i] = swfdec_bits_get_vs32 (bits);
      SWFDEC_LOG ("  int %u: %d", i, pool->ints[i]);
    }
  }

  /* read all unsigned integers */
  READ_U30 (pool->n_uints, bits);
  SWFDEC_LOG ("%u unsigned integers", pool->n_uints);
  if (pool->n_uints) {
    pool->uints = swfdec_as_context_try_new (context, guint, pool->n_uints);
    if (pool->uints == NULL) {
      pool->n_uints = 0;
      return FALSE;
    }
    for (i = 1; i < pool->n_uints && swfdec_bits_left (bits); i++) {
      pool->uints[i] = swfdec_bits_get_vu32 (bits);
      SWFDEC_LOG ("  uint %u: %u", i, pool->uints[i]);
    }
  }

  /* read all doubles */
  READ_U30 (pool->n_doubles, bits);
  SWFDEC_LOG ("%u doubles", pool->n_doubles);
  if (pool->n_doubles) {
    pool->doubles = swfdec_as_context_try_new (context, double, pool->n_doubles);
    if (pool->doubles == NULL) {
      pool->n_doubles = 0;
      return FALSE;
    }
    pool->doubles[0] = NAN;
    for (i = 1; i < pool->n_doubles && swfdec_bits_left (bits); i++) {
      pool->doubles[i] = swfdec_bits_get_ldouble (bits);
      SWFDEC_LOG ("  double %u: %g", i, pool->doubles[i]);
    }
  }

  /* read all strings */
  READ_U30 (pool->n_strings, bits);
  SWFDEC_LOG ("%u strings", pool->n_strings);
  if (pool->n_strings) {
    pool->strings = swfdec_as_context_try_new (context, const char *, pool->n_strings);
    if (pool->strings == NULL) {
      pool->n_strings = 0;
      return FALSE;
    }
    for (i = 1; i < pool->n_strings; i++) {
      guint len;
      char *s;
      READ_U30 (len, bits);
      s = swfdec_bits_get_string_length (bits, len, 9);
      if (s == NULL)
	return FALSE;
      pool->strings[i] = swfdec_as_context_give_string (context, s);
      SWFDEC_LOG ("  string %u: %s", i, pool->strings[i]);
    }
  }

  /* read all namespaces */
  READ_U30 (pool->n_namespaces, bits);
  SWFDEC_LOG ("%u namespaces", pool->n_namespaces);
  if (pool->n_namespaces) {
    pool->namespaces = swfdec_as_context_try_new (context, SwfdecAbcNamespace *, pool->n_namespaces);
    if (pool->namespaces == NULL) {
      pool->n_namespaces = 0;
      return FALSE;
    }
    for (i = 1; i < pool->n_namespaces; i++) {
      SwfdecAbcNamespaceType type;
      guint id;
      switch (swfdec_bits_get_u8 (bits)) {
	case SWFDEC_ABC_CONST_PRIVATE_NAMESPACE:
	  type = SWFDEC_ABC_NAMESPACE_PRIVATE;
	  break;
	case SWFDEC_ABC_CONST_NAMESPACE:
	case SWFDEC_ABC_CONST_PACKAGE_NAMESPACE:
	  type = SWFDEC_ABC_NAMESPACE_PUBLIC;
	  break;
	case SWFDEC_ABC_CONST_INTERNAL_NAMESPACE:
	  type = SWFDEC_ABC_NAMESPACE_PACKAGE;
	  break;
	case SWFDEC_ABC_CONST_PROTECTED_NAMESPACE:
	  type = SWFDEC_ABC_NAMESPACE_PROTECTED;
	  break;
	case SWFDEC_ABC_CONST_EXPLICIT_NAMESPACE:
	  type = SWFDEC_ABC_NAMESPACE_EXPLICIT;
	  break;
	case SWFDEC_ABC_CONST_STATIC_PROTECTED_NAMESPACE:
	  type = SWFDEC_ABC_NAMESPACE_STATIC_PROTECTED;
	  break;
	default:
	  THROW (pool, "Cpool entry %u is wrong type.", i);
      }
      READ_U30 (id, bits);
      if (id == 0) {
	SWFDEC_LOG ("  namespace %u: undefined", i);
	pool->namespaces[i] = (type == SWFDEC_ABC_NAMESPACE_PRIVATE ? 
	    swfdec_abc_namespace_new : swfdec_as_context_get_namespace) (context,
	    type, NULL, SWFDEC_AS_STR_undefined);
      } else if (id < pool->n_strings) {
	SWFDEC_LOG ("  namespace %u: %s", i, pool->strings[id]);
	pool->namespaces[i] = (type == SWFDEC_ABC_NAMESPACE_PRIVATE ? 
	    swfdec_abc_namespace_new : swfdec_as_context_get_namespace) (context,
	    type, NULL, pool->strings[id]);
      } else {
	THROW (pool, "Cpool index %u is out of range %u.", id, pool->n_strings);
      }
    }
  }

  /* read all namespace sets */
  READ_U30 (pool->n_nssets, bits);
  SWFDEC_LOG ("%u namespace sets", pool->n_nssets);
  if (pool->n_nssets) {
    pool->nssets = swfdec_as_context_try_new (context, SwfdecAbcNsSet *, pool->n_nssets);
    if (pool->nssets == NULL) {
      pool->n_nssets = 0;
      return FALSE;
    }
    for (i = 0; i < pool->n_nssets; i++) {
      pool->nssets[i] = swfdec_abc_ns_set_new ();
    }
    for (i = 1; i < pool->n_nssets; i++) {
      guint j, len;
      READ_U30 (len, bits);
      for (j = 0; j < len; j++) {
	guint ns;
	READ_U30 (ns, bits);
	if (ns == 0 || ns >= pool->n_namespaces)
	  return FALSE;
	swfdec_abc_ns_set_add (pool->nssets[i], pool->namespaces[ns]);
      }
      SWFDEC_LOG ("  ns set %u: %u namespaces", i, len);
    }
  }

  /* read all multinames */
  READ_U30 (pool->n_multinames, bits);
  SWFDEC_LOG ("%u multinames", pool->n_multinames);
  if (pool->n_multinames) {
    guint nsid, nameid;
    pool->multinames = swfdec_as_context_try_new (context, SwfdecAbcMultiname, pool->n_multinames);
    if (pool->multinames == NULL) {
      pool->n_multinames = 0;
      return FALSE;
    }
    for (i = 1; i < pool->n_multinames; i++) {
      switch (swfdec_bits_get_u8 (bits)) {
	case 0x0D:
	  SWFDEC_FIXME ("implement attributes");
	case 0x07:
	  READ_U30 (nsid, bits);
	  if (nsid >= pool->n_namespaces)
	    THROW (pool, "Cpool index %u is out of range %u.", nsid, pool->n_namespaces);
	  READ_U30 (nameid, bits);
	  if (nameid >= pool->n_strings)
	    THROW (pool, "Cpool index %u is out of range %u.", nameid, pool->n_strings);
	  swfdec_abc_multiname_init (&pool->multinames[i],
	      nsid == 0 ? SWFDEC_ABC_MULTINAME_ANY : pool->namespaces[nsid],
	      nameid == 0 ? SWFDEC_ABC_MULTINAME_ANY : pool->strings[nameid]);
	  break;
	case 0x10:
	  SWFDEC_FIXME ("implement attributes");
	case 0x0F:
	  READ_U30 (nameid, bits);
	  if (nameid >= pool->n_strings)
	    THROW (pool, "Cpool index %u is out of range %u.", nameid, pool->n_strings);
	  swfdec_abc_multiname_init (&pool->multinames[i],
	      NULL,
	      nameid == 0 ? SWFDEC_ABC_MULTINAME_ANY : pool->strings[nameid]);
	  break;
	case 0x12:
	  SWFDEC_FIXME ("implement attributes");
	case 0x11:
	  swfdec_abc_multiname_init (&pool->multinames[i],
	      NULL, NULL);
	  break;
	case 0x0E:
	  SWFDEC_FIXME ("implement attributes");
	case 0x09:
	  READ_U30 (nameid, bits);
	  if (nameid >= pool->n_strings)
	    THROW (pool, "Cpool index %u is out of range %u.", nameid, pool->n_strings);
	  READ_U30 (nsid, bits);
	  if (nsid >= pool->n_nssets)
	    THROW (pool, "Cpool index %u is out of range %u.", nsid, pool->n_nssets);
	  swfdec_abc_multiname_init_set (&pool->multinames[i], pool->nssets[nsid],
	      nameid == 0 ? SWFDEC_ABC_MULTINAME_ANY : pool->strings[nameid]);
	  break;
	case 0x1C:
	  SWFDEC_FIXME ("implement attributes");
	case 0x1B:
	  READ_U30 (nsid, bits);
	  if (nsid >= pool->n_nssets)
	    THROW (pool, "Cpool index %u is out of range %u.", nsid, pool->n_nssets);
	  swfdec_abc_multiname_init_set (&pool->multinames[i], pool->nssets[nsid], NULL);
	  break;
	default:
	  THROW (pool, "Cpool entry %u is wrong type.", i);
      }
      SWFDEC_LOG ("  multiname %u: %s::%s", i, pool->multinames[i].ns == NULL ? 
	  (pool->multinames[i].nsset ? "[SET]" : "[RUNTIME]") : 
	  pool->multinames[i].ns == SWFDEC_ABC_MULTINAME_ANY ? "[*]" : pool->multinames[i].ns->uri,
	  pool->multinames[i].name == NULL ? "[RUNTIME]" : 
	  pool->multinames[i].name == SWFDEC_ABC_MULTINAME_ANY ? "[*]" : pool->multinames[i].name);
    }
  }

  return TRUE;
}

static gboolean
swfdec_abc_pool_parse_method (SwfdecAbcPool *pool, SwfdecBits *bits, SwfdecAbcTraits *traits, SwfdecAbcFunction **ret)
{
  guint id;

  READ_U30 (id, bits);
  if (id >= pool->n_functions) {
    THROW (pool, "Method_info %u exceeds method_count=%u.", id, pool->n_functions);
  } else if (pool->functions[id] == NULL) {
    THROW (pool, "MethodInfo-%u referenced before definition.", id);
  } else if (traits) {
    if (!swfdec_abc_function_bind (pool->functions[id], traits)) {
      THROW (pool, "Function %s has already been bound to %s.", pool->functions[id]->name,
	  pool->functions[id]->bound_traits->name);
    }
    g_assert (traits->construct == NULL);
    traits->construct = pool->functions[id];
    SWFDEC_LOG ("binding method %u to traits %s", id, traits->name);
  } else {
    SWFDEC_LOG ("found method %u", id);
  }
  if (ret)
    *ret = pool->functions[id];
  return TRUE;
}

static gboolean
swfdec_abc_pool_parse_traits (SwfdecAbcPool *pool, SwfdecAbcTraits *traits, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (pool);
  SwfdecAbcTraits *base = traits->base;
  guint i, n_slots, n_methods, n_traits;
  gboolean early, metadata;

  READ_U30 (n_traits, bits);
  SWFDEC_LOG ("%u traits", n_traits);

  /* ensure we can copy protected traits from base class into new protected namespace */
  traits->n_traits = n_traits;
  if (base && base->protected_ns && traits->protected_ns) {
    for (i = 0; i < base->n_traits; i++) {
      if (base->traits[i].ns == base->protected_ns)
	traits->n_traits++;
    }
  }

  if (traits->n_traits) {
    traits->traits = swfdec_as_context_try_new (context, SwfdecAbcTrait, traits->n_traits);
    if (traits->traits == NULL) {
      traits->n_traits = 0;
      return FALSE;
    }
  }
  if (base && base->protected_ns && traits->protected_ns) {
    traits->n_traits = n_traits;
    for (i = 0; i < base->n_traits; i++) {
      if (base->traits[i].ns == base->protected_ns) {
	traits->traits[traits->n_traits] = base->traits[i];
	traits->traits[traits->n_traits].ns = traits->protected_ns;
	traits->n_traits++;
      }
    }
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
    if (!swfdec_abc_pool_parse_qname (pool, bits, &trait->ns, &trait->name))
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
	      THROW (pool, "The ABC data is corrupt, attempt to read out of bounds.");
	    if (slot > n_slots)
	      n_slots = slot;
	    slot--;
	    if (base && slot < base->n_slots)
	      THROW (pool, "Illegal override of %s in %s.", trait->name, traits->name);
	    SWFDEC_LOG ("  slot: %u", slot);
	  }

	  if (swfdec_abc_traits_get_trait (traits, trait->ns, trait->name))
	    THROW (pool, "The ABC data is corrupt, attempt to read out of bounds.");
				
	  if (type == 4) {
	    guint id;
	    READ_U30 (id, bits);
	    if (id >= pool->n_classes)
	      THROW (pool, "ClassInfo %u exceeds class_count=%u.", id, pool->n_functions);
	    if (pool->classes[id] == NULL)
	      THROW (pool, "ClassInfo-%u is referenced before definition.", id);

	    trait->type = SWFDEC_ABC_BINDING_NEW (SWFDEC_ABC_TRAIT_CONST, slot);
	    trait->default_index = id;
	    trait->default_type = G_MAXUINT; /* magic value */
	  } else {
	    g_assert (type == 0 || type == 6);
	    READ_U30 (trait->traits_type, bits);
	    READ_U30 (trait->default_index, bits);
	    if (trait->default_index) {
	      trait->default_type = swfdec_bits_get_u8 (bits);
	    }
	    trait->type = SWFDEC_ABC_BINDING_NEW (type == 0 ? SWFDEC_ABC_TRAIT_SLOT : SWFDEC_ABC_TRAIT_CONST, slot);
	  }
	}
	break;
      case 1: /* method */
      case 2: /* getter */
      case 3: /* setter */
	{
	  guint ignore, method_id;
	  SwfdecAbcBinding bind;
	  const SwfdecAbcTrait *found;
	  SwfdecAbcFunction *fun;
	  READ_U30 (ignore, bits);
	  SWFDEC_LOG ("  display id = %u (ignored)", ignore);
	  READ_U30 (method_id, bits);
	  if (method_id >= pool->n_functions) {
	    THROW (pool, "Method_info %u exceeds method_count=%u.", method_id, pool->n_functions);
	  } else if (pool->functions[method_id] == NULL) {
	    THROW (pool, "MethodInfo-%u referenced before definition.", method_id);
	  } else {
	    fun = pool->functions[method_id];
	  }
	  if (!swfdec_abc_function_bind (fun, traits)) {
	    THROW (pool, "Function %s has already been bound to %s.", traits->name,
		fun->bound_traits->name);
	  }

	  if (traits->base && (found = swfdec_abc_traits_find_trait (traits->base,
	      trait->ns == traits->protected_ns ? traits->base->protected_ns: trait->ns, 
	      trait->name))) {
	    bind = found->type;
	    SWFDEC_LOG ("  found method %u of type %u to override", 
		SWFDEC_ABC_BINDING_GET_ID (bind), SWFDEC_ABC_BINDING_GET_TYPE (bind));
	    trait->default_index = found->default_index;
	    trait->default_type = found->default_type;
	  } else {
	    bind = SWFDEC_ABC_BINDING_NONE;
	  }

	  if (type == 1) { 
	    /* method */
	    if (bind == SWFDEC_ABC_BINDING_NONE) {
	      if (trait->override)
		THROW (pool, "Illegal override of %s in %s.", trait->name, traits->name);
	      trait->type = SWFDEC_ABC_BINDING_NEW (SWFDEC_ABC_TRAIT_METHOD, n_methods);
	      SWFDEC_LOG ("  method: %u (new)", n_methods);
	      n_methods++;
	    } else if (SWFDEC_ABC_BINDING_IS_TYPE (bind, SWFDEC_ABC_TRAIT_METHOD)) {
	      if (!trait->override)
		THROW (pool, "Illegal override of %s in %s.", trait->name, traits->name);
	      trait->type = SWFDEC_ABC_BINDING_NEW (SWFDEC_ABC_TRAIT_METHOD, 
		  SWFDEC_ABC_BINDING_GET_ID (bind));
	      SWFDEC_LOG ("  method: %u (override)", SWFDEC_ABC_BINDING_GET_ID (bind));
	    } else {
	      THROW (pool, "The ABC data is corrupt, attempt to read out of bounds.");
	    }
	    trait->default_index = method_id;
	  } else {
	    SwfdecAbcTraitType ttype = type == 2 ? SWFDEC_ABC_TRAIT_GET : SWFDEC_ABC_TRAIT_SET;
	    /* getter or setter */

	    /* check override */
	    if (bind == SWFDEC_ABC_BINDING_NONE) {
	      if (trait->override)
		THROW (pool, "Illegal override of %s in %s.", trait->name, traits->name);
	    } else if (SWFDEC_ABC_BINDING_IS_ACCESSOR (bind)) {
	      if ((trait->override ? TRUE : FALSE) != ((SWFDEC_ABC_BINDING_GET_TYPE (bind) & ttype) == ttype))
		THROW (pool, "Illegal override of %s in %s.", trait->name, traits->name);
	      SWFDEC_LOG ("  method: %u (override)", SWFDEC_ABC_BINDING_GET_ID (bind));
	    } else {
	      THROW (pool, "The ABC data is corrupt, attempt to read out of bounds.");
	    }

	    /* try to find own trait */
	    found = swfdec_abc_traits_get_trait (traits, trait->ns, trait->name);
	    if (found) {
	      /* FIXME: This is kinda (read: very) hacky */
	      trait = (SwfdecAbcTrait *) found;
	      trait->type = SWFDEC_ABC_BINDING_NEW (SWFDEC_ABC_TRAIT_GETSET, 
		  SWFDEC_ABC_BINDING_GET_ID (trait->type));
	      SWFDEC_LOG ("  method: %u (reuse)", SWFDEC_ABC_BINDING_GET_ID (trait->type));
	    } else if (bind == SWFDEC_ABC_BINDING_NONE) {
	      trait->type = SWFDEC_ABC_BINDING_NEW (ttype, n_methods);
	      SWFDEC_LOG ("  method: %u (new)", n_methods);
	      n_methods += 2;
	    } else {
	      trait->type = SWFDEC_ABC_BINDING_NEW (SWFDEC_ABC_BINDING_GET_TYPE (bind) | ttype,
		  SWFDEC_ABC_BINDING_GET_ID (bind));
	    }
	    if (type == 2) {
	      trait->default_index = method_id;
	    } else {
	      trait->default_type = method_id;
	    }
	  }
	}
        break;
      default:
	THROW (pool, "Unsupported traits kind=%u.", type);
    }

    if (metadata) {
      guint ignore, j, count;
      READ_U30 (count, bits);
  
      SWFDEC_INFO ("parse metadata, in particular \"NeedsDxns\"");
      for (j = 0; j < count; j++) {
	READ_U30 (ignore, bits);
      }
    }
  }

  traits->n_slots = n_slots;
  traits->n_methods = n_methods;

  return TRUE;
}

static gboolean
swfdec_abc_pool_parse_methods (SwfdecAbcPool *pool, SwfdecBits *bits,
    const GCallback *natives, guint n_natives)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (pool);
  guint i;

  READ_U30 (pool->n_functions, bits);
  SWFDEC_LOG ("%u methods", pool->n_functions);
  if (pool->n_functions) {
    gboolean param_names, optional, native;
    pool->functions = swfdec_as_context_try_new (context, SwfdecAbcFunction *, pool->n_functions);
    if (pool->functions == NULL) {
      pool->n_functions = 0;
      return FALSE;
    }
    for (i = 0; i < pool->n_functions; i++) {
      guint id, j, len;
      SwfdecAbcFunction *fun = pool->functions[i] = 
	g_object_new (SWFDEC_TYPE_ABC_FUNCTION, "context", context, NULL);
      fun->pool = pool;
      READ_U30 (len, bits);
      SWFDEC_LOG ("  function %u:", i);
      fun->args = swfdec_as_context_try_new (context, SwfdecAbcFunctionArgument, len + 1);
      if (fun->args == NULL)
	return FALSE;
      fun->n_args = len;
      fun->min_args = len;
      READ_U30 (id, bits);
      if (id == 0) {
	fun->return_type = NULL;
      } else if (id < pool->n_multinames) {
	fun->return_type = &pool->multinames[id];
      } else {
	THROW (pool, "Cpool index %u is out of range %u.", id, pool->n_multinames);
      }
      SWFDEC_LOG ("    return type: %s", fun->return_type ? ((SwfdecAbcMultiname *) fun->return_type)->name : "void");
      for (j = 1; j <= fun->n_args; j++) {
	READ_U30 (id, bits);
	if (id == 0) {
	  fun->args[j].type = NULL;
	} else if (id < pool->n_multinames) {
	  fun->args[j].type = &pool->multinames[id];
	} else {
	  THROW (pool, "Cpool index %u is out of range %u.", id, pool->n_multinames);
	}
	SWFDEC_LOG ("    argument %u: type %s", j, fun->args[j].type ? ((SwfdecAbcMultiname *) fun->args[j].type)->name : "void");
      }
      READ_U30 (id, bits);
      if (id > pool->n_strings) {
	THROW (pool, "Cpool index %u is out of range %u.", id, pool->n_strings);
      } else if (id > 0) {
	fun->name = pool->strings[id];
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

	if (i >= n_natives) {
	  SWFDEC_ERROR ("only %u native methods specified, but wanting index %u", n_natives, i);
	} else {
	  fun->native = natives[i];
	}
	if (fun->native == NULL) {
	  SWFDEC_INFO ("no native for index %u", i);
	}
      }
      if (optional) {
	READ_U30 (len, bits);
	if (len == 0 || len > fun->n_args)
	  return FALSE;
	fun->min_args -= len;
	for (j = fun->min_args + 1; j <= fun->n_args; j++) {
	  READ_U30 (id, bits);
	  fun->args[j].default_index = id;
	  fun->args[j].default_type = swfdec_bits_get_u8 (bits);
	}
      }
      if (param_names) {
	/* Tamarin doesn't parse this, so we must not error out here */
	for (j = 1; j <= fun->n_args; j++) {
	  id = swfdec_bits_get_vu32 (bits);
	  if (id > 0 && id < pool->n_strings)
	    fun->args[j].name = pool->strings[id];
	}
      }
    }
  }

  return TRUE;
}

static gboolean
swfdec_abc_pool_skip_metadata (SwfdecAbcPool *pool, SwfdecBits *bits)
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
swfdec_abc_pool_parse_instance (SwfdecAbcPool *pool, guint instance_id, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (pool);
  gboolean protected_ns;
  SwfdecAbcTraits *traits;
  guint id, i, n;

  SWFDEC_LOG ("instance: %u", instance_id);
  traits = g_object_new (SWFDEC_TYPE_ABC_TRAITS, "context", context, NULL);
  traits->pool = pool;
  if (!swfdec_abc_pool_parse_qname (pool, bits, &traits->ns, &traits->name))
    return FALSE;
  SWFDEC_LOG ("  name: %s::%s", traits->ns->uri, traits->name);
  
  READ_U30 (id, bits);
  /* id == 0 means no base traits */
  if (id >= pool->n_multinames) {
    THROW (pool, "Cpool index %u is out of range %u.", id, pool->n_multinames);
  } else if (id > 0) {
    SwfdecAbcTraits *base = swfdec_abc_global_get_traits_for_multiname (pool->global,
	&pool->multinames[id]);
    if (base == NULL)
      THROW (pool, "Class %s could not be found.", pool->multinames[id].name);
    if (base->final || 
	(pool->global->pool != NULL && 
	 (base == SWFDEC_ABC_CLASS_TRAITS (context) || base == SWFDEC_ABC_FUNCTION_TRAITS (context)))) {
      THROW (pool, "Class %s cannot extend final base class.", pool->multinames[id].name);
    }
    if (base->interface || traits->interface) {
      THROW (pool, "Class %s cannot extend %s.", pool->multinames[id].name, base->name);
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
    } else if (id < pool->n_namespaces) {
      traits->protected_ns = pool->namespaces[id];
    } else if (id != 0) {
      THROW (pool, "Cpool index %u is out of range %u.", id, pool->n_multinames);
    }
  }
  READ_U30 (n, bits);
  if (n > 0) {
    SWFDEC_FIXME ("implement interface parsing");
    for (i = 0; i < n; i++) {
      READ_U30 (id, bits);
    }
  } 
  if (!swfdec_abc_pool_parse_method (pool, bits, traits, NULL))
    return FALSE;

  if (!swfdec_abc_pool_parse_traits (pool, traits, bits))
    return FALSE;
  swfdec_abc_global_add_traits (pool->global, traits);
  pool->instances[instance_id] = traits;

  return TRUE;
}

static gboolean
swfdec_abc_pool_parse_instances (SwfdecAbcPool *pool, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (pool);
  guint i;

  READ_U30 (pool->n_classes, bits);
  SWFDEC_LOG ("%u classes", pool->n_classes);
  if (pool->n_classes) {
    pool->instances = swfdec_as_context_try_new (context, SwfdecAbcTraits *, pool->n_classes);
    pool->classes = swfdec_as_context_try_new (context, SwfdecAbcTraits *, pool->n_classes);
    if (pool->instances == NULL || pool->classes == NULL) {
      if (pool->instances) {
	swfdec_as_context_free (context, sizeof (SwfdecAbcFunction *) * pool->n_classes, pool->instances);
	pool->instances = NULL;
      }
      if (pool->classes) {
	swfdec_as_context_free (context, sizeof (SwfdecAbcFunction *) * pool->n_classes, pool->classes);
	pool->classes = NULL;
      }
      pool->n_classes = 0;
      return FALSE;
    }
    for (i = 0; i < pool->n_classes; i++) {
      if (!swfdec_abc_pool_parse_instance (pool, i, bits))
	return FALSE;
    }
  }
  return TRUE;
}

static gboolean
swfdec_abc_pool_parse_classes (SwfdecAbcPool *pool, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (pool);
  guint i;

  if (pool->n_classes) {
    for (i = 0; i < pool->n_classes; i++) {
      SwfdecAbcTraits *traits;

      SWFDEC_LOG ("class %u", i);
      traits = g_object_new (SWFDEC_TYPE_ABC_TRAITS, "context", context, NULL);
      traits->pool = pool;

      traits->ns = pool->instances[i]->ns;
      traits->name = swfdec_as_context_give_string (context,
	  g_strconcat (pool->instances[i]->name, "$", NULL));
      traits->final = TRUE;
      traits->sealed = FALSE;
      if (SWFDEC_ABC_GLOBAL (context->global)->pool)
	traits->base = SWFDEC_ABC_CLASS_TRAITS (context);
      else
	traits->base = pool->instances[SWFDEC_ABC_TYPE_CLASS];
      g_assert (traits->base);

      if (!swfdec_abc_pool_parse_method (pool, bits, traits, NULL))
	return FALSE;

      if (!swfdec_abc_pool_parse_traits (pool, traits, bits))
	return FALSE;

      traits->instance_traits = pool->instances[i];
      pool->classes[i] = traits;
    }
  }
  return TRUE;
}

static gboolean
swfdec_abc_pool_parse_scripts (SwfdecAbcPool *pool, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (pool);
  SwfdecAbcScript *script = NULL;
  SwfdecAbcTraits *traits;
  SwfdecAbcFunction *fun;
  guint i, j, n_scripts;

  READ_U30 (n_scripts, bits);
  SWFDEC_LOG ("%u scripts ", n_scripts);
  for (i = 0; i < n_scripts; i++) {
    SWFDEC_LOG ("script %u", i);
    traits = g_object_new (SWFDEC_TYPE_ABC_TRAITS, "context", context, NULL);
    traits->pool = pool;
    traits->final = TRUE;
    traits->ns = context->public_ns;
    traits->name = SWFDEC_AS_STR_global;
    if (SWFDEC_ABC_GLOBAL (context->global)->pool)
      traits->base = SWFDEC_ABC_OBJECT_TRAITS (context);
    else
      traits->base = pool->instances[SWFDEC_ABC_TYPE_OBJECT];
    if (!swfdec_abc_pool_parse_method (pool, bits, traits, &fun))
      return FALSE;
    traits->construct = fun;
    if (!swfdec_abc_pool_parse_traits (pool, traits, bits))
      return FALSE;
    script = g_object_new (SWFDEC_TYPE_ABC_SCRIPT, "context", context, NULL);
    script->traits = traits;
    for (j = 0; j < traits->n_traits; j++) {
      SwfdecAbcTrait *trait = &traits->traits[j];
      if (trait->type == SWFDEC_ABC_TRAIT_NONE)
	continue;
      swfdec_abc_global_add_script (pool->global, trait->ns, trait->name, script, 
	  i + 1 == n_scripts);
    }
  }
  if (script == 0)
    THROW (pool, "No entry point was found.");
  pool->main = script;
  return TRUE;
}

static gboolean
swfdec_abc_pool_parse_bodies (SwfdecAbcPool *pool, SwfdecBits *bits)
{
  SwfdecAsContext *context = swfdec_gc_object_get_context (pool);
  SwfdecAbcTraits *traits;
  SwfdecAbcFunction *fun;
  guint i, j, max_scope, min_scope, n_bodies, size;

  READ_U30 (n_bodies, bits);
  SWFDEC_LOG ("%u bodies", n_bodies);
  for (i = 0; i < n_bodies; i++) {
    SWFDEC_LOG ("body %u", i);
    if (!swfdec_abc_pool_parse_method (pool, bits, NULL, &fun))
      return FALSE;
    if (swfdec_abc_function_is_native (fun))
      THROW (pool, "Native method %"G_GSIZE_FORMAT" has illegal method body.", fun - pool->functions[0]);
    if (fun->bound_traits && fun->bound_traits->interface)
      THROW (pool, "Interface method %"G_GSIZE_FORMAT" has illegal method body.", fun - pool->functions[0]);
    if (fun->code != NULL)
      THROW (pool, "Method %"G_GSIZE_FORMAT" has a duplicate method body.", fun - pool->functions[0]);

    READ_U30 (fun->n_stack, bits);
    SWFDEC_LOG ("  stack: %u", fun->n_stack);
    READ_U30 (fun->n_locals, bits);
    if (fun->n_args >= fun->n_locals)
      THROW (pool, "The ABC data is corrupt, attempt to read out of bounds.");
    SWFDEC_LOG ("  locals: %u", fun->n_locals);
    READ_U30 (min_scope, bits);
    READ_U30 (max_scope, bits);
    if (min_scope > max_scope)
      THROW (pool, "The ABC data is corrupt, attempt to read out of bounds.");
    fun->n_scope = max_scope - min_scope;
    SWFDEC_LOG ("  scope: %u (%u - %u)", fun->n_scope, min_scope, max_scope);
    READ_U30 (size, bits);
    if (size == 0)
      THROW (pool, "Invalid code_length=0.");
    SWFDEC_LOG ("  code: %u bytes", size);
    fun->code = swfdec_bits_get_buffer (bits, size);
    if (fun->code == NULL)
      THROW (pool, "The ABC data is corrupt, attempt to read out of bounds.");

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
    traits->pool = pool;
    traits->final = TRUE;
    traits->ns = context->public_ns;
    traits->name = SWFDEC_AS_STR_EMPTY;
    if (SWFDEC_ABC_GLOBAL (context->global)->pool)
      traits->base = SWFDEC_ABC_OBJECT_TRAITS (context);
    else
      traits->base = pool->instances[SWFDEC_ABC_TYPE_OBJECT];
    if (!swfdec_abc_pool_parse_traits (pool, traits, bits))
      return FALSE;
    fun->activation = traits;
  }
  return TRUE;
}

static gboolean
swfdec_abc_pool_parse (SwfdecAbcPool *pool, SwfdecBits *bits,
    const GCallback *natives, guint n_natives)
{
  return swfdec_abc_pool_parse_constants (pool, bits) &&
      swfdec_abc_pool_parse_methods (pool, bits, natives, n_natives) &&
      swfdec_abc_pool_skip_metadata (pool, bits) &&
      swfdec_abc_pool_parse_instances (pool, bits) &&
      swfdec_abc_pool_parse_classes (pool, bits) &&
      swfdec_abc_pool_parse_scripts (pool, bits) &&
      swfdec_abc_pool_parse_bodies (pool, bits);
}

SwfdecAbcPool *
swfdec_abc_pool_new (SwfdecAsContext *context, SwfdecBits *bits)
{
  return swfdec_abc_pool_new_trusted (context, bits, NULL, 0);
}

SwfdecAbcPool *
swfdec_abc_pool_new_trusted (SwfdecAsContext *context, SwfdecBits *bits,
    const GCallback *natives, guint n_natives)
{
  SwfdecAbcPool *pool;
  guint major, minor;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), NULL);
  g_return_val_if_fail (SWFDEC_IS_ABC_GLOBAL (context->global), NULL);
  g_return_val_if_fail (bits != NULL, NULL);

  pool = g_object_new (SWFDEC_TYPE_ABC_POOL, "context", context, NULL);
  pool->global = SWFDEC_ABC_GLOBAL (context->global);

  minor = swfdec_bits_get_u16 (bits);
  major = swfdec_bits_get_u16 (bits);
  SWFDEC_LOG ("  major version: %u", major);
  SWFDEC_LOG ("  minor version: %u", minor);
  if (major == 46 && minor == 16) {
    if (!swfdec_abc_pool_parse (pool, bits, natives, n_natives)) {
      swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_VERIFY_ERROR,
	  "The ABC data is corrupt, attempt to read out of bounds.");
      return NULL;
    }
  } else {
    swfdec_as_context_throw_abc (context, SWFDEC_ABC_TYPE_VERIFY_ERROR,
	"Not an ABC pool.  major_version=%u minor_version=%u.", major, minor);
    return NULL;
  }
  
//#define SWFDEC_PRINT_STUBS
#ifdef SWFDEC_PRINT_STUBS
  {
    guint i;

    if (SWFDEC_ABC_GLOBAL (context->global)->pool == NULL)
      SWFDEC_ABC_GLOBAL (context->global)->pool = pool;
    
    /* update machine types */
    SWFDEC_ABC_BOOLEAN_TRAITS (context)->machine_type = SWFDEC_ABC_INT;
    SWFDEC_ABC_INT_TRAITS (context)->machine_type = SWFDEC_ABC_INT;
    SWFDEC_ABC_UINT_TRAITS (context)->machine_type = SWFDEC_ABC_UINT;
    SWFDEC_ABC_NUMBER_TRAITS (context)->machine_type = SWFDEC_ABC_DOUBLE;
    SWFDEC_ABC_STRING_TRAITS (context)->machine_type = SWFDEC_ABC_STRING;

    g_print ("missing implementations:\n");
    for (i = 0; i < pool->n_functions; i++) {
      SwfdecAbcFunction *fun = pool->functions[i];
      char *name;
      if (fun->code != NULL)
	continue;
      /* This is why we can't print this stuff by default: We need to resolve
       * the function, which could throw */
      if (!swfdec_abc_function_resolve (fun) ||
	  (fun->bound_traits != NULL && !swfdec_abc_traits_resolve (fun->bound_traits))) {
	g_assert_not_reached ();
      }
      name = swfdec_abc_function_describe (fun);
      g_print ("%s %s\n", fun->native == NULL ? "XXX" : "   ", name);
      g_free (name);
      }
  }
#endif

  return pool;
}

gboolean
swfdec_abc_pool_get_constant (SwfdecAbcPool *pool, SwfdecAsValue *value, 
    guint type, guint id)
{
  g_return_val_if_fail (SWFDEC_IS_ABC_POOL (pool), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  switch (type) {
    case SWFDEC_ABC_CONST_UNDEFINED:
      SWFDEC_AS_VALUE_SET_UNDEFINED (value);
      break;
    case SWFDEC_ABC_CONST_STRING:
      if (id >= pool->n_strings)
	THROW (pool, "Cpool index %u is out of range %u.", id, pool->n_strings);
      SWFDEC_AS_VALUE_SET_STRING (value, pool->strings[id]);
      break;
    case SWFDEC_ABC_CONST_INT:
      if (id >= pool->n_ints)
	THROW (pool, "Cpool index %u is out of range %u.", id, pool->n_ints);
      SWFDEC_AS_VALUE_SET_INT (value, pool->ints[id]);
      break;
    case SWFDEC_ABC_CONST_UINT:
      if (id >= pool->n_uints)
	THROW (pool, "Cpool index %u is out of range %u.", id, pool->n_uints);
      SWFDEC_AS_VALUE_SET_NUMBER (value, pool->uints[id]);
      break;
    case SWFDEC_ABC_CONST_DOUBLE:
      if (id >= pool->n_doubles)
	THROW (pool, "Cpool index %u is out of range %u.", id, pool->n_doubles);
      SWFDEC_AS_VALUE_SET_NUMBER (value, pool->doubles[id]);
      break;
    case SWFDEC_ABC_CONST_FALSE:
      SWFDEC_AS_VALUE_SET_BOOLEAN (value, FALSE);
      break;
    case SWFDEC_ABC_CONST_TRUE:
      SWFDEC_AS_VALUE_SET_BOOLEAN (value, TRUE);
      break;
    case SWFDEC_ABC_CONST_NULL:
      SWFDEC_AS_VALUE_SET_NULL (value);
      break;
    case SWFDEC_ABC_CONST_PRIVATE_NAMESPACE:
    case SWFDEC_ABC_CONST_NAMESPACE:
    case SWFDEC_ABC_CONST_PACKAGE_NAMESPACE:
    case SWFDEC_ABC_CONST_INTERNAL_NAMESPACE:
    case SWFDEC_ABC_CONST_PROTECTED_NAMESPACE:
    case SWFDEC_ABC_CONST_EXPLICIT_NAMESPACE:
    case SWFDEC_ABC_CONST_STATIC_PROTECTED_NAMESPACE:
      if (id >= pool->n_namespaces)
	THROW (pool, "Cpool index %u is out of range %u.", id, pool->n_namespaces);
      SWFDEC_AS_VALUE_SET_NAMESPACE (value, pool->namespaces[id]);
      break;
    default:
      THROW (pool, "Illegal default value for type %s.", "FIXME");
  }
  return TRUE;
}
