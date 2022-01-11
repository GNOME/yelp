/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009-2020 Shaun McCance  <shaunm@gnome.org>
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
#include <libxml/xpathInternals.h>

#include "yelp-error.h"
#include "yelp-mallard-document.h"
#include "yelp-settings.h"
#include "yelp-storage.h"
#include "yelp-transform.h"
#include "yelp-debug.h"

#define STYLESHEET DATADIR"/yelp/xslt/mal2html.xsl"
#define MALLARD_NS BAD_CAST "http://projectmallard.org/1.0/"
#define CACHE_NS BAD_CAST "http://projectmallard.org/cache/1.0/"

typedef enum {
    MALLARD_STATE_BLANK,
    MALLARD_STATE_THINKING,
    MALLARD_STATE_IDLE,
    MALLARD_STATE_STOP
} MallardState;

typedef struct {
    YelpMallardDocument *mallard;

    gchar         *page_id;
    gchar         *filename;
    xmlDocPtr      xmldoc;
    YelpTransform *transform;

    guint          chunk_ready;
    guint          finished;
    guint          error;

    xmlNodePtr          cur;
    xmlNodePtr          cache;
    xmlXPathContextPtr  xpath;

    gboolean       link_title;
    gboolean       sort_title;

    gchar         *page_title;
    gchar         *page_desc;
    gchar         *page_keywords;
    gchar         *next_page;
} MallardPageData;

static void           yelp_mallard_document_dispose    (GObject                  *object);
static void           yelp_mallard_document_finalize   (GObject                  *object);

static void           mallard_index             (YelpDocument         *document);
static gboolean       mallard_request_page      (YelpDocument         *document,
                                                 const gchar          *page_id,
                                                 GCancellable         *cancellable,
                                                 YelpDocumentCallback  callback,
                                                 gpointer              user_data,
                                                 GDestroyNotify        notify);

static void           transform_chunk_ready     (YelpTransform        *transform,
                                                 gchar                *chunk_id,
                                                 MallardPageData      *page_data);
static void           transform_finished        (YelpTransform        *transform,
                                                 MallardPageData      *page_data);
static void           transform_error           (YelpTransform        *transform,
                                                 MallardPageData      *page_data);

static void           mallard_think             (YelpMallardDocument  *mallard);
static void           mallard_try_run           (YelpMallardDocument  *mallard,
                                                 const gchar          *page_id);

static void           mallard_page_data_cancel  (MallardPageData      *page_data);
static void           mallard_page_data_walk    (MallardPageData      *page_data);
static void           mallard_page_data_info    (MallardPageData      *page_data,
                                                 xmlNodePtr            info_node,
                                                 xmlNodePtr            cache_node);
static void           mallard_page_data_run     (MallardPageData      *page_data);
static void           mallard_page_data_free    (MallardPageData      *page_data);
static void           mallard_reload            (YelpMallardDocument  *mallard);
static void           mallard_monitor_changed   (GFileMonitor         *monitor,
                                                 GFile                *file,
                                                 GFile                *other_file,
                                                 GFileMonitorEvent     event_type,
                                                 YelpMallardDocument  *mallard);

static const char *   xml_node_get_icon         (xmlNodePtr            node);
static gboolean       xml_node_is_ns_name       (xmlNodePtr            node,
                                                 const xmlChar        *ns,
                                                 const xmlChar        *name);


typedef struct _YelpMallardDocumentPrivate  YelpMallardDocumentPrivate;
struct _YelpMallardDocumentPrivate {
    MallardState   state;

    GMutex         mutex;
    GThread       *thread;
    gboolean       thread_running;
    GThread       *index;
    gboolean       index_running;
    GSList        *pending;

    xmlDocPtr      cache;
    xmlNsPtr       mallard_ns;
    xmlNsPtr       cache_ns;
    GHashTable    *pages_hash;

    GFileMonitor **monitors;

    xmlXPathCompExprPtr  normalize;
};

G_DEFINE_TYPE_WITH_PRIVATE (YelpMallardDocument, yelp_mallard_document, YELP_TYPE_DOCUMENT)

/******************************************************************************/

static void
yelp_mallard_document_class_init (YelpMallardDocumentClass *klass)
{
    GObjectClass      *object_class   = G_OBJECT_CLASS (klass);
    YelpDocumentClass *document_class = YELP_DOCUMENT_CLASS (klass);

    object_class->dispose = yelp_mallard_document_dispose;
    object_class->finalize = yelp_mallard_document_finalize;

    document_class->request_page = mallard_request_page;
    document_class->index = mallard_index;
}

