/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Mikael Hallendal <micke@codefactory.se>
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

#include <config.h>

#include <libgnome/gnome-init.h>
#include <libgnome/gnome-program.h>
#include <libgnomevfs/gnome-vfs.h>

#include "yelp-uri.h"
#include "yelp-reader.h"

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

	reader = yelp_reader_new (FALSE);


	g_signal_connect (reader, "open", 
			  G_CALLBACK (open_cb),
			  NULL);
	g_signal_connect (reader, "read", 
			  G_CALLBACK (read_cb),
			  NULL);
	g_signal_connect (reader, "close", 
			  G_CALLBACK (close_cb),
			  NULL);

	yelp_reader_read (uri);
	
	yelp_uri_unref (uri);

	g_main_loop_run (g_main_loop_new (NULL, TRUE));

        gnome_vfs_shutdown ();

        return 0;
}
