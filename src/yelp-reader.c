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
#include <libgnomevfs/gnome-vfs.h>
#include <libxml/xmlIO.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include <string.h>

#include "yelp-cache.h"
#include "yelp-marshal.h"
#include "yelp-reader.h"

#define d(x)

#define BUFFER_SIZE 16384

#define STAMP_MUTEX_LOCK    g_mutex_lock(priv->stamp_mutex);
#define STAMP_MUTEX_UNLOCK  g_mutex_unlock(priv->stamp_mutex);

struct _YelpReaderPriv {
	gint            stamp;

        gboolean        active;

	/* Locks */
	GMutex         *stamp_mutex;
	GAsyncQueue    *thread_queue;
};

typedef struct {
	YelpReader *reader;
	gint        stamp;
	YelpURI    *uri;
} ReaderThreadData;

typedef enum {
	READER_QUEUE_TYPE_START,
	READER_QUEUE_TYPE_DATA,
	READER_QUEUE_TYPE_CANCELLED,
	READER_QUEUE_TYPE_FINISHED
} ReaderQueueType;

typedef struct {
	YelpReader      *reader;
	gint             stamp;
	gchar           *data;
	ReaderQueueType  type;
} ReaderQueueData;

static void      reader_class_init        (YelpReaderClass     *klass);
static void      reader_init              (YelpReader          *reader);

static void      reader_convert_start     (ReaderThreadData    *th_data);

static void      reader_file_start        (ReaderThreadData    *th_data);

static gboolean  reader_check_cancelled   (YelpReader          *reader,
					   gint                 stamp);
static gpointer  reader_start             (ReaderThreadData    *th_data);
static void      reader_change_stamp      (YelpReader          *reader);
static gboolean  reader_idle_check_queue  (ReaderThreadData    *th_data);

static ReaderQueueData * 
reader_q_data_new                         (YelpReader          *reader,
					   gint                 stamp,
					   ReaderQueueType      type);
static void      reader_q_data_free       (ReaderQueueData     *q_data);

#if 0
/* FIXME: Solve this so we don't leak */
static void      reader_th_data_free      (ReaderThreadData    *th_data);
#endif
static gchar *   reader_get_chunk         (const gchar         *document,
					   const gchar         *section);

enum {
	START,
	DATA,
	FINISHED,
	CANCELLED,
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
			      yelp_marshal_VOID__STRING_INT,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_INT);

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
	signals[CANCELLED] =
		g_signal_new ("cancelled",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpReaderClass,
					       cancelled),
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
	YelpReaderPriv *priv;
	
	priv = g_new0 (YelpReaderPriv, 1);
	reader->priv = priv;
	
	priv->active       = FALSE;
	priv->stamp        = 0;
	priv->stamp_mutex  = g_mutex_new ();
	priv->thread_queue = g_async_queue_new ();
}

