/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@imendio.com>
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
 * Author: Mikael Hallendal <micke@imendio.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <bonobo/bonobo-main.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomeui/gnome-about.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-url.h>
#include <libgnome/gnome-program.h>
#include <libgnome/gnome-config.h>
#include <glade/glade.h>
#include <string.h>
#include "yelp-error.h"
#include "yelp-html.h"
#include "yelp-util.h"
#include "yelp-section.h"
#include "yelp-history.h"
#include "yelp-view-content.h"
#include "yelp-view-index.h"
#include "yelp-view-toc.h"
#include "yelp-window.h"

#define d(x)
#define RESPONSE_PREV 1
#define RESPONSE_NEXT 2

#define YELP_CONFIG_WIDTH  "/yelp/Geometry/width"
#define YELP_CONFIG_HEIGHT "/yelp/Geometry/height"
#define YELP_CONFIG_WIDTH_DEFAULT  "600"
#define YELP_CONFIG_HEIGHT_DEFAULT "420"

typedef enum {
	YELP_WINDOW_ACTION_BACK = 1,
	YELP_WINDOW_ACTION_FORWARD
} YelpHistoryAction;


static void        window_init		          (YelpWindow        *window);
static void        window_class_init	          (YelpWindowClass   *klass);

static void        window_populate                (YelpWindow        *window);

static gboolean    window_handle_uri              (YelpWindow        *window,
						   YelpURI           *uri);
static void        window_uri_selected_cb         (gpointer           view,
						   YelpURI           *uri,
						   gboolean           handled,
						   YelpWindow        *window);
static void        window_title_changed_cb        (gpointer           view,
						   const gchar       *title,
						   YelpWindow        *window);
static void        window_toggle_history_back     (YelpHistory       *history,
						   gboolean           sensitive,
						   YelpWindow        *window);
 
static void        window_toggle_history_forward  (YelpHistory       *history,
						   gboolean           sensitive,
						   YelpWindow        *window);

static void        window_history_action          (YelpWindow        *window,
						   YelpHistoryAction  action);
static void        window_back_button_clicked     (GtkWidget         *button,
						   YelpWindow        *window);
static void        window_forward_button_clicked  (GtkWidget         *button,
						   YelpWindow        *window);
static void        window_home_button_clicked     (GtkWidget         *button,
						   YelpWindow        *window);
static void        window_index_button_clicked    (GtkWidget         *button,
						   YelpWindow        *window);
static void        window_new_window_cb           (gpointer           data,
						   guint              section,
						   GtkWidget         *widget);
static void        window_close_window_cb         (gpointer           data,
						   guint              section,
						   GtkWidget         *widget);
static void        window_find_cb                 (gpointer data,     guint section,
						   GtkWidget         *widget);
static void        window_find_again_cb           (gpointer data,     guint section,
						   GtkWidget         *widget);
static void        window_history_go_cb           (gpointer           data,
						   guint              section,
						   GtkWidget         *widget);
static void        window_go_home_cb              (gpointer           data,
						   guint              section,
						   GtkWidget         *widget);
static void        window_go_index_cb             (gpointer           data,
						   guint              section,
						   GtkWidget         *widget);
static void        window_about_cb                (gpointer           data,
						   guint              section,
						   GtkWidget         *widget);
static gboolean    window_find_delete_event_cb    (GtkWidget         *widget,
						   gpointer           user_data);
static void        window_find_response_cb        (GtkWidget         *dialog ,
						   gint               response,
						   YelpWindow        *window);
static YelpView *  window_get_active_view         (YelpWindow        *window);
static GtkWidget * window_create_toolbar          (YelpWindow        *window);
static GdkPixbuf * window_load_icon               (void);
static gboolean    window_configure_cb            (GtkWidget         *widget,
						   GdkEventConfigure *event,
						   gpointer           data);


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

	YelpView       *toc_view;
	YelpView       *content_view;
	YelpView       *index_view;
	
	GtkWidget      *find_dialog;
	GtkWidget      *find_entry;
	GtkWidget      *case_checkbutton;
	GtkWidget      *wrap_checkbutton;
	gchar          *find_string;
	gboolean        match_case;
	gboolean        wrap;
	
	YelpHistory    *history;

	GtkItemFactory *item_factory;

	GtkWidget      *forward_button;
	GtkWidget      *back_button;
};