static void
yelp_mallard_document_init (YelpMallardDocument *mallard)
{
    YelpMallardDocumentPrivate *priv = yelp_mallard_document_get_instance_private (mallard);
    xmlNodePtr cur;

    g_mutex_init (&priv->mutex);

    priv->thread_running = FALSE;
    priv->index_running = FALSE;

    priv->cache = xmlNewDoc (BAD_CAST "1.0");
    priv->mallard_ns = xmlNewNs (NULL, MALLARD_NS, BAD_CAST "mal");
    priv->cache_ns = xmlNewNs (NULL, CACHE_NS, BAD_CAST "cache");
    cur = xmlNewDocNode (priv->cache, priv->cache_ns, BAD_CAST "cache", NULL);
    xmlDocSetRootElement (priv->cache, cur);
    priv->cache_ns->next = priv->mallard_ns;
    priv->mallard_ns->next = cur->nsDef;
    cur->nsDef = priv->cache_ns;
    priv->pages_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              NULL,
                                              (GDestroyNotify) mallard_page_data_free);
    priv->normalize = xmlXPathCompile (BAD_CAST "normalize-space(.)");
}

static void
yelp_mallard_document_dispose (GObject *object)
{
    gint i;
    YelpMallardDocumentPrivate *priv =
        yelp_mallard_document_get_instance_private (YELP_MALLARD_DOCUMENT (object));

    if (priv->monitors != NULL) {
        for (i = 0; priv->monitors[i]; i++) {
            g_object_unref (priv->monitors[i]);
        }
        g_free (priv->monitors);
        priv->monitors = NULL;
    }

    G_OBJECT_CLASS (yelp_mallard_document_parent_class)->dispose (object);
}

static void
yelp_mallard_document_finalize (GObject *object)
{
    YelpMallardDocumentPrivate *priv =
        yelp_mallard_document_get_instance_private (YELP_MALLARD_DOCUMENT (object));

    g_mutex_clear (&priv->mutex);
    g_hash_table_destroy (priv->pages_hash);

    xmlFreeDoc (priv->cache);
    if (priv->normalize)
        xmlXPathFreeCompExpr (priv->normalize);

    G_OBJECT_CLASS (yelp_mallard_document_parent_class)->finalize (object);
}

/******************************************************************************/

YelpDocument *
yelp_mallard_document_new (YelpUri *uri)
{
    YelpMallardDocument *mallard;
    YelpMallardDocumentPrivate *priv;
    gchar **path;
    gint path_i;

    g_return_val_if_fail (uri != NULL, NULL);

    mallard = (YelpMallardDocument *) g_object_new (YELP_TYPE_MALLARD_DOCUMENT,
                                                    "document-uri", uri,
                                                    NULL);
    priv = yelp_mallard_document_get_instance_private (mallard);

    yelp_document_set_page_id ((YelpDocument *) mallard, NULL, "index");
    yelp_document_set_page_id ((YelpDocument *) mallard, "index", "index");

    path = yelp_uri_get_search_path (uri);
    priv->monitors = g_new0 (GFileMonitor*, g_strv_length (path) + 1);
    for (path_i = 0; path[path_i]; path_i++) {
        GFile *file;
        file = g_file_new_for_path (path[path_i]);
        priv->monitors[path_i] = g_file_monitor (file,
                                                 G_FILE_MONITOR_WATCH_MOVES,
                                                 NULL, NULL);
        g_signal_connect (priv->monitors[path_i], "changed",
                          G_CALLBACK (mallard_monitor_changed),
                          mallard);
        g_object_unref (file);
    }
    g_strfreev (path);

    g_signal_connect_swapped (yelp_settings_get_default (),
                              "colors-changed",
                              G_CALLBACK (mallard_reload),
                              mallard);

    return (YelpDocument *) mallard;
}

