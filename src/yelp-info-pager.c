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
 *         Davyd Madeley  <davyd@madeley.id.au>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
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
#include "yelp-info-parser.h"
#include "yelp-settings.h"

#define STYLESHEET_PATH DATADIR"/yelp/xslt/"
#define INFO_STYLESHEET STYLESHEET_PATH"info2html.xsl"

#define YELP_INFO_PAGER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_INFO_PAGER, YelpInfoPagerPriv))

struct _YelpInfoPagerPriv {
    GtkTreeStore  *tree;
    GHashTable    *frags_hash;
};

typedef struct {
    gchar *frag_id;
    gchar *frag_name;
} hash_lookup;

static void           info_pager_class_init   (YelpInfoPagerClass *klass);
static void           info_pager_init         (YelpInfoPager      *pager);
static void           info_pager_dispose      (GObject           *gobject);

static xmlDocPtr      info_pager_parse        (YelpPager        *pager);
static gchar **       info_pager_params       (YelpPager        *pager);

static const gchar *  info_pager_resolve_frag (YelpPager        *pager,
					      const gchar       *frag_id);
static GtkTreeModel * info_pager_get_sections (YelpPager        *pager);

static gboolean       tree_hash_id            (GtkTreeModel     *model,
					       GtkTreePath      *path,
					       GtkTreeIter      *iter,
					       YelpInfoPager    *pager);
static gboolean       tree_find_id            (GtkTreeModel     *model,
					       GtkTreePath      *path,
					       GtkTreeIter      *iter,
					       hash_lookup      *hash);

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

    g_type_class_add_private (klass, sizeof (YelpInfoPagerPriv));
}

static void
info_pager_init (YelpInfoPager *pager)
{
    YelpInfoPagerPriv *priv;

    pager->priv = priv = YELP_INFO_PAGER_GET_PRIVATE (pager);

    /* In this hash, key == value */
    priv->frags_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static void
info_pager_dispose (GObject *object)
{
    YelpInfoPager *pager = YELP_INFO_PAGER (object);

    g_object_unref (pager->priv->tree);
    g_hash_table_destroy (pager->priv->frags_hash);

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
    xmlDocPtr      doc = NULL;
    GError        *error = NULL;
    YelpInfoPagerPriv *priv;

    g_return_val_if_fail (YELP_IS_INFO_PAGER (pager), FALSE);
    priv = YELP_INFO_PAGER (pager)->priv;

    doc_info = yelp_pager_get_doc_info (pager);
    filename = yelp_doc_info_get_filename (doc_info);

    g_object_ref (pager);

    /* DO STUFF HERE */
    /* parse into a GtkTreeStore */
    priv->tree = yelp_info_parser_parse_file (filename);

    if (!priv->tree) {
	g_set_error (&error, YELP_ERROR, YELP_ERROR_NO_DOC,
		     _("The file  ‘%s’ could not be parsed. Either the file "
		       "does not exist, or it is not a well-formed info page."),
		     filename);
	yelp_pager_error (pager, error);

    } else {

	/* create the XML file */
	doc = yelp_info_parser_parse_tree (priv->tree);
	
	gtk_tree_model_foreach (GTK_TREE_MODEL (priv->tree),
				(GtkTreeModelForeachFunc) tree_hash_id,
				pager);
    }

    g_object_unref (pager);
    g_free (filename);

    return doc;
}

static gchar **
info_pager_params (YelpPager *pager)
{
    gchar **params;
    gint params_i = 0;
    gint params_max = 10;

    params = g_new0 (gchar *, params_max);

    yelp_settings_params (&params, &params_i, &params_max);

    if ((params_i + 10) >= params_max - 1) {
	params_max += 20;
	params = g_renew (gchar *, params, params_max);
    }

    params[params_i++] = "stylesheet_path";
    params[params_i++] = g_strdup_printf ("\"file://%s\"", STYLESHEET_PATH);

    params[params_i] = NULL;

    return params;
}

static const gchar *
info_pager_resolve_frag (YelpPager *pager, const gchar *frag_id)
{
    g_return_val_if_fail (YELP_IS_INFO_PAGER (pager), NULL);
    if (frag_id) {
	gchar *id = g_hash_table_lookup (YELP_INFO_PAGER (pager)->priv->frags_hash,
					 frag_id);
	if (!id) {
	    hash_lookup *l = g_new0 (hash_lookup, 1);
	    
	    l->frag_name = g_strdup (frag_id);

	    gtk_tree_model_foreach (GTK_TREE_MODEL (YELP_INFO_PAGER (pager)->priv->tree),
				    (GtkTreeModelForeachFunc) tree_find_id,
				    l);
	    if (l->frag_id)
		id = g_hash_table_lookup (YELP_INFO_PAGER (pager)->priv->frags_hash,
					  l->frag_id);
	    g_free (l->frag_name);
	    g_free (l->frag_id);
	    g_free (l);
	}
	return id;
    } else
	return g_hash_table_lookup (YELP_INFO_PAGER (pager)->priv->frags_hash, "1");
}

static GtkTreeModel *
info_pager_get_sections (YelpPager *pager)
{
    g_return_val_if_fail (YELP_IS_INFO_PAGER (pager), NULL);

    return GTK_TREE_MODEL (YELP_INFO_PAGER (pager)->priv->tree);
}

static gboolean
tree_hash_id (GtkTreeModel   *model,
	      GtkTreePath    *path,
	      GtkTreeIter    *iter,
	      YelpInfoPager  *pager)
{
    YelpInfoPagerPriv *priv;
    gchar *id;

    priv = pager->priv;

    gtk_tree_model_get (model, iter,
			YELP_PAGER_COLUMN_ID, &id,
			-1);
    if (id)
	g_hash_table_replace (priv->frags_hash, id, id);

    return FALSE;
}

static gboolean
tree_find_id (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
	      hash_lookup *hash)
{
    gchar *title;
    gchar *id;
    gtk_tree_model_get (model, iter,
			YELP_PAGER_COLUMN_ID, &id,
			YELP_PAGER_COLUMN_TITLE, &title,
			-1);
    if (g_str_equal (title, hash->frag_name)) {
	hash->frag_id = g_strdup (id);
	return TRUE;
    }
    return FALSE;

}
