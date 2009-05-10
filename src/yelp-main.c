/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2003 Mikael Hallendal <micke@imendio.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <dbus/dbus-glib-bindings.h>
#include <string.h>
#include <stdlib.h>

#ifdef WITH_SMCLIENT
#include "eggsmclient.h"
#endif /* WITH_SMCLIENT */
#include "client-bindings.h"
#include "yelp-window.h"
#include "yelp-base.h"
#include "yelp-html.h"

static gchar       *cache_dir;
static gchar       *open_urls;
static gchar       *startup_id;
static gchar       **files;
static gboolean     private = FALSE;
static YelpBase    *priv_base = NULL;

static DBusGConnection *connection;

/*structure defining command line option.*/
enum {
	OPTION_OPEN_URLS = 1,
	OPTION_CACHE_DIR
};

static void           main_start              (const gchar *url);
static DBusGProxy *   main_dbus_get_proxy     (void);
static gboolean       main_is_running         (void);
static gboolean       main_slave_start         (gchar                *url);

#ifdef WITH_SMCLIENT
static int            main_save_session       (EggSMClient           *client,
					       gpointer              cdata);

static void           main_client_die         (EggSMClient          *client, 
					       gpointer              cdata);
#endif /* WITH_SMCLIENT */

static gboolean	      main_restore_session    (void);
static Time slowly_and_stupidly_obtain_timestamp (Display *xdisplay);


static const GOptionEntry options[] = {
	{
		"open-urls",
		'\0',
		G_OPTION_FLAG_HIDDEN,
		G_OPTION_ARG_STRING,
		&open_urls,
		NULL, NULL,
	},
	{
		"private-session",
		'p',
		0, G_OPTION_ARG_NONE,
		&private,
		N_("Use a private session"),
		NULL,
	},
	{
		"with-cache-dir",
		'\0',
		0, 
		G_OPTION_ARG_STRING,
		&cache_dir,
		N_("Define which cache directory to use"),
		NULL,
	},
	{ G_OPTION_REMAINING, 
	  0, 0, G_OPTION_ARG_FILENAME_ARRAY, 
	  &files, NULL,  NULL },
	{ NULL}
};

static void
main_start (const gchar *url)
{
	YelpBase *base;
	GError *error = NULL;

	base = yelp_base_new (private);
	
	server_new_window (base, (gchar *) url, startup_id, &error);
	priv_base = base;
	
	gtk_main ();
	
	return;
}

	
static gboolean
main_slave_start (gchar *url)
{
	DBusGProxy *proxy = NULL;
	GError *error = NULL;

	proxy = main_dbus_get_proxy ();

	if (!proxy)
		g_error ("Cannot connect to dbus\n");

	if (!org_gnome_YelpService_new_window (proxy, url, startup_id, 
						    &error))
		g_error ("%s\n", error->message);

	return FALSE;
}


#ifdef WITH_SMCLIENT
static gint 
main_save_session (EggSMClient        *client,
                   gpointer            cdata) 
{
	gchar                 **argv;
	gchar                 *open_windows = NULL;
	gint                    i=1;
	gint                    arg_len = 1;
	gboolean                store_open_urls = FALSE;
	/* DBusGProxy             *proxy = NULL; */
	GError                 *error = NULL;

	/*proxy = main_dbus_get_proxy ();
	if (!proxy) {
	g_warning ("Unable to connect to bus again\n");*/
	

	if (!server_get_url_list (priv_base, &open_windows, 
				  &error))
		g_warning ("Cannot recieve window list - %s\n", error->message);

	if (open_windows != NULL) {
		store_open_urls = TRUE;
		arg_len++;
	}

	if (cache_dir) {
		arg_len++;
	}

	argv = g_malloc0 (sizeof (gchar *) * arg_len);

	/* Program name */
	argv[0] = g_strdup ("yelp");
	
	if (cache_dir) {
		argv[1] = g_strdup_printf ("--with-cache-dir=\"%s\"", 
					   cache_dir);
	}

	if (store_open_urls) {
		argv[arg_len - 1] = g_strdup_printf ("--open-urls=\"%s\"", 
						     open_windows);
	}

	egg_sm_client_set_restart_command (client, arg_len, (const char **) argv);

	for (i = 0; i < arg_len; ++i) {
		g_free (argv[i]);
	}	
	g_free (argv);

	return TRUE;
}

static void
main_client_die (EggSMClient *client, 
		 gpointer     cdata)
{
	gtk_main_quit ();
}
#endif /* WITH_SMCLIENT */

static gboolean
main_restore_session (void)
{
	YelpBase *yelp_base;

	yelp_base = yelp_base_new (private);
	priv_base = yelp_base;

        if (!yelp_base) {
                g_error ("Couldn't activate YelpBase");
        }
	g_print ("restoring session\n");
	if (open_urls) {
		gchar **urls = g_strsplit_set (open_urls, ";\"", -1);
		gchar *url;
		gint   i = 0;
		GError *error = NULL;

		while ((url = urls[i]) != NULL) {
			if (*url != '\0')
				server_new_window (yelp_base, url, 
							startup_id, &error);
			++i;
		}
		
		g_strfreev (urls);
	}
	gtk_main ();
	return FALSE;

}

/* Copied from libnautilus/nautilus-program-choosing.c; Needed in case
 * we have no DESKTOP_STARTUP_ID (with its accompanying timestamp).
 */
