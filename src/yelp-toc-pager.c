/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003 Shaun McCance  <shaunm@gnome.org>
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

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xmlreader.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/HTMLtree.h>
#include <libxslt/xslt.h>
#include <libxslt/templates.h>
#include <libxslt/transform.h>
#include <libxslt/extensions.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/xsltutils.h>
#include <spoon.h>
#include <spoon-reg-utils.h>

#include "yelp-debug.h"
#include "yelp-error.h"
#include "yelp-settings.h"
#include "yelp-toc-pager.h"
#include "yelp-utils.h"
#include "yelp-io-channel.h"

#define YELP_NAMESPACE "http://www.gnome.org/yelp/ns"

#define STYLESHEET_PATH DATADIR"/yelp/xslt/"
#define TOC_STYLESHEET  STYLESHEET_PATH"toc2html.xsl"

typedef gboolean      (*ProcessFunction)        (YelpTocPager      *pager);

#define YELP_TOC_PAGER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_TOC_PAGER, YelpTocPagerPriv))

struct _YelpTocPagerPriv {
    gboolean      sk_docomf;
    GSList       *omf_pending;   /* list of directories which contain omf files
				  * to process */
    GHashTable   *omf_dirhash;   /* key   = directory name; 
				    value = GSList of omf files to process */
    xmlDocPtr     omfindex_xml;  /* in memory cache file for omf files */
    xmlNodePtr    omf_root;
    xmlNodePtr    omf_ins;

#ifdef ENABLE_MAN
    gint manpage_count;
    xmlNodePtr root;
    xmlNodePtr ins;
    xmlDocPtr manindex_xml;
    GSList *mandir_list;
    GSList *mandir_ptr;          /* ptr to current entry in mandir_list */
    GSList *mandir_langpath;     /* ptr to current entry in mandir_ptr */
    GHashTable *man_secthash;
    GHashTable *man_manhash;
    GHashTable *man_dirlang;     /* key = man page directory, value = language */
#endif

    xmlDocPtr     toc_doc;
    xmlDocPtr     man_doc;
    xmlDocPtr     info_doc;

    GHashTable   *unique_hash;
    GHashTable   *category_hash;

    xmlParserCtxtPtr  parser;

    ProcessFunction   pending_func;

    gboolean      cancel;
    gint          pause_count;

    xsltStylesheetPtr       stylesheet;
    xsltTransformContextPtr transformContext;
    
    guint process_id;
};

static void          toc_pager_class_init      (YelpTocPagerClass *klass);
static void          toc_pager_init            (YelpTocPager      *pager);
static void          toc_pager_dispose         (GObject           *gobject);

static void          toc_pager_error           (YelpPager        *pager);
static void          toc_pager_finish          (YelpPager        *pager);

static gboolean      toc_pager_process         (YelpPager         *pager);
static void          toc_pager_cancel          (YelpPager         *pager);
static const gchar * toc_pager_resolve_frag    (YelpPager         *pager,
						const gchar       *frag_id);
static GtkTreeModel * toc_pager_get_sections    (YelpPager         *pager);

static gboolean      toc_process_pending       (YelpTocPager      *pager);


static gboolean      process_read_menu         (YelpTocPager      *pager);
static gboolean      process_xslt              (YelpTocPager      *pager);
static gboolean      process_read_scrollkeeper (YelpTocPager      *pager,
                                                gchar             *content_list);
static gboolean      process_omf_pending       (YelpTocPager      *pager);
#ifdef ENABLE_MAN
static gboolean      process_mandir_pending    (YelpTocPager      *pager);
#endif
#ifdef ENABLE_INFO
static gboolean      process_info_pending      (YelpTocPager      *pager);
#endif
static gboolean      process_cleanup           (YelpTocPager      *pager);

static void          toc_add_doc_info          (YelpTocPager      *pager,
						YelpDocInfo       *doc_info);
static void          toc_remove_doc_info       (YelpTocPager      *pager,
						YelpDocInfo       *doc_info);
static void          xml_trim_titles           (xmlNodePtr         node,
						xmlChar           *nodetype);
static void          xslt_yelp_document        (xsltTransformContextPtr ctxt,
						xmlNodePtr              node,
						xmlNodePtr              inst,
						xsltStylePreCompPtr     comp);

static YelpPagerClass *parent_class;

static YelpTocPager   *toc_pager;

GType
yelp_toc_pager_get_type (void)
{
    static GType type = 0;

    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpTocPagerClass),
	    NULL,
	    NULL,
	    (GClassInitFunc) toc_pager_class_init,
	    NULL,
	    NULL,
	    sizeof (YelpTocPager),
	    0,
	    (GInstanceInitFunc) toc_pager_init,
	};
	type = g_type_register_static (YELP_TYPE_PAGER,
				       "YelpTocPager", 
				       &info, 0);
    }
    return type;
}

static void
toc_pager_class_init (YelpTocPagerClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);
    YelpPagerClass *pager_class  = YELP_PAGER_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = toc_pager_dispose;

    pager_class->error        = toc_pager_error;
    pager_class->cancel       = toc_pager_cancel;
    pager_class->finish       = toc_pager_finish;

    pager_class->process      = toc_pager_process;
    pager_class->cancel       = toc_pager_cancel;
    pager_class->resolve_frag = toc_pager_resolve_frag;
    pager_class->get_sections = toc_pager_get_sections;

    g_type_class_add_private (klass, sizeof (YelpTocPagerPriv));
}

static void
toc_pager_init (YelpTocPager *pager)
{
    YelpTocPagerPriv *priv;

    pager->priv = priv = YELP_TOC_PAGER_GET_PRIVATE (pager);

    priv->parser = xmlNewParserCtxt ();

    priv->unique_hash   = g_hash_table_new (g_str_hash, g_str_equal);
    priv->category_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
						 g_free,     NULL);

    priv->cancel       = 0;
    priv->pause_count  = 0;
    priv->pending_func = NULL;
    priv->process_id   = 0;
}

static void
toc_pager_dispose (GObject *object)
{
    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

void
yelp_toc_pager_init (void)
{
    YelpDocInfo *doc_info;

    doc_info = yelp_doc_info_get ("x-yelp-toc:", FALSE);
    
    toc_pager = (YelpTocPager *) g_object_new (YELP_TYPE_TOC_PAGER,
					       "document-info", doc_info,
					       NULL);

    yelp_pager_start (YELP_PAGER (toc_pager));
}

YelpTocPager *
yelp_toc_pager_get (void)
{
    return toc_pager;
}

void
yelp_toc_pager_pause (YelpTocPager *pager)
{
    g_return_if_fail (pager != NULL);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  pause_count = %i\n", pager->priv->pause_count + 1);

    pager->priv->pause_count ++;

    debug_print (DB_FUNCTION, "exiting\n");
}

void
yelp_toc_pager_unpause (YelpTocPager *pager)
{
    g_return_if_fail (pager != NULL);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  pause_count = %i\n", pager->priv->pause_count - 1);

    pager->priv->pause_count--;
    if (pager->priv->pause_count < 0) {
	g_warning (_("YelpTocPager: Pause count is negative."));
	pager->priv->pause_count = 0;
    }

    if (pager->priv->pause_count < 1 &&
	pager->priv->process_id == 0) {
	pager->priv->process_id = 
	    gtk_idle_add_priority (G_PRIORITY_LOW,
				   (GtkFunction) toc_process_pending,
				   pager);
    }
    
    debug_print (DB_FUNCTION, "exiting\n");
}

/******************************************************************************/

static void
toc_pager_error (YelpPager *pager)
{
    debug_print (DB_FUNCTION, "entering\n");
    yelp_pager_set_state (pager, YELP_PAGER_STATE_ERROR);
}

static void
toc_pager_cancel (YelpPager *pager)
{
    YelpTocPagerPriv *priv = YELP_TOC_PAGER (pager)->priv;

    debug_print (DB_FUNCTION, "entering\n");
    yelp_pager_set_state (pager, YELP_PAGER_STATE_INVALID);

    priv->cancel = TRUE;
}

static void
toc_pager_finish (YelpPager   *pager)
{
    debug_print (DB_FUNCTION, "entering\n");
    yelp_pager_set_state (pager, YELP_PAGER_STATE_FINISHED);
}

static gboolean
toc_pager_process (YelpPager *pager)
{
    YelpTocPagerPriv *priv = YELP_TOC_PAGER (pager)->priv;

    debug_print (DB_FUNCTION, "entering\n");

    yelp_pager_set_state (pager, YELP_PAGER_STATE_PARSING);
    g_signal_emit_by_name (pager, "parse");

    /* Set it running */
    yelp_pager_set_state (pager, YELP_PAGER_STATE_RUNNING);
    g_signal_emit_by_name (pager, "start");

    priv->pending_func = (ProcessFunction) process_read_menu;

    if (priv->process_id == 0) {
	priv->process_id = 
	    gtk_idle_add_priority (G_PRIORITY_LOW,
				   (GtkFunction) toc_process_pending,
				   pager);
    }
    return FALSE;
}

/* we take advantage of the resolve_frag function to do lazy
 * loading of each page the TOC.  frag_id is the id attribute 
 * of the toc element that we want. 
 *
 * yes, this kind of a hack */
static const gchar *
toc_pager_resolve_frag (YelpPager *pager, const gchar *frag_id)
{
    YelpTocPagerPriv *priv = YELP_TOC_PAGER (pager)->priv;
    YelpPage *page = NULL;
    GError *error = NULL;
    xmlDocPtr outdoc = NULL;
    gchar **params = NULL;
    gint  params_i = 0;
    gint  params_max = 10;
    GtkIconInfo *info;
    GtkIconTheme *theme = (GtkIconTheme *) yelp_settings_get_icon_theme ();

    if (priv->toc_doc == NULL)
	return NULL;

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "frag_id = %s\n", frag_id);

    /* if frag_id is NULL, we assume we are loading the top level TOC page */
    if (frag_id == NULL)
	frag_id = "index";

    /* we use the "frag_id" as the page_id here */
    page = (YelpPage *) yelp_pager_get_page_from_id (pager, frag_id);

    /* if we found the page just return */
    if (page != NULL) {
	debug_print (DB_DEBUG, "found the frag_id=%s\n", frag_id);
	return frag_id;
    }

    /* only create and parse the stylesheet on the first call to this function */
    if (!priv->stylesheet) {
	priv->stylesheet = xsltParseStylesheetFile (BAD_CAST TOC_STYLESHEET);
    }

    if (!priv->stylesheet) {
	g_set_error (&error, YELP_ERROR, YELP_ERROR_PROC,
		     _("The table of contents could not be processed. The "
		       "file ‘%s’ is either missing or is not a valid XSLT "
		       "stylesheet."),
		     TOC_STYLESHEET);
	yelp_pager_error (YELP_PAGER (pager), error);
	goto done;
    }

    priv->transformContext = xsltNewTransformContext (priv->stylesheet,
						      priv->toc_doc);
    priv->transformContext->_private = pager;
    xsltRegisterExtElement (priv->transformContext,
			    BAD_CAST "document",
			    BAD_CAST YELP_NAMESPACE,
			    (xsltTransformFunction) xslt_yelp_document);

    params = g_new0 (gchar *, params_max);
    yelp_settings_params (&params, &params_i, &params_max);

    if ((params_i + 10) >= params_max - 1) {
	params_max += 10;
	params = g_renew (gchar *, params, params_max);
    }

    info = gtk_icon_theme_lookup_icon (theme, "yelp-icon-big", 192, 0);
    if (info) {
	params[params_i++] = "help_icon";
	params[params_i++] = g_strdup_printf ("\"%s\"",
					      gtk_icon_info_get_filename (info));
	params[params_i++] = "help_icon_size";
	params[params_i++] = g_strdup_printf ("%i",
					      gtk_icon_info_get_base_size (info));
	gtk_icon_info_free (info);
    }

    /* we specify the id attribute of the toc element that we want to process to
     * the stylesheet */
    params[params_i++] = "yelp.toc.id";
    params[params_i++] = g_strdup_printf ("\'%s\'", frag_id);

    params[params_i++] = NULL;

    outdoc = xsltApplyStylesheetUser (priv->stylesheet,
				      priv->toc_doc,
				      (const gchar **)params, NULL, NULL,
				      priv->transformContext);
    /* Don't do this */
    g_signal_emit_by_name (pager, "finish");

 done:
    if (params) {
	for (params_i = 0; params[params_i] != NULL; params_i++)
	    if (params_i % 2 == 1)
		g_free ((gchar *) params[params_i]);
    }
    if (outdoc)
	xmlFreeDoc (outdoc);
    /* with this hack (this function) we can't free the toc_doc */
    /*if (priv->toc_doc) {
	xmlFreeDoc (priv->toc_doc);
	priv->toc_doc = NULL;
    }*/
    if (priv->transformContext) {
	xsltFreeTransformContext (priv->transformContext);
	priv->transformContext = NULL;
    }

    return frag_id;
}

