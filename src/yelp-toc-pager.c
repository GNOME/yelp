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

#include <string.h>
#include <glib.h>
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

#define TOC_STYLESHEET_PATH DATADIR"/sgml/docbook/yelp/"
#define TOC_STYLESHEET      TOC_STYLESHEET_PATH"toc2html.xsl"

typedef gboolean      (*ProcessFunction)        (YelpTocPager      *pager);

typedef struct _YelpListing YelpListing;

struct _YelpTocPagerPriv {
    gboolean      sk_docomf;
    GSList       *omf_pending;

#ifdef ENABLE_MAN
    gint    mandirs_i;
    gint    langs_i;
    gint    manpaths_i;
    gchar **manpaths;

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
}

static void
toc_pager_init (YelpTocPager *pager)
{
    YelpTocPagerPriv *priv;

    priv = g_new0 (YelpTocPagerPriv, 1);
    pager->priv = priv;

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

    doc_info = yelp_doc_info_get ("x-yelp-toc:");
    
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
	// FIXME: clean stuff up.
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
    if (xmlStrEqual((const xmlChar*) name, "docomf"))
	priv->sk_docomf = TRUE;
}

static void
sk_endElement (void          *pager,
	       const xmlChar *name)
{
    YelpTocPagerPriv *priv = YELP_TOC_PAGER (pager)->priv;
    if (xmlStrEqual((const xmlChar*) name, "docomf"))
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
	omf = g_strndup (ch, len);
	priv->omf_pending = g_slist_prepend (priv->omf_pending, omf);
    }
}

