/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2007-2020 Shaun McCance <shaunm@gnome.org>
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
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libxml/tree.h>

#include "yelp-error.h"
#include "yelp-man-document.h"
#include "yelp-man-parser.h"
#include "yelp-transform.h"
#include "yelp-settings.h"

#define STYLESHEET DATADIR"/yelp/xslt/man2html.xsl"

typedef enum {
    MAN_STATE_BLANK,   /* Brand new, run transform as needed */
    MAN_STATE_PARSING, /* Parsing/transforming document, please wait */
    MAN_STATE_PARSED,  /* All done, if we ain't got it, it ain't here */
    MAN_STATE_STOP     /* Stop everything now, object to be disposed */
} ManState;

typedef struct _YelpManDocumentPrivate  YelpManDocumentPrivate;
struct _YelpManDocumentPrivate {
    ManState    state;
    gchar      *page_id;

    GMutex      mutex;
    GThread    *thread;

    xmlDocPtr   xmldoc;

    gboolean    process_running;
    gboolean    transform_running;

    YelpTransform *transform;
    guint          chunk_ready;
    guint          finished;
    guint          error;
};

G_DEFINE_TYPE_WITH_PRIVATE (YelpManDocument, yelp_man_document, YELP_TYPE_DOCUMENT)

static void           yelp_man_document_finalize         (GObject                *object);

/* YelpDocument */
static gboolean       man_request_page                   (YelpDocument           *document,
                                                          const gchar            *page_id,
                                                          GCancellable           *cancellable,
                                                          YelpDocumentCallback    callback,
                                                          gpointer                user_data,
                                                          GDestroyNotify          notify);

/* YelpTransform */
static void           transform_chunk_ready              (YelpTransform          *transform,
                                                          gchar                  *chunk_id,
                                                          YelpManDocument        *man);
static void           transform_finished                 (YelpTransform          *transform,
                                                          YelpManDocument        *man);
static void           transform_error                    (YelpTransform          *transform,
                                                          YelpManDocument        *man);
static void           transform_finalized                (YelpManDocument        *man,
                                                          gpointer                transform);

/* Threaded */
static void           man_document_process               (YelpManDocument        *man);

static void           man_document_disconnect            (YelpManDocument        *man);


static void
yelp_man_document_class_init (YelpManDocumentClass *klass)
{
    GObjectClass      *object_class   = G_OBJECT_CLASS (klass);
    YelpDocumentClass *document_class = YELP_DOCUMENT_CLASS (klass);

    object_class->finalize = yelp_man_document_finalize;

    document_class->request_page = man_request_page;
}

static void
yelp_man_document_init (YelpManDocument *man)
{
    YelpManDocumentPrivate *priv = yelp_man_document_get_instance_private (man);

    priv->state = MAN_STATE_BLANK;
    g_mutex_init (&priv->mutex);
}

static void
yelp_man_document_finalize (GObject *object)
{
    YelpManDocumentPrivate *priv =
        yelp_man_document_get_instance_private (YELP_MAN_DOCUMENT (object));

    if (priv->xmldoc)
	xmlFreeDoc (priv->xmldoc);

    g_mutex_clear (&priv->mutex);
    g_free (priv->page_id);

    G_OBJECT_CLASS (yelp_man_document_parent_class)->finalize (object);
}

/******************************************************************************/

YelpDocument *
yelp_man_document_new (YelpUri *uri)
{
    g_return_val_if_fail (uri != NULL, NULL);

    return  (YelpDocument *) g_object_new (YELP_TYPE_MAN_DOCUMENT,
                                           "document-uri", uri,
                                           NULL);
}


/******************************************************************************/
/** YelpDocument **************************************************************/