static GtkTreeModel *
toc_pager_get_sections (YelpPager *pager)
{
    return NULL;
}

/******************************************************************************/

static gboolean
toc_process_pending (YelpTocPager *pager)
{
    gboolean readd;
    YelpTocPagerPriv *priv = pager->priv;
    static ProcessFunction process_funcs[] = {
	process_read_menu,
	/*process_omf_pending,*/
#ifdef ENABLE_MAN
	process_mandir_pending,
#endif
#ifdef ENABLE_INFO
	process_info_pending,
#endif
	/* process_xslt, */
	process_cleanup,
	NULL
    };
    static gint process_i = 0;

    if (priv->cancel || !priv->pending_func) {
	/* FIXME: clean stuff up. */
	priv->process_id = 0;
	return FALSE;
    }

    readd = priv->pending_func(pager);

    /* if the pending function returned FALSE, then we move on to
     * calling the next function */
    if (!readd)
	priv->pending_func = process_funcs[++process_i];

    if (priv->pending_func == NULL)
	g_signal_emit_by_name (pager, "finish");

    if ((priv->pending_func != NULL) && (priv->pause_count < 1))
	return TRUE;
    else {
	priv->process_id = 0;
	return FALSE;
    }
}

/** process_read_scrollkeeper *************************************************/

static void
sk_startElement (void           *pager,
		 const xmlChar  *name,
		 const xmlChar **attrs)
{
    YelpTocPagerPriv *priv = YELP_TOC_PAGER (pager)->priv;
    if (xmlStrEqual((const xmlChar*) name, BAD_CAST "docomf"))
	priv->sk_docomf = TRUE;
}

static void
sk_endElement (void          *pager,
	       const xmlChar *name)
{
    YelpTocPagerPriv *priv = YELP_TOC_PAGER (pager)->priv;
    if (xmlStrEqual((const xmlChar*) name, BAD_CAST "docomf"))
	priv->sk_docomf = FALSE;
}

static void
sk_characters (void          *pager,
	       const xmlChar *ch,
	       int            len)
{
    gchar *omf;
    YelpTocPagerPriv *priv = YELP_TOC_PAGER (pager)->priv;

    if (priv->sk_docomf) {
	GSList *filelist = NULL;
	GSList *ptr = NULL;
	gchar *dirname = NULL;
	    
	omf = g_strndup ((gchar *) ch, len);
	/* since the directory that the omf file is in can be in any
	 * order, we use a hash to aggregrate the files by directory.
	 * We end up with a hash whose key is the dirname, and whose
	 * values are GSLists of filenames. */
	if (!priv->omf_dirhash)
	    priv->omf_dirhash = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                               g_free, NULL);

	dirname = g_path_get_dirname (omf);
	
	ptr = g_hash_table_lookup (priv->omf_dirhash, dirname);
	
	if (!ptr) {
	    /* this directory is not in the hash; create an entry for it */
	    filelist = g_slist_append (filelist, omf);
	    g_hash_table_insert (priv->omf_dirhash, dirname, filelist);
	
	    /* add the directory name to a list so we can iterate through
	     * the hash later */
	    priv->omf_pending = g_slist_prepend (priv->omf_pending, 
	                                         g_path_get_dirname(omf));
	} else {
	    /* prepend to the existing file list for this directory entry */
	    ptr = g_slist_prepend (ptr, omf);
	    /* writes over old value */
	    g_hash_table_insert (priv->omf_dirhash, dirname, ptr);
	}
	
    }
}

static gboolean
process_read_scrollkeeper (YelpTocPager *pager, gchar *content_list)
{
    static xmlSAXHandler sk_sax_handler = { 0, };

    if (!sk_sax_handler.startElement) {
	sk_sax_handler.startElement = sk_startElement;
	sk_sax_handler.endElement   = sk_endElement;
	sk_sax_handler.characters   = sk_characters;
	sk_sax_handler.initialized  = TRUE;
    }

    xmlSAXUserParseFile (&sk_sax_handler, pager, content_list);

    return FALSE;
}

static gboolean
files_are_equivalent (gchar *file1, gchar *file2)
{
    FILE *f1;
    FILE *f2;
    guint data1;
    guint data2;

    f1 = fopen (file1, "r");
    if (!f1)
	return FALSE;

    f2 = fopen (file2, "r");
    if (!f2)
	return FALSE;

    while (fread (&data1, sizeof (gint), 1, f1)) {
	fread (&data2, sizeof (gint), 1, f2);
	if (data1 != data2)
	    return FALSE;
    }

    fclose (f1);
    fclose (f2);

    return TRUE;
}

/* returns a newly created hash table with the following keys:
 * url, title, description, category, language, seriesid
 * caller must free the hash table with g_hash_table_destroy()
 * returns NULL on failure */
static GHashTable *
get_omf_attributes (YelpTocPager *pager, const gchar *omffile)
{
    YelpTocPagerPriv *priv  = YELP_TOC_PAGER (pager)->priv;
    GHashTable *hash = NULL;
    xmlXPathContextPtr xpath   = NULL;
    xmlXPathObjectPtr  object  = NULL;
    xmlDocPtr          omf_doc = NULL;

    omf_doc = xmlCtxtReadFile (priv->parser, omffile, NULL,
                               XML_PARSE_NOBLANKS | XML_PARSE_NOCDATA  |
                               XML_PARSE_NOENT    | XML_PARSE_NOERROR  |
                               XML_PARSE_NONET    );

    if (!omf_doc) {
	g_warning (_("Could not load the OMF file '%s'."), omffile);
	return NULL;
    }

    hash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    xpath = xmlXPathNewContext (omf_doc);

    object = 
       xmlXPathEvalExpression (BAD_CAST "string(/omf/resource/identifier/@url)", 
    		               xpath);
    g_hash_table_insert (hash, "url", g_strdup ((gchar *) object->stringval));
    xmlXPathFreeObject (object);

    object =
       xmlXPathEvalExpression (BAD_CAST "string(/omf/resource/title)", 
                               xpath);
    g_hash_table_insert (hash, "title", g_strdup ((gchar *) object->stringval));
    xmlXPathFreeObject (object);

    object =
       xmlXPathEvalExpression (BAD_CAST "string(/omf/resource/description)", 
                               xpath);
    g_hash_table_insert (hash, "description", g_strdup ((gchar *) object->stringval));
    xmlXPathFreeObject (object);

    object =
       xmlXPathEvalExpression (BAD_CAST "string(/omf/resource/subject/@category)", 
                               xpath);
    g_hash_table_insert (hash, "category", g_strdup ((gchar *) object->stringval));
    xmlXPathFreeObject (object);

    object =
       xmlXPathEvalExpression (BAD_CAST "string(/omf/resource/language/@code)", 
                               xpath);
    g_hash_table_insert (hash, "language", g_strdup ((gchar *) object->stringval));
    xmlXPathFreeObject (object);

    object =
       xmlXPathEvalExpression (BAD_CAST "string(/omf/resource/relation/@seriesid)", 
                               xpath);
    g_hash_table_insert (hash, "seriesid", g_strdup ((gchar *) object->stringval));
    xmlXPathFreeObject (object);

    xmlXPathFreeContext (xpath);
    xmlFreeDoc (omf_doc);

    return hash;
}

