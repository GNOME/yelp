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
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xinclude.h>

#include "yelp-docbook-document.h"
#include "yelp-error.h"
#include "yelp-settings.h"
#include "yelp-storage.h"
#include "yelp-transform.h"
#include "yelp-debug.h"

#define STYLESHEET DATADIR"/yelp/xslt/db2html.xsl"
#define DEFAULT_CATALOG "file:///etc/xml/catalog"
#define YELP_CATALOG "file://"DATADIR"/yelp/dtd/catalog"

typedef enum {
    DOCBOOK_STATE_BLANK,   /* Brand new, run transform as needed */
    DOCBOOK_STATE_PARSING, /* Parsing/transforming document, please wait */
    DOCBOOK_STATE_PARSED,  /* All done, if we ain't got it, it ain't here */
    DOCBOOK_STATE_STOP     /* Stop everything now, object to be disposed */
} DocbookState;

enum {
    DOCBOOK_COLUMN_ID,
    DOCBOOK_COLUMN_TITLE
};

static void           yelp_docbook_document_dispose         (GObject                  *object);
static void           yelp_docbook_document_finalize        (GObject                  *object);

static void           docbook_index             (YelpDocument         *document);
static gboolean       docbook_request_page      (YelpDocument         *document,
                                                 const gchar          *page_id,
                                                 GCancellable         *cancellable,
                                                 YelpDocumentCallback  callback,
                                                 gpointer              user_data,
                                                 GDestroyNotify        notify);

static void           docbook_process           (YelpDocbookDocument  *docbook);
static void           docbook_disconnect        (YelpDocbookDocument  *docbook);
static gboolean       docbook_reload            (YelpDocbookDocument  *docbook);
static void           docbook_monitor_changed   (GFileMonitor         *monitor,
                                                 GFile                *file,
                                                 GFile                *other_file,
                                                 GFileMonitorEvent     event_type,
                                                 YelpDocbookDocument  *docbook);

static void           docbook_walk              (YelpDocbookDocument  *docbook);
static gboolean       docbook_walk_chunkQ       (YelpDocbookDocument  *docbook,
                                                 xmlNodePtr            cur,
                                                 gint                  depth,
                                                 gint                  max_depth);
static gboolean       docbook_walk_divisionQ    (YelpDocbookDocument  *docbook,
                                                 xmlNodePtr            cur);
static gchar *        docbook_walk_get_title    (YelpDocbookDocument  *docbook,
                                                 xmlNodePtr            cur);
static gchar *        docbook_walk_get_keywords (YelpDocbookDocument  *docbook,
                                                 xmlNodePtr            cur);

static void           transform_chunk_ready     (YelpTransform        *transform,
                                                 gchar                *chunk_id,
                                                 YelpDocbookDocument  *docbook);
static void           transform_finished        (YelpTransform        *transform,
                                                 YelpDocbookDocument  *docbook);
static void           transform_error           (YelpTransform        *transform,
                                                 YelpDocbookDocument  *docbook);
static void           transform_finalized       (YelpDocbookDocument  *docbook,
                                                 gpointer              transform);

typedef struct _YelpDocbookDocumentPrivate  YelpDocbookDocumentPrivate;
struct _YelpDocbookDocumentPrivate {
    DocbookState   state;

    GMutex         mutex;
    GThread       *thread;

    GThread       *index;
    gboolean       index_running;

    gboolean       process_running;
    gboolean       transform_running;

    YelpTransform *transform;
    guint          chunk_ready;
    guint          finished;
    guint          error;

    xmlDocPtr     xmldoc;
    xmlNodePtr    xmlcur;
    gint          max_depth;
    gint          cur_depth;
    gchar        *cur_page_id;
    gchar        *cur_prev_id;
    gchar        *root_id;

    GFileMonitor **monitors;
    gint64         reload_time;

    GHashTable   *autoids;
};

G_DEFINE_TYPE_WITH_PRIVATE (YelpDocbookDocument, yelp_docbook_document, YELP_TYPE_DOCUMENT)

/******************************************************************************/

static void
yelp_docbook_document_class_init (YelpDocbookDocumentClass *klass)
{
    GObjectClass      *object_class   = G_OBJECT_CLASS (klass);
    YelpDocumentClass *document_class = YELP_DOCUMENT_CLASS (klass);
    const gchar *catalog = g_getenv ("XML_CATALOG_FILES");

    /* We ship a faux DocBook catalog. It just contains the common entity
     * definitions. Documents can use the named entities they expect to
     * be able to use, but we don't have to depend on docbook-dtds.
     */
    if (catalog == NULL)
        catalog = DEFAULT_CATALOG;
    if (!strstr(catalog, YELP_CATALOG)) {
        gchar *newcat = g_strconcat (YELP_CATALOG, " ", catalog, NULL);
        g_setenv ("XML_CATALOG_FILES", newcat, TRUE);
        g_free (newcat);
    }

    object_class->dispose = yelp_docbook_document_dispose;
    object_class->finalize = yelp_docbook_document_finalize;

    document_class->index = docbook_index;
    document_class->request_page = docbook_request_page;
}

