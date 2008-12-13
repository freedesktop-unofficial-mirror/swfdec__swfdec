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

#include <swfdec/swfdec.h>
#include <swfdec/swfdec_rtmp_header.h>
#include <swfdec/swfdec_rtmp_packet.h>

static char *
swfdec_buffer_to_string (SwfdecBuffer *b)
{
  GString *string;
  guint i;

  g_return_val_if_fail (b != NULL, NULL);

  string = g_string_new ("");
  for (i = 0; i < b->length; i++) {
    guchar c = (guchar) b->data[i];
    switch (c) {
      case '\"': 
      case '\'': 
      case '\\':
	g_string_append_c (string, '\\');
	g_string_append_c (string, (char) c);
	break;
      case '\f':
	g_string_append_c (string, '\\');
	g_string_append_c (string, 'f');
	break;
      case '\n':
	g_string_append_c (string, '\\');
	g_string_append_c (string, 'n');
	break;
      case '\r':
	g_string_append_c (string, '\\');
	g_string_append_c (string, 'r');
	break;
      case '\t':
	g_string_append_c (string, '\\');
	g_string_append_c (string, 't');
	break;
      case '\v':
	g_string_append_c (string, '\\');
	g_string_append_c (string, 'v');
	break;
      default:
	if (g_ascii_isprint(c)) {
	  g_string_append_c (string, (char) c);
	} else if (i < b->length - 1 && g_ascii_isdigit(b->data[i + 1])) {
	  g_string_append_printf (string, "\\%03o", (guint) c);
	} else {
	  g_string_append_printf (string, "\\%o",(guint) c);
	}
	break;
    }
  }

  return g_string_free (string, FALSE);
}

static SwfdecBuffer *
swfdec_buffer_from_string (const char *s)
{
  SwfdecBuffer *buffer;
  GString *string;

  g_return_val_if_fail (s != NULL, NULL);

  string = g_string_new ("");

  while (*s) {
    if (*s == '\\') {
      s++;
      switch (*s) {
	case '"': 
	  g_string_append_c (string, '"');
	  break;
	case '\'': 
	  g_string_append_c (string, '\'');
	  break;
	case '\\':
	  g_string_append_c (string, '\\');
	  break;
	case 'f':
	  g_string_append_c (string, '\f');
	  break;
	case 'n':
	  g_string_append_c (string, '\n');
	  break;
	case 'r':
	  g_string_append_c (string, '\r');
	  break;
	case 't':
	  g_string_append_c (string, '\t');
	  break;
	case 'v':
	  g_string_append_c (string, '\v');
	  break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	  {
	    guint val = s[0] - '0';
	    if (s[1] >= '0' && s[1] <= '7') {
	      s++;
	      val = 8 * val + s[0] - '0';
	      if (s[1] >= '0' && s[1] <= '7') {
		s++;
		val = 8 * val + s[0] - '0';
		if (val >= 256) {
		  g_printerr ("Invalid octal escape sequence");
		}
	      }
	    }
	    g_string_append_c (string, val);
	  }
	  break;
	default:
	  g_printerr ("Stray \\ in string");
	  break;
      }
    } else {
      g_string_append_c (string, *s);
    }
    s++;
  }
  
  buffer = swfdec_buffer_new_for_data ((guchar *) string->str, string->len);
  g_string_free (string, FALSE);
  return buffer;
}

typedef struct _RtmpConn {
  SwfdecBufferQueue *	queue;
  GHashTable *		pending;
  guint			packet_size;
} RtmpConn;

static RtmpConn *
rtmp_conn_new (void)
{
  RtmpConn *conn = g_slice_new (RtmpConn);

  conn->queue = swfdec_buffer_queue_new ();
  conn->pending = g_hash_table_new (g_direct_hash, g_direct_equal);
  conn->packet_size = 128;

  return conn;
}

static void
rtmp_conn_free (RtmpConn *conn)
{
  swfdec_buffer_queue_unref (conn->queue);
  g_hash_table_destroy (conn->pending);
  g_slice_free (RtmpConn, conn);
}

typedef struct _RtmpState {
  guint			handshake;
  RtmpConn *		send;
  RtmpConn *		recv;
} RtmpState;

static RtmpState *
rtmp_state_new (void)
{
  RtmpState *state = g_slice_new0 (RtmpState);

  state->handshake = 3;
  state->send = rtmp_conn_new ();
  state->recv = rtmp_conn_new ();

  return state;
}

static void
rtmp_state_free (RtmpState *state)
{
  rtmp_conn_free (state->recv);
  g_slice_free (RtmpState, state);
}

static void
write_line (GPtrArray *lines, const char *type, SwfdecBuffer *buffer)
{
  char *s;

  s = swfdec_buffer_to_string (buffer);
  g_ptr_array_add (lines, g_strconcat (type, " ", s, NULL));
  g_free (s);
}