static void
create_toc_from_omf_cache_file (YelpTocPager *pager, const gchar *index_file)
{
    xmlXPathContextPtr xpath = NULL;
    xmlXPathObjectPtr objfiles = NULL;
    xmlDocPtr omfindex_xml = NULL;
    gint i;
    
    YelpTocPagerPriv *priv  = YELP_TOC_PAGER (pager)->priv;

    omfindex_xml = xmlReadFile (index_file, NULL, XML_PARSE_NOCDATA | 
		                XML_PARSE_NOERROR | XML_PARSE_NONET);

    if (omfindex_xml == NULL) {
	g_warning ("Unable to parse index file \"%s\"\n" 
		   "try deleting this file and running yelp again.\n", index_file);
	return; 
    }
    
    xpath = xmlXPathNewContext (omfindex_xml);
    
    objfiles = xmlXPathEvalExpression (BAD_CAST "/omfindex/dir/omffile", xpath);

    for (i=0; i < objfiles->nodesetval->nodeNr; i++) {
	YelpDocInfo *doc_info = NULL;
	YelpDocInfo *doc_old = NULL;
	xmlXPathObjectPtr objattr = NULL;
	xmlNodePtr node = NULL;
	xmlChar *attr = NULL;
	gchar *id = NULL;
	
	xpath->node = objfiles->nodesetval->nodeTab[i];
	
	objattr = xmlXPathEvalExpression (BAD_CAST "url", xpath);
	node = objattr->nodesetval->nodeTab[0];
	attr = xmlNodeListGetString (omfindex_xml, node->xmlChildrenNode, 1);
	xmlXPathFreeObject (objattr);

	if (!attr) {
	    g_warning ("missing required \"url\" element in omf cache file\n");
	    continue;
	}

	doc_info = yelp_doc_info_get ((const gchar *) attr, TRUE);
	xmlFree (attr);

	if (!doc_info)
	    return;
	
	objattr = xmlXPathEvalExpression (BAD_CAST "title", xpath);
	node = objattr->nodesetval->nodeTab[0];
	attr = xmlNodeListGetString (omfindex_xml, node->xmlChildrenNode, 1);
	yelp_doc_info_set_title (doc_info, (gchar *)attr);
	xmlXPathFreeObject (objattr);
	xmlFree (attr);

	objattr = xmlXPathEvalExpression (BAD_CAST "description", xpath);
	node = objattr->nodesetval->nodeTab[0];
	attr = xmlNodeListGetString (omfindex_xml, node->xmlChildrenNode, 1);
	yelp_doc_info_set_description (doc_info, (gchar *)attr);
	xmlXPathFreeObject (objattr);
	xmlFree (attr);

	objattr = xmlXPathEvalExpression (BAD_CAST "category", xpath);
	node = objattr->nodesetval->nodeTab[0];
	attr = xmlNodeListGetString (omfindex_xml, node->xmlChildrenNode, 1);
	yelp_doc_info_set_category (doc_info, (gchar *)attr);
	xmlXPathFreeObject (objattr);
	xmlFree (attr);

	objattr = xmlXPathEvalExpression (BAD_CAST "language", xpath);
	node = objattr->nodesetval->nodeTab[0];
	attr = xmlNodeListGetString (omfindex_xml, node->xmlChildrenNode, 1);
	yelp_doc_info_set_language (doc_info, (gchar *)attr);
	xmlXPathFreeObject (objattr);
	xmlFree (attr);

	objattr = xmlXPathEvalExpression (BAD_CAST "seriesid", xpath);
	node = objattr->nodesetval->nodeTab[0];
	attr = xmlNodeListGetString (omfindex_xml, node->xmlChildrenNode, 1);
	id = g_strconcat ("scrollkeeper.", (gchar *) attr, NULL);
	yelp_doc_info_set_id (doc_info, (gchar *)id);
	xmlXPathFreeObject (objattr);
	xmlFree (attr);

	doc_old = g_hash_table_lookup (priv->unique_hash, id);
	g_free (id);

	if (doc_old) {
	    if (yelp_doc_info_cmp_language (doc_info, doc_old) < 0) {
		toc_remove_doc_info (YELP_TOC_PAGER (pager), doc_old);
		toc_add_doc_info (YELP_TOC_PAGER (pager), doc_info);
	    }
	} else
	    toc_add_doc_info (YELP_TOC_PAGER (pager), doc_info);
	
    }

    xmlXPathFreeContext (xpath);
    xmlXPathFreeObject (objfiles);
    xmlFreeDoc (omfindex_xml);
	    
    return;
}

/** process_omf_pending *******************************************************/

