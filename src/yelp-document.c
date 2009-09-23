/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2007 Shaun McCance  <shaunm@gnome.org>
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
#include "yelp-debug.h"

#define YELP_DOCUMENT_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_DOCUMENT, YelpDocumentPriv))

typedef struct _Request Request;
struct _Request {
    YelpDocument     *document;
    YelpDocumentFunc  func;
    gpointer          user_data;

    YelpError        *error;

    gint    req_id;
    gchar  *page_id;

    gint     idle_funcs;
    gboolean cancel;
};

struct _YelpDocumentPriv {
    GMutex       *mutex;

    gchar        *root_id;
    YelpError    *final_error;


    GHashTable   *reqs_by_req_id;   /* Indexed by the request ID */
    GHashTable   *reqs_by_page_id;  /* Indexed by page ID, contains GSList */
    GSList       *reqs_pending;     /* List of requests that need a page */

    GtkTreeModel *sections;         /* Sections of the document, for display */
    /* Real page IDs map to themselves, so this list doubles
     * as a list of all valid page IDs.
     */
    GHashTable   *page_ids;      /* Mapping of fragment IDs to real page IDs */
    GHashTable   *titles;        /* Mapping of page IDs to titles */
    GHashTable   *contents;      /* Mapping of page IDs to string content */
    GHashTable   *pages;         /* Mapping of page IDs to open YelpPages */

    GHashTable   *prev_ids;      /* Mapping of page IDs to "previous page" IDs */
    GHashTable   *next_ids;      /* Mapping of page IDs to "next page" IDs */
    GHashTable   *up_ids;        /* Mapping of page IDs to "up page" IDs */
};

static void           document_class_init     (YelpDocumentClass   *klass);
static void           document_init           (YelpDocument        *document);
static void           document_dispose        (GObject             *object);
static gpointer       document_get_sections   (YelpDocument        *document);

static gboolean       request_idle_title      (Request             *request);
static gboolean       request_idle_page       (Request             *request);
static gboolean       request_idle_error      (Request             *request);
static gboolean       request_idle_final      (YelpDocument        *document);
static void           request_try_free        (Request             *request);
static void           request_free            (Request             *request);

static void           hash_slist_insert       (GHashTable          *hash,
					       const gchar         *key,
					       gpointer             value);
static void           hash_slist_remove       (GHashTable          *hash,
					       const gchar         *key,
					       gpointer             value);

GStaticMutex str_mutex = G_STATIC_MUTEX_INIT;
GHashTable *str_refs = NULL;
static gchar * str_ref    (gchar  *str);
static void    str_unref  (gchar  *str);

static GObjectClass *parent_class;

GType
yelp_document_get_type (void)
{
    static GType type = 0;
    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpDocumentClass),
	    NULL, NULL,
	    (GClassInitFunc) document_class_init,
	    NULL, NULL,
	    sizeof (YelpDocument),
	    0,
	    (GInstanceInitFunc) document_init,
	};
	type = g_type_register_static (G_TYPE_OBJECT,
				       "YelpDocument", 
				       &info, 0);
    }
    return type;
}

static void
document_class_init (YelpDocumentClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose      = document_dispose;

    klass->get_sections        = document_get_sections;

    g_type_class_add_private (klass, sizeof (YelpDocumentPriv));
}

