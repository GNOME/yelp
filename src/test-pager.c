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

#include <libgnomeui/gnome-ui-init.h>
#include <string.h>

#include "config.h"
#include "yelp-pager.h"
#include "yelp-db-pager.h"
#include "yelp-man-pager.h"
#include "yelp-settings.c"
#include "yelp-toc-pager.h"
#include "yelp-utils.h"

static void    pager_parse_cb        (YelpPager    *pager);
static void    pager_start_cb        (YelpPager    *pager);
static void    pager_page_cb         (YelpPager    *pager,
				      gchar        *page_id);
static void    pager_finish_cb       (YelpPager    *pager);
static void    pager_cancel_cb       (YelpPager    *pager);
static void    pager_error_cb        (YelpPager    *pager);

gboolean main_running = TRUE;

gint 
main (gint argc, gchar **argv) 
{
    YelpDocInfo *doc_info;
    YelpPager   *pager;

    if (argc < 2) {
	g_error ("Usage: test-pager file\n");
	return 1;
    }

    gnome_program_init (PACKAGE, VERSION,
			LIBGNOMEUI_MODULE, argc, argv,
			GNOME_PROGRAM_STANDARD_PROPERTIES,
			NULL);
    yelp_toc_pager_init ();
    yelp_settings_init ();

    doc_info = yelp_doc_info_get (argv[1], FALSE);

    if (!doc_info) {
	printf ("Failed to load URI: %s\n", argv[1]);
	return -1;
    }

    switch (yelp_doc_info_get_type (doc_info)) {
    case YELP_DOC_TYPE_DOCBOOK_XML:
	pager = yelp_db_pager_new (doc_info);
	break;
    case YELP_DOC_TYPE_MAN:
	pager = yelp_man_pager_new (doc_info);
	break;
    default:
	printf ("No pager type exists for this URI.\n");
	return 1;
    }

    g_signal_connect (pager, "parse",    G_CALLBACK (pager_parse_cb), NULL);
    g_signal_connect (pager, "start",    G_CALLBACK (pager_start_cb), NULL);
    g_signal_connect (pager, "page",     G_CALLBACK (pager_page_cb), NULL);
    g_signal_connect (pager, "finish",   G_CALLBACK (pager_finish_cb), NULL);
    g_signal_connect (pager, "cancel",   G_CALLBACK (pager_cancel_cb), NULL);
    g_signal_connect (pager, "error",    G_CALLBACK (pager_error_cb), NULL);

    yelp_pager_start (pager);

    gtk_main ();

    return 0;
}

static void
pager_parse_cb (YelpPager    *pager)
{
    printf ("pager_parse_cb\n");
}

static void
pager_start_cb (YelpPager    *pager)
{
    printf ("pager_start_cb\n");
}

static void
pager_page_cb (YelpPager    *pager,
	       gchar        *page_id)
{
    const YelpPage *page = yelp_pager_get_page (pager, page_id);

    printf ("pager_page_cb:\n"
	    "   id:     %s\n"
	    "   title:  %s\n"
	    "   strlen: %d\n",
	    page->page_id,
	    page->title,
	    strlen (page->contents));
}

static void
pager_finish_cb (YelpPager    *pager)
{
    printf ("pager_finish_cb\n");
    gtk_main_quit ();
}

static void
pager_cancel_cb (YelpPager    *pager)
{
    printf ("pager_cancel_cb\n");
    gtk_main_quit ();
}

static void
pager_error_cb (YelpPager    *pager)
{
    GError *error = yelp_pager_get_error (pager);

    if (!error)
	printf ("pager_error_cb: NO ERROR\n");
    else
	printf ("pager_error_cb: %s\n", error->message);

    gtk_main_quit ();
}