static gboolean
mallard_request_page (YelpDocument         *document,
                      const gchar          *page_id,
                      GCancellable         *cancellable,
                      YelpDocumentCallback  callback,
                      gpointer              user_data,
                      GDestroyNotify        notify)
{
    YelpMallardDocumentPrivate *priv =
        yelp_mallard_document_get_instance_private (YELP_MALLARD_DOCUMENT (document));
    gchar *docuri;
    GError *error;
    gboolean handled;

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "    page_id=\"%s\"\n", page_id);

    if (page_id == NULL)
        page_id = "index";

    handled =
        YELP_DOCUMENT_CLASS (yelp_mallard_document_parent_class)->request_page (document,
                                                                                page_id,
                                                                                cancellable,
                                                                                callback,
                                                                                user_data,
                                                                                notify);
    if (handled) {
        return TRUE;
    }

    g_mutex_lock (&priv->mutex);

    if (priv->state == MALLARD_STATE_BLANK) {
        priv->state = MALLARD_STATE_THINKING;
        priv->thread_running = TRUE;
        g_object_ref (document);
        priv->thread = g_thread_new ("mallard-page",
                                     (GThreadFunc)(GCallback) mallard_think,
                                     document);
    }

    switch (priv->state) {
    case MALLARD_STATE_THINKING:
        priv->pending = g_slist_prepend (priv->pending, (gpointer) g_strdup (page_id));
        break;
    case MALLARD_STATE_IDLE:
        mallard_try_run ((YelpMallardDocument *) document, page_id);
        break;
    case MALLARD_STATE_BLANK:
    case MALLARD_STATE_STOP:
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
mallard_think (YelpMallardDocument *mallard)
{
    YelpMallardDocumentPrivate *priv = yelp_mallard_document_get_instance_private (mallard);
    GError *error = NULL;
    gboolean editor_mode;

    gchar **path;
    gint path_i;
    GFile *gfile = NULL;
    GFileEnumerator *children = NULL;
    GFileInfo *pageinfo;

    editor_mode = yelp_settings_get_editor_mode (yelp_settings_get_default ());

    path = yelp_uri_get_search_path (yelp_document_get_uri ((YelpDocument *) mallard));
    if (!path || path[0] == NULL ||
        !g_file_test (path[0], G_FILE_TEST_IS_DIR)) {
        /* This basically only happens when someone passes an actual directory
           manually, which will have a singleton search path.
         */
        error = g_error_new (YELP_ERROR, YELP_ERROR_NOT_FOUND,
                             _("The directory ‘%s’ does not exist."),
                             path && path[0] ? path[0] : "NULL");
	yelp_document_error_pending ((YelpDocument *) mallard, error);
        g_error_free (error);
	goto done;
    }

    for (path_i = 0; path[path_i] != NULL; path_i++) {
        gfile = g_file_new_for_path (path[path_i]);
        children = g_file_enumerate_children (gfile,
                                              G_FILE_ATTRIBUTE_STANDARD_NAME,
                                              G_FILE_QUERY_INFO_NONE,
                                              NULL, NULL);
        while ((pageinfo = g_file_enumerator_next_file (children, NULL, NULL))) {
            MallardPageData *page_data;
            gchar *filename;
            GFile *pagefile;
            filename = g_file_info_get_attribute_as_string (pageinfo,
                                                            G_FILE_ATTRIBUTE_STANDARD_NAME);
            /* Only .page files, or .page.stub files in editor mode.
               Also, filenames with # really mess things up, and emacs
               creates them in the background all the time.
               FIXME: Let's add support for .stack files.
             */
            if (strchr(filename, '#') ||
                ( !g_str_has_suffix (filename, ".page") &&
                  !(editor_mode && g_str_has_suffix (filename, ".page.stub"))
                  )) {
                g_free (filename);
                g_object_unref (pageinfo);
                continue;
            }
            page_data = g_new0 (MallardPageData, 1);
            page_data->mallard = mallard;
            pagefile = g_file_resolve_relative_path (gfile, filename);
            page_data->filename = g_file_get_path (pagefile);
            mallard_page_data_walk (page_data);
            if (page_data->page_id == NULL) {
                mallard_page_data_free (page_data);
            }
            else if (g_hash_table_lookup (priv->pages_hash, page_data->page_id) != NULL) {
                mallard_page_data_free (page_data);
            }
            else {
                g_mutex_lock (&priv->mutex);
                yelp_document_set_root_id ((YelpDocument *) mallard,
                                           page_data->page_id, "index");
                yelp_document_set_page_id ((YelpDocument *) mallard,
                                           page_data->page_id, page_data->page_id);
                g_hash_table_insert (priv->pages_hash, page_data->page_id, page_data);
                yelp_document_set_page_title ((YelpDocument *) mallard,
                                              page_data->page_id,
                                              page_data->page_title);
                yelp_document_set_page_desc ((YelpDocument *) mallard,
                                             page_data->page_id,
                                             page_data->page_desc);
                yelp_document_set_page_keywords ((YelpDocument *) mallard,
                                                 page_data->page_id,
                                                 page_data->page_keywords);

                if (page_data->next_page != NULL) {
                    yelp_document_set_next_id ((YelpDocument *) mallard,
                                               page_data->page_id,
                                               page_data->next_page);
                    yelp_document_set_prev_id ((YelpDocument *) mallard,
                                               page_data->next_page,
                                               page_data->page_id);
                }
                yelp_document_signal ((YelpDocument *) mallard,
                                      page_data->page_id,
                                      YELP_DOCUMENT_SIGNAL_INFO,
                                      NULL);
                g_mutex_unlock (&priv->mutex);
            }
            g_object_unref (pagefile);
            g_free (filename);
            g_object_unref (pageinfo);
        }
    }
    g_strfreev (path);

    g_mutex_lock (&priv->mutex);
    priv->state = MALLARD_STATE_IDLE;
    while (priv->pending) {
        gchar *page_id = (gchar *) priv->pending->data;
        mallard_try_run (mallard, page_id);
        g_free (page_id);
        priv->pending = g_slist_delete_link (priv->pending, priv->pending);
    }
    g_mutex_unlock (&priv->mutex);

 done:
    g_object_unref (children);
    g_object_unref (gfile);

    priv->thread_running = FALSE;
    g_object_unref (mallard);
}

static void
mallard_try_run (YelpMallardDocument *mallard,
                 const gchar         *page_id)
{
    /* We expect to be in a locked mutex when this function is called. */
    YelpMallardDocumentPrivate *priv = yelp_mallard_document_get_instance_private (mallard);
    MallardPageData *page_data = NULL;
    gchar *real_id = NULL;
    GError *error;

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "    page_id=\"%s\"\n", page_id);

    if (page_id != NULL)
        real_id = yelp_document_get_page_id ((YelpDocument *) mallard, page_id);

    if (real_id != NULL) {
        page_data = g_hash_table_lookup (priv->pages_hash, real_id);
        g_free (real_id);
    }

    if (page_data == NULL) {
        gchar *docuri = yelp_uri_get_document_uri (yelp_document_get_uri ((YelpDocument *) mallard));
        error = g_error_new (YELP_ERROR, YELP_ERROR_NOT_FOUND,
                             _("The page ‘%s’ was not found in the document ‘%s’."),
                             page_id, docuri);
        g_free (docuri);
        yelp_document_signal ((YelpDocument *) mallard, page_id,
                              YELP_DOCUMENT_SIGNAL_ERROR,
                              error);
        g_error_free (error);
        return;
    }

    if (page_data->transform != NULL) {
        /* It's already running. Just let it be. */
        return;
    }

    mallard_page_data_run (page_data);
}