static GtkItemFactoryEntry menu_items[] = {
	{N_("/_File"),              NULL,         0,                  0,                           "<Branch>"},
	{N_("/File/_New window"),   NULL,         window_new_window_cb,   0,                       "<StockItem>", GTK_STOCK_NEW     },
	{N_("/File/_Close window"), NULL,         window_close_window_cb, 0,                       "<StockItem>", GTK_STOCK_CLOSE   },

	{N_("/_Edit"),              NULL,         0,                  0,                           "<Branch>"},
	{N_("/Edit/_Find in page..."), NULL,      window_find_cb,     0,                           "<StockItem>", GTK_STOCK_FIND    },
	{N_("/Edit/_Find again"),   "<Control>g", window_find_again_cb, 0,                         "<StockItem>", GTK_STOCK_FIND    },
	
	{N_("/_Go"),                NULL,         0,                  0,                           "<Branch>"},
	{N_("/Go/_Back"),           NULL,         window_history_go_cb,   YELP_WINDOW_ACTION_BACK,     "<StockItem>", GTK_STOCK_GO_BACK    },
	{N_("/Go/_Forward"),        NULL,         window_history_go_cb,   YELP_WINDOW_ACTION_FORWARD,  "<StockItem>", GTK_STOCK_GO_FORWARD },
	{N_("/Go/_Home"),           NULL,         window_go_home_cb,      0,                           "<StockItem>", GTK_STOCK_HOME },
	{N_("/Go/_Index"),          NULL,         window_go_index_cb,     0,                           "<StockItem>", GTK_STOCK_INDEX },

	{N_("/_Help"),              NULL,         0,                  0,                           "<Branch>"},
	{N_("/Help/_About"),        NULL,         window_about_cb,        0,                           "<StockItem>", GNOME_STOCK_ABOUT },
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
                                (GClassInitFunc) window_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpWindow),
                                0,
                                (GInstanceInitFunc) window_init,
                        };
                
                window_type = g_type_register_static (GNOME_TYPE_APP,
                                                      "YelpWindow", 
                                                      &window_info, 0);
        }
        
        return window_type;
}

static void
window_init (YelpWindow *window)
{
        YelpWindowPriv *priv;
/* 	YelpURI        *uri; */
	gint width, height;
	
        priv = g_new0 (YelpWindowPriv, 1);
        window->priv = priv;
        
	priv->toc_view     = NULL;
	priv->content_view = NULL;
	priv->index_view   = NULL;
	
	priv->match_case   = FALSE;
	priv->wrap         = TRUE;

	priv->history = yelp_history_new ();
	
	g_signal_connect (priv->history, 
			  "back_exists_changed",
			  G_CALLBACK (window_toggle_history_back),
			  window);

	g_signal_connect (priv->history, 
			  "forward_exists_changed",
			  G_CALLBACK (window_toggle_history_forward),
			  window);

	width = gnome_config_get_int (YELP_CONFIG_WIDTH
				     "=" YELP_CONFIG_WIDTH_DEFAULT);
	height = gnome_config_get_int (YELP_CONFIG_HEIGHT
				      "=" YELP_CONFIG_HEIGHT_DEFAULT);
	gtk_window_set_default_size (GTK_WINDOW (window), width, height);
	g_signal_connect (window,
			  "configure-event",
			  G_CALLBACK (window_configure_cb),
			  NULL);

	gtk_window_set_title (GTK_WINDOW (window), _("Help Browser"));
}

