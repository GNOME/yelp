/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2002 Shaun McCance
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.,  59 Temple Place - Suite 330, Cambridge, MA 02139, USA.
 *
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#include "yelp-man-parser.h"
#include "yelp-utils.h"

gint 
main (gint argc, gchar **argv) 
{
    YelpManParser *parser;
    YelpDocInfo   *doc_info;
    xmlDocPtr      doc;

    if (argc < 2) {
	g_error ("Usage: test-man-parser file\n");
	return 1;
    }

    parser = yelp_man_parser_new ();
    doc_info = yelp_doc_info_get (argv[1], FALSE);
    if (!doc_info) {
	printf ("Failed to load URI: %s\n", argv[1]);
	return -1;
    }
    doc = yelp_man_parser_parse_doc (parser, doc_info);

    yelp_man_parser_free (parser);

    xmlDocDump (stdout, doc);
    xmlFreeDoc (doc);

    return 0;
}