/******************************************************************************/
/** MallardPageData ***********************************************************/

static void
mallard_page_data_cancel (MallardPageData *page_data)
{
    if (page_data->transform) {
        if (page_data->chunk_ready) {
            g_signal_handler_disconnect (page_data->transform, page_data->chunk_ready);
            page_data->chunk_ready = 0;
        }
        if (page_data->finished) {
            g_signal_handler_disconnect (page_data->transform, page_data->finished);
            page_data->finished = 0;
        }
        if (page_data->error) {
            g_signal_handler_disconnect (page_data->transform, page_data->error);
            page_data->error = 0;
        }
        yelp_transform_cancel (page_data->transform);
        g_object_unref (page_data->transform);
        page_data->transform = NULL;
    }
}

static void
mallard_page_data_walk (MallardPageData *page_data)
{
    YelpMallardDocumentPrivate *priv = yelp_mallard_document_get_instance_private (page_data->mallard);
    xmlParserCtxtPtr parserCtxt = NULL;
    xmlChar *id = NULL;

    if (page_data->cur == NULL) {
        parserCtxt = xmlNewParserCtxt ();
        page_data->xmldoc = xmlCtxtReadFile (parserCtxt,
                                             (const char *) page_data->filename, NULL,
                                             XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
                                             XML_PARSE_NOENT   | XML_PARSE_NONET   );
        if (page_data->xmldoc == NULL)
            goto done;
        if (xmlXIncludeProcessFlags (page_data->xmldoc,
                                     XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
                                     XML_PARSE_NOENT   | XML_PARSE_NONET   )
            < 0)
            goto done;
        page_data->cur = xmlDocGetRootElement (page_data->xmldoc);
        page_data->cache = xmlDocGetRootElement (priv->cache);
        page_data->xpath = xmlXPathNewContext (page_data->xmldoc);
        mallard_page_data_walk (page_data);
    } else {
        gboolean ispage;
        xmlNodePtr child, oldcur, oldcache, info;

        id = xmlGetProp (page_data->cur, BAD_CAST "id");
        if (id == NULL)
            goto done;

        ispage = xml_node_is_ns_name (page_data->cur, MALLARD_NS, BAD_CAST "page");
        if (ispage && g_hash_table_lookup (priv->pages_hash, id) != NULL)
            goto done;

        page_data->cache = xmlNewChild (page_data->cache,
                                        priv->mallard_ns,
                                        page_data->cur->name,
                                        NULL);

        if (ispage) {
            page_data->page_id = g_strdup ((gchar *) id);
            xmlSetProp (page_data->cache, BAD_CAST "id", id);
            yelp_document_set_page_id ((YelpDocument *) page_data->mallard,
                                       g_strrstr (page_data->filename, G_DIR_SEPARATOR_S),
                                       page_data->page_id);
            yelp_document_set_page_icon ((YelpDocument *) page_data->mallard,
                                         page_data->page_id,
                                         xml_node_get_icon (page_data->cur));
        } else {
            gchar *newid = g_strdup_printf ("%s#%s", page_data->page_id, id);
            xmlSetProp (page_data->cache, BAD_CAST "id", BAD_CAST newid);
            g_free (newid);
        }

        info = xmlNewChild (page_data->cache,
                            priv->mallard_ns,
                            BAD_CAST "info", NULL);
        page_data->link_title = FALSE;
        page_data->sort_title = FALSE;
        for (child = page_data->cur->children; child; child = child->next) {
            if (child->type != XML_ELEMENT_NODE)
                continue;
            if (xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "info")) {
                mallard_page_data_info (page_data, child, info);
            }
            else if (xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "title")) {
                xmlNodePtr node;
                xmlNodePtr title_node = xmlNewChild (page_data->cache,
                                                     priv->mallard_ns,
                                                     BAD_CAST "title", NULL);
                for (node = child->children; node; node = node->next) {
                    xmlAddChild (title_node, xmlCopyNode (node, 1));
                }
                if (!page_data->link_title) {
                    xmlNodePtr title_node2 = xmlNewChild (info,
                                                          priv->mallard_ns,
                                                          BAD_CAST "title", NULL);
                    xmlSetProp (title_node2, BAD_CAST "type", BAD_CAST "link");
                    for (node = child->children; node; node = node->next) {
                        xmlAddChild (title_node2, xmlCopyNode (node, 1));
                    }
                }
                if (!page_data->sort_title) {
                    xmlNodePtr title_node2 = xmlNewChild (info,
                                                          priv->mallard_ns,
                                                          BAD_CAST "title", NULL);
                    xmlSetProp (title_node2, BAD_CAST "type", BAD_CAST "sort");
                    for (node = child->children; node; node = node->next) {
                        xmlAddChild (title_node2, xmlCopyNode (node, 1));
                    }
                }
                if (page_data->page_title == NULL) {
                    xmlXPathObjectPtr obj;
                    page_data->xpath->node = child;
                    obj = xmlXPathCompiledEval (priv->normalize, page_data->xpath);
                    page_data->page_title = g_strdup ((const gchar *) obj->stringval);
                    xmlXPathFreeObject (obj);
                }
            }
            else if (xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "section")) {
                oldcur = page_data->cur;
                oldcache = page_data->cache;
                page_data->cur = child;
                mallard_page_data_walk (page_data);
                page_data->cur = oldcur;
                page_data->cache = oldcache;
            }
        }
    }

 done:
    if (id)
        xmlFree (id);
    if (parserCtxt)
	xmlFreeParserCtxt (parserCtxt);
}

