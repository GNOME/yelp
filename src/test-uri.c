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

#include <libgnomevfs/gnome-vfs.h>
#include "yelp-uri.h"

int
main (int argc, char **argv)
{
        YelpURI *uri;
        GError  *error = NULL;
        
        if (argc < 2) {
                g_print ("Usage: test-uri uri\n");
                return 1;
        }

	gnome_vfs_init ();

        uri = yelp_uri_new (argv[1], &error);
        
        if (error) {
                g_print ("Error: %s\n", error->message);

                return 1;
        } 

        g_print ("URI_TYPE   : %d\n", yelp_uri_get_type (uri));
	g_print ("URI_PATH   : %s\n", yelp_uri_get_path (uri));
	g_print ("URI_SECTION: %s\n", yelp_uri_get_section (uri));

	yelp_uri_unref (uri);

        gnome_vfs_shutdown ();

        return 0;
}
