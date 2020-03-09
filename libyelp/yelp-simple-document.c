/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2020 Shaun McCance  <shaunm@gnome.org>
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "yelp-document.h"
#include "yelp-simple-document.h"

typedef struct _Request Request;
struct _Request {
    YelpDocument         *document;
    GCancellable         *cancellable;
    YelpDocumentCallback  callback;
    gpointer              user_data;

    gint                  idle_funcs;
};

struct _YelpSimpleDocumentPrivate {
    GFile        *file;
    GInputStream *stream;
    gchar        *page_id;

    gchar        *contents;
    gssize        contents_len;
    gssize        contents_read;
    gchar        *mime_type;
    gboolean      started;
    gboolean      finished;

    GSList       *reqs;
};

#define BUFFER_SIZE 4096

G_DEFINE_TYPE_WITH_PRIVATE (YelpSimpleDocument, yelp_simple_document, YELP_TYPE_DOCUMENT)

static void           yelp_simple_document_dispose     (GObject                 *object);
static void           yelp_simple_document_finalize    (GObject                 *object);

static gboolean       document_request_page            (YelpDocument            *document,
							const gchar             *page_id,
							GCancellable            *cancellable,
							YelpDocumentCallback     callback,
							gpointer                 user_data,
							GDestroyNotify           notify);
static const gchar *  document_read_contents           (YelpDocument            *document,
							const gchar             *page_id);
static void           document_finish_read             (YelpDocument            *document,
							const gchar             *contents);
static gchar *        document_get_mime_type           (YelpDocument            *document,
							const gchar             *mime_type);
static gboolean       document_signal_all              (YelpSimpleDocument      *document);

static void           file_info_cb                     (GFile                   *file,
							GAsyncResult            *result,
							YelpSimpleDocument      *document);
static void           file_read_cb                     (GFile                   *file,
							GAsyncResult            *result,
							YelpSimpleDocument      *document);
static void           stream_read_cb                   (GInputStream            *stream,
							GAsyncResult            *result,
							YelpSimpleDocument      *document);
static void           stream_close_cb                  (GInputStream            *stream,
							GAsyncResult            *result,
							YelpSimpleDocument      *document);

static void           request_cancel                   (GCancellable            *cancellable,
							Request                 *request);
static gboolean       request_try_free                 (Request                 *request);
static void           request_free                     (Request                 *request);

static void
yelp_simple_document_class_init (YelpSimpleDocumentClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    YelpDocumentClass *document_class = YELP_DOCUMENT_CLASS (klass);

    object_class->dispose  = yelp_simple_document_dispose;
    object_class->finalize = yelp_simple_document_finalize;

    document_class->request_page = document_request_page;
    document_class->read_contents = document_read_contents;
    document_class->finish_read = document_finish_read;
    document_class->get_mime_type = document_get_mime_type;
}

static void
yelp_simple_document_init (YelpSimpleDocument *document)
{
    document->priv = yelp_simple_document_get_instance_private (document);

    document->priv->file = NULL;
    document->priv->stream = NULL;

    document->priv->started = FALSE;
    document->priv->finished = FALSE;
    document->priv->contents = NULL;
    document->priv->mime_type = NULL;
}

static void
yelp_simple_document_dispose (GObject *object)
{
    YelpSimpleDocument *document = YELP_SIMPLE_DOCUMENT (object);

    if (document->priv->reqs) {
	g_slist_foreach (document->priv->reqs, (GFunc)(GCallback) request_try_free, NULL);
	g_slist_free (document->priv->reqs);
	document->priv->reqs = NULL;
    }

    if (document->priv->file) {
	g_object_unref (document->priv->file);
	document->priv->file = NULL;
    }

    if (document->priv->stream) {
	g_object_unref (document->priv->stream);
	document->priv->stream = NULL;
    }

    G_OBJECT_CLASS (yelp_simple_document_parent_class)->dispose (object);
}

static void
yelp_simple_document_finalize (GObject *object)
{
    YelpSimpleDocument *document = YELP_SIMPLE_DOCUMENT (object);

    g_free (document->priv->contents);
    g_free (document->priv->mime_type);
    g_free (document->priv->page_id);

    G_OBJECT_CLASS (yelp_simple_document_parent_class)->finalize (object);
}

