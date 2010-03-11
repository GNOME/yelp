/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2010 Shaun McCance <shaunm@gnome.org>
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

#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib.h>
#include <gtk/gtk.h>

#include "yelp-application.h"
#include "yelp-dbus.h"
#include "yelp-view.h"

typedef struct _YelpApplicationLoad YelpApplicationLoad;
struct _YelpApplicationLoad {
    YelpApplication *app;
    guint timestamp;
};

static void          yelp_application_init             (YelpApplication       *app);
static void          yelp_application_class_init       (YelpApplicationClass  *klass);
static void          yelp_application_dispose          (GObject               *object);
static void          yelp_application_finalize         (GObject               *object);

static GtkWidget *   application_new_window            (YelpApplication       *app);
static void          application_uri_resolved          (YelpUri               *uri,
                                                        YelpApplicationLoad   *data);

G_DEFINE_TYPE (YelpApplication, yelp_application, G_TYPE_OBJECT);
#define GET_PRIV(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_APPLICATION, YelpApplicationPrivate))

typedef struct _YelpApplicationPrivate YelpApplicationPrivate;
struct _YelpApplicationPrivate {
    DBusGConnection *connection;

    GSList *windows;

    GHashTable *windows_by_document;
};

static void
yelp_application_init (YelpApplication *app)
{
    YelpApplicationPrivate *priv = GET_PRIV (app);

    priv->windows_by_document = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free,
                                                       NULL);
}

static void
yelp_application_class_init (YelpApplicationClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = yelp_application_dispose;
    object_class->finalize = yelp_application_finalize;

    dbus_g_object_type_install_info (YELP_TYPE_APPLICATION,
                                     &dbus_glib_yelp_object_info);

    g_type_class_add_private (klass, sizeof (YelpApplicationPrivate));
}

static void
yelp_application_dispose (GObject *object)
{
    YelpApplicationPrivate *priv = GET_PRIV (object);

    g_object_unref (priv->connection);

    g_hash_table_destroy (priv->windows_by_document);

    G_OBJECT_CLASS (yelp_application_parent_class)->dispose (object);
}

static void
yelp_application_finalize (GObject *object)
{
    G_OBJECT_CLASS (yelp_application_parent_class)->finalize (object);
}


/******************************************************************************/

YelpApplication *
yelp_application_new (void)
{
    YelpApplication *app;

    app = (YelpApplication *) g_object_new (YELP_TYPE_APPLICATION, NULL);

    return app;
}

gint
yelp_application_run (YelpApplication  *app,
                      GOptionContext   *context,
                      gint              argc,
                      gchar           **argv)
{
    GError *error = NULL;
    DBusGProxy *proxy;
    guint request;
    YelpApplicationPrivate *priv = GET_PRIV (app);
    gchar *uri;

    priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
    if (priv->connection == NULL) {
        g_warning ("Unable to connect to dbus: %s", error->message);
        g_error_free (error);
        return 1;
    }

    /* FIXME: canonicalize relative URI */
    if (argc > 1)
        uri = argv[1];
    else
        uri = "ghelp:user-guide";

    proxy = dbus_g_proxy_new_for_name (priv->connection,
                                       DBUS_SERVICE_DBUS,
                                       DBUS_PATH_DBUS,
                                       DBUS_INTERFACE_DBUS);

    if (!org_freedesktop_DBus_request_name (proxy,
                                            "org.gnome.Yelp",
                                            0, &request,
                                            &error)) {
        g_warning ("Unable to register service: %s", error->message);
        g_error_free (error);
        g_object_unref (proxy);
        return 1;
    }

    g_object_unref (proxy);

    if (request == DBUS_REQUEST_NAME_REPLY_EXISTS ||
        request == DBUS_REQUEST_NAME_REPLY_IN_QUEUE) {
        gchar *newuri;

        newuri = g_strdup (uri);

        proxy = dbus_g_proxy_new_for_name (priv->connection,
                                           "org.gnome.Yelp",
                                           "/org/gnome/Yelp",
                                           "org.gnome.Yelp");
        if (!dbus_g_proxy_call (proxy, "LoadUri", &error,
                                G_TYPE_STRING, newuri,
                                G_TYPE_UINT, GDK_CURRENT_TIME,
                                G_TYPE_INVALID, G_TYPE_INVALID)) {
            g_warning ("Unable to notify existing process: %s\n", error->message);
            g_error_free (error);
        }

        g_free (newuri);
        g_object_unref (proxy);
        return 1;
    }

    dbus_g_connection_register_g_object (priv->connection,
                                         "/org/gnome/Yelp",
                                         G_OBJECT (app));

    yelp_application_load_uri (app, uri, GDK_CURRENT_TIME, NULL);

    gtk_main ();

    return 0;
}

gboolean
yelp_application_load_uri (YelpApplication  *app,
                           const gchar      *uri,
                           guint             timestamp,
                           GError          **error)
{
    YelpApplicationLoad *data;
    YelpUri *yuri;

    data = g_new (YelpApplicationLoad, 1);
    data->app = app;
    data->timestamp = timestamp;
    
    yuri = yelp_uri_new (uri);
    
    g_signal_connect (yuri, "resolved",
                      G_CALLBACK (application_uri_resolved),
                      data);
    yelp_uri_resolve (yuri);

    return TRUE;
}

static GtkWidget *
application_new_window (YelpApplication *app)
{
    GtkWidget *window, *scroll, *view;
    YelpApplicationPrivate *priv = GET_PRIV (app);
    
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_UTILITY);
    gtk_window_set_default_size (GTK_WINDOW (window), 520, 580);
    priv->windows = g_slist_prepend (priv->windows, window);

    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (window), scroll);

    view = yelp_view_new ();
    gtk_container_add (GTK_CONTAINER (scroll), view);

    g_object_set_data (G_OBJECT (window), "view", view);

    return window;
}

static void
application_uri_resolved (YelpUri             *uri,
                          YelpApplicationLoad *data)
{
    GtkWidget *window;
    YelpView *view;
    gchar *doc_uri;
    YelpApplicationPrivate *priv = GET_PRIV (data->app);

    doc_uri = yelp_uri_get_document_uri (uri);

    window = g_hash_table_lookup (priv->windows_by_document, doc_uri);

    if (window == NULL) {
        window = application_new_window (data->app);
        g_hash_table_insert (priv->windows_by_document, doc_uri, window);
    }
    else {
        g_free (doc_uri);
    }

    view = g_object_get_data (G_OBJECT (window), "view");
    yelp_view_load_uri (YELP_VIEW (view), uri);

    gtk_widget_show_all (window);
    gtk_window_present_with_time (GTK_WINDOW (window), data->timestamp);

    g_free (data);
}
