/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2009 Shaun McCance  <shaunm@gnome.org>
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include "yelp-view.h"
#include "yelp-uri.h"
#include "yelp-simple-document.h"

static void
activate_cb (GtkEntry *entry,
	     YelpView *view)
{
    /* I put in the double-load to test some race condition bugs.
     * I decided to leave it in.
     */
    yelp_view_load (view, gtk_entry_get_text (entry));
    yelp_view_load (view, gtk_entry_get_text (entry));
}

static void
state_cb (YelpView   *view,
	  GParamSpec *spec,
	  GtkWindow  *window)
{
    YelpViewState state;
    g_object_get (view, "state", &state, NULL);
    printf ("STATE: %i\n", (int)state);
}

static void
title_cb (YelpView   *view,
	  GParamSpec *spec,
	  GtkWindow  *window)
{
    gchar *title;
    g_object_get (view, "page-title", &title, NULL);
    gtk_window_set_title (window, title);
    g_free (title);
}

int
main (int argc, char **argv)
{
    GtkWidget *window, *vbox, *entry, *scroll, *view;

    gtk_init (&argc, &argv);

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint ((GtkWindow *) window,
			      GDK_WINDOW_TYPE_HINT_UTILITY);
    gtk_window_set_default_size (GTK_WINDOW (window), 520, 580);
    g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add (GTK_CONTAINER (window), vbox);

    entry = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);

    view = yelp_view_new ();
    g_signal_connect (view, "notify::state",
		      G_CALLBACK (state_cb), window);
    g_signal_connect (view, "notify::title",
		      G_CALLBACK (title_cb), window);
    gtk_container_add (GTK_CONTAINER (scroll), view);


    g_signal_connect (entry, "activate", G_CALLBACK (activate_cb), view);

    if (argc >= 2) {
	/* I put in the double-load to test some race condition bugs.
	 * I decided to leave it in.
	 */
	yelp_view_load (YELP_VIEW (view), argv[1]);
	yelp_view_load (YELP_VIEW (view), argv[1]);
    }

    gtk_widget_show_all (GTK_WIDGET (window));

    gtk_main ();

    return 0;
}
