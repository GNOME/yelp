/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 CodeFactory AB
 * Copyright (C) 2001 Mikael Hallendal <micke@codefactory.se>
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
 * Author: Mikael Hallendal <micke@codefactory.se>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <libgtkhtml/gtkhtml.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-about.h>
#include <string.h>
#include "ghelp-uri.h"
#include "yelp-index.h"
#include "yelp-toc.h"
#include "yelp-view.h"
#include "yelp-window.h"
#include "yelp-book.h"
#include "metadata.h"

static void yelp_window_init		         (YelpWindow          *window);
static void yelp_window_class_init	         (YelpWindowClass     *klass);

static void yelp_window_populate                 (YelpWindow          *window);

static void yelp_window_uri_entry_activate_cb    (GtkWidget           *widget, 
						  YelpWindow          *window);

static void yelp_window_exit                     (gpointer             data,
						  guint                section,
						  GtkWidget           *widget);
static void yelp_window_new_window               (gpointer             data,
						  guint                section,
						  GtkWidget           *widget);
static void yelp_window_uri_selected_cb          (YelpWindow          *window,
						  GnomeVFSURI         *uri);
static void yelp_window_about                    (gpointer             data,
						  guint                section,
						  GtkWidget           *widget);

struct _YelpWindowPriv {
	YelpBase       *base;
	
        GtkWidget      *hpaned;
        GtkWidget      *notebook;
	GtkWidget      *uri_entry;

        GtkWidget      *yelp_view;
        GtkWidget      *yelp_toc;

	YelpIndex      *index;

	GtkItemFactory *item_factory;
};

static GtkItemFactoryEntry menu_items[] = {
	{ "/_File",        NULL,           0, 0,                 "<Branch>" },
	{ "/File/_New window", "<Control>N", yelp_window_new_window, 0, NULL},
	{ "/File/E_xit",   "<Control>Q",   yelp_window_exit,  0, NULL       },
	{ "/_Help",        NULL,           0, 0,                 "<Branch>" },
	{ "/Help/_About",  NULL,           yelp_window_about, 0, NULL       },
};

GType
yelp_window_get_type (void)
{
        static GType window_type = 0;

        if (!window_type) {
                static const GTypeInfo window_info =
                        {
                                sizeof (YelpWindowClass),
                                NULL,
                                NULL,
                                (GClassInitFunc) yelp_window_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpWindow),
                                0,
                                (GInstanceInitFunc) yelp_window_init,
                        };
                
                window_type = g_type_register_static (GTK_TYPE_WINDOW, 
                                                      "YelpWindow", 
                                                      &window_info, 0);
        }
        
        return window_type;
}

static void
yelp_window_init (YelpWindow *window)
{
        YelpWindowPriv *priv;
       
        priv = g_new0 (YelpWindowPriv, 1);
        window->priv = priv;
        
        priv->index = yelp_index_new ();

	g_signal_connect_swapped (priv->index, "uri_selected",
				  G_CALLBACK (yelp_window_uri_selected_cb),
				  G_OBJECT (window));

        gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);

	gtk_window_set_title (GTK_WINDOW (window), "Yelp: GNOME Help Browser");
}

static void
yelp_window_class_init (YelpWindowClass *klass)
{
        GtkObjectClass *object_class;
        
        object_class = (GtkObjectClass*) klass;
}

