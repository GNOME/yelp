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

static void 
print_doc_info (YelpDocInfo *doc)
{
    gchar *type, *uri, *file;
    gboolean hasfile = FALSE;
    gint i, max, tmp;

    switch (yelp_doc_info_get_type(doc)) {
    case YELP_DOC_TYPE_ERROR:
	type = "YELP_DOC_TYPE_ERROR";
	break;
    case YELP_DOC_TYPE_DOCBOOK_XML:
	type = "YELP_DOC_TYPE_DOCBOOK_XML";
	break;
    case YELP_DOC_TYPE_DOCBOOK_SGML:
	type = "YELP_DOC_TYPE_DOCBOOK_SGML";
	break;
    case YELP_DOC_TYPE_HTML:
	type = "YELP_DOC_TYPE_HTML";
	break;
    case YELP_DOC_TYPE_MAN:
	type = "YELP_DOC_TYPE_MAN";
	break;
    case YELP_DOC_TYPE_INFO:
	type = "YELP_DOC_TYPE_INFO";
	break;
    case YELP_DOC_TYPE_TOC:
	type = "YELP_DOC_TYPE_DOC";
	break;
    case YELP_DOC_TYPE_EXTERNAL:
	type = "YELP_DOC_TYPE_EXTERNAL";
	break;
    }

    printf ("Address:  %i\n", (guint) doc);
    printf ("Type:     %s\n", type);

    max = 0;
    tmp = YELP_URI_TYPE_ANY;
    while ((tmp = tmp >> 1))
	max++;

    for (i = 0; i <= max; i++) {
	uri = yelp_doc_info_get_uri (doc, NULL, 1 << i);
	if (uri) {
	    printf ("URI:      %s\n", uri);
	    if ((1 << i) == YELP_URI_TYPE_FILE)
		hasfile = TRUE;
	    g_free (uri);
	}
    }

    if (hasfile) {
	file = yelp_doc_info_get_filename (doc);
	printf ("Filename: %s\n", file);
	g_free (file);
    }
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
