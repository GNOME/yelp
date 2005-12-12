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

#include "yelp-error.h"
#include "yelp-settings.h"
#include "yelp-toc-pager.h"
#include "yelp-utils.h"

#ifdef YELP_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define YELP_NAMESPACE "http://www.gnome.org/yelp/ns"

#define STYLESHEET_PATH DATADIR"/yelp/xslt/"
#define TOC_STYLESHEET  STYLESHEET_PATH"toc2html.xsl"

typedef gboolean      (*ProcessFunction)        (YelpTocPager      *pager);

typedef struct _YelpListing YelpListing;

#define YELP_TOC_PAGER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_TOC_PAGER, YelpTocPagerPriv))

struct _YelpTocPagerPriv {
    gboolean      sk_docomf;
    GSList       *omf_pending;

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
#endif

#ifdef ENABLE_INFO
    GSList       *info_dir_pending;
    GSList       *info_pending;
#endif

    xmlDocPtr     toc_doc;

    GHashTable   *unique_hash;
    GHashTable   *category_hash;

    xmlParserCtxtPtr  parser;

    ProcessFunction   pending_func;

    gboolean      cancel;
    gint          pause_count;

    xsltStylesheetPtr       stylesheet;
    xsltTransformContextPtr transformContext;
};

struct _YelpListing {
    gchar       *id;
    gchar       *title;

    GSList      *listings;
    GSList      *documents;

    gboolean     has_listings;
};

static void          toc_pager_class_init      (YelpTocPagerClass *klass);
static void          toc_pager_init            (YelpTocPager      *pager);
static void          toc_pager_dispose         (GObject           *gobject);

static void          toc_pager_error           (YelpPager        *pager);
static void          toc_pager_cancel          (YelpPager        *pager);
static void          toc_pager_finish          (YelpPager        *pager);

gboolean             toc_pager_process         (YelpPager         *pager);
void                 toc_pager_cancel          (YelpPager         *pager);
const gchar *        toc_pager_resolve_frag    (YelpPager         *pager,
						const gchar       *frag_id);
GtkTreeModel *       toc_pager_get_sections    (YelpPager         *pager);

static gboolean      toc_process_pending       (YelpTocPager      *pager);


static gboolean      process_read_menu         (YelpTocPager      *pager);
static gboolean      process_xslt              (YelpTocPager      *pager);
static gboolean      process_read_scrollkeeper (YelpTocPager      *pager);
static gboolean      process_omf_pending       (YelpTocPager      *pager);
#ifdef ENABLE_MAN
static gboolean      process_mandir_pending    (YelpTocPager      *pager);
#endif
#ifdef ENABLE_INFO
static gboolean      process_info_dir_pending  (YelpTocPager      *pager);
static gboolean      process_info_pending      (YelpTocPager      *pager);
#endif

static void          toc_add_doc_info          (YelpTocPager      *pager,
						YelpDocInfo       *doc_info);
static void          toc_remove_doc_info       (YelpTocPager      *pager,
						YelpDocInfo       *doc_info);
static void          xml_trim_titles           (xmlNodePtr         node);
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
}

static void
toc_pager_dispose (GObject *object)
{
    YelpTocPager *pager = YELP_TOC_PAGER (object);

    g_free (pager->priv);

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

    d (g_print ("yelp_toc_pager_pause\n"));
    d (g_print ("  pause_caunt = %i\n", pager->priv->pause_count + 1));

    pager->priv->pause_count = pager->priv->pause_count + 1;
}

void
yelp_toc_pager_unpause (YelpTocPager *pager)
{
    g_return_if_fail (pager != NULL);

    d (g_print ("yelp_toc_pager_unpause\n"));
    d (g_print ("  pause_caunt = %i\n", pager->priv->pause_count - 1));

    pager->priv->pause_count = pager->priv->pause_count - 1;
    if (pager->priv->pause_count < 0) {
	g_warning (_("YelpTocPager: Pause count is negative."));
	pager->priv->pause_count = 0;
    }

    if (pager->priv->pause_count < 1) {
	gtk_idle_add_priority (G_PRIORITY_LOW,
			       (GtkFunction) toc_process_pending,
			       pager);
    }
}

/******************************************************************************/