static void
yelp_window_populate (YelpWindow *window)
{
        YelpWindowPriv *priv;
	GtkWidget      *vbox;
        GtkWidget      *html_sw;
        GtkWidget      *tree_sw;
        GtkWidget      *search_list_sw;
        GtkWidget      *search_box;
	GtkWidget      *frame;
	GtkWidget      *main_box;
		
        priv = window->priv;

	main_box           = gtk_vbox_new (FALSE, 0);
 	html_sw            = gtk_scrolled_window_new (NULL, NULL);
        tree_sw            = gtk_scrolled_window_new (NULL, NULL);
        search_list_sw     = gtk_scrolled_window_new (NULL, NULL); 
        search_box         = gtk_vbox_new (FALSE, 0);

        priv->notebook     = gtk_notebook_new ();
        priv->hpaned       = gtk_hpaned_new ();

	priv->item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR,
						   "<main>", NULL);
	gtk_container_add (GTK_CONTAINER (window), main_box);
	
	gtk_item_factory_create_items (priv->item_factory,
				       G_N_ELEMENTS (menu_items),
				       menu_items,
				       window);

	gtk_box_pack_start (GTK_BOX (main_box),
			    gtk_item_factory_get_widget (priv->item_factory,
							 "<main>"),
			    FALSE, FALSE, 0);

	vbox = gtk_vbox_new (FALSE, 3);
	priv->uri_entry = gtk_entry_new ();
	g_signal_connect (G_OBJECT (priv->uri_entry),
			  "activate",
			  G_CALLBACK (yelp_window_uri_entry_activate_cb),
			  window);

	gtk_box_pack_start (GTK_BOX (vbox), priv->uri_entry, FALSE, FALSE, 0);

        /* Html View */
        priv->yelp_view = yelp_view_new ();

	g_signal_connect_swapped (priv->yelp_view, "uri_selected",
				  G_CALLBACK (yelp_window_uri_selected_cb),
				  G_OBJECT (window));
	
        gtk_container_add (GTK_CONTAINER (html_sw), priv->yelp_view);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (html_sw),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        


	frame = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (frame), html_sw);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0),
        gtk_paned_add2 (GTK_PANED (priv->hpaned), vbox);

        /* Tree */
        priv->yelp_toc = yelp_toc_new (yelp_base_get_bookshelf (priv->base));

        gtk_container_add (GTK_CONTAINER (tree_sw), priv->yelp_toc);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (tree_sw),
                                        GTK_POLICY_AUTOMATIC, 
                                        GTK_POLICY_AUTOMATIC);

        gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), 
                                  tree_sw, gtk_label_new ("Contents"));

	g_signal_connect_swapped (G_OBJECT (priv->yelp_toc), "uri_selected",
				  G_CALLBACK (yelp_window_uri_selected_cb),
				  G_OBJECT (window));

        /* Search */
        gtk_container_add (GTK_CONTAINER (search_list_sw), 
			   yelp_index_get_list (priv->index));
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (search_list_sw),
                                        GTK_POLICY_AUTOMATIC, 
                                        GTK_POLICY_AUTOMATIC);
        gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), 
                                  search_box, gtk_label_new ("Index"));

        gtk_box_pack_start (GTK_BOX (search_box), 
			               yelp_index_get_entry (priv->index),
                            FALSE, FALSE, 0);
        gtk_box_pack_end_defaults (GTK_BOX (search_box), search_list_sw);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
        gtk_container_add (GTK_CONTAINER (frame), priv->notebook);
        gtk_paned_add1 (GTK_PANED (priv->hpaned), frame);
        
        gtk_paned_set_position (GTK_PANED (priv->hpaned), 250);

	gtk_box_pack_start (GTK_BOX (main_box), priv->hpaned,
			    TRUE, TRUE, 0);
}

static void
yelp_window_uri_entry_activate_cb (GtkWidget *widget, YelpWindow *window)
{
	YelpWindowPriv *priv;
	GnomeVFSURI    *uri;
	const gchar    *str;
	
	priv = window->priv;

	str = gtk_entry_get_text (GTK_ENTRY (widget));
	
 	uri = gnome_vfs_uri_new (str);

/* 	yelp_view_open_uri (YELP_VIEW (priv->yelp_view), str); */
 	yelp_view_open_uri (YELP_VIEW (priv->yelp_view), uri);
 	yelp_toc_open (YELP_TOC (priv->yelp_toc), uri);
	
 	gnome_vfs_uri_unref (uri);
}

static void
yelp_window_exit (gpointer data, guint section, GtkWidget *widget)
{
	gtk_main_quit ();
}

static void
yelp_window_new_window (gpointer data, guint section, GtkWidget *widget)
{
	YelpWindow *window;
	GtkWidget  *new_window;
	
	g_return_if_fail (YELP_IS_WINDOW (data));
	
	window = YELP_WINDOW (data);
	
	new_window = yelp_base_new_window (window->priv->base);
	
	gtk_widget_show_all (new_window);
}

static void
yelp_window_uri_selected_cb (YelpWindow        *window,
			     GnomeVFSURI       *uri)
{
	YelpWindowPriv *priv;
	gchar          *str_uri;
	
	priv = window->priv;

	if (!uri) {
		return;
	}

	str_uri = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
	gtk_entry_set_text (GTK_ENTRY (priv->uri_entry), str_uri);
	g_free (str_uri);

	yelp_view_open_uri (YELP_VIEW (window->priv->yelp_view), uri);
}

static void
yelp_window_about (gpointer data, guint section, GtkWidget *widget)
{
	GtkWidget *about;
	const gchar *authors[] = { 
		"Mikael Hallendal <micke@codefactory.se>",
		NULL
	};
	
	about = gnome_about_new (PACKAGE, VERSION,
				 "(C) 2001 Mikael Hallendal <micke@codefactory.se>",
				 "Help Browser for GNOME 2.0",
				 authors,
				 NULL,
				 NULL,
				 NULL);

	gtk_widget_show (about);
	g_print ("Show about\n");
}

GtkWidget *
yelp_window_new (YelpBase *base)
{
	YelpWindow *window;

	window = g_object_new (YELP_TYPE_WINDOW, NULL);

	window->priv->base = base;

        yelp_window_populate (window);

        return GTK_WIDGET (window);
}

void
yelp_window_open_uri (YelpWindow  *window,
		      const gchar *str_uri)
{
	YelpWindowPriv *priv;
	GnomeVFSURI    *uri;
	
	g_return_if_fail (YELP_IS_WINDOW (window));
	
	priv = window->priv;
	
	gtk_entry_set_text (GTK_ENTRY (priv->uri_entry), str_uri);

	uri = gnome_vfs_uri_new (str_uri);
	yelp_view_open_uri (YELP_VIEW (priv->yelp_view), uri);
	gnome_vfs_uri_unref (uri);
}