static void
yelp_docbook_document_init (YelpDocbookDocument *docbook)
{
    YelpDocbookDocumentPrivate *priv = yelp_docbook_document_get_instance_private (docbook);

    priv->state = DOCBOOK_STATE_BLANK;
    priv->autoids = NULL;

    g_mutex_init (&priv->mutex);
}

static void
yelp_docbook_document_dispose (GObject *object)
{
    gint i;
    YelpDocbookDocumentPrivate *priv =
        yelp_docbook_document_get_instance_private (YELP_DOCBOOK_DOCUMENT (object));

    if (priv->monitors != NULL) {
        for (i = 0; priv->monitors[i]; i++) {
            g_object_unref (priv->monitors[i]);
        }
        g_free (priv->monitors);
        priv->monitors = NULL;
    }

    G_OBJECT_CLASS (yelp_docbook_document_parent_class)->dispose (object);
}

static void
yelp_docbook_document_finalize (GObject *object)
{
    YelpDocbookDocumentPrivate *priv =
        yelp_docbook_document_get_instance_private (YELP_DOCBOOK_DOCUMENT (object));

    if (priv->xmldoc)
        xmlFreeDoc (priv->xmldoc);

    g_free (priv->cur_page_id);
    g_free (priv->cur_prev_id);
    g_free (priv->root_id);

    g_hash_table_destroy (priv->autoids);

    g_mutex_clear (&priv->mutex);

    G_OBJECT_CLASS (yelp_docbook_document_parent_class)->finalize (object);
}

/******************************************************************************/

YelpDocument *
yelp_docbook_document_new (YelpUri *uri)
{
    YelpDocbookDocument *docbook;
    YelpDocbookDocumentPrivate *priv;
    gchar **path;
    gint path_i;

    g_return_val_if_fail (uri != NULL, NULL);

    docbook = (YelpDocbookDocument *) g_object_new (YELP_TYPE_DOCBOOK_DOCUMENT,
                                                    "document-uri", uri,
                                                    NULL);
    priv = yelp_docbook_document_get_instance_private (docbook);

    path = yelp_uri_get_search_path (uri);
    priv->monitors = g_new0 (GFileMonitor*, g_strv_length (path) + 1);
    for (path_i = 0; path[path_i]; path_i++) {
        GFile *file;
        file = g_file_new_for_path (path[path_i]);
        priv->monitors[path_i] = g_file_monitor (file,
                                                 G_FILE_MONITOR_WATCH_MOVES,
                                                 NULL, NULL);
        g_signal_connect (priv->monitors[path_i], "changed",
                          G_CALLBACK (docbook_monitor_changed),
                          docbook);
        g_object_unref (file);
    }
    g_strfreev (path);

    g_signal_connect_swapped (yelp_settings_get_default (),
                              "colors-changed",
                              G_CALLBACK (docbook_reload),
                              docbook);

    return (YelpDocument *) docbook;
}

/******************************************************************************/

