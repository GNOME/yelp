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

#include <gtk/gtk.h>
#include <stdio.h>

#include "yelp-pager.h"
#include "yelp-db-pager.h"

void     list_sections     (YelpPager  *pager);
void     save_chunk        (YelpPager  *pager,
			    gchar      *chunk_id);

gint 
main (gint argc, gchar **argv) 
{
    YelpURI   *uri;
    YelpPager *pager;

    if (argc < 2) {
	return 1;
    }

    g_type_init ();
    g_thread_init (NULL);

    uri = yelp_uri_new (argv[1]);
    pager = yelp_db_pager_new (uri);

    g_signal_connect (pager,
		      "sections",
		      G_CALLBACK (list_sections),
		      NULL);

    g_signal_connect (pager,
		      "chunk",
		      G_CALLBACK (save_chunk),
		      NULL);

    yelp_pager_start (pager);

    gtk_main ();

    return 0;
}

void save_chunk (YelpPager *pager, gchar *chunk_id)
{
    printf("save_chunk: %s\n", chunk_id);
}

void
list_sections (YelpPager *pager)
{
    const GtkTreeModel *sections;

    sections = yelp_pager_get_sections (pager);

    printf ("%i\n", gtk_tree_model_get_n_columns (sections));
}
