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
#include <libgnome/gnome-url.h>
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

typedef enum {
	YELP_WINDOW_ACTION_BACK = 1,
	YELP_WINDOW_ACTION_FORWARD,
} YelpHistoryAction;

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
static void        yw_toggle_history_back    (YelpHistory         *history,
					      gboolean             sensitive,
					      YelpWindow          *window);

static void        yw_toggle_history_forward (YelpHistory         *history,
					      gboolean             sensitive,
					      YelpWindow          *window);

static void        yw_history_action         (YelpWindow          *window,
					      YelpHistoryAction    action);
static void        yw_back_button_clicked    (GtkWidget           *button,
					      YelpWindow          *window);
static void        yw_forward_button_clicked (GtkWidget           *button,
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
static void        yw_history_go_cb          (gpointer             data,
					      guint                section,
					      GtkWidget           *widget);
static void        yw_go_home_cb             (gpointer             data,
					      guint                section,
					      GtkWidget           *widget);
static void        yw_go_index_cb            (gpointer             data,
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
	GList          *index;

	GtkWidget      *notebook;

	GtkWidget      *toc_view;
	GtkWidget      *content_view;
	GtkWidget      *index_view;
	
	GtkWidget      *view_current;

	YelpHistory    *history;

	GtkItemFactory *item_factory;

	GtkWidget      *forward_button;
	GtkWidget      *back_button;
};

static GtkItemFactoryEntry menu_items[] = {
	{N_("/_File"),              NULL,         0,                  0,                           "<Branch>"},
	{N_("/File/_New window"),   NULL,         yw_new_window_cb,   0,                           "<StockItem>", GTK_STOCK_NEW     },
	{N_("/File/_Close window"), NULL,         yw_close_window_cb, 0,                           "<StockItem>", GTK_STOCK_CLOSE   },
	{N_("/File/_Quit"),         NULL,         yw_exit_cb,         0,                           "<StockItem>", GTK_STOCK_QUIT    },
	{N_("/_Go"),                NULL,         0,                  0,                           "<Branch>"},
	{N_("/Go/_Back"),           NULL,         yw_history_go_cb,   YELP_WINDOW_ACTION_BACK,     "<StockItem>", GTK_STOCK_GO_BACK    },
	{N_("/Go/_Forward"),        NULL,         yw_history_go_cb,   YELP_WINDOW_ACTION_FORWARD,  "<StockItem>", GTK_STOCK_GO_FORWARD },
	{N_("/Go/_Home"),           NULL,         yw_go_home_cb,      0,                           "<StockItem>", GTK_STOCK_HOME },
	{N_("/Go/_Index"),          NULL,         yw_go_index_cb,     0,                           "<StockItem>", GTK_STOCK_INDEX },
	{N_("/_Help"),              NULL,         0,                  0,                           "<Branch>"},
	{N_("/Help/_About"),        NULL,         yw_about_cb,        0,                           NULL },
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

	g_signal_connect (priv->history, 
			  "back_exists_changed",
			  G_CALLBACK (yw_toggle_history_back),
			  window);

	g_signal_connect (priv->history, 
			  "forward_exists_changed",
			  G_CALLBACK (yw_toggle_history_forward),
			  window);

	gtk_window_set_default_size (GTK_WINDOW (window), 600, 420);

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
	GtkAccelGroup  *accel_group;
	GtkWidget      *menu_item;
	
        priv = window->priv;

	main_box        = gtk_vbox_new (FALSE, 0);
	
	gtk_container_add (GTK_CONTAINER (window), main_box);

	accel_group  = gtk_accel_group_new ();
	priv->item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, 
					     "<main>", accel_group);

	gtk_item_factory_set_translate_func (priv->item_factory, 
					     (GtkTranslateFunc) gettext,
					     NULL, NULL);

	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

	gtk_item_factory_create_items (priv->item_factory,
				       G_N_ELEMENTS (menu_items),
				       menu_items,
				       window);

	menu_item = gtk_item_factory_get_item_by_action (priv->item_factory,
							 YELP_WINDOW_ACTION_BACK);
	gtk_widget_set_sensitive (menu_item, FALSE);

	menu_item = gtk_item_factory_get_item_by_action (priv->item_factory,
							 YELP_WINDOW_ACTION_FORWARD);
	gtk_widget_set_sensitive (menu_item, FALSE);

	gtk_box_pack_start (GTK_BOX (main_box),
			    gtk_item_factory_get_widget (priv->item_factory,
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

	if (priv->index) {
		gtk_notebook_insert_page (GTK_NOTEBOOK (priv->notebook),
					  priv->index_view,
					  NULL, PAGE_INDEX_VIEW);
	}
	
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
	} else if (strncmp (url, "index:", 6) == 0) {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook),
					       PAGE_INDEX_VIEW);
		yelp_view_index_show_uri (YELP_VIEW_INDEX (priv->index_view),
					  url);
		return TRUE;
	} else {
		/* FIXME: Show dialog on failure? */
		gnome_url_show (url, NULL);
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
yw_toggle_history_back (YelpHistory *history,
			gboolean     sensitive,
			YelpWindow  *window)
{
	YelpWindowPriv *priv;
	GtkWidget      *menu_item;

	g_return_if_fail (YELP_IS_HISTORY (history));
	g_return_if_fail (YELP_IS_WINDOW (window));

	priv = window->priv;

	gtk_widget_set_sensitive (priv->back_button, sensitive);

	menu_item = gtk_item_factory_get_item_by_action (priv->item_factory,
							 YELP_WINDOW_ACTION_BACK);
	gtk_widget_set_sensitive (menu_item, sensitive);
}

static void
yw_toggle_history_forward (YelpHistory *history,
			   gboolean     sensitive,
			   YelpWindow  *window)
{
	YelpWindowPriv *priv;
	GtkWidget      *menu_item;

	g_return_if_fail (YELP_IS_HISTORY (history));
	g_return_if_fail (YELP_IS_WINDOW (window));

	priv = window->priv;

	gtk_widget_set_sensitive (priv->forward_button, sensitive);

	menu_item = gtk_item_factory_get_item_by_action (priv->item_factory,
							 YELP_WINDOW_ACTION_FORWARD);
	gtk_widget_set_sensitive (menu_item, sensitive);
}

static void
yw_history_action (YelpWindow *window, YelpHistoryAction action)
{
	YelpWindowPriv *priv;
	const gchar    *url;
	
	g_return_if_fail (YELP_IS_WINDOW (window));

	priv = window->priv;

	switch (action) {
	case YELP_WINDOW_ACTION_BACK:
		url = yelp_history_go_back (priv->history);
		break;
	case YELP_WINDOW_ACTION_FORWARD:
		url = yelp_history_go_forward (priv->history);
		break;
	default:
		return;
	}
	
	if (url) {
		yw_handle_url (window, url);
	}
}

static void
yw_back_button_clicked (GtkWidget *button, YelpWindow *window)
{
	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (YELP_IS_WINDOW (window));

	yw_history_action (window, YELP_WINDOW_ACTION_BACK);
}

static void
yw_forward_button_clicked (GtkWidget *button, YelpWindow *window)
{
	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (YELP_IS_WINDOW (window));

	yw_history_action (window, YELP_WINDOW_ACTION_FORWARD);
}

static void
yw_home_button_clicked (GtkWidget *button, YelpWindow *window)
{
	g_return_if_fail (YELP_IS_WINDOW (window));

	yelp_history_goto (window->priv->history, "toc:");
	
	yelp_view_toc_open_url (YELP_VIEW_TOC (window->priv->toc_view),
				"toc:");

	gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook),
				       PAGE_TOC_VIEW);
}

