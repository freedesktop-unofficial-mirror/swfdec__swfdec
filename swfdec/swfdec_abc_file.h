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

#ifndef _SWFDEC_ABC_FILE_H_
#define _SWFDEC_ABC_FILE_H_

#include <swfdec/swfdec_abc_function.h>
#include <swfdec/swfdec_abc_global.h>
#include <swfdec/swfdec_abc_multiname.h>
#include <swfdec/swfdec_abc_namespace.h>
#include <swfdec/swfdec_abc_ns_set.h>
#include <swfdec/swfdec_abc_types.h>
#include <swfdec/swfdec_bits.h>
#include <swfdec/swfdec_gc_object.h>

G_BEGIN_DECLS

//typedef struct _SwfdecAbcFile SwfdecAbcFile;
typedef struct _SwfdecAbcFileClass SwfdecAbcFileClass;

#define SWFDEC_TYPE_ABC_FILE                    (swfdec_abc_file_get_type())
#define SWFDEC_IS_ABC_FILE(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_ABC_FILE))
#define SWFDEC_IS_ABC_FILE_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_ABC_FILE))
#define SWFDEC_ABC_FILE(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_ABC_FILE, SwfdecAbcFile))
#define SWFDEC_ABC_FILE_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_ABC_FILE, SwfdecAbcFileClass))
#define SWFDEC_ABC_FILE_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_ABC_FILE, SwfdecAbcFileClass))

struct _SwfdecAbcFile {
  SwfdecGcObject	object;

  SwfdecAbcGlobal *	global;		/* the global object we belong to */

  int *			ints;		/* all integer values in the file */
  guint			n_ints;		/* number of integers */
  guint *		uints;		/* all unsigned integer values in the file */
  guint			n_uints;	/* number of unsigned integers */
  double *		doubles;	/* all double values in the file */
  guint			n_doubles;	/* number of doubles */
  const char **		strings;	/* all string values in the file */
  guint			n_strings;	/* number of strings */
  SwfdecAbcNamespace **	namespaces;	/* all the namespaces in use */
  guint			n_namespaces;	/* number of namespaces */
  SwfdecAbcNsSet **	nssets;		/* all the namespace sets in use */
  guint			n_nssets;	/* number of namespace sets */
  SwfdecAbcMultiname *	multinames;   	/* all the multinames in use */
  guint			n_multinames;	/* number of multinames */
  SwfdecAbcFunction **	functions;   	/* all the functions defined */
  guint			n_functions;	/* number of functions */
  SwfdecAbcTraits **	instances;   	/* instance infos for the defined classes */
  SwfdecAbcTraits **	classes;   	/* all the classes defined */
  guint			n_classes;   	/* number of classes */
};

struct _SwfdecAbcFileClass {
  SwfdecGcObjectClass	object_class;
};

GType		swfdec_abc_file_get_type	(void);

SwfdecAbcFile *	swfdec_abc_file_new		(SwfdecAsContext *	context,
						 SwfdecBits *		bits);


G_END_DECLS
#endif
