/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009 Shaun McCance <shaunm@gnome.org>
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
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#include "yelp-view.h"

static void        yelp_view_init                 (YelpView           *view);
static void        yelp_view_class_init           (YelpViewClass      *klass);
static void        yelp_view_dispose              (GObject            *object);
static void        yelp_view_finalize             (GObject            *object);

static void        document_callback              (YelpDocument       *document,
                                                   YelpDocumentSignal  signal,
                                                   YelpView           *view,
                                                   GError             *error);

enum {
    NEW_VIEW_REQUESTED,
    LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (YelpView, yelp_view, WEBKIT_TYPE_WEB_VIEW);
#define GET_PRIV(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_VIEW, YelpViewPriv))

struct _YelpViewPriv {
    YelpUri       *uri;
    YelpDocument  *document;
    GCancellable  *cancellable;
};

#define TARGET_TYPE_URI_LIST     "text/uri-list"
enum {
    TARGET_URI_LIST
};

static void
yelp_view_init (YelpView *view)
{
    view->priv = GET_PRIV (view);

    view->priv->cancellable = NULL;
}

static void
yelp_view_dispose (GObject *object)
{
    YelpView *view = YELP_VIEW (object);

    if (view->priv->uri) {
        g_object_unref (view->priv->uri);
        view->priv->uri = NULL;
    }

    if (view->priv->cancellable) {
        g_cancellable_cancel (view->priv->cancellable);
        g_object_unref (view->priv->cancellable);
        view->priv->cancellable = NULL;
    }

    if (view->priv->document) {
        g_object_unref (view->priv->document);
        view->priv->document = NULL;
    }

    G_OBJECT_CLASS (yelp_view_parent_class)->dispose (object);
}

static void
yelp_view_finalize (GObject *object)
{
    YelpView *view = YELP_VIEW (object);

    G_OBJECT_CLASS (yelp_view_parent_class)->finalize (object);
}

static void
yelp_view_class_init (YelpViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = yelp_view_dispose;
    object_class->finalize = yelp_view_finalize;

    signals[NEW_VIEW_REQUESTED] =
	g_signal_new ("new_view_requested",
		      G_TYPE_FROM_CLASS (klass),
		      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
		      g_cclosure_marshal_VOID__STRING,
		      G_TYPE_NONE, 1, G_TYPE_STRING);

    g_type_class_add_private (klass, sizeof (YelpViewPriv));
}

/******************************************************************************/

GtkWidget *
yelp_view_new (void)
{
    return (GtkWidget *) g_object_new (YELP_TYPE_VIEW, NULL);
}

void
yelp_view_load (YelpView    *view,
                const gchar *uri)
{
    YelpUri *yuri = yelp_uri_resolve (uri);
    yelp_view_load_uri (view, yuri);
    g_object_unref (yuri);
}

void
yelp_view_load_uri (YelpView *view,
                    YelpUri  *uri)
{
    /* FIXME: want to get from a factory, just for testing */
    YelpDocument *document = yelp_simple_document_new (uri);
    yelp_view_load_document (view, uri, document);
    g_object_unref (document);
}

void
yelp_view_load_document (YelpView     *view,
                         YelpUri      *uri,
                         YelpDocument *document)
{
    gchar *page_id;

    /* FIXME: unset previous load */

    page_id = yelp_uri_get_page_id (uri);
    view->priv->uri = g_object_ref (uri);
    view->priv->cancellable = g_cancellable_new ();
    view->priv->document = g_object_ref (document);
    yelp_document_request_page (document,
                                page_id,
                                view->priv->cancellable,
                                (YelpDocumentCallback) document_callback,
                                view);

    g_free (page_id);
}

static void
document_callback (YelpDocument       *document,
                   YelpDocumentSignal  signal,
                   YelpView           *view,
                   GError             *error)
{
    if (signal == YELP_DOCUMENT_SIGNAL_INFO) {
    }
    else if (signal == YELP_DOCUMENT_SIGNAL_CONTENTS) {
	const gchar *contents = yelp_document_read_contents (document, NULL);
        gchar *base_uri, *mime_type, *page_id;
        base_uri = yelp_uri_get_base_uri (view->priv->uri);
        page_id = yelp_uri_get_page_id (view->priv->uri);
        mime_type = yelp_document_get_mime_type (document, page_id);
        webkit_web_view_load_string (WEBKIT_WEB_VIEW (view),
                                     contents,
                                     mime_type,
                                     "UTF-8",
                                     base_uri);
        g_free (page_id);
        g_free (mime_type);
        g_free (base_uri);
	yelp_document_finish_read (document, contents);
    }
    else if (signal == YELP_DOCUMENT_SIGNAL_ERROR) {
        printf ("ERROR: %s\n", error->message);
    }
}
