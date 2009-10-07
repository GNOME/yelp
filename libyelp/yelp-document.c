/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
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

#include "yelp-document.h"

typedef struct _Request Request;
struct _Request {
    YelpDocument         *document;
    gchar                *page_id;
    GCancellable         *cancellable;
    YelpDocumentCallback  callback;
    gpointer              user_data;

    gint                  idle_funcs;
};

struct _YelpDocumentPriv {
    GMutex       *mutex;

    GSList       *reqs_all;         /* Holds canonical refs, only free from here */
    GHashTable   *reqs_by_page_id;  /* Indexed by page ID, contains GSList */
    GSList       *reqs_pending;     /* List of requests that need a page */

    /* Real page IDs map to themselves, so this list doubles
     * as a list of all valid page IDs.
     */
    GHashTable   *page_ids;      /* Mapping of fragment IDs to real page IDs */
    GHashTable   *titles;        /* Mapping of page IDs to titles */
    GHashTable   *mime_types;    /* Mapping of page IDs to mime types */
    GHashTable   *contents;      /* Mapping of page IDs to string content */

    GHashTable   *root_ids;      /* Mapping of page IDs to "previous page" IDs */
    GHashTable   *prev_ids;      /* Mapping of page IDs to "previous page" IDs */
    GHashTable   *next_ids;      /* Mapping of page IDs to "next page" IDs */
    GHashTable   *up_ids;        /* Mapping of page IDs to "up page" IDs */

    GMutex       *str_mutex;
    GHashTable   *str_refs;
};

G_DEFINE_TYPE (YelpDocument, yelp_document, G_TYPE_OBJECT);

#define GET_PRIV(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_DOCUMENT, YelpDocumentPriv))

static void           yelp_document_class_init  (YelpDocumentClass    *klass);
static void           yelp_document_init        (YelpDocument         *document);
static void           yelp_document_dispose     (GObject              *object);
static void           yelp_document_finalize    (GObject              *object);

static gboolean       document_request_page     (YelpDocument         *document,
						 const gchar          *page_id,
						 GCancellable         *cancellable,
						 YelpDocumentCallback  callback,
						 gpointer              user_data);
static const gchar *  document_read_contents    (YelpDocument         *document,
						 const gchar          *page_id);
static void           document_finish_read      (YelpDocument         *document,
						 const gchar          *contents);
static gchar *        document_get_mime_type    (YelpDocument         *document,
						 const gchar          *mime_type);

static void           request_cancel            (GCancellable         *cancellable,
						 Request              *request);
static gboolean       request_idle_contents     (Request              *request);
static gboolean       request_idle_info         (Request              *request);
static void           request_try_free          (Request              *request);
static void           request_free              (Request              *request);

static const gchar *  str_ref                   (const gchar          *str);
static void           str_unref                 (const gchar          *str);
static void           hash_slist_insert         (GHashTable           *hash,
					         const gchar          *key,
						 gpointer              value);
static void           hash_slist_remove         (GHashTable           *hash,
						 const gchar          *key,
						 gpointer              value);

#if 0
static gboolean       request_idle_error        (Request             *request);
static gboolean       request_idle_final        (YelpDocument        *document);

#endif

GStaticMutex str_mutex = G_STATIC_MUTEX_INIT;
GHashTable  *str_refs  = NULL;

/******************************************************************************/

