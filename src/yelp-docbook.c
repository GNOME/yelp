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
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xinclude.h>

#include "yelp-error.h"
#include "yelp-docbook.h"
#include "yelp-settings.h"
#include "yelp-transform.h"
#include "yelp-debug.h"

#define STYLESHEET DATADIR"/yelp/xslt/db2html.xsl"

#define YELP_DOCBOOK_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_DOCBOOK, YelpDocbookPriv))

typedef struct _Request Request;
struct _Request {
    YelpDocbook      *document;
    YelpDocumentFunc  func;
    gpointer          user_data;

    gint    req_id;
    gchar  *page_id;

    gint     idle_funcs;
    gboolean cancel;
};

typedef enum {
    DOCBOOK_STATE_BLANK,   /* Brand new, run transform as needed */
    DOCBOOK_STATE_PARSING, /* Parsing/transforming document, please wait */
    DOCBOOK_STATE_PARSED,  /* All done, if we ain't got it, it ain't here */
    DOCBOOK_STATE_STOP     /* Stop everything now, object to be disposed */
} DocbookState;

struct _YelpDocbookPriv {
    gchar        *filename;
    DocbookState  state;

    GMutex     *mutex;
    GThread    *thread;

    gboolean    process_running;
    gboolean    transform_running;

    YelpTransform *transform;

    GtkTreeModel  *sections;

    GSList     *reqs_pending;     /* List of requests that need a page */
    GHashTable *reqs_by_req_id;   /* Indexed by the request ID */
    GHashTable *reqs_by_page_id;  /* Indexed by page ID, contains GSList */

    /* Real page IDs map to themselves, so this list doubles
     * as a list of all valid page IDs.
     */
    GHashTable   *page_ids;      /* Mapping of fragment IDs to real page IDs */
    GHashTable   *titles;        /* Mapping of page IDs to titles */
    GHashTable   *contents;      /* Mapping of page IDs to string content */
    GHashTable   *pages;         /* Mapping of page IDs to open YelpPages */

    GHashTable   *prev_ids;
    GHashTable   *next_ids;
    GHashTable   *up_ids;

    gchar        *root_id;
    gint          max_depth;
    xmlDocPtr     xmldoc;
    xmlNodePtr    xmlcur;
    gint          cur_depth;
    GtkTreeIter  *sections_iter; /* On the stack, do not free */
    gchar        *cur_page_id;
    gchar        *cur_prev_id;
};


static void           docbook_class_init      (YelpDocbookClass    *klass);
static void           docbook_init            (YelpDocbook         *docbook);
static void           docbook_try_dispose     (GObject             *object);
static void           docbook_dispose         (GObject             *object);

/* YelpDocument */
static gint           docbook_get_page        (YelpDocument        *document,
					       gchar               *page_id,
					       YelpDocumentFunc     func,
					       gpointer             user_data);
static void           docbook_cancel_page     (YelpDocument        *document,
					       gint                 req_id);
static void           docbook_release_page    (YelpDocument        *document,
					       YelpPage            *page);

/* YelpTransform */
static void           transform_func          (YelpTransform       *transform,
					       YelpTransformSignal  signal,
					       gpointer             func_data,
					       YelpDocbook         *docbook);
static void           transform_page_func     (YelpTransform       *transform,
					       gchar               *page_id,
					       YelpDocbook         *docbook);
static void           transform_final_func    (YelpTransform       *transform,
					       YelpDocbook         *docbook);

/* Threaded */
static void           docbook_process         (YelpDocbook         *docbook);
static void           docbook_map_id          (YelpDocbook         *docbook,
					       gchar               *source,
					       gchar               *target);
static void           docbook_add_title       (YelpDocbook         *docbook,
					       gchar               *page_id,
					       gchar               *title);

/* Request */
static gboolean       request_idle_title      (Request             *request);
static gboolean       request_idle_page       (Request             *request);
static gboolean       request_try_title       (Request             *request);
static gboolean       request_try_page        (Request             *request);
static void           request_error           (Request             *request,
					       YelpError           *error);
static void           docbook_error_all       (YelpDocbook         *docbook,
					       YelpError           *error);
static void           request_try_free        (Request             *request);
static void           request_free            (Request             *request);