static gboolean
docbook_request_page (YelpDocument         *document,
                      const gchar          *page_id,
                      GCancellable         *cancellable,
                      YelpDocumentCallback  callback,
                      gpointer              user_data,
                      GDestroyNotify        notify)
{
    YelpDocbookDocumentPrivate *priv =
        yelp_docbook_document_get_instance_private (YELP_DOCBOOK_DOCUMENT (document));
    gchar *docuri;
    GError *error;
    gboolean handled;

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "    page_id=\"%s\"\n", page_id);

    if (page_id == NULL)
        page_id = "//index";

    handled =
        YELP_DOCUMENT_CLASS (yelp_docbook_document_parent_class)->request_page (document,
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
    case DOCBOOK_STATE_BLANK:
        priv->state = DOCBOOK_STATE_PARSING;
        priv->process_running = TRUE;
        g_object_ref (document);
        priv->thread = g_thread_new ("docbook-page",
                                     (GThreadFunc)(GCallback) docbook_process,
                                     document);
        break;
    case DOCBOOK_STATE_PARSING:
        break;
    case DOCBOOK_STATE_PARSED:
    case DOCBOOK_STATE_STOP:
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

static void
docbook_process (YelpDocbookDocument *docbook)
{
    YelpDocbookDocumentPrivate *priv = yelp_docbook_document_get_instance_private (docbook);
    YelpDocument *document = YELP_DOCUMENT (docbook);
    GFile *file = NULL;
    gchar *filepath = NULL;
    xmlDocPtr xmldoc = NULL;
    xmlNodePtr xmlcur = NULL;
    xmlChar *id = NULL;
    xmlParserCtxtPtr parserCtxt = NULL;
    GError *error;
    gint  params_i = 0;
    gchar **params = NULL;

    debug_print (DB_FUNCTION, "entering\n");

    file = yelp_uri_get_file (yelp_document_get_uri (document));
    if (file == NULL) {
        error = g_error_new (YELP_ERROR, YELP_ERROR_NOT_FOUND,
                             _("The file does not exist."));
        yelp_document_error_pending (document, error);
        g_error_free (error);
        goto done;
    }

    filepath = g_file_get_path (file);
    g_object_unref (file);
    if (!g_file_test (filepath, G_FILE_TEST_IS_REGULAR)) {
        error = g_error_new (YELP_ERROR, YELP_ERROR_NOT_FOUND,
                             _("The file ‘%s’ does not exist."),
                             filepath);
        yelp_document_error_pending (document, error);
        g_error_free (error);
        goto done;
    }

    parserCtxt = xmlNewParserCtxt ();
    xmldoc = xmlCtxtReadFile (parserCtxt,
                              filepath, NULL,
                              XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
                              XML_PARSE_NOENT   | XML_PARSE_NONET   );

    if (xmldoc)
        xmlcur = xmlDocGetRootElement (xmldoc);

    if (xmldoc == NULL || xmlcur == NULL) {
        error = g_error_new (YELP_ERROR, YELP_ERROR_PROCESSING,
                             _("The file ‘%s’ could not be parsed because it is"
                               " not a well-formed XML document."),
                             filepath);
        yelp_document_error_pending (document, error);
        g_error_free (error);
        goto done;
    }

    if (xmlXIncludeProcessFlags (xmldoc,
                                 XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
                                 XML_PARSE_NOENT   | XML_PARSE_NONET   )
        < 0) {
        error = g_error_new (YELP_ERROR, YELP_ERROR_PROCESSING,
                             _("The file ‘%s’ could not be parsed because"
                               " one or more of its included files is not"
                               " a well-formed XML document."),
                             filepath);
        yelp_document_error_pending (document, error);
        g_error_free (error);
        goto done;
    }

    g_mutex_lock (&priv->mutex);
    if (!xmlStrcmp (xmlDocGetRootElement (xmldoc)->name, BAD_CAST "book"))
        priv->max_depth = 2;
    else
        priv->max_depth = 1;

    priv->xmldoc = xmldoc;
    priv->xmlcur = xmlcur;

    id = xmlGetProp (priv->xmlcur, BAD_CAST "id");
    if (!id)
        id = xmlGetNsProp (priv->xmlcur, BAD_CAST "id", XML_XML_NAMESPACE);

    if (id) {
        priv->root_id = g_strdup ((const gchar *) id);
        yelp_document_set_page_id (document, NULL, (gchar *) id);
        yelp_document_set_page_id (document, "//index", (gchar *) id);
    }
    else {
        priv->root_id = g_strdup ("//index");
        yelp_document_set_page_id (document, NULL, "//index");
        /* add the id attribute to the root element with value "index"
         * so when we try to load the document later, it doesn't fail */
        if (priv->xmlcur->ns)
            xmlSetProp (priv->xmlcur, BAD_CAST "xml:id", BAD_CAST "//index");
        else
            xmlSetProp (priv->xmlcur, BAD_CAST "id", BAD_CAST "//index");
    }
    yelp_document_set_root_id (document, priv->root_id, priv->root_id);
    g_mutex_unlock (&priv->mutex);

    g_mutex_lock (&priv->mutex);
    if (priv->state == DOCBOOK_STATE_STOP) {
        g_mutex_unlock (&priv->mutex);
        goto done;
    }
    g_mutex_unlock (&priv->mutex);

    docbook_walk (docbook);

    g_mutex_lock (&priv->mutex);
    if (priv->state == DOCBOOK_STATE_STOP) {
        g_mutex_unlock (&priv->mutex);
        goto done;
    }

    priv->state = DOCBOOK_STATE_PARSED;

    priv->transform = yelp_transform_new (STYLESHEET);
    priv->chunk_ready =
        g_signal_connect (priv->transform, "chunk-ready",
                          (GCallback) transform_chunk_ready,
                          docbook);
    priv->finished =
        g_signal_connect (priv->transform, "finished",
                          (GCallback) transform_finished,
                          docbook);
    priv->error =
        g_signal_connect (priv->transform, "error",
                          (GCallback) transform_error,
                          docbook);

    params = yelp_settings_get_all_params (yelp_settings_get_default (), 2, &params_i);
    params[params_i++] = g_strdup ("db.chunk.max_depth");
    params[params_i++] = g_strdup_printf ("%i", priv->max_depth);
    params[params_i] = NULL;

    priv->transform_running = TRUE;
    yelp_transform_start (priv->transform,
                          priv->xmldoc,
                          NULL,
			  (const gchar * const *) params);
    g_strfreev (params);
    g_mutex_unlock (&priv->mutex);

 done:
    g_free (filepath);
    if (id)
        xmlFree (id);
    if (parserCtxt)
        xmlFreeParserCtxt (parserCtxt);

    priv->process_running = FALSE;
    g_object_unref (docbook);
}

static void
docbook_disconnect (YelpDocbookDocument *docbook)
{
    YelpDocbookDocumentPrivate *priv = yelp_docbook_document_get_instance_private (docbook);
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

static gboolean
docbook_reload (YelpDocbookDocument *docbook)
{
    YelpDocbookDocumentPrivate *priv = yelp_docbook_document_get_instance_private (docbook);

    if (priv->index_running || priv->process_running || priv->transform_running)
        return TRUE;

    g_mutex_lock (&priv->mutex);

    priv->reload_time = g_get_monotonic_time();

    yelp_document_clear_contents (YELP_DOCUMENT (docbook));

    priv->state = DOCBOOK_STATE_PARSING;
    priv->process_running = TRUE;
    g_object_ref (docbook);
    priv->thread = g_thread_new ("docbook-reload",
                                 (GThreadFunc)(GCallback) docbook_process,
                                 docbook);

    g_mutex_unlock (&priv->mutex);

    return FALSE;
}

static void
docbook_monitor_changed   (GFileMonitor         *monitor,
                           GFile                *file,
                           GFile                *other_file,
                           GFileMonitorEvent     event_type,
                           YelpDocbookDocument  *docbook)
{
    YelpDocbookDocumentPrivate *priv = yelp_docbook_document_get_instance_private (docbook);

    if (g_get_monotonic_time() - priv->reload_time < 1000)
        return;

    if (priv->index_running || priv->process_running || priv->transform_running) {
        g_timeout_add_seconds (1, (GSourceFunc) docbook_reload, docbook);
        return;
    }

    docbook_reload (docbook);
}

/******************************************************************************/

static void
docbook_walk (YelpDocbookDocument *docbook)
{
    static       gint autoid = 0;
    gchar        autoidstr[20];
    xmlChar     *id = NULL;
    xmlChar     *title = NULL;
    xmlChar     *keywords = NULL;
    xmlNodePtr   cur, old_cur;
    gboolean chunkQ;
    YelpDocbookDocumentPrivate *priv = yelp_docbook_document_get_instance_private (docbook);
    YelpDocument *document = YELP_DOCUMENT (docbook);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_DEBUG, "  priv->xmlcur->name: %s\n", priv->xmlcur->name);

    /* Check for the db.chunk.max_depth PI and set max chunk depth */
    if (priv->cur_depth == 0)
        for (cur = priv->xmlcur; cur; cur = cur->prev)
            if (cur->type == XML_PI_NODE)
                if (!xmlStrcmp (cur->name, (const xmlChar *) "db.chunk.max_depth")) {
                    gint max = atoi ((gchar *) cur->content);
                    if (max)
                        priv->max_depth = max;
                    break;
                }

    id = xmlGetProp (priv->xmlcur, BAD_CAST "id");
    if (!id)
        id = xmlGetNsProp (priv->xmlcur, BAD_CAST "id", XML_XML_NAMESPACE);

    if (docbook_walk_divisionQ (docbook, priv->xmlcur) && !id) {
        /* If id attribute is not present, autogenerate a
         * unique value, and insert it into the in-memory tree */
        g_snprintf (autoidstr, 20, "//yelp-autoid-%d", ++autoid);
        if (priv->xmlcur->ns) {
            xmlSetProp (priv->xmlcur, BAD_CAST "xml:id", BAD_CAST autoidstr);
            id = xmlGetNsProp (priv->xmlcur, BAD_CAST "id", XML_XML_NAMESPACE);
        }
        else {
            xmlSetProp (priv->xmlcur, BAD_CAST "id", BAD_CAST autoidstr);
            id = xmlGetProp (priv->xmlcur, BAD_CAST "id");
        }

        if (!priv->autoids)
            priv->autoids = g_hash_table_new_full (g_str_hash, g_str_equal, xmlFree, xmlFree);
        g_hash_table_insert (priv->autoids, xmlGetNodePath(priv->xmlcur), xmlStrdup (id));
    }

    if (docbook_walk_chunkQ (docbook, priv->xmlcur, priv->cur_depth, priv->max_depth)) {
        title = BAD_CAST docbook_walk_get_title (docbook, priv->xmlcur);
        keywords = BAD_CAST docbook_walk_get_keywords (docbook, priv->xmlcur);

        debug_print (DB_DEBUG, "  id: \"%s\"\n", id);
        debug_print (DB_DEBUG, "  title: \"%s\"\n", title);

        yelp_document_set_page_title (document, (gchar *) id, (gchar *) title);
        yelp_document_set_page_keywords (document, (gchar *) id, (gchar *) keywords);

        if (priv->cur_prev_id) {
            yelp_document_set_prev_id (document, (gchar *) id, priv->cur_prev_id);
            yelp_document_set_next_id (document, priv->cur_prev_id, (gchar *) id);
            g_free (priv->cur_prev_id);
        }
        priv->cur_prev_id = g_strdup ((gchar *) id);

        if (priv->cur_page_id)
            yelp_document_set_up_id (document, (gchar *) id, priv->cur_page_id);
        priv->cur_page_id = g_strdup ((gchar *) id);
    }

    old_cur = priv->xmlcur;
    priv->cur_depth++;
    if (id) {
        yelp_document_set_root_id (document, (gchar *) id, priv->root_id);
        yelp_document_set_page_id (document, (gchar *) id, priv->cur_page_id);
    }

    chunkQ = docbook_walk_chunkQ (docbook, priv->xmlcur, priv->cur_depth, priv->max_depth);
    if (chunkQ)
        yelp_document_signal (YELP_DOCUMENT (docbook),
                              priv->cur_page_id,
                              YELP_DOCUMENT_SIGNAL_INFO,
                              NULL);

    for (cur = priv->xmlcur->children; cur; cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE) {
            priv->xmlcur = cur;
            docbook_walk (docbook);
        }
    }
    priv->cur_depth--;
    priv->xmlcur = old_cur;

    if (priv->cur_depth == 0) {
        g_free (priv->cur_prev_id);
        priv->cur_prev_id = NULL;

        g_free (priv->cur_page_id);
        priv->cur_page_id = NULL;
    }

    if (id != NULL)
        xmlFree (id);
    if (title != NULL)
        xmlFree (title);
    if (keywords != NULL)
        xmlFree (keywords);
}