static void
mallard_page_data_info (MallardPageData *page_data,
                        xmlNodePtr       info_node,
                        xmlNodePtr       cache_node)
{
    xmlNodePtr child;

    for (child = info_node->children; child; child = child->next) {
        if (xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "info")) {
            mallard_page_data_info (page_data, child, cache_node);
        }
        else if (xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "title")) {
            xmlNodePtr title_node;
            xmlChar *type, *role;
            title_node = xmlCopyNode (child, 1);
            xmlAddChild (cache_node, title_node);

            type = xmlGetProp (child, BAD_CAST "type");

            if (type != NULL) {
                role = xmlGetProp (child, BAD_CAST "role");
                if (xmlStrEqual (type, BAD_CAST "link") && role == NULL)
                    page_data->link_title = TRUE;
                if (xmlStrEqual (type, BAD_CAST "sort"))
                    page_data->sort_title = TRUE;
                if (xmlStrEqual (type, BAD_CAST "text")) {
                    YelpMallardDocumentPrivate *priv =
                        yelp_mallard_document_get_instance_private (page_data->mallard);
                    xmlXPathObjectPtr obj;
                    page_data->xpath->node = child;
                    obj = xmlXPathCompiledEval (priv->normalize, page_data->xpath);
                    g_free (page_data->page_title);
                    page_data->page_title = g_strdup ((const gchar *) obj->stringval);
                    xmlXPathFreeObject (obj);
                }
                if (role != NULL)
                    xmlFree (role);
                xmlFree (type);
            }
        }
        else if (xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "desc")) {
            YelpMallardDocumentPrivate *priv = yelp_mallard_document_get_instance_private (page_data->mallard);
            xmlXPathObjectPtr obj;
            page_data->xpath->node = child;
            obj = xmlXPathCompiledEval (priv->normalize, page_data->xpath);
            g_free(page_data->page_desc);
            page_data->page_desc = g_strdup ((const gchar *) obj->stringval);
            xmlXPathFreeObject (obj);

            xmlAddChild (cache_node, xmlCopyNode (child, 1));
        }
        else if (xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "keywords")) {
            /* FIXME: multiple keywords? same for desc/title */

            YelpMallardDocumentPrivate *priv = yelp_mallard_document_get_instance_private (page_data->mallard);
            xmlXPathObjectPtr obj;
            page_data->xpath->node = child;
            obj = xmlXPathCompiledEval (priv->normalize, page_data->xpath);
            g_free(page_data->page_keywords);
            page_data->page_keywords = g_strdup ((const gchar *) obj->stringval);
            xmlXPathFreeObject (obj);

            xmlAddChild (cache_node, xmlCopyNode (child, 1));
        }
        else if (xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "link")) {
            xmlChar *type, *next;

            xmlAddChild (cache_node, xmlCopyNode (child, 1));

            type = xmlGetProp (child, BAD_CAST "type");
            if (type != NULL) {
                if (xmlStrEqual (type, BAD_CAST "next")) {
                    next = xmlGetProp (child, BAD_CAST "xref");
                    if (next != NULL) {
                        if (page_data->next_page != NULL)
                            g_free (page_data->next_page);
                        page_data->next_page = g_strdup ((const gchar *) next);
                        xmlFree (next);
                    }
                }
                xmlFree (type);
            }

        }
        else if (xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "revision")) {
            xmlAddChild (cache_node, xmlCopyNode (child, 1));
        }
        else {
            if (child->ns != NULL && !xmlStrEqual(child->ns->href, MALLARD_NS)) {
                xmlAddChild (cache_node, xmlCopyNode (child, 1));
            }
        }
    }
}

