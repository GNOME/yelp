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
#include "yelp-db-pager.h"
#include "yelp-toc-pager.h"

#define YELP_NAMESPACE "http://www.gnome.org/yelp/ns"

#define DB_STYLESHEET_PATH DATADIR"/sgml/docbook/yelp/"
#define DB_STYLESHEET      DB_STYLESHEET_PATH"db2html.xsl"

#define MAX_CHUNK_DEPTH 2

#define d(x)

struct _YelpDBPagerPriv {
    GtkTreeModel   *sects;

    GHashTable     *frags_hash;
};

typedef struct _DBWalker DBWalker;
struct _DBWalker {
    YelpDBPager *pager;
    gchar       *page_id;

    xmlDocPtr  doc;
    xmlNodePtr cur;

    GtkTreeIter *iter;

    gint depth;
};

static void          db_pager_class_init   (YelpDBPagerClass *klass);
static void          db_pager_init         (YelpDBPager      *pager);
static void          db_pager_dispose      (GObject          *gobject);

void                 db_pager_error        (YelpPager   *pager);
void                 db_pager_cancel       (YelpPager   *pager);
void                 db_pager_finish       (YelpPager   *pager);

gboolean             db_pager_process      (YelpPager   *pager);
gchar *              db_pager_resolve_uri  (YelpPager   *pager,
					    YelpURI     *uri);
const GtkTreeModel * db_pager_get_sections (YelpPager   *pager);

static void     walker_walk_xml       (DBWalker         *walker);
gboolean        walker_is_chunk       (DBWalker         *walker);

gboolean        xml_is_division       (xmlNodePtr        node);
gboolean        xml_is_info           (xmlNodePtr        node);
gchar *         xml_get_title         (xmlNodePtr        node);

void            xslt_yelp_document    (xsltTransformContextPtr ctxt,
				       xmlNodePtr              node,
				       xmlNodePtr              inst,
				       xsltStylePreCompPtr     comp);
void            xslt_yelp_cache       (xsltTransformContextPtr ctxt,
				       xmlNodePtr              node,
				       xmlNodePtr              inst,
				       xsltStylePreCompPtr     comp);

static YelpPagerClass *parent_class;

GType
yelp_db_pager_get_type (void)
{
    static GType type = 0;

    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpDBPagerClass),
	    NULL,
	    NULL,
	    (GClassInitFunc) db_pager_class_init,
	    NULL,
	    NULL,
	    sizeof (YelpDBPager),
	    0,
	    (GInstanceInitFunc) db_pager_init,
	};
	type = g_type_register_static (YELP_TYPE_PAGER,
				       "YelpDBPager", 
				       &info, 0);
    }
    return type;
}

static void
db_pager_class_init (YelpDBPagerClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);
    YelpPagerClass *pager_class  = YELP_PAGER_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = db_pager_dispose;

    pager_class->error        = db_pager_error;
    pager_class->cancel       = db_pager_cancel;
    pager_class->finish       = db_pager_finish;

    pager_class->process      = db_pager_process;
    pager_class->resolve_uri  = db_pager_resolve_uri;
    pager_class->get_sections = db_pager_get_sections;
}