static gboolean
docbook_walk_chunkQ (YelpDocbookDocument *docbook,
                     xmlNodePtr           cur,
                     gint                 cur_depth,
                     gint                 max_depth)
{
    if (cur_depth <= max_depth)
        return docbook_walk_divisionQ (docbook, cur);
    else
        return FALSE;
}

static gboolean
docbook_walk_divisionQ (YelpDocbookDocument *docbook, xmlNodePtr node)
{
    return (!xmlStrcmp (node->name, (const xmlChar *) "appendix")     ||
            !xmlStrcmp (node->name, (const xmlChar *) "article")      ||
            !xmlStrcmp (node->name, (const xmlChar *) "book")         ||
            !xmlStrcmp (node->name, (const xmlChar *) "bibliography") ||
            !xmlStrcmp (node->name, (const xmlChar *) "bibliodiv")    ||
            !xmlStrcmp (node->name, (const xmlChar *) "chapter")      ||
            !xmlStrcmp (node->name, (const xmlChar *) "colophon")     ||
            !xmlStrcmp (node->name, (const xmlChar *) "dedication")   ||
            !xmlStrcmp (node->name, (const xmlChar *) "glossary")     ||
            !xmlStrcmp (node->name, (const xmlChar *) "glossdiv")     ||
            !xmlStrcmp (node->name, (const xmlChar *) "lot")          ||
            !xmlStrcmp (node->name, (const xmlChar *) "index")        ||
            !xmlStrcmp (node->name, (const xmlChar *) "part")         ||
            !xmlStrcmp (node->name, (const xmlChar *) "preface")      ||
            !xmlStrcmp (node->name, (const xmlChar *) "reference")    ||
            !xmlStrcmp (node->name, (const xmlChar *) "refentry")     ||
            !xmlStrcmp (node->name, (const xmlChar *) "sect1")        ||
            !xmlStrcmp (node->name, (const xmlChar *) "sect2")        ||
            !xmlStrcmp (node->name, (const xmlChar *) "sect3")        ||
            !xmlStrcmp (node->name, (const xmlChar *) "sect4")        ||
            !xmlStrcmp (node->name, (const xmlChar *) "sect5")        ||
            !xmlStrcmp (node->name, (const xmlChar *) "section")      ||
            !xmlStrcmp (node->name, (const xmlChar *) "set")          ||
            !xmlStrcmp (node->name, (const xmlChar *) "setindex")     ||
            !xmlStrcmp (node->name, (const xmlChar *) "simplesect")   ||
            !xmlStrcmp (node->name, (const xmlChar *) "toc")          );
}

