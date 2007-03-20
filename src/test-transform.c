/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2006 Shaun McCance
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
#include <libxml/parser.h>
#include <libxml/xinclude.h>

#include "yelp-error.h"
#include "yelp-transform.h"

static gint timeout = -1;
static gchar **files = NULL;
static const GOptionEntry options[] = {
    { "timeout", 't',
      0, G_OPTION_ARG_INT,
      &timeout,
      "Time out after N milliseconds", "N"
    },
    { G_OPTION_REMAINING, 
      0, 0, G_OPTION_ARG_FILENAME_ARRAY, 
      &files, NULL, NULL },
    { NULL }
};

GMainLoop *loop;

static void
transform_release (YelpTransform *transform)
{
    static gboolean released = FALSE;

    if (!released) {
	printf ("\nRELEASE\n");
	yelp_transform_release (transform);
	released = TRUE;
    }
}

static void
transform_func (YelpTransform       *transform,
		YelpTransformSignal  signal,
		gpointer            *func_data,
		gpointer             user_data)
{
    gchar *chunk_id;
    gchar *chunk_data;
    gchar *chunk_short;
    YelpError *error;

    switch (signal) {
    case YELP_TRANSFORM_CHUNK:
	chunk_id = (gchar *) func_data;
	printf ("\nCHUNK: %s\n", chunk_id);

	chunk_data = yelp_transform_eat_chunk (transform, chunk_id);
	chunk_short = g_strndup (chunk_data, 300);
	printf ("%s\n", chunk_short);

	g_free (chunk_short);
	g_free (chunk_data);
	g_free (chunk_id);
	break;
    case YELP_TRANSFORM_ERROR:
	error = (YelpError *) func_data;
	printf ("\nERROR: %s\n", yelp_error_get_title (error));
	yelp_error_free (error);
	break;
    case YELP_TRANSFORM_FINAL:
	printf ("\nFINAL\n");
	transform_release (transform);
	g_main_loop_quit (loop);
	break;
    }
}

gint 
main (gint argc, gchar **argv) 
{
    GOptionContext *context;
    xmlParserCtxtPtr parser;
    xmlDocPtr doc;
    YelpTransform *transform;
    gchar **params;
    gchar *stylesheet;
    gchar *file;

    g_thread_init (NULL);

    context = g_option_context_new ("[STYLESHEET] FILE");
    g_option_context_add_main_entries (context, options, NULL);
    g_option_context_parse (context, &argc, &argv, NULL);

    if (files == NULL || files[0] == NULL) {
	g_printerr ("Usage: test-transform [OPTION...] [STYLESHEET] FILE\n");
	return 1;
    }

    if (files[1] == NULL) {
	stylesheet = DATADIR"/yelp/xslt/db2html.xsl";
	file = files[0];
    } else {
	stylesheet = files[0];
	file = files[1];
    }

    params = g_new0 (gchar *, 7);
    params[0] = "db.chunk.extension";
    params[1] = "\"\"";
    params[2] = "db.chunk.info_basename";
    params[3] = "\"x-yelp-titlepage\"";
    params[4] = "db.chunk.max_depth";
    params[5] = "2";
    params[6] = NULL;

    transform = yelp_transform_new (stylesheet,
				    (YelpTransformFunc) transform_func,
				    NULL);
    parser = xmlNewParserCtxt ();
    doc = xmlCtxtReadFile (parser,
			   file,
			   NULL,
			   XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
			   XML_PARSE_NOENT   | XML_PARSE_NONET   );
    xmlFreeParserCtxt (parser);
    xmlXIncludeProcessFlags (doc,
			     XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
			     XML_PARSE_NOENT   | XML_PARSE_NONET   );
    yelp_transform_start (transform, doc, params);

    if (timeout >= 0)
	g_timeout_add (timeout, (GSourceFunc) transform_release, transform);

    loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);

    return 0;
}
