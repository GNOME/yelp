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

#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxslt/xslt.h>
#include <libxslt/templates.h>
#include <libxslt/transform.h>
#include <libxslt/extensions.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/xsltutils.h>

#include "yelp-error.h"
#include "yelp-man-pager.h"
#include "yelp-man-parser.h"
#include "yelp-settings.h"
#include "yelp-toc-pager.h"

#define YELP_NAMESPACE "http://www.gnome.org/yelp/ns"

#define MAN_STYLESHEET_PATH DATADIR"/sgml/docbook/yelp/"
#define MAN_STYLESHEET      MAN_STYLESHEET_PATH"man2html.xsl"

#define MAX_CHUNK_DEPTH 2

#define d(x)

struct _YelpManPagerPriv {
};

static void          man_pager_class_init   (YelpManPagerClass *klass);
static void          man_pager_init         (YelpManPager      *pager);
static void          man_pager_dispose      (GObject           *gobject);

void                 man_pager_error        (YelpPager        *pager);
void                 man_pager_cancel       (YelpPager        *pager);
void                 man_pager_finish       (YelpPager        *pager);

gboolean             man_pager_process      (YelpPager        *pager);
const gchar *        man_pager_resolve_frag (YelpPager        *pager,
					     const gchar      *frag_id);
GtkTreeModel *       man_pager_get_sections (YelpPager        *pager);

static void        xslt_yelp_document    (xsltTransformContextPtr ctxt,
					  xmlNodePtr              node,
					  xmlNodePtr              inst,
					  xsltStylePreCompPtr     comp);
static gboolean    xml_is_info           (xmlNodePtr        node);
static gchar *     xml_get_title         (xmlNodePtr node);

static YelpPagerClass *parent_class;

GType
yelp_man_pager_get_type (void)
{
    static GType type = 0;

    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpManPagerClass),
	    NULL,
	    NULL,
	    (GClassInitFunc) man_pager_class_init,
	    NULL,
	    NULL,
	    sizeof (YelpManPager),
	    0,
	    (GInstanceInitFunc) man_pager_init,
	};
	type = g_type_register_static (YELP_TYPE_PAGER,
				       "YelpManPager", 
				       &info, 0);
    }
    return type;
}

static void
man_pager_class_init (YelpManPagerClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);
    YelpPagerClass *pager_class  = YELP_PAGER_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = man_pager_dispose;

    pager_class->error        = man_pager_error;
    pager_class->cancel       = man_pager_cancel;
    pager_class->finish       = man_pager_finish;

    pager_class->process      = man_pager_process;
    pager_class->resolve_frag = man_pager_resolve_frag;
    pager_class->get_sections = man_pager_get_sections;
}

static void
man_pager_init (YelpManPager *pager)
{
    YelpManPagerPriv *priv;

    priv = g_new0 (YelpManPagerPriv, 1);
    pager->priv = priv;
}

static void
man_pager_dispose (GObject *object)
{
    YelpManPager *pager = YELP_MAN_PAGER (object);

    g_free (pager->priv);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

YelpPager *
yelp_man_pager_new (YelpDocInfo *doc_info)
{
    YelpManPager *pager;

    g_return_val_if_fail (doc_info != NULL, NULL);

    pager = (YelpManPager *) g_object_new (YELP_TYPE_MAN_PAGER,
					   "document-info", doc_info,
					   NULL);

    return (YelpPager *) pager;
}

gboolean
man_pager_process (YelpPager *pager)
{
    YelpDocInfo   *doc_info;
    gchar         *filename;
    YelpManParser *parser;
    xmlDocPtr      doc;
    GError        *error;

    xsltStylesheetPtr       stylesheet;
    xsltTransformContextPtr tctxt;

    const gchar  *params[40];
    gint i = 0;

    g_return_val_if_fail (YELP_IS_MAN_PAGER (pager), FALSE);

    doc_info = yelp_pager_get_doc_info (pager);

    filename = yelp_doc_info_get_filename (doc_info);

    g_object_ref (pager);

    yelp_toc_pager_pause (yelp_toc_pager_get ());

    parser = yelp_man_parser_new ();
    doc = yelp_man_parser_parse_file (parser, filename);
    yelp_man_parser_free (parser);

    if (doc == NULL) {
	yelp_set_error (&error, YELP_ERROR_NO_DOC);
	yelp_pager_error (pager, error);
	return FALSE;
    }

    g_signal_emit_by_name (pager, "contents");

    while (gtk_events_pending ())
	gtk_main_iteration ();

    params[i++] = "stylesheet_path";
    params[i++] = "\"file://" MAN_STYLESHEET_PATH "/\"";
    /*
    params[i++] = "color_gray_background";
    params[i++] = yelp_settings_get_color (YELP_COLOR_GRAY_BACKGROUND);
    params[i++] = "color_gray_border";
    params[i++] = yelp_settings_get_color (YELP_COLOR_GRAY_BORDER);
    */
    params[i++] = NULL;

    stylesheet = xsltParseStylesheetFile (MAN_STYLESHEET);
    tctxt      = xsltNewTransformContext (stylesheet,
					  doc);
    tctxt->_private = pager;
    xsltRegisterExtElement (tctxt,
			    "document",
			    YELP_NAMESPACE,
			    (xsltTransformFunction) xslt_yelp_document);

    while (gtk_events_pending ())
	gtk_main_iteration ();

    xsltApplyStylesheetUser (stylesheet,
			     doc,
			     params,
			     NULL, NULL,
			     tctxt);

    xmlFreeDoc (doc);
    xsltFreeStylesheet (stylesheet);

    g_free (filename);

    yelp_pager_set_state (pager, YELP_PAGER_STATE_FINISHED);
    g_signal_emit_by_name (pager, "finish");

    g_object_unref (pager);

    return FALSE;
}

void
man_pager_error (YelpPager   *pager)
{
    yelp_toc_pager_unpause (yelp_toc_pager_get ());
}

void
man_pager_cancel (YelpPager *pager)
{
    yelp_toc_pager_unpause (yelp_toc_pager_get ());
    // FIXME: actually cancel
}

void
man_pager_finish (YelpPager   *pager)
{
    yelp_toc_pager_unpause (yelp_toc_pager_get ());
}

const gchar *
man_pager_resolve_frag (YelpPager *pager, const gchar *frag_id)
{
    return "index";
}

GtkTreeModel *
man_pager_get_sections (YelpPager *pager)
{
    return NULL;
}

void
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
    xmlDocPtr   new_doc, old_doc;
    xmlNodePtr  old_insert;

    if (!ctxt || !node || !inst || !comp)
	return;

    while (gtk_events_pending ())
	gtk_main_iteration ();

    pager = (YelpPager *) ctxt->_private;

    page_id = xsltEvalAttrValueTemplate (ctxt, inst,
					  (const xmlChar *) "href",
					  NULL);
    if (page_id == NULL) {
	xsltTransformError (ctxt, NULL, inst,
			    _("No href attribute found on yelp:document"));
	error = NULL;
	yelp_pager_error (pager, error);

	return;
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

	return;
    }

    style->omitXmlDeclaration = TRUE;

    new_doc = xmlNewDoc ("1.0");
    new_doc->charset = XML_CHAR_ENCODING_UTF8;
    ctxt->output = new_doc;
    ctxt->insert = (xmlNodePtr) new_doc;

    xsltApplyOneTemplate (ctxt, node, inst->children, NULL, NULL);

    htmlDocDumpMemory (new_doc, &page_buf, &buf_size, 0);

    ctxt->outputFile = old_outfile;
    ctxt->output     = old_doc;
    ctxt->insert     = old_insert;

    page_title = xml_get_title (node);

    if (!page_title)
	page_title = g_strdup ("FIXME");

    page = g_new0 (YelpPage, 1);

    page->page_id  = page_id;
    page->title    = page_title;
    page->contents = page_buf;

    page->prev_id = NULL;
    page->next_id = NULL;
    page->toc_id  = NULL;

    yelp_pager_add_page (pager, page);
    g_signal_emit_by_name (pager, "page", page_id);

    while (gtk_events_pending ())
	gtk_main_iteration ();

    xmlFreeDoc (new_doc);
}