/* Walker */
static void           docbook_walk            (YelpDocbook         *docbook);
static gboolean       docbook_walk_chunkQ     (YelpDocbook         *docbook);
static gboolean       docbook_walk_divisionQ  (YelpDocbook         *docbook);
static gchar *        docbook_walk_get_title  (YelpDocbook         *docbook);

static void           hash_slist_insert       (GHashTable          *hash,
					       const gchar         *key,
					       gpointer             value);
static void           hash_slist_remove       (GHashTable          *hash,
					       const gchar         *key,
					       gpointer             value);

static YelpDocumentClass *parent_class;

GType
yelp_docbook_get_type (void)
{
    static GType type = 0;
    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpDocbookClass),
	    NULL, NULL,
	    (GClassInitFunc) docbook_class_init,
	    NULL, NULL,
	    sizeof (YelpDocbook),
	    0,
	    (GInstanceInitFunc) docbook_init,
	};
	type = g_type_register_static (YELP_TYPE_DOCUMENT,
				       "YelpDocbook", 
				       &info, 0);
    }
    return type;
}

static void
docbook_class_init (YelpDocbookClass *klass)
{
    GObjectClass      *object_class   = G_OBJECT_CLASS (klass);
    YelpDocumentClass *document_class = YELP_DOCUMENT_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = docbook_try_dispose;

    document_class->get_page = docbook_get_page;
    document_class->cancel_page = docbook_cancel_page;
    document_class->release_page = docbook_release_page;

    g_type_class_add_private (klass, sizeof (YelpDocbookPriv));
}

static void
docbook_init (YelpDocbook *docbook)
{
    YelpDocbookPriv *priv;
    priv = docbook->priv = YELP_DOCBOOK_GET_PRIVATE (docbook);

    priv->sections = NULL;

    priv->reqs_pending = NULL;

    priv->reqs_by_req_id =
	g_hash_table_new_full (g_direct_hash, g_direct_equal,
			       NULL,
			       (GDestroyNotify) request_try_free);
    priv->reqs_by_page_id =
	g_hash_table_new_full (g_str_hash, g_str_equal,
			       g_free,
			       (GDestroyNotify) g_slist_free);

    priv->page_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    priv->titles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    priv->contents = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    priv->pages = g_hash_table_new_full (g_str_hash, g_str_equal,
					 g_free,
					 (GDestroyNotify) g_slist_free);

    priv->prev_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    priv->next_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    priv->up_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    priv->state = DOCBOOK_STATE_BLANK;

    priv->mutex = g_mutex_new ();
}

static void
docbook_try_dispose (GObject *object)
{
    YelpDocbookPriv *priv;

    g_assert (object != NULL && YELP_IS_DOCBOOK (object));
    priv = YELP_DOCBOOK (object)->priv;

    g_mutex_lock (priv->mutex);
    if (priv->process_running || priv->transform_running) {
	priv->state = DOCBOOK_STATE_STOP;
	g_idle_add ((GSourceFunc) docbook_try_dispose, object);
	g_mutex_unlock (priv->mutex);
    } else {
	g_mutex_unlock (priv->mutex);
	docbook_dispose (object);
    }
}

static void
docbook_dispose (GObject *object)
{
    YelpDocbook *docbook = YELP_DOCBOOK (object);

    g_free (docbook->priv->filename);

    g_slist_free (docbook->priv->reqs_pending);

    g_hash_table_destroy (docbook->priv->reqs_by_page_id);
    /* This one destroys the actual requests */
    g_hash_table_destroy (docbook->priv->reqs_by_req_id);

    g_object_unref (docbook->priv->sections);
    g_hash_table_destroy (docbook->priv->page_ids);
    g_hash_table_destroy (docbook->priv->titles);

    g_hash_table_destroy (docbook->priv->contents);
    g_hash_table_destroy (docbook->priv->pages);

    g_hash_table_destroy (docbook->priv->prev_ids);
    g_hash_table_destroy (docbook->priv->next_ids);
    g_hash_table_destroy (docbook->priv->up_ids);

    g_free (docbook->priv->root_id);

    if (docbook->priv->xmldoc)
	xmlFreeDoc (docbook->priv->xmldoc);

    g_free (docbook->priv->cur_page_id);
    g_free (docbook->priv->cur_prev_id);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

YelpDocument *
yelp_docbook_new (gchar *filename)
{
    YelpDocbook *docbook;

    g_return_val_if_fail (filename != NULL, NULL);

    docbook = (YelpDocbook *) g_object_new (YELP_TYPE_DOCBOOK, NULL);
    docbook->priv->filename = g_strdup (filename);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  filename = \"%s\"\n", filename);

    g_hash_table_insert (docbook->priv->page_ids,
			 g_strdup ("x-yelp-titlepage"),
			 g_strdup ("x-yelp-titlepage"));

    docbook->priv->sections =
	GTK_TREE_MODEL (gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING));

    return (YelpDocument *) docbook;
}