static void
document_init (YelpDocument *document)
{
    YelpDocumentPriv *priv;

    document->priv = priv = YELP_DOCUMENT_GET_PRIVATE (document);

    priv->mutex = g_mutex_new ();

    priv->reqs_by_req_id =
	g_hash_table_new_full (g_direct_hash, g_direct_equal,
			       NULL,
			       (GDestroyNotify) request_try_free);
    priv->reqs_by_page_id =
	g_hash_table_new_full (g_str_hash, g_str_equal,
			       g_free,
			       (GDestroyNotify) g_slist_free);
    priv->reqs_pending = NULL;

    priv->sections = NULL;
    priv->page_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    priv->titles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    priv->contents = g_hash_table_new_full (g_str_hash, g_str_equal,
					    g_free,
					    (GDestroyNotify) str_unref);
    priv->pages = g_hash_table_new_full (g_str_hash, g_str_equal,
					 g_free,
					 (GDestroyNotify) g_slist_free);
    priv->prev_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    priv->next_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    priv->up_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

static gpointer
document_get_sections (YelpDocument *document)
{
    return NULL;
}


static void
document_dispose (GObject *object)
{
    YelpDocument *document = YELP_DOCUMENT (object);

    g_free (document->priv->root_id);

    g_slist_free (document->priv->reqs_pending);
    g_hash_table_destroy (document->priv->reqs_by_page_id);
    g_hash_table_destroy (document->priv->reqs_by_req_id);

    g_hash_table_destroy (document->priv->page_ids);
    g_hash_table_destroy (document->priv->titles);

    g_hash_table_destroy (document->priv->contents);
    g_hash_table_destroy (document->priv->pages);

    g_hash_table_destroy (document->priv->prev_ids);
    g_hash_table_destroy (document->priv->next_ids);
    g_hash_table_destroy (document->priv->up_ids);

    g_mutex_free (document->priv->mutex);

    parent_class->dispose (object);
}

/******************************************************************************/

gint
yelp_document_get_page (YelpDocument     *document,
			gchar            *page_id,
			YelpDocumentFunc  func,
			gpointer          user_data)
{
    YelpDocumentPriv *priv;
    gchar *real_id;
    gboolean handled = FALSE;
    Request *request;
    gint req_id;
    static gint request_id = 0;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  page_id = \"%s\"\n", page_id);

    priv = document->priv;

    request = g_slice_new0 (Request);
    request->document = document;
    request->func = func;
    request->user_data = user_data;
    request->req_id = req_id = ++request_id;

    real_id = g_hash_table_lookup (priv->page_ids, page_id);
    if (real_id)
	request->page_id = g_strdup (real_id);
    else
	request->page_id = g_strdup (page_id);

    g_mutex_lock (priv->mutex);

    g_hash_table_insert (priv->reqs_by_req_id,
			 GINT_TO_POINTER (req_id),
			 request);
    hash_slist_insert (priv->reqs_by_page_id,
		       request->page_id,
		       request);
    priv->reqs_pending = g_slist_prepend (priv->reqs_pending, request);

    if (g_hash_table_lookup (priv->titles, request->page_id)) {
	request->idle_funcs++;
	g_idle_add ((GSourceFunc) request_idle_title, request);
    }

    if (g_hash_table_lookup (priv->contents, request->page_id)) {
	request->idle_funcs++;
	g_idle_add ((GSourceFunc) request_idle_page, request);
	handled = TRUE;
    }

    g_mutex_unlock (priv->mutex);

    YELP_DOCUMENT_GET_CLASS (document)->request (document,
						 req_id,
						 handled,
						 request->page_id,
						 func,
						 user_data);

    return req_id;
}

void
yelp_document_cancel_page (YelpDocument *document, gint req_id)
{
    YelpDocumentPriv *priv;
    Request *request;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  req_id = %i\n", req_id);

    priv = document->priv;

    g_mutex_lock (priv->mutex);
    request = g_hash_table_lookup (priv->reqs_by_req_id,
				   GINT_TO_POINTER (req_id));
    if (request) {
	priv->reqs_pending = g_slist_remove (priv->reqs_pending,
					     (gconstpointer) request);
	hash_slist_remove (priv->reqs_by_page_id,
			   request->page_id,
			   request);
    } else {
	g_warning ("YelpDocument: Attempted to remove request %i,"
		   " but no such request exists.",
		   req_id);
    }
    g_mutex_unlock (priv->mutex);

    if (YELP_DOCUMENT_GET_CLASS (document)->cancel)
	YELP_DOCUMENT_GET_CLASS (document)->cancel (document, req_id);
}

