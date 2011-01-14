/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2009 Shaun McCance  <shaunm@gnome.org>
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

static void           yelp_docbook_document_class_init      (YelpDocbookDocumentClass *klass);
static void           yelp_docbook_document_init            (YelpDocbookDocument      *docbook);
static void           yelp_docbook_document_dispose         (GObject                  *object);
static void           yelp_docbook_document_finalize        (GObject                  *object);

static gboolean       docbook_request_page      (YelpDocument         *document,
                                                 const gchar          *page_id,
                                                 GCancellable         *cancellable,
                                                 YelpDocumentCallback  callback,
                                                 gpointer              user_data);

static void           docbook_process           (YelpDocbookDocument  *docbook);
static void           docbook_disconnect        (YelpDocbookDocument  *docbook);

static void           docbook_walk              (YelpDocbookDocument  *docbook);
static gboolean       docbook_walk_chunkQ       (YelpDocbookDocument  *docbook);
static gboolean       docbook_walk_divisionQ    (YelpDocbookDocument  *docbook);
static gchar *        docbook_walk_get_title    (YelpDocbookDocument  *docbook);

static void           transform_chunk_ready     (YelpTransform        *transform,
                                                 gchar                *chunk_id,
                                                 YelpDocbookDocument  *docbook);
static void           transform_finished        (YelpTransform        *transform,
                                                 YelpDocbookDocument  *docbook);
static void           transform_error           (YelpTransform        *transform,
                                                 YelpDocbookDocument  *docbook);
static void           transform_finalized       (YelpDocbookDocument  *docbook,
                                                 gpointer              transform);

/* FIXME */
#if 0
/* static gpointer       docbook_get_sections    (YelpDocument        *document); */
#endif

G_DEFINE_TYPE (YelpDocbookDocument, yelp_docbook_document, YELP_TYPE_DOCUMENT);
#define GET_PRIV(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_DOCBOOK_DOCUMENT, YelpDocbookDocumentPrivate))

typedef struct _YelpDocbookDocumentPrivate  YelpDocbookDocumentPrivate;
struct _YelpDocbookDocumentPrivate {
    YelpUri       *uri;

    DocbookState   state;

    GMutex        *mutex;
    GThread       *thread;

    gboolean       process_running;
    gboolean       transform_running;

    YelpTransform *transform;
    guint          chunk_ready;
    guint          finished;
    guint          error;

    /* FIXME: all */
    GtkTreeModel  *sections;
    GtkTreeIter   *sections_iter; /* On the stack, do not free */

    xmlDocPtr     xmldoc;
    xmlNodePtr    xmlcur;
    gint          max_depth;
    gint          cur_depth;
    gchar        *cur_page_id;
    gchar        *cur_prev_id;
    gchar        *root_id;
};

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

    document_class->request_page = docbook_request_page;
    /*document_class->get_sections = docbook_get_sections;*/

    g_type_class_add_private (klass, sizeof (YelpDocbookDocumentPrivate));
}

static void
yelp_docbook_document_init (YelpDocbookDocument *docbook)
{
    YelpDocbookDocumentPrivate *priv = GET_PRIV (docbook);

    priv->sections = NULL;

    priv->state = DOCBOOK_STATE_BLANK;

    priv->mutex = g_mutex_new ();
}

static void
yelp_docbook_document_dispose (GObject *object)
{
    YelpDocbookDocumentPrivate *priv = GET_PRIV (object);

    if (priv->uri) {
        g_object_unref (priv->uri);
        priv->uri = NULL;
    }

    if (priv->sections) {
        g_object_unref (priv->sections);
        priv->sections = NULL;
    }

    G_OBJECT_CLASS (yelp_docbook_document_parent_class)->dispose (object);
}

static void
yelp_docbook_document_finalize (GObject *object)
{
    YelpDocbookDocumentPrivate *priv = GET_PRIV (object);

    if (priv->xmldoc)
        xmlFreeDoc (priv->xmldoc);

    g_free (priv->cur_page_id);
    g_free (priv->cur_prev_id);
    g_free (priv->root_id);

    g_mutex_free (priv->mutex);

    G_OBJECT_CLASS (yelp_docbook_document_parent_class)->finalize (object);
}

