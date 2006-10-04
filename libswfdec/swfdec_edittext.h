
#ifndef _SWFDEC_EDIT_TEXT_H_
#define _SWFDEC_EDIT_TEXT_H_

#include "swfdec_types.h"
#include "color.h"
#include <swfdec_object.h>
#include <pango/pango.h>

G_BEGIN_DECLS
typedef struct _SwfdecEditText SwfdecEditText;
typedef struct _SwfdecEditTextClass SwfdecEditTextClass;

#define SWFDEC_TYPE_EDIT_TEXT                    (swfdec_edit_text_get_type())
#define SWFDEC_IS_EDIT_TEXT(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_EDIT_TEXT))
#define SWFDEC_IS_EDIT_TEXT_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_EDIT_TEXT))
#define SWFDEC_EDIT_TEXT(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_EDIT_TEXT, SwfdecEditText))
#define SWFDEC_EDIT_TEXT_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_EDIT_TEXT, SwfdecEditTextClass))

struct _SwfdecEditText
{
  SwfdecObject		object;

  /* text info */
  char *		text;		/* actual displayed text or NULL if none */
  gboolean		password;	/* if text is a passowrd and should be displayed as '*' */
  unsigned int		max_length;	/* maximum number of characters */

  /* layout info */
  SwfdecFont *	  	font;		/* font or NULL for default */
  gboolean		wrap;
  gboolean		multiline;
  PangoAlignment	align;
  gboolean		justify;
  unsigned int		indent;		/* first line indentation */
  int			spacing;	/* spacing between lines */
  /* visual info */
  swf_color		color;		/* text color */
  gboolean		selectable;
  gboolean		border;		/* draw a border around the text field */
  unsigned int		height;
  unsigned int		left_margin;
  unsigned int		right_margin;
  gboolean		autosize;	/* FIXME: implement */

  /* variable info */
  char *		variable;	/* name of the variable */
  gboolean		readonly;

};

struct _SwfdecEditTextClass
{
  SwfdecObjectClass	object_class;
};

GType		swfdec_edit_text_get_type	(void);

int		tag_func_define_edit_text	(SwfdecDecoder * s);

G_END_DECLS
#endif
