/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
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

#include "yelp-uri.h"

static void
print_uri (YelpURI *uri)
{
    gchar *type;
    gchar *str_uri;
	
    switch (yelp_uri_get_resource_type (uri)) {
    case YELP_URI_TYPE_UNKNOWN:
	type = "YELP_URI_TYPE_UNKNOWN";
	break;
    case YELP_URI_TYPE_RELATIVE:
	type = "YELP_URI_TYPE_RELATIVE";
	break;
    case YELP_URI_TYPE_DOCBOOK_XML:
	type = "YELP_URI_TYPE_DOCBOOK_XML";
	break;
    case YELP_URI_TYPE_DOCBOOK_SGML:
	type = "YELP_URI_TYPE_DOCBOOK_SGML";
	break;
    case YELP_URI_TYPE_GHELP:
	type = "YELP_URI_TYPE_GHELP";
	break;
    case YELP_URI_TYPE_HTML:
	type = "YELP_URI_TYPE_HTML";
	break;
    case YELP_URI_TYPE_MAN:
	type = "YELP_URI_TYPE_MAN";
	break;
    case YELP_URI_TYPE_INFO:
	type = "YELP_URI_TYPE_INFO";
	break;
    case YELP_URI_TYPE_TOC:
	type = "YELP_URI_TYPE_TOC";
	break;
    case YELP_URI_TYPE_PATH:
	type = "YELP_URI_TYPE_PATH";
	break;
    case YELP_URI_TYPE_FILE:
	type = "YELP_URI_TYPE_FILE";
	break;
    default:
	type = NULL;
	break;
    }
    g_print ("TYPE     : %s\n", type);
    g_print ("PATH     : %s\n", yelp_uri_get_path (uri));
    if (yelp_uri_get_fragment (uri))
	g_print ("FRAGMENT : %s\n", yelp_uri_get_fragment (uri));
    g_print ("\n");

    /*
    str_uri = yelp_uri_to_string (uri);
    g_print ("URI_TO_STRING: %s\n", str_uri);
    g_free (str_uri);
    */
}

int
main (int argc, char **argv)
{
    GnomeProgram *program;
    YelpURI      *uri;
    YelpURI      *rel_uri;
	
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

    /*
    if (!yelp_uri_exists (uri)) {
	g_print ("URI (%s) does not exist\n", argv[1]);

	return 1;
    }
    */

    g_print ("STRING   : %s\n", argv[1]);
    print_uri (uri);

    /*
    g_print ("---------------\n");
	
    rel_uri = yelp_uri_get_relative (uri, "?link");
    print_uri (rel_uri);
    g_object_unref (rel_uri);
	
    g_print ("---------------\n");

    rel_uri = yelp_uri_get_relative (uri, "link");
    print_uri (rel_uri);
    g_object_unref (rel_uri);
    */
	
    g_object_unref (uri);

    gnome_vfs_shutdown ();

    return 0;
}