static void
window_class_init (YelpWindowClass *klass)
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
window_populate (YelpWindow *window)
{
        YelpWindowPriv *priv;
	GtkWidget      *toolbar;
	GtkWidget      *main_box;
	GtkWidget      *sw;
	GtkAccelGroup  *accel_group;
	GtkWidget      *menu_item;
	
        priv = window->priv;

	main_box        = gtk_vbox_new (FALSE, 0);
	
	gnome_app_set_contents (GNOME_APP (window), main_box);
	
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

	gnome_app_set_menus (GNOME_APP (window), GTK_MENU_BAR (
				gtk_item_factory_get_widget (priv->item_factory, "<main>")));

	toolbar = window_create_toolbar (window);

	gnome_app_set_toolbar (GNOME_APP (window), GTK_TOOLBAR (toolbar));
			
	priv->notebook  = gtk_notebook_new ();

	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);

	sw = gtk_scrolled_window_new (NULL, NULL);
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	gtk_container_add (GTK_CONTAINER (sw), 
			   priv->toc_view->widget);

	gtk_notebook_insert_page (GTK_NOTEBOOK (priv->notebook),
				  sw, NULL, PAGE_TOC_VIEW);
	
	gtk_notebook_insert_page (GTK_NOTEBOOK (priv->notebook),
				  priv->content_view->widget,
				  NULL, PAGE_CONTENT_VIEW);

	if (priv->index) {
		gtk_notebook_insert_page (GTK_NOTEBOOK (priv->notebook),
					  priv->index_view->widget,
					  NULL, PAGE_INDEX_VIEW);
	}
	
	gtk_box_pack_end (GTK_BOX (main_box), priv->notebook,
			  TRUE, TRUE, 0);

	gtk_widget_grab_focus (priv->content_view->widget);
}

static gboolean
window_handle_uri (YelpWindow *window, YelpURI *uri)
{
	YelpWindowPriv *priv;
	GError         *error = NULL;
	gboolean        handled = FALSE;

	priv = window->priv;

	d(g_print ("Handling URL: %s\n", yelp_uri_to_string (uri)));

	if (!yelp_uri_exists (uri)) {
		gchar *str_uri = yelp_uri_to_string (uri);
		
		g_set_error (&error,
			     YELP_ERROR,
			     YELP_ERROR_URI_NOT_EXIST,
			     _("The document '%s' does not exist"), str_uri);
		g_free (str_uri);
	}
	else if (yelp_uri_get_type (uri) == YELP_URI_TYPE_TOC) {
		d(g_print ("[TOC]\n"));
		
		yelp_view_show_uri (priv->toc_view, uri, NULL);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook),
					       PAGE_TOC_VIEW);
		handled = TRUE;
	}
	else if (yelp_uri_get_type (uri) == YELP_URI_TYPE_INDEX) {
		d(g_print ("[INDEX]\n"));
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook),
					       PAGE_INDEX_VIEW);
		yelp_view_show_uri (priv->index_view, uri, &error);
		handled = TRUE;
	}
	else if (yelp_uri_get_type (uri) == YELP_URI_TYPE_MAN ||
		 yelp_uri_get_type (uri) == YELP_URI_TYPE_INFO ||
		 yelp_uri_get_type (uri) == YELP_URI_TYPE_DOCBOOK_XML ||
		 yelp_uri_get_type (uri) == YELP_URI_TYPE_DOCBOOK_SGML ||
		 yelp_uri_get_type (uri) == YELP_URI_TYPE_HTML ||
		 yelp_uri_get_type (uri) == YELP_URI_TYPE_PATH) {
		d(g_print ("[CONTENT]\n"));
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook),
					       PAGE_CONTENT_VIEW);
		yelp_view_show_uri (priv->content_view,
				    uri, &error);
		handled = TRUE;
	} else {
		gnome_url_show (yelp_uri_to_string (uri), &error);
		handled = FALSE;
	}

	if (error) {
		GtkWidget *dialog;
		
		dialog = gtk_message_dialog_new (GTK_WINDOW (window),
						 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}

	return handled;
}

static void
window_uri_selected_cb (gpointer    view, 
			YelpURI    *uri, 
			gboolean    handled,
			YelpWindow *window)
{
	YelpWindowPriv *priv;

	g_return_if_fail (YELP_IS_WINDOW (window));

	d(g_print ("uri_selected: %s, handled: %d\n", 
		   yelp_uri_to_string (uri), handled));

	priv = window->priv;

	yelp_uri_ref (uri);
	
	if (handled) {
		yelp_history_goto (priv->history, uri);
	} else {
		if (window_handle_uri (window, uri)) {
			yelp_history_goto (priv->history, uri);
		}
	}

	yelp_uri_unref (uri);
}

static void
window_title_changed_cb (gpointer view, const gchar *title, YelpWindow *window)
{
	gchar *new_title;
	
	g_return_if_fail (title != NULL);
	g_return_if_fail (YELP_IS_WINDOW (window));
	
	new_title = g_strconcat (title, " - ", _("Help Browser"), NULL);

	gtk_window_set_title (GTK_WINDOW (window), new_title);

	g_free (new_title);
}

