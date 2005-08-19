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
#include <glib/gi18n.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xinclude.h>
#include <libxslt/xslt.h>
#include <libxslt/templates.h>
#include <libxslt/transform.h>
#include <libxslt/extensions.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/xsltutils.h>

#include "yelp-error.h"
#include "yelp-db-pager.h"
#include "yelp-toc-pager.h"
#include "yelp-settings.h"

#ifdef YELP_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define STYLESHEET_PATH DATADIR"/yelp/xslt/"
#define DB_STYLESHEET   STYLESHEET_PATH"db2html.xsl"
#define DB_TITLE        STYLESHEET_PATH"db-title.xsl"

#define BOOK_CHUNK_DEPTH 2
#define ARTICLE_CHUNK_DEPTH 1

#define EVENTS_PENDING while (yelp_pager_get_state (pager) <= YELP_PAGER_STATE_RUNNING && gtk_events_pending ()) gtk_main_iteration ();
#define CANCEL_CHECK if (!main_running || yelp_pager_get_state (pager) >= YELP_PAGER_STATE_ERROR) goto done;

extern gboolean main_running;

#define YELP_DB_PAGER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_DB_PAGER, YelpDBPagerPriv))

struct _YelpDBPagerPriv {
    GtkTreeModel   *sects;
    GHashTable     *frags_hash;

    gint            max_depth;
    gchar          *root_id;
};

typedef struct _DBWalker DBWalker;
struct _DBWalker {
    YelpDBPager *pager;
    gchar       *page_id;

    xmlDocPtr  doc;
    xmlNodePtr cur;

    GtkTreeIter *iter;

    gint depth;
    gint max_depth;

    xsltStylesheetPtr       titleStylesheet;
};

static void           db_pager_class_init   (YelpDBPagerClass *klass);
static void           db_pager_init         (YelpDBPager      *pager);
static void           db_pager_dispose      (GObject          *gobject);

static void           db_pager_cancel       (YelpPager        *pager);
static xmlDocPtr      db_pager_parse        (YelpPager        *pager);
static gchar **       db_pager_params       (YelpPager        *pager);

static const gchar *  db_pager_resolve_frag (YelpPager        *pager,
					     const gchar      *frag_id);
static GtkTreeModel * db_pager_get_sections (YelpPager        *pager);

static void            walker_walk_xml      (DBWalker         *walker);

static gboolean        node_is_chunk        (DBWalker         *walker);
static gboolean        node_is_division     (DBWalker         *walker);
static gchar *         node_get_title       (DBWalker         *walker);

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
	type = g_type_register_static (YELP_TYPE_XSLT_PAGER,
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
    YelpXsltPagerClass *xslt_class = YELP_XSLT_PAGER_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = db_pager_dispose;

    pager_class->cancel   = db_pager_cancel;

    pager_class->resolve_frag = db_pager_resolve_frag;
    pager_class->get_sections = db_pager_get_sections;

    xslt_class->parse  = db_pager_parse;
    xslt_class->params = db_pager_params;

    xslt_class->stylesheet = DB_STYLESHEET;

    g_type_class_add_private (klass, sizeof (YelpDBPagerPriv));
}

static void
db_pager_init (YelpDBPager *pager)
{
    YelpDBPagerPriv *priv;

    pager->priv = priv = YELP_DB_PAGER_GET_PRIVATE (pager);

    pager->priv->sects = NULL;

    pager->priv->frags_hash =
	g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    pager->priv->root_id = NULL;
}

static void
db_pager_dispose (GObject *object)
{
    YelpDBPager *pager = YELP_DB_PAGER (object);

    g_object_unref (pager->priv->sects);
    g_hash_table_destroy (pager->priv->frags_hash);

    g_free (pager->priv->root_id);
    g_free (pager->priv);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

YelpPager *
yelp_db_pager_new (YelpDocInfo *doc_info)
{
    YelpDBPager *pager;

    g_return_val_if_fail (doc_info != NULL, NULL);

    pager = (YelpDBPager *) g_object_new (YELP_TYPE_DB_PAGER,
					  "document-info", doc_info,
					  NULL);

    g_hash_table_insert (pager->priv->frags_hash,
			 g_strdup ("x-yelp-titlepage"),
			 g_strdup ("x-yelp-titlepage"));

    if (!pager->priv->sects)
	pager->priv->sects =
	    GTK_TREE_MODEL (gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING));

    return (YelpPager *) pager;
}