static gchar *
docbook_walk_get_title (YelpDocbookDocument *docbook,
                        xmlNodePtr           cur)
{
    gchar *infoname = NULL;
    xmlNodePtr child = NULL;
    xmlNodePtr title = NULL;
    xmlNodePtr title_tmp = NULL;

    if (!xmlStrcmp (cur->name, BAD_CAST "refentry")) {
        /* The title for a refentry element can come from the following:
         *   refmeta/refentrytitle
         *   refentryinfo/title[abbrev]
         *   refnamediv/refname
         * We take the first one we find.
         */
        for (child = cur->children; child; child = child->next) {
            if (!xmlStrcmp (child->name, BAD_CAST "refmeta")) {
                for (title = child->children; title; title = title->next) {
                    if (!xmlStrcmp (title->name, BAD_CAST "refentrytitle"))
                        break;
                }
                if (title)
                    goto done;
            }
            else if (!xmlStrcmp (child->name, BAD_CAST "refentryinfo")) {
                for (title = child->children; title; title = title->next) {
                    if (!xmlStrcmp (title->name, BAD_CAST "titleabbrev"))
                        break;
                    else if (!xmlStrcmp (title->name, BAD_CAST "title"))
                        title_tmp = title;
                }
                if (title)
                    goto done;
                else if (title_tmp) {
                    title = title_tmp;
                    goto done;
                }
            }
            else if (!xmlStrcmp (child->name, BAD_CAST "refnamediv")) {
                for (title = child->children; title; title = title->next) {
                    if (!xmlStrcmp (title->name, BAD_CAST "refname"))
                        break;
                    else if (!xmlStrcmp (title->name, BAD_CAST "refpurpose")) {
                        title = NULL;
                        break;
                    }
                }
                if (title)
                    goto done;
            }
            else if (!xmlStrncmp (child->name, BAD_CAST "refsect", 7))
                break;
        }
    }
    else {
        /* The title for other elements appears in the following:
         *   title[abbrev]
         *   *info/title[abbrev]
         *   blockinfo/title[abbrev]
         *   objectinfo/title[abbrev]
         * We take them in that order.
         */
        xmlNodePtr infos[3] = {NULL, NULL, NULL};
        int i;

        infoname = g_strdup_printf ("%sinfo", cur->name);

        for (child = cur->children; child; child = child->next) {
            if (!xmlStrcmp (child->name, BAD_CAST "titleabbrev")) {
                title = child;
                goto done;
            }
            else if (!xmlStrcmp (child->name, BAD_CAST "title"))
                title_tmp = child;
            else if (!xmlStrcmp (child->name, BAD_CAST "info"))
                infos[0] = child;
            else if (!xmlStrcmp (child->name, BAD_CAST infoname))
                infos[0] = child;
            else if (!xmlStrcmp (child->name, BAD_CAST "blockinfo"))
                infos[1] = child;
            else if (!xmlStrcmp (child->name, BAD_CAST "objectinfo"))
                infos[2] = child;
        }

        if (title_tmp) {
            title = title_tmp;
            goto done;
        }

        for (i = 0; i < 3; i++) {
            child = infos[i];
            if (child) {
                for (title = child->children; title; title = title->next) {
                    if (!xmlStrcmp (title->name, BAD_CAST "titleabbrev"))
                        goto done;
                    else if (!xmlStrcmp (title->name, BAD_CAST "title"))
                        title_tmp = title;
                }
                if (title_tmp) {
                    title = title_tmp;
                    goto done;
                }
            }
        }
    }

 done:
    g_free (infoname);

    if (title) {
        xmlChar *title_s = xmlNodeGetContent (title);
        gchar *ret = g_strdup ((const gchar *) title_s);
        xmlFree (title_s);
        return ret;
    }
    else
        return g_strdup (_("Unknown"));
}