static void
reader_convert_start (ReaderThreadData *th_data)
{
	YelpReader      *reader;
	YelpReaderPriv  *priv;
	YelpURI         *uri;
	gchar           *command_line = NULL;
	GError          *error = NULL;
	gint             exit_status;
	ReaderQueueData *q_data;
	gint             stamp; 
	
	g_return_if_fail (th_data != NULL);

	reader = th_data->reader;
	priv   = reader->priv;
	uri    = th_data->uri;
	stamp  = th_data->stamp;
	
	d(g_print ("convert_start\n"));

	STAMP_MUTEX_LOCK;
	
	if (reader_check_cancelled (reader, th_data->stamp)) {
		
		STAMP_MUTEX_UNLOCK;
		
		return;
	}

	STAMP_MUTEX_UNLOCK;

	switch (yelp_uri_get_type (uri)) {
	case YELP_URI_TYPE_MAN:
		command_line = g_strdup_printf ("gnome2-man2html %s",
						yelp_uri_get_path (uri));
		break;
	case YELP_URI_TYPE_INFO:
		if (yelp_uri_get_section (uri)) {
			command_line = 
				g_strdup_printf ("gnome2-info2html %s?%s",
						 yelp_uri_get_path (uri),
						 yelp_uri_get_section (uri));
		} else { 
			command_line = 
				g_strdup_printf ("gnome2-info2html %s",
						 yelp_uri_get_path (uri));
		}
		
		break;
	case YELP_URI_TYPE_DOCBOOK_XML:
	case YELP_URI_TYPE_DOCBOOK_SGML:
		command_line = g_strdup_printf ("yelp-db2html %s",
						yelp_uri_get_path (uri));
		break;
	default:
		/* Set error */
		break;
	}

	q_data = reader_q_data_new (reader, priv->stamp,
				    READER_QUEUE_TYPE_START);
	
	g_async_queue_push (priv->thread_queue, q_data);

	q_data = reader_q_data_new (reader, priv->stamp, 
				    READER_QUEUE_TYPE_DATA);
	
	g_spawn_command_line_sync (command_line,
				   &q_data->data,
				   NULL,
				   &exit_status,
				   &error /* FIXME */);
	if (yelp_uri_get_type (uri) == YELP_URI_TYPE_DOCBOOK_XML ||
	    yelp_uri_get_type (uri) == YELP_URI_TYPE_DOCBOOK_SGML) {
		 yelp_cache_add (yelp_uri_get_path (uri), q_data->data);
	}

	g_free (command_line);

	STAMP_MUTEX_LOCK;

	if (reader_check_cancelled (reader, stamp)) {
		
		STAMP_MUTEX_UNLOCK;

		reader_q_data_free (q_data);
		
		return;
	}
	
	STAMP_MUTEX_UNLOCK;

	if (error) {
		/* FIXME: Don't do this */
		g_signal_emit (reader, signals[ERROR], 0, error);
		g_error_free (error);
	} else {
		if (yelp_uri_get_type (uri) == YELP_URI_TYPE_DOCBOOK_XML ||
		    yelp_uri_get_type (uri) == YELP_URI_TYPE_DOCBOOK_SGML) {
			gchar *chunk;
			
			if (yelp_uri_get_section (uri) &&
			    strcmp (yelp_uri_get_section (uri), "")) {
				chunk = reader_get_chunk (q_data->data,
							  yelp_uri_get_section (uri));
			} else {
				chunk = reader_get_chunk (q_data->data,
							  "toc");
			}

			g_free (q_data->data);
			q_data->data = chunk;

			if (!q_data->data) {
				q_data->data = g_strdup ("<html><body></body></html>");
			}
		}
		
		g_async_queue_push (priv->thread_queue, q_data);
			
		q_data = reader_q_data_new (reader, priv->stamp, 
					    READER_QUEUE_TYPE_FINISHED);

		g_async_queue_push (priv->thread_queue, q_data);
	}
}

static void
reader_file_start (ReaderThreadData *th_data)
{
	YelpReader       *reader;
	YelpReaderPriv   *priv;
	YelpURI          *uri;
	gint              stamp;
	GnomeVFSHandle   *handle;
	GnomeVFSResult    result;
	ReaderQueueData  *q_data;
	gchar             buffer[BUFFER_SIZE];
	GnomeVFSFileSize  n;
	
	g_return_if_fail (th_data != NULL);

	reader = th_data->reader;
	priv   = reader->priv;
	uri    = th_data->uri;
	stamp  = th_data->stamp;
	
	d(g_print ("file_start\n"));

	STAMP_MUTEX_LOCK;
	
	if (reader_check_cancelled (reader, stamp)) {
		
		STAMP_MUTEX_UNLOCK;

		return;
	}

	STAMP_MUTEX_UNLOCK;

	result = gnome_vfs_open (&handle, 
				 yelp_uri_get_path (uri),
				 GNOME_VFS_OPEN_READ);
	
	if (result != GNOME_VFS_OK) {
		/* FIXME: Signal error */
		return;
	}

	while (TRUE) {
		result = gnome_vfs_read (handle, buffer, BUFFER_SIZE, &n);
		
		/* FIXME: Do some error checking */
		if (result != GNOME_VFS_OK) {
			break;
		}
		
		q_data = reader_q_data_new (reader, stamp, 
					    READER_QUEUE_TYPE_DATA);
		q_data->data = g_strdup (buffer);
		
		g_async_queue_push (priv->thread_queue, q_data);

		STAMP_MUTEX_LOCK;
		
		if (reader_check_cancelled (reader, stamp)) {
		
			STAMP_MUTEX_UNLOCK;

			return;
		}
		
		STAMP_MUTEX_UNLOCK;
	}
	
	q_data = reader_q_data_new (reader, stamp, READER_QUEUE_TYPE_FINISHED);
	
	g_async_queue_push (priv->thread_queue, q_data);
}

static gboolean
reader_check_cancelled (YelpReader *reader, gint stamp)
{
	YelpReaderPriv *priv;
	
	g_return_val_if_fail (YELP_IS_READER (reader), TRUE);
	
	priv = reader->priv;

	d(g_print ("check_cancelled\n"));
	
	if (priv->stamp != stamp) {
		return TRUE;
	}
	
	return FALSE;
}