static void
mallard_page_data_run (MallardPageData *page_data)
{
    YelpSettings *settings = yelp_settings_get_default ();
    YelpMallardDocumentPrivate *priv = yelp_mallard_document_get_instance_private (page_data->mallard);
    gchar **params = NULL;

    mallard_page_data_cancel (page_data);
    page_data->transform = yelp_transform_new (STYLESHEET);

    page_data->chunk_ready =
        g_signal_connect (page_data->transform, "chunk-ready",
                          (GCallback) transform_chunk_ready,
                          page_data);
    page_data->finished =
        g_signal_connect (page_data->transform, "finished",
                          (GCallback) transform_finished,
                          page_data);
    page_data->error =
        g_signal_connect (page_data->transform, "error",
                          (GCallback) transform_error,
                          page_data);

    if (g_str_has_suffix (page_data->filename, ".page.stub")) {
        gint end;
        params = yelp_settings_get_all_params (settings, 2, &end);
        params[end++] = g_strdup ("yelp.stub");
        params[end++] = g_strdup ("true()");
    }
    else
        params = yelp_settings_get_all_params (settings, 0, NULL);

    yelp_transform_start (page_data->transform,
			  page_data->xmldoc,
                          priv->cache,
			  (const gchar * const *) params);
    g_strfreev (params);
}

static void
mallard_page_data_free (MallardPageData *page_data)
{
    mallard_page_data_cancel (page_data);
    g_free (page_data->page_id);
    g_free (page_data->filename);
    if (page_data->xmldoc)
        xmlFreeDoc (page_data->xmldoc);
    if (page_data->xpath)
        xmlXPathFreeContext (page_data->xpath);
    g_free (page_data->page_title);
    g_free (page_data->page_desc);
    g_free (page_data->page_keywords);
    g_free (page_data->next_page);
    g_free (page_data);
}

/******************************************************************************/
/** YelpTransform *************************************************************/

static void
transform_chunk_ready (YelpTransform   *transform,
                       gchar           *chunk_id,
                       MallardPageData *page_data)
{
    YelpMallardDocumentPrivate *priv;
    gchar *content;

    debug_print (DB_FUNCTION, "entering\n");

    g_assert (page_data != NULL && page_data->mallard != NULL &&
              YELP_IS_MALLARD_DOCUMENT (page_data->mallard));
    g_assert (transform == page_data->transform);

    priv = yelp_mallard_document_get_instance_private (page_data->mallard);

    if (priv->state == MALLARD_STATE_STOP) {
        mallard_page_data_cancel (page_data);
        return;
    }

    content = yelp_transform_take_chunk (transform, chunk_id);
    yelp_document_give_contents (YELP_DOCUMENT (page_data->mallard),
                                 chunk_id,
                                 content,
                                 "application/xhtml+xml");

    yelp_document_signal (YELP_DOCUMENT (page_data->mallard),
                          chunk_id,
                          YELP_DOCUMENT_SIGNAL_CONTENTS,
                          NULL);
}

static void
transform_finished (YelpTransform   *transform,
                    MallardPageData *page_data)
{
    YelpMallardDocumentPrivate *priv;

    g_assert (page_data != NULL && page_data->mallard != NULL &&
              YELP_IS_MALLARD_DOCUMENT (page_data->mallard));
    g_assert (transform == page_data->transform);

    priv = yelp_mallard_document_get_instance_private (page_data->mallard);

    if (priv->state == MALLARD_STATE_STOP) {
        mallard_page_data_cancel (page_data);
        return;
    }

    mallard_page_data_cancel (page_data);

    if (page_data->xmldoc)
	xmlFreeDoc (page_data->xmldoc);
    page_data->xmldoc = NULL;
}

static void
transform_error (YelpTransform   *transform,
                 MallardPageData *page_data)
{
    YelpMallardDocumentPrivate *priv;
    GError *error;

    g_assert (page_data != NULL && page_data->mallard != NULL &&
              YELP_IS_MALLARD_DOCUMENT (page_data->mallard));
    g_assert (transform == page_data->transform);

    priv = yelp_mallard_document_get_instance_private (page_data->mallard);

    if (priv->state == MALLARD_STATE_STOP) {
        mallard_page_data_cancel (page_data);
        return;
    }

    error = yelp_transform_get_error (transform);
    yelp_document_error_pending ((YelpDocument *) (page_data->mallard), error);
    g_error_free (error);

    mallard_page_data_cancel (page_data);
}

static const char *
xml_node_get_icon (xmlNodePtr node)
{
    xmlChar *style;
    gchar **styles;
    const gchar *icon = "yelp-page-symbolic";
    style = xmlGetProp (node, BAD_CAST "style");
    if (style) {
        gint i;
        styles = g_strsplit ((const gchar *)style, " ", -1);
        for (i = 0; styles[i] != NULL; i++) {
            if (g_str_equal (styles[i], "video")) {
                icon = "yelp-page-video-symbolic";
                break;
            }
            else if (g_str_equal (styles[i], "task")) {
                icon = "yelp-page-task-symbolic";
                break;
            }
            else if (g_str_equal (styles[i], "tip")) {
                icon = "yelp-page-tip-symbolic";
                break;
            }
            else if (g_str_equal (styles[i], "problem")) {
                icon = "yelp-page-problem-symbolic";
                break;
            }
            else if (g_str_equal (styles[i], "ui")) {
                icon = "yelp-page-ui-symbolic";
                break;
            }
        }
        g_strfreev (styles);
        xmlFree (style);
    }
    return icon;
}