static gboolean
man_request_page (YelpDocument         *document,
                  const gchar          *page_id,
                  GCancellable         *cancellable,
                  YelpDocumentCallback  callback,
                  gpointer              user_data,
                  GDestroyNotify        notify)
{
    YelpManDocumentPrivate *priv =
        yelp_man_document_get_instance_private (YELP_MAN_DOCUMENT (document));
    gchar *docuri, *fulluri;
    GError *error;
    gboolean handled;

    fulluri = yelp_uri_get_canonical_uri (yelp_document_get_uri (document));
    if (g_str_has_prefix (fulluri, "man:"))
        priv->page_id = g_strdup (fulluri + 4);
    else
        priv->page_id = g_strdup ("//index");
    g_free (fulluri);

    handled =
        YELP_DOCUMENT_CLASS (yelp_man_document_parent_class)->request_page (document,
                                                                            page_id,
                                                                            cancellable,
                                                                            callback,
                                                                            user_data,
                                                                            notify);
    if (handled) {
        return handled;
    }

    g_mutex_lock (&priv->mutex);

    switch (priv->state) {
    case MAN_STATE_BLANK:
	priv->state = MAN_STATE_PARSING;
	priv->process_running = TRUE;
        g_object_ref (document);
        yelp_document_set_page_id (document, page_id, priv->page_id);
        yelp_document_set_page_id (document, NULL, priv->page_id);
        yelp_document_set_page_id (document, "//index", priv->page_id);
        yelp_document_set_page_id (document, priv->page_id, priv->page_id);
        yelp_document_set_root_id (document, priv->page_id, priv->page_id);
	priv->thread = g_thread_new ("man-page",
                                     (GThreadFunc)(GCallback) man_document_process,
                                     document);
	break;
    case MAN_STATE_PARSING:
	break;
    case MAN_STATE_PARSED:
    case MAN_STATE_STOP:
        docuri = yelp_uri_get_document_uri (yelp_document_get_uri (document));
        error = g_error_new (YELP_ERROR, YELP_ERROR_NOT_FOUND,
                             _("The page ‘%s’ was not found in the document ‘%s’."),
                             page_id, docuri);
        g_free (docuri);
        yelp_document_signal (document, page_id,
                              YELP_DOCUMENT_SIGNAL_ERROR,
                              error);
        g_error_free (error);
        break;
    default:
        g_assert_not_reached ();
        break;
    }

    g_mutex_unlock (&priv->mutex);
    return FALSE;
}


/******************************************************************************/
/** YelpTransform *************************************************************/

static void
transform_chunk_ready (YelpTransform    *transform,
                       gchar            *chunk_id,
                       YelpManDocument  *man)
{
    YelpManDocumentPrivate *priv = yelp_man_document_get_instance_private (man);
    gchar *content;

    g_assert (transform == priv->transform);

    if (priv->state == MAN_STATE_STOP) {
        man_document_disconnect (man);
        return;
    }

    content = yelp_transform_take_chunk (transform, chunk_id);
    yelp_document_give_contents (YELP_DOCUMENT (man),
                                 priv->page_id,
                                 content,
                                 "application/xhtml+xml");

    yelp_document_signal (YELP_DOCUMENT (man),
                          priv->page_id,
                          YELP_DOCUMENT_SIGNAL_INFO,
                          NULL);
    yelp_document_signal (YELP_DOCUMENT (man),
                          priv->page_id,
                          YELP_DOCUMENT_SIGNAL_CONTENTS,
                          NULL);
}

static void
transform_finished (YelpTransform    *transform,
                    YelpManDocument  *man)
{
    YelpManDocumentPrivate *priv = yelp_man_document_get_instance_private (man);
    gchar *docuri;
    GError *error;

    g_assert (transform == priv->transform);

    if (priv->state == MAN_STATE_STOP) {
        man_document_disconnect (man);
        return;
    }

    man_document_disconnect (man);
    priv->state = MAN_STATE_PARSED;

    /* We want to free priv->xmldoc, but we can't free it before transform
       is finalized.   Otherwise, we could crash when YelpTransform frees
       its libxslt resources.
     */
    g_object_weak_ref ((GObject *) transform,
                       (GWeakNotify) transform_finalized,
                       man);

    docuri = yelp_uri_get_document_uri (yelp_document_get_uri ((YelpDocument *) man));
    error = g_error_new (YELP_ERROR, YELP_ERROR_NOT_FOUND,
                         _("The requested page was not found in the document ‘%s’."),
                         docuri);
    g_free (docuri);
    yelp_document_error_pending ((YelpDocument *) man, error);
    g_error_free (error);
}