static void
window_toggle_history_back (YelpHistory *history,
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
window_toggle_history_forward (YelpHistory *history,
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
window_history_action (YelpWindow *window, YelpHistoryAction action)
{
	YelpWindowPriv *priv;
	YelpURI        *uri;
	
	g_return_if_fail (YELP_IS_WINDOW (window));

	priv = window->priv;

	switch (action) {
	case YELP_WINDOW_ACTION_BACK:
		uri = yelp_history_go_back (priv->history);
		break;
	case YELP_WINDOW_ACTION_FORWARD:
		uri = yelp_history_go_forward (priv->history);
		break;
	default:
		return;
	}
	
	if (uri) {
		window_handle_uri (window, uri);
	}
}

static void
window_back_button_clicked (GtkWidget *button, YelpWindow *window)
{
	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (YELP_IS_WINDOW (window));

	window_history_action (window, YELP_WINDOW_ACTION_BACK);
}

static void
window_forward_button_clicked (GtkWidget *button, YelpWindow *window)
{
	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (YELP_IS_WINDOW (window));

	window_history_action (window, YELP_WINDOW_ACTION_FORWARD);
}

static void
window_home_button_clicked (GtkWidget *button, YelpWindow *window)
{
	YelpURI *uri;
	
	g_return_if_fail (YELP_IS_WINDOW (window));
	
	uri = yelp_uri_new ("toc:");

	yelp_history_goto (window->priv->history, uri);
	yelp_view_show_uri (window->priv->toc_view, uri, NULL);

	yelp_uri_unref (uri);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook),
				       PAGE_TOC_VIEW);
}

static void
window_index_button_clicked (GtkWidget *button, YelpWindow *window)
{
	YelpURI *uri;
	
	g_return_if_fail (YELP_IS_WINDOW (window));

	uri = yelp_uri_new ("index:");
	yelp_history_goto (window->priv->history, uri);
	yelp_uri_unref (uri);
	
	gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook),
				       PAGE_INDEX_VIEW);
}

static void
window_new_window_cb (gpointer data, guint section, GtkWidget *widget)
{
	g_return_if_fail (YELP_IS_WINDOW (data));

	g_signal_emit (data, signals[NEW_WINDOW_REQUESTED], 0);
}

static void
window_close_window_cb (gpointer   data,
			guint      section,
			GtkWidget *widget)
{
	gtk_widget_destroy (GTK_WIDGET (data));
}

static void
window_find_cb (gpointer data, guint section, GtkWidget *widget)
{
	YelpWindow     *window = data;
	YelpWindowPriv *priv;
	GladeXML       *glade;
	
	g_return_if_fail (YELP_IS_WINDOW (data));

	window = YELP_WINDOW (data);

	priv = window->priv;

	if (!priv->find_dialog) {
		glade = glade_xml_new (DATADIR "/yelp/ui/yelp.glade", "find_dialog", NULL);
		if (!glade) {
			g_warning ("Couldn't find necessary glade file " DATADIR "/yelp/ui/yelp.glade");
			return;
		}

		priv->find_dialog = glade_xml_get_widget (glade, "find_dialog");

		priv->find_entry = glade_xml_get_widget (glade, "find_entry");
		priv->case_checkbutton = glade_xml_get_widget (glade, "case_check");
		priv->wrap_checkbutton = glade_xml_get_widget (glade, "wrap_check");

		g_signal_connect (priv->find_dialog,
				  "delete_event",
				  G_CALLBACK (window_find_delete_event_cb),
				  NULL);

		g_signal_connect (priv->find_dialog,
				  "response",
				  G_CALLBACK (window_find_response_cb),
				  window);

		g_object_unref (glade);
	}

	gtk_window_present (GTK_WINDOW (priv->find_dialog));
}

static void
window_find_again_cb (gpointer data, guint section, GtkWidget *widget)
{
	YelpWindow     *window = data;
	YelpWindowPriv *priv;
	YelpView       *view;
	YelpHtml       *html;
	
	g_return_if_fail (YELP_IS_WINDOW (data));

	window = YELP_WINDOW (data);

	priv = window->priv;

	if (priv->find_string) {
		view = window_get_active_view (window);
		html = yelp_view_get_html (view);
		
		yelp_html_find (html,
				priv->find_string,
				priv->match_case,
				priv->wrap,
				TRUE);
	}
}
	
