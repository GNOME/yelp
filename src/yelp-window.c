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
#include <bonobo/bonobo-main.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomeui/gnome-about.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>
#include "yelp-html.h"
#include "yelp-util.h"
#include "yelp-section.h"
#include "yelp-history.h"
#include "yelp-view-content.h"
#include "yelp-view-index.h"
#include "yelp-view-toc.h"
#include "yelp-window.h"

#define d(x)

static void        yw_init		     (YelpWindow          *window);
static void        yw_class_init	     (YelpWindowClass     *klass);

static void        yw_populate               (YelpWindow          *window);

static gboolean    yw_handle_url             (YelpWindow          *window,
					      const gchar         *url);
static void        yw_url_selected_cb        (gpointer             view,
					      char                *url,
					      char                *base_url,
					      gboolean             handled,
					      YelpWindow          *window);
static void        yw_toggle_history_buttons (GtkWidget           *button,
					      gboolean             sensitive,
					      YelpHistory         *history);
static void        yw_history_button_clicked (GtkWidget           *button,
					      YelpWindow          *window);
static void        yw_home_button_clicked    (GtkWidget           *button,
					      YelpWindow          *window);
static void        yw_index_button_clicked   (GtkWidget           *button,
					      YelpWindow          *window);
static void        yw_new_window_cb          (gpointer             data,
					      guint                section,
					      GtkWidget           *widget);
static void        yw_close_window_cb        (gpointer             data,
					      guint                section,
					      GtkWidget           *widget);
static void        yw_exit_cb                (gpointer             data,
					      guint                section,
					      GtkWidget           *widget);
static void        yw_about_cb               (gpointer             data,
					      guint                section,
					      GtkWidget           *widget);
static GtkWidget * yw_create_toolbar         (YelpWindow          *window);

enum {
	PAGE_TOC_VIEW,
	PAGE_CONTENT_VIEW,
	PAGE_INDEX_VIEW
};

enum {
	NEW_WINDOW_REQUESTED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

struct _YelpWindowPriv {
	GNode          *doc_tree;

	GtkWidget      *notebook;

	GtkWidget      *toc_view;
	GtkWidget      *content_view;
	GtkWidget      *index_view;
	
	GtkWidget      *view_current;

	YelpHistory    *history;

	GtkWidget      *forward_button;
	GtkWidget      *back_button;
};

static GtkItemFactoryEntry menu_items[] = {
	{N_("/_File"),              NULL,         0,                  0, "<Branch>"},
	{N_("/File/_New window"),   NULL,         yw_new_window_cb,   0, "<StockItem>", GTK_STOCK_NEW   },
	{N_("/File/_Close window"), NULL,         yw_close_window_cb, 0, "<StockItem>", GTK_STOCK_CLOSE },
	{N_("/File/_Quit"),         NULL,         yw_exit_cb,         0, "<StockItem>", GTK_STOCK_QUIT  },
	{N_("/_Help"),              NULL,         0,                  0, "<Branch>"},
	{N_("/Help/_About"),        NULL,         yw_about_cb,        0, NULL },
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
                                (GClassInitFunc) yw_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpWindow),
                                0,
                                (GInstanceInitFunc) yw_init,
                        };
                
                window_type = g_type_register_static (GTK_TYPE_WINDOW,
                                                      "YelpWindow", 
                                                      &window_info, 0);
        }
        
        return window_type;
}

static void
yw_init (YelpWindow *window)
{
        YelpWindowPriv *priv;
       
        priv = g_new0 (YelpWindowPriv, 1);
        window->priv = priv;
        
	priv->toc_view    = NULL;
	priv->content_view = NULL;
	priv->index_view   = NULL;
	priv->view_current = NULL;
	
	priv->history = yelp_history_new ();
	yelp_history_goto (priv->history, "toc:");
	
        gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);

	gtk_window_set_title (GTK_WINDOW (window), _("Help Browser"));
}

