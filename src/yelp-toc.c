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
#include <libxml/xmlreader.h>
#include <spoon.h>
#ifdef ENABLE_INFO
#include <spoon-info.h>
#endif /* ENABLE_INFO */

#include "yelp-error.h"
#include "yelp-toc.h"
#include "yelp-settings.h"
#include "yelp-transform.h"
#include "yelp-debug.h"

#define STYLESHEET DATADIR"/yelp/xslt/toc2html.xsl"

#define YELP_TOC_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_TOC, YelpTocPriv))

typedef enum {
    TOC_STATE_BLANK,   /* Brand new, run transform as needed */
    TOC_STATE_PARSING, /* Parsing/transforming document, please wait */
    TOC_STATE_PARSED,  /* All done, if we ain't got it, it ain't here */
    TOC_STATE_STOP     /* Stop everything now, object to be disposed */
} TocState;

struct _YelpTocPriv {
    TocState   state;

    GMutex        *mutex;
    GThread       *thread;

    gboolean       process_running;
    gboolean       transform_running;

    YelpTransform *transform;

    xmlDocPtr     xmldoc;
    xmlNodePtr    xmlcur;
    gchar        *cur_page_id;
    gchar        *cur_prev_id;
};


static void           toc_class_init      (YelpTocClass    *klass);
static void           toc_init            (YelpToc         *toc);
static void           toc_try_dispose     (GObject             *object);
static void           toc_dispose         (GObject             *object);

/* YelpDocument */
static void           toc_request         (YelpDocument        *document,
					       gint                 req_id,
					       gboolean             handled,
					       gchar               *page_id,
					       YelpDocumentFunc     func,
					       gpointer             user_data);

/* YelpTransform */
static void           transform_func          (YelpTransform       *transform,
					       YelpTransformSignal  signal,
					       gpointer             func_data,
					       YelpToc         *toc);
static void           transform_page_func     (YelpTransform       *transform,
					       gchar               *page_id,
					       YelpToc         *toc);
static void           transform_final_func    (YelpTransform       *transform,
					       YelpToc         *toc);

/* Threaded */
static void           toc_process         (YelpToc         *toc);
#ifdef ENABLE_INFO
static void           toc_process_info    (YelpToc         *toc);
#endif /* ENABLE_INFO */
static void           xml_trim_titles     (xmlNodePtr       node, 
					   xmlChar * nodetype);

static YelpDocumentClass *parent_class;

GType
yelp_toc_get_type (void)
{
    static GType type = 0;
    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpTocClass),
	    NULL, NULL,
	    (GClassInitFunc) toc_class_init,
	    NULL, NULL,
	    sizeof (YelpToc),
	    0,
	    (GInstanceInitFunc) toc_init,
	};
	type = g_type_register_static (YELP_TYPE_DOCUMENT,
				       "YelpToc", 
				       &info, 0);
    }
    return type;
}

static void
toc_class_init (YelpTocClass *klass)
{
    GObjectClass      *object_class   = G_OBJECT_CLASS (klass);
    YelpDocumentClass *document_class = YELP_DOCUMENT_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = toc_try_dispose;

    document_class->request = toc_request;
    document_class->cancel = NULL;

    g_type_class_add_private (klass, sizeof (YelpTocPriv));
}

static void
toc_init (YelpToc *toc)
{
    YelpTocPriv *priv;
    priv = toc->priv = YELP_TOC_GET_PRIVATE (toc);

    priv->state = TOC_STATE_BLANK;

    priv->mutex = g_mutex_new ();
}

static void
toc_try_dispose (GObject *object)
{
    YelpTocPriv *priv;

    g_assert (object != NULL && YELP_IS_TOC (object));

    priv = YELP_TOC (object)->priv;

    g_mutex_lock (priv->mutex);
    if (priv->process_running || priv->transform_running) {
	priv->state = TOC_STATE_STOP;
	g_idle_add ((GSourceFunc) toc_try_dispose, object);
	g_mutex_unlock (priv->mutex);
    } else {
	g_mutex_unlock (priv->mutex);
	toc_dispose (object);
    }
}

