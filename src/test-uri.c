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

#include "yelp-utils.h"
#include "yelp-uri.h"

static void 
print_doc_info (YelpDocInfo *doc)
{
    gchar *type, *file;

    switch (doc->type) {
    case YELP_TYPE_ERROR:
	type = "YELP_TYPE_ERROR";
	break;
    case YELP_TYPE_DOCBOOK_XML:
	type = "YELP_TYPE_DOCBOOK_XML";
	break;
    case YELP_TYPE_DOCBOOK_SGML:
	type = "YELP_TYPE_DOCBOOK_SGML";
	break;
    case YELP_TYPE_HTML:
	type = "YELP_TYPE_HTML";
	break;
    case YELP_TYPE_MAN:
	type = "YELP_TYPE_MAN";
	break;
    case YELP_TYPE_INFO:
	type = "YELP_TYPE_INFO";
	break;
    case YELP_TYPE_TOC:
	type = "YELP_TYPE_DOC";
	break;
    case YELP_TYPE_EXTERNAL:
	type = "YELP_TYPE_EXTERNAL";
	break;
    }

    file = yelp_doc_info_get_filename (doc);

    printf ("Address:  %i\n"
	    "URI:      %s\n"
	    "Type:     %s\n"
	    "Filename: %s\n",
	    (gint) doc,
	    doc->uri,
	    type,
	    file);

    g_free (file);
}

int
main (int argc, char **argv)
{
    GnomeProgram *program;
    YelpDocInfo  *doc;
    gint i;
	
    if (argc < 2) {
	g_print ("Usage: test-uri uri\n");
	return 1;
    }

    program = gnome_program_init (PACKAGE, VERSION,
				  LIBGNOME_MODULE, argc, argv,
				  GNOME_PROGRAM_STANDARD_PROPERTIES,
				  NULL);

    gnome_vfs_init ();

    for (i = 1; i < argc; i++) {
	if (i != 1)
	    printf ("\n");
	doc = yelp_doc_info_get (argv[i]);
	if (doc)
	    print_doc_info (doc);
	else
	    printf ("Failed to load URI: %s\n", argv[i]);
    }

    gnome_vfs_shutdown ();

    return 0;
}
