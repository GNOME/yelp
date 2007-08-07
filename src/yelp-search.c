/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2007 Don Scorgie <Don@Scorgie.org>
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
 * Author: Don Scorgie <Don@Scorgie.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libxml/tree.h>

#include "yelp-error.h"
#include "yelp-search.h"
#include "yelp-search-parser.h"
#include "yelp-transform.h"
#include "yelp-debug.h"

#define STYLESHEET DATADIR"/yelp/xslt/search2html.xsl"

#define YELP_SEARCH_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_SEARCH, YelpSearchPriv))

typedef enum {
    SEARCH_STATE_BLANK,   /* Brand new, run transform as needed */
    SEARCH_STATE_PARSING, /* Parsing/transforming document, please wait */
    SEARCH_STATE_PARSED,  /* All done, if we ain't got it, it ain't here */
    SEARCH_STATE_STOP     /* Stop everything now, object to be disposed */
} SearchState;

struct _YelpSearchPriv {
    gchar      *search_terms;
    SearchState    state;

    GMutex     *mutex;
    GThread    *thread;

    xmlDocPtr   xmldoc;

    gboolean    process_running;
    gboolean    transform_running;

    YelpTransform *transform;
};


static void           search_class_init       (YelpSearchClass        *klass);
static void           search_init             (YelpSearch             *search);
static void           search_try_dispose      (GObject             *object);
static void           search_dispose          (GObject             *object);

/* YelpDocument */
static void           search_request          (YelpDocument        *document,
					    gint                 req_id,
					    gboolean             handled,
					    gchar               *page_id,
					    YelpDocumentFunc     func,
					    gpointer             user_data);

/* YelpTransform */
static void           transform_func          (YelpTransform       *transform,
					       YelpTransformSignal  signal,
					       gpointer             func_data,
					       YelpSearch             *search);
static void           transform_page_func     (YelpTransform       *transform,
					       gchar               *page_id,
					       YelpSearch             *search);
static void           transform_final_func    (YelpTransform       *transform,
					       YelpSearch             *search);

/* Threaded */
static void           search_process             (YelpSearch             *search);

static YelpDocumentClass *parent_class;

GType
yelp_search_get_type (void)
{
    static GType type = 0;
    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpSearchClass),
	    NULL, NULL,
	    (GClassInitFunc) search_class_init,
	    NULL, NULL,
	    sizeof (YelpSearch),
	    0,
	    (GInstanceInitFunc) search_init,
	};
	type = g_type_register_static (YELP_TYPE_DOCUMENT,
				       "YelpSearch", 
				       &info, 0);
    }
    return type;
}

static void
search_class_init (YelpSearchClass *klass)
{
    GObjectClass      *object_class   = G_OBJECT_CLASS (klass);
    YelpDocumentClass *document_class = YELP_DOCUMENT_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = search_try_dispose;

    document_class->request = search_request;
    document_class->cancel = NULL;

    g_type_class_add_private (klass, sizeof (YelpSearchPriv));
}

static void
search_init (YelpSearch *search)
{
    YelpSearchPriv *priv;

    priv = search->priv = YELP_SEARCH_GET_PRIVATE (search);

    priv->state = SEARCH_STATE_BLANK;

    priv->mutex = g_mutex_new ();
}

static void
search_try_dispose (GObject *object)
{
    YelpSearchPriv *priv;

    g_assert (object != NULL && YELP_IS_SEARCH (object));
    priv = YELP_SEARCH (object)->priv;

    g_mutex_lock (priv->mutex);
    if (priv->process_running || priv->transform_running) {
	priv->state = SEARCH_STATE_STOP;
	g_idle_add ((GSourceFunc) search_try_dispose, object);
	g_mutex_unlock (priv->mutex);
    } else {
	g_mutex_unlock (priv->mutex);
	search_dispose (object);
    }
}

static void
search_dispose (GObject *object)
{
    YelpSearch *search = YELP_SEARCH (object);

    g_free (search->priv->search_terms);

    if (search->priv->xmldoc)
	xmlFreeDoc (search->priv->xmldoc);

    g_mutex_free (search->priv->mutex);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

YelpDocument *
yelp_search_new (gchar *filename)
{
    YelpSearch *search;

    g_return_val_if_fail (filename != NULL, NULL);

    search = (YelpSearch *) g_object_new (YELP_TYPE_SEARCH, NULL);
    search->priv->search_terms = g_strdup (filename);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  filename = \"%s\"\n", filename);

    yelp_document_add_page_id (YELP_DOCUMENT (search), "x-yelp-index", "index");

    return (YelpDocument *) search;
}


