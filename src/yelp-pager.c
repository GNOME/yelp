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

    GMutex         *state_mutex;
    YelpPagerState  state;

    GMutex         *error_mutex;
    GError         *error;

    GMutex         *chunk_mutex;
    GHashTable     *chunk_hash;
};

enum {
    PROP_0,
    PROP_URI
};

enum {
    START,
    SECTIONS,
    CHUNK,
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

    signals[SECTIONS] = g_signal_new
	("sections",
	 G_TYPE_FROM_CLASS (klass),
	 G_SIGNAL_RUN_FIRST, 0,
	 NULL, NULL,
	 yelp_marshal_VOID__VOID,
	 G_TYPE_NONE, 0);

    signals[CHUNK] = g_signal_new
	("chunk",
	 G_TYPE_FROM_CLASS (klass),
	 G_SIGNAL_RUN_FIRST, 0,
	 NULL, NULL,
	 yelp_marshal_VOID__STRING,
	 G_TYPE_NONE, 1,
	 G_TYPE_STRING);

    signals[FINISH] = g_signal_new
	("finish",
	 G_TYPE_FROM_CLASS (klass),
	 G_SIGNAL_RUN_FIRST, 0,
	 NULL, NULL,
	 yelp_marshal_VOID__VOID,
	 G_TYPE_NONE, 0);

    signals[CANCEL] = g_signal_new
	("cancel",
	 G_TYPE_FROM_CLASS (klass),
	 G_SIGNAL_RUN_FIRST, 0,
	 NULL, NULL,
	 yelp_marshal_VOID__VOID,
	 G_TYPE_NONE, 0);

