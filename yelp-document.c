/*
 * Copyright (C) 2006 Brent Smith <gnome@nextreality.net>
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
 * Author: Brent Smith  <gnome@nextreality.net>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "yelp-document.h"
#include "yelp-page.h"
#include "yelp-utils.h"
#include "yelp-debug.h"

#define YELP_DOCUMENT_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_DOCUMENT, YelpDocumentPriv))

struct _YelpDocumentPriv {
	YelpDocInfo *doc_info;

	/* key is string of page name, value is the corresponding YelpPage */
	GHashTable  *page_hash;

	/* the source_id of any current page requests */
	gint idle_source_id;

	/* callback functions for current page request */
	YelpPageLoadFunc  page_func;
	YelpPageTitleFunc title_func;
	YelpPageErrorFunc error_func;
};

/* properties for the YelpDocument class */
enum {
	PROP_0,
	PROP_DOCINFO
};

/* this defines our prototypes for class and instance init functions,
 * our full get_type function, and the global yelp_document_parent_class */
G_DEFINE_TYPE (YelpDocument, yelp_document, G_TYPE_OBJECT);

/* prototypes for static functions */
static void    document_set_property   (GObject         *object,
                                        guint            prop_id,
                                        const GValue    *value,
                                        GParamSpec      *pspec);
static void    document_get_property   (GObject         *object,
                                        guint            prop_id,
                                        GValue          *value,
                                        GParamSpec      *pspec);

static void
yelp_document_dispose (GObject *object)
{
	YelpDocument *document = YELP_DOCUMENT (object);
	YelpDocumentPriv *priv = document->priv;

	G_OBJECT_CLASS (yelp_document_parent_class)->dispose (object);
}

static void
yelp_document_finalize (GObject *object)
{
	G_OBJECT_CLASS (yelp_document_parent_class)->finalize (object);
}

static void
yelp_document_class_init (YelpDocumentClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	g_object_class->dispose  = yelp_document_dispose;
	g_object_class->finalize = yelp_document_finalize;

	/* install the document-info property */
	g_object_class_install_property (g_object_class,
	                                 PROP_DOCINFO,
	                                 g_param_spec_pointer ("document-info",
	                                                       "Document Information",
	                                                       "The YelpDocInfo struct",
	                                                       G_PARAM_CONSTRUCT_ONLY | 
	                                                       G_PARAM_READWRITE));

	g_type_class_add_private (g_object_class, sizeof (YelpDocumentPriv));
}

static void
yelp_document_init (YelpDocument *document)
{
	document->priv = YELP_DOCUMENT_GET_PRIVATE (document);
	YelpDocumentPriv *priv = document->priv;

	/* initialize the page hash table */
	priv->page_hash = 
		g_hash_table_new_full (g_str_hash, 
		                       g_str_equal, 
		                       NULL, /* use page->page_id directly */
				       (GDestroyNotify)yelp_page_free);
	
}

static void
document_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
	YelpDocument *document = YELP_DOCUMENT (object);

	switch (prop_id) {
	case PROP_DOCINFO:
		if (document->priv->doc_info)
			yelp_doc_info_unref (document->priv->doc_info);
		document->priv->doc_info = (YelpDocInfo *) g_value_get_pointer (value);
		yelp_doc_info_ref (document->priv->doc_info);
		break;
	default:
		break;
	}
}

static void
document_get_property (GObject      *object,
                       guint         prop_id,
                       GValue       *value,
                       GParamSpec   *pspec)
{
	YelpDocument *document = YELP_DOCUMENT (object);

	switch (prop_id) {
	case PROP_DOCINFO:
		g_value_set_pointer (value, document->priv->doc_info);
		break;
	default:
		break;
	}
}

/*****************************************
 * public methods for class YelpDocument *
 *****************************************/

/**
 * @document:   The #YelpDocument in which to get the page
 * @page_id:    a NULL terminated string indicating the name of the page to get.
 * @page_func:  the YelpPageLoadFunc callback function which is called when the
 * page has completed loading. 
 * @title_func: the YelpPageTitleFunc callback function which is called when the
 * title of the page is known.
 * @error_func: the YelpPageErrorFunc callback function which is called when an
 * error occurs trying to get the page.
 * @user_date:  User defined data that is passed to each callback function as the
 * last parameter
 *
 * Initiates a page request for the given document.  It takes callback 
 * functions for page loading, page title loading, and error handling. It returns a 
 * request ID, which can be used by yelp_document_cancel_get() to cancel the request. 
 *
 * Returns: an integer representing the source_id of the idle function that is
 * added to the main loop.  Use the yelp_document_cancel_get() function
 * with this source_id to cancel any pending page requests.  If the
 * function fails or there is already a page request in progress, it should 
 * return 0.
 */ 
