/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Mikael Hallendal <micke@imendio.com>
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

#include <libgnome/gnome-init.h>
#include <libgnome/gnome-program.h>
#include <libgnomevfs/gnome-vfs.h>

#include <string.h>

#include "yelp-uri.h"
#include "yelp-reader.h"

static void
start_cb (YelpReader *reader)
{
	g_print ("------------------------------------------\n");
}

static void
data_cb (YelpReader       *reader, 
	 const gchar      *buffer,
	 gint              len)
{
	g_print ("data_cb [%d <-> %d]\n", len, strlen (buffer));
 	g_print (buffer);
}

static void
finished_cb (YelpReader *reader)
{
	g_print ("------------------------------------------\n");
}

int
main (int argc, char **argv)
{
	GnomeProgram *program;
        YelpURI      *uri;
	YelpReader   *reader;
		
        if (argc < 2) {
                g_print ("Usage: test-uri uri\n");
                return 1;
        }

	g_thread_init (NULL);

	program = gnome_program_init (PACKAGE, VERSION,
				      LIBGNOME_MODULE,
				      argc, argv,
				      GNOME_PROGRAM_STANDARD_PROPERTIES,
				      NULL);

	gnome_vfs_init ();

        uri = yelp_uri_new (argv[1]);
        
        if (!yelp_uri_exists (uri)) {
                g_print ("URI (%s) does not exist\n", argv[1]);

                return 1;
        }

	reader = yelp_reader_new ();

	g_signal_connect (reader, "start", 
			  G_CALLBACK (start_cb),
			  NULL);
	g_signal_connect (reader, "data", 
			  G_CALLBACK (data_cb),
			  NULL);
	g_signal_connect (reader, "finished", 
			  G_CALLBACK (finished_cb),
			  NULL);

	yelp_reader_start (reader, uri);
	
	yelp_uri_unref (uri);

	g_main_loop_run (g_main_loop_new (NULL, TRUE));

        gnome_vfs_shutdown ();

        return 0;
}
