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

#include "yelp-pager.h"
#include "yelp-marshal.h"
#include "yelp-uri.h"

#define d(x)

struct _YelpPagerPriv {
    YelpURI        *uri;

    YelpPagerState  state;

    GError         *error;

    GHashTable     *page_hash;
};

enum {
    PROP_0,
    PROP_URI
};

enum {
    START,
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
	 PROP_URI,
	 g_param_spec_object ("uri",
			      _("Document URI"),
			      _("The URI of the document to be processed"),
			      YELP_TYPE_URI,
			      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    signals[START] = g_signal_new
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
	 G_SIGNAL_RUN_LAST,
	 G_STRUCT_OFFSET (YelpPagerClass, finish),
	 NULL, NULL,
	 yelp_marshal_VOID__VOID,
	 G_TYPE_NONE, 0);

    signals[CANCEL] = g_signal_new
	("cancel",
	 G_TYPE_FROM_CLASS (klass),
	 G_SIGNAL_RUN_LAST,
	 G_STRUCT_OFFSET (YelpPagerClass, cancel),
	 NULL, NULL,
	 yelp_marshal_VOID__VOID,
	 G_TYPE_NONE, 0);

    signals[ERROR] = g_signal_new
	("error",
	 G_TYPE_FROM_CLASS (klass),
	 G_SIGNAL_RUN_LAST,
	 G_STRUCT_OFFSET (YelpPagerClass, error),
	 NULL, NULL,
	 yelp_marshal_VOID__VOID,
	 G_TYPE_NONE, 0);
}

static void
pager_init (YelpPager *pager)
{
    YelpPagerPriv *priv;

    priv = g_new0 (YelpPagerPriv, 1);
    pager->priv = priv;

    pager->priv->uri   = NULL;
    pager->priv->state = YELP_PAGER_STATE_NEW;

    pager->priv->error = NULL;
    pager->priv->page_hash =
	g_hash_table_new_full (g_str_hash,
			       g_str_equal,
			       g_free,
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
    case PROP_URI:
	if (pager->priv->uri)
	    g_object_unref (pager->priv->uri);
	pager->priv->uri = (YelpURI *) g_value_get_object (value);
	g_object_ref (pager->priv->uri);
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
    case PROP_URI:
	g_value_set_object (value, pager->priv->uri);
	break;
    default:
	break;
    }
}

static void
pager_dispose (GObject *object)
{
    YelpPager *pager = YELP_PAGER (object);

    g_object_unref (pager->priv->uri);

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

    switch (yelp_pager_get_state (pager)) {
    case YELP_PAGER_STATE_NEW:
    case YELP_PAGER_STATE_CANCEL:
	yelp_pager_set_state (pager, YELP_PAGER_STATE_START);

	g_object_ref (pager);
	g_signal_emit (pager, signals[START], 0);

	gtk_idle_add ((GtkFunction) (YELP_PAGER_GET_CLASS (pager)->process),
		      pager);

	g_object_unref (pager);
	return TRUE;

    default:
	g_object_unref (pager);
	return FALSE;
    }
}

void
yelp_pager_cancel (YelpPager *pager)
{
    g_return_if_fail (pager != NULL);
    g_return_if_fail (YELP_IS_PAGER (pager));

    YELP_PAGER_GET_CLASS (pager)->cancel (pager);
}

YelpURI *
yelp_pager_get_uri (YelpPager *pager)
{
    g_return_val_if_fail (pager != NULL, FALSE);
    g_return_val_if_fail (YELP_IS_PAGER (pager), FALSE);

    return pager->priv->uri;
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
    if (pager->priv->error)
	g_error_free (pager->priv->error);
    pager->priv->error = error;

    yelp_pager_set_state (pager, YELP_PAGER_STATE_ERROR);

    g_signal_emit_by_name (pager, "error");
}

const GtkTreeModel *
yelp_pager_get_sections (YelpPager *pager)
{
    g_return_val_if_fail (pager != NULL, NULL);
    g_return_val_if_fail (YELP_IS_PAGER (pager), NULL);

    return YELP_PAGER_GET_CLASS (pager)->get_sections (pager);
}

const YelpPage *
yelp_pager_lookup_page (YelpPager *pager, YelpURI *uri)
{
    gchar *page_id = NULL;
    YelpPage *page;

    g_return_val_if_fail (pager != NULL, NULL);
    g_return_val_if_fail (YELP_IS_PAGER (pager), NULL);

    page_id = (gchar *) (YELP_PAGER_GET_CLASS (pager)->resolve_uri (pager, uri));

    if (!page_id)
	page_id = yelp_uri_get_fragment (uri);

    page = (YelpPage *) yelp_pager_get_page (pager, page_id);

    g_free (page_id);

    return (const YelpPage *) page;
}

const YelpPage *
yelp_pager_get_page (YelpPager *pager, gchar *id)
{
    YelpPage *page;

    g_return_val_if_fail (pager != NULL, NULL);
    g_return_val_if_fail (YELP_IS_PAGER (pager), NULL);

    page = (YelpPage *) g_hash_table_lookup (pager->priv->page_hash, id);

    return (const YelpPage *) page;
}

void 
yelp_pager_add_page (YelpPager *pager,
		     gchar     *id,
		     gchar     *title,
		     gchar     *chunk)
{
    YelpPage *page;

    g_return_if_fail (pager != NULL);
    g_return_if_fail (YELP_IS_PAGER (pager));

    page = g_new0 (YelpPage, 1);

    page->id    = id;
    page->title = title;
    page->chunk = chunk;

    g_hash_table_insert (pager->priv->page_hash, id, page);
}

void
yelp_page_free (YelpPage *page)
{
    g_free (page->id);
    g_free (page->title);
    g_free (page->chunk);

    g_free (page);
}