static void
db_pager_init (YelpDBPager *pager)
{
    YelpDBPagerPriv *priv;

    priv = g_new0 (YelpDBPagerPriv, 1);
    pager->priv = priv;

    pager->priv->sects = NULL;

    pager->priv->frags_hash =
	g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

static void
db_pager_dispose (GObject *object)
{
    YelpDBPager *pager = YELP_DB_PAGER (object);

    g_object_unref (pager->priv->sects);

    g_hash_table_destroy (pager->priv->frags_hash);

    g_free (pager->priv);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

YelpPager *
yelp_db_pager_new (YelpURI *uri)
{
    YelpDBPager *pager;

    g_return_val_if_fail (YELP_IS_URI (uri), NULL);

    pager = (YelpDBPager *) g_object_new (YELP_TYPE_DB_PAGER,
					  "uri", uri,
					  NULL);

    pager->priv->sects =
	GTK_TREE_MODEL (gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING));

    return (YelpPager *) pager;
}

gboolean
db_pager_process (YelpPager *pager)
{
    YelpURI *uri           = yelp_pager_get_uri (pager);
    gchar         *uri_str = yelp_uri_get_path ((YelpURI *) uri);
    DBWalker      *walker;
    GError        *error = NULL;

    xmlDocPtr               doc;
    xmlParserCtxtPtr        ctxt;
    xsltStylesheetPtr       stylesheet;
    xsltTransformContextPtr tctxt;

    gchar        *uri_slash;
    gchar        *doc_name;
    gchar        *doc_path;
    const gchar  *params[15];

    g_return_val_if_fail (pager != NULL, FALSE);
    g_return_val_if_fail (YELP_IS_DB_PAGER (pager), FALSE);

    g_object_ref (pager);

    yelp_toc_pager_pause (yelp_toc_pager_get ());

    ctxt = xmlNewParserCtxt ();
    doc = xmlCtxtReadFile (ctxt,
			   (const char *) uri_str,
			   NULL,
			   XML_PARSE_DTDLOAD  |
			   XML_PARSE_XINCLUDE |
			   XML_PARSE_NOCDATA  |
			   XML_PARSE_NOENT    |
			   XML_PARSE_NONET    );

    if (doc == NULL) {
	gchar *str_uri = yelp_uri_to_string (uri);
	g_set_error (&error,
		     YELP_ERROR,
		     YELP_ERROR_FAILED_OPEN,
		     _("The document '%s' could not be opened"), str_uri);
	yelp_pager_error (pager, error);

	g_free (str_uri);
	return FALSE;
    }

    walker = g_new0 (DBWalker, 1);
    walker->pager = YELP_DB_PAGER (pager);
    walker->doc   = doc;
    walker->cur   = xmlDocGetRootElement (walker->doc);

    while (gtk_events_pending ())
	gtk_main_iteration ();

    walker_walk_xml (walker);

    while (gtk_events_pending ())
	gtk_main_iteration ();

    uri_slash = g_strrstr (uri_str, "/");
    doc_name  = g_strndup (uri_str, uri_slash - uri_str + 1);
    doc_path  = g_strdup  (uri_slash + 1);

    params[0]  = "doc_name";
    params[1]  = g_strconcat("\"", doc_name, "\"", NULL) ;
    params[2]  = "doc_path";
    params[3]  = g_strconcat("\"file://", doc_path, "/\"", NULL) ;
    params[4]  = "stylesheet_path";
    params[5]  = g_strconcat("\"file://", DB_STYLESHEET_PATH, "/\"", NULL) ;
    params[6]  = "chunk_depth";
    params[7]  = "2";
    params[8]  = "html_extension";
    params[9]  = "\"\"";
    params[10] = "resolve_xref_chunk";
    params[11] = "0";
    params[12] = "mediaobject_path";
    params[13] = doc_path;
    params[14] = NULL;

    stylesheet = xsltParseStylesheetFile (DB_STYLESHEET);
    tctxt      = xsltNewTransformContext (stylesheet,
					  doc);
    tctxt->_private = pager;
    xsltRegisterExtElement (tctxt,
			    "document",
			    YELP_NAMESPACE,
			    (xsltTransformFunction) xslt_yelp_document);
    xsltRegisterExtElement (tctxt,
			    "cache",
			    YELP_NAMESPACE,
			    (xsltTransformFunction) xslt_yelp_cache);

    while (gtk_events_pending ())
	gtk_main_iteration ();

    xsltApplyStylesheetUser (stylesheet,
			     doc,
			     params,
			     NULL, NULL,
			     tctxt);

    xmlFreeDoc (doc);
    xsltFreeStylesheet (stylesheet);
    xmlFreeParserCtxt (ctxt);

    g_free (doc_name);
    g_free (doc_path);

    yelp_pager_set_state (pager, YELP_PAGER_STATE_FINISH);
    g_signal_emit_by_name (pager, "finish");

    g_object_unref (pager);

    return FALSE;
}

void
db_pager_error (YelpPager   *pager)
{
    yelp_toc_pager_unpause (yelp_toc_pager_get ());
}

void
db_pager_cancel (YelpPager *pager)
{
    yelp_toc_pager_unpause (yelp_toc_pager_get ());
    // FIXME: actually cancel
}

void
db_pager_finish (YelpPager   *pager)
{
    yelp_toc_pager_unpause (yelp_toc_pager_get ());
}

gchar *
db_pager_resolve_uri (YelpPager *pager, YelpURI *uri)
{
    YelpDBPager *db_pager;
    gchar       *frag_id;
    gchar       *page_id;

    g_return_val_if_fail (pager != NULL, NULL);
    g_return_val_if_fail (YELP_IS_DB_PAGER (pager), NULL);

    db_pager = YELP_DB_PAGER (pager);

    frag_id = yelp_uri_get_fragment (uri);

    if (frag_id)
	page_id = g_hash_table_lookup (db_pager->priv->frags_hash,
				       frag_id);
    else
	page_id = g_strdup ("index");

    g_free (frag_id);
    return page_id;
}

const GtkTreeModel *
db_pager_get_sections (YelpPager *pager)
{
    g_return_val_if_fail (pager != NULL, NULL);
    g_return_val_if_fail (YELP_IS_DB_PAGER (pager), NULL);

    return YELP_DB_PAGER (pager)->priv->sects;
}

void
xslt_yelp_document (xsltTransformContextPtr ctxt,
		    xmlNodePtr              node,
		    xmlNodePtr              inst,
		    xsltStylePreCompPtr     comp)
{
    GError  *error;
    xmlChar *page_id = NULL;
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

    xmlDocDumpFormatMemory (new_doc, &page_buf, &buf_size, 0);

    ctxt->outputFile = old_outfile;
    ctxt->output     = old_doc;
    ctxt->insert     = old_insert;

    // FIXME
    yelp_pager_add_page (pager, page_id, g_strdup ("FIXME"), page_buf);
    g_signal_emit_by_name (pager, "page", page_id);

    while (gtk_events_pending ())
	gtk_main_iteration ();

    xmlFreeDoc (new_doc);
}

void
xslt_yelp_cache (xsltTransformContextPtr ctxt,
		 xmlNodePtr              node,
		 xmlNodePtr              inst,
		 xsltStylePreCompPtr     comp)
{
}

static void
walker_walk_xml (DBWalker *walker)
{
    xmlChar     *id    = NULL;
    xmlChar     *title = NULL;
    gchar       *old_id;
    xmlNodePtr   cur;
    xmlNodePtr   old_cur;
    GtkTreeIter  iter;
    GtkTreeIter *old_iter;
    YelpDBPagerPriv *priv = walker->pager->priv;

    id = xmlGetProp (walker->cur, "id");

    if (walker_is_chunk (walker)) {
	if (xml_is_info (walker->cur)) {
	    xmlFree (id);
	    id = g_strdup ("titlepage");
	}

	title = xml_get_title (walker->cur);

	if (id) {
	    gtk_tree_store_append (GTK_TREE_STORE (priv->sects),
				   &iter, walker->iter);

	    gtk_tree_store_set (GTK_TREE_STORE (priv->sects),
				&iter,
				0, g_strdup (id),
				1, g_strdup (title),
				-1);

	    old_id          = walker->page_id;
	    walker->page_id = id;

	    old_iter     = walker->iter;
	    if (!xml_is_info (walker->cur) &&
		walker->cur->parent->type != XML_DOCUMENT_NODE) {

	    walker->iter = &iter;
	    }
	}
    }

    while (gtk_events_pending ())
	gtk_main_iteration ();

    old_cur = walker->cur;
    walker->depth++;

    if (id) {
	g_hash_table_insert (priv->frags_hash,
			     g_strdup (id),
			     g_strdup (walker->page_id));
    }

    cur = walker->cur->children;
    while (cur != NULL) {
	if (cur->type == XML_ELEMENT_NODE) {
	    walker->cur = cur;
	    walker_walk_xml (walker);
	}
	cur = cur->next;
    }

    walker->depth--;
    walker->cur = old_cur;

    if (walker_is_chunk (walker) && id) {
	walker->iter     = old_iter;

	walker->page_id = old_id;
    }

    xmlFree (id);
    xmlFree (title);
}

gchar *
xml_get_title (xmlNodePtr node)
{
    gchar *title = NULL;
    xmlNodePtr cur;

    if (xml_is_info (node))
	title = _("Titlepage");
    else if (node->parent->type == XML_DOCUMENT_NODE)
	title = _("Contents");
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
	title = _("Unknown");

    return title;
}

gboolean
walker_is_chunk (DBWalker *walker)
{
    if (walker->depth <= MAX_CHUNK_DEPTH) {
	if (xml_is_division (walker->cur))
	    return TRUE;
	else if (walker->depth == 1 && xml_is_info (walker->cur))
	    return TRUE;
    }
    return FALSE;
}

gboolean
xml_is_division (xmlNodePtr node)
{
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