static void
yw_index_button_clicked (GtkWidget *button, YelpWindow *window)
{
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
yw_history_go_cb (gpointer data, guint section, GtkWidget *widget)
{
	yw_history_action (data, section);
}

static void
yw_go_home_cb (gpointer data, guint section, GtkWidget *widget)
{
	yw_home_button_clicked (NULL, YELP_WINDOW (data));
}

static void
yw_go_index_cb (gpointer data, guint section, GtkWidget *widget)
{
	yw_index_button_clicked (NULL, YELP_WINDOW (data));
}

static void
yw_about_cb (gpointer data, guint section, GtkWidget *widget)
{
	GtkWidget   *about;
	const gchar *authors[] = { 
		"Mikael Hallendal <micke@codefactory.se>",
		"Alexander Larsson <alexl@redhat.com>",
		NULL
	};
	gchar       *translator_credits = _("translator_credits");
	
	about = gnome_about_new (PACKAGE, VERSION,
				 "(C) 2001-2002 Mikael Hallendal <micke@codefactory.se>",
				 _("Help Browser for GNOME 2.0"),
				 authors,
				 NULL,
				 strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
				 NULL);

	gtk_widget_show (about);
}

static GtkWidget *
yw_create_toolbar (YelpWindow *window)
{
	YelpWindowPriv *priv;
	GtkWidget      *toolbar;
	GtkWidget      *button;
	GtkWidget      *icon;

	g_return_val_if_fail (YELP_IS_WINDOW (window), NULL);

	priv = window->priv;

	toolbar = gtk_toolbar_new ();

	icon = gtk_image_new_from_stock ("gtk-go-back", 
					 GTK_ICON_SIZE_LARGE_TOOLBAR);

	priv->back_button = gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
						     _("Back"),
						     _("Show previous page in history"),
						     NULL, icon, 
						     G_CALLBACK (yw_back_button_clicked),
						     window);
	
	gtk_widget_set_sensitive (priv->back_button, FALSE);

	icon = gtk_image_new_from_stock ("gtk-go-forward", 
					 GTK_ICON_SIZE_LARGE_TOOLBAR);

	priv->forward_button = gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
							_("Forward"),
							_("Show next page in history"),
							NULL, icon,
							G_CALLBACK (yw_forward_button_clicked),
							window);

	gtk_widget_set_sensitive (priv->forward_button, FALSE);

	gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

	icon = gtk_image_new_from_stock ("gtk-home", 
					 GTK_ICON_SIZE_LARGE_TOOLBAR);
	
	button = gtk_toolbar_append_item (GTK_TOOLBAR (toolbar), 
					  _("Home"),
					  _("Go to home view"), 
					  NULL, icon,
					  G_CALLBACK (yw_home_button_clicked),
					  window);

	if (priv->index) {
		icon = gtk_image_new_from_stock ("gtk-index",
						 GTK_ICON_SIZE_LARGE_TOOLBAR);

		button = gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
						  _("Index"),
						  _("Search in the index"), 
						  NULL, icon,
						  G_CALLBACK (yw_index_button_clicked),
						  window);
		
	}
	
	return toolbar;
}

GtkWidget *
yelp_window_new (GNode *doc_tree, GList *index)
{
	YelpWindow     *window;
	YelpWindowPriv *priv;
	
	window = g_object_new (YELP_TYPE_WINDOW, NULL);
	priv   = window->priv;

	priv->doc_tree = doc_tree;
	priv->index    = index;

 	priv->toc_view    = yelp_view_toc_new (doc_tree);
 	priv->content_view = yelp_view_content_new (doc_tree);

	if (priv->index) {
		priv->index_view   = yelp_view_index_new (index);
	} 

	g_signal_connect (priv->toc_view, "url_selected",
			  G_CALLBACK (yw_url_selected_cb),
			  window);
	g_signal_connect (priv->content_view, "url_selected",
			  G_CALLBACK (yw_url_selected_cb),
			  window);
	g_signal_connect (priv->index_view, "url_selected",
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

const gchar *
yelp_window_get_current_uri (YelpWindow *window)
{
	g_return_val_if_fail (YELP_IS_WINDOW (window), NULL);
	
	return yelp_history_get_current (window->priv->history);
}
