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

#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomeui/gnome-client.h>
#include <string.h>
#include <stdlib.h>

#include "GNOME_Yelp.h"
#include "yelp-window.h"
#include "yelp-base.h"

#define YELP_FACTORY_OAFIID "OAFIID:GNOME_Yelp_Factory"

poptContext  poptCon;
gint         next_opt;
gchar       *cache_dir;
gchar       *open_urls;

/*structure defining command line option.*/
enum {
	OPTION_OPEN_URLS = 1,
	OPTION_CACHE_DIR
};

static BonoboObject * main_base_factory       (BonoboGenericFactory *factory,
					       const gchar          *iid,
					       gpointer              closure);
static CORBA_Object   main_activate_base      (void);
static void           main_open_new_window    (CORBA_Object          yelp_base,
					       const gchar          *url);
static void           main_start              (gchar                *url);
static gboolean       main_idle_start         (gchar                *url);
static int            main_save_session       (GnomeClient          *client,
					       gint                  phase,
					       GnomeRestartStyle     rstyle,
					       gint                  shutdown,
					       GnomeInteractStyle    istyle,
					       gint                  fast,
					       gpointer              cdata);

static void           main_client_die         (GnomeClient          *client, 
					       gpointer              cdata);

static gboolean	      main_restore_session    (void);

struct poptOption options[] = {
	{
		"open-urls",
		'\0',
		POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN,
		&open_urls,
		OPTION_OPEN_URLS,
		NULL, NULL,
	},
	{
		"with-cache-dir",
		'\0',
		POPT_ARG_STRING,
		&cache_dir,
		OPTION_CACHE_DIR,
		N_("Define which cache directory to use"),
		NULL,
	},
	POPT_TABLEEND
};

static BonoboObject *
main_base_factory (BonoboGenericFactory *factory,
		   const gchar          *iid,
		   gpointer              closure)
{
	static YelpBase *yelp_base = NULL;

	if (!yelp_base) {
		yelp_base = yelp_base_new ();
	} else { 
		bonobo_object_ref (BONOBO_OBJECT (yelp_base));
	}

	return BONOBO_OBJECT (yelp_base);
}

static CORBA_Object
main_activate_base ()
{
	CORBA_Environment ev;
	CORBA_Object      yelp_base;
	
	CORBA_exception_init (&ev);
	
	yelp_base = bonobo_activation_activate_from_id ("OAFIID:GNOME_Yelp",
							0, NULL, &ev);
	
	if (BONOBO_EX (&ev) || yelp_base == CORBA_OBJECT_NIL) {
		g_error (_("Could not activate Yelp: '%s'"),
			   bonobo_exception_get_text (&ev));
	}

	CORBA_exception_free (&ev);
	
	return yelp_base;
}

static void
main_open_new_window (CORBA_Object yelp_base, const gchar *url)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);

	GNOME_Yelp_newWindow (yelp_base, url, &ev);

	if (BONOBO_EX (&ev)) {
		g_error (_("Could not open new window."));
	}

	CORBA_exception_free (&ev);
}
	
static void
main_start (gchar *url) 
{
	CORBA_Object yelp_base;
	
	yelp_base = main_activate_base ();

	if (!yelp_base) {
		g_error ("Couldn't activate YelpBase");
	}
	
	main_open_new_window (yelp_base, url);
	
	bonobo_object_release_unref (yelp_base, NULL);

	if (url) {
		g_free (url);
	}
}

static gboolean
main_idle_start (gchar *url)
{
	CORBA_Object yelp_base;
	
	yelp_base = main_activate_base ();

	if (!yelp_base) {
		g_error ("Couldn't activate YelpBase");
	}

	main_open_new_window (yelp_base, url);

	if (url) {
		g_free (url);
	}
	
	return FALSE;
}