YelpDocument *
yelp_simple_document_new (YelpUri *uri)
{
    YelpSimpleDocument *document;

    document = (YelpSimpleDocument *) g_object_new (YELP_TYPE_SIMPLE_DOCUMENT,
                                                    "document-uri", uri,
                                                    NULL);
    document->priv->file = yelp_uri_get_file (uri);
    document->priv->page_id = yelp_uri_get_page_id (uri);

    return (YelpDocument *) document;
}

/******************************************************************************/

static gboolean
document_request_page (YelpDocument         *document,
		       const gchar          *page_id,
		       GCancellable         *cancellable,
		       YelpDocumentCallback  callback,
		       gpointer              user_data,
		       GDestroyNotify        notify)
{
    YelpSimpleDocument *simple = YELP_SIMPLE_DOCUMENT (document);
    Request *request;
    gboolean ret = FALSE;

    request = g_slice_new0 (Request);

    request->document = g_object_ref (document);
    request->callback = callback;
    request->user_data = user_data;

    request->cancellable = g_object_ref (cancellable);
    g_signal_connect (cancellable, "cancelled",
		      G_CALLBACK (request_cancel), request);

    simple->priv->reqs = g_slist_prepend (simple->priv->reqs, request);

    if (simple->priv->finished) {
	g_idle_add ((GSourceFunc) document_signal_all, simple);
	ret = TRUE;
    }
    else if (!simple->priv->started) {
	simple->priv->started = TRUE;
	g_file_query_info_async (simple->priv->file,
				 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				 G_FILE_QUERY_INFO_NONE,
				 G_PRIORITY_DEFAULT,
				 NULL,
				 (GAsyncReadyCallback) file_info_cb,
				 document);
    }

    return ret;
}

static const gchar *
document_read_contents (YelpDocument *document,
			const gchar  *page_id)
{
    YelpSimpleDocument *simple = YELP_SIMPLE_DOCUMENT (document);

    return (const gchar*) simple->priv->contents;
}

static void
document_finish_read (YelpDocument *document,
		      const gchar  *contents)
{
    YelpSimpleDocument *simple = YELP_SIMPLE_DOCUMENT (document);

    if (simple->priv->reqs == NULL) {
	g_free (simple->priv->contents);
	simple->priv->contents = NULL;

	g_free (simple->priv->mime_type);
	simple->priv->mime_type = NULL;
    }
}

static gchar *
document_get_mime_type (YelpDocument *document,
			const gchar  *mime_type)
{
    YelpSimpleDocument *simple = YELP_SIMPLE_DOCUMENT (document);

    if (simple->priv->mime_type)
	return g_strdup (simple->priv->mime_type);
    else
	return NULL;
}

static gboolean
document_signal_all (YelpSimpleDocument *document)
{
    GSList *cur;
    for (cur = document->priv->reqs; cur != NULL; cur = cur->next) {
        Request *request = (Request *) cur->data;
        if (request->callback) {
            request->callback (request->document,
                               YELP_DOCUMENT_SIGNAL_INFO,
                               request->user_data,
                               NULL);
            request->callback (request->document,
                               YELP_DOCUMENT_SIGNAL_CONTENTS,
                               request->user_data,
                               NULL);
        }
    }
    return FALSE;
}

/******************************************************************************/

