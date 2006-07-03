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

#include "yelp-error.h"
#include "yelp-transform.h"

static void   chunk_func (YelpTransform      *transform,
			  YelpTransformChunk *chunk,
			  gpointer            user_data);
static void   error_func (YelpTransform      *transform,
			  YelpError          *error,
			  gpointer            user_data);
static void   final_func (YelpTransform      *transform,
			  gpointer            user_data);

GMainLoop *loop;

static void
chunk_func (YelpTransform      *transform,
	    YelpTransformChunk *chunk,
	    gpointer            user_data)
{
  printf ("CHUNK: %s\n", chunk->id);
  printf ("  %s\n", chunk->title);
}

static void
error_func (YelpTransform *transform,
	    YelpError     *error,
	    gpointer       user_data)
{
  printf ("ERROR: %s\n", error->title);
}

static void
final_func (YelpTransform *transform,
	    gpointer       user_data)
{
  printf ("FINAL\n");
  g_main_loop_quit (loop);
}


gint 
main (gint argc, gchar **argv) 
{
  xmlParserCtxtPtr parser;
  xmlDocPtr doc;
  YelpTransform *transform;
  gchar **params;

  g_thread_init (NULL);

  if (argc < 2) {
    g_error ("Usage: test-transform file\n");
    return 1;
  }

  params = g_new0 (gchar *, 7);
  params[0] = "db.chunk.extension";
  params[1] = "\"\"";
  params[2] = "db.chunk.info_basename";
  params[3] = "\"x-yelp-titlepage\"";
  params[4] = "db.chunk.max_depth";
  params[5] = "2";
  params[6] = NULL;

  transform = yelp_transform_new (DATADIR"/yelp/xslt/db2html.xsl",
				  chunk_func,
				  error_func,
				  final_func,
				  NULL);
  parser = xmlNewParserCtxt ();
  doc = xmlCtxtReadFile (parser,
			 argv[1],
			 NULL,
			 XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
			 XML_PARSE_NOENT   | XML_PARSE_NONET   );
  xmlXIncludeProcessFlags (doc,
			   XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
			   XML_PARSE_NOENT   | XML_PARSE_NONET   );
  yelp_transform_start (transform, doc, params);

  g_timeout_add (400, (GSourceFunc) yelp_transform_release, transform);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  return 0;
}