static gboolean
process_omf_pending (YelpTocPager *pager)
{
    YelpTocPagerPriv *priv  = YELP_TOC_PAGER (pager)->priv;
    YelpDocInfo *doc_info;
    YelpDocInfo *doc_old;
    static gboolean first_call = TRUE;
    static gchar *index_file = NULL;
    static gchar *content_list = NULL;
    static gchar *sk_file = NULL;
    GHashTable *omf_hash = NULL;
    GSList  *filelist    = NULL;
    GSList  *firstfile   = NULL;
    GSList  *first = NULL;
    gchar   *dir   = NULL;
    gchar   *file  = NULL;
    gchar   *lang  = NULL;
    gchar   *stderr_str = NULL;
    gchar   *command    = NULL;
    FILE    *newindex   = NULL;

    if (!index_file)
	index_file = g_build_filename (yelp_dot_dir(), "omfindex.xml", NULL);
    
    /* on first call to this function, we check for the existence of a
     * cache file.  If it exists, we get our information from there.  If 
     * it doesn't exist, then we create a list of omf files to process
     * with the process_read_scrollkeeper() function */
    if (first_call) {
	first_call = FALSE;

	sk_file = g_build_filename (yelp_dot_dir(), "sk-content-list.last", NULL);
	    
	/* get current language */
	const gchar * const * langs = g_get_language_names ();
	if (langs && langs[0])
	    lang = (gchar *) langs[0];
	else
	    lang = "C";
	    
	command = g_strconcat ("scrollkeeper-get-content-list ", lang, NULL);

	if (!g_spawn_command_line_sync (command, &content_list, 
	                                &stderr_str, NULL, NULL)) {
	    g_critical ("scrollkeeper-get-content-list failed: %s", stderr_str);
	    return FALSE;
	}

	/* required to remove LFs and extra whitespace from the filename */
	g_strstrip (content_list);
	
	g_free (stderr_str);
	g_free (command);
	
	if (g_file_test (index_file, G_FILE_TEST_IS_REGULAR) &&
	    g_file_test (sk_file, G_FILE_TEST_IS_REGULAR)) {
	    /* the presence of a cache file is not enough.  The cache file
	     * could be outdated since the scrollkeeper database could have
	     * been rebuilt since the last time the cache file was created.
	     * We solve this by copying the scrollkeeper content list file
	     * to ~/.gnome/yelp.d/sk-content-list.last when the
	     * cache file is originally created.  On subsequent runs, we then
	     * do a byte by byte comparison between the generated content 
	     * list and the old one at ~/.gnome2/yelp.d/ - If they are exactly
	     * the same, then we use the cache file; otherwise the cache file
	     * will get rebuilt. Of course this operates under the assumption
	     * that the contents list generated by scrollkeeper is always the
	     * same if the scrollkeeper database has not been rebuilt.  If that 
	     * ever fails to be the case then we end up rebuilding the cache
	     * file everytime, so there's no regression of functionality */
	    
	    if (!files_are_equivalent (sk_file, content_list)) {
	    	process_read_scrollkeeper (pager, content_list);
		return TRUE;
	    }

	    create_toc_from_omf_cache_file (pager, index_file);
	    
	    /* done processing, don't call this function again */
	    return FALSE;
	}
	    
	process_read_scrollkeeper (pager, content_list);
	
	/* return true, so that this function is called again to continue 
	 * processing */
	return TRUE;
    } 

    /* if we've made it to here, that means we need to regenerate the cache
     * file, so allocate an in memory XML document here */
    if (!priv->omfindex_xml) {
        priv->omfindex_xml = xmlNewDoc (BAD_CAST "1.0");
        priv->omf_root = xmlNewNode (NULL, BAD_CAST "omfindex");

        xmlDocSetRootElement (priv->omfindex_xml, priv->omf_root);

        xmlAddChild (priv->omf_root, xmlNewText (BAD_CAST "\n  "));
    }
    
    /* get the "first" directory in the GSList "omf_pending" */
    first = priv->omf_pending;
    
    /* if this is NULL, then we are done processing */
    /* FIXME: cleanup any leftover stuff here */
    if (!first) {
	GnomeVFSURI *fromfile = gnome_vfs_uri_new (content_list);
	GnomeVFSURI *tofile = gnome_vfs_uri_new (sk_file);
	gint result = 0;

	/* create the new cache file */
	if (!(newindex = g_fopen (index_file, "w")))
	    g_warning ("Unable to create '%s'\n", index_file);
	else {
	    xmlDocDump (newindex, priv->omfindex_xml);
	    fclose (newindex);
	}

	/* copy the newly created file to ~/.gnome2/yelp.d/omfindex.xml,
	 * overwriting the old one if necessary */
	result = gnome_vfs_xfer_uri (fromfile, tofile, 
	                             GNOME_VFS_XFER_DEFAULT,
			             GNOME_VFS_XFER_ERROR_MODE_ABORT,
			             GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
			             NULL, NULL);

	gnome_vfs_uri_unref (fromfile);
	gnome_vfs_uri_unref (tofile);

	if (result != GNOME_VFS_OK) 
	    g_critical ("Unable to copy %s to %s\n", content_list, sk_file);

	if (priv->omf_dirhash)
	    g_hash_table_destroy (priv->omf_dirhash);

	xmlFreeDoc (priv->omfindex_xml);

	return FALSE;
    }

    /* remove this "first" directory from our list */
    priv->omf_pending = g_slist_remove_link (priv->omf_pending, first);
    dir = (gchar *) first->data;

    priv->omf_ins = xmlNewChild (priv->omf_root, NULL, BAD_CAST "dir", NULL);
    xmlAddChild (priv->omf_ins, xmlNewText (BAD_CAST "\n    "));
    xmlNewChild (priv->omf_ins, NULL, BAD_CAST "name", BAD_CAST dir);
    xmlAddChild (priv->omf_ins, xmlNewText (BAD_CAST "\n    "));
    
    filelist = g_hash_table_lookup (priv->omf_dirhash, dir);

    while (filelist && filelist->data) {
	gchar *id = NULL;
	gchar *url = NULL;

	firstfile = filelist;
	filelist = g_slist_remove_link (filelist, firstfile);

	file = (gchar *) firstfile->data;

	if (!file || !g_str_has_suffix (file, ".omf"))
	    goto done;

	omf_hash = get_omf_attributes (pager, file);

	if (!omf_hash)
	    goto done;

	/* url is required, if it's not present don't even add the
	 * entry to the cache file */
	url = g_hash_table_lookup (omf_hash, "url");
	if (url == NULL || *url == '\0')
	    goto done;

	/* this add all the omf information to the xml cache file */
	priv->omf_ins = xmlNewChild (priv->omf_ins, NULL, 
			             BAD_CAST "omffile", NULL);
	xmlAddChild (priv->omf_ins, xmlNewText (BAD_CAST "\n      "));
	xmlNewChild (priv->omf_ins, NULL, BAD_CAST "path", BAD_CAST file);
	xmlAddChild (priv->omf_ins, xmlNewText (BAD_CAST "\n      "));
	xmlNewChild (priv->omf_ins, NULL, BAD_CAST "url", 
	             BAD_CAST g_hash_table_lookup (omf_hash, "url"));
	xmlAddChild (priv->omf_ins, xmlNewText (BAD_CAST "\n      "));
	xmlNewChild (priv->omf_ins, NULL, BAD_CAST "title", 
	             BAD_CAST g_hash_table_lookup (omf_hash, "title"));
	xmlAddChild (priv->omf_ins, xmlNewText (BAD_CAST "\n      "));
	xmlNewChild (priv->omf_ins, NULL, BAD_CAST "language", 
	             BAD_CAST g_hash_table_lookup (omf_hash, "language"));
	xmlAddChild (priv->omf_ins, xmlNewText (BAD_CAST "\n      "));
	xmlNewChild (priv->omf_ins, NULL, BAD_CAST "category", 
	             BAD_CAST g_hash_table_lookup (omf_hash, "category"));
	xmlAddChild (priv->omf_ins, xmlNewText (BAD_CAST "\n      "));
	xmlNewChild (priv->omf_ins, NULL, BAD_CAST "description", 
	             BAD_CAST g_hash_table_lookup (omf_hash, "description"));
	xmlAddChild (priv->omf_ins, xmlNewText (BAD_CAST "\n      "));
	xmlNewChild (priv->omf_ins, NULL, BAD_CAST "seriesid", 
	             BAD_CAST g_hash_table_lookup (omf_hash, "seriesid"));
	xmlAddChild (priv->omf_ins, xmlNewText (BAD_CAST "\n    "));
	priv->omf_ins = priv->omf_ins->parent;

	doc_info = 
	    yelp_doc_info_get ((const gchar *) g_hash_table_lookup (omf_hash, "url"), 
	                       FALSE);

	if (!doc_info)
	    goto done;

	yelp_doc_info_set_title       (doc_info, g_hash_table_lookup (omf_hash, "title"));
	yelp_doc_info_set_language    (doc_info, g_hash_table_lookup (omf_hash, "language"));
	yelp_doc_info_set_category    (doc_info, g_hash_table_lookup (omf_hash, "category"));
	yelp_doc_info_set_description (doc_info, g_hash_table_lookup (omf_hash, "description"));

	id = g_strconcat ("scrollkeeper.", 
	                  (gchar *) g_hash_table_lookup (omf_hash, "seriesid"), 
			  NULL);

	yelp_doc_info_set_id (doc_info, id);

	doc_old = g_hash_table_lookup (priv->unique_hash, id);

	if (doc_old) {
	    if (yelp_doc_info_cmp_language (doc_info, doc_old) < 0) {
		toc_remove_doc_info (YELP_TOC_PAGER (pager), doc_old);
		toc_add_doc_info (YELP_TOC_PAGER (pager), doc_info);
	    }
	} else
	    toc_add_doc_info (YELP_TOC_PAGER (pager), doc_info);

done:
	if (omf_hash)
	    g_hash_table_destroy (omf_hash);
	g_free (file);
	g_slist_free_1 (firstfile);
	g_free (id);
	
    } /* end while */

    xmlAddChild (priv->omf_ins, xmlNewText (BAD_CAST "\n  "));

    g_free (dir);
    g_slist_free_1 (first);

    /* continue processing */
    return TRUE;
}

#ifdef ENABLE_MAN
static void
add_man_page_to_toc (YelpTocPager *pager, gchar *dirname, gchar *filename)
{
    xmlNodePtr tmp = NULL;
    gchar *manname = NULL;
    gchar *mansect = NULL;
    gchar *manman = NULL;
    gchar *c1 = NULL;
    gchar *c2 = NULL;
    
    YelpTocPagerPriv *priv  = YELP_TOC_PAGER (pager)->priv;
		    
    c1 = g_strrstr (filename, ".bz2");
  
    if (c1 && strlen (c1) != 4)
	c1 = NULL;

    if (!c1) {
	c1 = g_strrstr (filename, ".gz");
	if (c1 && strlen (c1) != 3)
	    c1 = NULL;
    }

    /* c1 points to the file extension, either .gz or .bz2 at this point 
     * or is NULL if the filename does not have either extension */
    if (c1)
	c2 = g_strrstr_len (filename, c1 - filename, ".");
    else
	c2 = g_strrstr (filename, ".");

    /* c2 points to the period preceding the man page section in the 
     * filename at this point */
    if (c2) {
	manname = g_strndup (filename, c2 - filename);
	if (c1)
	    mansect = g_strndup (c2 + 1, c1 - c2 - 1);
	else
	    mansect = g_strdup (c2 + 1);
    }

    /* if filename has no period in it, we have no idea what man
     * section to place it in.  So just ignore it. */
    else return;

    manman = g_strconcat (manname, ".", mansect, NULL);
    
    if (g_hash_table_lookup (priv->man_manhash, manman) == NULL) {
	tmp = g_hash_table_lookup (priv->man_secthash, mansect);

	if (tmp == NULL && strlen (mansect) > 1) {
	    gchar *mansect0 = g_strndup (mansect, 1);
	    tmp = g_hash_table_lookup (priv->man_secthash, mansect0);
	    g_free (mansect0);
	}

	if (tmp) {
	    gchar *tooltip, *url_full, *url_short;
	    YelpDocInfo *info;

	    url_full = g_strconcat ("man:", dirname, "/", filename, NULL);
	    url_short = g_strconcat ("man:", manname, ".", mansect, NULL);
	    info = yelp_doc_info_get (url_full, TRUE);

	    if (info) {
		gchar *lang = NULL;

		if (priv->man_dirlang)
		    lang = g_hash_table_lookup (priv->man_dirlang, dirname);

		if (lang)
		    yelp_doc_info_set_language (info, lang);

		yelp_doc_info_add_uri (info, url_short, YELP_URI_TYPE_MAN);
		tmp = xmlNewChild (tmp, NULL, BAD_CAST "doc", NULL);
		xmlNewProp (tmp, BAD_CAST "href", BAD_CAST url_full);
		xmlNewChild (tmp, NULL, BAD_CAST "title", BAD_CAST manname);
		tooltip = g_strdup_printf (_("Read man page for %s"), manname);
		xmlNewChild (tmp, NULL, BAD_CAST "tooltip", BAD_CAST tooltip);

		g_free (tooltip);
	    }

	    g_free (url_full);
	    g_free (url_short);
	} else {
	    d (g_warning ("Could not locate section %s for %s\n", mansect, manman));
	}

	g_hash_table_insert (priv->man_manhash, g_strdup (manman), priv);
    }

    g_free (manname);
    g_free (mansect);
    g_free (manman);
}

static void
create_manindex_file (gchar *index_file, xmlDocPtr xmldoc)
{
    FILE *newindex = NULL;

    /* check to see if the file already exists, if so rename it */
    /* I only enable this for debugging */
#if 0
    if (g_file_test (index_file, G_FILE_TEST_EXISTS)) {
	struct stat buff;
	gchar *backup_file = NULL;

	if (g_stat (index_file, &buff) < 0)
	    g_warning ("Unable to stat file \"%s\"\n", index_file);

	backup_file = g_strdup_printf ("%s.%d", index_file, (int) buff.st_mtime);
	
	if (g_rename (index_file, backup_file) < 0)
	    g_warning ("Unable to rename \"%s\" to \"%s\"\n", index_file, backup_file);
	
	g_free (backup_file);
    }
#endif
		
    if (!(newindex = g_fopen (index_file, "w")))
	g_warning ("Unable to create '%s'\n", index_file);
    else {
	xmlDocDump (newindex, xmldoc);
	fclose (newindex);
    }
}

