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

#include "yelp-debug.h"
#include "yelp-document.h"
#include "yelp-error.h"
#include "yelp-docbook-document.h"
#include "yelp-help-list.h"
#include "yelp-info-document.h"
#include "yelp-mallard-document.h"
#include "yelp-man-document.h"
#include "yelp-man-search.h"
#include "yelp-settings.h"
#include "yelp-simple-document.h"
#include "yelp-storage.h"
#include "yelp-transform.h"

#define LINKS_STYLESHEET DATADIR"/yelp/xslt/links2html.xsl"

enum {
    PROP_0,
    PROP_URI,
    PROP_INDEXED
};

typedef struct _Request Request;
struct _Request {
    YelpDocument         *document;
    gchar                *page_id;
    GCancellable         *cancellable;
    YelpDocumentCallback  callback;
    gpointer              user_data;
    GDestroyNotify        notify;
    GError               *error;

    gint                  idle_funcs;
};

typedef struct _Hash Hash;
struct _Hash {
    gpointer        null;
    GHashTable     *hash;
    GDestroyNotify  destroy;
};

struct _YelpDocumentPrivate {
    GMutex  mutex;

    GSList *reqs_all;         /* Holds canonical refs, only free from here */
    Hash   *reqs_by_page_id;  /* Indexed by page ID, contains GSList */
    GSList *reqs_pending;     /* List of requests that need a page */

    GSList *reqs_search;      /* Pending search requests, not in reqs_all */
    gboolean indexed;

    YelpUri *uri;
    gchar   *doc_uri;

    /* Real page IDs map to themselves, so this list doubles
     * as a list of all valid page IDs.
     */
    GHashTable *core_ids;  /* Mapping of real IDs to themselves, for a set */
    Hash   *page_ids;      /* Mapping of fragment IDs to real page IDs */
    Hash   *titles;        /* Mapping of page IDs to titles */
    Hash   *descs;         /* Mapping of page IDs to descs */
    Hash   *keywords;      /* Mapping of page IDs to keywords */
    Hash   *icons;         /* Mapping of page IDs to icons */
    Hash   *mime_types;    /* Mapping of page IDs to mime types */
    Hash   *contents;      /* Mapping of page IDs to string content */

    Hash   *root_ids;      /* Mapping of page IDs to "root page" IDs */
    Hash   *prev_ids;      /* Mapping of page IDs to "previous page" IDs */
    Hash   *next_ids;      /* Mapping of page IDs to "next page" IDs */
    Hash   *up_ids;        /* Mapping of page IDs to "up page" IDs */

    GError *idle_error;
};

G_DEFINE_TYPE_WITH_PRIVATE (YelpDocument, yelp_document, G_TYPE_OBJECT)

static void           yelp_document_dispose     (GObject              *object);
static void           yelp_document_finalize    (GObject              *object);
static void           document_get_property     (GObject              *object,
                                                 guint                 prop_id,
                                                 GValue               *value,
                                                 GParamSpec           *pspec);
static void           document_set_property     (GObject              *object,
                                                 guint                 prop_id,
                                                 const GValue         *value,
                                                 GParamSpec           *pspec);
static gboolean       document_request_page     (YelpDocument         *document,
                                                 const gchar          *page_id,
                                                 GCancellable         *cancellable,
                                                 YelpDocumentCallback  callback,
                                                 gpointer              user_data,
                                                 GDestroyNotify        notify);
static gboolean       document_indexed          (YelpDocument         *document);
static const gchar *  document_read_contents    (YelpDocument         *document,
                                                 const gchar          *page_id);
static void           document_finish_read      (YelpDocument         *document,
                                                 const gchar          *contents);
static gchar *        document_get_mime_type    (YelpDocument         *document,
                                                 const gchar          *mime_type);
static void           document_index            (YelpDocument         *document);

static Hash *         hash_new                  (GDestroyNotify        destroy);
static void           hash_free                 (Hash                 *hash);
static gpointer       hash_lookup               (Hash                 *hash,
                                                 const gchar          *key);
static void           hash_replace              (Hash                 *hash,
                                                 const gchar          *key,
                                                 gpointer              value);
static void           hash_remove               (Hash                 *hash,
                                                 const gchar          *key);
static void           hash_slist_insert         (Hash                 *hash,
                                                 const gchar          *key,
                                                 gpointer              value);
static void           hash_slist_remove         (Hash                 *hash,
                                                 const gchar          *key,
                                                 gpointer              value);

static void           request_cancel            (GCancellable         *cancellable,
                                                 Request              *request);
static gboolean       request_idle_contents     (Request              *request);
static gboolean       request_idle_info         (Request              *request);
static gboolean       request_idle_error        (Request              *request);
static gboolean       request_try_free          (Request              *request);
static void           request_free              (Request              *request);

static const gchar *  str_ref                   (const gchar          *str);
static void           str_unref                 (const gchar          *str);

static GMutex str_mutex;
static GHashTable  *str_refs  = NULL;
static GHashTable *documents = NULL;

/******************************************************************************/

YelpDocument *
yelp_document_lookup_document_uri (const gchar *docuri)
{
    if (!documents)
        return NULL;

    return g_hash_table_lookup (documents, docuri);
}