/******************************************************************************/

YelpDocument *
yelp_docbook_document_new (YelpUri *uri)
{
    YelpDocbookDocument *docbook;
    YelpDocbookDocumentPrivate *priv;

    g_return_val_if_fail (uri != NULL, NULL);

    docbook = (YelpDocbookDocument *) g_object_new (YELP_TYPE_DOCBOOK_DOCUMENT, NULL);
    priv = GET_PRIV (docbook);

    priv->uri = g_object_ref (uri);

    priv->sections =
        GTK_TREE_MODEL (gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING));

    return (YelpDocument *) docbook;
}

/******************************************************************************/

/** YelpDocument **************************************************************/

/* static gpointer */
/* docbook_get_sections (YelpDocument *document) */
/* { */
/*     YelpDocbook *db = (YelpDocbook *) document; */
    
/*     return (gpointer) (db->priv->sections); */
/* } */

static gboolean
docbook_request_page (YelpDocument         *document,
                      const gchar          *page_id,
                      GCancellable         *cancellable,
                      YelpDocumentCallback  callback,
                      gpointer              user_data)
{
    YelpDocbookDocumentPrivate *priv = GET_PRIV (document);
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
                                                                                user_data);
    if (handled) {
        return;
    }

    g_mutex_lock (priv->mutex);

    switch (priv->state) {
    case DOCBOOK_STATE_BLANK:
        priv->state = DOCBOOK_STATE_PARSING;
        priv->process_running = TRUE;
        g_object_ref (document);
        priv->thread = g_thread_create ((GThreadFunc) docbook_process,
                                        document, FALSE, NULL);
        break;
    case DOCBOOK_STATE_PARSING:
        break;
    case DOCBOOK_STATE_PARSED:
    case DOCBOOK_STATE_STOP:
        docuri = yelp_uri_get_document_uri (priv->uri);
        error = g_error_new (YELP_ERROR, YELP_ERROR_NOT_FOUND,
                             _("The page ‘%s’ was not found in the document ‘%s’."),
                             page_id, docuri);
        g_free (docuri);
        yelp_document_signal (document, page_id,
                              YELP_DOCUMENT_SIGNAL_ERROR,
                              error);
        g_error_free (error);
        break;
    }

    g_mutex_unlock (priv->mutex);
}

/******************************************************************************/