/******************************************************************************/

void
yelp_document_release_page (YelpDocument *document, YelpPage *page)
{
    YelpDocumentPriv *priv;

    g_return_if_fail (YELP_IS_DOCUMENT (document));

    debug_print (DB_FUNCTION, "entering\n");

    priv = document->priv;

    g_mutex_lock (priv->mutex);

    hash_slist_remove (priv->pages, page->id, page);
    if (page->content)
	str_unref (page->content);

    g_mutex_unlock (priv->mutex);
}

/******************************************************************************/

void
yelp_document_set_root_id (YelpDocument *document, gchar *root_id)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    if (document->priv->root_id)
	g_free (document->priv->root_id);

    document->priv->root_id = g_strdup (root_id);
}

void
yelp_document_add_page_id (YelpDocument *document, gchar *id, gchar *page_id)
{
    GSList *reqs, *cur;
    Request *request;
    gchar *title, *contents;
    YelpDocumentPriv *priv;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    priv = document->priv;

    g_mutex_lock (priv->mutex);
    if (g_hash_table_lookup (priv->page_ids, id)) {
	g_warning ("YelpDocument: Attempted to add an ID mapping from"
		   " %s to %s, but %s is already mapped.",
		   id, page_id, id);
	g_mutex_unlock (priv->mutex);
	return;
    }

    g_hash_table_insert (priv->page_ids, g_strdup (id), g_strdup (page_id));

    if (!g_str_equal (id, page_id)) {
	title = g_hash_table_lookup (priv->titles, page_id);
	contents = g_hash_table_lookup (priv->contents, page_id);
	reqs = g_hash_table_lookup (priv->reqs_by_page_id, id);
	for (cur = reqs; cur != NULL; cur = cur->next) {
	    if (cur->data) {
		request = (Request *) cur->data;
		g_free (request->page_id);
		request->page_id = g_strdup (page_id);
		hash_slist_insert (priv->reqs_by_page_id, page_id, request);
		if (title) {
		    request->idle_funcs++;
		    g_idle_add ((GSourceFunc) request_idle_title, request);
		}
		if (contents) {
		    request->idle_funcs++;
		    g_idle_add ((GSourceFunc) request_idle_page, request);
		}
	    }
	}
	if (reqs)
	    g_hash_table_remove (priv->reqs_by_page_id, id);
    }

    g_mutex_unlock (priv->mutex);
}

void
yelp_document_add_prev_id (YelpDocument *document, gchar *page_id, gchar *prev_id)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);
    g_hash_table_replace (document->priv->prev_ids, g_strdup (page_id), g_strdup (prev_id));
    g_mutex_unlock (document->priv->mutex);
}

void
yelp_document_add_next_id (YelpDocument *document, gchar *page_id, gchar *next_id)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);
    g_hash_table_replace (document->priv->next_ids, g_strdup (page_id), g_strdup (next_id));
    g_mutex_unlock (document->priv->mutex);
}

void
yelp_document_add_up_id (YelpDocument *document, gchar *page_id, gchar *up_id)
{
    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    g_mutex_lock (document->priv->mutex);
    g_hash_table_replace (document->priv->up_ids, g_strdup (page_id), g_strdup (up_id));
    g_mutex_unlock (document->priv->mutex);
}

void
yelp_document_add_title (YelpDocument *document, gchar *page_id, gchar *title)
{
    GSList *reqs, *cur;
    Request *request;
    YelpDocumentPriv *priv;

    g_assert (document != NULL && YELP_IS_DOCUMENT (document));

    debug_print (DB_FUNCTION, "entering\n");
    priv = document->priv;

    g_mutex_lock (priv->mutex);

    g_hash_table_replace (priv->titles, g_strdup (page_id), g_strdup (title));

    reqs = g_hash_table_lookup (priv->reqs_by_page_id, page_id);
    for (cur = reqs; cur != NULL; cur = cur->next) {
	if (cur->data) {
	    request = (Request *) cur->data;
	    request->idle_funcs++;
	    g_idle_add ((GSourceFunc) request_idle_title, request);
	}
    }

    g_mutex_unlock (priv->mutex);
}

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

