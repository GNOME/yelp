/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Mikael Hallendal <micke@codefactory.se>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Mikael Hallendal <micke@codefactory.se>
 */

#include <libgnome/gnome-url.h>
#include <libxml/xmlIO.h>

#include "yelp-db2html.h"
#include "yelp-marshal.h"
#include "yelp-reader.h"

#define d(x)
#define BUFFER_SIZE 16384

struct _YelpReaderPriv {
        YelpURI *current_uri;
        
};


static void      reader_class_init        (YelpReaderClass     *klass);
static void      reader_init              (YelpReader          *reader);

static void      reader_db_start          (YelpReader          *reader,
					   YelpURI             *uri);
static gint      reader_db_write          (YelpReader          *reader,
					   const gchar         *buffer,
					   gint                 len);
static gint      reader_db_close          (YelpReader          *reader);
static void      reader_man_info_start    (YelpReader          *reader,
					   YelpURI             *uri);
static gboolean  reader_io_watch_cb       (GIOChannel          *io_channel,
					   GIOCondition         condition,
					   YelpReader          *reader);
static void      reader_file_start        (YelpReader          *reader,
					   YelpURI             *uri);


enum {
	START,
	DATA,
	FINISHED,
	ERROR,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

GType
yelp_reader_get_type (void)
{
        static GType type = 0;

        if (!type) {
                static const GTypeInfo info =
                        {
                                sizeof (YelpReaderClass),
                                NULL,
                                NULL,
                                (GClassInitFunc) reader_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpReader),
                                0,
                                (GInstanceInitFunc) reader_init,
                        };
                
                type = g_type_register_static (G_TYPE_OBJECT,
                                               "YelpReader", 
                                               &info, 0);
        }
        
        return type;
}

static void
reader_class_init (YelpReaderClass *klass)
{
	signals[START] = 
		g_signal_new ("start",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpReaderClass,
					       start),
			      NULL, NULL,
			      yelp_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	signals[DATA] =
		g_signal_new ("data",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpReaderClass,
					       data),
			      NULL, NULL,
			      yelp_marshal_VOID__INT_STRING,
			      G_TYPE_NONE,
			      2, G_TYPE_INT, G_TYPE_STRING);

	signals[FINISHED] =
		g_signal_new ("finished",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpReaderClass,
					       finished),
			      NULL, NULL,
			      yelp_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals[ERROR] =
		g_signal_new ("error",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpReaderClass,
					       error),
			      NULL, NULL,
			      yelp_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
}

static void
reader_init (YelpReader *reader)
{
}

static void
reader_db_start (YelpReader *reader, YelpURI *uri)
{
	xmlOutputBufferPtr  buf;
	GError             *error = NULL;

	g_return_if_fail (YELP_IS_READER (reader));
	g_return_if_fail (uri != NULL);
	
	buf = xmlAllocOutputBuffer (NULL);
	
	buf->writecallback = (xmlOutputWriteCallback) reader_db_write;
	buf->closecallback = (xmlOutputCloseCallback) reader_db_close;
	buf->context       = reader;
	
	g_signal_emit (reader, signals[START], 0);

	yelp_db2html_convert (uri, buf, &error);
	
	if (error) {
		g_warning ("Have an error here: %s\n", error->message);
		g_signal_emit (reader, signals[ERROR], 0, error);
		g_error_free (error);
	}

        xmlOutputBufferClose (buf);
}

static int
reader_db_write (YelpReader *reader, const gchar *buffer, gint len)
{
	g_return_val_if_fail (YELP_IS_READER (reader), -1);
	
	d(g_print ("reader_db_write: %d\n", len));
	
	if (len <= 0) {
		return 0;
	}

	g_signal_emit (reader, signals[DATA], 0, len, buffer);

	return len;
}

static int
reader_db_close (YelpReader *reader)
{
	g_return_val_if_fail (YELP_IS_READER (reader), -1);
	
	g_signal_emit (reader, signals[FINISHED], 0);

	return 0;
}

