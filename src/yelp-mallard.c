/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009 Shaun McCance  <shaunm@gnome.org>
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

#include "yelp-error.h"
#include "yelp-mallard.h"
#include "yelp-settings.h"
#include "yelp-transform.h"
#include "yelp-debug.h"

#define STYLESHEET DATADIR"/yelp/xslt/mal2html.xsl"
#define MALLARD_NS "http://projectmallard.org/1.0/"
#define MALLARD_CACHE_NS "http://projectmallard.org/cache/1.0/"

#define YELP_MALLARD_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_MALLARD, YelpMallardPriv))

typedef enum {
    MALLARD_STATE_BLANK,
    MALLARD_STATE_THINKING,
    MALLARD_STATE_IDLE,
    MALLARD_STATE_STOP
} MallardState;

typedef struct {
    YelpMallard   *mallard;
    gchar         *page_id;
    gchar         *filename;
    xmlDocPtr      xmldoc;
    YelpTransform *transform;

    xmlNodePtr     cur;
    xmlNodePtr     cache;
    gboolean       link_title;
    gboolean       sort_title;
} MallardPageData;

struct _YelpMallardPriv {
    gchar         *directory;
    MallardState   state;

    GMutex        *mutex;
    GThread       *thread;
    gboolean       thread_running;
    gint           transforms_running;
    GSList        *pending;

    xmlDocPtr      cache;
    xmlNsPtr       cache_mal_ns;
    xmlNsPtr       cache_cache_ns;
    GHashTable    *pages_hash;
};


static void           mallard_class_init      (YelpMallardClass    *klass);
static void           mallard_init            (YelpMallard         *mallard);
static void           mallard_try_dispose     (GObject             *object);
static void           mallard_dispose         (GObject             *object);

/* YelpDocument */
static void           mallard_request         (YelpDocument        *document,
					       gint                 req_id,
					       gboolean             handled,
					       gchar               *page_id,
					       YelpDocumentFunc     func,
					       gpointer             user_data);

/* YelpTransform */
static void           transform_func          (YelpTransform       *transform,
					       YelpTransformSignal  signal,
					       gpointer             func_data,
					       MallardPageData     *page_data);
static void           transform_page_func     (YelpTransform       *transform,
					       gchar               *page_id,
					       MallardPageData     *page_data);
static void           transform_final_func    (YelpTransform       *transform,
					       MallardPageData     *page_data);
/* Other */
static void           mallard_think           (YelpMallard         *mallard);
static void           mallard_try_run         (YelpMallard         *mallard,
                                               gchar               *page_id);

static void           mallard_page_data_walk  (MallardPageData     *page_data);
static void           mallard_page_data_info  (MallardPageData     *page_data,
                                               xmlNodePtr           info_node,
                                               xmlNodePtr           cache_node);
static void           mallard_page_data_run   (MallardPageData     *page_data);
static void           mallard_page_data_free  (MallardPageData     *page_data);


static YelpDocumentClass *parent_class;

GType
yelp_mallard_get_type (void)
{
    static GType type = 0;
    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpMallardClass),
	    NULL, NULL,
	    (GClassInitFunc) mallard_class_init,
	    NULL, NULL,
	    sizeof (YelpMallard),
	    0,
	    (GInstanceInitFunc) mallard_init,
	};
	type = g_type_register_static (YELP_TYPE_DOCUMENT,
				       "YelpMallard", 
				       &info, 0);
    }
    return type;
}

static void
mallard_class_init (YelpMallardClass *klass)
{
    GObjectClass      *object_class   = G_OBJECT_CLASS (klass);
    YelpDocumentClass *document_class = YELP_DOCUMENT_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = mallard_try_dispose;

    document_class->request = mallard_request;
    document_class->cancel = NULL;

    g_type_class_add_private (klass, sizeof (YelpMallardPriv));
}

static void
mallard_init (YelpMallard *mallard)
{
    YelpMallardPriv *priv;
    xmlNodePtr cur;
    priv = mallard->priv = YELP_MALLARD_GET_PRIVATE (mallard);

    priv->mutex = g_mutex_new ();

    priv->thread_running = FALSE;
    priv->transforms_running = 0;

    priv->cache = xmlNewDoc (BAD_CAST "1.0");
    priv->cache_cache_ns = xmlNewNs (NULL, BAD_CAST MALLARD_CACHE_NS, BAD_CAST "cache");
    priv->cache_mal_ns = xmlNewNs (NULL, BAD_CAST MALLARD_NS, BAD_CAST "mal");
    cur = xmlNewDocNode (priv->cache, priv->cache_cache_ns, BAD_CAST "cache", NULL);
    xmlDocSetRootElement (priv->cache, cur);
    priv->cache_cache_ns->next = priv->cache_mal_ns;
    priv->cache_mal_ns->next = cur->nsDef;
    cur->nsDef = priv->cache_cache_ns;
    priv->pages_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              NULL,
                                              (GDestroyNotify) mallard_page_data_free);
}