static void
toc_pager_error (YelpPager *pager)
{
    d (g_print ("toc_pager_error\n"));
    yelp_pager_set_state (pager, YELP_PAGER_STATE_ERROR);
}

static void
toc_pager_cancel (YelpPager *pager)
{
    YelpTocPagerPriv *priv = YELP_TOC_PAGER (pager)->priv;

    d (g_print ("toc_pager_cancel\n"));
    yelp_pager_set_state (pager, YELP_PAGER_STATE_INVALID);

    priv->cancel = TRUE;
}

static void
toc_pager_finish (YelpPager   *pager)
{
    d (g_print ("toc_pager_finish\n"));
    yelp_pager_set_state (pager, YELP_PAGER_STATE_FINISHED);
}

gboolean
toc_pager_process (YelpPager *pager)
{
    YelpTocPagerPriv *priv = YELP_TOC_PAGER (pager)->priv;

    d (g_print ("toc_pager_process\n"));

    yelp_pager_set_state (pager, YELP_PAGER_STATE_PARSING);
    g_signal_emit_by_name (pager, "parse");

    /* Set it running */
    yelp_pager_set_state (pager, YELP_PAGER_STATE_RUNNING);
    g_signal_emit_by_name (pager, "start");

    priv->pending_func = (ProcessFunction) process_read_menu;

    gtk_idle_add_priority (G_PRIORITY_LOW,
			   (GtkFunction) toc_process_pending,
			   pager);
    return FALSE;
}

const gchar *
toc_pager_resolve_frag (YelpPager *pager, const gchar *frag_id)
{
    if (!frag_id)
	return "index";
    else
	return frag_id;
}

GtkTreeModel *
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
    static gpointer process_funcs[] = {
	process_read_menu,
	process_read_scrollkeeper,
	process_omf_pending,
	process_xslt,
#ifdef ENABLE_MAN
	process_mandir_pending,
	process_xslt,
#endif
#ifdef ENABLE_INFO
	process_info_dir_pending,
	process_info_pending,
#endif
	NULL
    };
    static gint process_i = 0;

    if (priv->cancel || !priv->pending_func) {
	/* FIXME: clean stuff up. */
	return FALSE;
    }

    readd = priv->pending_func(pager);

    if (!readd)
	priv->pending_func = process_funcs[++process_i];

    if (priv->pending_func == NULL)
	g_signal_emit_by_name (pager, "finish");

    if ((priv->pending_func != NULL) && (priv->pause_count < 1))
	return TRUE;
    else
	return FALSE;
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
	omf = g_strndup ((gchar *) ch, len);
	priv->omf_pending = g_slist_prepend (priv->omf_pending, omf);
    }
}

static gboolean
process_read_scrollkeeper (YelpTocPager *pager)
{
    gchar  *content_list;
    gchar  *stderr_str;
    gchar  *lang;
    gchar  *command;
    static xmlSAXHandler sk_sax_handler = { 0, };

    const gchar * const * langs = g_get_language_names ();
    if (langs && langs[0])
	lang = (gchar *) langs[0];
    else
	lang = "C";

    command = g_strconcat("scrollkeeper-get-content-list ", lang, NULL);

    if (g_spawn_command_line_sync (command, &content_list, &stderr_str, NULL, NULL)) {
	if (!sk_sax_handler.startElement) {
	    sk_sax_handler.startElement = sk_startElement;
	    sk_sax_handler.endElement   = sk_endElement;
	    sk_sax_handler.characters   = sk_characters;
	    sk_sax_handler.initialized  = TRUE;
	}
	content_list = g_strstrip (content_list);
	xmlSAXUserParseFile (&sk_sax_handler, pager, content_list);
    }

    g_free (content_list);
    g_free (stderr_str);
    g_free (command);
    return FALSE;
}

/** process_omf_pending *******************************************************/

