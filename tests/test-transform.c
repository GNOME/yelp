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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#include <config.h>

#include <glib.h>
#include <libxml/parser.h>
#include <libxml/xinclude.h>

#include "yelp-error.h"
#include "yelp-transform.h"

static gint num_chunks = 0;
static gboolean freed;

static gint timeout = -1;
static gboolean random_timeout = FALSE;
static gchar **files = NULL;
static const GOptionEntry options[] = {
    { "random-timeout", 'r',
      0, G_OPTION_ARG_NONE,
      &random_timeout,
      "Time out after a random amount of time", NULL },
    { "timeout", 't',
      0, G_OPTION_ARG_INT,
      &timeout,
      "Time out after N milliseconds", "N" },
    { G_OPTION_REMAINING, 
      0, 0, G_OPTION_ARG_FILENAME_ARRAY, 
      &files, NULL, NULL },
    { NULL }
};

GMainLoop *loop;

static gboolean
transform_release (YelpTransform *transform)
{
    printf ("\nRELEASE\n");
    if (!freed) {
	yelp_transform_cancel (transform);
	g_object_unref (transform);
    }
    freed = TRUE;
    return FALSE;
}

static void
transform_chunk (YelpTransform *transform,
		 const gchar   *chunk_id,
		 gpointer       user_data)
{
    gchar *chunk, *small;
    num_chunks++;
    printf ("\nCHUNK %i: %s\n", num_chunks, chunk_id);

    chunk = yelp_transform_take_chunk (transform, chunk_id);
    small = g_strndup (chunk, 300);
    printf ("%s\n", small);

    g_free (small);
    g_free (chunk);
}

static void
transform_finished (YelpTransform *transform,
		    gpointer       user_data)
{
    printf ("\nFINAL\n");
    if (!freed) {
	yelp_transform_cancel (transform);
	g_object_unref (transform);
    }
    freed = TRUE;
}

static void
transform_error (YelpTransform *transform,
		 gpointer       user_data)
{
    printf ("\nERROR\n");
}

static void
transform_destroyed (gpointer  data,
		     GObject  *object)
{
    printf ("\nFREED\n");
    g_main_loop_quit (loop);
}

gint 
main (gint argc, gchar **argv) 
{
    GOptionContext *context;
    xmlParserCtxtPtr parser;
    xmlDocPtr doc;
    YelpTransform *transform;
    gchar **params;
    const gchar *stylesheet;
    gchar *file;

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
    params[0] = g_strdup ("db.chunk.extension");
    params[1] = g_strdup ("\"\"");
    params[2] = g_strdup ("db.chunk.info_basename");
    params[3] = g_strdup ("\"x-yelp-titlepage\"");
    params[4] = g_strdup ("db.chunk.max_depth");
    params[5] = g_strdup ("2");
    params[6] = NULL;

    transform = yelp_transform_new (stylesheet);
    g_object_weak_ref ((GObject *) transform, transform_destroyed, NULL);

    g_signal_connect (transform, "chunk-ready", (GCallback) transform_chunk, NULL);
    g_signal_connect (transform, "finished", (GCallback) transform_finished, NULL);
    g_signal_connect (transform, "error", (GCallback) transform_error, NULL);

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
    if (!yelp_transform_start (transform, doc, NULL, (const gchar **) params))
	return 1;

    if (random_timeout) {
	GRand *rand = g_rand_new ();
	timeout = g_rand_int_range (rand, 80, 280);
	g_rand_free (rand);
    }

    if (timeout >= 0)
	g_timeout_add (timeout, (GSourceFunc) transform_release, transform);

    loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);

    return 0;
}