/* returns 0 on error, 1 otherwise */
static int
create_toc_from_index (YelpTocPager *pager, gchar *index_file)
{
    const gchar * const * langs = g_get_language_names ();
    xmlXPathContextPtr xpath = NULL;
    xmlXPathObjectPtr objsect = NULL;
    xmlDocPtr manindex_xml = NULL;
    xmlNodePtr root = NULL;
    xmlChar *language = NULL;
    gint update_flag = 0;
    gint i, j;
    
    YelpTocPagerPriv *priv  = YELP_TOC_PAGER (pager)->priv;

    manindex_xml = xmlReadFile (index_file, NULL, XML_PARSE_NOCDATA | 
		                XML_PARSE_NOERROR | XML_PARSE_NONET);

    if (manindex_xml == NULL) {
	g_warning ("Unable to parse index file \"%s\"\n", index_file);
	return 0;
    }
    
    /* check the language that this index file was generated for */
    root = xmlDocGetRootElement (manindex_xml);
    language = xmlGetProp (root, BAD_CAST "lang");
    
    /* if the language is not the same as the current language (specified
     * by the LANGUAGE environment variable) then return so that the index 
     * is recreated */
    if (language == NULL || !g_str_equal (BAD_CAST language, langs[0])) {
	debug_print (DB_INFO, 
	             "LANGUAGE and index file language do not match, "
		     "index file will be recreated.\n");
	xmlFreeDoc (manindex_xml);
	return 0;
    }

    xmlFree (language);
    
    xpath = xmlXPathNewContext (manindex_xml);
    objsect = xmlXPathEvalExpression (BAD_CAST "/manindex/mansect", xpath);

    for (i=0; i < objsect->nodesetval->nodeNr; i++) {
	xmlXPathObjectPtr objdirs;

	if (!priv->man_manhash)
	    priv->man_manhash = g_hash_table_new_full (g_str_hash, g_str_equal,
		                                       g_free, NULL);

	xpath->node = objsect->nodesetval->nodeTab[i];
	objdirs = xmlXPathEvalExpression (BAD_CAST "dir", xpath);

	for (j=0; j < objdirs->nodesetval->nodeNr; j++) {
	    xmlNodePtr dirnode = objdirs->nodesetval->nodeTab[j];
	    xmlNodePtr node = NULL;
	    xmlChar *dirname = NULL;
	    xmlChar *dirmtime = NULL;
	    xmlChar *lang = NULL;
	    time_t mtime;
	    struct stat buf;
	    
            lang     = xmlGetProp (dirnode, BAD_CAST "lang");
	    dirmtime = xmlGetProp (dirnode, BAD_CAST "mtime");

	    if (lang == NULL)
		lang = xmlStrdup (BAD_CAST "C");

	    for (node = dirnode->children; node != NULL; node = node->next) {
		if (node->type == XML_ELEMENT_NODE && 
		    g_str_equal ((gchar *)node->name, "name")) {
		    dirname = xmlNodeGetContent (node);
		}
	    }

	    g_hash_table_insert (priv->man_dirlang, g_strdup ((gchar *)dirname), 
	                         g_strdup ((gchar *)lang));
	    
	    /* if we can't stat the dirname for some reason, then skip adding
	     * this directory to the TOC, remove the dirnode, and set the flag
	     * to rewrite the cache file */
	    if (g_stat ((gchar *)dirname, &buf) < 0) {
		g_warning ("Unable to stat dir \"%s\", removing from cache file.\n", dirname);
		xmlUnlinkNode (dirnode);
		xmlFreeNode (dirnode);
		update_flag = 1;
	    	continue;
	    }

	    /* FIXME: need some error checking */
	    mtime = (time_t) atoi ((gchar *)dirmtime);

	    /* see if directory mtime has changed - if so recreate
	     * the directory node */
	    if (buf.st_mtime > mtime) {
		GDir *dir;
		xmlNodePtr newNode = NULL;
		gchar mtime_str[20];
		gchar *filename = NULL;
		
		/* this means we will rewrite the cache file at the end */
		update_flag = 1;
		
		if ((dir = g_dir_open ((gchar *)dirname, 0, NULL))) {
		    g_snprintf (mtime_str, 20, "%u", (guint) buf.st_mtime);

		    newNode = xmlNewNode (NULL, BAD_CAST "dir");
		    xmlNewProp (newNode, BAD_CAST "mtime", BAD_CAST mtime_str);
		    xmlAddChild (newNode, xmlNewText (BAD_CAST "\n      "));
		    xmlNewChild (newNode, NULL, BAD_CAST "name", dirname);
		    xmlAddChild (newNode, xmlNewText (BAD_CAST "\n      "));

		    while ((filename = (gchar *) g_dir_read_name (dir))) {

			xmlNewChild (newNode, NULL, BAD_CAST "page", BAD_CAST filename);
			xmlAddChild (newNode, xmlNewText (BAD_CAST "\n      "));

			add_man_page_to_toc (pager, (gchar *)dirname, filename);
			priv->manpage_count++;
		    }

		    g_dir_close (dir);
		}

		/* we replace the node in the tree */
		xmlReplaceNode (dirnode, newNode);
	    
	    /* otherwise just read from the index file */
	    } else {
	    
		for (node = dirnode->children; node != NULL; node = node->next) {
		    xmlChar *manpage = NULL;

		    if (node->type == XML_ELEMENT_NODE && 
		        g_str_equal ((gchar *)node->name, "page")) {
			manpage = xmlNodeGetContent (node);
			add_man_page_to_toc (pager, (gchar *)dirname, (gchar *)manpage);
			priv->manpage_count++;
			xmlFree (manpage);
		    }
		}
	    
	    }
	    xmlFree (lang);
	    xmlFree (dirmtime);
	    xmlFree (dirname);
	}

	/* cleanup */
	xmlXPathFreeObject (objdirs);

	if (priv->man_manhash) {
	    g_hash_table_destroy (priv->man_manhash);
	    priv->man_manhash = NULL;
	}
    }

    if (update_flag) {
	create_manindex_file (index_file, manindex_xml);
    }

    xmlXPathFreeContext (xpath);
    xmlXPathFreeObject (objsect);
    xmlFreeDoc (manindex_xml);
    
    return 1;
}

static gboolean
process_mandir_pending (YelpTocPager *pager)
{
    static gchar *index_file = NULL;
    gchar *filename = NULL;
    gchar *dirname = NULL;
    GDir  *dir = NULL; 
    gint i, j, k;

    YelpTocPagerPriv *priv  = YELP_TOC_PAGER (pager)->priv;

    if (!priv->man_doc) {
	xmlXPathContextPtr xpath;
	xmlXPathObjectPtr  obj;

	/* NOTE: this document is free()'d at the end of the process_xslt function */
	priv->man_doc = xmlCtxtReadFile (priv->parser, DATADIR "/yelp/man.xml", NULL,
					 XML_PARSE_NOBLANKS | XML_PARSE_NOCDATA  |
					 XML_PARSE_NOENT    | XML_PARSE_NOERROR  |
					 XML_PARSE_NONET    );
	priv->man_secthash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	xpath = xmlXPathNewContext (priv->man_doc);
	obj = xmlXPathEvalExpression (BAD_CAST "//toc", xpath);

	for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	    xmlNodePtr node = obj->nodesetval->nodeTab[i];
	    xmlChar *sect = xmlGetProp (node, BAD_CAST "sect");

	    if (sect) {
		gchar **sects = g_strsplit ((gchar *)sect, " ", 0);

		for (j = 0; sects[j] != NULL; j++)
		    g_hash_table_insert (priv->man_secthash, 
					 g_strdup (sects[j]), node);
		g_strfreev (sects);
	    }
	    xmlFree (sect);
	    xml_trim_titles (node, BAD_CAST "title");
	    xml_trim_titles (node, BAD_CAST "description");
	}
	xmlXPathFreeObject (obj);
	xmlXPathFreeContext (xpath);
    }

    if (!index_file)
	index_file = g_build_filename (yelp_dot_dir(), "manindex.xml", NULL);
  
    /* On first call to this function, create a list of directories that we 
     * need to process: we actually make a linked list (mandir_list) whose 
     * members who are linked lists containing directories for a particular 
     * man directory such as "man1", "man2", etc..  We do this because we 
     * need a separate hash for every mandir */
    if (!priv->mandir_list) {
	const gchar * const * langs = g_get_language_names ();
	gchar *manpath = NULL;
	gchar **manpaths = NULL;
	gchar **mandirs = NULL;

	if (!priv->man_dirlang)
	    priv->man_dirlang = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                               g_free,     g_free);

	/* check for the existence of the xml cache file in ~/.gnome2/yelp.d/
	 * if it exists, use it as the source for man pages instead of 
	 * searching the hard disk for them - should make startup much faster */
	if (g_file_test (index_file, G_FILE_TEST_IS_REGULAR) &&
	    create_toc_from_index (pager, index_file)) {
	    xmlNodePtr mynode;

            mynode = xmlCopyNode (xmlDocGetRootElement (priv->man_doc), 1);
	    xmlAddChild (xmlDocGetRootElement(priv->toc_doc), mynode);

	    xmlFreeDoc (priv->man_doc);

	    /* we are done processing, return FALSE so we don't call this 
	     * function again. */
	    return FALSE;
	}
	
	mandirs = yelp_get_man_paths ();
	priv->manindex_xml = xmlNewDoc (BAD_CAST "1.0");
	priv->root = xmlNewNode (NULL, BAD_CAST "manindex");
	xmlNewProp (priv->root, BAD_CAST "lang", BAD_CAST langs[0]);
	priv->ins  = priv->root;

	xmlDocSetRootElement (priv->manindex_xml, priv->root);

	xmlAddChild (priv->root, xmlNewText (BAD_CAST "\n  "));

	if (!g_spawn_command_line_sync ("manpath", &manpath, NULL, NULL, NULL))
	    manpath = g_strdup (g_getenv ("MANPATH"));

	if (!manpath) {
	    manpath = g_strdup ("/usr/share/man");
	}

	g_strstrip (manpath);
	manpaths = g_strsplit (manpath, G_SEARCHPATH_SEPARATOR_S, -1);
	g_free (manpath);

	/* order is important here, since we may encounter collisions when adding 
	 * a man page to the hash table "man_manhash".
	 *
	 * We want the resulting list to be sorted in order of priority:
	 * 1) Highest priority is given to the base directory name returned 
	 *    from $MANPATH or the manpath program.  So if that is 
	 *    /usr/local/share:/usr/share then the man page 
	 *    /usr/local/share/man/man1/python.1.gz would have precedence over
	 *    /usr/share/man/man1/python.1.gz 
	 * 2) Next highest priority is given to the language.
	 * 3) Lowest priority is given to the section name, i.e. "man0p" "man1"
	 *    since a man page from one section should never conflict with a man page
	 *    from another section (because of the .0p or .1 section in the filename)
	 *
	 * Here is an example of how it should be sorted when 
	 *   $MANPATH=/usr/local/share:/usr/share and LANGUAGE="es"
	 *   
	 * Adding /usr/local/share/man/es/man0p
	 * Adding /usr/share/man/es/man0p
	 * Adding /usr/local/share/man/man0p
	 * Adding /usr/share/man/man0p
	 * Adding /usr/local/share/man/es/man1
	 * Adding /usr/share/man/es/man1
	 * Adding /usr/local/share/man/man1
	 * Adding /usr/share/man/man1 
	 */
	
	for (i=0; mandirs[i] != NULL; i++) {
	    GSList *tmplist = NULL;

	    for (j=0; langs[j] != NULL; j++) {
		for (k=0; manpaths[k] != NULL; k++) { 
		    if (g_str_equal (langs[j], "C"))
			dirname = g_build_filename (manpaths[k], mandirs[i], NULL);
		    else
			dirname = g_build_filename (manpaths[k], langs[j], 
			                            mandirs[i],
			                            NULL);

		    g_hash_table_insert (priv->man_dirlang, g_strdup (dirname),
		                         g_strdup (langs[j]));

		    /* prepend to list for speed, reverse it at the end */
		    tmplist = g_slist_prepend (tmplist, dirname);
		}
	    }
	    tmplist = g_slist_reverse (tmplist);

	    priv->mandir_list = g_slist_prepend (priv->mandir_list, tmplist);
	}
	priv->mandir_list = g_slist_reverse (priv->mandir_list);