gchar *
xml_get_title (xmlNodePtr node)
{
    gchar *title = NULL;
    gchar *ret   = NULL;
    xmlNodePtr cur;

    if (xml_is_info (node))
	title = g_strdup (_("Titlepage"));
    else if (node->parent && node->parent->type == XML_DOCUMENT_NODE)
	title = g_strdup (_("Contents"));
    else {
	for (cur = node->children; cur; cur = cur->next) {
	    if (!xmlStrcmp (cur->name, (xmlChar *) "title")) {
		if (title)
		    g_free (title);
		title = xmlNodeGetContent (cur);
	    }
	    else if (!xmlStrcmp (cur->name, (xmlChar *) "titleabbrev")) {
		if (title)
		    g_free (title);
		title = xmlNodeGetContent (cur);
		break;
	    }
	}
    }

    if (!title)
	title = g_strdup (_("Unknown"));

    // This really isn't adequate for what we want.
    ret = g_strdup (g_strstrip (title));
    g_free (title);

    return ret;
}

gboolean
xml_is_info (xmlNodePtr node)
{
    return (!xmlStrcmp (node->name, (const xmlChar *) "appendixinfo")     ||
	    !xmlStrcmp (node->name, (const xmlChar *) "articleinfo")      ||
	    !xmlStrcmp (node->name, (const xmlChar *) "bookinfo")         ||
	    !xmlStrcmp (node->name, (const xmlChar *) "bibliographyinfo") ||
	    !xmlStrcmp (node->name, (const xmlChar *) "chapterinfo")      ||
	    !xmlStrcmp (node->name, (const xmlChar *) "glossaryinfo")     ||
	    !xmlStrcmp (node->name, (const xmlChar *) "indexinfo")        ||
	    !xmlStrcmp (node->name, (const xmlChar *) "partinfo")         ||
	    !xmlStrcmp (node->name, (const xmlChar *) "prefaceinfo")      ||
	    !xmlStrcmp (node->name, (const xmlChar *) "referenceinfo")    ||
	    !xmlStrcmp (node->name, (const xmlChar *) "refentryinfo")     ||
	    !xmlStrcmp (node->name, (const xmlChar *) "refsect1info")     ||
	    !xmlStrcmp (node->name, (const xmlChar *) "refsect2info")     ||
	    !xmlStrcmp (node->name, (const xmlChar *) "refsect3info")     ||
	    !xmlStrcmp (node->name, (const xmlChar *) "refsectioninfo")   ||
	    !xmlStrcmp (node->name, (const xmlChar *) "sect1info")        ||
	    !xmlStrcmp (node->name, (const xmlChar *) "sect2info")        ||
	    !xmlStrcmp (node->name, (const xmlChar *) "sect3info")        ||
	    !xmlStrcmp (node->name, (const xmlChar *) "sect4info")        ||
	    !xmlStrcmp (node->name, (const xmlChar *) "sect5info")        ||
	    !xmlStrcmp (node->name, (const xmlChar *) "sectioninfo")      ||
	    !xmlStrcmp (node->name, (const xmlChar *) "setinfo")          ||
	    !xmlStrcmp (node->name, (const xmlChar *) "setindexinfo")     );
}