/******************************************************************************/
/** YelpDocument **************************************************************/

static gint
docbook_get_page (YelpDocument     *document,
		  gchar            *page_id,
		  YelpDocumentFunc  func,
		  gpointer          user_data)
{
    Request *request;
    gchar *real_id;
    gboolean needs_parse = FALSE;
    static gint request_id = 0;
    YelpDocbook *docbook = YELP_DOCBOOK (document);
    YelpDocbookPriv *priv = docbook->priv;

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  page_id = \"%s\"\n", page_id);

    request = g_new0 (Request, 1);
    request->document = docbook;
    request->func = func;
    request->user_data = user_data;
    request->req_id = ++request_id;

    real_id = g_hash_table_lookup (priv->page_ids, page_id);
    if (real_id)
	request->page_id = g_strdup (real_id);
    else
	request->page_id = g_strdup (page_id);

    g_mutex_lock (priv->mutex);
    g_hash_table_insert (priv->reqs_by_req_id,
			 GINT_TO_POINTER (request->req_id),
			 request);
    hash_slist_insert (priv->reqs_by_page_id,
		       request->page_id,
		       request);
    g_mutex_unlock (priv->mutex);

    if (!request_try_title (request))
	switch (priv->state) {
	case DOCBOOK_STATE_BLANK:
	    needs_parse = TRUE;
	case DOCBOOK_STATE_PARSING:
	    g_mutex_lock (priv->mutex);
	    priv->reqs_pending = g_slist_prepend (priv->reqs_pending, request);
	    g_mutex_unlock (priv->mutex);
	    break;
	case DOCBOOK_STATE_PARSED:
	    /* FIXME: error out */
	    goto done;
	case DOCBOOK_STATE_STOP:
	    goto done;
	}
    else if (!request_try_page (request))
	switch (priv->state) {
	case DOCBOOK_STATE_BLANK:
	    needs_parse = TRUE;
	case DOCBOOK_STATE_PARSING:
	    g_mutex_lock (priv->mutex);
	    priv->reqs_pending = g_slist_prepend (priv->reqs_pending, request);
	    g_mutex_unlock (priv->mutex);
	    break;
	case DOCBOOK_STATE_PARSED:
	    /* FIXME: error out */
	    goto done;
	case DOCBOOK_STATE_STOP:
	    goto done;
	}

    debug_print (DB_DEBUG, "  needs_parse = %i\n", needs_parse);
    if (needs_parse) {
	g_mutex_lock (priv->mutex);

	priv->state = DOCBOOK_STATE_PARSING;

	priv->process_running = TRUE;
	priv->thread = g_thread_create ((GThreadFunc) docbook_process,
					docbook, FALSE, NULL);

	g_mutex_unlock (priv->mutex);
    }

 done:
    return request->req_id;
}

static void
docbook_cancel_page (YelpDocument *document, gint req_id)
{
    YelpDocbookPriv *priv;
    Request *request;

    debug_print (DB_FUNCTION, "entering\n");
    g_assert (document != NULL && YELP_IS_DOCBOOK (document));
    priv = YELP_DOCBOOK (document)->priv;

    g_mutex_lock (priv->mutex);
    request = g_hash_table_lookup (priv->reqs_by_req_id,
				   GINT_TO_POINTER (req_id));
    if (request) {
	priv->reqs_pending = g_slist_remove (priv->reqs_pending,
					     (gconstpointer) request);
	hash_slist_remove (priv->reqs_by_page_id,
			   request->page_id,
			   request);
	g_hash_table_remove (priv->reqs_by_req_id,
			     GINT_TO_POINTER (req_id));
    } else {
	g_warning ("YelpDocbook: Attempted to remove request %i,"
		   " but no such request exists.",
		   req_id);
    }
    g_mutex_unlock (priv->mutex);
}