YelpDocument *
yelp_document_get_for_uri (YelpUri *uri)
{
    YelpUriDocumentType doctype;
    gchar *docuri = NULL;
    gchar *page_id, *tmp;
    YelpDocument *document = NULL;

    if (documents == NULL)
        documents = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           g_free, g_object_unref);

    g_return_val_if_fail (yelp_uri_is_resolved (uri), NULL);

    doctype = yelp_uri_get_document_type (uri);

    if (doctype == YELP_URI_DOCUMENT_TYPE_TEXT ||
        doctype == YELP_URI_DOCUMENT_TYPE_HTML ||
        doctype == YELP_URI_DOCUMENT_TYPE_XHTML) {
        /* We use YelpSimpleDocument for these, which is a single-file
         * responder. But the document URI may be set to the directory
         * holding the file, to allow a directory of HTML files to act
         * as a single document. So we cache these by a fuller URI.
         */
        docuri = yelp_uri_get_document_uri (uri);
        page_id = yelp_uri_get_page_id (uri);
        tmp = g_strconcat (docuri, "/", page_id, NULL);
        g_free (docuri);
        g_free (page_id);
        docuri = tmp;
    }
    else if (doctype == YELP_URI_DOCUMENT_TYPE_MAN) {
        /* The document URI for man pages is just man:, so we use the
         * full canonical URI to look these up.
         */
        docuri = yelp_uri_get_canonical_uri (uri);
    }
    else {
        docuri = yelp_uri_get_document_uri (uri);
    }

    if (docuri == NULL)
        return NULL;
    document = g_hash_table_lookup (documents, docuri);

    if (document != NULL) {
        g_free (docuri);
        return g_object_ref (document);
    }

    switch (doctype) {
    case YELP_URI_DOCUMENT_TYPE_TEXT:
    case YELP_URI_DOCUMENT_TYPE_HTML:
    case YELP_URI_DOCUMENT_TYPE_XHTML:
        document = yelp_simple_document_new (uri);
        break;
    case YELP_URI_DOCUMENT_TYPE_DOCBOOK:
        document = yelp_docbook_document_new (uri);
        break;
    case YELP_URI_DOCUMENT_TYPE_MALLARD:
        document = yelp_mallard_document_new (uri);
        break;
    case YELP_URI_DOCUMENT_TYPE_MAN:
        document = yelp_man_document_new (uri);
        break;
    case YELP_URI_DOCUMENT_TYPE_INFO:
        document = yelp_info_document_new (uri);
        break;
    case YELP_URI_DOCUMENT_TYPE_HELP_LIST:
        document = yelp_help_list_new (uri);
        break;
    case YELP_URI_DOCUMENT_TYPE_NOT_FOUND:
    case YELP_URI_DOCUMENT_TYPE_EXTERNAL:
    case YELP_URI_DOCUMENT_TYPE_ERROR:
        break;
    case YELP_URI_DOCUMENT_TYPE_UNRESOLVED:
    default:
        g_assert_not_reached ();
    }

    if (document != NULL) {
	g_hash_table_insert (documents, docuri, document);
	return g_object_ref (document);
    }

    g_free (docuri);
    return NULL;
}

/******************************************************************************/

