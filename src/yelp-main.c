/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Mikael Hallendal <micke@codefactory.se>
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
 * Author: Mikael Hallendal <micke@codefactory.se>
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
#include <stdlib.h>

#include "GNOME_Yelp.h"
#include "yelp-window.h"
#include "yelp-base.h"

#define YELP_FACTORY_OAFIID "OAFIID:GNOME_Yelp_Factory"

static BonoboObject * yelp_base_factory       (BonoboGenericFactory *factory,
					       const gchar          *iid,
					       gpointer              closure);
static CORBA_Object   yelp_main_activate_base (void);
static gboolean       yelp_main_idle_start    (gchar                *url);
static int            yelp_save_session       (GnomeClient          *client,
					       gint                  phase,
					       GnomeRestartStyle     rstyle,
					       gint                  shutdown,
					       GnomeInteractStyle    istyle,
					       gint                  fast,
					       gpointer              cdata);

static gint           yelp_client_die         (GnomeClient          *client, 
					       gpointer              cdata);


static BonoboObject *
yelp_base_factory (BonoboGenericFactory *factory,
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
yelp_main_activate_base ()
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
yelp_main_open_new_window (CORBA_Object yelp_base, const gchar *url)
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
yelp_main_start (gchar *url) 
{
	CORBA_Object yelp_base;
	
	yelp_base = yelp_main_activate_base ();

	if (!yelp_base) {
		g_error ("Couldn't activate YelpBase");
	}
	
	yelp_main_open_new_window (yelp_base, url);
	
	bonobo_object_release_unref (yelp_base, NULL);

	if (url) {
		g_free (url);
	}
}

static gboolean
yelp_main_idle_start (gchar *url)
{
	CORBA_Object yelp_base;
	
	yelp_base = yelp_main_activate_base ();

	if (!yelp_base) {
		g_error ("Couldn't activate YelpBase");
	}

	yelp_main_open_new_window (yelp_base, url);

	if (url) {
		g_free (url);
	}
	
	return FALSE;
}


static gint 
yelp_save_session (GnomeClient        *client,
                   gint                phase,
                   GnomeRestartStyle   rstyle,
                   gint                shutdown,
                   GnomeInteractStyle  istyle,
                   gint                fast,
                   gpointer            cdata) 
{

        gchar *argv[]= { NULL };

        argv[0] = (gchar*) cdata;
        gnome_client_set_clone_command (client, 1, argv);
        gnome_client_set_restart_command (client, 1, argv);
        return TRUE;
}

static gint 
yelp_client_die (GnomeClient *client, 
		 gpointer     cdata)
{
        exit (0);
}

int
main (int argc, char **argv) 
{
	GnomeProgram *program;
	CORBA_Object  factory;
	gchar        *url = NULL;
	GnomeClient  *client;
	
	bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);  
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
	g_thread_init (NULL);
	
	if (argc >= 2) {
		url = g_strdup (argv[1]);
	} else {
		url = g_strdup ("");
	}

	program = gnome_program_init (PACKAGE, VERSION,
				      LIBGNOMEUI_MODULE,
				      argc, argv, 
				      GNOME_PROGRAM_STANDARD_PROPERTIES,
				      NULL);

	gnome_vfs_init ();

	client = gnome_master_client ();
        g_signal_connect (client, "save_yourself",
                          G_CALLBACK (yelp_save_session), (gpointer) argv[0]);
        g_signal_connect (client, "die",
                          G_CALLBACK (yelp_client_die), NULL);

	factory = bonobo_activation_activate_from_id (YELP_FACTORY_OAFIID,
						      Bonobo_ACTIVATION_FLAG_EXISTING_ONLY, 
						      NULL, NULL);
	
	if (!factory) {
		BonoboGenericFactory   *factory;
		/* Not started, start now */

		factory = bonobo_generic_factory_new (YELP_FACTORY_OAFIID,
						      yelp_base_factory,
						      NULL);

		bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));
		g_idle_add ((GSourceFunc) yelp_main_idle_start, url);
		bonobo_main ();
	} else {
		yelp_main_start (url);
	}

        return 0;
}