YelpDocument *
yelp_document_get_for_uri (YelpUri *uri)
{
    static GHashTable *documents = NULL;
    gchar *base_uri;
    YelpDocument *document = NULL;

    if (documents == NULL)
	documents = g_hash_table_new_full (g_str_hash, g_str_equal,
					   g_free, g_object_unref);

    g_return_val_if_fail (yelp_uri_is_resolved (uri), NULL);

    base_uri = yelp_uri_get_base_uri (uri);
    if (base_uri == NULL)
	return NULL;

    document = g_hash_table_lookup (documents, base_uri);

    if (document != NULL) {
	g_free (base_uri);
	return g_object_ref (document);
    }

    switch (yelp_uri_get_document_type (uri)) {
    case YELP_URI_DOCUMENT_TYPE_TEXT:
    case YELP_URI_DOCUMENT_TYPE_HTML:
    case YELP_URI_DOCUMENT_TYPE_XHTML:
	document = yelp_simple_document_new (uri);
	break;
    case YELP_URI_DOCUMENT_TYPE_DOCBOOK:
	/* FIXME */
	break;
    case YELP_URI_DOCUMENT_TYPE_MALLARD:
	/* FIXME */
	break;
    case YELP_URI_DOCUMENT_TYPE_MAN:
	/* FIXME */
	break;
    case YELP_URI_DOCUMENT_TYPE_INFO:
	/* FIXME */
	break;
    case YELP_URI_DOCUMENT_TYPE_TOC:
	/* FIXME */
	break;
    case YELP_URI_DOCUMENT_TYPE_SEARCH:
	/* FIXME */
	break;
    case YELP_URI_DOCUMENT_TYPE_NOT_FOUND:
    case YELP_URI_DOCUMENT_TYPE_EXTERNAL:
    case YELP_URI_DOCUMENT_TYPE_ERROR:
	break;
    }

    if (document != NULL) {
	g_hash_table_insert (documents, base_uri, document);
	return g_object_ref (document);
    }

    g_free (base_uri);
    return NULL;
}

/******************************************************************************/

static void
yelp_document_class_init (YelpDocumentClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose  = yelp_document_dispose;
    object_class->finalize = yelp_document_finalize;

    klass->request_page =   document_request_page;
    klass->read_contents =  document_read_contents;
    klass->finish_read =    document_finish_read;
    klass->get_mime_type =  document_get_mime_type;

    g_type_class_add_private (klass, sizeof (YelpDocumentPriv));
}

static void
yelp_document_init (YelpDocument *document)
{
    YelpDocumentPriv *priv;

    document->priv = priv = GET_PRIV (document);

    priv->mutex = g_mutex_new ();

    priv->reqs_by_page_id =
	g_hash_table_new_full (g_str_hash, g_str_equal,
			       g_free,
			       (GDestroyNotify) g_slist_free);
    priv->reqs_all = NULL;
    priv->reqs_pending = NULL;

    priv->page_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    priv->titles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    priv->mime_types = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    priv->contents = g_hash_table_new_full (g_str_hash, g_str_equal,
					    g_free,
					    (GDestroyNotify) str_unref);

    priv->root_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    priv->prev_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    priv->next_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    priv->up_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

static void
yelp_document_dispose (GObject *object)
{
    YelpDocument *document = YELP_DOCUMENT (object);

    if (document->priv->reqs_all) {
	g_slist_foreach (document->priv->reqs_all, request_try_free, NULL);
	g_slist_free (document->priv->reqs_all);
	document->priv->reqs_all = NULL;
    }

    G_OBJECT_CLASS (yelp_document_parent_class)->dispose (object);
}

static void
yelp_document_finalize (GObject *object)
{
    YelpDocument *document = YELP_DOCUMENT (object);

    g_slist_free (document->priv->reqs_pending);
    g_hash_table_destroy (document->priv->reqs_by_page_id);

    g_hash_table_destroy (document->priv->page_ids);
    g_hash_table_destroy (document->priv->titles);
    g_hash_table_destroy (document->priv->mime_types);

    g_hash_table_destroy (document->priv->contents);

    g_hash_table_destroy (document->priv->root_ids);
    g_hash_table_destroy (document->priv->prev_ids);
    g_hash_table_destroy (document->priv->next_ids);
    g_hash_table_destroy (document->priv->up_ids);

    g_mutex_free (document->priv->mutex);

    G_OBJECT_CLASS (yelp_document_parent_class)->finalize (object);
}

/******************************************************************************/

gchar *
yelp_document_get_page_id (YelpDocument *document,
			   const gchar  *id)
{
    gchar *ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);
    ret = g_hash_table_lookup (document->priv->page_ids, id);
    if (ret)
	ret = g_strdup (ret);

    g_mutex_unlock (document->priv->mutex);

    return ret;
}