#ifdef YELP_DEBUG
	/* debugging: print out lists in order */
	GSList *list1 = priv->mandir_list;
	GSList *list2 = NULL;

	while (list1 != NULL) {
	    list2 = list1->data;
	    while (list2 && list2->data) {
	        debug_print (DB_INFO, "Dir=%s\n", (gchar *)list2->data);
	        list2 = g_slist_next (list2);
	    }
	    list1 = g_slist_next (list1);
	}
#endif

	g_strfreev (manpaths);
 
	priv->mandir_ptr = priv->mandir_list;
	if (priv->mandir_list && priv->mandir_list->data) {
	    priv->mandir_langpath = priv->mandir_list->data;
	}

	/* return TRUE, so that this function is called again.*/
	return TRUE;
    }
    
    /* iterate through our previously created linked lists and create the
     * table of contents from them */
    if (priv->mandir_langpath && priv->mandir_langpath->data) {
	dirname = priv->mandir_langpath->data;

	if ((dir = g_dir_open (dirname, 0, NULL))) {
	    struct stat buf;
	    gchar mtime_str[20];
	    gchar *lang = NULL;

	    if (g_stat (dirname, &buf) < 0)
		debug_print (DB_WARN, "Unable to stat dir: \"%s\"\n", dirname);

	    /* check to see if we need create a new hash table */
	    if (!priv->man_manhash) {
		priv->man_manhash = 
		    g_hash_table_new_full (g_str_hash, g_str_equal,
	                                   g_free,     NULL);

		priv->ins = xmlNewChild (priv->root, NULL, BAD_CAST "mansect", NULL);
		xmlAddChild (priv->ins, xmlNewText (BAD_CAST "\n    "));			    
	    }

	    g_snprintf (mtime_str, 20, "%u", (guint) buf.st_mtime);

	    lang = g_hash_table_lookup (priv->man_dirlang, dirname);

	    priv->ins = xmlNewChild (priv->ins, NULL, BAD_CAST "dir", NULL);

	    if (lang == NULL) {
		g_print ("dir %s is null\n", dirname);
		lang = "C";
	    }
	    
	    xmlNewProp (priv->ins, BAD_CAST "lang", BAD_CAST lang);

	    xmlNewProp (priv->ins, BAD_CAST "mtime", BAD_CAST mtime_str);
	    xmlAddChild (priv->ins, xmlNewText (BAD_CAST "\n      "));
	    xmlNewChild (priv->ins, NULL, BAD_CAST "name", BAD_CAST dirname);
	    xmlAddChild (priv->ins, xmlNewText (BAD_CAST "\n      "));
	    xmlAddChild (priv->ins->parent, xmlNewText (BAD_CAST "\n    "));

	    while ((filename = (gchar *) g_dir_read_name (dir))) {

		xmlNewChild (priv->ins, NULL, BAD_CAST "page", BAD_CAST filename);
		xmlAddChild (priv->ins, xmlNewText (BAD_CAST "\n      "));

		add_man_page_to_toc (pager, dirname, filename);
		priv->manpage_count++;
	    }

	    priv->ins = priv->ins->parent;

	    g_dir_close (dir);
	}

	priv->mandir_langpath = g_slist_next (priv->mandir_langpath);

    } else {
	priv->mandir_ptr = g_slist_next (priv->mandir_ptr);

	if (priv->mandir_ptr && priv->mandir_ptr->data) {
	    priv->mandir_langpath = priv->mandir_ptr->data;

	    if (priv->man_manhash) {
		g_hash_table_destroy (priv->man_manhash);
		priv->man_manhash = NULL;
	    }
	} else {   /* no more entries to prcoess, write file & cleanup */
	    GSList *listptr = priv->mandir_list;
	    xmlNodePtr copynode;

	    while (listptr && listptr->data)  {
		GSList *langptr = listptr->data;

		while (langptr && langptr->data) {
		    g_free (langptr->data);
		    langptr = g_slist_next (langptr);
		}
		
		g_slist_free (listptr->data);
		   
		listptr = g_slist_next (listptr);
	    }

	    g_slist_free (priv->mandir_list);

	    create_manindex_file (index_file, priv->manindex_xml);

	    xmlFree (priv->manindex_xml);

            copynode = xmlCopyNode (xmlDocGetRootElement (priv->man_doc), 1);
	    xmlAddChild (xmlDocGetRootElement(priv->toc_doc), copynode);

	    xmlFreeDoc (priv->man_doc);

	    /* done processing */
	    return FALSE;
	}
    }

    return TRUE;
}
#endif // ENABLE_MAN

