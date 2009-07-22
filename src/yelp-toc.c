/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2007 Shaun McCance  <shaunm@gnome.org>
 *                    2007 Don Scorgie    <Don@Scorgie.org>
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
 *         Don Scorgie    <Don@Scorgie.org>
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
#define I_KNOW_RARIAN_0_8_IS_UNSTABLE
#include <rarian.h>

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
    gboolean       man_processed;
    gboolean       info_processed;

    YelpTransform *transform;

    xmlDocPtr     xmldoc;
    xmlNodePtr    xmlcur;
    gchar        *cur_page_id;
    gchar        *cur_prev_id;
};


static YelpDocument *toc_doc = NULL;

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
static void           toc_process_info    (YelpToc         *toc);
static void           toc_process_man     (YelpToc         *toc);
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

    priv->man_processed = FALSE;
    priv->info_processed = FALSE;

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

    toc_doc = NULL;
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
    debug_print (DB_FUNCTION, "entering\n");

    if (toc_doc == NULL) {
	toc_doc = (YelpDocument *) g_object_new (YELP_TYPE_TOC, NULL);
    }

    return (YelpDocument *) toc_doc;
}

YelpDocument *
yelp_toc_get (void)
{
    debug_print (DB_FUNCTION, "entering\n");

    if (toc_doc == NULL) {
	toc_doc = (YelpDocument *) g_object_new (YELP_TYPE_TOC, NULL);
    }

    return (YelpDocument *) toc_doc;
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
	priv->process_running = FALSE;
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
	priv->process_running = FALSE;
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
    yelp_document_final_pending (YELP_DOCUMENT (toc), error);

    yelp_transform_release (priv->transform);
    priv->transform = NULL;

    priv->transform_running = FALSE;
    priv->process_running = FALSE;
    priv->state = TOC_STATE_PARSED;

    if (priv->xmldoc) {
	xmlFreeDoc (priv->xmldoc);
    }
    priv->xmldoc = NULL;

    g_mutex_unlock (priv->mutex);
}


/******************************************************************************/
/** Threaded ******************************************************************/

static int
rrn_add_document (void *reg, void * user_data)
{
    xmlNodePtr node = (xmlNodePtr) user_data;
    RrnReg *r = (RrnReg *) reg;
    xmlNodePtr new;
    gchar *tmp;
    new = xmlNewChild (node, NULL, BAD_CAST "doc", NULL);
    xmlNewNsProp (new, NULL, BAD_CAST "href", BAD_CAST r->uri);
    xmlNewTextChild (new, NULL, BAD_CAST "title", BAD_CAST r->name);
    xmlNewTextChild (new, NULL, BAD_CAST "description", BAD_CAST r->comment);
    tmp = g_strdup_printf ("%d", r->weight);
    xmlNewNsProp (new, NULL, BAD_CAST "weight", BAD_CAST tmp);
    g_free (tmp);
    return FALSE;
}

