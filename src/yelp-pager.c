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

#include "yelp-pager.h"
#include "yelp-marshal.h"

#include <string.h>

#ifdef YELP_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define YELP_PAGER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_PAGER, YelpPagerPriv))

struct _YelpPagerPriv {
    YelpDocInfo     *doc_info;
    YelpPagerState   state;

    GError          *error;

    GHashTable      *page_hash;
};

enum {
    PROP_0,
    PROP_DOCINFO
};

enum {
    PARSE,
    RUN,
    PAGE,
    FINISH,
    CANCEL,
    ERROR,
    LAST_SIGNAL
};

static void    pager_class_init        (YelpPagerClass  *klass);
static void    pager_init              (YelpPager       *pager);
static void    pager_set_property      (GObject         *object,
					guint            prop_id,
					const GValue    *value,
					GParamSpec      *pspec);
static void    pager_get_property      (GObject         *object,
					guint            prop_id,
					GValue          *value,
					GParamSpec      *pspec);
static void    pager_dispose           (GObject         *object);


static GObjectClass *parent_class;
static gint          signals[LAST_SIGNAL] = { 0 };

GType
yelp_pager_get_type (void)
{
    static GType type = 0;

    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpPagerClass),
	    NULL,
	    NULL,
	    (GClassInitFunc) pager_class_init,
	    NULL,
	    NULL,
	    sizeof (YelpPager),
	    0,
	    (GInstanceInitFunc) pager_init,
	};
	type = g_type_register_static (G_TYPE_OBJECT,
				       "YelpPager", 
				       &info, 0);
    }
    return type;
}