static void
yw_class_init (YelpWindowClass *klass)
{
	signals[NEW_WINDOW_REQUESTED] =
		g_signal_new ("new_window_requested",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpWindowClass,
					       new_window_requested),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
yw_populate (YelpWindow *window)
{
        YelpWindowPriv *priv;
	GtkWidget      *toolbar;
	GtkWidget      *main_box;
	GtkWidget      *sw;
	GtkItemFactory *item_factory;
	GtkAccelGroup  *accel_group;
	
        priv = window->priv;

	main_box        = gtk_vbox_new (FALSE, 0);
	
	gtk_container_add (GTK_CONTAINER (window), main_box);

	accel_group  = gtk_accel_group_new ();
	item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, 
					     "<main>", accel_group);

	gtk_item_factory_set_translate_func (item_factory, 
					     (GtkTranslateFunc) gettext,
					     NULL, NULL);

	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

	gtk_item_factory_create_items (item_factory,
				       G_N_ELEMENTS (menu_items),
				       menu_items,
				       window);

	gtk_box_pack_start (GTK_BOX (main_box),
			    gtk_item_factory_get_widget (item_factory,
							 "<main>"),
			    FALSE, FALSE, 0);

	toolbar         = yw_create_toolbar (window);

	priv->notebook  = gtk_notebook_new ();

	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);

	sw = gtk_scrolled_window_new (NULL, NULL);
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	gtk_container_add (GTK_CONTAINER (sw), priv->toc_view);

	gtk_notebook_insert_page (GTK_NOTEBOOK (priv->notebook),
				  sw, NULL, PAGE_TOC_VIEW);
	
	gtk_notebook_insert_page (GTK_NOTEBOOK (priv->notebook),
				  priv->content_view,
				  NULL, PAGE_CONTENT_VIEW);

	gtk_notebook_insert_page (GTK_NOTEBOOK (priv->notebook),
				  priv->index_view,
				  NULL, PAGE_INDEX_VIEW);
	
	gtk_box_pack_start (GTK_BOX (main_box), toolbar, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (main_box), priv->notebook,
			  TRUE, TRUE, 0);
}

static gboolean
yw_handle_url (YelpWindow *window, const gchar *url)
{
	YelpWindowPriv *priv;
	
	priv = window->priv;

	if (strncmp (url, "toc:", 4) == 0) {
		yelp_view_toc_open_url (YELP_VIEW_TOC (priv->toc_view),
					url);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook),
					       PAGE_TOC_VIEW);
		return TRUE;
	} else if (strncmp (url, "man:", 4) == 0 ||
		   strncmp (url, "info:", 5) == 0 ||
		   strncmp (url, "ghelp:", 6) == 0 ||
		   strncmp (url, "path:", 5) == 0) {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook),
					       PAGE_CONTENT_VIEW);
		yelp_view_content_show_uri (YELP_VIEW_CONTENT (priv->content_view),
					    url);
		return TRUE;
	} else {
		g_warning ("Unhandled URL: %s\n", url);
		/* TODO: Open external url in mozilla/evoltion etc */
	}

	return FALSE;
}

static void
yw_url_selected_cb (gpointer    view,
		    char       *url,
		    char       *base_url,
		    gboolean    handled,
		    YelpWindow *window)
{
	YelpWindowPriv *priv;
	gchar          *abs_url = NULL;

	g_return_if_fail (YELP_IS_WINDOW (window));

	d(g_print ("url_selected: %s base: %s, handled: %d\n", url, base_url, handled));

	priv = window->priv;
	
	if (url && base_url) {
		abs_url = yelp_util_resolve_relative_uri (base_url, url);
		d(g_print ("Link '%s' pressed relative to: %s -> %s\n", 
			   url,
			   base_url,
			   abs_url));
        } else {
		if (url) {
			abs_url = g_strdup (url);
		}
		else if (base_url) {
			abs_url = g_strdup (base_url);
		}
        }

	if (handled) {
		yelp_history_goto (priv->history, abs_url);
	} else {
		if (yw_handle_url (window, abs_url)) {
			yelp_history_goto (priv->history, abs_url);
		}
	}

	g_free (abs_url);
}

static void
yw_toggle_history_buttons (GtkWidget   *button, 
			   gboolean     sensitive,
			   YelpHistory *history)
{
	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (YELP_IS_HISTORY (history));

	gtk_widget_set_sensitive (button, sensitive);
}

static void
yw_history_button_clicked (GtkWidget *button, YelpWindow *window)
{
	YelpWindowPriv    *priv;
	const gchar       *url;
	
	g_return_if_fail (YELP_IS_WINDOW (window));

	priv = window->priv;

	if (button == priv->forward_button) {
		url = yelp_history_go_forward (priv->history);
	}
	else if (button == priv->back_button) {
		url = yelp_history_go_back (priv->history);
	}

	if (url) {
		yw_handle_url (window, url);
	}
}

static void
yw_home_button_clicked (GtkWidget *button, YelpWindow *window)
{
	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (YELP_IS_WINDOW (window));

	yelp_view_toc_open_url (YELP_VIEW_TOC (window->priv->toc_view),
				"toc:");
	gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook),
				       PAGE_TOC_VIEW);
}

static void
yw_index_button_clicked (GtkWidget *button, YelpWindow *window)
{
	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (YELP_IS_WINDOW (window));

	gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook),
				       PAGE_INDEX_VIEW);
}