void
yelp_document_set_page_id (YelpDocument *document,
			   const gchar  *id,
			   const gchar  *page_id)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);

    g_hash_table_replace (document->priv->root_ids, g_strdup (id), g_strdup (page_id));

    if (!g_str_equal (id, page_id)) {
	GSList *reqs, *cur;
	reqs = g_hash_table_lookup (document->priv->reqs_by_page_id, id);
	for (cur = reqs; cur != NULL; cur = cur->next) {
	    Request *request = (Request *) cur->data;
	    g_free (request->page_id);
	    request->page_id = g_strdup (page_id);
	    hash_slist_insert (document->priv->reqs_by_page_id, page_id, request);
	}
	if (reqs)
	    g_hash_table_remove (document->priv->reqs_by_page_id, id);
    }

    g_mutex_unlock (document->priv->mutex);
}

gchar *
yelp_document_get_root_id (YelpDocument *document,
			   const gchar  *page_id)
{
    gchar *real, *ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);
    real = g_hash_table_lookup (document->priv->page_ids, page_id);
    if (real) {
	ret = g_hash_table_lookup (document->priv->root_ids, real);
	if (ret)
	    ret = g_strdup (ret);
    }
    g_mutex_unlock (document->priv->mutex);

    return ret;
}

void
yelp_document_set_root_id (YelpDocument *document,
			   const gchar  *page_id,
			   const gchar  *root_id)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);
    g_hash_table_replace (document->priv->root_ids, g_strdup (page_id), g_strdup (root_id));
    g_mutex_unlock (document->priv->mutex);
}

gchar *
yelp_document_get_prev_id (YelpDocument *document,
			   const gchar  *page_id)
{
    gchar *real, *ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);
    real = g_hash_table_lookup (document->priv->page_ids, page_id);
    if (real) {
	ret = g_hash_table_lookup (document->priv->prev_ids, real);
	if (ret)
	    ret = g_strdup (ret);
    }
    g_mutex_unlock (document->priv->mutex);

    return ret;
}

void
yelp_document_set_prev_id (YelpDocument *document,
			   const gchar  *page_id,
			   const gchar  *prev_id)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);
    g_hash_table_replace (document->priv->prev_ids, g_strdup (page_id), g_strdup (prev_id));
    g_mutex_unlock (document->priv->mutex);
}

gchar *
yelp_document_get_next_id (YelpDocument *document,
			   const gchar  *page_id)
{
    gchar *real, *ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);
    real = g_hash_table_lookup (document->priv->page_ids, page_id);
    if (real) {
	ret = g_hash_table_lookup (document->priv->next_ids, real);
	if (ret)
	    ret = g_strdup (ret);
    }
    g_mutex_unlock (document->priv->mutex);

    return ret;
}

void
yelp_document_set_next_id (YelpDocument *document,
			   const gchar  *page_id,
			   const gchar  *next_id)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);
    g_hash_table_replace (document->priv->next_ids, g_strdup (page_id), g_strdup (next_id));
    g_mutex_unlock (document->priv->mutex);
}

gchar *
yelp_document_get_up_id (YelpDocument *document,
			 const gchar  *page_id)
{
    gchar *real, *ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);
    real = g_hash_table_lookup (document->priv->page_ids, page_id);
    if (real) {
	ret = g_hash_table_lookup (document->priv->up_ids, real);
	if (ret)
	    ret = g_strdup (ret);
    }
    g_mutex_unlock (document->priv->mutex);

    return ret;
}

void
yelp_document_set_up_id (YelpDocument *document,
			 const gchar  *page_id,
			 const gchar  *up_id)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);
    g_hash_table_replace (document->priv->up_ids, g_strdup (page_id), g_strdup (up_id));
    g_mutex_unlock (document->priv->mutex);
}