static void
pager_class_init (YelpPagerClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->set_property = pager_set_property;
    object_class->get_property = pager_get_property;
    object_class->dispose      = pager_dispose;

    g_object_class_install_property
	(object_class,
	 PROP_DOCINFO,
	 g_param_spec_pointer ("document-info",
			       _("Document Information"),
			       _("The YelpDocInfo struct of the document"),
			       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    signals[PARSE] = g_signal_new
	("parse",
	 G_TYPE_FROM_CLASS (klass),
	 G_SIGNAL_RUN_FIRST, 0,
	 NULL, NULL,
	 yelp_marshal_VOID__VOID,
	 G_TYPE_NONE, 0);

    signals[RUN] = g_signal_new
	("start",
	 G_TYPE_FROM_CLASS (klass),
	 G_SIGNAL_RUN_FIRST, 0,
	 NULL, NULL,
	 yelp_marshal_VOID__VOID,
	 G_TYPE_NONE, 0);

    signals[PAGE] = g_signal_new
	("page",
	 G_TYPE_FROM_CLASS (klass),
	 G_SIGNAL_RUN_FIRST, 0,
	 NULL, NULL,
	 yelp_marshal_VOID__STRING,
	 G_TYPE_NONE, 1,
	 G_TYPE_STRING);

    signals[FINISH] = g_signal_new
	("finish",
	 G_TYPE_FROM_CLASS (klass),
	 G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (YelpPagerClass, finish),
	 NULL, NULL,
	 yelp_marshal_VOID__VOID,
	 G_TYPE_NONE, 0);

    signals[CANCEL] = g_signal_new
	("cancel",
	 G_TYPE_FROM_CLASS (klass),
	 G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (YelpPagerClass, cancel),
	 NULL, NULL,
	 yelp_marshal_VOID__VOID,
	 G_TYPE_NONE, 0);

    signals[ERROR] = g_signal_new
	("error",
	 G_TYPE_FROM_CLASS (klass),
	 G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (YelpPagerClass, error),
	 NULL, NULL,
	 yelp_marshal_VOID__VOID,
	 G_TYPE_NONE, 0);

    g_type_class_add_private (klass, sizeof (YelpPagerPriv));
}

static void
pager_init (YelpPager *pager)
{
    YelpPagerPriv *priv;

    pager->priv = priv = YELP_PAGER_GET_PRIVATE (pager);

    priv->doc_info = NULL;
    priv->state = YELP_PAGER_STATE_NEW;

    priv->error = NULL;
    priv->page_hash =
	g_hash_table_new_full (g_str_hash,
			       g_str_equal,
			       NULL, /* Use page->page_id directly */
			       (GDestroyNotify) yelp_page_free);
}

static void
pager_set_property (GObject      *object,
		    guint         prop_id,
		    const GValue *value,
		    GParamSpec   *pspec)
{
    YelpPager *pager = YELP_PAGER (object);

    switch (prop_id) {
    case PROP_DOCINFO:
	if (pager->priv->doc_info)
	    yelp_doc_info_unref (pager->priv->doc_info);
	pager->priv->doc_info =
	    (YelpDocInfo *) g_value_get_pointer (value);
	yelp_doc_info_ref (pager->priv->doc_info);
	break;
    default:
	break;
    }
}

static void
pager_get_property (GObject      *object,
		    guint         prop_id,
		    GValue       *value,
		    GParamSpec   *pspec)
{
    YelpPager *pager = YELP_PAGER (object);

    switch (prop_id) {
    case PROP_DOCINFO:
	g_value_set_pointer (value, pager->priv->doc_info);
	break;
    default:
	break;
    }
}

static void
pager_dispose (GObject *object)
{
    YelpPager *pager = YELP_PAGER (object);

    if (pager->priv->doc_info)
	yelp_doc_info_unref (pager->priv->doc_info);

    if (pager->priv->error)
	g_error_free (pager->priv->error);

    g_hash_table_destroy (pager->priv->page_hash);

    g_free (pager->priv);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

gboolean
yelp_pager_start (YelpPager *pager)
{
    g_return_val_if_fail (pager != NULL, FALSE);
    g_return_val_if_fail (YELP_IS_PAGER (pager), FALSE);
    g_return_val_if_fail (pager->priv->state == YELP_PAGER_STATE_NEW ||
			  pager->priv->state == YELP_PAGER_STATE_INVALID,
			  FALSE);

    pager->priv->state = YELP_PAGER_STATE_STARTED;
    gtk_idle_add ((GtkFunction) (YELP_PAGER_GET_CLASS (pager)->process),
		  pager);

    return TRUE;
}

void
yelp_pager_cancel (YelpPager *pager)
{
    g_return_if_fail (pager != NULL);
    g_return_if_fail (YELP_IS_PAGER (pager));

    d (g_print ("yelp_pager_cancel\n"));

    yelp_pager_set_state (pager, YELP_PAGER_STATE_INVALID);

    g_signal_emit (pager, signals[CANCEL], 0);
}

YelpDocInfo *
yelp_pager_get_doc_info (YelpPager *pager)
{
    g_return_val_if_fail (YELP_IS_PAGER (pager), NULL);

    return pager->priv->doc_info;
}

YelpPagerState
yelp_pager_get_state (YelpPager *pager)
{
    g_return_val_if_fail (pager != NULL, 0);
    g_return_val_if_fail (YELP_IS_PAGER (pager), 0);

    return pager->priv->state;
}

void
yelp_pager_set_state (YelpPager *pager, YelpPagerState state)
{
    g_return_if_fail (pager != NULL);
    g_return_if_fail (YELP_IS_PAGER (pager));

    pager->priv->state = state;
}

GError *
yelp_pager_get_error (YelpPager *pager)
{
    GError *error;

    g_return_val_if_fail (pager != NULL, NULL);
    g_return_val_if_fail (YELP_IS_PAGER (pager), NULL);

    if (pager->priv->error)
	error = g_error_copy (pager->priv->error);
    else
	error = NULL;

    return error;
}

void
yelp_pager_error (YelpPager *pager, GError *error)
{
    d (g_print ("yelp_pager_error\n"));

    if (pager->priv->error)
	g_error_free (pager->priv->error);
    pager->priv->error = error;

    yelp_pager_set_state (pager, YELP_PAGER_STATE_ERROR);

    g_signal_emit (pager, signals[ERROR], 0);
}

GtkTreeModel *
yelp_pager_get_sections (YelpPager *pager)
{
    g_return_val_if_fail (pager != NULL, NULL);
    g_return_val_if_fail (YELP_IS_PAGER (pager), NULL);

    return YELP_PAGER_GET_CLASS (pager)->get_sections (pager);
}

const gchar *
yelp_pager_resolve_frag (YelpPager *pager, const gchar *frag_id)
{
    return YELP_PAGER_GET_CLASS (pager)->resolve_frag (pager, frag_id);
}

gboolean
yelp_pager_page_contains_frag (YelpPager   *pager,
			       const gchar *page_id,
			       const gchar *frag_id)
{
    const gchar *frag_page_id =
	YELP_PAGER_GET_CLASS (pager)->resolve_frag (pager, frag_id);

    return !strcmp (frag_page_id, page_id);
}

const YelpPage *
yelp_pager_get_page (YelpPager *pager, const gchar *frag_id)
{
    YelpPage    *page;
    const gchar *page_id =
	YELP_PAGER_GET_CLASS (pager)->resolve_frag (pager, frag_id);

    if (page_id == NULL)
	return NULL;

    page = (YelpPage *) g_hash_table_lookup (pager->priv->page_hash, page_id);

    return (const YelpPage *) page;
}

void 
yelp_pager_add_page (YelpPager *pager,
		     YelpPage  *page)
{
    g_return_if_fail (pager != NULL);
    g_return_if_fail (YELP_IS_PAGER (pager));

    g_return_if_fail (page->page_id != NULL);

    g_hash_table_replace (pager->priv->page_hash, page->page_id, page);
}

void
yelp_page_free (YelpPage *page)
{
    g_return_if_fail (page != NULL);

    g_free (page->page_id);
    g_free (page->title);
    g_free (page->contents);

    g_free (page->prev_id);
    g_free (page->next_id);
    g_free (page->toc_id);

    g_free (page);
}