static void
yw_new_window_cb (gpointer data, guint section, GtkWidget *widget)
{
	g_return_if_fail (YELP_IS_WINDOW (data));

	g_signal_emit (data, signals[NEW_WINDOW_REQUESTED], 0);
}

static void
yw_close_window_cb (gpointer   data,
		    guint      section,
		    GtkWidget *widget)
{
	gtk_widget_destroy (GTK_WIDGET (data));
}

static void
yw_exit_cb (gpointer data, guint section, GtkWidget *widget)
{
	bonobo_main_quit ();
}

static void
yw_about_cb (gpointer data, guint section, GtkWidget *widget)
{
	GtkWidget *about;
	const gchar *authors[] = { 
		"Mikael Hallendal <micke@codefactory.se>",
		"Alexander Larsson <alexl@redhat.com>",
		NULL
	};
	
	about = gnome_about_new (PACKAGE, VERSION,
				 "(C) 2001-2002 Mikael Hallendal <micke@codefactory.se>",
				 _("Help Browser for GNOME 2.0"),
				 authors,
				 NULL,
				 NULL,
				 NULL);

	gtk_widget_show (about);
}

static GtkWidget *
yw_create_toolbar (YelpWindow *window)
{
	YelpWindowPriv *priv;
	GtkWidget      *toolbar;
	GtkWidget      *button;

	g_return_val_if_fail (YELP_IS_WINDOW (window), NULL);

	priv = window->priv;

	toolbar = gtk_toolbar_new ();

	priv->back_button = gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
						      "gtk-go-back",
						      _("Go back in the history list"), "",
						      NULL, NULL, -1);

	gtk_widget_set_sensitive (priv->back_button, FALSE);

	g_signal_connect_swapped (priv->history, "back_exists_changed",
				  G_CALLBACK (yw_toggle_history_buttons),
				  G_OBJECT (priv->back_button));

	g_signal_connect (priv->back_button, "clicked",
			  G_CALLBACK (yw_history_button_clicked),
			  G_OBJECT (window));
	
	priv->forward_button = gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
							 "gtk-go-forward",
							 _("Go forth in the history list"), "",
							 NULL, NULL, -1);

	gtk_widget_set_sensitive (priv->forward_button, FALSE);

	g_signal_connect_swapped (priv->history, "forward_exists_changed",
 				  G_CALLBACK (yw_toggle_history_buttons),
				  G_OBJECT (priv->forward_button));
	
	g_signal_connect (priv->forward_button, "clicked",
			  G_CALLBACK (yw_history_button_clicked),
			  G_OBJECT (window));

	button = gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar), 
					   "gtk-home",
					   _("Overview of contents"), "",
					   NULL, NULL, -1);

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (yw_home_button_clicked),
			  G_OBJECT (window));

	gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

	button = gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
					   "gtk-index",
					   _("Search in the index"), "",
					   NULL, NULL, -1);
	
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (yw_index_button_clicked),
			  G_OBJECT (window));

#if 0	
	gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));
	
	button = gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
					   "gtk-find",
					   _("Search for a phrase in the document"), "",
					   NULL, NULL, -1);

	/* Connect to find-button */

	gtk_toolbar_append_widget (GTK_TOOLBAR (toolbar),
				   gtk_label_new (_("Help on ")),
				   "", "");
	
	gtk_toolbar_append_widget (GTK_TOOLBAR (toolbar),
				   gtk_entry_new (),
				   _("Enter phrase to search for in all documents"), "");
#endif

	return toolbar;
}

GtkWidget *
yelp_window_new (GNode *doc_tree)
{
	YelpWindow     *window;
	YelpWindowPriv *priv;
	
	window = g_object_new (YELP_TYPE_WINDOW, NULL);
	priv   = window->priv;

	priv->doc_tree = doc_tree;

 	priv->toc_view    = yelp_view_toc_new (doc_tree);
 	priv->content_view = yelp_view_content_new (doc_tree);
	priv->index_view   = yelp_view_index_new (NULL);

	g_signal_connect (priv->toc_view, "url_selected",
			  G_CALLBACK (yw_url_selected_cb),
			  window);
	g_signal_connect (priv->content_view, "url_selected",
			  G_CALLBACK (yw_url_selected_cb),
			  window);

	yw_populate (window);

        return GTK_WIDGET (window);
}

void
yelp_window_open_uri (YelpWindow  *window,
		      const gchar *str_uri)
{
	YelpWindowPriv *priv;
	
	g_return_if_fail (YELP_IS_WINDOW (window));
	
	priv = window->priv;

	yw_handle_url (window, str_uri);
}