gchar *
yelp_document_get_title (YelpDocument *document,
			 const gchar  *page_id)
{
    gchar *real, *ret = NULL;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);
    real = g_hash_table_lookup (document->priv->page_ids, page_id);
    if (real) {
	ret = g_hash_table_lookup (document->priv->titles, real);
	if (ret)
	    ret = g_strdup (ret);
    }
    g_mutex_unlock (document->priv->mutex);

    return ret;
}

void
yelp_document_set_title (YelpDocument *document,
			 const gchar  *page_id,
			 const gchar  *title)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);
    g_hash_table_replace (document->priv->titles, g_strdup (page_id), g_strdup (title));
    g_mutex_unlock (document->priv->mutex);
}

/******************************************************************************/

gboolean
yelp_document_request_page (YelpDocument         *document,
			    const gchar          *page_id,
			    GCancellable         *cancellable,
			    YelpDocumentCallback  callback,
			    gpointer              user_data)
{
    g_return_if_fail (YELP_IS_DOCUMENT (document));
    g_return_if_fail (YELP_DOCUMENT_GET_CLASS (document)->request_page != NULL);

    return YELP_DOCUMENT_GET_CLASS (document)->request_page (document,
							     page_id,
							     cancellable,
							     callback,
							     user_data);
}

static gboolean
document_request_page (YelpDocument         *document,
		       const gchar          *page_id,
		       GCancellable         *cancellable,
		       YelpDocumentCallback  callback,
		       gpointer              user_data)
{
    Request *request;
    gchar *real_id;
    gboolean ret = FALSE;

    request = g_slice_new0 (Request);
    request->document = g_object_ref (document);

    real_id = g_hash_table_lookup (document->priv->page_ids, page_id);
    if (real_id)
	request->page_id = g_strdup (real_id);
    else
	request->page_id = g_strdup (page_id);

    request->cancellable = g_object_ref (cancellable);
    g_signal_connect (cancellable, "cancelled",
		      G_CALLBACK (request_cancel), request);

    request->callback = callback;
    request->user_data = user_data;
    request->idle_funcs = 0;

    g_mutex_lock (document->priv->mutex);

    hash_slist_insert (document->priv->reqs_by_page_id,
		       request->page_id,
		       request);

    document->priv->reqs_all = g_slist_prepend (document->priv->reqs_all, request);
    document->priv->reqs_pending = g_slist_prepend (document->priv->reqs_pending, request);

    if (g_hash_table_lookup (document->priv->titles, request->page_id)) {
	request->idle_funcs++;
	g_idle_add ((GSourceFunc) request_idle_info, request);
    }

    if (g_hash_table_lookup (document->priv->contents, request->page_id)) {
	request->idle_funcs++;
	g_idle_add ((GSourceFunc) request_idle_contents, request);
	ret = TRUE;
    }

    g_mutex_unlock (document->priv->mutex);

    return ret;
}

/******************************************************************************/

const gchar *
yelp_document_read_contents (YelpDocument *document,
			     const gchar  *page_id)
{
    g_return_val_if_fail (YELP_IS_DOCUMENT (document), NULL);
    g_return_val_if_fail (YELP_DOCUMENT_GET_CLASS (document)->read_contents != NULL, NULL);

    YELP_DOCUMENT_GET_CLASS (document)->read_contents (document, page_id);
}

