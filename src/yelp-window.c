/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
#include <libgnome/gnome-i18n.h>
#include <string.h>
#include "yelp-index.h"
#include "yelp-toc.h"
#include "yelp-view-doc.h"
#include "yelp-window.h"
#include "yelp-section.h"
#include "yelp-history.h"

static void yelp_window_init		       (YelpWindow          *window);
static void yelp_window_class_init	       (YelpWindowClass     *klass);

static void yelp_window_populate               (YelpWindow          *window);

static void yelp_window_close_cb               (gpointer             data,
						guint                section,
						GtkWidget           *widget);

static void yelp_window_section_selected_cb    (YelpWindow          *window,
						YelpSection         *section);
static void yelp_window_toggle_history_buttons (GtkWidget           *button,
						gboolean             sensitive,
						YelpHistory         *history);
static void yelp_window_history_button_pressed (GtkWidget           *button,
						YelpWindow          *window);

struct _YelpWindowPriv {
	YelpBase       *base;
	
        GtkWidget      *yelp_view_doc;
        GtkWidget      *yelp_toc;

	YelpIndex      *index;
	YelpHistory    *history;

	GtkWidget      *forward_button;
	GtkWidget      *back_button;
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
        
        priv->index   = yelp_index_new ();

	g_signal_connect_swapped (priv->index, "section_selected",
				  G_CALLBACK (yelp_window_section_selected_cb),
				  G_OBJECT (window));

	priv->history = yelp_history_new ();
	
        gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);

	gtk_window_set_title (GTK_WINDOW (window), _("Yelp: GNOME Help Browser"));
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
        GtkWidget      *html_sw;
        GtkWidget      *tree_sw;
        GtkWidget      *search_list_sw;
        GtkWidget      *search_box;
	GtkWidget      *frame;
	GtkWidget      *main_box;
	GtkWidget      *toolbar;
	GtkWidget      *button;
	GtkWidget      *hpaned;
	
        priv = window->priv;

	main_box        = gtk_vbox_new (FALSE, 0);
 	html_sw         = gtk_scrolled_window_new (NULL, NULL);
        tree_sw         = gtk_scrolled_window_new (NULL, NULL);
        search_list_sw  = gtk_scrolled_window_new (NULL, NULL); 
        search_box      = gtk_vbox_new (FALSE, 0);
        hpaned          = gtk_hpaned_new ();
	
	gtk_container_add (GTK_CONTAINER (window), main_box);
	
	toolbar = gtk_toolbar_new ();

	priv->back_button = gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
						      "gtk-go-back",
						      "", "",
						      NULL, NULL, -1);

	gtk_widget_set_sensitive (priv->back_button, FALSE);

	g_signal_connect_swapped (priv->history, "back_exists_changed",
				  G_CALLBACK (yelp_window_toggle_history_buttons),
				  G_OBJECT (priv->back_button));

	g_signal_connect (priv->back_button, "clicked",
			  G_CALLBACK (yelp_window_history_button_pressed),
			  G_OBJECT (window));
	
	priv->forward_button = gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
							 "gtk-go-forward",
							 "", "",
							 NULL, NULL, -1);

	button = gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar), 
					   "gtk-home",
					   "", "",
					   NULL, NULL, -1);

	/* Connect to Home-button */

	gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

	button = gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
					   "gtk-index",
					   "", "",
					   NULL, NULL, -1);
	
	gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));
	
	button = gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
					   "gtk-find",
					   "", "",
					   NULL, NULL, -1);

	/* Connect to find-button */

	gtk_toolbar_append_widget (GTK_TOOLBAR (toolbar),
				   gtk_label_new (_("Help on ")),
				   "", "");
	
	gtk_toolbar_append_widget (GTK_TOOLBAR (toolbar),
				   gtk_entry_new (),
				   "", "");

	gtk_widget_set_sensitive (priv->forward_button, FALSE);

	g_signal_connect_swapped (priv->history, "forward_exists_changed",
 				  G_CALLBACK (yelp_window_toggle_history_buttons),
				  G_OBJECT (priv->forward_button));
	
	g_signal_connect (priv->forward_button, "clicked",
			  G_CALLBACK (yelp_window_history_button_pressed),
			  G_OBJECT (window));

	button = gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
					   "gtk-close",
					   "", "",
					   NULL, NULL, -1);
	
	g_signal_connect (button, "clicked",
			  G_CALLBACK (yelp_window_close_cb),
			  G_OBJECT (window));
	
	gtk_box_pack_start (GTK_BOX (main_box), toolbar, FALSE, FALSE, 0);

        /* Doc View */
        priv->yelp_view_doc = yelp_view_doc_new ();

	g_signal_connect_swapped (priv->yelp_view_doc, "section_selected",
				  G_CALLBACK (yelp_window_section_selected_cb),
				  G_OBJECT (window));
	
        gtk_container_add (GTK_CONTAINER (html_sw), priv->yelp_view_doc);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (html_sw),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);

	frame = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (frame), html_sw);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
        gtk_paned_add2 (GTK_PANED (hpaned), frame);

        /* Tree */
        priv->yelp_toc = yelp_toc_new (yelp_base_get_bookshelf (priv->base));

        gtk_container_add (GTK_CONTAINER (tree_sw), priv->yelp_toc);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (tree_sw),
                                        GTK_POLICY_AUTOMATIC, 
                                        GTK_POLICY_AUTOMATIC);

	g_signal_connect_swapped (G_OBJECT (priv->yelp_toc), 
				  "section_selected",
				  G_CALLBACK (yelp_window_section_selected_cb),
				  G_OBJECT (window));

        /* Search */
        gtk_container_add (GTK_CONTAINER (search_list_sw), 
			   yelp_index_get_list (priv->index));

        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (search_list_sw),
                                        GTK_POLICY_AUTOMATIC, 
                                        GTK_POLICY_AUTOMATIC);

        gtk_box_pack_start (GTK_BOX (search_box), 
			    yelp_index_get_entry (priv->index),
                            FALSE, FALSE, 0);

        gtk_box_pack_end_defaults (GTK_BOX (search_box), search_list_sw);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);

	gtk_container_add (GTK_CONTAINER (frame), tree_sw);
	
        gtk_paned_add1 (GTK_PANED (hpaned), frame);
        
        gtk_paned_set_position (GTK_PANED (hpaned), 250);

	gtk_box_pack_start (GTK_BOX (main_box), hpaned, TRUE, TRUE, 0);
}