static gchar *
docbook_walk_get_keywords (YelpDocbookDocument *docbook,
                           xmlNodePtr           cur)
{
    xmlNodePtr info, keywordset, keyword;
    GString *ret = NULL;

    for (info = cur->children; info; info = info->next) {
        if (g_str_has_suffix ((const gchar *) info->name, "info")) {
            for (keywordset = info->children; keywordset; keywordset = keywordset->next) {
                if (!xmlStrcmp (keywordset->name, BAD_CAST "keywordset")) {
                    for (keyword = keywordset->children; keyword; keyword = keyword->next) {
                        if (!xmlStrcmp (keyword->name, BAD_CAST "keyword")) {
                            xmlChar *content;
                            if (ret)
                                g_string_append(ret, ", ");
                            else
                                ret = g_string_new ("");
                            /* FIXME: try this with just ->children->text */
                            content = xmlNodeGetContent (keyword);
                            g_string_append (ret, (gchar *) content);
                            xmlFree (content);
                        }
                    }
                }
            }
            break;
        }
    }

    if (ret)
        return g_string_free (ret, FALSE);
    else
        return NULL;
}

/******************************************************************************/

static void
transform_chunk_ready (YelpTransform       *transform,
                       gchar               *chunk_id,
                       YelpDocbookDocument *docbook)
{
    YelpDocbookDocumentPrivate *priv = yelp_docbook_document_get_instance_private (docbook);
    gchar *content;

    debug_print (DB_FUNCTION, "entering\n");
    g_assert (transform == priv->transform);

    if (priv->state == DOCBOOK_STATE_STOP) {
        docbook_disconnect (docbook);
        return;
    }

    content = yelp_transform_take_chunk (transform, chunk_id);
    yelp_document_give_contents (YELP_DOCUMENT (docbook),
                                 chunk_id,
                                 content,
                                 "application/xhtml+xml");

    yelp_document_signal (YELP_DOCUMENT (docbook),
                          chunk_id,
                          YELP_DOCUMENT_SIGNAL_CONTENTS,
                          NULL);
}