static void
toc_dispose (GObject *object)
{
    YelpToc *toc = YELP_TOC (object);

    if (toc->priv->xmldoc)
	xmlFreeDoc (toc->priv->xmldoc);

    g_free (toc->priv->cur_page_id);
    g_free (toc->priv->cur_prev_id);

    g_mutex_free (toc->priv->mutex);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

YelpDocument *
yelp_toc_new (void)
{
    YelpToc *toc;
    debug_print (DB_FUNCTION, "entering\n");

    toc = (YelpToc *) g_object_new (YELP_TYPE_TOC, NULL);

    return (YelpDocument *) toc;
}


/******************************************************************************/
/** YelpDocument **************************************************************/

static void
toc_request (YelpDocument     *document,
		 gint              req_id,
		 gboolean          handled,
		 gchar            *page_id,
		 YelpDocumentFunc  func,
		 gpointer          user_data)
{
    YelpToc *toc;
    YelpTocPriv *priv;
    YelpError *error;

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  req_id  = %i\n", req_id);
    debug_print (DB_ARG, "  page_id = \"%s\"\n", page_id);

    g_assert (document != NULL && YELP_IS_TOC (document));

    if (handled)
	return;

    toc = YELP_TOC (document);
    priv = toc->priv;

    g_mutex_lock (priv->mutex);

    switch (priv->state) {
    case TOC_STATE_BLANK:
	priv->state = TOC_STATE_PARSING;
	priv->process_running = TRUE;
	priv->thread = g_thread_create ((GThreadFunc) toc_process,
					toc, FALSE, NULL);
	break;
    case TOC_STATE_PARSING:
	break;
    case TOC_STATE_PARSED:
    case TOC_STATE_STOP:
	error = yelp_error_new (_("Page not found"),
				_("The page %s was not found in the TOC."),
				page_id);
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
		YelpToc         *toc)
{
    YelpTocPriv *priv;

    debug_print (DB_FUNCTION, "entering\n");

    g_assert (toc != NULL && YELP_IS_TOC (toc));

    priv = toc->priv;

    g_assert (transform == priv->transform);

    if (priv->state == TOC_STATE_STOP) {
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
	transform_page_func (transform, (gchar *) func_data, toc);
	break;
    case YELP_TRANSFORM_ERROR:
	yelp_document_error_pending (YELP_DOCUMENT (toc), (YelpError *) func_data);
	yelp_transform_release (transform);
	priv->transform = NULL;
	priv->transform_running = FALSE;
	break;
    case YELP_TRANSFORM_FINAL:
	transform_final_func (transform, toc);
	break;
    }
}

static void
transform_page_func (YelpTransform *transform,
		     gchar         *page_id,
		     YelpToc   *toc)
{
    YelpTocPriv *priv;
    gchar *content;
    debug_print (DB_FUNCTION, "entering\n");

    priv = toc->priv;
    g_mutex_lock (priv->mutex);

    content = yelp_transform_eat_chunk (transform, page_id);

#if 0 /* Used for debugging */
    gchar * filename = NULL;
    filename = g_strdup_printf ("out/%s.html", page_id);
    g_file_set_contents (filename, content, -1, NULL);
#endif

    yelp_document_add_page (YELP_DOCUMENT (toc), page_id, content);

    g_free (page_id);

    g_mutex_unlock (priv->mutex);
}

static void
transform_final_func (YelpTransform *transform, YelpToc *toc)
{
    YelpError *error;
    YelpTocPriv *priv = toc->priv;

    debug_print (DB_FUNCTION, "entering\n");

    g_mutex_lock (priv->mutex);

    error = yelp_error_new (_("Page not found"),
			    _("The requested page was not found in the TOC."));
    yelp_document_error_pending (YELP_DOCUMENT (toc), error);

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

static int
spoon_add_document (void *reg, void * user_data)
{
    xmlNodePtr node = (xmlNodePtr) user_data;
    SpoonReg *r = (SpoonReg *) reg;
    xmlNodePtr new;
    gchar *tmp;
    new = xmlNewChild (node, NULL, BAD_CAST "doc", NULL);
    xmlNewNsProp (new, NULL, BAD_CAST "href", BAD_CAST r->uri);
    xmlNewTextChild (new, NULL, BAD_CAST "title", BAD_CAST r->name);
    xmlNewTextChild (new, NULL, BAD_CAST "description", BAD_CAST r->comment);
    tmp = g_strdup_printf ("%d", r->weight);
    xmlNewNsProp (new, NULL, BAD_CAST "weight", BAD_CAST tmp);
    return FALSE;
}

static void
toc_process (YelpToc *toc)
{
    YelpTocPriv *priv;
    xmlDocPtr xmldoc = NULL;
    YelpError *error = NULL;
    xmlParserCtxtPtr parserCtxt = NULL;
    YelpDocument *document;

    xmlTextReaderPtr   reader;
    xmlXPathContextPtr xpath;
    xmlXPathObjectPtr  obj;
    gint i, ret;
    debug_print (DB_FUNCTION, "entering\n");

    g_assert (toc != NULL && YELP_IS_TOC (toc));
    g_object_ref (toc);
    priv = toc->priv;
    document = YELP_DOCUMENT (toc);

    parserCtxt = xmlNewParserCtxt ();
    xmldoc = xmlCtxtReadFile (parserCtxt,
			      (const char *) DATADIR "/yelp/toc.xml", NULL,
			      XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
			      XML_PARSE_NOENT   | XML_PARSE_NONET   );

    if (xmldoc == NULL) {
	error = yelp_error_new (_("Could not parse file"),
				_("The â€˜âTOC file€™ "
				  "could not be parsed because it is"
				  " not a well-formed XML document."));
	yelp_document_error_pending (document, error);
	goto done;
    }

    xpath = xmlXPathNewContext (xmldoc);
    obj = xmlXPathEvalExpression (BAD_CAST "//toc", xpath);

    for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	xmlNodePtr node = obj->nodesetval->nodeTab[i];
	xmlChar *icon = NULL;

	xml_trim_titles (node, BAD_CAST "title");
	xml_trim_titles (node, BAD_CAST "description");

	/* FIXME: Once hooked up properly into yelp, uncomment
	 * to make the nice shiny icon appear
	 */
	/*icon = xmlGetProp (node, BAD_CAST "icon");*/
	if (icon) {
	    GtkIconInfo *info;
	    GtkIconTheme *theme = 
		(GtkIconTheme *) gtk_icon_theme_get_default ();
	    info = gtk_icon_theme_lookup_icon (theme, (gchar *) icon, 48, 0);
	    if (info) {
		xmlNodePtr new = xmlNewChild (node, NULL, BAD_CAST "icon", 
					      NULL);
		xmlNewNsProp (new, NULL, BAD_CAST "file", 
			      BAD_CAST gtk_icon_info_get_filename (info));
		gtk_icon_info_free (info);
	    }
	}
	xmlFree (icon);

    }
    xmlXPathFreeObject (obj);

    reader = xmlReaderForFile (DATADIR "/yelp/scrollkeeper.xml", NULL,
			       XML_PARSE_NOBLANKS | XML_PARSE_NOCDATA  |
			       XML_PARSE_NOENT    | XML_PARSE_NOERROR  |
			       XML_PARSE_NONET    );
    ret = xmlTextReaderRead (reader);
    while (ret == 1) {
	if (!xmlStrcmp (xmlTextReaderConstLocalName (reader),
			BAD_CAST "toc")) {
	    xmlChar *id = xmlTextReaderGetAttribute (reader, BAD_CAST "id");
	    xmlNodePtr node;
	    gchar *xpath_s;
	    if (!id) {
		ret = xmlTextReaderRead (reader);
		continue;
	    }

	    g_mutex_lock (priv->mutex);

	    yelp_document_add_page_id (YELP_DOCUMENT (toc), (gchar *) id, (gchar *) id);
	    g_mutex_unlock (priv->mutex);
	    xpath_s = g_strdup_printf ("//toc[@id = '%s']", id);
	    obj = xmlXPathEvalExpression (BAD_CAST xpath_s, xpath);
	    g_free (xpath_s);

	    node = obj->nodesetval->nodeTab[0];
	    xmlXPathFreeObject (obj);

	    ret = xmlTextReaderRead (reader);
	    while (ret == 1) {
		if (!xmlStrcmp (xmlTextReaderConstLocalName (reader),
				BAD_CAST "subject")) {
		    xmlChar *cat = xmlTextReaderGetAttribute (reader, 
							      BAD_CAST "category");
		    spoon_for_each_in_category (spoon_add_document,
						(char *) cat,
						(void *) node);
		    xmlFree (cat);
		}
		else if (!xmlStrcmp (xmlTextReaderConstLocalName (reader),
				     BAD_CAST "toc")) {
		    break;
		}
		ret = xmlTextReaderRead (reader);
	    }

	    xmlFree (id);
	    ret = xmlTextReaderRead (reader);
	} else {
	    ret = xmlTextReaderRead (reader);
	}
    }
    xmlFreeTextReader (reader);
    xmlXPathFreeContext (xpath);

    g_mutex_lock (priv->mutex);
    priv->xmldoc = xmldoc;
    g_mutex_unlock (priv->mutex);

#ifdef ENABLE_INFO
    toc_process_info (toc);
#endif /* ENABLE_INFO */

    g_mutex_lock (priv->mutex);

    priv->transform = yelp_transform_new (STYLESHEET,
					  (YelpTransformFunc) transform_func,
					  toc);
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
    g_object_unref (toc);
}


static void
xml_trim_titles (xmlNodePtr node, xmlChar * nodetype)
{
    xmlNodePtr cur, keep = NULL;
    xmlChar *keep_lang = NULL;
    int j, keep_pri = INT_MAX;

    const gchar * const * langs = g_get_language_names ();

    for (cur = node->children; cur; cur = cur->next) {
	if (!xmlStrcmp (cur->name, nodetype)) {
	    xmlChar *cur_lang = NULL;
	    int cur_pri = INT_MAX;
	    cur_lang = xmlNodeGetLang (cur);
	    if (cur_lang) {
		for (j = 0; langs[j]; j++) {
		    if (g_str_equal (cur_lang, langs[j])) {
			cur_pri = j;
			break;
		    }
		}
	    } else {
		cur_pri = INT_MAX - 1;
	    }
	    if (cur_pri <= keep_pri) {
		if (keep_lang)
		    xmlFree (keep_lang);
		keep_lang = cur_lang;
		keep_pri = cur_pri;
		keep = cur;
	    } else {
		if (cur_lang)
		    xmlFree (cur_lang);
	    }
	}
    }
    cur = node->children;
    while (cur) {
	xmlNodePtr this = cur;
	cur = cur->next;
	if (!xmlStrcmp (this->name, nodetype)) {
	    if (this != keep) {
		xmlUnlinkNode (this);
		xmlFreeNode (this);
	    }
	}
    }
    xmlFree (keep_lang);
}

#ifdef ENABLE_INFO
static int
spoon_info_add_document (SpoonInfoEntry *entry, void *user_data)
{
    xmlNodePtr node = (xmlNodePtr) user_data;
    xmlNodePtr new;
    gchar *tmp;

    new = xmlNewChild (node, NULL, BAD_CAST "doc", NULL);
    if (entry->section)
	tmp = g_strdup_printf("info:%s#%s", entry->name, entry->section);
    else
	tmp = g_strdup_printf("info:%s", entry->name);
    xmlNewNsProp (new, NULL, BAD_CAST "href", BAD_CAST tmp);
    xmlNewTextChild (new, NULL, BAD_CAST "title", BAD_CAST entry->name);
    xmlNewTextChild (new, NULL, BAD_CAST "description", BAD_CAST entry->comment);
    return TRUE;

}

static void
toc_process_info (YelpToc *toc)
{
    xmlNodePtr node = NULL;
    xmlNodePtr cat_node = NULL;
    xmlNodePtr mynode = NULL;
    char **categories = NULL;
    char **cat_iter = NULL;
    int sectno = 0;
    YelpTocPriv * priv = toc->priv;
    int i;
    xmlXPathContextPtr xpath;
    xmlXPathObjectPtr  obj;
    xmlDocPtr info_doc;    
    xmlParserCtxtPtr parserCtxt = NULL;

    debug_print (DB_FUNCTION, "entering\n");

    parserCtxt = xmlNewParserCtxt ();
    
    info_doc = xmlCtxtReadFile (parserCtxt, 
				      DATADIR "/yelp/info.xml", NULL,
				      XML_PARSE_NOBLANKS | 
				      XML_PARSE_NOCDATA  |
				      XML_PARSE_NOENT    | 
				      XML_PARSE_NOERROR  |
				      XML_PARSE_NONET    );

    if (!info_doc) {
	g_warning ("Could not process info TOC");
	goto done;
    }

    g_mutex_lock (priv->mutex);
    yelp_document_add_page_id (YELP_DOCUMENT (toc), (gchar *) "Info", (gchar *) "Info");
    g_mutex_unlock (priv->mutex);

    xpath = xmlXPathNewContext (info_doc);
    obj = xmlXPathEvalExpression (BAD_CAST "//toc", xpath);
    node = obj->nodesetval->nodeTab[0];
    for (i=0; i < obj->nodesetval->nodeNr; i++) {
	xmlNodePtr tmpnode = obj->nodesetval->nodeTab[i];
	xml_trim_titles (tmpnode, BAD_CAST "title");
	xml_trim_titles (tmpnode, BAD_CAST "description");
    }
    xmlXPathFreeObject (obj);
    xmlXPathFreeContext (xpath);

    categories = spoon_info_get_categories ();
    cat_iter = categories;

    while (cat_iter && *cat_iter) {
	char *tmp;
	
	cat_node = xmlNewChild (node, NULL, BAD_CAST "toc",
				NULL);
	tmp = g_strdup_printf ("%d", sectno);
	xmlNewNsProp (cat_node, NULL, BAD_CAST "sect",
		      BAD_CAST tmp);
	g_free (tmp);
	tmp = g_strdup_printf ("infosect%d", sectno);
	g_mutex_lock (priv->mutex);
	yelp_document_add_page_id (YELP_DOCUMENT (toc), (gchar *) tmp, (gchar *) tmp);
	g_mutex_unlock (priv->mutex);

	xmlNewNsProp (cat_node, NULL, BAD_CAST "id",
		      BAD_CAST tmp);
	g_free (tmp);
	sectno++;
	xmlNewTextChild (cat_node, NULL, BAD_CAST "title",
			 BAD_CAST *cat_iter);
	
	spoon_info_for_each_in_category (*cat_iter, (SpoonInfoForeachFunc) spoon_info_add_document, 
					 cat_node);
	cat_iter++;
    }

    mynode = xmlCopyNode (xmlDocGetRootElement (info_doc), 1);

    g_mutex_lock (priv->mutex);
    xmlAddChild (xmlDocGetRootElement (priv->xmldoc), mynode);
    g_mutex_unlock (priv->mutex);

    xmlFreeDoc (info_doc);

 done:
    if (parserCtxt)
	xmlFreeParserCtxt (parserCtxt);

}
#endif /* ENABLE_INFO */