#ifdef ENABLE_INFO
static gboolean
process_info_pending (YelpTocPager *pager)
{
    gchar ** info_paths = yelp_get_info_paths ();
    int i = 0;
    gchar *filename = NULL;
    xmlNodePtr mynode = NULL;

    GHashTable *info_parsed = g_hash_table_new_full (g_str_hash, g_str_equal,
						     g_free, g_free);
    GHashTable *categories = g_hash_table_new_full (g_str_hash, g_str_equal,
						    g_free, NULL);
    
    YelpTocPagerPriv *priv = YELP_TOC_PAGER (pager)->priv;
    xmlNodePtr node = NULL;

    for (i=0; info_paths[i]; i++) {
	filename = g_strconcat (info_paths[i], "/dir", NULL);
	
	/* Search out the directory listing for info and follow that */
	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
	    g_free (filename);
	    continue;
	} else {
	    GIOChannel *channel;
	    gchar *str = NULL;
	    gsize len;
	    gchar ** files;
	    gchar ** ptr;
	    xmlNodePtr tmp;
	    xmlNodePtr new_node = NULL;
	    gboolean menufound = FALSE;

	    if (!priv->info_doc) {
		xmlXPathContextPtr xpath;
		xmlXPathObjectPtr  obj;
		priv->info_doc = xmlCtxtReadFile (priv->parser, DATADIR "/yelp/info.xml", NULL,
						 XML_PARSE_NOBLANKS | XML_PARSE_NOCDATA  |
						 XML_PARSE_NOENT    | XML_PARSE_NOERROR  |
						 XML_PARSE_NONET    );
		
		xpath = xmlXPathNewContext (priv->info_doc);
		obj = xmlXPathEvalExpression (BAD_CAST "//toc", xpath);
		node = obj->nodesetval->nodeTab[0];
		for (i=0; i < obj->nodesetval->nodeNr; i++) {
		    xmlNodePtr tmpnode = obj->nodesetval->nodeTab[i];
		    xml_trim_titles (tmpnode, BAD_CAST "title");
		    xml_trim_titles (tmpnode, BAD_CAST "description");
		}
		xmlXPathFreeObject (obj);
		xmlXPathFreeContext (xpath);
	    }
	    
	    channel = yelp_io_channel_new_file (filename, NULL);
	    g_io_channel_read_to_end (channel, &str, &len, NULL);
	    g_io_channel_shutdown (channel, FALSE, NULL);
	    g_io_channel_unref (channel);

	    files = g_strsplit (str, "\n", -1);
	    
	    for (ptr = files; *ptr != NULL; ptr++) {
		if (!menufound) {
		    if (g_str_has_prefix (*ptr, "* Menu:")) {
			menufound = TRUE;
		    }
		} else {
		    if (g_str_has_prefix (*ptr, "*")) {
			/* A menu item */
			gchar *tooltip = NULL;
			gchar *fname = NULL;
			gchar *desc = NULL;
			gchar *part1 = NULL, *part2 = NULL, *part3 = NULL;
			gchar *path = *ptr;
			gchar **nextline = ptr;
			gchar *p = NULL;
			nextline++;
			/* Due to braindead info stuff, we gotta manually
			 * split everything up...
			 */
			path++; 
			if (!path)
			    goto done;
			part1 = strchr (path, ':');
			if (!part1)
			    goto done;
			part2 = g_strndup (path, part1-path);
			p = g_strdup (part2);
			p = g_strstrip (p);
			tooltip = g_strdup_printf (_("Read info page for %s"), p);
			g_free (p);
			path = part1+1;
			part1 = strchr (path, ')');
			if (!part1)
			    goto done;
			part1 = strchr (part1, '.');
			if (!part1)
			    goto done;
			part3 = g_strndup (path, part1-path);
			part1++;
			desc = g_strdup (part1);
			g_strstrip (desc);
			
			fname = g_strconcat ("info:", g_strstrip(part3), NULL);
			
			if (!g_hash_table_lookup (info_parsed, fname)) {
			    g_hash_table_insert (info_parsed, 
						 g_strdup (fname),
						 g_strdup (part1));
			    
			    while (*nextline &&
				   g_ascii_isspace (**nextline)) {
				gchar *newbit = g_strdup (*nextline);
				gchar *olddesc = desc;
				g_strstrip (newbit);
				desc = g_strconcat (desc, " ",newbit, NULL);
				nextline++;
				g_free (newbit);
				g_free (olddesc);
			    }
			    ptr = --nextline;
			    
			    tmp = xmlNewChild (new_node, NULL, BAD_CAST "doc",
					       NULL);
			    xmlNewNsProp (tmp, NULL, BAD_CAST "href", BAD_CAST fname);
			    
			    xmlNewTextChild (tmp, NULL, BAD_CAST "title",
					 BAD_CAST part2);
			    xmlNewTextChild (tmp, NULL, BAD_CAST "tooltip",
					 BAD_CAST tooltip);
			    
			    xmlNewTextChild (tmp, NULL, BAD_CAST "description",
					 BAD_CAST desc);
			}
		    done:
			g_free (part2);
			g_free (part3);
			g_free (tooltip);
			g_free (desc);
			g_free (fname);
		    } else if (!g_ascii_isspace (**ptr) && g_ascii_isprint(**ptr)){
			new_node = g_hash_table_lookup (categories, *ptr);
			if (!new_node) {
			    gchar *tmp;

			    /* A new section */
			    static int sectno = 1;
			    new_node = xmlNewChild (node, NULL, BAD_CAST "toc",
						    NULL);
			    tmp = g_strdup_printf ("%d", sectno);
			    xmlNewNsProp (new_node, NULL, BAD_CAST "sect",
					  BAD_CAST tmp);
			    g_free (tmp);
			    tmp = g_strdup_printf ("infosect%d", sectno);
			    xmlNewNsProp (new_node, NULL, BAD_CAST "id",
					  BAD_CAST tmp);
			    g_free (tmp);
			    sectno++;
			    xmlNewTextChild (new_node, NULL, BAD_CAST "title",
					     BAD_CAST *ptr);
			    g_hash_table_insert (categories, 
						 g_strdup (*ptr),
						 new_node);
			}
			
		    }
		}
	    }
	    g_free (filename);
	    g_free (str);
	    g_strfreev (files);
	}
    }
    g_hash_table_destroy (info_parsed);
    g_hash_table_destroy (categories);
    
    mynode = xmlCopyNode (xmlDocGetRootElement (priv->info_doc), 1);
    xmlAddChild (xmlDocGetRootElement (priv->toc_doc), mynode);
    
    xmlFreeDoc (priv->info_doc);
    
    return FALSE;
}
#endif /* ENABLE_INFO */

static int
spoon_add_document (void *reg, void * user_data)
{
    xmlNodePtr node = (xmlNodePtr) user_data;
    SpoonReg *r = (SpoonReg *) reg;
    xmlNodePtr new;
    gchar tmp[10];

    new = xmlNewChild (node, NULL, BAD_CAST "doc", NULL);
    xmlNewNsProp (new, NULL, BAD_CAST "href", BAD_CAST r->uri);
    xmlNewTextChild (new, NULL, BAD_CAST "title", BAD_CAST r->name);
    xmlNewTextChild (new, NULL, BAD_CAST "description", BAD_CAST r->comment);
    g_sprintf (&tmp, "%d", r->weight);
    xmlNewNsProp (new, NULL, BAD_CAST "weight", BAD_CAST tmp);
    return FALSE;
}