static void
yelp_document_class_init (YelpDocumentClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose  = yelp_document_dispose;
    object_class->finalize = yelp_document_finalize;
    object_class->get_property = document_get_property;
    object_class->set_property = document_set_property;

    klass->request_page =   document_request_page;
    klass->read_contents =  document_read_contents;
    klass->finish_read =    document_finish_read;
    klass->get_mime_type =  document_get_mime_type;
    klass->index =          document_index;

    g_object_class_install_property (object_class,
                                     PROP_INDEXED,
                                     g_param_spec_boolean ("indexed",
                                                           "Indexed",
                                                           "Whether the document content has been indexed",
                                                           FALSE,
                                                           G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
                                                           G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (object_class,
                                     PROP_URI,
                                     g_param_spec_object ("document-uri",
                                                          "Document URI",
                                                          "The URI which identifies the document",
                                                          YELP_TYPE_URI,
                                                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS));
}

static void
yelp_document_init (YelpDocument *document)
{
    YelpDocumentPrivate *priv;

    document->priv = priv = yelp_document_get_instance_private (document);

    g_mutex_init (&priv->mutex);

    priv->reqs_by_page_id = hash_new ((GDestroyNotify) g_slist_free);
    priv->reqs_all = NULL;
    priv->reqs_pending = NULL;
    priv->reqs_search = NULL;

    priv->core_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    priv->page_ids = hash_new (g_free );
    priv->titles = hash_new (g_free);
    priv->descs = hash_new (g_free);
    priv->keywords = hash_new (g_free);
    priv->icons = hash_new (g_free);
    priv->mime_types = hash_new (g_free);
    priv->contents = hash_new ((GDestroyNotify) str_unref);

    priv->root_ids = hash_new (g_free);
    priv->prev_ids = hash_new (g_free);
    priv->next_ids = hash_new (g_free);
    priv->up_ids = hash_new (g_free);
}

static void
yelp_document_dispose (GObject *object)
{
    YelpDocument *document = YELP_DOCUMENT (object);

    if (document->priv->reqs_all) {
	g_slist_foreach (document->priv->reqs_all,
			 (GFunc)(GCallback) request_try_free,
			 NULL);
	g_slist_free (document->priv->reqs_all);
	document->priv->reqs_all = NULL;
    }

    if (document->priv->reqs_search) {
	g_slist_foreach (document->priv->reqs_search,
			 (GFunc)(GCallback) request_try_free,
			 NULL);
	g_slist_free (document->priv->reqs_search);
	document->priv->reqs_search = NULL;
    }

    G_OBJECT_CLASS (yelp_document_parent_class)->dispose (object);
}

static void
yelp_document_finalize (GObject *object)
{
    YelpDocument *document = YELP_DOCUMENT (object);

    g_clear_object (&document->priv->uri);
    g_free (document->priv->doc_uri);

    g_slist_free (document->priv->reqs_pending);
    hash_free (document->priv->reqs_by_page_id);

    hash_free (document->priv->page_ids);
    hash_free (document->priv->titles);
    hash_free (document->priv->descs);
    hash_free (document->priv->keywords);
    hash_free (document->priv->icons);
    hash_free (document->priv->mime_types);

    hash_free (document->priv->contents);

    hash_free (document->priv->root_ids);
    hash_free (document->priv->prev_ids);
    hash_free (document->priv->next_ids);
    hash_free (document->priv->up_ids);

    g_hash_table_destroy (document->priv->core_ids);

    g_mutex_clear (&document->priv->mutex);

    G_OBJECT_CLASS (yelp_document_parent_class)->finalize (object);
}

static void
document_get_property (GObject      *object,
                       guint         prop_id,
                       GValue       *value,
                       GParamSpec   *pspec)
{
    YelpDocument *document = YELP_DOCUMENT (object);

    switch (prop_id) {
    case PROP_INDEXED:
        g_value_set_boolean (value, document->priv->indexed);
        break;
    case PROP_URI:
        g_value_set_object (value, document->priv->uri);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
document_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
    YelpDocument *document = YELP_DOCUMENT (object);

    switch (prop_id) {
    case PROP_INDEXED:
        document->priv->indexed = g_value_get_boolean (value);
        if (document->priv->indexed)
            g_idle_add ((GSourceFunc) document_indexed, document);
        break;
    case PROP_URI:
        document->priv->uri = g_value_dup_object (value);
        if (document->priv->uri)
            document->priv->doc_uri = yelp_uri_get_document_uri (document->priv->uri);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/******************************************************************************/

YelpUri *
yelp_document_get_uri (YelpDocument *document)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    return document->priv->uri;
}

gchar **
yelp_document_list_page_ids (YelpDocument *document)
{
    GList *lst, *cur;
    gint i;
    gchar **ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);

    lst = g_hash_table_get_keys (document->priv->core_ids);
    ret = g_new0 (gchar *, g_list_length (lst) + 1);
    i = 0;
    for (cur = lst; cur != NULL; cur = cur->next) {
        ret[i] = g_strdup ((gchar *) cur->data);
        i++;
    }
    g_list_free (lst);

    g_mutex_unlock (&document->priv->mutex);

    return ret;
}

gchar *
yelp_document_get_page_id (YelpDocument *document,
			   const gchar  *id)
{
    gchar *ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    if (id != NULL && g_str_has_prefix (id, "search="))
        return g_strdup (id);

    g_mutex_lock (&document->priv->mutex);
    ret = hash_lookup (document->priv->page_ids, id);
    if (ret)
	ret = g_strdup (ret);

    g_mutex_unlock (&document->priv->mutex);

    return ret;
}

void
yelp_document_set_page_id (YelpDocument *document,
			   const gchar  *id,
			   const gchar  *page_id)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);

    hash_replace (document->priv->page_ids, id, g_strdup (page_id));

    if (id == NULL || !g_str_equal (id, page_id)) {
	GSList *reqs, *cur;
	reqs = hash_lookup (document->priv->reqs_by_page_id, id);
	for (cur = reqs; cur != NULL; cur = cur->next) {
	    Request *request = (Request *) cur->data;
            if (request == NULL)
                continue;
	    g_free (request->page_id);
	    request->page_id = g_strdup (page_id);
	    hash_slist_insert (document->priv->reqs_by_page_id, page_id, request);
	}
	if (reqs) {
	    hash_remove (document->priv->reqs_by_page_id, id);
        }
    }

    if (page_id != NULL) {
        if (g_hash_table_lookup (document->priv->core_ids, page_id) == NULL) {
            gchar *ins = g_strdup (page_id);
            g_hash_table_insert (document->priv->core_ids, ins, ins);
        }
    }

    g_mutex_unlock (&document->priv->mutex);
}

gchar *
yelp_document_get_root_id (YelpDocument *document,
			   const gchar  *page_id)
{
    gchar *real, *ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);
    if (page_id != NULL && g_str_has_prefix (page_id, "search="))
        real = hash_lookup (document->priv->page_ids, NULL);
    else
        real = hash_lookup (document->priv->page_ids, page_id);
    if (real) {
	ret = hash_lookup (document->priv->root_ids, real);
	if (ret)
	    ret = g_strdup (ret);
    }
    g_mutex_unlock (&document->priv->mutex);

    return ret;
}

void
yelp_document_set_root_id (YelpDocument *document,
			   const gchar  *page_id,
			   const gchar  *root_id)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);
    hash_replace (document->priv->root_ids, page_id, g_strdup (root_id));
    g_mutex_unlock (&document->priv->mutex);
}

gchar *
yelp_document_get_prev_id (YelpDocument *document,
			   const gchar  *page_id)
{
    gchar *real, *ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);
    real = hash_lookup (document->priv->page_ids, page_id);
    if (real) {
	ret = hash_lookup (document->priv->prev_ids, real);
	if (ret)
	    ret = g_strdup (ret);
    }
    g_mutex_unlock (&document->priv->mutex);

    return ret;
}