static void
transform_finished (YelpTransform       *transform,
                    YelpDocbookDocument *docbook)
{
    YelpDocbookDocumentPrivate *priv = yelp_docbook_document_get_instance_private (docbook);
    YelpDocument *document = YELP_DOCUMENT (docbook);
    gchar *docuri;
    GError *error;

    debug_print (DB_FUNCTION, "entering\n");
    g_assert (transform == priv->transform);

    if (priv->state == DOCBOOK_STATE_STOP) {
        docbook_disconnect (docbook);
        return;
    }

    docbook_disconnect (docbook);

    /* We want to free priv->xmldoc, but we can't free it before transform
       is finalized.   Otherwise, we could crash when YelpTransform frees
       its libxslt resources.
     */
    g_object_weak_ref ((GObject *) transform,
                       (GWeakNotify) transform_finalized,
                       docbook);

    docuri = yelp_uri_get_document_uri (yelp_document_get_uri (document));
    error = g_error_new (YELP_ERROR, YELP_ERROR_NOT_FOUND,
                         _("The requested page was not found in the document ‘%s’."),
                         docuri);
    g_free (docuri);
    yelp_document_error_pending ((YelpDocument *) docbook, error);
    g_error_free (error);
}

static void
transform_error (YelpTransform       *transform,
                 YelpDocbookDocument *docbook)
{
    YelpDocbookDocumentPrivate *priv = yelp_docbook_document_get_instance_private (docbook);
    GError *error;

    debug_print (DB_FUNCTION, "entering\n");
    g_assert (transform == priv->transform);

    if (priv->state == DOCBOOK_STATE_STOP) {
        docbook_disconnect (docbook);
        return;
    }

    error = yelp_transform_get_error (transform);
    yelp_document_error_pending ((YelpDocument *) docbook, error);
    g_error_free (error);

    docbook_disconnect (docbook);
}

static void
transform_finalized (YelpDocbookDocument *docbook,
                     gpointer             transform)
{
    YelpDocbookDocumentPrivate *priv = yelp_docbook_document_get_instance_private (docbook);

    debug_print (DB_FUNCTION, "entering\n");

    if (priv->xmldoc)
	xmlFreeDoc (priv->xmldoc);
    priv->xmldoc = NULL;
}

/******************************************************************************/

static gboolean
docbook_index_done (YelpDocbookDocument *docbook)
{
    g_object_set (docbook, "indexed", TRUE, NULL);
    g_object_unref (docbook);
    return FALSE;
}

typedef struct {
    YelpDocbookDocument *docbook;
    xmlDocPtr doc;
    xmlNodePtr cur;
    gchar *doc_uri;
    GString *str;
    gint depth;
    gint max_depth;
    gboolean in_info;
} DocbookIndexData;

static void
docbook_index_node (DocbookIndexData *index)
{
    xmlNodePtr oldcur, child;

    if ((g_str_equal (index->cur->parent->name, "menuchoice") ||
         g_str_equal (index->cur->parent->name, "keycombo")) &&
        index->cur->prev != NULL) {
        g_string_append_c (index->str, ' ');
    }
    if (index->cur->type == XML_TEXT_NODE) {
        g_string_append (index->str, (const gchar *) index->cur->content);
        return;
    }
    if (index->cur->type != XML_ELEMENT_NODE) {
        return;
    }
    if (g_str_equal (index->cur->name, "remark")) {
        return;
    }
    if (g_str_has_suffix ((const gchar *) index->cur->name, "info")) {
        return;
    }
    oldcur = index->cur;
    for (child = index->cur->children; child; child = child->next) {
        index->cur = child;
        docbook_index_node (index);
        index->cur = oldcur;
    }
}