static const gchar *
document_read_contents (YelpDocument *document,
			const gchar  *page_id)
{
    gchar *real, *str;

    g_mutex_lock (document->priv->mutex);

    real = g_hash_table_lookup (document->priv->page_ids, page_id);
    str = g_hash_table_lookup (document->priv->contents, real);
    if (str)
	str_ref (str);

    g_mutex_unlock (document->priv->mutex);

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
yelp_document_take_contents (YelpDocument *document,
			     const gchar  *page_id,
			     gchar        *contents,
			     const gchar  *mime)
{
    g_return_if_fail (YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);

    g_hash_table_replace (document->priv->contents,
			  g_strdup (page_id),
			  (gpointer) str_ref (contents));
   
    g_hash_table_replace (document->priv->mime_types,
			  g_strdup (page_id),
			  g_strdup (mime));

    g_mutex_unlock (document->priv->mutex);
}

gchar *
yelp_document_get_mime_type (YelpDocument *document,
			     const gchar  *page_id)
{
    g_return_if_fail (YELP_IS_DOCUMENT (document));
    g_return_if_fail (YELP_DOCUMENT_GET_CLASS (document)->get_mime_type != NULL);

    YELP_DOCUMENT_GET_CLASS (document)->get_mime_type (document, page_id);
}

static gchar *
document_get_mime_type (YelpDocument *document,
			const gchar  *page_id)
{
    gchar *real, *ret = NULL;

    g_mutex_lock (document->priv->mutex);
    real = g_hash_table_lookup (document->priv->page_ids, page_id);
    if (real) {
	ret = g_hash_table_lookup (document->priv->mime_types, real);
	if (ret)
	    ret = g_strdup (ret);
    }
    g_mutex_unlock (document->priv->mutex);

    return ret;
}

/******************************************************************************/

void
yelp_document_signal (YelpDocument       *document,
		      const gchar        *page_id,
		      YelpDocumentSignal  signal,
		      GError             *error)
{
    GSList *reqs, *cur;

    g_return_if_fail (YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);

    reqs = g_hash_table_lookup (document->priv->reqs_by_page_id, page_id);
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
	    /* FIXME */
	default:
	    break;
	}
    }

    g_mutex_unlock (document->priv->mutex);
}

static void
request_cancel (GCancellable *cancellable, Request *request)
{
    GSList *cur;
    YelpDocument *document = request->document;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);

    document->priv->reqs_pending = g_slist_remove (document->priv->reqs_pending,
						   (gconstpointer) request);
    hash_slist_remove (document->priv->reqs_by_page_id,
		       request->page_id,
		       request);
    for (cur = document->priv->reqs_all; cur != NULL; cur = cur->next) {
	if (cur->data == request) {
	    document->priv->reqs_all = g_slist_delete_link (document->priv->reqs_all, cur);
	    break;
	}
    }
    request_try_free (request);

    g_mutex_unlock (document->priv->mutex);
}

static gboolean
request_idle_contents (Request *request)
{
    YelpDocument *document;
    YelpDocumentCallback callback = NULL;
    gpointer user_data = user_data;

    g_assert (request != NULL && YELP_IS_DOCUMENT (request->document));

    if (g_cancellable_is_cancelled (request->cancellable)) {
	request->idle_funcs--;
	return FALSE;
    }

    document = g_object_ref (request->document);

    g_mutex_lock (document->priv->mutex);

    callback = request->callback;
    user_data = request->user_data;
    request->idle_funcs--;

    g_mutex_unlock (document->priv->mutex);

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
    gpointer user_data = user_data;

    g_assert (request != NULL && YELP_IS_DOCUMENT (request->document));

    if (g_cancellable_is_cancelled (request->cancellable)) {
	request->idle_funcs--;
	return FALSE;
    }

    document = g_object_ref (request->document);

    g_mutex_lock (document->priv->mutex);

    callback = request->callback;
    user_data = request->user_data;
    request->idle_funcs--;

    g_mutex_unlock (document->priv->mutex);

    if (callback)
	callback (document, YELP_DOCUMENT_SIGNAL_INFO, user_data, NULL);

    g_object_unref (document);
    return FALSE;
}

static void
request_try_free (Request *request)
{
    if (!g_cancellable_is_cancelled (request->cancellable))
	g_cancellable_cancel (request->cancellable);

    if (request->idle_funcs == 0)
	request_free (request);
    else
	g_idle_add ((GSourceFunc) request_try_free, request);
}

static void
request_free (Request *request)
{
    g_object_unref (request->document);
    g_free (request->page_id);
    g_object_unref (request->cancellable);

    g_slice_free (Request, request);
}

/******************************************************************************/