void
yelp_document_set_prev_id (YelpDocument *document,
			   const gchar  *page_id,
			   const gchar  *prev_id)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);
    hash_replace (document->priv->prev_ids, page_id, g_strdup (prev_id));
    g_mutex_unlock (&document->priv->mutex);
}

gchar *
yelp_document_get_next_id (YelpDocument *document,
			   const gchar  *page_id)
{
    gchar *real, *ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);
    real = hash_lookup (document->priv->page_ids, page_id);
    if (real) {
	ret = hash_lookup (document->priv->next_ids, real);
	if (ret)
	    ret = g_strdup (ret);
    }
    g_mutex_unlock (&document->priv->mutex);

    return ret;
}

void
yelp_document_set_next_id (YelpDocument *document,
			   const gchar  *page_id,
			   const gchar  *next_id)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);
    hash_replace (document->priv->next_ids, page_id, g_strdup (next_id));
    g_mutex_unlock (&document->priv->mutex);
}

gchar *
yelp_document_get_up_id (YelpDocument *document,
			 const gchar  *page_id)
{
    gchar *real, *ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);
    real = hash_lookup (document->priv->page_ids, page_id);
    if (real) {
	ret = hash_lookup (document->priv->up_ids, real);
	if (ret)
	    ret = g_strdup (ret);
    }
    g_mutex_unlock (&document->priv->mutex);

    return ret;
}

void
yelp_document_set_up_id (YelpDocument *document,
			 const gchar  *page_id,
			 const gchar  *up_id)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);
    hash_replace (document->priv->up_ids, page_id, g_strdup (up_id));
    g_mutex_unlock (&document->priv->mutex);
}

gchar *
yelp_document_get_root_title (YelpDocument *document,
                              const gchar  *page_id)
{
    gchar *real, *root, *ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);
    if (page_id != NULL && g_str_has_prefix (page_id, "search=")) {
        ret = yelp_storage_get_root_title (yelp_storage_get_default (),
                                           document->priv->doc_uri);
    }
    else {
        real = hash_lookup (document->priv->page_ids, page_id);
        if (real) {
            root = hash_lookup (document->priv->root_ids, real);
            if (root) {
                ret = hash_lookup (document->priv->titles, root);
                if (ret)
                    ret = g_strdup (ret);
            }
        }
    }

    g_mutex_unlock (&document->priv->mutex);

    return ret;
}

gchar *
yelp_document_get_page_title (YelpDocument *document,
                              const gchar  *page_id)
{
    gchar *real, *ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    if (page_id != NULL && g_str_has_prefix (page_id, "search=")) {
        ret = g_uri_unescape_string (page_id + 7, NULL);
        return ret;
    }

    g_mutex_lock (&document->priv->mutex);
    real = hash_lookup (document->priv->page_ids, page_id);
    if (real) {
	ret = hash_lookup (document->priv->titles, real);
	if (ret)
	    ret = g_strdup (ret);
    }
    g_mutex_unlock (&document->priv->mutex);

    return ret;
}

void
yelp_document_set_page_title (YelpDocument *document,
                              const gchar  *page_id,
                              const gchar  *title)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);
    hash_replace (document->priv->titles, page_id, g_strdup (title));
    g_mutex_unlock (&document->priv->mutex);
}

gchar *
yelp_document_get_page_desc (YelpDocument *document,
                             const gchar  *page_id)
{
    gchar *real, *ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    if (page_id != NULL && g_str_has_prefix (page_id, "search="))
        return yelp_document_get_root_title (document, page_id);

    g_mutex_lock (&document->priv->mutex);
    real = hash_lookup (document->priv->page_ids, page_id);
    if (real) {
	ret = hash_lookup (document->priv->descs, real);
	if (ret)
	    ret = g_strdup (ret);
    }
    g_mutex_unlock (&document->priv->mutex);

    return ret;
}

void
yelp_document_set_page_desc (YelpDocument *document,
                             const gchar  *page_id,
                             const gchar  *desc)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);
    hash_replace (document->priv->descs, page_id, g_strdup (desc));
    g_mutex_unlock (&document->priv->mutex);
}

gchar *
yelp_document_get_page_keywords (YelpDocument *document,
                                 const gchar  *page_id)
{
    gchar *real, *ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    if (page_id != NULL && g_str_has_prefix (page_id, "search="))
        return NULL;

    g_mutex_lock (&document->priv->mutex);
    real = hash_lookup (document->priv->page_ids, page_id);
    if (real) {
	ret = hash_lookup (document->priv->keywords, real);
	if (ret)
	    ret = g_strdup (ret);
    }
    g_mutex_unlock (&document->priv->mutex);

    return ret;
}

void
yelp_document_set_page_keywords (YelpDocument *document,
                                 const gchar  *page_id,
                                 const gchar  *keywords)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);
    hash_replace (document->priv->keywords, page_id, g_strdup (keywords));
    g_mutex_unlock (&document->priv->mutex);
}

gchar *
yelp_document_get_page_icon (YelpDocument *document,
                             const gchar  *page_id)
{
    gchar *real, *ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    if (page_id != NULL && g_str_has_prefix (page_id, "search="))
        return g_strdup ("yelp-page-search-symbolic");

    g_mutex_lock (&document->priv->mutex);
    real = hash_lookup (document->priv->page_ids, page_id);
    if (real) {
	ret = hash_lookup (document->priv->icons, real);
	if (ret)
	    ret = g_strdup (ret);
    }
    g_mutex_unlock (&document->priv->mutex);

    if (ret == NULL)
        ret = g_strdup ("yelp-page-symbolic");

    return ret;
}