static void
yelp_window_close_cb (gpointer data, guint section, GtkWidget *widget)
{
	gtk_main_quit ();
}

static void
yelp_window_section_selected_cb (YelpWindow  *window,
				 YelpSection *section)
{
	YelpWindowPriv *priv;
	
	priv = window->priv;

	if (!section->uri) {
		return;
	}

	yelp_history_goto (priv->history, section);

	yelp_view_doc_open_section (YELP_VIEW_DOC (window->priv->yelp_view_doc), section);

}

static void
yelp_window_toggle_history_buttons (GtkWidget   *button, 
				    gboolean     sensitive,
				    YelpHistory *history)
{
	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (YELP_IS_HISTORY (history));

	g_print ("History button toggled\n");
	
	gtk_widget_set_sensitive (button, sensitive);
}

static void
yelp_window_history_button_pressed (GtkWidget *button, YelpWindow *window)
{
	YelpWindowPriv    *priv;
	const YelpSection *section = NULL;
	
	g_return_if_fail (YELP_IS_WINDOW (window));

	priv = window->priv;

	if (button == priv->forward_button) {
		section = yelp_history_go_forward (priv->history);
	}
	else if (button == priv->back_button) {
		section = yelp_history_go_back (priv->history);
	}

	if (section) {
		yelp_view_doc_open_section (YELP_VIEW_DOC (priv->yelp_view_doc), section);
	}
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
	
	g_return_if_fail (YELP_IS_WINDOW (window));
	
	priv = window->priv;
	
	yelp_view_doc_open_section (YELP_VIEW_DOC (priv->yelp_view_doc), 
				    yelp_section_new (NULL, str_uri,
						      NULL, NULL));
}
