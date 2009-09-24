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
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#include <gtk/gtk.h>
#include <webkit/webkit.h>

#include "yelp-location-entry.h"
#include "yelp-view.h"
#include "yelp-uri.h"
#include "yelp-simple-document.h"

static void
activate_cb (GtkEntry *entry,
	     YelpView *view)
{
    yelp_view_load (view, gtk_entry_get_text (entry));
}

int
main (int argc, char **argv)
{
    GtkWidget *window, *vbox, *entry, *scroll, *view;
    YelpUri *uri;
    YelpDocument *document;
    GCancellable *cancellable;

    gtk_init (&argc, &argv);

    g_thread_init (NULL);

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (window), vbox);

    entry = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);

    view = yelp_view_new ();
    gtk_container_add (GTK_CONTAINER (scroll), view);

    g_signal_connect (entry, "activate", activate_cb, view);

    if (argc >= 2)
	yelp_view_load (view, argv[1]);

    gtk_widget_show_all (GTK_WIDGET (window));

    gtk_main ();
}