void
yelp_document_set_page_icon (YelpDocument *document,
                             const gchar  *page_id,
                             const gchar  *icon)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);
    hash_replace (document->priv->icons, page_id, g_strdup (icon));
    g_mutex_unlock (&document->priv->mutex);
}


static void
search_transform_chunk_ready (YelpTransform   *transform,
                              gchar           *chunk_id,
                              YelpDocument    *document)
{
    gchar *content;
    content = yelp_transform_take_chunk (transform, chunk_id);
    yelp_document_give_contents (document,
                                 chunk_id,
                                 content,
                                 "application/xhtml+xml");
    yelp_document_signal (document,
                          chunk_id,
                          YELP_DOCUMENT_SIGNAL_CONTENTS,
                          NULL);
}


static void
search_transform_finished (YelpTransform   *transform,
                           YelpDocument    *document)
{
    xmlFreeDoc (yelp_transform_get_xmldoc (transform));
    g_object_unref (transform);
}


static gboolean
document_indexed (YelpDocument *document)
{
    g_mutex_lock (&document->priv->mutex);

    while (document->priv->reqs_search != NULL) {
        xmlDocPtr resultsdoc;
        xmlNodePtr rootnode;
        gchar *term, *text;
        GVariant *value;
        GVariantIter *iter;
        YelpTransform *transform;
        gchar **params = NULL;
        Request *request = (Request *) document->priv->reqs_search->data;

        resultsdoc = xmlNewDoc (BAD_CAST "1.0");
        rootnode = xmlNewDocNode (resultsdoc, NULL, BAD_CAST "links", NULL);
        xmlDocSetRootElement (resultsdoc, rootnode);
        xmlSetProp (rootnode, BAD_CAST "id", BAD_CAST request->page_id);

        xmlSetProp (rootnode, BAD_CAST "docref", BAD_CAST request->page_id);

        text = yelp_storage_get_root_title (yelp_storage_get_default (),
                                            document->priv->doc_uri);
        if (text) {
            xmlSetProp (rootnode, BAD_CAST "docref", BAD_CAST "xref:");
            xmlSetProp (rootnode, BAD_CAST "doctitle", BAD_CAST text);
            g_free (text);
        }

        term = g_uri_unescape_string (request->page_id + 7, "");
        text = g_strdup_printf (_("Search results for “%s”"), term);
        xmlNewTextChild (rootnode, NULL, BAD_CAST "title", BAD_CAST text);
        g_free (text);

        value = YELP_IS_MAN_DOCUMENT (document)
                ? yelp_man_search (term)
                : yelp_storage_search (yelp_storage_get_default (),
                                       document->priv->doc_uri,
                                       term);

        iter = g_variant_iter_new (value);
        if (g_variant_iter_n_children (iter) == 0) {
            xmlNewTextChild (rootnode, NULL, BAD_CAST "p",
                             BAD_CAST _("No matching help pages found."));
        }
        else {
            gchar *url, *title, *desc, *icon; /* do not free */
            while (g_variant_iter_loop (iter, "(&s&s&s&s)", &url, &title, &desc, &icon)) {
                xmlNodePtr linknode;
                gchar *xref_uri = NULL;

                if (g_str_has_prefix (url, document->priv->doc_uri)) {
                    gchar *urloffset = url + strlen(document->priv->doc_uri) + 1; /* do not free */
                    if (urloffset[0] == '?')
                        urloffset += 1; /* handle oddity of old ghelp URIs */
                    xref_uri = g_strdup_printf ("xref:%s", urloffset);
                }
                linknode = xmlNewChild (rootnode, NULL, BAD_CAST "link", NULL);
                xmlSetProp (linknode, BAD_CAST "href", BAD_CAST xref_uri);
                xmlNewTextChild (linknode, NULL, BAD_CAST "title", BAD_CAST title);
                xmlNewTextChild (linknode, NULL, BAD_CAST "desc", BAD_CAST desc);
                g_free (xref_uri);
            }
        }
        g_variant_iter_free (iter);
        g_variant_unref (value);
        g_free (term);

        params = yelp_settings_get_all_params (yelp_settings_get_default (), 0, NULL);
        transform = yelp_transform_new (LINKS_STYLESHEET);
        g_signal_connect (transform, "chunk-ready",
                          (GCallback) search_transform_chunk_ready,
                          document);
        g_signal_connect (transform, "finished",
                          (GCallback) search_transform_finished,
                          document);
        yelp_transform_start (transform, resultsdoc, NULL,
                              (const gchar * const *) params);
        g_strfreev (params);

        document->priv->reqs_search = g_slist_delete_link (document->priv->reqs_search,
                                                           document->priv->reqs_search);
    }

    g_mutex_unlock (&document->priv->mutex);

    return FALSE;
}

/******************************************************************************/

gboolean
yelp_document_request_page (YelpDocument         *document,
			    const gchar          *page_id,
			    GCancellable         *cancellable,
			    YelpDocumentCallback  callback,
			    gpointer              user_data,
			    GDestroyNotify        notify)
{
    g_return_val_if_fail (YELP_IS_DOCUMENT (document), FALSE);
    g_return_val_if_fail (YELP_DOCUMENT_GET_CLASS (document)->request_page != NULL, FALSE);

    if (page_id && g_str_has_prefix (page_id, "search="))
        return document_request_page (document,
                                      page_id,
                                      cancellable,
                                      callback,
                                      user_data,
                                      notify);

    return YELP_DOCUMENT_GET_CLASS (document)->request_page (document,
							     page_id,
							     cancellable,
							     callback,
							     user_data,
							     notify);
}