static gboolean
process_omf_pending (YelpTocPager *pager)
{
    GSList  *first = NULL;
    gchar   *file  = NULL;
    gchar   *id    = NULL;

    xmlDocPtr          omf_doc    = NULL;
    xmlXPathContextPtr omf_xpath  = NULL;
    xmlXPathObjectPtr  omf_url    = NULL;
    xmlXPathObjectPtr  omf_title  = NULL;
    xmlXPathObjectPtr  omf_description = NULL;
    xmlXPathObjectPtr  omf_category    = NULL;
    xmlXPathObjectPtr  omf_language    = NULL;
    xmlXPathObjectPtr  omf_seriesid    = NULL;

    YelpDocInfo *doc_info;
    YelpDocInfo *doc_old;

    YelpTocPagerPriv *priv  = YELP_TOC_PAGER (pager)->priv;

    first = priv->omf_pending;
    if (!first)
	goto done;
    priv->omf_pending = g_slist_remove_link (priv->omf_pending, first);
    file = (gchar *) first->data;
    if (!file || !g_str_has_suffix (file, ".omf"))
	goto done;

    omf_doc = xmlCtxtReadFile (priv->parser, (const char *) file, NULL,
			       XML_PARSE_NOBLANKS | XML_PARSE_NOCDATA  |
			       XML_PARSE_NOENT    | XML_PARSE_NOERROR  |
			       XML_PARSE_NONET    );
    if (!omf_doc) {
	g_warning (_("Could not load the OMF file '%s'."), file);
	goto done;
    }

    omf_xpath = xmlXPathNewContext (omf_doc);
    omf_url =
	xmlXPathEvalExpression (BAD_CAST "string(/omf/resource/identifier/@url)", omf_xpath);
    omf_title =
	xmlXPathEvalExpression (BAD_CAST "string(/omf/resource/title)", omf_xpath);
    omf_description =
	xmlXPathEvalExpression (BAD_CAST "string(/omf/resource/description)", omf_xpath);
    omf_category =
	xmlXPathEvalExpression (BAD_CAST "string(/omf/resource/subject/@category)", omf_xpath);
    omf_language =
	xmlXPathEvalExpression (BAD_CAST "string(/omf/resource/language/@code)", omf_xpath);
    omf_seriesid =
	xmlXPathEvalExpression (BAD_CAST "string(/omf/resource/relation/@seriesid)", omf_xpath);

    doc_info = yelp_doc_info_get ((const gchar *) omf_url->stringval, FALSE);
    if (!doc_info)
	goto done;
    yelp_doc_info_set_title (doc_info, (gchar *) omf_title->stringval);
    yelp_doc_info_set_language (doc_info, (gchar *) omf_language->stringval);
    yelp_doc_info_set_category (doc_info, (gchar *) omf_category->stringval);
    yelp_doc_info_set_description (doc_info, (gchar *) omf_description->stringval);

    id = g_strconcat ("scrollkeeper.", (gchar *) omf_seriesid->stringval, NULL);
    yelp_doc_info_set_id (doc_info, id);

    doc_old = g_hash_table_lookup (priv->unique_hash, id);

    if (doc_old) {
	if (yelp_doc_info_cmp_language (doc_info, doc_old) < 0) {
	    toc_remove_doc_info (YELP_TOC_PAGER (pager), doc_old);
	    toc_add_doc_info (YELP_TOC_PAGER (pager), doc_info);
	}
    } else {
	toc_add_doc_info (YELP_TOC_PAGER (pager), doc_info);
    }

 done:
    if (omf_url)
	xmlXPathFreeObject (omf_url);
    if (omf_title)
	xmlXPathFreeObject (omf_title);
    if (omf_description)
	xmlXPathFreeObject (omf_description);
    if (omf_category)
	xmlXPathFreeObject (omf_category);
    if (omf_language)
	xmlXPathFreeObject (omf_language);
    if (omf_seriesid)
	xmlXPathFreeObject (omf_seriesid);

    if (omf_doc)
	xmlFreeDoc (omf_doc);
    if (omf_xpath)
	xmlXPathFreeContext (omf_xpath);

    g_free (id);
    g_free (file);
    g_slist_free_1 (first);

    if (priv->omf_pending)
	return TRUE;
    else
	return FALSE;
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

    if (c1)
	c2 = g_strrstr_len (filename, c1 - filename, ".");
    else
	c2 = g_strrstr (filename, ".");

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
    xmlXPathContextPtr xpath = NULL;
    xmlXPathObjectPtr objsect = NULL;
    xmlDocPtr manindex_xml = NULL;
    gint update_flag = 0;
    gint i, j, k;
    
    YelpTocPagerPriv *priv  = YELP_TOC_PAGER (pager)->priv;

    manindex_xml = xmlReadFile (index_file, NULL, XML_PARSE_NOCDATA | 
		                XML_PARSE_NOERROR | XML_PARSE_NONET);

    if (manindex_xml == NULL) {
	g_warning ("Unable to parse index file \"%s\"\n", index_file);
	return 0;
    }
    
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
	    xmlXPathObjectPtr objdirname;
	    xmlXPathObjectPtr objdirmtime;
	    xmlXPathObjectPtr objmanpages;
	    xmlNodePtr dirnode = objdirs->nodesetval->nodeTab[j];
	    xmlNodePtr node = NULL;
	    xmlChar *dirname = NULL;
	    xmlChar *dirmtime = NULL;
	    time_t mtime;
	    struct stat buf;
	    
	    xpath->node = dirnode;
	    objdirmtime = xmlXPathEvalExpression (BAD_CAST "@mtime", xpath);
	    
	    node = objdirmtime->nodesetval->nodeTab[0];
	    dirmtime = xmlNodeListGetString (manindex_xml, 
	                                     node->xmlChildrenNode, 1);
	    
	    objdirname = xmlXPathEvalExpression (BAD_CAST "name[1]", xpath);

	    node = objdirname->nodesetval->nodeTab[0];
	    dirname = xmlNodeListGetString (manindex_xml, 
	                                    node->xmlChildrenNode, 1);
	    
	    if (g_stat ((gchar *)dirname, &buf) < 0)
		g_warning ("Unable to stat dir: \"%s\"\n", dirname);
	    
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
		}

		/* we replace the node in the tree */
		xmlReplaceNode (dirnode, newNode);

		g_dir_close (dir);
	    
	    /* otherwise just read from the index file */
	    } else {
	    
		objmanpages = xmlXPathEvalExpression (BAD_CAST "page", xpath);

		for (k=0; k < objmanpages->nodesetval->nodeNr; k++) {
		    xmlNodePtr node = objmanpages->nodesetval->nodeTab[k];
		    xmlChar *manpage = NULL;

		    manpage = xmlNodeListGetString (manindex_xml,
		                                    node->xmlChildrenNode, 1);

		    add_man_page_to_toc (pager, (gchar *)dirname, (gchar *)manpage);
		    priv->manpage_count++;
	        }
	    }
	}

	if (priv->man_manhash) {
	    g_hash_table_destroy (priv->man_manhash);
	    priv->man_manhash = NULL;
	}
    }

    if (update_flag) {
	create_manindex_file (index_file, manindex_xml);
    }

    xmlFree (manindex_xml);
    
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

    if (!priv->toc_doc) {
	xmlXPathContextPtr xpath;
	xmlXPathObjectPtr  obj;

	priv->toc_doc = xmlCtxtReadFile (priv->parser, DATADIR "/yelp/man.xml", NULL,
					 XML_PARSE_NOBLANKS | XML_PARSE_NOCDATA  |
					 XML_PARSE_NOENT    | XML_PARSE_NOERROR  |
					 XML_PARSE_NONET    );
	priv->man_secthash = g_hash_table_new (g_str_hash, g_str_equal);

	xpath = xmlXPathNewContext (priv->toc_doc);
	obj = xmlXPathEvalExpression (BAD_CAST "//toc", xpath);

	for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	    xmlNodePtr node = obj->nodesetval->nodeTab[i];
	    xmlChar *sect = xmlGetProp (node, BAD_CAST "sect");
	    if (sect)
		g_hash_table_insert (priv->man_secthash, sect, node);

	    xml_trim_titles (node);
	}
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

	/* check for the existence of the xml cache file in ~/.gnome2/yelp.d/
	 * if it exists, use it as the source for man pages instead of 
	 * searching the hard disk for them - should make startup much faster */
	if (g_file_test (index_file, G_FILE_TEST_EXISTS) &&
	    create_toc_from_index (pager, index_file)) {

	    /* we are done.. */
	    return FALSE;
	} else {
	    priv->manindex_xml = xmlNewDoc (BAD_CAST "1.0");
	    priv->root = xmlNewNode (NULL, BAD_CAST "manindex");
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

			tmplist = g_slist_prepend (tmplist, dirname);
		    }
		}

		priv->mandir_list = g_slist_prepend (priv->mandir_list, tmplist);
	    }
 
	    priv->mandir_ptr = priv->mandir_list;
	    if (priv->mandir_list && priv->mandir_list->data) {
		priv->mandir_langpath = priv->mandir_list->data;
	    }
	}
    }
    /* iterate through our previously created linked lists and create the
     * table of contents from them */
    else {
	if (!priv->man_manhash) {
	    priv->man_manhash = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                               g_free,     NULL);

            priv->ins = xmlNewChild (priv->root, NULL, BAD_CAST "mansect", NULL);
	    xmlAddChild (priv->ins, xmlNewText (BAD_CAST "\n    "));			    
	}

	if (priv->mandir_langpath && priv->mandir_langpath->data) {
	    dirname = priv->mandir_langpath->data;

	    if ((dir = g_dir_open (dirname, 0, NULL))) {
		struct stat buf;
		gchar mtime_str[20];

		if (g_stat (dirname, &buf) < 0)
		    g_warning ("Unable to stat dir: \"%s\"\n", dirname);

		g_snprintf (mtime_str, 20, "%u", (guint) buf.st_mtime);

		priv->ins = xmlNewChild (priv->ins, NULL, BAD_CAST "dir", NULL);
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

		/* done processing */
		return FALSE;
	    }
	}
    }

    return TRUE;
}
#endif // ENABLE_MAN

