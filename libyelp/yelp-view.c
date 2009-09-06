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

static void        view_init		          (YelpView        *view);
static void        view_class_init	          (YelpViewClass   *klass);

enum {
    NEW_VIEW_REQUESTED,
    LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

#define YELP_VIEW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_VIEW, YelpWindowView))

struct _YelpWindowPriv {
    WebKitWebView  *webview;
};

#define TARGET_TYPE_URI_LIST     "text/uri-list"
enum {
    TARGET_URI_LIST
};

GType
yelp_view_get_type (void)
{
    static GType view_type = 0;

    if (!window_type) {
	static const GTypeInfo view_info = {
	    sizeof (YelpViewClass),
	    NULL,
	    NULL,
	    (GClassInitFunc) view_class_init,
	    NULL,
	    NULL,
	    sizeof (YelpView),
	    0,
	    (GInstanceInitFunc) view_init,
	};
	view_type = g_type_register_static (GTK_TYPE_VIEW,
                                            "YelpView",
                                            &view_info, 0);
    }
    return view_type;
}

static void
view_init (YelpView *view)
{
    view->priv = YELP_VIEW_GET_PRIVATE (view);
    view->priv->webview = (WebKitWebView *) webkit_web_view_new ();
    gtk_container_add (GTK_CONTAINER (view), (GtkWidget *) webview);
}

static void
view_dispose (GObject *object)
{
    parent_class->dispose (object);
}

static void
view_finalize (GObject *object)
{
    YelpView *view = YELP_VIEW (object);
    YelpViewPriv *priv = view->priv;

    /* FIXME Free stuff */

    parent_class->finalize (object);
}

static void
view_class_init (YelpViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

    object_class->dispose = view_dispose;
    object_class->finalize = view_finalize;

    signals[NEW_VIEW_REQUESTED] =
	g_signal_new ("new_view_requested",
		      G_TYPE_FROM_CLASS (klass),
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (YelpViewClass,
				       new_view_requested),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__STRING,
		      G_TYPE_NONE, 1, G_TYPE_STRING);

    g_type_class_add_private (klass, sizeof (YelpViewPriv));
}