static gint 
main_save_session (GnomeClient        *client,
                   gint                phase,
                   GnomeRestartStyle   rstyle,
                   gint                shutdown,
                   GnomeInteractStyle  istyle,
                   gint                fast,
                   gpointer            cdata) 
{
	
	GNOME_Yelp_WindowList  *list;
	CORBA_Environment       ev;
	CORBA_Object            yelp_base;
	gchar                 **argv;
	gint                    i=1;
	gint                    arg_len = 1;
	gboolean                store_open_urls = FALSE;

	CORBA_exception_init (&ev);

	yelp_base = main_activate_base ();

	list = GNOME_Yelp_getWindows (yelp_base, &ev);

	bonobo_object_release_unref (yelp_base, NULL);

	if (cache_dir) {
		arg_len++;
	}

	if (list->_length > 0) {
		store_open_urls = TRUE;
		arg_len++;
	}
	
	argv = g_malloc0 (sizeof (gchar *) * arg_len);

	/* Program name */
	argv[0] = g_strdup ((gchar *) cdata);
	
	if (cache_dir) {
		argv[1] = g_strdup_printf ("--with-cache-dir=\"%s\"", 
					   cache_dir);
	}

	if (store_open_urls) {
		gchar *urls;
		
		/* Get the URI of each window */
		urls = g_strdup_printf ("--open-urls=\"%s", list->_buffer[0]);
		
		for (i=1; i < list->_length; i++) {
			gchar *tmp;
			
			tmp = g_strconcat (urls, ";", list->_buffer[i], NULL);
			g_free (urls);
			urls = tmp;
		}

		argv[2] = g_strconcat (urls, "\"", NULL);
		g_free (urls);
	}

	gnome_client_set_clone_command (client, arg_len, argv);
	gnome_client_set_restart_command (client, arg_len, argv);

	for (i = 0; i < arg_len; ++i) {
		g_free (argv[i]);
	}
	
	g_free (argv);

	return TRUE;
}

static void
main_client_die (GnomeClient *client, 
		 gpointer     cdata)
{
	bonobo_main_quit ();
}

static gboolean
main_restore_session (void)
{
	CORBA_Object yelp_base;

	yelp_base = main_activate_base ();

        if (!yelp_base) {
                g_error ("Couldn't activate YelpBase");
        }

	if (open_urls) {
		gchar **urls = g_strsplit (open_urls, ";", -1);
		gchar *url;
		gint   i = 0;
		
		while ((url = urls[i]) != NULL) {
			main_open_new_window (yelp_base, url);
			++i;
		}
		
		g_strfreev (urls);
	}

	return FALSE;
}

int
main (int argc, char **argv) 
{
	GnomeProgram  *program;
	CORBA_Object   factory;
	gchar         *url = NULL;
	GnomeClient   *client;
	gboolean       session_started = FALSE;
	const gchar  **args;
	
	
	bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);  
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	program = gnome_program_init (PACKAGE, VERSION,
				      LIBGNOMEUI_MODULE,
				      argc, argv,
				      GNOME_PARAM_POPT_TABLE, options,
				      GNOME_PROGRAM_STANDARD_PROPERTIES,
				      NULL);

	/* Need to set this to the canonical DISPLAY value, since
	   that's where we're registering per-display components */
	bonobo_activation_set_activation_env_value
		("DISPLAY",
		 gdk_display_get_name (gdk_display_get_default ()) );

	gnome_vfs_init ();
	
	/* Commandline parsing is done here */
	g_object_get (G_OBJECT (program),
		      GNOME_PARAM_POPT_CONTEXT, &poptCon,
		      NULL);

	args = poptGetArgs (poptCon);
	if (args) {
		url = g_strdup (*args);
	} else {
		url = g_strdup ("");
	}

	client = gnome_master_client ();
        g_signal_connect (client, "save_yourself",
                          G_CALLBACK (main_save_session), (gpointer) argv[0]);
        g_signal_connect (client, "die",
                          G_CALLBACK (main_client_die), NULL);

	factory = bonobo_activation_activate_from_id (YELP_FACTORY_OAFIID,
						      Bonobo_ACTIVATION_FLAG_EXISTING_ONLY, 
						      NULL, NULL);
	
	/* Check for previous session to restore. */
	if (gnome_client_get_flags (client) & GNOME_CLIENT_RESTORED) {
		session_started = TRUE;
        }

	if (!factory) { /* Not started, start now */ 
		BonoboGenericFactory *factory;
		char                 *registration_id;

		registration_id = bonobo_activation_make_registration_id (
					YELP_FACTORY_OAFIID,
					gdk_display_get_name (gdk_display_get_default ()));
		factory = bonobo_generic_factory_new (registration_id,
						      main_base_factory,
						      NULL);
		g_free (registration_id);

		bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));
        
	        /* If started by session, restore from last session */
		if (session_started) {
			g_idle_add ((GSourceFunc) main_restore_session, NULL);
		} else {
			g_idle_add ((GSourceFunc) main_idle_start, url);
		}

		bonobo_main ();
	} else {
		main_start (url);
	}

        return 0;
}
