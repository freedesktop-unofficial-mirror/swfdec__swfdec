noinst_PROGRAMS = swfdec-extract dump crashfinder rtmp-trace

crashfinder_CFLAGS = $(GLOBAL_CFLAGS) $(SWFDEC_CFLAGS) $(CAIRO_CFLAGS)
crashfinder_LDFLAGS = $(SWFDEC_LIBS) $(CAIRO_LIBS)
crashfinder_SOURCES = crashfinder.c

dump_CFLAGS = $(GLOBAL_CFLAGS) $(SWFDEC_CFLAGS) $(CAIRO_CFLAGS) $(PANGO_CFLAGS)
dump_LDFLAGS = $(SWFDEC_LIBS) $(CAIRO_LIBS) $(PANGO_LIBS)

swfdec_extract_CFLAGS = $(GLOBAL_CFLAGS) $(SWFDEC_CFLAGS) $(CAIRO_CFLAGS)
swfdec_extract_LDFLAGS = $(SWFDEC_LIBS) $(CAIRO_LIBS)

rtmp_trace_CFLAGS = $(GLOBAL_CFLAGS) $(SWFDEC_CFLAGS)
rtmp_trace_LDFLAGS = $(SWFDEC_LIBS)