static gboolean
xml_node_is_ns_name (xmlNodePtr      node,
                     const xmlChar  *ns,
                     const xmlChar  *name)
{
    if (node->ns == NULL)
        return (ns == NULL);
    else if (ns != NULL && node->ns->href != NULL)
        return (xmlStrEqual (ns, node->ns->href) && xmlStrEqual (name, node->name));
    return FALSE;
}

/******************************************************************************/

typedef struct {
    YelpMallardDocument *mallard;
    xmlDocPtr doc;
    xmlNodePtr cur;
    GString *str;
    gboolean is_inline;
    gboolean in_info;
} MallardIndexData;

static void
mallard_index_node (MallardIndexData *index)
{
    xmlNodePtr orig, child;
    gboolean was_inline, was_info;

    orig = index->cur;
    was_inline = index->is_inline;
    was_info = index->in_info;

    for (child = index->cur->children; child; child = child->next) {
        if (index->is_inline) {
            if ((xml_node_is_ns_name (child->parent, MALLARD_NS, BAD_CAST "guiseq") ||
                 xml_node_is_ns_name (child->parent, MALLARD_NS, BAD_CAST "keyseq")) &&
                child->prev != NULL) {
                g_string_append_c (index->str, ' ');
            }
            if (child->type == XML_TEXT_NODE) {
                g_string_append (index->str, (const gchar *) child->content);
                continue;
            }
        }

        if (child->type != XML_ELEMENT_NODE ||
            xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "comment"))
            continue;

        if (xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "info")) {
            index->in_info = TRUE;
        }
        else if (xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "p") ||
            xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "code") ||
            xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "screen") ||
            xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "title") ||
            xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "desc") ||
            xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "keywords") ||
            xml_node_is_ns_name (child, MALLARD_NS, BAD_CAST "cite")) {
            index->is_inline = TRUE;
        }
        else if (index->in_info && !index->is_inline) {
            continue;
        }

        index->cur = child;
        mallard_index_node (index);

        if (index->is_inline && !was_inline) {
            g_string_append_c (index->str, '\n');
        }

        index->cur = orig;
        index->is_inline = was_inline;
        index->in_info = was_info;
    }
}

static gboolean
mallard_index_done (YelpMallardDocument *mallard)
{
    g_object_set (mallard, "indexed", TRUE, NULL);
    return FALSE;
}

static void
mallard_index_threaded (YelpMallardDocument *mallard)
{
    gchar **path;
    gint path_i;
    GHashTable *ids;
    gchar *doc_uri;
    YelpUri *document_uri;
    YelpMallardDocumentPrivate *priv = yelp_mallard_document_get_instance_private (mallard);

    document_uri = yelp_document_get_uri (YELP_DOCUMENT (mallard));
    doc_uri = yelp_uri_get_document_uri (document_uri);
    ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    path = yelp_uri_get_search_path (document_uri);
    for (path_i = 0; path[path_i] != NULL; path_i++) {
        GFile *gfile;
        GFileEnumerator *children;
        GFileInfo *pageinfo;
        gfile = g_file_new_for_path (path[path_i]);
        children = g_file_enumerate_children (gfile,
                                              G_FILE_ATTRIBUTE_STANDARD_NAME,
                                              G_FILE_QUERY_INFO_NONE,
                                              NULL, NULL);
        while ((pageinfo = g_file_enumerator_next_file (children, NULL, NULL))) {
            gchar *filename;
            GFile *pagefile;
            xmlParserCtxtPtr parserCtxt = NULL;
            xmlXPathContextPtr xpath = NULL;
            xmlXPathObjectPtr obj;
            MallardIndexData *index = NULL;
            xmlChar *id = NULL;
            YelpUri *uri;
            gchar *title, *desc, *fulltext, *tmp, *full_uri;

            filename = g_file_info_get_attribute_as_string (pageinfo,
                                                            G_FILE_ATTRIBUTE_STANDARD_NAME);
            if (!g_str_has_suffix (filename, ".page")) {
                g_free (filename);
                g_object_unref (pageinfo);
                continue;
            }
            pagefile = g_file_resolve_relative_path (gfile, filename);
            g_free (filename);
            filename = g_file_get_path (pagefile);

            index = g_new0 (MallardIndexData, 1);
            index->mallard = mallard;

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

            id = xmlGetProp (index->cur, BAD_CAST "id");
            if (id == NULL)
                goto done;
            if (g_hash_table_lookup (ids, id))
                goto done;
            /* We never use the value. Just need something non-NULL. */
            g_hash_table_insert (ids, g_strdup ((const gchar *) id), id);

            xpath = xmlXPathNewContext (index->doc);
            xmlXPathRegisterNs (xpath, BAD_CAST "mal", MALLARD_NS);
            obj = xmlXPathEvalExpression (BAD_CAST
                                          "normalize-space((/mal:page/mal:info/mal:title[@type='text'] |"
                                          "/mal:page/mal:title)[1])",
                                          xpath);
            title = g_strdup ((const gchar *) obj->stringval);
            xmlXPathFreeObject (obj);
            obj = xmlXPathEvalExpression (BAD_CAST
                                          "normalize-space(/mal:page/mal:info/mal:desc[1])",
                                          xpath);
            desc = g_strdup ((const gchar *) obj->stringval);
            xmlXPathFreeObject (obj);

            index->str = g_string_new ((const gchar *) desc);
            g_string_append_c (index->str, ' ');
            mallard_index_node (index);

            tmp = g_strconcat ("xref:", id, NULL);
            uri = yelp_uri_new_relative (document_uri, tmp);
            yelp_uri_resolve_sync (uri);
            full_uri = yelp_uri_get_canonical_uri (uri);
            g_free (tmp);
            g_object_unref (uri);

            fulltext = g_string_free (index->str, FALSE);

            yelp_storage_update (yelp_storage_get_default (),
                                 doc_uri, full_uri,
                                 title, desc,
                                 xml_node_get_icon (xmlDocGetRootElement (index->doc)),
                                 fulltext);
            if (g_str_equal (id, "index"))
                yelp_storage_set_root_title (yelp_storage_get_default (),
                                             doc_uri, title);
            g_free (full_uri);
            g_free (title);
            g_free (desc);
            g_free (fulltext);

        done:
            if (id)
                xmlFree (id);
            if (xpath != NULL)
                xmlXPathFreeContext (xpath);
            if (index->doc != NULL)
                xmlFreeDoc (index->doc);
            g_free (index);
            if (parserCtxt != NULL)
                xmlFreeParserCtxt (parserCtxt);
            g_object_unref (pagefile);
            g_free (filename);
            g_object_unref (pageinfo);
        }
    }
    g_strfreev (path);
    g_hash_table_destroy (ids);
    priv->index_running = FALSE;
    g_free (doc_uri);
    g_object_unref (mallard);
    g_idle_add ((GSourceFunc) mallard_index_done, mallard);
}