static gboolean
document_request_page (YelpDocument         *document,
		       const gchar          *page_id,
		       GCancellable         *cancellable,
		       YelpDocumentCallback  callback,
		       gpointer              user_data,
		       GDestroyNotify        notify)
{
    Request *request;
    gchar *real_id;
    gboolean ret = FALSE;

    request = g_slice_new0 (Request);
    request->document = g_object_ref (document);

    real_id = hash_lookup (document->priv->page_ids, page_id);
    if (real_id)
	request->page_id = g_strdup (real_id);
    else
	request->page_id = g_strdup (page_id);

    if (cancellable) {
      request->cancellable = g_object_ref (cancellable);
      g_signal_connect (cancellable, "cancelled",
              G_CALLBACK (request_cancel), request);
    }
    else {
      request->cancellable = NULL;
    }

    request->callback = callback;
    request->user_data = user_data;
    request->notify = notify;
    request->idle_funcs = 0;

    g_mutex_lock (&document->priv->mutex);

    if (page_id && g_str_has_prefix (page_id, "search=")) {
        document->priv->reqs_search = g_slist_prepend (document->priv->reqs_search, request);
        if (document->priv->indexed)
            g_idle_add ((GSourceFunc) document_indexed, document);
        else
            yelp_document_index (document);
    }

    hash_slist_insert (document->priv->reqs_by_page_id,
		       request->page_id,
		       request);

    document->priv->reqs_all = g_slist_prepend (document->priv->reqs_all, request);
    if (page_id && g_str_has_prefix (page_id, "search=")) {
    }
    else {
        document->priv->reqs_pending = g_slist_prepend (document->priv->reqs_pending, request);
    }

    if (hash_lookup (document->priv->titles, request->page_id)) {
	request->idle_funcs++;
	g_idle_add ((GSourceFunc) request_idle_info, request);
    }

    if (hash_lookup (document->priv->contents, request->page_id)) {
	request->idle_funcs++;
	g_idle_add ((GSourceFunc) request_idle_contents, request);
	ret = TRUE;
    }

    g_mutex_unlock (&document->priv->mutex);

    return ret;
}

void
yelp_document_clear_contents (YelpDocument *document)
{
    g_mutex_lock (&document->priv->mutex);

    if (document->priv->contents->null) {
        str_unref (document->priv->contents->null);
        document->priv->contents->null = NULL;
    }
    g_hash_table_remove_all (document->priv->contents->hash);

    g_mutex_unlock (&document->priv->mutex);
}

gchar **
yelp_document_get_requests (YelpDocument *document)
{
    GList *reqs, *cur;
    gchar **ret;
    gint i;

    g_mutex_lock (&document->priv->mutex);

    reqs = g_hash_table_get_keys (document->priv->reqs_by_page_id->hash);
    ret = g_new0 (gchar*, g_list_length (reqs) + 1);
    for (cur = reqs, i = 0; cur; cur = cur->next, i++) {
        ret[i] = g_strdup ((gchar *) cur->data);
    }
    g_list_free (reqs);

    g_mutex_unlock (&document->priv->mutex);

    return ret;
}

/******************************************************************************/

const gchar *
yelp_document_read_contents (YelpDocument *document,
			     const gchar  *page_id)
{
    g_return_val_if_fail (YELP_IS_DOCUMENT (document), NULL);
    g_return_val_if_fail (YELP_DOCUMENT_GET_CLASS (document)->read_contents != NULL, NULL);

    if (page_id != NULL && g_str_has_prefix (page_id, "search="))
        return document_read_contents (document, page_id);

    return YELP_DOCUMENT_GET_CLASS (document)->read_contents (document, page_id);
}

static const gchar *
document_read_contents (YelpDocument *document,
			const gchar  *page_id)
{
    gchar *real, *str; /* do not free real */

    g_mutex_lock (&document->priv->mutex);

    if (page_id != NULL && g_str_has_prefix (page_id, "search="))
        real = (gchar *) page_id;
    else
        real = hash_lookup (document->priv->page_ids, page_id);

    str = hash_lookup (document->priv->contents, real);
    if (str)
	str_ref (str);

    g_mutex_unlock (&document->priv->mutex);

    return (const gchar *) str;
}

void
yelp_document_finish_read (YelpDocument *document,
			   const gchar  *contents)
{
    g_return_if_fail (YELP_IS_DOCUMENT (document));
    g_return_if_fail (YELP_DOCUMENT_GET_CLASS (document)->finish_read != NULL);

    YELP_DOCUMENT_GET_CLASS (document)->finish_read (document, contents);
}

static void
document_finish_read (YelpDocument *document,
		      const gchar  *contents)
{
    str_unref (contents);
}

void
yelp_document_give_contents (YelpDocument *document,
			     const gchar  *page_id,
			     gchar        *contents,
			     const gchar  *mime)
{
    g_return_if_fail (YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);

    hash_replace (document->priv->contents,
                  page_id,
                  (gpointer) str_ref (contents));

    hash_replace (document->priv->mime_types,
                  page_id,
                  g_strdup (mime));

    g_mutex_unlock (&document->priv->mutex);
}