static void
toc_process (YelpToc *toc)
{
    YelpTocPriv *priv;
    YelpError *error = NULL;
    xmlParserCtxtPtr parserCtxt = NULL;
    YelpDocument *document;
    gint  params_i = 0;
    gint  params_max = 10;
    gchar **params = NULL;
    GtkIconInfo *info = NULL ;
    GtkIconTheme *theme = (GtkIconTheme *) yelp_settings_get_icon_theme ();

    GThread *info_thread;
    GThread *man_thread;
    xmlXPathContextPtr xpath;
    xmlXPathObjectPtr  obj;
    gint i;
    debug_print (DB_FUNCTION, "entering\n");

    g_assert (toc != NULL && YELP_IS_TOC (toc));
    g_object_ref (toc);
    priv = toc->priv;
    document = YELP_DOCUMENT (toc);

    parserCtxt = xmlNewParserCtxt ();
    priv->xmldoc = xmlCtxtReadFile (parserCtxt,
			      (const char *) DATADIR "/yelp/toc.xml", NULL,
			      XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
			      XML_PARSE_NOENT   | XML_PARSE_NONET   );

    if (priv->xmldoc == NULL) {
	error = yelp_error_new (_("Could not parse file"),
				_("The TOC file "
				  "could not be parsed because it is"
				  " not a well-formed XML document."));
	yelp_document_error_pending (document, error);
	goto done;
    }

    xpath = xmlXPathNewContext (priv->xmldoc);
    obj = xmlXPathEvalExpression (BAD_CAST "//toc", xpath);

    for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	xmlNodePtr node = obj->nodesetval->nodeTab[i];
	xmlChar *icon = NULL;
	xmlChar *id = NULL;

	xmlNodeSetLang (node, BAD_CAST g_get_language_names ()[0]);

	xml_trim_titles (node, BAD_CAST "title");
	xml_trim_titles (node, BAD_CAST "description");

	icon = xmlGetProp (node, BAD_CAST "icon");
	if (icon) {
	    info = gtk_icon_theme_lookup_icon (theme, (gchar *) icon, 48, 
					       GTK_ICON_LOOKUP_NO_SVG);
	    if (info) {
		xmlNodePtr new = xmlNewChild (node, NULL, BAD_CAST "icon", 
					      NULL);
		xmlNewNsProp (new, NULL, BAD_CAST "file", 
			      BAD_CAST gtk_icon_info_get_filename (info));
		gtk_icon_info_free (info);
	    }
	}
	xmlFree (icon);
	id = xmlGetProp (node, BAD_CAST "id");

	if (id) {
	    if (g_str_equal (id, "Core"))
		xmlSetProp (node, BAD_CAST "id", BAD_CAST "index");
	    rrn_for_each_in_category (rrn_add_document,
				      (char *) id,
				      (void *) node);
	    xmlFree (id);
	}

    }
    xmlXPathFreeObject (obj);

    man_thread = g_thread_create ((GThreadFunc) toc_process_man, toc, TRUE, NULL);
    if (!man_thread) {
	g_warning ("Could not create Man page thread");
	priv->man_processed = TRUE;
    }

    info_thread = g_thread_create ((GThreadFunc) toc_process_info, toc, TRUE, NULL);
    if (!info_thread) {
	g_warning ("Could not create Info page thread");
	priv->info_processed = TRUE;
    }

    params = g_new0 (gchar *, params_max);
    yelp_settings_params (&params, &params_i, &params_max);

    if ((params_i + 10) >= params_max - 1) {
	params_max += 10;
	params = g_renew (gchar *, params, params_max);
    }

    info = gtk_icon_theme_lookup_icon (theme, "yelp-icon-big", 192, 0);
    if (info) {
	params[params_i++] = g_strdup ("help_icon");
	params[params_i++] = g_strdup_printf ("\"%s\"",
				      gtk_icon_info_get_filename (info));
	params[params_i++] = g_strdup ("help_icon_size");
	params[params_i++] = g_strdup_printf ("%i",
					      gtk_icon_info_get_base_size (info));
	gtk_icon_info_free (info);
    }

    params[params_i] = NULL;

    while (!priv->info_processed || !priv->man_processed) {
	g_thread_yield ();
    }

    g_mutex_lock (priv->mutex);

    priv->transform = yelp_transform_new (STYLESHEET,
					  (YelpTransformFunc) transform_func,
					  toc);
    priv->transform_running = TRUE;

    yelp_transform_start (priv->transform,
			  priv->xmldoc,
			  params);
    g_strfreev (params);
    g_mutex_unlock (priv->mutex);

 done:
    if (parserCtxt)
	xmlFreeParserCtxt (parserCtxt);

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