static void
docbook_process (YelpDocbookDocument *docbook)
{
    YelpDocbookDocumentPrivate *priv = GET_PRIV (docbook);
    YelpDocument *document = YELP_DOCUMENT (docbook);
    GFile *file = NULL;
    gchar *filepath = NULL;
    xmlDocPtr xmldoc = NULL;
    xmlChar *id = NULL;
    xmlParserCtxtPtr parserCtxt = NULL;
    GError *error;
    gint  params_i = 0;
    gchar **params = NULL;

    debug_print (DB_FUNCTION, "entering\n");

    file = yelp_uri_get_file (priv->uri);
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

    if (xmldoc == NULL) {
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

    g_mutex_lock (priv->mutex);
    if (!xmlStrcmp (xmlDocGetRootElement (xmldoc)->name, BAD_CAST "book"))
        priv->max_depth = 2;
    else
        priv->max_depth = 1;

    priv->xmldoc = xmldoc;
    priv->xmlcur = xmlDocGetRootElement (xmldoc);

    id = xmlGetProp (priv->xmlcur, BAD_CAST "id");
    if (!id)
        id = xmlGetNsProp (priv->xmlcur, XML_XML_NAMESPACE, BAD_CAST "id");

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
            xmlNewNsProp (priv->xmlcur,
                          xmlNewNs (priv->xmlcur, XML_XML_NAMESPACE, BAD_CAST "xml"),
                          BAD_CAST "id", BAD_CAST "//index");
        else
            xmlNewProp (priv->xmlcur, BAD_CAST "id", BAD_CAST "//index");
    }
    yelp_document_set_root_id (document, priv->root_id, priv->root_id);
    g_mutex_unlock (priv->mutex);

    g_mutex_lock (priv->mutex);
    if (priv->state == DOCBOOK_STATE_STOP) {
        g_mutex_unlock (priv->mutex);
        goto done;
    }
    g_mutex_unlock (priv->mutex);

    docbook_walk (docbook);

    g_mutex_lock (priv->mutex);
    if (priv->state == DOCBOOK_STATE_STOP) {
        g_mutex_unlock (priv->mutex);
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
    g_mutex_unlock (priv->mutex);

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
    YelpDocbookDocumentPrivate *priv = GET_PRIV (docbook);
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

/******************************************************************************/

static void
docbook_walk (YelpDocbookDocument *docbook)
{
    static       gint autoid = 0;
    gchar        autoidstr[20];
    xmlChar     *id = NULL;
    xmlChar     *title = NULL;
    gchar       *old_page_id = NULL;
    xmlNodePtr   cur, old_cur;
    GtkTreeIter  iter;
    GtkTreeIter *old_iter = NULL;
    gboolean chunkQ;
    YelpDocbookDocumentPrivate *priv = GET_PRIV (docbook);
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
        id = xmlGetNsProp (priv->xmlcur, XML_XML_NAMESPACE, BAD_CAST "id");

    if (docbook_walk_divisionQ (docbook) && !id) {
        /* If id attribute is not present, autogenerate a
         * unique value, and insert it into the in-memory tree */
        g_snprintf (autoidstr, 20, "//autoid-%d", ++autoid);
        if (priv->xmlcur->ns) {
            xmlNewNsProp (priv->xmlcur,
                          xmlNewNs (priv->xmlcur, XML_XML_NAMESPACE, BAD_CAST "xml"),
                          BAD_CAST "id", BAD_CAST autoidstr);
            id = xmlGetNsProp (priv->xmlcur, XML_XML_NAMESPACE, BAD_CAST "id");
        }
        else {
            xmlNewProp (priv->xmlcur, BAD_CAST "id", BAD_CAST autoidstr);
            id = xmlGetProp (priv->xmlcur, BAD_CAST "id"); 
        }
    }

    if (docbook_walk_chunkQ (docbook)) {
        title = BAD_CAST docbook_walk_get_title (docbook);

        debug_print (DB_DEBUG, "  id: \"%s\"\n", id);
        debug_print (DB_DEBUG, "  title: \"%s\"\n", title);

        yelp_document_set_page_title (document, (gchar *) id, (gchar *) title);

        gdk_threads_enter ();
        gtk_tree_store_append (GTK_TREE_STORE (priv->sections),
                               &iter,
                               priv->sections_iter);
        gtk_tree_store_set (GTK_TREE_STORE (priv->sections),
                            &iter,
                            DOCBOOK_COLUMN_ID, id,
                            DOCBOOK_COLUMN_TITLE, title,
                            -1);
        gdk_threads_leave ();

        if (priv->cur_prev_id) {
            yelp_document_set_prev_id (document, (gchar *) id, priv->cur_prev_id);
            yelp_document_set_next_id (document, priv->cur_prev_id, (gchar *) id);
            g_free (priv->cur_prev_id);
        }
        priv->cur_prev_id = g_strdup ((gchar *) id);

        if (priv->cur_page_id)
            yelp_document_set_up_id (document, (gchar *) id, priv->cur_page_id);
        old_page_id = priv->cur_page_id;
        priv->cur_page_id = g_strdup ((gchar *) id);

        old_iter = priv->sections_iter;
        if (priv->xmlcur->parent->type != XML_DOCUMENT_NODE)
            priv->sections_iter = &iter;
    }

    old_cur = priv->xmlcur;
    priv->cur_depth++;
    if (id) {
        yelp_document_set_root_id (document, (gchar *) id, priv->root_id);
        yelp_document_set_page_id (document, (gchar *) id, priv->cur_page_id);
    }

    chunkQ = docbook_walk_chunkQ (docbook);
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

    if (chunkQ) {
        priv->sections_iter = old_iter;
        g_free (priv->cur_page_id);
        priv->cur_page_id = old_page_id;
    }

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
}

static gboolean
docbook_walk_chunkQ (YelpDocbookDocument *docbook)
{
    YelpDocbookDocumentPrivate *priv = GET_PRIV (docbook);
    if (priv->cur_depth <= priv->max_depth)
        return docbook_walk_divisionQ (docbook);
    else
        return FALSE;
}

static gboolean
docbook_walk_divisionQ (YelpDocbookDocument *docbook)
{
    YelpDocbookDocumentPrivate *priv = GET_PRIV (docbook);
    xmlNodePtr node = priv->xmlcur;
    return (!xmlStrcmp (node->name, (const xmlChar *) "appendix")     ||
            !xmlStrcmp (node->name, (const xmlChar *) "article")      ||
            !xmlStrcmp (node->name, (const xmlChar *) "book")         ||
            !xmlStrcmp (node->name, (const xmlChar *) "bibliography") ||
            !xmlStrcmp (node->name, (const xmlChar *) "chapter")      ||
            !xmlStrcmp (node->name, (const xmlChar *) "colophon")     ||
            !xmlStrcmp (node->name, (const xmlChar *) "glossary")     ||
            !xmlStrcmp (node->name, (const xmlChar *) "index")        ||
            !xmlStrcmp (node->name, (const xmlChar *) "part")         ||
            !xmlStrcmp (node->name, (const xmlChar *) "preface")      ||
            !xmlStrcmp (node->name, (const xmlChar *) "reference")    ||
            !xmlStrcmp (node->name, (const xmlChar *) "refentry")     ||
            !xmlStrcmp (node->name, (const xmlChar *) "refsect1")     ||
            !xmlStrcmp (node->name, (const xmlChar *) "refsect2")     ||
            !xmlStrcmp (node->name, (const xmlChar *) "refsect3")     ||
            !xmlStrcmp (node->name, (const xmlChar *) "refsection")   ||
            !xmlStrcmp (node->name, (const xmlChar *) "sect1")        ||
            !xmlStrcmp (node->name, (const xmlChar *) "sect2")        ||
            !xmlStrcmp (node->name, (const xmlChar *) "sect3")        ||
            !xmlStrcmp (node->name, (const xmlChar *) "sect4")        ||
            !xmlStrcmp (node->name, (const xmlChar *) "sect5")        ||
            !xmlStrcmp (node->name, (const xmlChar *) "section")      ||
            !xmlStrcmp (node->name, (const xmlChar *) "set")          ||
            !xmlStrcmp (node->name, (const xmlChar *) "setindex")     ||
            !xmlStrcmp (node->name, (const xmlChar *) "simplesect")   );
}

static gchar *
docbook_walk_get_title (YelpDocbookDocument *docbook)
{
    YelpDocbookDocumentPrivate *priv = GET_PRIV (docbook);
    gchar *infoname = NULL;
    xmlNodePtr child = NULL;
    xmlNodePtr title = NULL;
    xmlNodePtr title_tmp = NULL;

    if (!xmlStrcmp (priv->xmlcur->name, BAD_CAST "refentry")) {
        /* The title for a refentry element can come from the following:
         *   refmeta/refentrytitle
         *   refentryinfo/title[abbrev]
         *   refnamediv/refname
         * We take the first one we find.
         */
        for (child = priv->xmlcur->children; child; child = child->next) {
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

        infoname = g_strdup_printf ("%sinfo", priv->xmlcur->name);

        for (child = priv->xmlcur->children; child; child = child->next) {
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

    if (title)
        return (gchar *) xmlNodeGetContent (title);
    else
        return g_strdup (_("Unknown"));
}

/******************************************************************************/

static void
transform_chunk_ready (YelpTransform       *transform,
                       gchar               *chunk_id,
                       YelpDocbookDocument *docbook)
{
    YelpDocbookDocumentPrivate *priv = GET_PRIV (docbook);
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
    YelpDocbookDocumentPrivate *priv = GET_PRIV (docbook);
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

    docuri = yelp_uri_get_document_uri (priv->uri);
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
    YelpDocbookDocumentPrivate *priv = GET_PRIV (docbook);
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
    YelpDocbookDocumentPrivate *priv = GET_PRIV (docbook);
 
    debug_print (DB_FUNCTION, "entering\n");

    if (priv->xmldoc)
	xmlFreeDoc (priv->xmldoc);
    priv->xmldoc = NULL;
}