static void
docbook_release_page (YelpDocument *document, YelpPage *page)
{
    YelpDocbookPriv *priv;

    debug_print (DB_FUNCTION, "entering\n");
    g_assert (document != NULL && YELP_IS_DOCBOOK (document));
    priv = YELP_DOCBOOK (document)->priv;

    g_mutex_lock (priv->mutex);
    hash_slist_remove (priv->pages,
		       yelp_page_get_id (page),
		       page);
    g_mutex_unlock (priv->mutex);
}


/******************************************************************************/
/** YelpTransform *************************************************************/

static void
transform_func (YelpTransform       *transform,
		YelpTransformSignal  signal,
		gpointer             func_data,
		YelpDocbook         *docbook)
{
    YelpDocbookPriv *priv;

    debug_print (DB_FUNCTION, "entering\n");
    g_assert (docbook != NULL && YELP_IS_DOCBOOK (docbook));
    priv = docbook->priv;
    g_assert (transform == priv->transform);

    if (priv->state == DOCBOOK_STATE_STOP) {
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
	priv->transform = NULL;
	priv->transform_running = FALSE;
	return;
    }

    switch (signal) {
    case YELP_TRANSFORM_CHUNK:
	transform_page_func (transform, (gchar *) func_data, docbook);
	break;
    case YELP_TRANSFORM_ERROR:
	docbook_error_all (docbook, (YelpError *) func_data);
	yelp_transform_release (transform);
	priv->transform = NULL;
	priv->transform_running = FALSE;
	break;
    case YELP_TRANSFORM_FINAL:
	transform_final_func (transform, docbook);
	break;
    }
}

