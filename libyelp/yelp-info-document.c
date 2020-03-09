/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2007 Don Scorgie <dscorgie@svn.gnome.org>
 * Copyright (C) 2010-2020 Shaun McCance <shaunm@gnome.org>
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
 * Author: Don Scorgie <dscorgie@svn.gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libxml/tree.h>

#include "yelp-error.h"
#include "yelp-info-document.h"
#include "yelp-info-parser.h"
#include "yelp-transform.h"
#include "yelp-debug.h"
#include "yelp-settings.h"

#define STYLESHEET DATADIR"/yelp/xslt/info2html.xsl"

typedef enum {
    INFO_STATE_BLANK,   /* Brand new, run transform as needed */
    INFO_STATE_PARSING, /* Parsing/transforming document, please wait */
    INFO_STATE_PARSED,  /* All done, if we ain't got it, it ain't here */
    INFO_STATE_STOP     /* Stop everything now, object to be disposed */
} InfoState;

typedef struct _YelpInfoDocumentPrivate  YelpInfoDocumentPrivate;
struct _YelpInfoDocumentPrivate {
    InfoState    state;

    GMutex      mutex;
    GThread    *thread;

    xmlDocPtr   xmldoc;
    GtkTreeModel  *sections;

    gboolean    process_running;
    gboolean    transform_running;

    YelpTransform *transform;
    guint          chunk_ready;
    guint          finished;
    guint          error;

