/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2007 Shaun McCance  <shaunm@gnome.org>
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
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#include <glib.h>

#include "yelp-page.h"
#include "yelp-error.h"

#define READ_SIZE 1024

static gchar *file = NULL;
static const GOptionEntry options[] = {
    { G_OPTION_REMAINING, 
      0, 0, G_OPTION_ARG_FILENAME, 
      &file, NULL, NULL },
    { NULL }
};

gint 
main (gint argc, gchar **argv) 
{
    GOptionContext *context;
    YelpPage *page;
    YelpError *error;
    gchar buffer[READ_SIZE];
    gsize read;
    gint num_reads;

    context = g_option_context_new ("[FILE]");
    g_option_context_add_main_entries (context, options, NULL);
    g_option_context_parse (context, &argc, &argv, NULL);

    if (file == NULL) {
	gint i;
	gchar *str, *new;
	str = g_strdup ("Fe fi fo fum. I smell the blood of an Englishman.\n");
	for (i = 0; i < 5; i++) {
	    new = g_strconcat (str, str, str, str, str, str, str, NULL);
	    g_free (str);
	    str = new;
	}
	page = yelp_page_new_string (NULL, NULL, str);
    } else {
	g_error ("File pages not yet supported.\n");
	return 1;
    }

    num_reads = 0;
    while (yelp_page_read (page, buffer, READ_SIZE, &read, &error)
	   == G_IO_STATUS_NORMAL) {
	gchar *str;
	num_reads++;
	/* buffer isn't NULL-terminated */
	str = g_strndup (buffer, READ_SIZE);
	printf ("%s", str);
	g_free (str);
    }

    printf ("\n%i total reads\n", num_reads);
    printf ("%i bytes in last read\n", read);

    yelp_page_free (page);

    return 0;
}