static int
rrn_info_add_document (RrnInfoEntry *entry, void *user_data)
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
    xmlNewTextChild (new, NULL, BAD_CAST "title", BAD_CAST entry->doc_name);
    xmlNewTextChild (new, NULL, BAD_CAST "description", BAD_CAST entry->comment);
    g_free(tmp);
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

    categories = rrn_info_get_categories ();
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
	
	rrn_info_for_each_in_category (*cat_iter, (RrnInfoForeachFunc) rrn_info_add_document, 
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
    g_mutex_lock (priv->mutex);
    priv->info_processed = TRUE;
    g_mutex_unlock (priv->mutex);
}

static int
rrn_add_man_document (RrnManEntry *entry, void *user_data)
{
    xmlNodePtr node = (xmlNodePtr) user_data;
    xmlNodePtr new;
    gchar *tmp;

    new = xmlNewChild (node, NULL, BAD_CAST "doc", NULL);
    tmp = g_strdup_printf ("man:%s", entry->path);

    xmlNewNsProp (new, NULL, BAD_CAST "href", BAD_CAST tmp);
    xmlNewTextChild (new, NULL, BAD_CAST "title", BAD_CAST entry->name);
    if (entry->comment)
	xmlNewTextChild (new, NULL, BAD_CAST "description", BAD_CAST entry->comment);
    g_free(tmp);

    return TRUE;
}

void
toc_process_man (YelpToc *toc)
{
    xmlNodePtr cat_node = NULL;
    xmlNodePtr mynode = NULL;
    YelpTocPriv * priv = toc->priv;
    int i, j;
    xmlXPathContextPtr xpath;
    xmlXPathObjectPtr  obj;
    xmlDocPtr          man_doc;
    xmlParserCtxtPtr parserCtxt = NULL;

    debug_print (DB_FUNCTION, "entering\n");

    parserCtxt = xmlNewParserCtxt ();

    man_doc = xmlCtxtReadFile (parserCtxt, DATADIR "/yelp/man.xml", NULL,
				     XML_PARSE_NOBLANKS | XML_PARSE_NOCDATA  |
				     XML_PARSE_NOENT    | XML_PARSE_NOERROR  |
				     XML_PARSE_NONET    );
    if (!man_doc) {
	g_warning ("Could not process man TOC");
	goto done;
    }

    xpath = xmlXPathNewContext (man_doc);
    obj = xmlXPathEvalExpression (BAD_CAST "//toc", xpath);

    for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	xmlNodePtr node = obj->nodesetval->nodeTab[i];
	xmlChar *sect = xmlGetProp (node, BAD_CAST "sect");
	xmlChar *id = xmlGetProp (node, BAD_CAST "id");

	if (sect) {
	    gchar **sects = g_strsplit ((gchar *)sect, " ", 0);

	    g_mutex_lock (priv->mutex);
	    yelp_document_add_page_id (YELP_DOCUMENT (toc), (gchar *) id, (gchar *) id);
	    g_mutex_unlock (priv->mutex);
	    
	    cat_node = xmlNewChild (node, NULL, BAD_CAST "toc",
				    NULL);
	    for (j = 0; sects[j] != NULL; j++) {
		rrn_man_for_each_in_category (sects[j], (RrnManForeachFunc) rrn_add_man_document, node);
	    }
	    g_strfreev (sects);
	}
	xmlFree (sect);
	xmlFree (id);
	xml_trim_titles (node, BAD_CAST "title");
	xml_trim_titles (node, BAD_CAST "description");
    }
    xmlXPathFreeObject (obj);
    xmlXPathFreeContext (xpath);

    mynode = xmlCopyNode (xmlDocGetRootElement (man_doc), 1);

    g_mutex_lock (priv->mutex);
    xmlAddChild (xmlDocGetRootElement (priv->xmldoc), mynode);
    g_mutex_unlock (priv->mutex);

    xmlFreeDoc (man_doc);

 done:
    if (parserCtxt)
	xmlFreeParserCtxt (parserCtxt);
    g_mutex_lock (priv->mutex);
    priv->man_processed = TRUE;
    g_mutex_unlock (priv->mutex);
}