static void
file_info_cb (GFile              *file,
	      GAsyncResult       *result,
	      YelpSimpleDocument *document)
{
    GFileInfo *info = g_file_query_info_finish (file, result, NULL);
    const gchar *type = g_file_info_get_attribute_string (info,
							  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
    if (g_str_equal (type, "text/x-readme"))
        document->priv->mime_type = g_strdup ("text/plain");
    else
        document->priv->mime_type = g_strdup (type);
    g_object_unref (info);

    if (document->priv->page_id) {
        yelp_document_set_page_id (YELP_DOCUMENT (document),
                                   document->priv->page_id,
                                   document->priv->page_id);
        yelp_document_set_page_id (YELP_DOCUMENT (document),
                                   "//index",
                                   document->priv->page_id);
        yelp_document_set_page_id (YELP_DOCUMENT (document),
                                   NULL,
                                   document->priv->page_id);
    }
    else {
        yelp_document_set_page_id (YELP_DOCUMENT (document), "//index", "//index");
        yelp_document_set_page_id (YELP_DOCUMENT (document), NULL, "//index");
    }

    if (g_str_equal (document->priv->mime_type, "text/plain")) {
        gchar *basename = g_file_get_basename (document->priv->file);
        yelp_document_set_page_title (YELP_DOCUMENT (document), "//index", basename);
        g_free (basename);
    }

    yelp_document_set_page_icon (YELP_DOCUMENT (document), "//index", "yelp-page-symbolic");

    g_file_read_async (document->priv->file,
		       G_PRIORITY_DEFAULT,
		       NULL,
		       (GAsyncReadyCallback) file_read_cb,
		       document);
}

static void
file_read_cb (GFile              *file,
	      GAsyncResult       *result,
	      YelpSimpleDocument *document)
{
    GError *error = NULL;

    document->priv->stream = (GInputStream *) g_file_read_finish (file, result, &error);

    if (document->priv->stream == NULL) {
	GSList *cur;
	for (cur = document->priv->reqs; cur != NULL; cur = cur->next) {
	    Request *request = (Request *) cur->data;
	    if (request->callback) {
		GError *err = g_error_copy (error);
		request->callback (request->document,
				   YELP_DOCUMENT_SIGNAL_ERROR,
				   request->user_data,
				   err);
		g_error_free (err);
	    }
	}
	g_error_free (error);
	return;
    }

    g_assert (document->priv->contents == NULL);
    document->priv->contents_len = BUFFER_SIZE;
    document->priv->contents = g_realloc (document->priv->contents,
					  document->priv->contents_len);
    document->priv->contents[0] = '\0';
    document->priv->contents_read = 0;
    g_input_stream_read_async (document->priv->stream,
			       document->priv->contents,
			       BUFFER_SIZE,
			       G_PRIORITY_DEFAULT,
			       NULL,
			       (GAsyncReadyCallback) stream_read_cb,
			       document);
}

static void
stream_read_cb (GInputStream       *stream,
		GAsyncResult       *result,
		YelpSimpleDocument *document)
{
    gssize bytes;

    bytes = g_input_stream_read_finish (stream, result, NULL);
    document->priv->contents_read += bytes;

    if (bytes == 0) {
	/* If the preceding read filled contents, it was extended before the
	   read that gave us zero bytes.  Otherwise, there's room for this
	   byte.  I'm 99.99% certain I'm right.
	 */
	g_assert (document->priv->contents_read < document->priv->contents_len);
	document->priv->contents[document->priv->contents_read + 1] = '\0';
	g_input_stream_close_async (document->priv->stream,
				    G_PRIORITY_DEFAULT,
				    NULL,
				    (GAsyncReadyCallback) stream_close_cb,
				    document);
	return;
    }

    if (document->priv->contents_read == document->priv->contents_len) {
	document->priv->contents_len = document->priv->contents_read + BUFFER_SIZE;
	document->priv->contents = g_realloc (document->priv->contents,
					      document->priv->contents_len);
    }
    g_input_stream_read_async (document->priv->stream,
			       document->priv->contents + document->priv->contents_read,
			       document->priv->contents_len - document->priv->contents_read,
			       G_PRIORITY_DEFAULT,
			       NULL,
			       (GAsyncReadyCallback) stream_read_cb,
			       document);
}

static void
stream_close_cb (GInputStream       *stream,
		 GAsyncResult       *result,
		 YelpSimpleDocument *document)
{
    document->priv->finished = TRUE;
    document_signal_all (document);
}

/******************************************************************************/

static void
request_cancel (GCancellable *cancellable, Request *request)
{
    GSList *cur;
    YelpSimpleDocument *document = (YelpSimpleDocument *) request->document;

    g_assert (document != NULL && YELP_IS_SIMPLE_DOCUMENT (document));

    for (cur = document->priv->reqs; cur != NULL; cur = cur->next) {
	if (cur->data == request) {
	    document->priv->reqs = g_slist_delete_link (document->priv->reqs, cur);
	    break;
	}
    }
    request_try_free (request);
}

static gboolean
request_try_free (Request *request)
{
    if (!g_cancellable_is_cancelled (request->cancellable))
	g_cancellable_cancel (request->cancellable);

    if (request->idle_funcs == 0)
	request_free (request);
    else
	g_idle_add ((GSourceFunc) request_try_free, request);
    return FALSE;
}

static void
request_free (Request *request)
{
    g_object_unref (request->document);
    g_object_unref (request->cancellable);

    g_slice_free (Request, request);
}