    signals[ERROR] = g_signal_new
	("error",
	 G_TYPE_FROM_CLASS (klass),
	 G_SIGNAL_RUN_FIRST, 0,
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

    pager->priv->state_mutex = g_mutex_new ();
    pager->priv->error_mutex = g_mutex_new ();
    pager->priv->chunk_mutex = g_mutex_new ();

    pager->priv->error = NULL;
    pager->priv->chunk_hash =
	g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
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

    g_mutex_free (pager->priv->state_mutex);
    g_mutex_free (pager->priv->error_mutex);
    g_mutex_free (pager->priv->chunk_mutex);

    if (pager->priv->error)
	g_error_free (pager->priv->error);

    g_hash_table_destroy (pager->priv->chunk_hash);

    g_free (pager->priv);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

/**
 * yelp_pager_start:
 * @pager: a #YelpPager.
 *
 * Spawns a thread to process the document.  If the document has already
 * been processed, no thread will be spawned, and %FALSE will be returned.
 **/
gboolean
yelp_pager_start (YelpPager *pager)
{
    GError   *error   = NULL;

    g_return_val_if_fail (pager != NULL, FALSE);
    g_return_val_if_fail (YELP_IS_PAGER (pager), FALSE);

    yelp_pager_lock_state (pager);

    switch (yelp_pager_get_state (pager)) {
    case YELP_PAGER_STATE_NEW:
    case YELP_PAGER_STATE_CANCEL:
	yelp_pager_set_state (pager, YELP_PAGER_STATE_START);

	g_object_ref (pager);
	g_signal_emit (pager, signals[START], 0);

	g_thread_create ((GThreadFunc) (YELP_PAGER_GET_CLASS (pager)->process),
			 pager, FALSE, &error);

	if (error) {
	    yelp_pager_unlock_state (pager);
	    yelp_pager_error (pager, error);
	    g_object_unref (pager);

	    return FALSE;
	}

	yelp_pager_unlock_state (pager);
	g_object_unref (pager);
	return TRUE;

    default:
	yelp_pager_unlock_state (pager);
	g_object_unref (pager);
	return FALSE;
    }
}

/**
 * yelp_pager_cancel:
 * @pager: a #YelpPager.
 *
 * Cancels the document processing.  The processing thread may continue
 * for a short while after this function returns before it recognizes
 * that it has been cancelled.
 **/
void
yelp_pager_cancel (YelpPager *pager)
{
    g_return_if_fail (pager != NULL);
    g_return_if_fail (YELP_IS_PAGER (pager));

    YELP_PAGER_GET_CLASS (pager)->cancel (pager);
}

/**
 * yelp_pager_get_uri:
 * @pager: a #YelpPager.
 *
 * Returns the URI of the documnt @pager is transforming.
 **/
const YelpURI *
yelp_pager_get_uri (YelpPager *pager)
{
    g_return_val_if_fail (pager != NULL, FALSE);
    g_return_val_if_fail (YELP_IS_PAGER (pager), FALSE);

    return (const YelpURI *) (pager->priv->uri);
}

/**
 * yelp_pager_get_state:
 * @pager: a #YelpPager
 *
 * Returns the state of @pager.  This does not handle locking itself.  You
 * must call yelp_pager_lock_state before and yelp_pager_unlock_state after.
 **/
YelpPagerState
yelp_pager_get_state (YelpPager *pager)
{
    g_return_val_if_fail (pager != NULL, 0);
    g_return_val_if_fail (YELP_IS_PAGER (pager), 0);

    return pager->priv->state;
}

/**
 * yelp_pager_set_state:
 * @pager: a #YelpPager
 * @state: a #YelpPagerState
 *
 * Sets the state of @pager to @state.  This does not handle locking itself. You
 * must call yelp_pager_lock_state before and yelp_pager_unlock_state after.
 **/
void
yelp_pager_set_state (YelpPager *pager, YelpPagerState state)
{
    g_return_if_fail (pager != NULL);
    g_return_if_fail (YELP_IS_PAGER (pager));

    pager->priv->state = state;
}

/**
 * yelp_pager_lock_state:
 * @pager: a #YelpPager.
 *
 * Locks the state @pager.  You should generally maintain a lock on the state
 * across most operations on @pager.
 **/
void
yelp_pager_lock_state (YelpPager *pager)
{
    g_return_if_fail (pager != NULL);
    g_return_if_fail (YELP_IS_PAGER (pager));

    g_mutex_lock (pager->priv->state_mutex);
}

/**
 * yelp_pager_unlock_state:
 * @pager: a #YelpPager.
 *
 * Releases a lock on the state of @pager.
 **/
void
yelp_pager_unlock_state (YelpPager *pager)
{
    g_return_if_fail (pager != NULL);
    g_return_if_fail (YELP_IS_PAGER (pager));

    g_mutex_unlock (pager->priv->state_mutex);
}

/**
 * yelp_pager_get_error:
 * @pager: a #YelpPager.
 *
 * Returns a #GError for the processing error.
 * The caller is responsible for freeing the #GError.
 **/
GError *
yelp_pager_get_error (YelpPager *pager)
{
    GError *error;

    g_return_val_if_fail (pager != NULL, NULL);
    g_return_val_if_fail (YELP_IS_PAGER (pager), NULL);

    g_mutex_lock (pager->priv->error_mutex);

    if (pager->priv->error)
	error = g_error_copy (pager->priv->error);
    else
	error = NULL;

    g_mutex_unlock (pager->priv->error_mutex);

    return error;
}

/**
 * yelp_pager_error:
 * @pager: a #YelpPager
 * @error: a #GError
 *
 * Sets the error of @pager and emits the "error" signal.  You must
 * release locks on the state before calling this.
 **/
void
yelp_pager_error (YelpPager *pager, GError *error)
{
    yelp_pager_lock_state (pager);
    g_mutex_lock (pager->priv->error_mutex);

    if (pager->priv->error)
	g_error_free (pager->priv->error);
    pager->priv->error = error;

    g_mutex_unlock (pager->priv->error_mutex);

    yelp_pager_set_state (pager, YELP_PAGER_STATE_ERROR);

    g_signal_emit_by_name (pager, "error");
    yelp_pager_unlock_state (pager);
}

/**
 * yelp_pager_get_sections:
 * @pager: a #YelpPager
 *
 * Returns a reference to the #GtkTreeModel where the section outline is stored.
 **/
const GtkTreeModel *
yelp_pager_get_sections (YelpPager *pager)
{
    g_return_val_if_fail (pager != NULL, NULL);
    g_return_val_if_fail (YELP_IS_PAGER (pager), NULL);

    return YELP_PAGER_GET_CLASS (pager)->get_sections (pager);
}

/**
 * yelp_pager_lookup_chunk:
 * @pager: a #YelpPager
 * @uri: a #YelpURI
 *
 * Look up and return the appropriate chunk for @uri, automatically resolving
 * which chunk to use based on the fragment identifier.
 **/
const gchar *
yelp_pager_lookup_chunk (YelpPager *pager, YelpURI *uri)
{
    gchar *chunk_id = NULL;
    gchar *chunk;

    g_return_val_if_fail (pager != NULL, NULL);
    g_return_val_if_fail (YELP_IS_PAGER (pager), NULL);

    chunk_id = (gchar *) (YELP_PAGER_GET_CLASS (pager)->resolve_uri (pager, uri));

    if (chunk_id)
	chunk_id = g_strdup (chunk_id);
    else
	chunk_id = yelp_uri_get_fragment (uri);

    chunk = (gchar *) yelp_pager_get_chunk (pager, chunk_id);

    g_free (chunk_id);

    return (const gchar *) chunk;
}

/**
 * yelp_pager_get_chunk:
 * @pager: a #YelpPager
 * @id: the chunk id
 *
 * Return the chunk with id @id.
 **/
const gchar *
yelp_pager_get_chunk (YelpPager *pager, gchar *id)
{
    gchar *chunk;

    g_return_val_if_fail (pager != NULL, NULL);
    g_return_val_if_fail (YELP_IS_PAGER (pager), NULL);

    g_mutex_lock (pager->priv->chunk_mutex);
    chunk = (gchar *) g_hash_table_lookup (pager->priv->chunk_hash, id);
    g_mutex_unlock (pager->priv->chunk_mutex);

    return (const gchar *) chunk;
}

/**
 * yelp_pager_add_chunk:
 * @pager: a #YelpPager
 * @id: the id of the new chunk
 * @chunk: the contents of the new chunk
 *
 * Add the chunk @chunk with id @id.
 **/
void 
yelp_pager_add_chunk (YelpPager *pager, gchar *id, gchar *chunk)
{
    g_return_if_fail (pager != NULL);
    g_return_if_fail (YELP_IS_PAGER (pager));

    g_mutex_lock (pager->priv->chunk_mutex);
    g_hash_table_insert (pager->priv->chunk_hash, id, chunk);
    g_mutex_unlock (pager->priv->chunk_mutex);
}