static const gchar *
str_ref (const gchar *str)
{
    gpointer p;
    guint i;

    g_static_mutex_lock (&str_mutex);
    if (str_refs == NULL)
	str_refs = g_hash_table_new (g_direct_hash, g_direct_equal);
    p = g_hash_table_lookup (str_refs, str);

    i = GPOINTER_TO_UINT (p);
    i++;
    p = GUINT_TO_POINTER (i);

    g_hash_table_insert (str_refs, (gpointer) str, p);
    g_static_mutex_unlock (&str_mutex);

    return str;
}

static void
str_unref (const gchar *str)
{
    gpointer p;
    guint i;

    g_static_mutex_lock (&str_mutex);
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

    g_static_mutex_unlock (&str_mutex);
}

static void
hash_slist_insert (GHashTable  *hash,
		   const gchar *key,
		   gpointer     value)
{
    GSList *list;
    list = g_hash_table_lookup (hash, key);
    if (list) {
	list->next = g_slist_prepend (list->next, value);
    } else {
	list = g_slist_prepend (NULL, value);
	list = g_slist_prepend (list, NULL);
	g_hash_table_insert (hash, g_strdup (key), list);
    }
}

static void
hash_slist_remove (GHashTable  *hash,
		   const gchar *key,
		   gpointer     value)
{
    GSList *list;
    list = g_hash_table_lookup (hash, key);
    if (list) {
	list = g_slist_remove (list, value);
	if (list->next == NULL)
	    g_hash_table_remove (hash, key);
    }
}

#if 0

void
yelp_document_add_page (YelpDocument *document, gchar *page_id, const gchar *contents)
{
    GSList *reqs, *cur;
    Request *request;
    YelpDocumentPriv *priv;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  page_id = \"%s\"\n", page_id);
    priv = document->priv;

    g_mutex_lock (priv->mutex);

    g_hash_table_replace (priv->contents,
			  g_strdup (page_id),
			  str_ref ((gchar *) contents));

    reqs = g_hash_table_lookup (priv->reqs_by_page_id, page_id);
    for (cur = reqs; cur != NULL; cur = cur->next) {
	if (cur->data) {
	    request = (Request *) cur->data;
	    request->idle_funcs++;
	    g_idle_add ((GSourceFunc) request_idle_page, request);
	}
    }

    g_mutex_unlock (priv->mutex);
}

gboolean
yelp_document_has_page (YelpDocument *document, gchar *page_id)
{
    gchar *content;
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));
    content = g_hash_table_lookup (document->priv->contents, page_id);
    return !(content == NULL);
}

void
yelp_document_error_request (YelpDocument *document, gint req_id, YelpError *error)
{
    Request *request;
    YelpDocumentPriv *priv;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    debug_print (DB_FUNCTION, "entering\n");
    priv = document->priv;

    g_mutex_lock (priv->mutex);

    request = g_hash_table_lookup (priv->reqs_by_req_id,
				   GINT_TO_POINTER (req_id));
    if (request) {
	request->error = error;
	request->idle_funcs++;
	g_idle_add ((GSourceFunc) request_idle_error, request);
    } else {
	yelp_error_free (error);
    }

    g_mutex_unlock (priv->mutex);
}

void
yelp_document_error_page (YelpDocument *document, gchar *page_id, YelpError *error)
{
    GSList *requests;
    Request *request = NULL;
    YelpDocumentPriv *priv;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    debug_print (DB_FUNCTION, "entering\n");
    priv = document->priv;
    g_mutex_lock (priv->mutex);

    requests = g_hash_table_lookup (priv->reqs_by_page_id, page_id);
    while (requests) {
	request = (Request *) requests->data;
	if (request && request->error == NULL) {
	    request->error = yelp_error_copy (error);
	    request->idle_funcs++;
	    g_idle_add ((GSourceFunc) request_idle_error, request);
	}
	requests = requests->next;
    }

    yelp_error_free (error);

    g_mutex_unlock (priv->mutex);
}