static gpointer 
reader_start (ReaderThreadData *th_data)
{
	YelpReader         *reader;
	YelpReaderPriv     *priv;
	YelpURI            *uri;
	gchar              *str_uri;
	
	g_return_val_if_fail (th_data != NULL, NULL);

	reader = th_data->reader;
	priv   = reader->priv;
	uri    = th_data->uri;
	
	d(g_print ("reader_start\n"));

	if (!yelp_uri_exists (uri)) {
		return NULL;
	}

	STAMP_MUTEX_LOCK;
	
	if (reader_check_cancelled (reader, th_data->stamp)) {
		STAMP_MUTEX_UNLOCK;

		/* FIXME: refs??? */
/* 		reader_th_data_free(th_data); */

		return NULL;
	}

	if (priv->active) {
		reader_change_stamp (reader);
	}

	STAMP_MUTEX_UNLOCK;
	
	switch (yelp_uri_get_type (uri)) {
/* 		reader_db_start (th_data); */
/* 		break; */
	case YELP_URI_TYPE_DOCBOOK_XML:
	case YELP_URI_TYPE_DOCBOOK_SGML:
	case YELP_URI_TYPE_MAN:
	case YELP_URI_TYPE_INFO:
		reader_convert_start (th_data);
		break;
	case YELP_URI_TYPE_HTML:
		reader_file_start (th_data);
		break;
	case YELP_URI_TYPE_TOC:
		/* Should this be handled here?? */
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

	return NULL;
}

static void 
reader_change_stamp (YelpReader *reader)
{
	YelpReaderPriv *priv;
	
	g_return_if_fail (YELP_IS_READER (reader));
	
	priv = reader->priv;

	if ((priv->stamp++) >= G_MAXINT) {
		priv->stamp = 1;
	}
}

static gboolean
reader_idle_check_queue (ReaderThreadData *th_data)
{
	YelpReader      *reader;
	YelpReaderPriv  *priv;
	ReaderQueueData *q_data;
	gboolean         ret_val = TRUE;
	
	g_return_val_if_fail (th_data != NULL, FALSE);

	reader = th_data->reader;
	priv   = reader->priv;
	
	if (!g_mutex_trylock (priv->stamp_mutex)) {
		return TRUE;
	}

	if (th_data->stamp != priv->stamp) {
		STAMP_MUTEX_UNLOCK;
	
/* 		reader_th_data_free (th_data); */

		return FALSE;
	}

	d(g_print ("Poping from queue... "));
	q_data = (ReaderQueueData *)
		g_async_queue_try_pop (priv->thread_queue);
	d(g_print ("done\n"));
	
	if (q_data) {
		if (priv->stamp != q_data->stamp) {
			/* Some old data */
			reader_q_data_free (q_data);
			ret_val = TRUE;

			q_data = NULL;
		}
	}
	
	if (q_data) {
		switch (q_data->type) {
		case READER_QUEUE_TYPE_START:
			g_signal_emit (reader, signals[START], 0);
			ret_val = TRUE;
			break;
		case READER_QUEUE_TYPE_DATA:
			d(g_print ("queue_type_data\n"));
			
			if (q_data->data != NULL) {
				g_signal_emit (reader, signals[DATA], 0, 
					       q_data->data, -1);
			}
			ret_val = TRUE;
			break;
		case READER_QUEUE_TYPE_CANCELLED:
			priv->active = FALSE;

			if ((priv->stamp++) >= G_MAXINT) {
				priv->stamp = 1;
			}

			g_signal_emit (reader, signals[CANCELLED], 0);

			ret_val = FALSE;
			break;
		case READER_QUEUE_TYPE_FINISHED:
			priv->active = FALSE;

			d(g_print ("queue_type_finished\n"));
			
			if ((priv->stamp++) >= G_MAXINT) {
				priv->stamp = 1;
			}

			g_signal_emit (reader, signals[FINISHED], 0);

			ret_val = FALSE;
			break;
		default:
			g_assert_not_reached ();
		}

		reader_q_data_free (q_data);
	}

	STAMP_MUTEX_UNLOCK;
			
	return ret_val;
}

static ReaderQueueData * 
reader_q_data_new (YelpReader *reader, gint stamp, ReaderQueueType type)
{
	ReaderQueueData *q_data;
	
	q_data = g_new0 (ReaderQueueData, 1);
	q_data->reader = g_object_ref (reader);
	q_data->stamp  = stamp;
	q_data->type   = type;
	q_data->data   = NULL;

	return q_data;
}

static void
reader_q_data_free (ReaderQueueData *q_data)
{
	g_return_if_fail (q_data != NULL);
	
	g_object_unref (q_data->reader);
	g_free (q_data->data);
	g_free (q_data);
}

#if 0
static void
reader_th_data_free (ReaderThreadData *th_data)
{
	g_return_if_fail (th_data != NULL);
	
	g_object_unref (th_data->reader);
	yelp_uri_unref (th_data->uri);
	
	g_free (th_data);
}
#endif

static gchar *
reader_get_chunk (const gchar *document, const gchar *section)
{
	gchar       *header;
	gchar       *chunk;
	const gchar *footer;
	gchar       *ret_val;
	const gchar *start;
	const gchar *end;
	gchar       *tag;
	GTimer      *timer;
	
/* 	g_print ("%s\n", document); */

	timer = g_timer_new ();

	end = strstr (document, "<!-- End of header -->");
	
	if (!end) {
/* 		g_warning ("Wrong type of document\n"); */
		return g_strdup (document);
	}
	
	header = g_strndup (document, end - document);
	
	tag = g_strdup_printf ("<!-- Start of chunk: [%s] -->", section);
	start = strstr (document, tag);
	g_free (tag);
	
	if (!start) {
/* 		g_warning ("Document doesn't include section: '%s'", section); */
		g_free (header);

		return g_strdup (document);
	}

	end = strstr (start, "<!-- End of chunk -->");

	if (!end) {
/* 		g_warning ("Document is doesn't contain end tag for section: %s", */
/* 			   section); */
		g_free (header);

		return g_strdup (document);
	}
	
	chunk = g_strndup (start, end - start);
	
	footer = strstr (document, "<!-- Start of footer -->");
	
	if (!footer) {
/* 		g_warning ("Couldn't find footer in document"); */
		g_free (header);
		g_free (chunk);

		return g_strdup (document);
	}
	 
	ret_val = g_strconcat (header, chunk, footer, NULL);
	
	g_free (header);
	g_free (chunk);

/* 	g_print ("Finding chunk took: %f seconds\n",  */
/* 		 g_timer_elapsed (timer, 0)); */

	return ret_val;
}

YelpReader *
yelp_reader_new ()
{
        YelpReader     *reader;
        YelpReaderPriv *priv;
	
        reader = g_object_new (YELP_TYPE_READER, NULL);
        
	priv = reader->priv;
	
        return reader;
}

gboolean
yelp_reader_start (YelpReader *reader, YelpURI *uri)
{
        YelpReaderPriv   *priv;
	ReaderThreadData *th_data;
	gint              stamp;
	
	g_return_val_if_fail (YELP_IS_READER (reader), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	priv = reader->priv;

	d(g_print ("yelp_reader_start\n"));

	STAMP_MUTEX_LOCK;
	
	if (priv->active) {
		reader_change_stamp (reader);
	}
	
	stamp = priv->stamp;

	STAMP_MUTEX_UNLOCK;

	th_data = g_new0 (ReaderThreadData, 1);
	th_data->reader = g_object_ref (reader);
	th_data->uri    = yelp_uri_ref (uri);
	th_data->stamp  = stamp;

	if (yelp_uri_get_type (uri) == YELP_URI_TYPE_DOCBOOK_XML || 
	    yelp_uri_get_type (uri) == YELP_URI_TYPE_DOCBOOK_SGML) {
		const gchar *document;
		gchar *chunk;

		document = yelp_cache_lookup (yelp_uri_get_path (uri));
		
		if (document) {
			if (yelp_uri_get_section (uri) &&
			    strcmp (yelp_uri_get_section (uri), "")) {
				chunk = reader_get_chunk (document, 
							  yelp_uri_get_section (uri));
			} else {
				chunk = reader_get_chunk (document, "toc");
			}
		
			if (chunk) {
				ReaderQueueData *q_data;
				
				q_data = reader_q_data_new (reader, stamp, 
							    READER_QUEUE_TYPE_START);
				g_async_queue_push (priv->thread_queue, 
						    q_data);

				q_data = reader_q_data_new (reader, stamp, 
							    READER_QUEUE_TYPE_DATA);

				q_data->data = chunk;
				g_async_queue_push (priv->thread_queue, 
						    q_data);
				
				q_data = reader_q_data_new (reader, stamp, 
							    READER_QUEUE_TYPE_FINISHED);

				g_async_queue_push (priv->thread_queue, 
						    q_data);

				g_idle_add ((GSourceFunc) reader_idle_check_queue, th_data);
				return TRUE;
			}
		}
	}
	
	g_timeout_add (100, (GSourceFunc) reader_idle_check_queue, th_data);

	g_thread_create ((GThreadFunc) reader_start, th_data,
			 TRUE, 
			 NULL /* FIXME: check for errors */);
	return FALSE;
}

void
yelp_reader_cancel (YelpReader *reader)
{
	YelpReaderPriv *priv;
	
	g_return_if_fail (YELP_IS_READER (reader));
	
	priv = reader->priv;

	STAMP_MUTEX_LOCK;
	
	reader_change_stamp (reader);

	STAMP_MUTEX_UNLOCK;
}