GtkTreeModel *
yelp_document_get_sections (YelpDocument *document)
{
    return (YELP_DOCUMENT_GET_CLASS (document)->get_sections(document));
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
request_idle_title (Request *request)
{
    YelpDocument *document;
    YelpDocumentPriv *priv;
    YelpDocumentFunc  func = NULL;
    gchar *title;
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

    title = g_hash_table_lookup (priv->titles, request->page_id);
    if (title) {
	func = request->func;
	req_id = request->req_id;
	title = g_strdup (title);
	user_data = request->user_data;
    }

    request->idle_funcs--;
    g_mutex_unlock (priv->mutex);

    if (func)
	func (document,
	      YELP_DOCUMENT_SIGNAL_TITLE,
	      req_id,
	      title,
	      user_data);

    g_object_unref (document);
    return FALSE;
}

static gboolean
request_idle_page (Request *request)
{
    YelpDocument *document;
    YelpDocumentPriv *priv;
    YelpDocumentFunc  func = NULL;
    YelpPage *page = NULL;
    gchar *contents, *tmp;
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

    contents = g_hash_table_lookup (priv->contents, request->page_id);
    if (contents) {
	func = request->func;
	req_id = request->req_id;
	user_data = request->user_data;

	/* FIXME: there will come a day when we can't just assume XHTML */
	page = yelp_page_new_string (YELP_DOCUMENT (request->document),
				     request->page_id,
				     str_ref (contents),
				     YELP_PAGE_MIME_XHTML);
	tmp = g_hash_table_lookup (priv->prev_ids, request->page_id);
	if (tmp)
	    page->prev_id = g_strdup (tmp);
	tmp = g_hash_table_lookup (priv->next_ids, request->page_id);
	if (tmp)
	    page->next_id = g_strdup (tmp);
	tmp = g_hash_table_lookup (priv->up_ids, request->page_id);
	if (tmp)
	    page->up_id = g_strdup (tmp);
	if (priv->root_id)
	    page->root_id = g_strdup (priv->root_id);
	priv->reqs_pending = g_slist_remove (priv->reqs_pending, request);
    }

    request->idle_funcs--;
    g_mutex_unlock (priv->mutex);

    if (func) {
	func (document,
	      YELP_DOCUMENT_SIGNAL_PAGE,
	      req_id,
	      page,
	      user_data);
    }

    g_object_unref (document);

    return FALSE;
}

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

static void
request_try_free (Request *request) {
    debug_print (DB_FUNCTION, "entering\n");
    request->cancel = TRUE;

    if (request->idle_funcs == 0)
	request_free (request);
    else
	g_idle_add ((GSourceFunc) request_try_free, request);
}

static void
request_free (Request *request)
{
    debug_print (DB_FUNCTION, "entering\n");
    g_free (request->page_id);
    g_slice_free (Request, request);
}

/******************************************************************************/

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

static gchar *
str_ref (gchar *str)
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

    g_hash_table_insert (str_refs, str, p);

    g_static_mutex_unlock (&str_mutex);

    return str;
}

static void
str_unref (gchar *str)
{
    gpointer p;
    guint i;

    g_static_mutex_lock (&str_mutex);

    p = g_hash_table_lookup (str_refs, str);

    i = GPOINTER_TO_UINT (p);
    i--;
    p = GUINT_TO_POINTER (i);

    if (i > 0)
	g_hash_table_insert (str_refs, str, p);
    else {
	g_hash_table_remove (str_refs, str);
	g_free (str);
    }

    g_static_mutex_unlock (&str_mutex);
}