/******************************************************************************/
/** YelpDocument **************************************************************/

static void
search_request (YelpDocument     *document,
	     gint              req_id,
	     gboolean          handled,
	     gchar            *page_id,
	     YelpDocumentFunc  func,
	     gpointer          user_data)
{
    YelpSearch *search;
    YelpSearchPriv *priv;
    YelpError *error;

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  req_id  = %i\n", req_id);
    debug_print (DB_ARG, "  page_id = \"%s\"\n", page_id);

    g_assert (document != NULL && YELP_IS_SEARCH (document));

    if (handled)
	return;

    search = YELP_SEARCH (document);
    priv = search->priv;

    g_mutex_lock (priv->mutex);

    switch (priv->state) {
    case SEARCH_STATE_BLANK:
	priv->state = SEARCH_STATE_PARSING;
	priv->process_running = TRUE;
	priv->thread = g_thread_create ((GThreadFunc) search_process, search, FALSE, NULL);
	break;
    case SEARCH_STATE_PARSING:
	break;
    case SEARCH_STATE_PARSED:
    case SEARCH_STATE_STOP:
	/* Much bigger problems */
	error = yelp_error_new (_("Search could not be processed"),
				_("The requested search could not be processed."));
	yelp_document_error_request (document, req_id, error);
	break;
    }

    g_mutex_unlock (priv->mutex);
}


/******************************************************************************/
/** YelpTransform *************************************************************/

static void
transform_func (YelpTransform       *transform,
		YelpTransformSignal  signal,
		gpointer             func_data,
		YelpSearch             *search)
{
    YelpSearchPriv *priv;

    debug_print (DB_FUNCTION, "entering\n");

    g_assert (search != NULL && YELP_IS_SEARCH (search));

    priv = search->priv;

    g_assert (transform == priv->transform);

    if (priv->state == SEARCH_STATE_STOP) {
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
	transform_page_func (transform, (gchar *) func_data, search);
	break;
    case YELP_TRANSFORM_ERROR:
	yelp_document_error_pending (YELP_DOCUMENT (search), (YelpError *) func_data);
	yelp_transform_release (transform);
	priv->transform = NULL;
	priv->transform_running = FALSE;
	break;
    case YELP_TRANSFORM_FINAL:
	transform_final_func (transform, search);
	break;
    }
}

static void
transform_page_func (YelpTransform *transform,
		     gchar         *page_id,
		     YelpSearch       *search)
{
    YelpSearchPriv *priv;
    gchar *content;

    debug_print (DB_FUNCTION, "entering\n");

    priv = search->priv;
    g_mutex_lock (priv->mutex);

    content = yelp_transform_eat_chunk (transform, page_id);

    yelp_document_add_page (YELP_DOCUMENT (search), page_id, content);

    g_free (page_id);

    g_mutex_unlock (priv->mutex);
}

static void
transform_final_func (YelpTransform *transform, YelpSearch *search)
{
    YelpSearchPriv *priv = search->priv;

    debug_print (DB_FUNCTION, "entering\n");

    g_mutex_lock (priv->mutex);

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
search_process (YelpSearch *search)
{
    YelpSearchPriv *priv;
    YelpSearchParser *parser;
    YelpError *error = NULL;
    YelpDocument *document;

    debug_print (DB_FUNCTION, "entering\n");

    g_assert (search != NULL && YELP_IS_SEARCH (search));
    g_object_ref (search);
    priv = search->priv;
    document = YELP_DOCUMENT (search);

    parser = yelp_search_parser_new ();
    priv->xmldoc = yelp_search_parser_process (parser, priv->search_terms);
    yelp_search_parser_free (parser);

    if (priv->xmldoc == NULL) {
	error = yelp_error_new (_("Cannot process the search"),
				_("The search processor returned invalid results"));
	yelp_document_error_pending (document, error);
    }

    g_mutex_lock (priv->mutex);
    if (priv->state == SEARCH_STATE_STOP) {
	g_mutex_unlock (priv->mutex);
	goto done;
    }

    priv->transform = yelp_transform_new (STYLESHEET,
					  (YelpTransformFunc) transform_func,
					  search);
    priv->transform_running = TRUE;

    /* FIXME: we probably need to set our own params */
    yelp_transform_start (priv->transform,
			  priv->xmldoc,
			  NULL);
    g_mutex_unlock (priv->mutex);

 done:
    priv->process_running = FALSE;
    g_object_unref (search);
}
