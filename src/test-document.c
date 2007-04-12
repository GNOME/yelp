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
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/xinclude.h>

#include "yelp-error.h"
#include "yelp-docbook.h"

static gchar *mode = NULL;
static gchar **files = NULL;
static const GOptionEntry options[] = {
    { "mode", 'm',
      0, G_OPTION_ARG_STRING,
      &mode,
      "One of man or docbook", "MODE" },
    { G_OPTION_REMAINING,
      0, 0, G_OPTION_ARG_FILENAME_ARRAY,
      &files, NULL, NULL },
    { NULL }
};

static void   document_func (YelpDocument       *document,
			     YelpDocumentSignal  signal,
			     gint                req_id,
			     gpointer           *func_data,
			     gpointer            user_data);

GMainLoop *loop;

static void
document_func (YelpDocument       *document,
	       YelpDocumentSignal  signal,
	       gint                req_id,
	       gpointer           *func_data,
	       gpointer            user_data)
{
    gchar contents[60];
    gchar *contents_;
    gsize read;
    YelpPage *page;
    YelpError *error;
    switch (signal) {
    case YELP_DOCUMENT_SIGNAL_PAGE:
	page = (YelpPage *) func_data;
	printf ("PAGE: %s (%i)\n", page->id, req_id);
	printf ("  PREV: %s\n", page->prev_id);
	printf ("  NEXT: %s\n", page->next_id);
	printf ("  UP:   %s\n", page->up_id);
	printf ("  ROOT: %s\n", page->root_id);
	yelp_page_read (page, contents, 60, &read, NULL);
	/* contents isn't \0-terminated */
	contents_ = g_strndup (contents, read);
	printf ("  DATA: %s\n", contents_);
	g_free (contents_);
	yelp_page_free (page);
	break;
    case YELP_DOCUMENT_SIGNAL_TITLE:
	printf ("TITLE: %s (%i)\n", (gchar *) func_data, req_id);
	g_free (func_data);
	break;
    case YELP_DOCUMENT_SIGNAL_ERROR:
	error = (YelpError *) func_data;
	printf ("ERROR: %s\n", yelp_error_get_title (error));
	printf ("  %s\n", yelp_error_get_message (error));
	yelp_error_free (error);
	break;
    }
}

gint 
main (gint argc, gchar **argv) 
{
    GOptionContext *context;
    YelpDocument *document;
    gint i;

    g_thread_init (NULL);
    gdk_threads_init ();
    gdk_threads_leave ();

    gtk_init (&argc, &argv);

    context = g_option_context_new ("FILE PAGE_IDS...");
    g_option_context_add_main_entries (context, options, NULL);
    g_option_context_parse (context, &argc, &argv, NULL);

    if (files == NULL || files[0] == NULL) {
	g_printerr ("Usage: test-docbook FILE PAGE_IDS...\n");
	return 1;
    }

    if (g_str_equal (mode, "man"))
	document = yelp_man_new (files[0]);
    else
	document = yelp_docbook_new (files[0]);

    if (files[1] == NULL)
	yelp_document_get_page (document, "x-yelp-index", (YelpDocumentFunc) document_func, NULL);
    else
	for (i = 1; files[i]; i++)
	    yelp_document_get_page (document, files[i], (YelpDocumentFunc) document_func, NULL);

    loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);

    gdk_threads_leave ();

    return 0;
}
