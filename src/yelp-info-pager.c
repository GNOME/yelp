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
#include "yelp-info-pager.h"
#include "yelp-settings.h"

#define INFO_STYLESHEET_PATH DATADIR"/sgml/docbook/yelp/"
#define INFO_STYLESHEET      INFO_STYLESHEET_PATH"info2html.xsl"

struct _YelpInfoPagerPriv {
    gpointer unused;
};

static void           info_pager_class_init   (YelpInfoPagerClass *klass);
static void           info_pager_init         (YelpInfoPager      *pager);
static void           info_pager_dispose      (GObject           *gobject);

static xmlDocPtr      info_pager_parse        (YelpPager        *pager);
static gchar **       info_pager_params       (YelpPager        *pager);

static const gchar *  info_pager_resolve_frag (YelpPager        *pager,
					      const gchar      *frag_id);
static GtkTreeModel * info_pager_get_sections (YelpPager        *pager);

static YelpPagerClass *parent_class;

GType
yelp_info_pager_get_type (void)
{
    static GType type = 0;

    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpInfoPagerClass),
	    NULL,
	    NULL,
	    (GClassInitFunc) info_pager_class_init,
	    NULL,
	    NULL,
	    sizeof (YelpInfoPager),
	    0,
	    (GInstanceInitFunc) info_pager_init,
	};
	type = g_type_register_static (YELP_TYPE_XSLT_PAGER,
				       "YelpInfoPager", 
				       &info, 0);
    }
    return type;
}

static void
info_pager_class_init (YelpInfoPagerClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);
    YelpPagerClass *pager_class  = YELP_PAGER_CLASS (klass);
    YelpXsltPagerClass *xslt_class = YELP_XSLT_PAGER_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = info_pager_dispose;

    pager_class->resolve_frag = info_pager_resolve_frag;
    pager_class->get_sections = info_pager_get_sections;

    xslt_class->parse  = info_pager_parse;
    xslt_class->params = info_pager_params;

    xslt_class->stylesheet = INFO_STYLESHEET;
}

static void
info_pager_init (YelpInfoPager *pager)
{
    YelpInfoPagerPriv *priv;

    priv = g_new0 (YelpInfoPagerPriv, 1);
    pager->priv = priv;
}

static void
info_pager_dispose (GObject *object)
{
    YelpInfoPager *pager = YELP_INFO_PAGER (object);

    g_free (pager->priv);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

YelpPager *
yelp_info_pager_new (YelpDocInfo *doc_info)
{
    YelpInfoPager *pager;

    g_return_val_if_fail (doc_info != NULL, NULL);

    pager = (YelpInfoPager *) g_object_new (YELP_TYPE_INFO_PAGER,
					    "document-info", doc_info,
					    NULL);

    return (YelpPager *) pager;
}

static xmlDocPtr
info_pager_parse (YelpPager *pager)
{
    YelpDocInfo   *doc_info;
    gchar         *filename;
    xmlDocPtr      doc;
    GError        *error;

    g_return_val_if_fail (YELP_IS_INFO_PAGER (pager), FALSE);

    doc_info = yelp_pager_get_doc_info (pager);
    filename = yelp_doc_info_get_filename (doc_info);

    g_object_ref (pager);

    /* DO STUFF HERE */

    g_object_unref (pager);

    return doc;
}

static gchar **
info_pager_params (YelpPager *pager)
{
    gchar **params;
    gint params_i = 0;
    gint params_max = 10;

    params = g_new0 (gchar *, params_max);

    params[params_i++] = "stylesheet_path";
    params[params_i++] = g_strdup_printf ("\"file://%s\"", INFO_STYLESHEET_PATH);

    params[params_i] = NULL;

    return params;
}

static const gchar *
info_pager_resolve_frag (YelpPager *pager, const gchar *frag_id)
{
    /* DO SOMETHING ELSE HERE */
    return NULL;
}

static GtkTreeModel *
info_pager_get_sections (YelpPager *pager)
{
    /* RETURN THE TREE STORE HERE */
    return NULL;
}