static void
docbook_index_chunk (DocbookIndexData *index)
{
    xmlChar *id;
    xmlNodePtr child;
    gchar *title = NULL;
    gchar *keywords;
    GSList *chunks = NULL;
    YelpDocbookDocumentPrivate *priv = yelp_docbook_document_get_instance_private (index->docbook);

    id = xmlGetProp (index->cur, BAD_CAST "id");
    if (!id)
        id = xmlGetNsProp (index->cur, BAD_CAST "id", XML_XML_NAMESPACE);
    if (!id) {
        xmlChar *path = xmlGetNodePath (index->cur);
        id = g_hash_table_lookup (priv->autoids, path);
        if (id)
            id = xmlStrdup (id);
        xmlFree (path);
    }

    if (id != NULL) {
        title = docbook_walk_get_title (index->docbook, index->cur);
        if (index->cur->parent->parent == NULL)
            yelp_storage_set_root_title (yelp_storage_get_default (),
                                         index->doc_uri, title);
        index->str = g_string_new ("");
        keywords = docbook_walk_get_keywords (index->docbook, index->cur);
        if (keywords) {
            g_string_append (index->str, keywords);
            g_free (keywords);
        }
    }

    for (child = index->cur->children; child; child = child->next) {
        if (docbook_walk_chunkQ (index->docbook, child, index->depth, index->max_depth)) {
            chunks = g_slist_append (chunks, child);
        }
        else if (id != NULL) {
            xmlNodePtr oldcur = index->cur;
            index->cur = child;
            docbook_index_node (index);
            index->cur = oldcur;
        }
    }

    if (id != NULL) {
        YelpDocument *document = YELP_DOCUMENT (index->docbook);
        YelpUri *uri;
        gchar *full_uri, *tmp, *body;

        body = g_string_free (index->str, FALSE);
        index->str = NULL;

        tmp = g_strconcat ("xref:", id, NULL);
        uri = yelp_uri_new_relative (yelp_document_get_uri (document), tmp);
        g_free (tmp);
        yelp_uri_resolve_sync (uri);
        full_uri = yelp_uri_get_canonical_uri (uri);
        g_object_unref (uri);

        yelp_storage_update (yelp_storage_get_default (),
                             index->doc_uri, full_uri,
                             title, "", "yelp-page-symbolic",
                             body);
        if (index->cur->parent->parent == NULL)
            yelp_storage_set_root_title (yelp_storage_get_default (),
                                         index->doc_uri, title);
        g_free (full_uri);
        g_free (body);
        g_free (title);
        xmlFree (id);
    }

    index->depth++;
    while (chunks != NULL) {
        xmlNodePtr oldcur = index->cur;
        index->cur = (xmlNodePtr) chunks->data;
        docbook_index_chunk(index);
        index->cur = oldcur;
        chunks = g_slist_delete_link (chunks, chunks);
    }
    index->depth--;
}

static void
docbook_index_threaded (YelpDocbookDocument *docbook)
{
    DocbookIndexData *index = NULL;
    xmlParserCtxtPtr parserCtxt = NULL;
    GFile *file = NULL;
    gchar *filename = NULL;
    YelpUri *uri;
    YelpDocbookDocumentPrivate *priv = yelp_docbook_document_get_instance_private (docbook);

    uri = yelp_document_get_uri (YELP_DOCUMENT (docbook));
    file = yelp_uri_get_file (uri);
    if (file == NULL)
        goto done;
    filename = g_file_get_path (file);

    index = g_new0 (DocbookIndexData, 1);
    index->docbook = docbook;
    index->doc_uri = yelp_uri_get_document_uri (uri);

    parserCtxt = xmlNewParserCtxt ();
    index->doc = xmlCtxtReadFile (parserCtxt, filename, NULL,
                                  XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
                                  XML_PARSE_NOENT   | XML_PARSE_NONET   );
    if (index->doc == NULL)
        goto done;
    if (xmlXIncludeProcessFlags (index->doc,
                                 XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
                                 XML_PARSE_NOENT   | XML_PARSE_NONET   )
        < 0)
        goto done;

    index->cur = xmlDocGetRootElement (index->doc);
    index->depth = 0;
    if (!xmlStrcmp (index->cur->name, BAD_CAST "book"))
        index->max_depth = 2;
    else
        index->max_depth = 1;
    docbook_index_chunk (index);

 done:
    if (file != NULL)
        g_object_unref (file);
    if (filename != NULL)
        g_free (filename);
    if (index != NULL) {
        if (index->doc != NULL)
            xmlFreeDoc (index->doc);
        if (index->doc_uri != NULL)
            g_free (index->doc_uri);
        g_free (index);
    }
    if (parserCtxt != NULL)
        xmlFreeParserCtxt (parserCtxt);

    priv->index_running = FALSE;
    g_idle_add ((GSourceFunc) docbook_index_done, docbook);
}

static void
docbook_index (YelpDocument *document)
{
    YelpDocbookDocumentPrivate *priv;
    gboolean done;

    g_object_get (document, "indexed", &done, NULL);
    if (done)
        return;

    priv = yelp_docbook_document_get_instance_private (YELP_DOCBOOK_DOCUMENT (document));
    g_object_ref (document);
    priv->index = g_thread_new ("docbook-index",
                                (GThreadFunc)(GCallback) docbook_index_threaded,
                                document);
    priv->index_running = TRUE;
}