static void
mallard_try_dispose (GObject *object)
{
    YelpMallardPriv *priv;

    g_assert (object != NULL && YELP_IS_MALLARD (object));

    priv = YELP_MALLARD (object)->priv;

    g_mutex_lock (priv->mutex);
    if (priv->thread_running || priv->transforms_running > 0) {
	priv->state = MALLARD_STATE_STOP;
	g_idle_add ((GSourceFunc) mallard_try_dispose, object);
	g_mutex_unlock (priv->mutex);
    } else {
	g_mutex_unlock (priv->mutex);
	mallard_dispose (object);
    }
}

static void
mallard_dispose (GObject *object)
{
    YelpMallard *mallard = YELP_MALLARD (object);

    g_free (mallard->priv->directory);

    g_mutex_free (mallard->priv->mutex);

    g_hash_table_destroy (mallard->priv->pages_hash);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

YelpDocument *
yelp_mallard_new (gchar *directory)
{
    YelpMallard *mallard;
    YelpDocument *document;

    g_return_val_if_fail (directory != NULL, NULL);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  directory = \"%s\"\n", directory);

    mallard = (YelpMallard *) g_object_new (YELP_TYPE_MALLARD, NULL);
    mallard->priv->directory = g_strdup (directory);

    document = (YelpDocument *) mallard;
    yelp_document_set_root_id (document, "index");
    yelp_document_add_page_id (document, "x-yelp-index", "index");

    return document;
}


static void
mallard_request (YelpDocument     *document,
                 gint              req_id,
                 gboolean          handled,
                 gchar            *page_id,
                 YelpDocumentFunc  func,
                 gpointer          user_data)
{
    YelpMallard *mallard;
    YelpMallardPriv *priv;
    YelpError *error;

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  req_id  = %i\n", req_id);
    debug_print (DB_ARG, "  page_id = \"%s\"\n", page_id);
    g_assert (G_TYPE_CHECK_INSTANCE_TYPE ((document), YELP_TYPE_MALLARD));
    g_assert (document != NULL && YELP_IS_MALLARD (document));

    if (handled) {
        return;
    }

    mallard = YELP_MALLARD (document);
    priv = mallard->priv;

    g_mutex_lock (priv->mutex);

    if (priv->state == MALLARD_STATE_BLANK) {
        priv->state = MALLARD_STATE_THINKING;
        priv->thread_running = TRUE;
        priv->thread = g_thread_create ((GThreadFunc) mallard_think,
                                        mallard, FALSE, NULL);
    }

    switch (priv->state) {
    case MALLARD_STATE_THINKING:
        priv->pending = g_slist_prepend (priv->pending, (gpointer) g_strdup (page_id));
        break;
    case MALLARD_STATE_IDLE:
        mallard_try_run (mallard, page_id);
        break;
    case MALLARD_STATE_BLANK:
    case MALLARD_STATE_STOP:
	error = yelp_error_new (_("Page not found"),
				_("The page %s was not found in the document %s."),
				page_id, priv->directory);
	yelp_document_error_request (document, req_id, error);
	break;
    }

    g_mutex_unlock (priv->mutex);
}

/******************************************************************************/

static void
mallard_think (YelpMallard *mallard)
{
    YelpMallardPriv *priv;
    YelpError *error = NULL;
    YelpDocument *document;

    GFile *gfile;
    GFileEnumerator *children;
    GFileInfo *pageinfo;

    debug_print (DB_FUNCTION, "entering\n");

    g_assert (mallard != NULL && YELP_IS_MALLARD (mallard));
    g_object_ref (mallard);
    priv = mallard->priv;
    document = YELP_DOCUMENT (mallard);

    if (!g_file_test (priv->directory, G_FILE_TEST_IS_DIR)) {
	error = yelp_error_new (_("Directory not found"),
				_("The directory ‘%s’ does not exist."),
				priv->directory);
	yelp_document_error_pending (document, error);
	goto done;
    }

    gfile = g_file_new_for_path (priv->directory);
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
        if (!g_str_has_suffix (filename, ".page")) {
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
        } else {
            g_mutex_lock (priv->mutex);
            g_hash_table_insert (priv->pages_hash, page_data->page_id, page_data);
            g_mutex_unlock (priv->mutex);
        }
        g_object_unref (pagefile);
        g_free (filename);
        g_object_unref (pageinfo);
    }

    g_mutex_lock (priv->mutex);
    priv->state = MALLARD_STATE_IDLE;
    while (priv->pending) {
        gchar *page_id;
        page_id = (gchar *) priv->pending->data;
        mallard_try_run (mallard, page_id);
        g_free (page_id);
        priv->pending = g_slist_delete_link (priv->pending, priv->pending);
    }
    g_mutex_unlock (priv->mutex);

 done:
    g_object_unref (children);
    g_object_unref (gfile);

    priv->thread_running = FALSE;
    g_object_unref (mallard);
}

