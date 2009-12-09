/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@imendio.com>
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
 * Author: Mikael Hallendal <micke@imendio.com>
 */

#include <config.h>

#include <dbus/dbus-glib-bindings.h>
#include <gdk/gdkx.h>
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn-launchee.h>

#include <string.h>

#include "yelp-window.h"
#include "yelp-settings.h"
#include "yelp-toc.h"
#include "yelp-base.h"
#include "yelp-bookmarks.h"
#include "server-bindings.h"

gboolean main_running;

#define YELP_BASE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_BASE, YelpBasePriv))

struct _YelpBasePriv {
	GNode  *toc_tree;
	gboolean private_session;

	GList  *index;
	GSList *windows;
};

static void           yelp_base_init                (YelpBase       *base);
static void           yelp_base_class_init          (YelpBaseClass  *klass);
static void           yelp_base_register_dbus       (YelpBase       *base);
static void           yelp_base_new_window_cb       (YelpWindow     *window,
						     const gchar    *uri,
						     YelpBase       *base);
static void           yelp_base_window_finalized_cb (YelpBase       *base,
						     YelpWindow     *window);


#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class;

GType
yelp_base_get_type (void)
{
    static GType base_type = 0;

    if (!base_type) {
        static const GTypeInfo base_info = {
            sizeof (YelpBaseClass),
            NULL,
            NULL,
            (GClassInitFunc) yelp_base_class_init,
            NULL,
            NULL,
            sizeof (YelpBase),
            0,
            (GInstanceInitFunc) yelp_base_init,
        };

        base_type = g_type_register_static (G_TYPE_OBJECT,
                                              "YelpBase",
                                              &base_info, 0);
    }

    return base_type;
}

static void
yelp_base_init (YelpBase *base)
{
        YelpBasePriv *priv;

        base->priv = priv = YELP_BASE_GET_PRIVATE (base);
	priv->toc_tree = g_node_new (NULL);
	priv->index    = NULL;
	priv->windows  = NULL;
	yelp_bookmarks_init ();
	yelp_settings_init ();
	/* Init here to start processing before 
	 * we even start the window */
	yelp_toc_new();

}

static void
yelp_base_class_init (YelpBaseClass *klass)
{
	parent_class = g_type_class_peek (PARENT_TYPE);

	main_running = TRUE;

	g_type_class_add_private (klass, sizeof (YelpBasePriv));
}

gboolean
server_new_window (YelpBase *base, gchar *url, gchar *timestamp, 
			GError **error)
{
	GtkWidget *new_window;

	new_window = yelp_base_new_window (base, url, timestamp);
	gtk_widget_show (new_window);
	return TRUE;
}

gboolean
server_get_url_list (YelpBase *server, gchar **urls, GError **error)
{
	gint len,  i;
	GSList *node;
	const gchar *uri;
	YelpBasePriv *priv;

	priv = server->priv;

	len  = g_slist_length (priv->windows);

	node = priv->windows;

	uri = yelp_window_get_uri ((YelpWindow *) node->data);
	*urls = g_strdup (uri);
	node = node->next;

	for (i = 0; node; node = node->next, i++) {
		gchar *list;
		uri = yelp_window_get_uri ((YelpWindow *) node->data);

		list = g_strconcat (uri, ";", *urls, NULL);
		g_free (*urls);
		*urls = g_strdup (list);
		g_free (list);
	}
	return TRUE;
}

static void
yelp_base_new_window_cb (YelpWindow *window, const gchar *uri, 
			 YelpBase *base)
{
	GtkWidget *new_window;
	
	g_return_if_fail (YELP_IS_WINDOW (window));
	g_return_if_fail (YELP_IS_BASE (base));
	
	new_window = yelp_base_new_window (base, uri, NULL);

	gtk_widget_show (new_window);
}

static void
yelp_base_window_finalized_cb (YelpBase *base, YelpWindow *window)
{
	YelpBasePriv *priv;
	
	g_return_if_fail (YELP_IS_BASE (base));

	priv = base->priv;
	
	priv->windows = g_slist_remove (priv->windows, window);

	if (g_slist_length (priv->windows) == 0) {
		main_running = FALSE;
		gtk_main_quit ();
	}
}