static void
window_history_go_cb (gpointer data, guint section, GtkWidget *widget)
{
	window_history_action (data, section);
}

static void
window_go_home_cb (gpointer data, guint section, GtkWidget *widget)
{
	window_home_button_clicked (NULL, YELP_WINDOW (data));
}

static void
window_go_index_cb (gpointer data, guint section, GtkWidget *widget)
{
	window_index_button_clicked (NULL, YELP_WINDOW (data));
}

static void
window_about_cb (gpointer data, guint section, GtkWidget *widget)
{
	static GtkWidget *about = NULL;

	if (about == NULL) {
		const gchar *authors[] = { 
			"Mikael Hallendal <micke@imendio.com>",
			"Alexander Larsson <alexl@redhat.com>",
			NULL
		};
		/* Note to translators: put here your name (and address) so it
		 * will shop up in the "about" box */
		gchar       *translator_credits = _("translator_credits");
	
		about = gnome_about_new (PACKAGE, VERSION,
					 "Copyright 2001-2003 Mikael Hallendal <micke@imendio.com>",
					 _("A Help Browser for GNOME"),
					 authors,
					 NULL,
					 strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
					 window_load_icon ());

		/* set the widget pointer to NULL when the widget is destroyed */
		g_signal_connect (G_OBJECT (about), "destroy",
				  G_CALLBACK (gtk_widget_destroyed),
				  &about);
		gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW (data));
	}
	gtk_window_present (GTK_WINDOW (about));
}

static gboolean
window_find_delete_event_cb (GtkWidget *widget, gpointer user_data)
{
	gtk_widget_hide (widget);
	
	return TRUE;
}

static void
window_find_response_cb (GtkWidget  *dialog ,
			 gint        response,
			 YelpWindow *window)
{
	YelpWindowPriv *priv;
	YelpView       *view;
	YelpHtml       *html;
	const gchar    *tmp;
	
	priv = window->priv;

	view = window_get_active_view (window);
	html = yelp_view_get_html (view);

	switch (response) {
	case GTK_RESPONSE_CLOSE:
		gtk_widget_hide (dialog);
		break;

	case RESPONSE_PREV:
	case RESPONSE_NEXT:
		tmp = gtk_entry_get_text (GTK_ENTRY (priv->find_entry));

		priv->match_case = gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (priv->case_checkbutton));

		priv->wrap = gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (priv->wrap_checkbutton));

		g_free (priv->find_string);
		
		if (!priv->match_case) {
			priv->find_string = g_utf8_casefold (tmp, -1);
		} else {
			priv->find_string = g_strdup (tmp);
		}
		
		yelp_html_find (html,
				priv->find_string,
				priv->match_case,
				priv->wrap,
				response == RESPONSE_NEXT);
		break;
		
	default:
		break;
	}
	
}

static YelpView *
window_get_active_view (YelpWindow *window)
{
	YelpWindowPriv *priv;
	
	priv = window->priv;
	
	switch (gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook))) {
	case PAGE_TOC_VIEW:
		return priv->toc_view;
	case PAGE_CONTENT_VIEW:
		return priv->content_view;
	case PAGE_INDEX_VIEW:
		return priv->index_view;
		break;
	default:
		g_assert_not_reached ();
	}

	return NULL;
}