#ifdef ENABLE_INFO
static gboolean
process_info_dir_pending (YelpTocPager *pager)
{
    return FALSE;
}

static gboolean
process_info_pending (YelpTocPager *pager)
{
    return FALSE;
}
#endif /* ENABLE_INFO */

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

#ifdef ENABLE_MAN
	xmlChar *id = NULL;
	id = xmlGetProp (node, BAD_CAST "id");
	if (!xmlStrcmp (id, BAD_CAST "index")) {
	    xmlNodePtr new = xmlNewChild (node, NULL, BAD_CAST "toc", NULL);
	    xmlNewNsProp (new, NULL, BAD_CAST "id", BAD_CAST "Man");
	    xmlNewChild (new, NULL, BAD_CAST "title", 
			 BAD_CAST _("Manual Pages"));
	}
#endif

	xml_trim_titles (node);

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
		    g_hash_table_insert (priv->category_hash,
					 g_strdup ((gchar *) cat),
					 node);
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

    priv->stylesheet = xsltParseStylesheetFile (BAD_CAST TOC_STYLESHEET);
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
    if (priv->stylesheet) {
	xsltFreeStylesheet (priv->stylesheet);
	priv->stylesheet = NULL;
    }
    if (priv->transformContext) {
	xsltFreeTransformContext (priv->transformContext);
	priv->transformContext = NULL;
    }

    return FALSE;
}