static xmlDocPtr
db_pager_parse (YelpPager *pager)
{
    YelpDBPagerPriv *priv;
    YelpDocInfo     *doc_info;
    gchar           *filename = NULL;

    xmlParserCtxtPtr parserCtxt = NULL;
    xmlDocPtr doc = NULL;

    DBWalker    *walker = NULL;
    xmlChar     *id;
    GError      *error = NULL;

    d (g_print ("db_pager_parse\n"));

    doc_info = yelp_pager_get_doc_info (pager);

    g_return_val_if_fail (pager != NULL, NULL);
    g_return_val_if_fail (YELP_IS_DB_PAGER (pager), NULL);
    priv = YELP_DB_PAGER (pager)->priv;

    g_object_ref (pager);

    if (yelp_pager_get_state (pager) >= YELP_PAGER_STATE_ERROR)
	goto done;

    filename = yelp_doc_info_get_filename (doc_info);

    parserCtxt = xmlNewParserCtxt ();
    doc = xmlCtxtReadFile (parserCtxt,
			   (const char *) filename, NULL,
			   XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
			   XML_PARSE_NOENT   | XML_PARSE_NONET   );
    if (doc == NULL) {
	g_set_error (&error, YELP_ERROR, YELP_ERROR_NO_DOC,
		     _("The file ‘%s’ could not be parsed. Either the file "
		       "does not exist, or it is not well-formed XML."),
		     filename);
	yelp_pager_error (pager, error);
	goto done;
    }

    xmlXIncludeProcessFlags (doc,
			     XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
			     XML_PARSE_NOENT   | XML_PARSE_NONET   );

    walker = g_new0 (DBWalker, 1);
    walker->pager     = YELP_DB_PAGER (pager);
    walker->doc       = doc;
    walker->cur       = xmlDocGetRootElement (walker->doc);

    walker->titleStylesheet = xsltParseStylesheetFile (DB_TITLE);

    if (!xmlStrcmp (xmlDocGetRootElement (doc)->name, BAD_CAST "book"))
	priv->max_depth = walker->max_depth = BOOK_CHUNK_DEPTH;
    else
	priv->max_depth = walker->max_depth = ARTICLE_CHUNK_DEPTH;

    id = xmlGetProp (walker->cur, "id");
    if (id)
	priv->root_id = g_strdup (id);
    else
	priv->root_id = g_strdup ("index");

    EVENTS_PENDING;
    CANCEL_CHECK;

    walker_walk_xml (walker);

 done:
    if (walker) {
	if (walker->titleStylesheet)
	    xsltFreeStylesheet (walker->titleStylesheet);
	g_free (walker);
    }

    g_free (filename);

    if (parserCtxt)
	xmlFreeParserCtxt (parserCtxt);

    g_object_unref (pager);

    return doc;
}

static gchar **
db_pager_params (YelpPager *pager)
{
    YelpDBPagerPriv *priv;
    YelpDocInfo     *doc_info;
    gchar **params;
    gint params_i = 0;
    gint params_max = 20;

    d (g_print ("db_pager_process\n"));

    doc_info = yelp_pager_get_doc_info (pager);

    g_return_val_if_fail (pager != NULL, FALSE);
    g_return_val_if_fail (YELP_IS_DB_PAGER (pager), FALSE);
    priv = YELP_DB_PAGER (pager)->priv;

    if (yelp_pager_get_state (pager) >= YELP_PAGER_STATE_ERROR)
	return NULL;

    params = g_new0 (gchar *, params_max);

    yelp_settings_params (&params, &params_i, &params_max);

    if ((params_i + 10) >= params_max - 1) {
	params_max += 20;
	params = g_renew (gchar *, params, params_max);
    }
    params[params_i++] = "db.chunk.extension";
    params[params_i++] = g_strdup ("\"\"");
    params[params_i++] = "db.chunk.info_basename";
    params[params_i++] = g_strdup ("\"x-yelp-titlepage\"");
    params[params_i++] = "db.chunk.max_depth";
    params[params_i++] = g_strdup_printf ("%i", priv->max_depth);
    params[params_i++] = "yelp.javascript";
    params[params_i++] = g_strdup_printf ("\"%s\"", DATADIR "/yelp/yelp.js");

    params[params_i] = NULL;

    return params;
}