gchar *
yelp_document_get_mime_type (YelpDocument *document,
			     const gchar  *page_id)
{
    g_return_val_if_fail (YELP_IS_DOCUMENT (document), NULL);
    g_return_val_if_fail (YELP_DOCUMENT_GET_CLASS (document)->get_mime_type != NULL, NULL);

    return YELP_DOCUMENT_GET_CLASS (document)->get_mime_type (document, page_id);
}

static gchar *
document_get_mime_type (YelpDocument *document,
			const gchar  *page_id)
{
    gchar *real, *ret = NULL; /* do not free real */

    g_mutex_lock (&document->priv->mutex);
    if (page_id != NULL && g_str_has_prefix (page_id, "search="))
        real = (gchar *) page_id;
    else
        real = hash_lookup (document->priv->page_ids, page_id);
    if (real) {
	ret = hash_lookup (document->priv->mime_types, real);
	if (ret)
	    ret = g_strdup (ret);
    }
    g_mutex_unlock (&document->priv->mutex);

    return ret;
}

/******************************************************************************/

void
yelp_document_index (YelpDocument *document)
{
    g_return_if_fail (YELP_IS_DOCUMENT (document));
    g_return_if_fail (YELP_DOCUMENT_GET_CLASS (document)->index != NULL);

    YELP_DOCUMENT_GET_CLASS (document)->index (document);
}

static void
document_index (YelpDocument *document)
{
    g_object_set (document, "indexed", TRUE, NULL);
}

/******************************************************************************/

void
yelp_document_signal (YelpDocument       *document,
		      const gchar        *page_id,
		      YelpDocumentSignal  signal,
		      const GError       *error)
{
    GSList *reqs, *cur;

    g_return_if_fail (YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);

    reqs = hash_lookup (document->priv->reqs_by_page_id, page_id);
    for (cur = reqs; cur != NULL; cur = cur->next) {
	Request *request = (Request *) cur->data;
	if (!request)
	    continue;
	switch (signal) {
	case YELP_DOCUMENT_SIGNAL_CONTENTS:
	    request->idle_funcs++;
	    g_idle_add ((GSourceFunc) request_idle_contents, request);
	    break;
	case YELP_DOCUMENT_SIGNAL_INFO:
	    request->idle_funcs++;
	    g_idle_add ((GSourceFunc) request_idle_info, request);
	    break;
	case YELP_DOCUMENT_SIGNAL_ERROR:
	    request->idle_funcs++;
	    request->error = yelp_error_copy ((GError *) error);
	    g_idle_add ((GSourceFunc) request_idle_error, request);
            break;
	default:
	    break;
	}
    }

    g_mutex_unlock (&document->priv->mutex);
}

static gboolean
yelp_document_error_pending_idle (YelpDocument *document)
{
    YelpDocumentPrivate *priv = yelp_document_get_instance_private (document);
    GSList *cur;
    Request *request;

    g_mutex_lock (&priv->mutex);

    if (priv->reqs_pending) {
	for (cur = priv->reqs_pending; cur; cur = cur->next) {
	    request = cur->data;
	    request->error = yelp_error_copy ((GError *) priv->idle_error);
	    request->idle_funcs++;
	    g_idle_add ((GSourceFunc) request_idle_error, request);
	}

	g_slist_free (priv->reqs_pending);
	priv->reqs_pending = NULL;
    }

    g_mutex_unlock (&priv->mutex);

    g_object_unref (document);
    return FALSE;
}

void
yelp_document_error_pending (YelpDocument *document,
			     const GError *error)
{
    YelpDocumentPrivate *priv = yelp_document_get_instance_private (document);

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_object_ref (document);
    priv->idle_error = g_error_copy (error);
    g_idle_add ((GSourceFunc) yelp_document_error_pending_idle, document);
}

/******************************************************************************/

static Hash *
hash_new (GDestroyNotify destroy)
{
    Hash *hash = g_new0 (Hash, 1);
    hash->destroy = destroy;
    hash->hash = g_hash_table_new_full (g_str_hash, g_str_equal,
					g_free, destroy);
    return hash;
}

static void
hash_free (Hash *hash)
{
    if (hash->null)
	hash->destroy (hash->null);
    g_hash_table_destroy (hash->hash);
    g_free (hash);
}

static gpointer
hash_lookup (Hash *hash, const gchar *key)
{
    if (key == NULL)
	return hash->null;
    else
	return g_hash_table_lookup (hash->hash, key);
}

static void
hash_replace (Hash        *hash,
              const gchar *key,
              gpointer     value)
{
    if (key == NULL) {
        if (hash->null)
            hash->destroy (hash->null);
        hash->null = value;
    }
    else
        g_hash_table_replace (hash->hash, g_strdup (key), value);
}

static void
hash_remove (Hash        *hash,
             const gchar *key)
{
    if (key == NULL) {
        if (hash->null) {
            hash->destroy (hash->null);
            hash->null = NULL;
        }
    }
    else
        g_hash_table_remove (hash->hash, key);
}

static void
hash_slist_insert (Hash        *hash,
                   const gchar *key,
                   gpointer     value)
{
    GSList *list;
    list = hash_lookup (hash, key);
    if (list) {
        list->next = g_slist_prepend (list->next, value);
    } else {
        list = g_slist_prepend (NULL, value);
        list = g_slist_prepend (list, NULL);
        if (key == NULL)
            hash->null = list;
        else
            g_hash_table_insert (hash->hash, g_strdup (key), list);
    }
}