void
yelp_document_error_pending (YelpDocument *document, YelpError *error)
{
    GSList *cur;
    Request *request;
    YelpDocumentPriv *priv;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    debug_print (DB_FUNCTION, "entering\n");
    priv = document->priv;

    g_mutex_lock (priv->mutex);

    if (priv->reqs_pending) {
	for (cur = priv->reqs_pending; cur; cur = cur->next) {
	    request = cur->data;
	    if (cur->next)
		request->error = yelp_error_copy (error);
	    else
		request->error = error;
	    request->idle_funcs++;
	    g_idle_add ((GSourceFunc) request_idle_error, request);
	}

	g_slist_free (priv->reqs_pending);
	priv->reqs_pending = NULL;
    } else {
	yelp_error_free (error);
    }

    g_mutex_unlock (priv->mutex);
}

void
yelp_document_final_pending (YelpDocument *document, YelpError *error)
{
    YelpDocumentPriv *priv;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    debug_print (DB_FUNCTION, "entering\n");
    priv = document->priv;

    g_mutex_lock (priv->mutex);
    if (priv->reqs_pending) {
	priv->final_error = error;
	g_idle_add ((GSourceFunc) request_idle_final, document);
    } else {
	yelp_error_free (error);
    }

    g_mutex_unlock (priv->mutex);
}


/******************************************************************************/

static gboolean
request_idle_error (Request *request)
{
    YelpDocument *document;
    YelpDocumentPriv *priv;
    YelpDocumentFunc  func = NULL;
    YelpError *error = NULL;
    gint req_id = 0;
    gpointer user_data = user_data;

    g_assert (request != NULL && YELP_IS_DOCUMENT (request->document));

    if (request->cancel) {
	request->idle_funcs--;
	return FALSE;
    }

    debug_print (DB_FUNCTION, "entering\n");

    document = g_object_ref (request->document);
    priv = document->priv;

    g_mutex_lock (priv->mutex);

    if (request->error) {
	func = request->func;
	req_id = request->req_id;
	user_data = request->user_data;
	error = request->error;
	request->error = NULL;

	priv->reqs_pending = g_slist_remove (priv->reqs_pending, request);
    }

    request->idle_funcs--;
    g_mutex_unlock (priv->mutex);

    if (func)
	func (document,
	      YELP_DOCUMENT_SIGNAL_ERROR,
	      req_id,
	      error,
	      user_data);

    g_object_unref (document);
    return FALSE;
}


static gboolean
request_idle_final (YelpDocument *document)
{
    YelpDocumentPriv *priv;
    YelpDocumentFunc  func = NULL;
    YelpError *error = NULL;
    gint req_id = 0;
    gpointer user_data = user_data;
    Request *request = NULL;
    GSList *cur = NULL;

    debug_print (DB_FUNCTION, "entering\n");

    priv = document->priv;

    g_mutex_lock (priv->mutex);

    if (priv->reqs_pending == NULL) {
	/*
	  Time to bail as we shouldn't be here anyway.
	*/
	g_mutex_unlock (priv->mutex);
	return FALSE;
    }
    
    for (cur = priv->reqs_pending; cur; cur = cur->next) {
	request = cur->data;
	if (request->idle_funcs != 0) {
	    /* 
	       While there are outstanding requests, we should wait for them
	       to complete before signalling the error
	    */
	    request->idle_funcs++;
	    g_mutex_unlock (priv->mutex);
	    return TRUE;
	}
    }

    for (cur = priv->reqs_pending; cur; cur = cur->next) {
	request = cur->data;
	
	if (cur->next)
	    request->error = yelp_error_copy (priv->final_error);
	else
	    request->error = error;
	
	if (request->error) {
	    func = request->func;
	    req_id = request->req_id;
	    user_data = request->user_data;
	    error = request->error;
	    request->error = NULL;
	    
	    priv->reqs_pending = g_slist_remove (priv->reqs_pending, request);
	}
	
	
	if (func)
	    func (document,
		  YELP_DOCUMENT_SIGNAL_ERROR,
		  req_id,
		  error,
		  user_data);
    }
    g_mutex_unlock (priv->mutex);
    
    g_object_unref (document);
    return FALSE;
}

/******************************************************************************/


#endif