gint
yelp_document_get_page (YelpDocument     *document,
                        const gchar      *page_id,
                        YelpPageLoadFunc  page_func,
                        YelpPageTitleFunc title_func,
                        YelpPageErrorFunc error_func,
                        gpointer          user_data)
{
	debug_print (DB_FUNCTION, "entering\n");

	g_return_val_if_fail (YELP_IS_DOCUMENT (document), 0);
	g_return_val_if_fail (page_id != NULL, 0);
	g_return_val_if_fail (page_func != NULL, 0);
	g_return_val_if_fail (title_func != NULL, 0);
	g_return_val_if_fail (error_func != NULL, 0);

	YelpDocumentPriv *priv = document->priv;
	YelpDocumentClass *klass = YELP_DOCUMENT_GET_CLASS (document);

	/* check for an in progress page request and bail if one exists */
	if (priv->idle_source_id > 0)
		return 0;

	/* save our callback functions in the private structure for use by the
	 * virtual functions get_page and cancel */
	priv->page_func = page_func;
	priv->title_func = title_func;
	priv->error_func = error_func;

	/* add the virtual functions get_page and cancel (to be implemented by
	 * the derived class) to the main loop to be run during idle */
	priv->idle_source_id = 
		g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, (GSourceFunc)klass->get_page,
		                 document, (GDestroyNotify)klass->cancel);

	debug_print (DB_FUNCTION, "leaving\n");
	return priv->idle_source_id;
}

/**
 * @document:  The #YelpDocument in which to cancel a page get
 * @source_id: The source_id returned from the yelp_document_get_page() function.
 *
 * Cancels a page request that is currently in progress.  If there
 * is no page request in progress, then the function returns immediately. 
 *
 * Returns: TRUE if the page request was found and removed, FALSE otherwise
 */
gboolean
yelp_document_cancel_get (YelpDocument   *document,
                          gint            source_id)
{
	debug_print (DB_FUNCTION, "entering\n");

	YelpDocumentPriv *priv = document->priv;
	gboolean retval;

	g_return_if_fail (YELP_IS_DOCUMENT (document));
	g_return_if_fail (source_id > 0);

	/* return if we don't have a running page request */
	if (priv->idle_source_id <= 0)
		return FALSE;

	/* this will call the GDestroyNotify callback (the virtual function cancel()) 
	 * to cleanup any resources currently allocated for the page get request */
	retval = g_source_remove (source_id);
	
	/* reset some private data */
	priv->idle_source_id = 0;
	priv->page_func = NULL;
	priv->title_func = NULL;
	priv->error_func = NULL;

	debug_print (DB_FUNCTION, "leaving\n");
	return retval;
}

/**
 * @document:  The #YelpDocument in which to get the sections
 *
 * Gets all the sections in a document.
 *
 * Returns: a GtkTreeModel representing all the sections in the
 * document
 *
 */
GtkTreeModel *
yelp_document_get_sections (YelpDocument *document)
{
	debug_print (DB_FUNCTION, "entering\n");

	YelpDocumentClass *klass = YELP_DOCUMENT_GET_CLASS (document);
	GtkTreeModel *sections;

	sections = klass->get_sections (document);

	debug_print (DB_FUNCTION, "leaving\n");
	return sections;
}

/**
 * @document:  The #YelpDocument in which to add the page
 * @page:      A #YelpPage to add to the #YelpDocument
 *
 * Adds a page to the #YelpDocument passed.
 *
 * Returns: nothing
 */
void
yelp_document_add_page (YelpDocument *document,
                        YelpPage     *page)
{
	debug_print (DB_FUNCTION, "entering\n");

	g_return_if_fail (document != NULL);
	g_return_if_fail (YELP_IS_DOCUMENT (document));
	g_return_if_fail (page != NULL);

	const gchar *page_id = yelp_page_get_id (page);
	if (!page_id)
		return;

	YelpDocumentPriv *priv = document->priv;

	g_hash_table_replace (priv->page_hash, (gchar *)page_id, page);

	debug_print (DB_FUNCTION, "leaving\n");
}

/*const YelpPage *
yelp_pager_get_page (YelpPager *pager, const gchar *frag_id)
{
	YelpPage    *page;
	const gchar *page_id = YELP_PAGER_GET_CLASS (pager)->resolve_frag (pager, frag_id);

	if (page_id == NULL)
		return NULL;

	page = (YelpPage *) g_hash_table_lookup (pager->priv->page_hash, page_id);

	return (const YelpPage *) page;
}*/