static Time
slowly_and_stupidly_obtain_timestamp (Display *xdisplay)
{
	Window xwindow;
	XEvent event;
	
	{
		XSetWindowAttributes attrs;
		Atom atom_name;
		Atom atom_type;
		gchar* name;
		
		attrs.override_redirect = True;
		attrs.event_mask = PropertyChangeMask | StructureNotifyMask;
		
		xwindow =
			XCreateWindow (xdisplay,
				       RootWindow (xdisplay, 0),
				       -100, -100, 1, 1,
				       0,
				       CopyFromParent,
				       CopyFromParent,
				       CopyFromParent,
				       CWOverrideRedirect | CWEventMask,
				       &attrs);
		
		atom_name = XInternAtom (xdisplay, "WM_NAME", TRUE);
		g_assert (atom_name != None);
		atom_type = XInternAtom (xdisplay, "STRING", TRUE);
		g_assert (atom_type != None);
		
		name = "Fake Window";
		XChangeProperty (xdisplay, 
				 xwindow, atom_name,
				 atom_type,
				 8, PropModeReplace, (guchar *) name, strlen (name));
	}
	
	XWindowEvent (xdisplay,
		      xwindow,
		      PropertyChangeMask,
		      &event);
	
	XDestroyWindow(xdisplay, xwindow);
	
	return event.xproperty.time;
}

DBusGProxy *
main_dbus_get_proxy (void)
{
	if (!connection)
		return NULL;
	
	return dbus_g_proxy_new_for_name (connection,
					  "org.gnome.YelpService",
					  "/org/gnome/YelpService",
					  "org.gnome.YelpService");
}

gboolean
main_is_running (void)
{
	DBusGProxy *proxy = NULL;
	gboolean instance = TRUE;
	GError * error = NULL;
	
	if (!connection)
		return TRUE;
	
	proxy = dbus_g_proxy_new_for_name (connection,
					   DBUS_SERVICE_DBUS,
                                           DBUS_PATH_DBUS,
                                           DBUS_INTERFACE_DBUS);
	
        if (!dbus_g_proxy_call (proxy, "NameHasOwner", &error,
				G_TYPE_STRING, "org.gnome.YelpService",
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &instance,
				G_TYPE_INVALID)) {
		g_error ("Cannot connect to DBus - %s\n", error->message);
		exit (2);
	}
	return instance;
}


int
main (int argc, char **argv) 
{
	gchar         *url = NULL;
#ifdef WITH_SMCLIENT
	EggSMClient *sm_client;
	GError *error = NULL;
#endif /* WITH_SMCLIENT */
	gboolean retval;
	gboolean       session_started = FALSE;
	gchar *local_id;
	GOptionContext *context;

	g_thread_init(NULL);

	bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);  
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	local_id = (gchar *) g_getenv ("DESKTOP_STARTUP_ID");

	if (local_id != NULL && *local_id != '\0') {
		startup_id = g_strdup (local_id);
		putenv ("DESKTOP_STARTUP_ID=");
	}

	/* Commandline parsing is done here */
	context = g_option_context_new (N_(" GNOME Help Browser"));

	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

	g_option_context_add_group (context, gtk_get_option_group (TRUE));

#ifdef WITH_SMCLIENT
	g_option_context_add_group (context, egg_sm_client_get_option_group ());
#endif /* WITH_SMCLIENT */

	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);

	gtk_window_set_auto_startup_notification(FALSE);

	retval = g_option_context_parse (context, &argc, &argv, &error);
	g_option_context_free (context);
	if (!retval) {
		g_print ("%s", error->message);
		g_error_free (error);
		exit (1);
	}

	dbus_g_thread_init();

	if (!startup_id) {
		Time tmp;
		tmp = slowly_and_stupidly_obtain_timestamp (gdk_display);
		startup_id = g_strdup_printf ("_TIME%lu", tmp);
	}
	g_set_application_name (_("Help"));
	gtk_window_set_default_icon_name ("help-browser");

	if (!private) {
		connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
		if (!connection) {
			g_warning ("Cannot find dbus bus\n");
			private = TRUE;
		}
	}

	if (!yelp_html_initialize ()) {
		g_printerr ("Could not initialize gecko!\n");
		exit (1);
	}

	if (files != NULL && files[0] != NULL) {
		url = g_strdup (files[0]);
		g_strfreev (files);
	}

#ifdef WITH_SMCLIENT
	sm_client = egg_sm_client_get ();
	g_signal_connect (sm_client, "save-state",
	                  G_CALLBACK (main_save_session), NULL);
	g_signal_connect (sm_client, "quit",
	                  G_CALLBACK (main_client_die), NULL);
	session_started = egg_sm_client_is_resumed(sm_client);
#endif /* WITH_SMCLIENT */

	if (private || !main_is_running ()) {
		const gchar          *env;

		/* workaround for bug #329461 */
		env = g_getenv ("MOZ_ENABLE_PANGO");
		
		if (env == NULL ||
		    *env == '\0' ||
		    g_str_equal(env, "0")) 
			{
				g_setenv ("MOZ_DISABLE_PANGO", "1", TRUE);
			}
		
		if (session_started) {
			main_restore_session ();
		} else {
			main_start (url);
		}
	} else {
		main_slave_start (url);
	}

	yelp_html_shutdown ();

#ifdef WITH_SMCLIENT
	g_signal_handlers_disconnect_matched (sm_client, G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL, NULL, NULL);
#endif /* WITH_SMCLIENT */

        return 0;
}