static GtkWidget *
window_create_toolbar (YelpWindow *window)
{
	YelpWindowPriv  *priv;
	GtkWidget       *toolbar;
	GtkWidget       *button;
	GtkWidget       *icon;
	
	g_return_val_if_fail (YELP_IS_WINDOW (window), NULL);

	priv = window->priv;

	toolbar = gtk_toolbar_new ();
	
	icon = gtk_image_new_from_stock ("gtk-go-back", 
					 GTK_ICON_SIZE_LARGE_TOOLBAR);

	priv->back_button = gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
						     _("Back"),
						     _("Show previous page in history"),
						     NULL, icon, 
						     G_CALLBACK (window_back_button_clicked),
						     window);
	
	gtk_widget_set_sensitive (priv->back_button, FALSE);

	icon = gtk_image_new_from_stock ("gtk-go-forward", 
					 GTK_ICON_SIZE_LARGE_TOOLBAR);

	priv->forward_button = gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
							_("Forward"),
							_("Show next page in history"),
							NULL, icon,
							G_CALLBACK (window_forward_button_clicked),
							window);

	gtk_widget_set_sensitive (priv->forward_button, FALSE);

	gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

	icon = gtk_image_new_from_stock ("gtk-home", 
					 GTK_ICON_SIZE_LARGE_TOOLBAR);
	
	button = gtk_toolbar_append_item (GTK_TOOLBAR (toolbar), 
					  _("Home"),
					  _("Go to home view"), 
					  NULL, icon,
					  G_CALLBACK (window_home_button_clicked),
					  window);

	if (priv->index) {
		icon = gtk_image_new_from_stock ("gtk-index",
						 GTK_ICON_SIZE_LARGE_TOOLBAR);

		button = gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
						  _("Index"),
						  _("Search in the index"), 
						  NULL, icon,
						  G_CALLBACK (window_index_button_clicked),
						  window);
		
	}
	
	return toolbar;
}

static GdkPixbuf *
window_load_icon (void)
{
	static GdkPixbuf *pixbuf = NULL;
	
	if (!pixbuf) {
		gchar *file;
		
		file = gnome_program_locate_file (NULL,
						  GNOME_FILE_DOMAIN_PIXMAP,
						  "gnome-help.png",
						  TRUE,
						  NULL);

		if (file) {
			pixbuf = gdk_pixbuf_new_from_file (file,
							   NULL);
			g_free (file);
		}
	}

	return pixbuf;
}

static gboolean
window_configure_cb (GtkWidget         *widget,
		     GdkEventConfigure *event,
		     gpointer           data)
{
	gint width, height;
	gtk_window_get_size (GTK_WINDOW (widget), &width, &height);
	gnome_config_set_int (YELP_CONFIG_WIDTH, width);
	gnome_config_set_int (YELP_CONFIG_HEIGHT, height);
	gnome_config_sync ();

	return FALSE;
}

GtkWidget *
yelp_window_new (GNode *doc_tree, GList *index)
{
	YelpWindow     *window;
	YelpWindowPriv *priv;
	
	window = g_object_new (YELP_TYPE_WINDOW, 
			"app_id", PACKAGE, NULL);
	priv   = window->priv;

	priv->doc_tree = doc_tree;
	priv->index    = index;

 	priv->toc_view     = yelp_view_toc_new (doc_tree);
 	priv->content_view = yelp_view_content_new (doc_tree);

	if (priv->index) {
		priv->index_view = yelp_view_index_new (index);

		g_signal_connect (priv->index_view, "uri_selected",
				  G_CALLBACK (window_uri_selected_cb),
				  window);
	} 

	g_signal_connect (priv->toc_view, "uri_selected",
			  G_CALLBACK (window_uri_selected_cb),
			  window);

	g_signal_connect (priv->toc_view, "title_changed",
			  G_CALLBACK (window_title_changed_cb),
			  window);

	g_signal_connect (priv->content_view, "uri_selected",
			  G_CALLBACK (window_uri_selected_cb),
			  window);

	g_signal_connect (priv->content_view, "title_changed",
			  G_CALLBACK (window_title_changed_cb),
			  window);

	window_populate (window);

	gtk_window_set_icon (GTK_WINDOW (window), window_load_icon ());

        return GTK_WIDGET (window);
}

void
yelp_window_open_uri (YelpWindow  *window,
		      const gchar *str_uri)
{
	YelpWindowPriv *priv;
	YelpURI        *uri;
	
	g_return_if_fail (YELP_IS_WINDOW (window));
	
	priv = window->priv;

	uri = yelp_uri_new (str_uri);

	yelp_history_goto (priv->history, uri);
	
	window_handle_uri (window, uri);
	
	yelp_uri_unref (uri);
}

YelpURI *
yelp_window_get_current_uri (YelpWindow *window)
{
	g_return_val_if_fail (YELP_IS_WINDOW (window), NULL);
	
	return yelp_history_get_current (window->priv->history);
}