YelpBase *
yelp_base_new (gboolean priv)
{
        YelpBase     *base;
	
        base = g_object_new (YELP_TYPE_BASE, NULL);
	if (!priv)
		yelp_base_register_dbus (base);
	base->priv->private_session = priv;

        return base;
}

static void
sn_error_trap_push (SnDisplay *display,
		    Display *xdisplay)
{
	gdk_error_trap_push ();
}

static void
sn_error_trap_pop (SnDisplay *display,
		   Display *xdisplay)
{
	gdk_error_trap_pop ();
}

GtkWidget *
yelp_base_new_window (YelpBase *base, const gchar *uri, const gchar *startup_id)
{
	YelpBasePriv *priv;
	GtkWidget    *window;
	SnDisplay *sn_display = NULL;
	GdkScreen *screen = NULL;
	GdkDisplay *display = NULL;
	SnLauncheeContext *context = NULL;

        g_return_val_if_fail (YELP_IS_BASE (base), NULL);

	priv = base->priv;
        
        window = yelp_window_new (priv->toc_tree, priv->index);
	gtk_widget_realize (GTK_WIDGET (window));

	if (startup_id) {
		screen = gtk_window_get_screen (GTK_WINDOW (window));
		display = gdk_screen_get_display (screen);
		
		sn_display = 
			sn_display_new (gdk_x11_display_get_xdisplay (display),
					sn_error_trap_push, sn_error_trap_pop);
		context = sn_launchee_context_new (sn_display,
					   gdk_screen_get_number (screen),
						   startup_id);
		if (strncmp (sn_launchee_context_get_startup_id (context), 
			     "_TIME", 5) != 0)
			sn_launchee_context_setup_window (context,
					 GDK_WINDOW_XWINDOW (gtk_widget_get_window (window)));

		if (sn_launchee_context_get_id_has_timestamp (context)) {
			gulong time;

			time = sn_launchee_context_get_timestamp (context);
			gdk_x11_window_set_user_time (gtk_widget_get_window (window),
						      time);
		}		
	}
	        
	priv->windows = g_slist_prepend (priv->windows, window);

	g_object_weak_ref (G_OBJECT (window),
			   (GWeakNotify) yelp_base_window_finalized_cb,
			   base);
	
	g_signal_connect (window, "new_window_requested",
			  G_CALLBACK (yelp_base_new_window_cb),
			  base);

	gtk_widget_show (window);

	if (uri && uri[0] != '\0')
		yelp_window_load (YELP_WINDOW (window), uri);
	else
		yelp_window_load (YELP_WINDOW (window), "x-yelp-toc:");

	if (context) {
		sn_launchee_context_complete (context);
		sn_launchee_context_unref (context);
		sn_display_unref (sn_display);		
	}

	return window;
}

static void
yelp_base_register_dbus (YelpBase *base)
{
	GError *error = NULL;
        DBusGProxy *driver_proxy;
	YelpBaseClass *klass = YELP_BASE_GET_CLASS (base);
        guint request_ret;
	YelpBasePriv *priv;

	priv = base->priv;
	
        klass->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
        if (klass->connection == NULL) {
		g_warning("Unable to connect to dbus: %s", error->message);
		g_error_free (error);
		return;
	}

        dbus_g_object_type_install_info (YELP_TYPE_BASE,
                                         &dbus_glib_server_object_object_info);
	
	dbus_g_connection_register_g_object (klass->connection,
                                             "/org/gnome/YelpService",
                                             G_OBJECT (base));

        driver_proxy = dbus_g_proxy_new_for_name (klass->connection,
                                                  DBUS_SERVICE_DBUS,
                                                  DBUS_PATH_DBUS,
                                                  DBUS_INTERFACE_DBUS);
	
        if(!org_freedesktop_DBus_request_name (driver_proxy,
                                               "org.gnome.YelpService",
                                               0, &request_ret,
                                               &error)) {
		g_warning("Unable to register service: %s", error->message);
		g_error_free (error);
	}
        g_object_unref (driver_proxy);

}