static gboolean
process_read_scrollkeeper (YelpTocPager *pager)
{
    gchar  *content_list;
    gchar  *stderr;
    gchar  *lang;
    gchar  *command;
    static xmlSAXHandler sk_sax_handler = { 0, };

    const gchar * const * langs = g_get_language_names ();
    if (langs && langs[0])
	lang = (gchar *) langs[0];
    else
	lang = "C";

    command = g_strconcat("scrollkeeper-get-content-list ", lang, NULL);

    if (g_spawn_command_line_sync (command, &content_list, &stderr, NULL, NULL)) {
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
    g_free (stderr);
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
	xmlXPathEvalExpression ("string(/omf/resource/identifier/@url)", omf_xpath);
    omf_title =
	xmlXPathEvalExpression ("string(/omf/resource/title)", omf_xpath);
    omf_description =
	xmlXPathEvalExpression ("string(/omf/resource/description)", omf_xpath);
    omf_category =
	xmlXPathEvalExpression ("string(/omf/resource/subject/@category)", omf_xpath);
    omf_language =
	xmlXPathEvalExpression ("string(/omf/resource/language/@code)", omf_xpath);
    omf_seriesid =
	xmlXPathEvalExpression ("string(/omf/resource/relation/@seriesid)", omf_xpath);

    doc_info = yelp_doc_info_get (omf_url->stringval);
    if (!doc_info)
	goto done;
    yelp_doc_info_set_title (doc_info, omf_title->stringval);
    yelp_doc_info_set_language (doc_info, omf_language->stringval);
    yelp_doc_info_set_category (doc_info, omf_category->stringval);
    yelp_doc_info_set_description (doc_info, omf_description->stringval);

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
static gboolean
process_mandir_pending (YelpTocPager *pager)
{
    xmlNodePtr tmp;
    gchar *manpath;

    gchar *dirname, *filename;
    GDir  *dir;

    const gchar * const * langs = g_get_language_names ();

    YelpTocPagerPriv *priv  = YELP_TOC_PAGER (pager)->priv;

    if (!priv->toc_doc) {
	xmlXPathContextPtr xpath;
	xmlXPathObjectPtr  obj;
	gint i;
	priv->toc_doc = xmlCtxtReadFile (priv->parser, DATADIR "/yelp/man.xml", NULL,
					 XML_PARSE_NOBLANKS | XML_PARSE_NOCDATA  |
					 XML_PARSE_NOENT    | XML_PARSE_NOERROR  |
					 XML_PARSE_NONET    );
	priv->man_secthash = g_hash_table_new (g_str_hash, g_str_equal);

	xpath = xmlXPathNewContext (priv->toc_doc);
	obj = xmlXPathEvalExpression ("//toc", xpath);
	for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	    xmlNodePtr node = obj->nodesetval->nodeTab[i];
	    xmlChar *sect = xmlGetProp (node, "sect");
	    if (sect)
		g_hash_table_insert (priv->man_secthash, sect, node);

	    xml_trim_titles (node);
	}
    }

    if (priv->langs_i == 0 && priv->manpaths_i == 0) {
	if (priv->man_manhash)
	    g_hash_table_destroy (priv->man_manhash);
	priv->man_manhash = g_hash_table_new_full (g_str_hash, g_str_equal,
						   g_free,     NULL);
    }

    if (!priv->manpaths) {
	if (!g_spawn_command_line_sync ("manpath", &manpath, NULL, NULL, NULL))
	    manpath = g_strdup (g_getenv ("MANPATH"));

	if (manpath) {
	    g_strstrip (manpath);
	    priv->manpaths = g_strsplit (manpath, G_SEARCHPATH_SEPARATOR_S, -1);
	    g_free (manpath);
	} else {
	    goto done;
	}
    }

    if (g_str_equal (langs[priv->langs_i], "C"))
	dirname = g_build_filename (priv->manpaths[priv->manpaths_i],
				    mandirs[priv->mandirs_i],
				    NULL);
    else
	dirname = g_build_filename (priv->manpaths[priv->manpaths_i],
				    langs[priv->langs_i],
				    mandirs[priv->mandirs_i],
				    NULL);
    dir = g_dir_open (dirname, 0, NULL);
    if (dir) {
	while ((filename = (gchar *) g_dir_read_name (dir))) {
	    gchar *c1 = NULL, *c2 = NULL, *manname, *mansect, *manman;

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
	    } else {
		mansect = g_strdup (mandirs[priv->mandirs_i] + 3);
		if (c1)
		    manname = g_strndup (filename, c1 - filename);
		else
		    manname = g_strdup (filename);
	    }

	    manman = g_strconcat (manname, ".", mansect, NULL);

	    if (g_hash_table_lookup (priv->man_manhash, manman) == NULL) {
		tmp = g_hash_table_lookup (priv->man_secthash, mansect);
		if (tmp == NULL && strlen (mansect) > 1) {
		    gchar *mansect0 = g_strndup (mansect, 1);
		    tmp = g_hash_table_lookup (priv->man_secthash, mansect0);
		}

		if (tmp) {
		    gchar *tooltip, *url_full, *url_short;
		    YelpDocInfo *info;

		    url_full = g_strconcat ("man:", dirname, "/", filename, NULL);
		    url_short = g_strconcat ("man:", manname, ".", mansect, NULL);
		    info = yelp_doc_info_get (url_full);

		    if (info) {
			yelp_doc_info_add_uri (info, url_short, YELP_URI_TYPE_MAN);
			tmp = xmlNewChild (tmp, NULL, "doc", NULL);
			xmlNewNsProp (tmp, NULL, "href", url_full);

			xmlNewChild (tmp, NULL, "title", manname);
			tooltip = g_strdup_printf (_("Read man page for %s"), manname);
			xmlNewChild (tmp, NULL, "tooltip", tooltip);
			g_free (tooltip);
		    }

		    g_free (url_full);
		    g_free (url_short);
		} else {
		    g_warning ("Could not locate section %s for %s\n",
			       mansect, manman);
		}

		g_hash_table_insert (priv->man_manhash, g_strdup (manman), priv);
	    }

	    g_free (manman);
	    g_free (manname);
	    g_free (mansect);
	}
	g_dir_close (dir);
    }

 done:
    g_free (dirname);

    priv->manpaths_i++;
    if (priv->manpaths[priv->manpaths_i] == NULL) {
	priv->manpaths_i = 0;
	priv->langs_i++;
    }
    if (langs[priv->langs_i] == NULL) {
	priv->langs_i = 0;
	priv->mandirs_i++;
	if (priv->man_manhash) {
	    g_hash_table_destroy (priv->man_manhash);
	    priv->man_manhash = NULL;
	}
    }
    if (mandirs[priv->mandirs_i] == NULL) {
	if (priv->manpaths) {
	    g_strfreev (priv->manpaths);
	    priv->manpaths = NULL;
	}
	if (priv->man_secthash) {
	    g_hash_table_destroy (priv->man_secthash);
	    priv->man_secthash = NULL;
	}
	return FALSE;
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
#endif // ENABLE_INFO

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
    obj = xmlXPathEvalExpression ("//toc", xpath);
    for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	xmlNodePtr node = obj->nodesetval->nodeTab[i];
	xmlChar *icon = NULL;
	xmlChar *id = NULL;

#ifdef ENABLE_MAN
	id = xmlGetProp (node, "id");
	if (!xmlStrcmp (id, "index")) {
	    xmlNodePtr new = xmlNewChild (node, NULL, "toc", NULL);
	    xmlNewNsProp (new, NULL, "id", "Man");
	    xmlNewChild (new, NULL, "title", _("Man Pages"));
	}
#endif

	xml_trim_titles (node);

	icon = xmlGetProp (node, "icon");
	if (icon) {
	    GtkIconInfo *info;
	    GtkIconTheme *theme = 
		(GtkIconTheme *) yelp_settings_get_icon_theme ();
	    info = gtk_icon_theme_lookup_icon (theme, icon, 48, 0);
	    if (info) {
		xmlNodePtr new = xmlNewChild (node, NULL, "icon", NULL);
		xmlNewNsProp (new, NULL, "file", gtk_icon_info_get_filename (info));
		gtk_icon_info_free (info);
	    }
	}

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
	    xmlChar *id = xmlTextReaderGetAttribute (reader, "id");
	    xmlNodePtr node;
	    gchar *xpath_s;

	    if (!id) {
		ret = xmlTextReaderRead (reader);
		continue;
	    }

	    xpath_s = g_strdup_printf ("//toc[@id = '%s']", id);
	    obj = xmlXPathEvalExpression (xpath_s, xpath);
	    node = obj->nodesetval->nodeTab[0];
	    xmlXPathFreeObject (obj);

	    ret = xmlTextReaderRead (reader);
	    while (ret == 1) {
		if (!xmlStrcmp (xmlTextReaderConstLocalName (reader),
				BAD_CAST "subject")) {
		    xmlChar *cat = xmlTextReaderGetAttribute (reader, "category");
		    g_hash_table_insert (priv->category_hash,
					 g_strdup (cat),
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
    xmlDocPtr outdoc;
    YelpTocPagerPriv *priv = pager->priv;
    const gchar **params;
    gint  params_i = 0;
    gint  params_max = 10;
    GtkIconInfo *info;
    GtkIconTheme *theme = (GtkIconTheme *) yelp_settings_get_icon_theme ();

    priv->stylesheet = xsltParseStylesheetFile (TOC_STYLESHEET);
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
			    "document",
			    YELP_NAMESPACE,
			    (xsltTransformFunction) xslt_yelp_document);

    params = g_new0 (gchar *, params_max);
    yelp_settings_params (&params, &params_i, &params_max);

    if ((params_i + 10) >= params_max - 1) {
	params_max += 10;
	params = g_renew (gchar *, params, params_max);
    }

    info = gtk_icon_theme_lookup_icon (theme, "gnome-help", 192, 0);
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
				      params, NULL, NULL,
				      priv->transformContext);
    g_signal_emit_by_name (pager, "finish");

 done:
    for (params_i = 0; params[params_i] != NULL; params_i++)
	if (params_i % 2 == 1)
	    g_free ((gchar *) params[params_i]);
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
    new = xmlNewChild (node, NULL, "doc", NULL);
    xmlNewNsProp (new, NULL, "href", text);
    g_free (text);

    text = (gchar *) yelp_doc_info_get_title (doc_info);
    xmlNewTextChild (new, NULL, "title", text);
    g_free (text);

    text = (gchar *) yelp_doc_info_get_description (doc_info);
    xmlNewTextChild (new, NULL, "description", text);
    g_free (text);
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

    new_doc = xmlNewDoc ("1.0");
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
	page->page_id = g_strdup (page_id);
	xmlFree (page_id);
    }
    if (page_title) {
	page->title = g_strdup (page_title);
	xmlFree (page_title);
    } else {
	page->title = g_strdup (_("Help Contents"));
    }
    page->contents = page_buf;

    cur = xmlDocGetRootElement (new_doc);
    for (cur = cur->children; cur; cur = cur->next) {
	if (!xmlStrcmp (cur->name, (xmlChar *) "head")) {
	    for (cur = cur->children; cur; cur = cur->next) {
		if (!xmlStrcmp (cur->name, (xmlChar *) "link")) {
		    xmlChar *rel = xmlGetProp (cur, "rel");

		    if (!xmlStrcmp (rel, (xmlChar *) "Previous"))
			page->prev_id = xmlGetProp (cur, "href");
		    else if (!xmlStrcmp (rel, (xmlChar *) "Next"))
			page->next_id = xmlGetProp (cur, "href");
		    else if (!xmlStrcmp (rel, (xmlChar *) "Top"))
			page->toc_id = xmlGetProp (cur, "href");

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