static gboolean
process_read_menu (YelpTocPager *pager)
{
    xmlTextReaderPtr   reader;
    xmlXPathContextPtr xpath;
    xmlXPathObjectPtr  obj;
    GError  *error = NULL;
    gint i, ret;

    YelpTocPagerPriv *priv = pager->priv;

    priv->toc_doc = xmlCtxtReadFile (priv->parser, DATADIR "/yelp/toc.xml", NULL,
				     XML_PARSE_NOBLANKS | XML_PARSE_NOCDATA  |
				     XML_PARSE_NOENT    | XML_PARSE_NOERROR  |
				     XML_PARSE_NONET    );
    if (!priv->toc_doc) {
	g_set_error (&error, YELP_ERROR, YELP_ERROR_NO_TOC,
		     _("The table of contents could not be loaded. The file "
		       "‘%s’ is either missing or is not well-formed XML."),
		     DATADIR "/yelp/toc.xml");
	yelp_pager_error (YELP_PAGER (pager), error);
	priv->cancel = TRUE;
	return FALSE;
    }

    xpath = xmlXPathNewContext (priv->toc_doc);
    obj = xmlXPathEvalExpression (BAD_CAST "//toc", xpath);
    for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	xmlNodePtr node = obj->nodesetval->nodeTab[i];
	xmlChar *icon = NULL;
#if 0
	xmlChar *id = NULL;
	xmlNodePtr cmdhelp;
	xmlNodePtr newnode;
	id = xmlGetProp (node, BAD_CAST "id");
	if (!xmlStrcmp (id, BAD_CAST "index")) {
	    cmdhelp = xmlNewChild (node, NULL, BAD_CAST "toc", NULL);
	    xmlNewNsProp (cmdhelp, NULL, BAD_CAST "id", BAD_CAST "ManInfoHolder");
	    xmlNewChild (cmdhelp, NULL, BAD_CAST "title", 
			 BAD_CAST _("Command Line Help"));
#ifdef ENABLE_MAN
	newnode = xmlNewChild (cmdhelp, NULL, BAD_CAST "toc", NULL);
	xmlNewNsProp (newnode, NULL, BAD_CAST "id", BAD_CAST "Man");
	xmlNewChild (newnode, NULL, BAD_CAST "title", 
		     BAD_CAST _("Manual Pages"));
#endif // ENABLE_MAN
#ifdef ENABLE_INFO
	newnode = xmlNewChild (cmdhelp, NULL, BAD_CAST "toc", NULL);
	xmlNewNsProp (newnode, NULL, BAD_CAST "id", BAD_CAST "Info");
	xmlNewChild (newnode, NULL, BAD_CAST "title", 
		     BAD_CAST _("GNU Info Pages"));
#endif //ENABLE_INFO
	}
	xmlFree (id);
#endif //ENABLE_MAN_OR_INFO

	xml_trim_titles (node, BAD_CAST "title");
	xml_trim_titles (node, BAD_CAST "description");

	icon = xmlGetProp (node, BAD_CAST "icon");
	if (icon) {
	    GtkIconInfo *info;
	    GtkIconTheme *theme = 
		(GtkIconTheme *) yelp_settings_get_icon_theme ();
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

    /* here we populate the category_hash structure of the YelpTocPager's
     * private data.  The hash key is a scrollkeeper category defined in the 
     * scrollkeeper.xml file and the value is a pointer to the parent node.
     */
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
		    /*g_hash_table_insert (priv->category_hash,
					 g_strdup ((gchar *) cat),
					 node);*/
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

    return FALSE;
}

static gboolean
process_xslt (YelpTocPager *pager)
{
    GError *error = NULL;
    xmlDocPtr outdoc = NULL;
    YelpTocPagerPriv *priv = pager->priv;
    gchar **params = NULL;
    gint  params_i = 0;
    gint  params_max = 10;
    GtkIconInfo *info;
    GtkIconTheme *theme = (GtkIconTheme *) yelp_settings_get_icon_theme ();

    if (!priv->toc_doc)
	return FALSE;

    /* only create and parse the stylesheet on the first call to this function */
    if (!priv->stylesheet) {
	priv->stylesheet = xsltParseStylesheetFile (BAD_CAST TOC_STYLESHEET);
    }

    if (!priv->stylesheet) {
	g_set_error (&error, YELP_ERROR, YELP_ERROR_PROC,
		     _("The table of contents could not be processed. The "
		       "file ‘%s’ is either missing or is not a valid XSLT "
		       "stylesheet."),
		     TOC_STYLESHEET);
	yelp_pager_error (YELP_PAGER (pager), error);
	goto done;
    }

    priv->transformContext = xsltNewTransformContext (priv->stylesheet,
						      priv->toc_doc);
    priv->transformContext->_private = pager;
    xsltRegisterExtElement (priv->transformContext,
			    BAD_CAST "document",
			    BAD_CAST YELP_NAMESPACE,
			    (xsltTransformFunction) xslt_yelp_document);

    params = g_new0 (gchar *, params_max);
    yelp_settings_params (&params, &params_i, &params_max);

    if ((params_i + 10) >= params_max - 1) {
	params_max += 10;
	params = g_renew (gchar *, params, params_max);
    }

    info = gtk_icon_theme_lookup_icon (theme, "yelp-icon-big", 192, 0);
    if (info) {
	params[params_i++] = "help_icon";
	params[params_i++] = g_strdup_printf ("\"%s\"",
					      gtk_icon_info_get_filename (info));
	params[params_i++] = "help_icon_size";
	params[params_i++] = g_strdup_printf ("%i",
					      gtk_icon_info_get_base_size (info));
	gtk_icon_info_free (info);
    }

    params[params_i++] = NULL;

    outdoc = xsltApplyStylesheetUser (priv->stylesheet,
				      priv->toc_doc,
				      (const gchar **)params, NULL, NULL,
				      priv->transformContext);
    /* Don't do this */
    g_signal_emit_by_name (pager, "finish");

 done:
    if (params) {
	for (params_i = 0; params[params_i] != NULL; params_i++)
	    if (params_i % 2 == 1)
		g_free ((gchar *) params[params_i]);
    }
    if (outdoc)
	xmlFreeDoc (outdoc);
    if (priv->toc_doc) {
	xmlFreeDoc (priv->toc_doc);
	priv->toc_doc = NULL;
    }
    if (priv->transformContext) {
	xsltFreeTransformContext (priv->transformContext);
	priv->transformContext = NULL;
    }

    return FALSE;
}

static gboolean
process_cleanup (YelpTocPager *pager)
{
    YelpTocPagerPriv *priv = pager->priv;

    /* dump the toc_doc if we are debugging */
    /*d (xmlDocDump (stdout, priv->toc_doc)); */

#ifdef ENABLE_MAN
    /* clean up the man directory language hash table */
    if (priv->man_dirlang) {
        g_hash_table_destroy (priv->man_dirlang);
        priv->man_dirlang = NULL;
    }

    /* clean up the man page section hash table */
    if (priv->man_secthash) {
	g_hash_table_destroy (priv->man_secthash);
	priv->man_secthash = NULL;
    }
#endif

    /* cleanup the stylesheet used to process the toc */
    if (priv->stylesheet) {
	xsltFreeStylesheet (priv->stylesheet);
	priv->stylesheet = NULL;
    }

    /* cleanup the parser context */
    if (priv->parser) {
	xmlFreeParserCtxt (priv->parser);
	priv->parser = NULL;
    }

    /* we only ever want to run this function once, so always return false */
    return FALSE;
}

static void
toc_add_doc_info (YelpTocPager *pager, YelpDocInfo *doc_info)
{
    xmlNodePtr node;
    xmlNodePtr new;
    gchar     *text;
    gchar     *category;

    g_return_if_fail (pager != NULL);
    if (doc_info == NULL)
	return;

    /* check if the document category exists, and return if it does not 
     * should fix 353554 */
    category = yelp_doc_info_get_category (doc_info);
    if (category == NULL) {
	debug_print (DB_DEBUG, "Missing category for %s\n", 
	             yelp_doc_info_get_title (doc_info));
	return;
    }

    YelpTocPagerPriv *priv = pager->priv;

    g_hash_table_insert (priv->unique_hash,
			 (gchar *) yelp_doc_info_get_id (doc_info),
			 doc_info);

    node = g_hash_table_lookup (priv->category_hash, category);

    text = yelp_doc_info_get_uri (doc_info, NULL, YELP_URI_TYPE_FILE);
    new = xmlNewChild (node, NULL, BAD_CAST "doc", NULL);
    xmlNewNsProp (new, NULL, BAD_CAST "href", BAD_CAST text);
    g_free (text);

    text = (gchar *) yelp_doc_info_get_title (doc_info);
    xmlNewTextChild (new, NULL, BAD_CAST "title", BAD_CAST text);

    text = (gchar *) yelp_doc_info_get_description (doc_info);
    xmlNewTextChild (new, NULL, BAD_CAST "description", BAD_CAST text);
}

static void
toc_remove_doc_info (YelpTocPager *pager, YelpDocInfo *doc_info)
{
#if 0
    GSList *category;
    YelpTocPagerPriv *priv = pager->priv;

    g_hash_table_remove (pager->priv->unique_hash, meta->id);

    for (category = meta->categories; category; category = category->next) {
	gchar  *catstr = (gchar *) category->data;
	GSList *metas =
	    (GSList *) g_hash_table_lookup (pager->priv->category_hash, catstr);

	metas = g_slist_remove (metas, meta);
	if (metas)
	    g_hash_table_insert (pager->priv->category_hash,
				 g_strdup (catstr), metas);
	else
	    g_hash_table_remove (pager->priv->category_hash, catstr);
    }

    priv->idx_pending = g_slist_remove (priv->idx_pending, meta);
#endif
}

static void
xslt_yelp_document (xsltTransformContextPtr ctxt,
		    xmlNodePtr              node,
		    xmlNodePtr              inst,
		    xsltStylePreCompPtr     comp)
{
    GError  *error;
    YelpPage *page;
    xmlChar *page_id = NULL;
    xmlChar *page_title = NULL;
    xmlChar *page_buf;
    gint     buf_size;
    YelpPager *pager;
    xsltStylesheetPtr style = NULL;
    const char *old_outfile;
    xmlDocPtr   new_doc = NULL;
    xmlDocPtr   old_doc;
    xmlNodePtr  old_insert;
    xmlNodePtr  cur;
    xmlDtdPtr dtd;

    if (!ctxt || !node || !inst || !comp)
	return;

    pager = (YelpPager *) ctxt->_private;

    page_id = xsltEvalAttrValueTemplate (ctxt, inst,
					 (const xmlChar *) "href",
					 NULL);
    if (page_id == NULL) {
	xsltTransformError (ctxt, NULL, inst,
			    _("No href attribute found on yelp:document"));
	error = NULL;
	yelp_pager_error (pager, error);
	goto done;
    }

    old_outfile = ctxt->outputFile;
    old_doc     = ctxt->output;
    old_insert  = ctxt->insert;
    ctxt->outputFile = (const char *) page_id;

    style = xsltNewStylesheet ();
    if (style == NULL) {
	xsltTransformError (ctxt, NULL, inst, _("Out of memory"));
	error = NULL;
	yelp_pager_error (pager, error);
	goto done;
    }

    style->omitXmlDeclaration = FALSE;

    new_doc = xmlNewDoc (BAD_CAST "1.0");
    dtd = xmlCreateIntSubset (new_doc,
			      BAD_CAST "html",
			      BAD_CAST "-//W3C//DTD XHTML 1.0 Strict//EN",
			      BAD_CAST "http://www.w3.org/TR/"
			      "xhtml1/DTD/xhtml1-strict.dtd");
    new_doc->intSubset = dtd;
    new_doc->extSubset = dtd;
    new_doc->charset = XML_CHAR_ENCODING_UTF8;
    new_doc->encoding = xmlStrdup (BAD_CAST "utf-8");
    new_doc->dict = ctxt->dict;
    xmlDictReference (new_doc->dict);

    ctxt->output = new_doc;
    ctxt->insert = (xmlNodePtr) new_doc;

    xsltApplyOneTemplate (ctxt, node, inst->children, NULL, NULL);

    xsltSaveResultToString (&page_buf, &buf_size, new_doc, style);

    ctxt->outputFile = old_outfile;
    ctxt->output     = old_doc;
    ctxt->insert     = old_insert;

    for (cur = node->children; cur; cur = cur->next) {
	if (!xmlStrcmp (cur->name, BAD_CAST "title")) {
	    page_title = xmlNodeGetContent (cur);
	    break;
	}
    }

    page = g_new0 (YelpPage, 1);

    if (page_id) {
	page->page_id = g_strdup ((gchar *) page_id);
	xmlFree (page_id);
    }
    if (page_title) {
	page->title = g_strdup ((gchar *) page_title);
	xmlFree (page_title);
    } else {
	page->title = g_strdup (_("Help Contents"));
    }
    page->contents = (gchar *) page_buf;

    cur = xmlDocGetRootElement (new_doc);
    for (cur = cur->children; cur; cur = cur->next) {
	if (!xmlStrcmp (cur->name, (xmlChar *) "head")) {
	    for (cur = cur->children; cur; cur = cur->next) {
		if (!xmlStrcmp (cur->name, (xmlChar *) "link")) {
		    xmlChar *rel = xmlGetProp (cur, BAD_CAST "rel");

		    if (!xmlStrcmp (rel, (xmlChar *) "Previous"))
			page->prev_id = (gchar *) xmlGetProp (cur, 
							      BAD_CAST "href");
		    else if (!xmlStrcmp (rel, (xmlChar *) "Next"))
			page->next_id = (gchar *) xmlGetProp (cur, 
							      BAD_CAST "href");
		    else if (!xmlStrcmp (rel, (xmlChar *) "Top"))
			page->toc_id = (gchar *) xmlGetProp (cur, 
							     BAD_CAST "href");

		    xmlFree (rel);
		}
	    }
	    break;
	}
    }

    yelp_pager_add_page (pager, page);
    g_signal_emit_by_name (pager, "page", page->page_id);

 done:
    if (new_doc)
	xmlFreeDoc (new_doc);
    if (style)
	xsltFreeStylesheet (style);
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