static void
mallard_try_run (YelpMallard *mallard,
                 gchar       *page_id)
{
    /* We expect to be in a locked mutex when this function is called. */
    MallardPageData *page_data;
    YelpError *error;

    page_data = g_hash_table_lookup (mallard->priv->pages_hash, page_id);
    if (page_data == NULL) {
	error = yelp_error_new (_("Page not found"),
				_("The page %s was not found in the document %s."),
				page_id, mallard->priv->directory);
	yelp_document_error_page (YELP_DOCUMENT (mallard), page_id, error);
        return;
    }

    mallard_page_data_run (page_data);
}

/******************************************************************************/
/** MallardPageData ***********************************************************/

static void
mallard_page_data_walk (MallardPageData *page_data)
{
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
        page_data->cur = xmlDocGetRootElement (page_data->xmldoc);
        page_data->cache = xmlDocGetRootElement (page_data->mallard->priv->cache);
        mallard_page_data_walk (page_data);
    } else {
        xmlNodePtr child, oldcur, oldcache, info;

        id = xmlGetProp (page_data->cur, BAD_CAST "id");
        if (id == NULL)
            goto done;

        page_data->cache = xmlNewChild (page_data->cache,
                                        page_data->mallard->priv->cache_mal_ns,
                                        page_data->cur->name,
                                        NULL);

        if (xmlStrEqual (page_data->cur->name, BAD_CAST "page")) {
            page_data->page_id = g_strdup ((gchar *) id);
            xmlSetProp (page_data->cache, BAD_CAST "id", id);
        } else {
            gchar *newid = g_strdup_printf ("%s#%s", page_data->page_id, id);
            xmlSetProp (page_data->cache, BAD_CAST "id", BAD_CAST newid);
            g_free (newid);
        }

        info = xmlNewChild (page_data->cache,
                            page_data->mallard->priv->cache_mal_ns,
                            BAD_CAST "info", NULL);
        page_data->link_title = FALSE;
        page_data->sort_title = FALSE;
        for (child = page_data->cur->children; child; child = child->next) {
            if (child->type != XML_ELEMENT_NODE)
                continue;
            if (xmlStrEqual (child->name, BAD_CAST "info")) {
                mallard_page_data_info (page_data, child, info);
            }
            else if (xmlStrEqual (child->name, BAD_CAST "title")) {
                xmlNodePtr node;
                xmlNodePtr title_node = xmlNewChild (page_data->cache,
                                                     page_data->mallard->priv->cache_mal_ns,
                                                     BAD_CAST "title", NULL);
                for (node = child->children; node; node = node->next) {
                    xmlAddChild (title_node, xmlCopyNode (node, 1));
                }
                if (!page_data->link_title) {
                    xmlNodePtr title_node = xmlNewChild (info,
                                                         page_data->mallard->priv->cache_mal_ns,
                                                         BAD_CAST "title", NULL);
                    xmlSetProp (title_node, BAD_CAST "type", BAD_CAST "link");
                    for (node = child->children; node; node = node->next) {
                        xmlAddChild (title_node, xmlCopyNode (node, 1));
                    }
                }
                if (!page_data->sort_title) {
                    xmlNodePtr title_node = xmlNewChild (info,
                                                         page_data->mallard->priv->cache_mal_ns,
                                                         BAD_CAST "title", NULL);
                    xmlSetProp (title_node, BAD_CAST "type", BAD_CAST "sort");
                    for (node = child->children; node; node = node->next) {
                        xmlAddChild (title_node, xmlCopyNode (node, 1));
                    }
                }
            }
            else if (xmlStrEqual (child->name, BAD_CAST "section")) {
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
        if (xmlStrEqual (child->name, BAD_CAST "info")) {
            mallard_page_data_info (page_data, child, cache_node);
        }
        else if (xmlStrEqual (child->name, BAD_CAST "title")) {
            xmlNodePtr node, title_node;
            xmlChar *type, *role;
            title_node = xmlCopyNode (child, 1);
            xmlAddChild (cache_node, title_node);

            type = xmlGetProp (child, BAD_CAST "type");
            role = xmlGetProp (child, BAD_CAST "role");

            if (xmlStrEqual (type, BAD_CAST "link") && role == NULL)
                page_data->link_title = TRUE;
            if (xmlStrEqual (type, BAD_CAST "sort"))
                page_data->sort_title = TRUE;
        }
        else if (xmlStrEqual (child->name, BAD_CAST "desc") ||
                 xmlStrEqual (child->name, BAD_CAST "link")) {
            xmlAddChild (cache_node, xmlCopyNode (child, 1));
        }
    }
}

static void
mallard_page_data_run (MallardPageData *page_data)
{
    gint  params_i = 0;
    gint  params_max = 10;
    gchar **params = NULL;
    page_data->transform = yelp_transform_new (STYLESHEET,
                                               (YelpTransformFunc) transform_func,
                                               page_data);
    page_data->mallard->priv->transforms_running++;

    params = g_new0 (gchar *, params_max);
    yelp_settings_params (&params, &params_i, &params_max);
    params[params_i] = NULL;

    yelp_transform_set_input (page_data->transform,
                              page_data->mallard->priv->cache);

    yelp_transform_start (page_data->transform,
			  page_data->xmldoc,
			  params);
    g_strfreev (params);
}

static void
mallard_page_data_free (MallardPageData *page_data)
{
    g_free (page_data->page_id);
    g_free (page_data->filename);
    if (page_data->xmldoc)
        xmlFreeDoc (page_data->xmldoc);
    if (page_data->transform)
	yelp_transform_release (page_data->transform);
    g_free (page_data);
}

/******************************************************************************/
/** YelpTransform *************************************************************/

static void
transform_func (YelpTransform       *transform,
		YelpTransformSignal  signal,
		gpointer             func_data,
		MallardPageData     *page_data)
{
    YelpMallardPriv *priv;
    debug_print (DB_FUNCTION, "entering\n");

    g_assert (page_data != NULL && page_data->mallard != NULL &&
              YELP_IS_MALLARD (page_data->mallard));
    g_assert (transform == page_data->transform);

    priv = page_data->mallard->priv;

    if (priv->state == MALLARD_STATE_STOP) {
	switch (signal) {
	case YELP_TRANSFORM_CHUNK:
	    g_free (func_data);
	    break;
	case YELP_TRANSFORM_ERROR:
	    yelp_error_free ((YelpError *) func_data);
	    break;
	case YELP_TRANSFORM_FINAL:
	    break;
	}
	yelp_transform_release (transform);
	page_data->transform = NULL;
	priv->transforms_running--;
	return;
    }

    switch (signal) {
    case YELP_TRANSFORM_CHUNK:
	transform_page_func (transform, (gchar *) func_data, page_data);
	break;
    case YELP_TRANSFORM_ERROR:
	yelp_document_error_pending (YELP_DOCUMENT (page_data->mallard), (YelpError *) func_data);
	yelp_transform_release (transform);
	page_data->transform = NULL;
	priv->transforms_running -= 1;
	break;
    case YELP_TRANSFORM_FINAL:
	transform_final_func (transform, page_data);
	break;
    }
}

static void
transform_page_func (YelpTransform   *transform,
		     gchar           *page_id,
		     MallardPageData *page_data)
{
    YelpMallardPriv *priv;
    gchar *content;

    debug_print (DB_FUNCTION, "entering\n");

    priv = page_data->mallard->priv;
    g_mutex_lock (priv->mutex);

    content = yelp_transform_eat_chunk (transform, page_id);
    yelp_document_add_page (YELP_DOCUMENT (page_data->mallard), page_id, content);

    g_free (page_id);

    g_mutex_unlock (priv->mutex);
}

static void
transform_final_func (YelpTransform *transform, MallardPageData *page_data)
{
    YelpMallardPriv *priv;

    debug_print (DB_FUNCTION, "entering\n");

    priv = page_data->mallard->priv;
    g_mutex_lock (priv->mutex);

    yelp_transform_release (transform);
    page_data->transform = NULL;
    priv->transforms_running -= 1;

    if (page_data->xmldoc)
	xmlFreeDoc (page_data->xmldoc);
    page_data->xmldoc = NULL;

    g_mutex_unlock (priv->mutex);
}