static void
mallard_index (YelpDocument *document)
{
    YelpMallardDocumentPrivate *priv;
    gboolean done;

    g_object_get (document, "indexed", &done, NULL);
    if (done)
        return;

    priv = yelp_mallard_document_get_instance_private (YELP_MALLARD_DOCUMENT (document));
    g_object_ref (document);
    priv->index = g_thread_new ("mallard-index",
                                (GThreadFunc)(GCallback) mallard_index_threaded,
                                document);
    priv->index_running = TRUE;
}

static void
mallard_monitor_changed (GFileMonitor         *monitor,
                         GFile                *file,
                         GFile                *other_file,
                         GFileMonitorEvent     event_type,
                         YelpMallardDocument  *mallard)
{
    char *filename;

    if (file) {
        filename = g_file_get_path (file);
        if (strchr(filename, '#')) {
            /* ignore emacs tmp files that mess up our uri handling anyway */
            g_free (filename);
            return;
        }
        g_free (filename);
        mallard_reload (mallard);
    }
}

static void
mallard_reload (YelpMallardDocument *mallard)
{
    gchar **ids;
    gint i;
    xmlNodePtr cur;
    YelpMallardDocumentPrivate *priv = yelp_mallard_document_get_instance_private (mallard);

    /* Exiting the thinking thread would require a fair amount of retooling.
       For now, we'll just fail to auto-reload if that's still happening.
    */
    if (priv->thread_running)
        return;

    g_mutex_lock (&priv->mutex);

    g_hash_table_remove_all (priv->pages_hash);
    g_object_set (mallard, "indexed", FALSE, NULL);

    ids = yelp_document_get_requests (YELP_DOCUMENT (mallard));
    for (i = 0; ids[i]; i++) {
        priv->pending = g_slist_prepend (priv->pending, ids[i]);
    }
    g_free (ids);

    yelp_document_clear_contents (YELP_DOCUMENT (mallard));

    xmlFreeDoc (priv->cache);
    priv->cache = xmlNewDoc (BAD_CAST "1.0");
    priv->cache_ns = xmlNewNs (NULL, CACHE_NS, BAD_CAST "cache");
    priv->mallard_ns = xmlNewNs (NULL, MALLARD_NS, BAD_CAST "mal");
    cur = xmlNewDocNode (priv->cache, priv->cache_ns, BAD_CAST "cache", NULL);
    xmlDocSetRootElement (priv->cache, cur);
    priv->cache_ns->next = priv->mallard_ns;
    priv->mallard_ns->next = cur->nsDef;
    cur->nsDef = priv->cache_ns;
    priv->state = MALLARD_STATE_THINKING;
    priv->thread_running = TRUE;
    g_object_ref (mallard);
    priv->thread = g_thread_new ("mallard-reload",
                                 (GThreadFunc)(GCallback) mallard_think,
                                 mallard);

    g_mutex_unlock (&priv->mutex);
}