static void
transform_page_func (YelpTransform *transform,
		     gchar         *page_id,
		     YelpDocbook   *docbook)
{
    YelpDocbookPriv *priv = docbook->priv;
    GSList *reqs, *cur;
    gchar *content;
    Request *request;

    debug_print (DB_FUNCTION, "entering\n");
    priv = docbook->priv;

    content = yelp_transform_eat_chunk (transform, page_id);

    g_mutex_lock (priv->mutex);
    if (g_hash_table_lookup (priv->contents, page_id)) {
	g_warning ("YelpDocbook: Attempted to add a page contents for"
		   " page %s, but contents already exist",
		   page_id);
	g_mutex_unlock (priv->mutex);
	g_free (page_id);
	g_free (content);
	return;
    }

    g_hash_table_insert (priv->contents, page_id, content);

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

static void
transform_final_func (YelpTransform *transform, YelpDocbook *docbook)
{
    YelpError *error;
    Request *request;
    YelpDocbookPriv *priv = docbook->priv;

    debug_print (DB_FUNCTION, "entering\n");

    g_mutex_lock (priv->mutex);

    while (priv->reqs_pending) {
	request = (Request *) priv->reqs_pending->data;

	error = yelp_error_new (_("Page not found"),
				_("The page %s was not found in the"
				  " document %s."),
				request->page_id,
				priv->filename);
	priv->reqs_pending = g_slist_delete_link (priv->reqs_pending,
						  priv->reqs_pending);
	request_error (request, error);
    }

    yelp_transform_release (transform);
    priv->transform = NULL;
    priv->transform_running = FALSE;

    if (priv->xmldoc)
	xmlFreeDoc (priv->xmldoc);
    priv->xmldoc = NULL;

    g_mutex_unlock (priv->mutex);
}


/******************************************************************************/
/** Threaded ******************************************************************/

static void
docbook_process (YelpDocbook *docbook)
{
    YelpDocbookPriv *priv;
    xmlDocPtr xmldoc = NULL;
    xmlChar *id = NULL;
    YelpError *error = NULL;
    xmlParserCtxtPtr parserCtxt = NULL;

    debug_print (DB_FUNCTION, "entering\n");

    g_assert (docbook != NULL && YELP_IS_DOCBOOK (docbook));
    g_object_ref (docbook);
    priv = docbook->priv;

    if (!g_file_test (priv->filename, G_FILE_TEST_IS_REGULAR)) {
	error = yelp_error_new (_("File not found"),
				_("The file ‘%s’ does not exist."),
				priv->filename);
	docbook_error_all (docbook, error);
	goto done;
    }

    parserCtxt = xmlNewParserCtxt ();
    xmldoc = xmlCtxtReadFile (parserCtxt,
			      (const char *) priv->filename, NULL,
			      XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
			      XML_PARSE_NOENT   | XML_PARSE_NONET   );

    if (xmldoc == NULL) {
	error = yelp_error_new (_("Could not parse file"),
				_("The file ‘%s’ could not be parsed because it is"
				  " not a well-formed XML document."),
				priv->filename);
	docbook_error_all (docbook, error);
	goto done;
    }

    if (xmlXIncludeProcessFlags (xmldoc,
				 XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
				 XML_PARSE_NOENT   | XML_PARSE_NONET   )
	< 0) {
	error = yelp_error_new (_("Could not parse file"),
				_("The file ‘%s’ could not be parsed because"
				  " one or more of its included files is not"
				  " a well-formed XML document."),
				priv->filename);
	docbook_error_all (docbook, error);
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
    if (id)
	priv->root_id = g_strdup ((gchar *) id);
    else {
	priv->root_id = g_strdup ("index");
	/* add the id attribute to the root element with value "index"
	 * so when we try to load the document later, it doesn't fail */
	xmlNewProp (priv->xmlcur, BAD_CAST "id", BAD_CAST "index");
    }
    g_mutex_unlock (priv->mutex);

    g_mutex_lock (priv->mutex);
    if (priv->state == DOCBOOK_STATE_STOP)
	goto done;
    g_mutex_unlock (priv->mutex);

    docbook_walk (docbook);

    g_mutex_lock (priv->mutex);
    if (priv->state == DOCBOOK_STATE_STOP)
	goto done;

    priv->transform = yelp_transform_new (STYLESHEET,
					  (YelpTransformFunc) transform_func,
					  docbook);
    priv->transform_running = TRUE;
    /* FIXME: we probably need to set our own params */
    yelp_transform_start (priv->transform,
			  priv->xmldoc,
			  NULL);
    g_mutex_unlock (priv->mutex);

 done:
    if (parserCtxt)
	xmlFreeParserCtxt (parserCtxt);

    priv->process_running = FALSE;
}

static void
docbook_map_id (YelpDocbook *docbook, gchar *source, gchar *target)
{
    GSList *reqs, *cur;
    Request *request;
    YelpDocbookPriv *priv = docbook->priv;

    g_mutex_lock (priv->mutex);
    if (g_hash_table_lookup (priv->page_ids, source)) {
	g_warning ("YelpDocbook: Attempted to add an ID mapping from"
		   " %1$s to %2$s, but %1$s is already mapped.",
		   source, target);
	g_mutex_unlock (priv->mutex);
	return;
    }

    g_hash_table_insert (priv->page_ids,
			 g_strdup (source),
			 g_strdup (target));

    if (!g_str_equal (source, target)) {
	reqs = g_hash_table_lookup (priv->reqs_by_page_id, source);
	for (cur = reqs; cur != NULL; cur = cur->next) {
	    if (cur->data) {
		request = (Request *) cur->data;
		g_free (request->page_id);
		request->page_id = g_strdup (target);
		hash_slist_insert (priv->reqs_by_page_id, target, request);
	    }
	}
	if (reqs)
	    g_hash_table_remove (priv->reqs_by_page_id, source);
    }

    g_mutex_unlock (priv->mutex);
}

static void
docbook_add_title (YelpDocbook *docbook, gchar *page_id, gchar *title)
{
    GSList *reqs, *cur;
    Request *request;
    YelpDocbookPriv *priv = docbook->priv;

    g_mutex_lock (priv->mutex);
    if (g_hash_table_lookup (priv->titles, page_id)) {
	g_warning ("YelpDocbook: Attempted to add the title '%1$s' for"
		   " page %2$s, but page %2$s already has a title.",
		   title, page_id);
	g_mutex_unlock (priv->mutex);
	return;
    }

    g_hash_table_insert (priv->titles, g_strdup (page_id), g_strdup (title));

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


/******************************************************************************/
/** Request *******************************************************************/

static gboolean
request_idle_title (Request *request)
{
    debug_print (DB_FUNCTION, "entering\n");

    if (request->cancel) {
	request->idle_funcs--;
	return FALSE;
    }

    request_try_title (request);
    request->idle_funcs--;

    return FALSE;
}

static gboolean
request_idle_page (Request *request)
{
    debug_print (DB_FUNCTION, "entering\n");

    if (request->cancel) {
	request->idle_funcs--;
	return FALSE;
    }

    request_try_page (request);
    request->idle_funcs--;

    return FALSE;
}

static gboolean
request_try_title (Request *request)
{
    YelpDocbookPriv *priv;
    gchar *title;
    gboolean retval = FALSE;

    debug_print (DB_FUNCTION, "entering\n");
    g_assert (request != NULL && YELP_IS_DOCBOOK (request->document));
    g_object_ref (request->document);
    priv = request->document->priv;

    title = g_hash_table_lookup (priv->titles, request->page_id);
    if (title) {
	request->func (YELP_DOCUMENT (request->document),
		       YELP_DOCUMENT_SIGNAL_TITLE,
		       request->req_id,
		       g_strdup (title),
		       request->user_data);
	retval = TRUE;
    }

    g_object_unref (request->document);
    return retval;
}

static gboolean
request_try_page (Request *request)
{
    YelpDocbookPriv *priv;
    YelpPage *page;
    gchar *content;
    gboolean retval = FALSE;

    debug_print (DB_FUNCTION, "entering\n");
    g_assert (request != NULL && YELP_IS_DOCBOOK (request->document));
    priv = request->document->priv;

    g_mutex_lock (priv->mutex);

    content = g_hash_table_lookup (priv->contents, request->page_id);
    if (content) {
	page = yelp_page_new_string (YELP_DOCUMENT (request->document),
				     request->page_id,
				     content);
	hash_slist_insert (priv->pages, request->page_id, page);
	yelp_page_set_prev_id (page, g_hash_table_lookup (priv->prev_ids, request->page_id));
	yelp_page_set_next_id (page, g_hash_table_lookup (priv->next_ids, request->page_id));
	yelp_page_set_up_id (page, g_hash_table_lookup (priv->up_ids, request->page_id));
	yelp_page_set_toc_id (page, priv->root_id);
	g_mutex_unlock (priv->mutex);
	request->func (YELP_DOCUMENT (request->document),
		       YELP_DOCUMENT_SIGNAL_PAGE,
		       request->req_id,
		       page,
		       request->user_data);
	g_mutex_lock (priv->mutex);
	priv->reqs_pending = g_slist_remove (priv->reqs_pending, request);
	retval = TRUE;
    }

    g_mutex_unlock (priv->mutex);
    return retval;
}

static void
request_error (Request *request, YelpError *error)
{
    YelpDocbookPriv *priv;

    debug_print (DB_FUNCTION, "entering\n");
    g_assert (request != NULL && YELP_IS_DOCBOOK (request->document));
    g_object_ref (request->document);
    priv = request->document->priv;

    request->func (YELP_DOCUMENT (request->document),
		   YELP_DOCUMENT_SIGNAL_ERROR,
		   request->req_id,
		   error,
		   request->user_data);
    g_object_ref (request->document);
}

static void
error_one (gpointer key, Request *request, YelpError *error)
{
    YelpError *new = yelp_error_copy (error);
    request_error (request, new);
}

static void
docbook_error_all (YelpDocbook *docbook, YelpError *error)
{
    YelpDocbookPriv *priv;

    debug_print (DB_FUNCTION, "entering\n");
    g_assert (docbook != NULL && YELP_IS_DOCBOOK (docbook));
    g_object_ref (docbook);
    priv = docbook->priv;

    g_hash_table_foreach (priv->reqs_by_req_id,
			  (GHFunc) error_one,
			  error);
    yelp_error_free (error);
    g_object_unref (docbook);
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
    g_free (request);
}


/******************************************************************************/
/** Walker ********************************************************************/

static void
docbook_walk (YelpDocbook *docbook)
{
    static       gint autoid = 0;
    gchar        autoidstr[20];
    xmlChar     *id = NULL;
    xmlChar     *title = NULL;
    gchar       *old_page_id = NULL;
    xmlNodePtr   cur, old_cur;
    GtkTreeIter  iter;
    GtkTreeIter *old_iter = NULL;
    YelpDocbookPriv *priv = docbook->priv;

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_DEBUG, "  priv->xmlcur->name: %s\n", priv->xmlcur->name);

    /* check for the yelp:chunk-depth or db.chunk.max_depth processing
     * instruction and set the max chunk depth accordingly.
     */
    if (priv->cur_depth == 0)
	for (cur = priv->xmlcur; cur; cur = cur->prev)
	    if (cur->type == XML_PI_NODE)
		if (!xmlStrcmp (cur->name, (const xmlChar *) "yelp:chunk-depth") ||
		    !xmlStrcmp (cur->name, (const xmlChar *) "db.chunk.max_depth")) {
		    gint max = atoi ((gchar *) cur->content);
		    if (max)
			priv->max_depth = max;
		    break;
		}

    id = xmlGetProp (priv->xmlcur, BAD_CAST "id");

    if (docbook_walk_chunkQ (docbook)) {
	title = BAD_CAST docbook_walk_get_title (docbook);

	/* if id attribute is not present, autogenerate a
	 * unique value, and insert it into the in-memory tree */
	if (!id) {
	    g_snprintf (autoidstr, 20, "_auto-gen-id-%d", ++autoid);
	    xmlNewProp (priv->xmlcur, BAD_CAST "id", BAD_CAST autoidstr);
	    id = xmlGetProp (priv->xmlcur, BAD_CAST "id"); 
	}

	debug_print (DB_DEBUG, "  id: \"%s\"\n", id);
	debug_print (DB_DEBUG, "  title: \"%s\"\n", title);

	docbook_add_title (docbook, (gchar *) id, (gchar *) title);

	gdk_threads_enter ();
	gtk_tree_store_append (GTK_TREE_STORE (priv->sections),
			       &iter,
			       priv->sections_iter);
	gtk_tree_store_set (GTK_TREE_STORE (priv->sections),
			    &iter,
			    YELP_DOCUMENT_COLUMN_ID, id,
			    YELP_DOCUMENT_COLUMN_TITLE, title,
			    -1);
	gdk_threads_leave ();

	if (priv->cur_prev_id) {
	    g_hash_table_insert (priv->prev_ids,
				 g_strdup ((gchar *) id),
				 g_strdup ((gchar *) priv->cur_prev_id));
	    g_hash_table_insert (priv->next_ids,
				 g_strdup ((gchar *) priv->cur_prev_id),
				 g_strdup ((gchar *) id));
	    g_free (priv->cur_prev_id);
	}
	priv->cur_prev_id = g_strdup ((gchar *) id);

	if (priv->cur_page_id)
	    g_hash_table_insert (priv->up_ids,
				 g_strdup ((gchar *) id),
				 g_strdup ((gchar *) priv->cur_page_id));
	old_page_id = priv->cur_page_id;
	priv->cur_page_id = g_strdup ((gchar *) id);

	old_iter = priv->sections_iter;
	if (priv->xmlcur->parent->type != XML_DOCUMENT_NODE)
	    priv->sections_iter = &iter;
    }

    old_cur = priv->xmlcur;
    priv->cur_depth++;

    if (id)
	docbook_map_id (docbook, (gchar *) id, (gchar *) priv->cur_page_id);

    for (cur = priv->xmlcur->children; cur; cur = cur->next) {
	if (cur->type == XML_ELEMENT_NODE) {
	    priv->xmlcur = cur;
	    docbook_walk (docbook);
	}
    }

    priv->cur_depth--;
    priv->xmlcur = old_cur;

    if (docbook_walk_chunkQ (docbook)) {
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
docbook_walk_chunkQ (YelpDocbook *docbook)
{
    if (docbook->priv->cur_depth <= docbook->priv->max_depth
	&& docbook_walk_divisionQ (docbook))
	return TRUE;
    else
	return FALSE;
}

static gboolean
docbook_walk_divisionQ (YelpDocbook *docbook)
{
    xmlNodePtr node = docbook->priv->xmlcur;
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
docbook_walk_get_title (YelpDocbook *docbook)
{
    gchar *infoname = NULL;
    xmlNodePtr child = NULL;
    xmlNodePtr title = NULL;
    xmlNodePtr title_tmp = NULL;
    YelpDocbookPriv *priv = docbook->priv;

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