static void
hash_slist_remove (Hash        *hash,
                   const gchar *key,
                   gpointer     value)
{
    GSList *list;
    list = hash_lookup (hash, key);
    if (list) {
        list = g_slist_remove (list, value);
        if (list->next == NULL)
            hash_remove (hash, key);
    }
}

/******************************************************************************/

static void
request_cancel (GCancellable *cancellable, Request *request)
{
    GSList *cur;
    YelpDocument *document = request->document;
    gboolean found = FALSE;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (&document->priv->mutex);

    document->priv->reqs_pending = g_slist_remove (document->priv->reqs_pending,
						   (gconstpointer) request);
    hash_slist_remove (document->priv->reqs_by_page_id,
		       request->page_id,
		       request);
    for (cur = document->priv->reqs_all; cur != NULL; cur = cur->next) {
	if (cur->data == request) {
	    document->priv->reqs_all = g_slist_delete_link (document->priv->reqs_all, cur);
            found = TRUE;
	    break;
	}
    }
    if (!found) {
        for (cur = document->priv->reqs_search; cur != NULL; cur = cur->next) {
            if (cur->data == request) {
                document->priv->reqs_search = g_slist_delete_link (document->priv->reqs_search, cur);
                break;
            }
        }
    }
    request_try_free (request);

    g_mutex_unlock (&document->priv->mutex);
}

static gboolean
request_idle_contents (Request *request)
{
    YelpDocument *document;
    YelpDocumentPrivate *priv;
    YelpDocumentCallback callback = NULL;
    gpointer user_data;

    g_assert (request != NULL && YELP_IS_DOCUMENT (request->document));

    if (g_cancellable_is_cancelled (request->cancellable)) {
	request->idle_funcs--;
	return FALSE;
    }

    document = g_object_ref (request->document);
    priv = yelp_document_get_instance_private (document);

    g_mutex_lock (&document->priv->mutex);

    priv->reqs_pending = g_slist_remove (priv->reqs_pending, request);

    callback = request->callback;
    user_data = request->user_data;
    request->idle_funcs--;

    g_mutex_unlock (&document->priv->mutex);

    if (callback)
	callback (document, YELP_DOCUMENT_SIGNAL_CONTENTS, user_data, NULL);

    g_object_unref (document);
    return FALSE;
}

static gboolean
request_idle_info (Request *request)
{
    YelpDocument *document;
    YelpDocumentCallback callback = NULL;
    gpointer user_data;

    g_assert (request != NULL && YELP_IS_DOCUMENT (request->document));

    if (g_cancellable_is_cancelled (request->cancellable)) {
	request->idle_funcs--;
	return FALSE;
    }

    document = g_object_ref (request->document);

    g_mutex_lock (&document->priv->mutex);

    callback = request->callback;
    user_data = request->user_data;
    request->idle_funcs--;

    g_mutex_unlock (&document->priv->mutex);

    if (callback)
	callback (document, YELP_DOCUMENT_SIGNAL_INFO, user_data, NULL);

    g_object_unref (document);
    return FALSE;
}

static gboolean
request_idle_error (Request *request)
{
    YelpDocument *document;
    YelpDocumentPrivate *priv;
    YelpDocumentCallback callback = NULL;
    GError *error = NULL;
    gpointer user_data;

    g_assert (request != NULL && YELP_IS_DOCUMENT (request->document));

    if (g_cancellable_is_cancelled (request->cancellable)) {
	request->idle_funcs--;
	return FALSE;
    }

    document = g_object_ref (request->document);
    priv = yelp_document_get_instance_private (document);

    g_mutex_lock (&priv->mutex);

    if (request->error) {
	callback = request->callback;
	user_data = request->user_data;
	error = request->error;
	priv->reqs_pending = g_slist_remove (priv->reqs_pending, request);
    }

    request->idle_funcs--;
    g_mutex_unlock (&priv->mutex);

    if (callback)
	callback (document,
		  YELP_DOCUMENT_SIGNAL_ERROR,
		  user_data,
		  error);

    g_clear_error (&request->error);
    g_object_unref (document);
    return FALSE;
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
    if (request->notify)
        request->notify (request->user_data);

    g_object_unref (request->document);
    g_free (request->page_id);
    g_object_unref (request->cancellable);

    if (request->error)
	g_error_free (request->error);

    g_slice_free (Request, request);
}

/******************************************************************************/

static const gchar *
str_ref (const gchar *str)
{
    gpointer p;
    guint i;

    g_mutex_lock (&str_mutex);
    if (str_refs == NULL)
	str_refs = g_hash_table_new (g_direct_hash, g_direct_equal);
    p = g_hash_table_lookup (str_refs, str);

    i = GPOINTER_TO_UINT (p);
    i++;
    p = GUINT_TO_POINTER (i);

    g_hash_table_insert (str_refs, (gpointer) str, p);
    g_mutex_unlock (&str_mutex);

    return str;
}

static void
str_unref (const gchar *str)
{
    gpointer p;
    guint i;

    g_mutex_lock (&str_mutex);
    p = g_hash_table_lookup (str_refs, str);

    i = GPOINTER_TO_UINT (p);
    i--;
    p = GUINT_TO_POINTER (i);

    if (i > 0) {
	g_hash_table_insert (str_refs, (gpointer) str, p);
    }
    else {
	g_hash_table_remove (str_refs, (gpointer) str);
	g_free ((gchar *) str);
    }

    g_mutex_unlock (&str_mutex);
}
