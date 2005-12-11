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
#include "yelp-db-print-pager.h"
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

#define YELP_DB_PRINT_PAGER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_DB_PRINT_PAGER, YelpDBPrintPagerPriv))

struct _YelpDBPrintPagerPriv {
    gchar          *root_id;
};

static void           db_print_pager_class_init   (YelpDBPrintPagerClass *klass);
static void           db_print_pager_init         (YelpDBPrintPager      *pager);
static void           db_print_pager_dispose      (GObject          *gobject);

static void           db_print_pager_cancel       (YelpPager        *pager);
static xmlDocPtr      db_print_pager_parse        (YelpPager        *pager);
static gchar **       db_print_pager_params       (YelpPager        *pager);

static const gchar *  db_print_pager_resolve_frag (YelpPager        *pager,
					     const gchar      *frag_id);
static GtkTreeModel * db_print_pager_get_sections (YelpPager        *pager);

static YelpPagerClass *parent_class;

GType
yelp_db_print_pager_get_type (void)
{
    static GType type = 0;

    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpDBPrintPagerClass),
	    NULL,
	    NULL,
	    (GClassInitFunc) db_print_pager_class_init,
	    NULL,
	    NULL,
	    sizeof (YelpDBPrintPager),
	    0,
	    (GInstanceInitFunc) db_print_pager_init,
	};
	type = g_type_register_static (YELP_TYPE_XSLT_PAGER,
				       "YelpDBPrintPager", 
				       &info, 0);
    }
    return type;
}

static void
db_print_pager_class_init (YelpDBPrintPagerClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);
    YelpPagerClass *pager_class  = YELP_PAGER_CLASS (klass);
    YelpXsltPagerClass *xslt_class = YELP_XSLT_PAGER_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = db_print_pager_dispose;

    pager_class->cancel   = db_print_pager_cancel;

    pager_class->resolve_frag = db_print_pager_resolve_frag;

    xslt_class->parse  = db_print_pager_parse;
    xslt_class->params = db_print_pager_params;

    xslt_class->stylesheet = DB_STYLESHEET;

    g_type_class_add_private (klass, sizeof (YelpDBPrintPagerPriv));
}

static void
db_print_pager_init (YelpDBPrintPager *pager)
{
    YelpDBPrintPagerPriv *priv;

    pager->priv = priv = YELP_DB_PRINT_PAGER_GET_PRIVATE (pager);

    pager->priv->root_id = NULL;
}

static void
db_print_pager_dispose (GObject *object)
{
    YelpDBPrintPager *pager = YELP_DB_PRINT_PAGER (object);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

YelpPager *
yelp_db_print_pager_new (YelpDocInfo *doc_info)
{
    YelpDBPrintPager *pager;

    g_return_val_if_fail (doc_info != NULL, NULL);

    pager = (YelpDBPrintPager *) g_object_new (YELP_TYPE_DB_PRINT_PAGER,
					  "document-info", doc_info,
					  NULL);

    return (YelpPager *) pager;
}

static xmlDocPtr
db_print_pager_parse (YelpPager *pager)
{
    YelpDBPrintPagerPriv *priv;
    YelpDocInfo     *doc_info;
    gchar           *filename = NULL;

    xmlParserCtxtPtr parserCtxt = NULL;
    xmlDocPtr doc = NULL;

    xmlChar     *id;
    GError      *error = NULL;

    d (g_print ("db_print_pager_parse\n"));

    doc_info = yelp_pager_get_doc_info (pager);

    g_return_val_if_fail (pager != NULL, NULL);
    g_return_val_if_fail (YELP_IS_DB_PRINT_PAGER (pager), NULL);
    priv = YELP_DB_PRINT_PAGER (pager)->priv;

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


    priv->root_id = g_strdup ("index");

    EVENTS_PENDING;
    CANCEL_CHECK;

 done:
    g_free (filename);

    if (parserCtxt)
	xmlFreeParserCtxt (parserCtxt);

    g_object_unref (pager);

    return doc;
}

static gchar **
db_print_pager_params (YelpPager *pager)
{
    YelpDBPrintPagerPriv *priv;
    YelpDocInfo     *doc_info;
    gchar **params;
    gint params_i = 0;
    gint params_max = 20;

    d (g_print ("db_print_pager_process\n"));

    doc_info = yelp_pager_get_doc_info (pager);

    g_return_val_if_fail (pager != NULL, FALSE);
    g_return_val_if_fail (YELP_IS_DB_PRINT_PAGER (pager), FALSE);
    priv = YELP_DB_PRINT_PAGER (pager)->priv;

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
    params[params_i++] = g_strdup ("\"index\"");
    params[params_i++] = "db.chunk.max_depth";
    params[params_i++] = g_strdup_printf ("0");
    params[params_i++] = "yelp.javascript";
    params[params_i++] = g_strdup_printf ("\"%s\"", DATADIR "/yelp/yelp.js");

    params[params_i] = NULL;

    return params;
}

static void
db_print_pager_cancel (YelpPager *pager)
{
    YelpDBPrintPagerPriv *priv = YELP_DB_PRINT_PAGER (pager)->priv;

    d (g_print ("db_print_pager_cancel\n"));

    yelp_pager_set_state (pager, YELP_PAGER_STATE_INVALID);

    g_free (priv->root_id);
    priv->root_id = NULL;

    YELP_PAGER_CLASS (parent_class)->cancel (pager);
}

static const gchar *
db_print_pager_resolve_frag (YelpPager *pager, const gchar *frag_id)
{
    YelpDBPrintPager  *db_print_pager;

    g_return_val_if_fail (pager != NULL, NULL);
    g_return_val_if_fail (YELP_IS_DB_PRINT_PAGER (pager), NULL);

    db_print_pager = YELP_DB_PRINT_PAGER (pager);

    return frag_id;
}