static void
db_pager_cancel (YelpPager *pager)
{
    YelpDBPagerPriv *priv = YELP_DB_PAGER (pager)->priv;

    d (g_print ("db_pager_cancel\n"));

    yelp_pager_set_state (pager, YELP_PAGER_STATE_INVALID);

    gtk_tree_store_clear (GTK_TREE_STORE (priv->sects));
    g_hash_table_foreach_remove (priv->frags_hash, (GHRFunc) gtk_true, NULL);

    g_free (priv->root_id);
    priv->root_id = NULL;

    YELP_PAGER_CLASS (parent_class)->cancel (pager);
}

static const gchar *
db_pager_resolve_frag (YelpPager *pager, const gchar *frag_id)
{
    YelpDBPager  *db_pager;
    gchar        *page_id = NULL;

    g_return_val_if_fail (pager != NULL, NULL);
    g_return_val_if_fail (YELP_IS_DB_PAGER (pager), NULL);

    db_pager = YELP_DB_PAGER (pager);

    if (frag_id)
	page_id = g_hash_table_lookup (db_pager->priv->frags_hash,
				       frag_id);
    else
	page_id = db_pager->priv->root_id;

    return (const gchar *) page_id;
}

static GtkTreeModel *
db_pager_get_sections (YelpPager *pager)
{
    g_return_val_if_fail (pager != NULL, NULL);
    g_return_val_if_fail (YELP_IS_DB_PAGER (pager), NULL);

    return YELP_DB_PAGER (pager)->priv->sects;
}

/******************************************************************************/

static void
walker_walk_xml (DBWalker *walker)
{
    xmlChar     *id    = NULL;
    xmlChar     *title = NULL;
    gchar       *old_id = NULL;
    xmlNodePtr   cur;
    xmlNodePtr   old_cur;
    GtkTreeIter  iter;
    GtkTreeIter *old_iter = NULL;
    YelpDBPagerPriv *priv = walker->pager->priv;

    if (walker->depth == 0) {
	cur = walker->cur;
	for ( ; cur; cur = cur->prev)
	    if (cur->type == XML_PI_NODE)
		if (!xmlStrcmp (cur->name,
				(const xmlChar *) "yelp:chunk-depth")) {
		    gint max = atoi (cur->content);
		    if (max)
			priv->max_depth = walker->max_depth = max;
		    break;
		}
    }

    id = xmlGetProp (walker->cur, "id");
    if (!id && walker->cur->parent->type == XML_DOCUMENT_NODE) {
	id = xmlStrdup ("__yelp_toc");
    }

    if (node_is_chunk (walker)) {
	title = node_get_title (walker);

	if (id) {
	    gtk_tree_store_append (GTK_TREE_STORE (priv->sects),
				   &iter, walker->iter);

	    gtk_tree_store_set (GTK_TREE_STORE (priv->sects),
				&iter,
				YELP_PAGER_COLUMN_ID, id,
				YELP_PAGER_COLUMN_TITLE, title,
				-1);

	    old_id          = walker->page_id;
	    walker->page_id = id;

	    old_iter     = walker->iter;

	    if (walker->cur->parent->type != XML_DOCUMENT_NODE)
		walker->iter = &iter;
	}
    }

    while (gtk_events_pending ())
	gtk_main_iteration ();
    /* FIXME : check for cancel */

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

    if (node_is_chunk (walker) && id) {
	walker->iter     = old_iter;

	walker->page_id = old_id;
    }

    if (id != NULL)
	xmlFree (id);
    if (title != NULL)
	xmlFree (title);
}

static gchar *
node_get_title (DBWalker *walker)
{
    gchar *title;
    xmlDocPtr doc;
    xmlChar *params[3];
    xmlChar *outstr;
    int outlen;

    params[0] = "node";
    params[1] = xmlGetNodePath (walker->cur);
    params[2] = NULL;

    doc = xsltApplyStylesheet (walker->titleStylesheet,
			       walker->doc,
			       (const char **)params);
    if (doc == NULL || xsltSaveResultToString (&outstr, &outlen, doc, walker->titleStylesheet) < 0)
	title = g_strdup (_("Unknown Section"));
    else {
	title = g_strdup (outstr);
	xmlFree (outstr);
    }

    xmlFreeDoc (doc);
    xmlFree (params[1]);

    return title;
}

static gboolean
node_is_chunk (DBWalker *walker)
{
    if (walker->depth <= walker->max_depth) {
	if (node_is_division (walker))
	    return TRUE;
    }
    return FALSE;
}

static gboolean
node_is_division (DBWalker *walker)
{
    xmlNodePtr node = walker->cur;
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
