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
    gpointer unused;
};

static void           man_pager_class_init   (YelpManPagerClass *klass);
static void           man_pager_init         (YelpManPager      *pager);
static void           man_pager_dispose      (GObject           *gobject);

static xmlDocPtr      man_pager_parse        (YelpPager        *pager);
static gchar **       man_pager_params       (YelpPager        *pager);

static const gchar *  man_pager_resolve_frag (YelpPager        *pager,
					      const gchar      *frag_id);
static GtkTreeModel * man_pager_get_sections (YelpPager        *pager);

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
	type = g_type_register_static (YELP_TYPE_XSLT_PAGER,
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
    YelpXsltPagerClass *xslt_class = YELP_XSLT_PAGER_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = man_pager_dispose;

    pager_class->resolve_frag = man_pager_resolve_frag;
    pager_class->get_sections = man_pager_get_sections;

    xslt_class->parse  = man_pager_parse;
    xslt_class->params = man_pager_params;

    xslt_class->stylesheet = MAN_STYLESHEET;
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

static xmlDocPtr
man_pager_parse (YelpPager *pager)
{
    YelpDocInfo   *doc_info;
    gchar         *filename;
    YelpManParser *parser;
    xmlDocPtr      doc;
    GError        *error;

    g_return_val_if_fail (YELP_IS_MAN_PAGER (pager), FALSE);

    doc_info = yelp_pager_get_doc_info (pager);
    filename = yelp_doc_info_get_filename (doc_info);

    g_object_ref (pager);

    parser = yelp_man_parser_new ();
    doc = yelp_man_parser_parse_file (parser, filename);
    yelp_man_parser_free (parser);

    if (doc == NULL) {
	yelp_set_error (&error, YELP_ERROR_NO_DOC);
	yelp_pager_error (pager, error);
    }

    g_object_unref (pager);

    return doc;
}

static gchar **
man_pager_params (YelpPager *pager)
{
    gchar **params;
    gint params_i = 0;
    gint params_max = 10;

    params = g_new0 (gchar *, params_max);

    params[params_i++] = "stylesheet_path";
    params[params_i++] = g_strdup_printf ("\"file://%s\"", MAN_STYLESHEET_PATH);

    params[params_i] = NULL;

    return params;
}

static const gchar *
man_pager_resolve_frag (YelpPager *pager, const gchar *frag_id)
{
    return "index";
}

static GtkTreeModel *
man_pager_get_sections (YelpPager *pager)
{
    return NULL;
}