static void
toc_add_doc_info (YelpTocPager *pager, YelpDocInfo *doc_info)
{
    xmlNodePtr node;
    xmlNodePtr new;
    gchar     *text;

    YelpTocPagerPriv *priv = pager->priv;

    g_hash_table_insert (priv->unique_hash,
			 (gchar *) yelp_doc_info_get_id (doc_info),
			 doc_info);

    node = g_hash_table_lookup (priv->category_hash,
				yelp_doc_info_get_category (doc_info));

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
	xsltTransformError (ctxt, NULL, inst,
			    _("Out of memory"));
	error = NULL;
	yelp_pager_error (pager, error);
	goto done;
    }

    style->omitXmlDeclaration = TRUE;

    new_doc = xmlNewDoc (BAD_CAST "1.0");
    new_doc->charset = XML_CHAR_ENCODING_UTF8;
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
xml_trim_titles (xmlNodePtr node)
{
    xmlNodePtr cur, keep = NULL;
    xmlChar *keep_lang = NULL;
    int j, keep_pri = INT_MAX;

    const gchar * const * langs = g_get_language_names ();

    for (cur = node->children; cur; cur = cur->next) {
	if (!xmlStrcmp (cur->name, BAD_CAST "title")) {
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
	if (!xmlStrcmp (this->name, BAD_CAST "title")) {
	    if (this != keep) {
		xmlUnlinkNode (this);
		xmlFreeNode (this);
	    }
	}
    }
    xmlFree (keep_lang);
}