static void
reader_man_info_start (YelpReader *reader, YelpURI *uri)
{
	gchar       *command_line = NULL;
	gchar      **argv = 0;
	gint         output_fd;
	GIOChannel  *io_channel;
	GError      *error = NULL;
	
	g_return_if_fail (YELP_IS_READER (reader));
	g_return_if_fail (uri != NULL);

	switch (yelp_uri_get_type (uri)) {
	case YELP_URI_TYPE_MAN:
		command_line = g_strdup_printf ("gnome2-man2html %s",
						yelp_uri_get_path (uri));
		break;
	case YELP_URI_TYPE_INFO:
		command_line = g_strdup_printf ("gnome2-info2html %s",
						yelp_uri_get_path (uri));
		break;
	default:
		/* Set error */
		break;
	}

	g_shell_parse_argv (command_line, NULL, &argv, &error);
	g_free (command_line);
	
	g_signal_emit (reader, signals[START], 0);
	
	g_spawn_async_with_pipes (NULL, argv, NULL,
				  G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_SEARCH_PATH,
				  NULL, NULL, NULL, NULL,
				  &output_fd, NULL, &error);
	g_strfreev (argv);

	io_channel = g_io_channel_unix_new (output_fd);
	
	g_io_add_watch (io_channel, G_IO_IN | G_IO_HUP,
			(GIOFunc) reader_io_watch_cb,
			reader);
	
	if (error) {
		g_signal_emit (reader, signals[ERROR], 0, error);
		g_error_free (error);
	}
}

static gboolean
reader_io_watch_cb (GIOChannel   *io_channel, 
		    GIOCondition  condition, 
		    YelpReader   *reader)
{
	static gchar  buffer[BUFFER_SIZE];
	guint         n = 0;
	gboolean      finished = FALSE;
	GIOStatus     io_status;
	GError       *error = NULL;
	
	g_return_val_if_fail (YELP_IS_READER (reader), FALSE);

	if (condition & G_IO_IN) {
 		d(g_print ("Read available\n"));
		
		do {
			io_status = g_io_channel_read_chars (io_channel,
							     buffer,
							     BUFFER_SIZE,
							     &n,
							     &error);
			
			if (error) {
				g_signal_emit (reader, signals[ERROR], 0, 
					       error);
				g_error_free (error);
				finished = TRUE;
			} else {
				switch (io_status) {
				case G_IO_STATUS_NORMAL:
					g_signal_emit (reader, signals[DATA], 
						       0, 
						       n, buffer);
				
					break;
				case G_IO_STATUS_EOF:
				case G_IO_STATUS_ERROR:
					d(g_print ("Finished!\n"));
					if (n > 0) {
						g_signal_emit (reader,
							       signals[DATA], 
							       0,
							       n, buffer);
					}
				
					/* Signal error */
					
					finished = TRUE;
					break;
				default:
					break;
				}
			}
		} while (io_status == G_IO_STATUS_NORMAL);
	}

	if (condition & G_IO_HUP || finished) {
		d(g_print ("Close\n"));

		g_io_channel_unref (io_channel);
		
		g_signal_emit (reader, signals[FINISHED], 0);
		
		return FALSE;
	}

	return TRUE;
}

static void
reader_file_start (YelpReader *reader, YelpURI *uri)
{
	GIOChannel *io_channel = NULL;
	GError     *error = NULL;
     
	g_return_if_fail (YELP_IS_READER (reader));
	g_return_if_fail (uri != NULL);
	
	io_channel = g_io_channel_new_file (yelp_uri_get_path (uri), 
					    "r", 
					    &error);
	
	if (error) {
		g_signal_emit (reader, signals[ERROR], 0, error);
		g_error_free (error);
	}

	g_io_add_watch (io_channel, G_IO_IN | G_IO_HUP,
			(GIOFunc) reader_io_watch_cb,
			reader);
}

YelpReader *
yelp_reader_new (gboolean async)
{
        YelpReader *reader;
        
        reader = g_object_new (YELP_TYPE_READER, NULL);
        
        return reader;
}

void
yelp_reader_read (YelpReader *reader, YelpURI *uri)
{
	gchar *str_uri;
		
	g_return_if_fail (uri != NULL);
	
	switch (yelp_uri_get_type (uri)) {
	case YELP_URI_TYPE_DOCBOOK_XML:
	case YELP_URI_TYPE_DOCBOOK_SGML:
		reader_db_start (reader, uri);
		break;
	case YELP_URI_TYPE_MAN:
	case YELP_URI_TYPE_INFO:
		reader_man_info_start (reader, uri);
		break;
	case YELP_URI_TYPE_HTML:
		reader_file_start (reader, uri);
		break;
	case YELP_URI_TYPE_TOC:
		/* Should this be handled here?? */
		break;
	case YELP_URI_TYPE_NON_EXISTENT:
		/* Set GError and signal error */
		break;
	case YELP_URI_TYPE_INDEX:
	case YELP_URI_TYPE_PATH:
		/* Hmm .. is this wrong or what?? */
		break;
	case YELP_URI_TYPE_UNKNOWN:
		str_uri = yelp_uri_to_string (uri);
		gnome_url_show (str_uri, NULL);
		g_free (str_uri);
		
		break;
	default:
		g_assert_not_reached ();
	}
}