    gchar   *root_id;
    gchar   *visit_prev_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (YelpInfoDocument, yelp_info_document, YELP_TYPE_DOCUMENT)

static void           yelp_info_document_dispose          (GObject                *object);
static void           yelp_info_document_finalize         (GObject                *object);

/* YelpDocument */
static gboolean       info_request_page                   (YelpDocument         *document,
                                                           const gchar          *page_id,
                                                           GCancellable         *cancellable,
                                                           YelpDocumentCallback  callback,
                                                           gpointer              user_data,
                                                           GDestroyNotify        notify);

/* YelpTransform */
static void           transform_chunk_ready     (YelpTransform        *transform,
                                                 gchar                *chunk_id,
                                                 YelpInfoDocument     *info);
static void           transform_finished        (YelpTransform        *transform,
                                                 YelpInfoDocument     *info);
static void           transform_error           (YelpTransform        *transform,
                                                 YelpInfoDocument     *info);
static void           transform_finalized       (YelpInfoDocument     *info,
                                                 gpointer              transform);

static void           info_document_process     (YelpInfoDocument     *info);
static gboolean       info_sections_visit       (GtkTreeModel         *model,
                                                 GtkTreePath          *path,
                                                 GtkTreeIter          *iter,
                                                 YelpInfoDocument     *info);
static void           info_document_disconnect  (YelpInfoDocument     *info);


static void
yelp_info_document_class_init (YelpInfoDocumentClass *klass)
{
    GObjectClass      *object_class   = G_OBJECT_CLASS (klass);
    YelpDocumentClass *document_class = YELP_DOCUMENT_CLASS (klass);

    object_class->dispose = yelp_info_document_dispose;
    object_class->finalize = yelp_info_document_finalize;

    document_class->request_page = info_request_page;
}

static void
yelp_info_document_init (YelpInfoDocument *info)
{
    YelpInfoDocumentPrivate *priv = yelp_info_document_get_instance_private (info);

    priv->state = INFO_STATE_BLANK;
    priv->xmldoc = NULL;
    g_mutex_init (&priv->mutex);
}

static void
yelp_info_document_dispose (GObject *object)
{
    YelpInfoDocumentPrivate *priv =
        yelp_info_document_get_instance_private (YELP_INFO_DOCUMENT (object));

    if (priv->sections) {
        g_object_unref (priv->sections);
        priv->sections = NULL;
    }

    if (priv->transform) {
        g_object_unref (priv->transform);
        priv->transform = NULL;
    }

    G_OBJECT_CLASS (yelp_info_document_parent_class)->dispose (object);
}

static void
yelp_info_document_finalize (GObject *object)
{
    YelpInfoDocumentPrivate *priv =
        yelp_info_document_get_instance_private (YELP_INFO_DOCUMENT (object));

    if (priv->xmldoc)
        xmlFreeDoc (priv->xmldoc);

    g_free (priv->root_id);
    g_free (priv->visit_prev_id);

    g_mutex_clear (&priv->mutex);

    G_OBJECT_CLASS (yelp_info_document_parent_class)->finalize (object);
}

/******************************************************************************/

YelpDocument *
yelp_info_document_new (YelpUri *uri)
{
    g_return_val_if_fail (uri != NULL, NULL);

    return (YelpDocument *) g_object_new (YELP_TYPE_INFO_DOCUMENT,
                                          "document-uri", uri,
                                          NULL);
}


/******************************************************************************/
/** YelpDocument **************************************************************/

static gboolean
info_request_page (YelpDocument         *document,
                   const gchar          *page_id,
                   GCancellable         *cancellable,
                   YelpDocumentCallback  callback,
                   gpointer              user_data,
                   GDestroyNotify        notify)
{
    YelpInfoDocumentPrivate *priv =
        yelp_info_document_get_instance_private (YELP_INFO_DOCUMENT (document));
    gchar *docuri;
    GError *error;
    gboolean handled;

    if (page_id == NULL)
        page_id = priv->root_id;

    handled =
        YELP_DOCUMENT_CLASS (yelp_info_document_parent_class)->request_page (document,
                                                                             page_id,
                                                                             cancellable,
                                                                             callback,
                                                                             user_data,
                                                                             notify);
    if (handled) {
        return TRUE;
    }

    g_mutex_lock (&priv->mutex);

    switch (priv->state) {
    case INFO_STATE_BLANK:
	priv->state = INFO_STATE_PARSING;
	priv->process_running = TRUE;
        g_object_ref (document);
	priv->thread = g_thread_new ("info-page",
                                     (GThreadFunc)(GCallback) info_document_process,
                                     document);
	break;
    case INFO_STATE_PARSING:
	break;
    case INFO_STATE_PARSED:
    case INFO_STATE_STOP:
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
    return TRUE;
}


/******************************************************************************/
/** YelpTransform *************************************************************/

static void
transform_chunk_ready (YelpTransform    *transform,
                       gchar            *chunk_id,
                       YelpInfoDocument *info)
{
    YelpInfoDocumentPrivate *priv = yelp_info_document_get_instance_private (info);
    gchar *content;

    g_assert (transform == priv->transform);

    if (priv->state == INFO_STATE_STOP) {
        info_document_disconnect (info);
        return;
    }

    content = yelp_transform_take_chunk (transform, chunk_id);
    yelp_document_give_contents (YELP_DOCUMENT (info),
                                 chunk_id,
                                 content,
                                 "application/xhtml+xml");

    yelp_document_signal (YELP_DOCUMENT (info),
                          chunk_id,
                          YELP_DOCUMENT_SIGNAL_INFO,
                          NULL);
    yelp_document_signal (YELP_DOCUMENT (info),
                          chunk_id,
                          YELP_DOCUMENT_SIGNAL_CONTENTS,
                          NULL);
}

static void
transform_finished (YelpTransform    *transform,
                    YelpInfoDocument *info)
{
    YelpInfoDocumentPrivate *priv = yelp_info_document_get_instance_private (info);
    gchar *docuri;
    GError *error;

    g_assert (transform == priv->transform);

    if (priv->state == INFO_STATE_STOP) {
        info_document_disconnect (info);
        return;
    }

    info_document_disconnect (info);
    priv->state = INFO_STATE_PARSED;

    /* We want to free priv->xmldoc, but we can't free it before transform
       is finalized.   Otherwise, we could crash when YelpTransform frees
       its libxslt resources.
     */
    g_object_weak_ref ((GObject *) transform,
                       (GWeakNotify) transform_finalized,
                       info);

    docuri = yelp_uri_get_document_uri (yelp_document_get_uri ((YelpDocument *) info));
    error = g_error_new (YELP_ERROR, YELP_ERROR_NOT_FOUND,
                         _("The requested page was not found in the document ‘%s’."),
                         docuri);
    g_free (docuri);
    yelp_document_error_pending ((YelpDocument *) info, error);
    g_error_free (error);
}

static void
transform_error (YelpTransform    *transform,
                 YelpInfoDocument *info)
{
    YelpInfoDocumentPrivate *priv = yelp_info_document_get_instance_private (info);
    GError *error;

    g_assert (transform == priv->transform);

    if (priv->state == INFO_STATE_STOP) {
        info_document_disconnect (info);
        return;
    }

    error = yelp_transform_get_error (transform);
    yelp_document_error_pending ((YelpDocument *) info, error);
    g_error_free (error);

    info_document_disconnect (info);
}

static void
transform_finalized (YelpInfoDocument *info,
                     gpointer          transform)
{
    YelpInfoDocumentPrivate *priv = yelp_info_document_get_instance_private (info);
 
    if (priv->xmldoc)
	xmlFreeDoc (priv->xmldoc);
    priv->xmldoc = NULL;
}



/******************************************************************************/
/** Threaded ******************************************************************/

static void
info_document_process (YelpInfoDocument *info)
{
    YelpInfoDocumentPrivate *priv = yelp_info_document_get_instance_private (info);
    GFile *file = NULL;
    gchar *filepath = NULL;
    GError *error;
    gint  params_i = 0;
    gchar **params = NULL;

    file = yelp_uri_get_file (yelp_document_get_uri ((YelpDocument *) info));
    if (file == NULL) {
        error = g_error_new (YELP_ERROR, YELP_ERROR_NOT_FOUND,
                             _("The file does not exist."));
        yelp_document_error_pending ((YelpDocument *) info, error);
        g_error_free (error);
        goto done;
    }

    filepath = g_file_get_path (file);
    g_object_unref (file);
    if (!g_file_test (filepath, G_FILE_TEST_IS_REGULAR)) {
        error = g_error_new (YELP_ERROR, YELP_ERROR_NOT_FOUND,
                             _("The file ‘%s’ does not exist."),
                             filepath);
        yelp_document_error_pending ((YelpDocument *) info, error);
        g_error_free (error);
        goto done;
    }

    priv->sections = (GtkTreeModel *) yelp_info_parser_parse_file (filepath);
    gtk_tree_model_foreach (priv->sections,
                            (GtkTreeModelForeachFunc) info_sections_visit,
                            info);
    priv->xmldoc = yelp_info_parser_parse_tree ((GtkTreeStore *) priv->sections);

    if (priv->xmldoc == NULL) {
	error = g_error_new (YELP_ERROR, YELP_ERROR_PROCESSING,
                             _("The file ‘%s’ could not be parsed because it is"
                               " not a well-formed info page."),
                             filepath);
	yelp_document_error_pending ((YelpDocument *) info, error);
        goto done;
    }

    g_mutex_lock (&priv->mutex);
    if (priv->state == INFO_STATE_STOP) {
	g_mutex_unlock (&priv->mutex);
	goto done;
    }

    priv->transform = yelp_transform_new (STYLESHEET);
    priv->chunk_ready =
        g_signal_connect (priv->transform, "chunk-ready",
                          (GCallback) transform_chunk_ready,
                          info);
    priv->finished =
        g_signal_connect (priv->transform, "finished",
                          (GCallback) transform_finished,
                          info);
    priv->error =
        g_signal_connect (priv->transform, "error",
                          (GCallback) transform_error,
                          info);

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
    g_object_unref (info);
}

static gboolean
info_sections_visit (GtkTreeModel     *model,
                     GtkTreePath      *path,
                     GtkTreeIter      *iter,
                     YelpInfoDocument *info)
{
    YelpInfoDocumentPrivate *priv = yelp_info_document_get_instance_private (info);
    gchar *page_id, *title;

    gtk_tree_model_get (model, iter,
                        INFO_PARSER_COLUMN_PAGE_NO, &page_id,
                        INFO_PARSER_COLUMN_PAGE_NAME, &title,
                        -1);
    yelp_document_set_page_id ((YelpDocument *) info, page_id, page_id);
    yelp_document_set_page_title ((YelpDocument *) info, page_id, title);

    if (priv->root_id == NULL) {
        priv->root_id = g_strdup (page_id);
        yelp_document_set_page_id ((YelpDocument *) info, NULL, page_id);
    }
    yelp_document_set_root_id ((YelpDocument *) info, page_id, priv->root_id);

    if (priv->visit_prev_id != NULL) {
        yelp_document_set_prev_id ((YelpDocument *) info, page_id, priv->visit_prev_id);
        yelp_document_set_next_id ((YelpDocument *) info, priv->visit_prev_id, page_id);
        g_free (priv->visit_prev_id);
    }
    priv->visit_prev_id = page_id;
    g_free (title);
    return FALSE;
}

static void
info_document_disconnect (YelpInfoDocument *info)
{
    YelpInfoDocumentPrivate *priv = yelp_info_document_get_instance_private (info);
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
