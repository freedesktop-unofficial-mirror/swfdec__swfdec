#include <gtk/gtk.h>
#include <math.h>
#include <swfdec.h>
#include <swfdec_buffer.h>
#include <swfdec_widget.h>

static void
iterate (gpointer dec)
{
  swfdec_decoder_iterate (dec);
}

static void
view_swf (SwfdecDecoder *dec, double scale, gboolean use_image)
{
  GtkWidget *window, *widget, *vbox;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  vbox = gtk_vbox_new (FALSE, 3);
  widget = swfdec_widget_new (dec);
  swfdec_widget_set_scale (SWFDEC_WIDGET (widget), scale);
  swfdec_widget_set_use_image (SWFDEC_WIDGET (widget), use_image);
  gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);
  widget = gtk_button_new_with_mnemonic ("_Iterate");
  g_signal_connect_swapped (widget, "clicked", G_CALLBACK (iterate), dec);
  gtk_box_pack_end (GTK_BOX (vbox), widget, FALSE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (window), vbox);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show_all (window);

  gtk_main ();
}

int 
main (int argc, char *argv[])
{
  int ret = 100;
  double scale;
  SwfdecDecoder *s;
  SwfdecBuffer *buffer;
  GError *error = NULL;
  gboolean use_image = FALSE;

  GOptionEntry options[] = {
    { "scale", 's', 0, G_OPTION_ARG_INT, &ret, "scale factor", "PERCENT" },
    { "image", 'i', 0, G_OPTION_ARG_NONE, &use_image, "use an intermediate image surface for drawing", NULL },
    { NULL }
  };
  GOptionContext *ctx;

  ctx = g_option_context_new ("");
  g_option_context_add_main_entries (ctx, options, "options");
  g_option_context_add_group (ctx, gtk_get_option_group (TRUE));
  g_option_context_parse (ctx, &argc, &argv, &error);
  g_option_context_free (ctx);

  if (error) {
    g_printerr ("Error parsing command line arguments: %s\n", error->message);
    g_error_free (error);
    return 1;
  }

  scale = ret / 100.;
  swfdec_init ();

  if (argc < 2) {
    g_printerr ("Usage: %s [OPTIONS] filename\n", argv[0]);
    return 1;
  }

  buffer = swfdec_buffer_new_from_file (argv[1], &error);
  if (buffer == NULL) {
    g_printerr ("Couldn't open file \"%s\": %s\n", argv[1], error->message);
    return 1;
  }

  s = swfdec_decoder_new();
  ret = swfdec_decoder_add_buffer(s, buffer);

  while (ret != SWFDEC_EOF) {
    ret = swfdec_decoder_parse(s);
    if (ret == SWFDEC_NEEDBITS) {
      swfdec_decoder_eof(s);
    }
    if (ret == SWFDEC_ERROR) {
      g_print("error while parsing\n");
      return 1;
    }
  }

  view_swf (s, scale, use_image);

  s = NULL;
  return 0;
}