static gboolean
process_one_packet (GPtrArray *lines, const char *type, RtmpConn *conn)
{
  SwfdecRtmpPacket *packet;
  SwfdecRtmpHeader header = { 0, };
  SwfdecBuffer *buffer;
  SwfdecBits bits;
  gsize header_size, i, remaining;

  /* determine size of header */
  buffer = swfdec_buffer_queue_peek (conn->queue, 1);
  if (buffer == NULL)
    return FALSE;
  header_size = swfdec_rtmp_header_peek_size (buffer->data[0]);
  swfdec_buffer_unref (buffer);

  /* read header */
  buffer = swfdec_buffer_queue_peek (conn->queue, header_size);
  if (buffer == NULL)
    return FALSE;
  swfdec_bits_init (&bits, buffer);
  i = swfdec_rtmp_header_peek_channel (&bits);
  packet = g_hash_table_lookup (conn->pending, GUINT_TO_POINTER (i));
  if (packet == NULL) {
    packet = swfdec_rtmp_packet_new_empty ();
    g_hash_table_insert (conn->pending, GUINT_TO_POINTER (i), packet);
  } else {
    swfdec_rtmp_header_copy (&header, &packet->header);
  }
  swfdec_rtmp_header_read (&header, &bits);
  swfdec_buffer_unref (buffer);

  /* read the data chunk */
  remaining = GPOINTER_TO_SIZE (packet->buffer);
  if (remaining) {
    if (header_size >= 4) {
      g_printerr ("not a continuation header, but old command not finished yet, dropping old command\n");
      remaining = header.size;
    }
  } else {
    remaining = header.size;
  }
  if (header_size + MIN (conn->packet_size, remaining) > swfdec_buffer_queue_get_depth (conn->queue))
    return FALSE;

  swfdec_rtmp_header_copy (&packet->header, &header);
  if (remaining <= conn->packet_size) {
    packet->buffer = GSIZE_TO_POINTER (0);
  } else {
    packet->buffer = GSIZE_TO_POINTER (remaining - conn->packet_size);
    remaining = conn->packet_size;
  }

  buffer = swfdec_buffer_queue_pull (conn->queue, header_size + remaining);
  g_assert (buffer);
  write_line (lines, type, buffer);
  return TRUE;
}

static void
rtmp_process_send (RtmpState *state, GPtrArray *lines, SwfdecBuffer *buffer)
{
  swfdec_buffer_queue_push (state->send->queue, buffer);

  switch (state->handshake) {
    case 0:
      break;
    case 1:
      buffer = swfdec_buffer_queue_pull (state->send->queue, 1536);
      if (buffer == NULL)
	return;
      write_line (lines, "send", buffer);
      state->handshake--;
      break;
    case 2:
      g_printerr ("sent data when waiting for RTMP handshake reply?");
      return;
    case 3:
      buffer = swfdec_buffer_queue_pull (state->send->queue, 1537);
      if (buffer) {
	write_line (lines, "send", buffer);
	state->handshake--;
      }
      return;
    default:
      g_assert_not_reached ();
      break;
  }

  while (process_one_packet (lines, "send", state->send));
}

static void
rtmp_process_recv (RtmpState *state, GPtrArray *lines, SwfdecBuffer *buffer)
{
  swfdec_buffer_queue_push (state->recv->queue, buffer);

  switch (state->handshake) {
    case 0:
      break;
    case 1:
      g_printerr ("received data when waiting for RTMP handshake data?");
      return;
    case 2:
      buffer = swfdec_buffer_queue_pull (state->recv->queue, 1536 * 2 + 1);
      if (buffer) {
	write_line (lines, "recv", buffer);
	state->handshake--;
      }
      return;
    case 3:
      g_printerr ("received data when before sending?");
      return;
    default:
      g_assert_not_reached ();
      break;
  }

  while (process_one_packet (lines, "recv", state->recv));
}

static char **
rtmp_process (char **lines)
{
  GPtrArray *result;
  RtmpState *state;
  guint i;

  result = g_ptr_array_new ();
  state = rtmp_state_new ();
  for (i = 0; lines[i] != NULL; i++) {
    if (g_str_has_prefix (lines[i], "send ")) {
      SwfdecBuffer *buffer = swfdec_buffer_from_string (lines[i] + 5);
      rtmp_process_send (state, result, buffer);
    } else if (g_str_has_prefix (lines[i], "recv ")) {
      SwfdecBuffer *buffer = swfdec_buffer_from_string (lines[i] + 5);
      rtmp_process_recv (state, result, buffer);
    } else if (lines[i][0]) {
      g_printerr ("ignoring line %u: %s\n", i + 1, lines[i]);
    }
  }
  g_strfreev (lines);
  g_ptr_array_add (result, g_strdup ("")); /* so we end with a newline */
  g_ptr_array_add (result, NULL);
  rtmp_state_free (state);

  return (char **) g_ptr_array_free (result, FALSE);
}

int
main (int argc, char **argv)
{
  GError *error = NULL;
  char *contents;
  char **lines;
  GOptionEntry options[] = {
    { NULL }
  };
  GOptionContext *ctx;

  ctx = g_option_context_new ("");
  g_option_context_add_main_entries (ctx, options, "options");
  g_option_context_parse (ctx, &argc, &argv, &error);
  g_option_context_free (ctx);

  if (error) {
    g_printerr ("Error parsing command line arguments: %s\n", error->message);
    g_error_free (error);
    return 1;
  }
  if (argc < 2) {
    g_printerr ("Usage: %s [OPTIONS] infile [outfile]\n", argv[0]);
    return 1;
  }
  if (!g_file_get_contents (argv[1], &contents, NULL, &error)) {
    g_printerr ("Error opening file: %s\n", error->message);
    g_error_free (error);
    return 1;
  }

  lines = g_strsplit (contents, "\n", -1);
  g_free (contents);


  lines = rtmp_process (lines);


  contents = g_strjoinv ("\n", lines);
  g_strfreev (lines);
  if (!g_file_set_contents (argc > 2 ? argv[2] : argv[1], contents, -1, &error)) {
    g_printerr ("Error saving file: %s\n", error->message);
    g_error_free (error);
    return 1;
  }
  return 0;
}

