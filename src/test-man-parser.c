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

#include <glib.h>
#include <libxml/tree.h>

#include "yelp-man-parser.h"

static gchar **files = NULL;
static const GOptionEntry options[] = {
    { G_OPTION_REMAINING, 
      0, 0, G_OPTION_ARG_FILENAME_ARRAY, 
      &files, NULL, NULL },
    { NULL }
};

gint 
main (gint argc, gchar **argv) 
{
    GOptionContext *context;
    YelpManParser *parser;
    xmlDocPtr doc;
    gchar *encoding;
    gint i;

    context = g_option_context_new ("FILES...");
    g_option_context_add_main_entries (context, options, NULL);
    g_option_context_parse (context, &argc, &argv, NULL);

    if (files == NULL || files[0] == NULL) {
	g_printerr ("Usage: test-man-parser [OPTION...] FILES...\n");
	return 1;
    }

    parser = yelp_man_parser_new ();

    encoding = (gchar *) g_getenv("MAN_ENCODING");
    if (encoding == NULL)
	encoding = "ISO-8859-1";

    for (i = 0; files[i]; i++) {
	doc = yelp_man_parser_parse_file (parser, files[i], encoding);
	xmlDocDump (stdout, doc);
	xmlFreeDoc (doc);
    }

    yelp_man_parser_free (parser);

    return 0;
}