static void
transform_error (YelpTransform    *transform,
                 YelpManDocument  *man)
{
    YelpManDocumentPrivate *priv = yelp_man_document_get_instance_private (man);
    GError *error;

    g_assert (transform == priv->transform);

    if (priv->state == MAN_STATE_STOP) {
        man_document_disconnect (man);
        return;
    }

    error = yelp_transform_get_error (transform);
    yelp_document_error_pending ((YelpDocument *) man, error);
    g_error_free (error);

    man_document_disconnect (man);
}

static void
transform_finalized (YelpManDocument  *man,
                     gpointer          transform)
{
    YelpManDocumentPrivate *priv = yelp_man_document_get_instance_private (man);
 
    if (priv->xmldoc)
	xmlFreeDoc (priv->xmldoc);
    priv->xmldoc = NULL;
}


/******************************************************************************/
/** Threaded ******************************************************************/

static void
man_document_process (YelpManDocument *man)
{
    YelpManDocumentPrivate *priv = yelp_man_document_get_instance_private (man);
    GFile *file = NULL;
    gchar *filepath = NULL;
    GError *error;
    gint  params_i = 0;
    gchar **params = NULL;
    YelpManParser *parser;

    file = yelp_uri_get_file (yelp_document_get_uri ((YelpDocument *) man));
    if (file == NULL) {
        error = g_error_new (YELP_ERROR, YELP_ERROR_NOT_FOUND,
                             _("The file does not exist."));
        yelp_document_error_pending ((YelpDocument *) man, error);
        g_error_free (error);
        goto done;
    }

    filepath = g_file_get_path (file);
    g_object_unref (file);
    if (!g_file_test (filepath, G_FILE_TEST_IS_REGULAR)) {
        error = g_error_new (YELP_ERROR, YELP_ERROR_NOT_FOUND,
                             _("The file ‘%s’ does not exist."),
                             filepath);
        yelp_document_error_pending ((YelpDocument *) man, error);
        g_error_free (error);
        goto done;
    }

    parser = yelp_man_parser_new ();
    priv->xmldoc = yelp_man_parser_parse_file (parser, filepath, &error);
    yelp_man_parser_free (parser);

    if (priv->xmldoc == NULL) {
	yelp_document_error_pending ((YelpDocument *) man, error);
    }

    g_mutex_lock (&priv->mutex);
    if (priv->state == MAN_STATE_STOP) {
	g_mutex_unlock (&priv->mutex);
	goto done;
    }

    priv->transform = yelp_transform_new (STYLESHEET);
    priv->chunk_ready =
        g_signal_connect (priv->transform, "chunk-ready",
                          (GCallback) transform_chunk_ready,
                          man);
    priv->finished =
        g_signal_connect (priv->transform, "finished",
                          (GCallback) transform_finished,
                          man);
    priv->error =
        g_signal_connect (priv->transform, "error",
                          (GCallback) transform_error,
                          man);

    params = yelp_settings_get_all_params (yelp_settings_get_default (), 0, &params_i);

    priv->transform_running = TRUE;
    yelp_transform_start (priv->transform,
                          priv->xmldoc,
                          NULL,
			  (const gchar * const *) params);
    g_strfreev (params);
    g_mutex_unlock (&priv->mutex);

 done:
    g_free (filepath);
    priv->process_running = FALSE;
    g_object_unref (man);
}

static void
man_document_disconnect (YelpManDocument *man)
{
    YelpManDocumentPrivate *priv = yelp_man_document_get_instance_private (man);
    if (priv->chunk_ready) {
        g_signal_handler_disconnect (priv->transform, priv->chunk_ready);
        priv->chunk_ready = 0;
    }
    if (priv->finished) {
        g_signal_handler_disconnect (priv->transform, priv->finished);
        priv->finished = 0;
    }
    if (priv->error) {
        g_signal_handler_disconnect (priv->transform, priv->error);
        priv->error = 0;
    }
    yelp_transform_cancel (priv->transform);
    g_object_unref (priv->transform);
    priv->transform = NULL;
    priv->transform_running = FALSE;
}
