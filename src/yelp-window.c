/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003 Shaun McCance <shaunm@gnome.org>
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
 * Author: Shaun McCance <shaunm@gnome.org>
 *   Based on implementation by Mikael Hallendal <micke@imendio.com>
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
#include "yelp-cache.h"
#include "yelp-db-pager.h"
#include "yelp-error.h"
#include "yelp-history.h"
#include "yelp-html.h"
#include "yelp-pager.h"
#include "yelp-section.h"
#include "yelp-toc-pager.h"
#include "yelp-util.h"
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

static void        window_error                   (YelpWindow        *window,
						   GError            *error);
static void        window_populate                (YelpWindow        *window);
static GtkWidget * window_create_toolbar          (YelpWindow        *window);
static GdkPixbuf * window_load_icon               (void);
static void        window_set_sections            (YelpWindow        *window,
						   GtkTreeModel      *sections);
static gboolean    window_handle_uri              (YelpWindow        *window,
						   YelpURI           *uri);
static gboolean    window_handle_pager_uri        (YelpWindow        *window,
						   YelpURI           *uri);
static gboolean    window_handle_html_uri         (YelpWindow        *window,
						   YelpURI           *uri);
static void        window_disconnect              (YelpWindow        *window);

static void        pager_page_cb                  (YelpPager         *pager,
						   gchar             *page_id,
						   gpointer           user_data);
static void        pager_error_cb                 (YelpPager         *pager,
						   gpointer           user_data);
static void        pager_finish_cb                (YelpPager         *pager,
						   gpointer           user_data);

static void        html_uri_selected_cb           (YelpHtml          *html,
						   YelpURI           *uri,
						   gboolean           handled,
						   gpointer           user_data);
static void        tree_selection_changed_cb      (GtkTreeSelection  *selection,
						   YelpWindow        *window);

static void        window_new_window_cb           (gpointer           data,
						   guint              section,
						   GtkWidget         *widget);
static void        window_close_window_cb         (gpointer           data,
						   guint              section,
						   GtkWidget         *widget);
static void        window_about_cb                (gpointer           data,
						   guint              section,
						   GtkWidget         *widget);
static gboolean    window_configure_cb            (GtkWidget         *widget,
						   GdkEventConfigure *event,
						   gpointer           data);

static void        window_back_clicked            (GtkWidget         *button,
						   YelpWindow        *window);
static void        window_forward_clicked         (GtkWidget         *button,
						   YelpWindow        *window);
static void        window_home_button_clicked     (GtkWidget         *button,
						   YelpWindow        *window);

static void        window_toggle_history_back     (YelpHistory       *history,
						   gboolean           sensitive,
						   YelpWindow        *window);
 
static void        window_toggle_history_forward  (YelpHistory       *history,
						   gboolean           sensitive,
						   YelpWindow        *window);
static void        window_history_action          (YelpWindow        *window,
						   YelpHistoryAction  action);



/*
static void        window_index_button_clicked    (GtkWidget         *button,
						   YelpWindow        *window);
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
static gboolean    window_find_delete_event_cb    (GtkWidget         *widget,
						   gpointer           user_data);
static void        window_find_response_cb        (GtkWidget         *dialog ,
						   gint               response,
						   YelpWindow        *window);
static YelpView *  window_get_active_view         (YelpWindow        *window);
*/

/*
enum {
    PAGE_TOC_VIEW,
    PAGE_CONTENT_VIEW,
    PAGE_INDEX_VIEW
};
*/

enum {
    NEW_WINDOW_REQUESTED,
    LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

struct _YelpWindowPriv {
    GNode          *doc_tree;
    GList          *index;

    GtkWidget      *main_box;
    GtkWidget      *pane;
    GtkWidget      *side_sects;
    YelpHtml       *html_view;

    GtkWidget      *side_sw;
    GtkWidget      *html_sw;

    YelpHistory    *history;

    YelpPager      *pager;
    gulong          page_handler;
    gulong          error_handler;
    gulong          finish_handler;

    GtkItemFactory *item_factory;

    GtkWidget      *forward_button;
    GtkWidget      *back_button;
};

static GtkItemFactoryEntry menu_items[] = {
    {N_("/_File"),                   NULL, 0, 0, "<Branch>"     },
    {N_("/File/_New window"),        NULL,
     window_new_window_cb,           0,
     "<StockItem>",                  GTK_STOCK_NEW              },
    {N_("/File/_Close window"),      NULL,
     window_close_window_cb,         0,
     "<StockItem>",                  GTK_STOCK_CLOSE            },

    {N_("/_Edit"),                   NULL, 0, 0, "<Branch>"     },
    /*
    {N_("/Edit/_Find in page..."),   NULL,
     window_find_cb,                 0,
     "<StockItem>",                  GTK_STOCK_FIND             },
    {N_("/Edit/_Find again"),        "<Control>g",
     window_find_again_cb,           0,
     "<StockItem>",                  GTK_STOCK_FIND             },
    */

    {N_("/_Go"),                     NULL, 0, 0, "<Branch>"     },
    /*
    {N_("/Go/_Back"),                NULL,
     window_history_go_cb,           YELP_WINDOW_ACTION_BACK,
     "<StockItem>",                  GTK_STOCK_GO_BACK          },
    {N_("/Go/_Forward"),             NULL,
     window_history_go_cb,           YELP_WINDOW_ACTION_FORWARD,
     "<StockItem>",                  GTK_STOCK_GO_FORWARD       },
    {N_("/Go/_Home"),                NULL,
     window_go_home_cb,              0,
     "<StockItem>",                  GTK_STOCK_HOME             },
    {N_("/Go/_Index"),               NULL,
     window_go_index_cb,             0,
     "<StockItem>",                  GTK_STOCK_INDEX            },
    */

    {N_("/_Help"),                   NULL, 0, 0, "<Branch>"     },
    {N_("/Help/_About"),             NULL,
     window_about_cb,                0,
     "<StockItem>",                  GNOME_STOCK_ABOUT          },
};

GType
yelp_window_get_type (void)
{
    static GType window_type = 0;

    if (!window_type) {
	static const GTypeInfo window_info = {
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

	window_type = g_type_register_static (GTK_TYPE_WINDOW,
					      "YelpWindow", 
					      &window_info, 0);
    }

    return window_type;
}

static void
window_init (YelpWindow *window)
{
    YelpWindowPriv *priv;
    gint width, height;

    priv = g_new0 (YelpWindowPriv, 1);
    window->priv = priv;

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

/******************************************************************************/

GtkWidget *
yelp_window_new (GNode *doc_tree, GList *index)
{
    YelpWindow     *window;
    YelpWindowPriv *priv;
	
    window = g_object_new (YELP_TYPE_WINDOW, NULL);

    priv   = window->priv;

    priv->doc_tree = doc_tree;
    priv->index    = index;

    window_populate (window);

    gtk_window_set_icon (GTK_WINDOW (window), window_load_icon ());

    return GTK_WIDGET (window);
}

void
yelp_window_open_uri (YelpWindow  *window,
		      YelpURI     *uri)
{
    g_return_if_fail (YELP_IS_WINDOW (window));

    yelp_history_goto (window->priv->history, uri);

    window_handle_uri (window, uri);
}

YelpURI *
yelp_window_get_current_uri (YelpWindow *window)
{
    g_return_val_if_fail (YELP_IS_WINDOW (window), NULL);
	
    return yelp_history_get_current (window->priv->history);
}

/******************************************************************************/

static void
window_error (YelpWindow *window, GError *error)
{
    GtkWidget *dialog;

    if (error) {
	dialog = gtk_message_dialog_new
	    (GTK_WINDOW (window),
	     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	     GTK_MESSAGE_ERROR,
	     GTK_BUTTONS_OK,
	     error->message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
    }
}

static void
window_populate (YelpWindow *window)
{
    YelpWindowPriv     *priv;
    GtkWidget          *toolbar;
    GtkAccelGroup      *accel_group;
    GtkWidget          *menu_item;
    GtkTreeSelection   *selection;

    priv = window->priv;

    priv->main_box = gtk_vbox_new (FALSE, 0);

    gtk_container_add (GTK_CONTAINER (window), priv->main_box);
	
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

    menu_item =
	gtk_item_factory_get_item_by_action (priv->item_factory,
					     YELP_WINDOW_ACTION_BACK);
    if (menu_item)
	gtk_widget_set_sensitive (menu_item, FALSE);

    menu_item =
	gtk_item_factory_get_item_by_action (priv->item_factory,
					     YELP_WINDOW_ACTION_FORWARD);
    if (menu_item)
	gtk_widget_set_sensitive (menu_item, FALSE);

    gtk_box_pack_start (GTK_BOX (priv->main_box),
			GTK_WIDGET (gtk_item_factory_get_widget
				    (priv->item_factory, "<main>")),
			FALSE, FALSE, 0);

    toolbar = window_create_toolbar (window);

    gtk_box_pack_start (GTK_BOX (priv->main_box),
			GTK_WIDGET (toolbar),
			FALSE, FALSE, 0);
			
    priv->pane = gtk_hpaned_new ();
    gtk_widget_ref (priv->pane);
    // We should probably remember the last position and set to that.
    gtk_paned_set_position (GTK_PANED (priv->pane), 180);

    priv->side_sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_ref (priv->side_sw);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->side_sw),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC);

    priv->side_sects = gtk_tree_view_new ();
    gtk_tree_view_insert_column_with_attributes
	(GTK_TREE_VIEW (priv->side_sects), -1,
	 _("Section"), gtk_cell_renderer_text_new (),
	 "text", 1,
	 NULL);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->side_sects));

    g_signal_connect (selection, "changed",
		      G_CALLBACK (tree_selection_changed_cb),
		      window);

    gtk_container_add (GTK_CONTAINER (priv->side_sw),     priv->side_sects);
    gtk_paned_add1    (GTK_PANED (priv->pane), priv->side_sw);

    priv->html_sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_ref (priv->html_sw);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->html_sw),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC);

    priv->html_view  = yelp_html_new ();
    g_signal_connect (priv->html_view,
		      "uri_selected",
		      G_CALLBACK (html_uri_selected_cb),
		      window);

    gtk_container_add (GTK_CONTAINER (priv->html_sw),
		       yelp_html_get_widget (priv->html_view));

    gtk_box_pack_start (GTK_BOX (priv->main_box),
			priv->html_sw,
			TRUE, TRUE, 0);

    gtk_widget_grab_focus (yelp_html_get_widget (priv->html_view));
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
    priv->back_button =
	gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
				 _("Back"),
				 _("Show previous page in history"),
				 NULL, icon, 
				 G_CALLBACK (window_back_clicked),
				 window);
    gtk_widget_set_sensitive (priv->back_button, FALSE);

    icon = gtk_image_new_from_stock ("gtk-go-forward", 
				     GTK_ICON_SIZE_LARGE_TOOLBAR);
    priv->forward_button =
	gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
				 _("Forward"),
				 _("Show next page in history"),
				 NULL, icon,
				 G_CALLBACK (window_forward_clicked),
				 window);
    gtk_widget_set_sensitive (priv->forward_button, FALSE);

    gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

    icon = gtk_image_new_from_stock ("gtk-home", 
				     GTK_ICON_SIZE_LARGE_TOOLBAR);
    button =
	gtk_toolbar_append_item (GTK_TOOLBAR (toolbar), 
				 _("Home"),
				 _("Go to home view"), 
				 NULL, icon,
				 G_CALLBACK (window_home_button_clicked),
				 window);

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

static void
window_set_sections (YelpWindow   *window,
		     GtkTreeModel *sections)
{
    GList *children, *child;
    gboolean has_child = FALSE;
    YelpWindowPriv *priv;

    g_return_if_fail (YELP_IS_WINDOW (window));
    priv = window->priv;

    if (sections) {
	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->side_sects), sections);

	children = gtk_container_get_children (GTK_CONTAINER (priv->main_box));

	for (child = children; child; child = child->next)
	    if (child->data == priv->html_sw)
		has_child = TRUE;

	if (has_child) {
	    gtk_container_remove (GTK_CONTAINER (priv->main_box),
				  priv->html_sw);
	    gtk_paned_add2       (GTK_PANED (priv->pane),
				  priv->html_sw);
	    gtk_box_pack_start   (GTK_BOX (priv->main_box),
				  priv->pane,
				  TRUE, TRUE, 0);
	    gtk_widget_show_all (priv->main_box);
	}
	g_list_free (children);
    } else {
	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->side_sects), sections);

	children = gtk_container_get_children (GTK_CONTAINER (priv->main_box));

	for (child = children; child; child = child->next)
	    if (child->data == priv->pane)
		has_child = TRUE;

	if (has_child) {
	    gtk_container_remove (GTK_CONTAINER (priv->main_box),
				  priv->pane);
	    gtk_container_remove (GTK_CONTAINER (priv->pane),
				  priv->html_sw);
	    gtk_box_pack_start   (GTK_BOX (priv->main_box),
				  priv->html_sw,
				  TRUE, TRUE, 0);
	    gtk_widget_show_all (priv->main_box);
	}
	g_list_free (children);
    }
}

static gboolean
window_handle_uri (YelpWindow *window,
		   YelpURI    *uri)
{
    YelpWindowPriv *priv;
    GError         *error = NULL;
    gboolean        handled = FALSE;

    priv = window->priv;

    g_return_val_if_fail (YELP_IS_WINDOW (window), FALSE);
    g_return_val_if_fail (YELP_IS_URI (uri), FALSE);

    if (!yelp_uri_exists (uri)) {
	gchar *str_uri = yelp_uri_to_string (uri);
		
	g_set_error (&error,
		     YELP_ERROR,
		     YELP_ERROR_URI_NOT_EXIST,
		     _("The document '%s' does not exist"), str_uri);
	window_error (window, error);

	g_free (str_uri);
	g_error_free (error);

	return FALSE;
    }

    switch (yelp_uri_get_resource_type (uri)) {
    case YELP_URI_TYPE_DOCBOOK_XML:
    case YELP_URI_TYPE_MAN:
    case YELP_URI_TYPE_INFO:
    case YELP_URI_TYPE_TOC:
	handled = window_handle_pager_uri (window, uri);
	break;
    case YELP_URI_TYPE_DOCBOOK_SGML:
	g_set_error (&error,
		     YELP_ERROR,
		     YELP_ERROR_NO_SGML,
		     _("DocBook SGML documents are no longer supported."));

	window_error (window, error);
	g_error_free (error);
	return FALSE;
    case YELP_URI_TYPE_HTML:
	handled = window_handle_html_uri (window, uri);
	break;
    case YELP_URI_TYPE_GHELP:
    case YELP_URI_TYPE_GHELP_OTHER:
    case YELP_URI_TYPE_INDEX:
    case YELP_URI_TYPE_PATH:
    case YELP_URI_TYPE_FILE:
    case YELP_URI_TYPE_UNKNOWN:
    case YELP_URI_TYPE_RELATIVE:
    default:
	break;
    }

    if (error)
	window_error (window, error);

    return handled;
}

static gboolean
window_handle_pager_uri (YelpWindow *window,
			 YelpURI    *uri)
{
    YelpWindowPriv *priv;
    GError         *error = NULL;
    gboolean    loadnow  = FALSE;
    gboolean    startnow = TRUE;
    gchar      *str_uri;
    gchar      *path;
    YelpPage   *page = NULL;
    YelpPager  *pager;

    priv = window->priv;

    window_disconnect (window);

    // Grab the appropriate pager from the cache
    if (yelp_uri_get_resource_type (uri) == YELP_URI_TYPE_TOC) {
	pager = YELP_PAGER (yelp_toc_pager_get ());
    } else {
	path  = yelp_uri_get_path (uri);
	pager = (YelpPager *) yelp_cache_lookup (path);

	// Create a new pager if one doesn't exist in the cache
	if (!pager) {
	    switch (yelp_uri_get_resource_type (uri)) {
	    case YELP_URI_TYPE_DOCBOOK_XML:
		pager = yelp_db_pager_new (uri);
		break;
	    case YELP_URI_TYPE_INFO:
		// FIXME: yelp_info_pager_new (uri);
		break;
	    case YELP_URI_TYPE_MAN:
		// FIXME: yelp_man_pager_new (uri);
		break;
	    default:
		break;
	    }

	    if (pager)
		yelp_cache_add (path, (GObject *) pager);
	}
    }

    if (!pager) {
	str_uri = yelp_uri_to_string (uri);
	g_set_error (&error,
		     YELP_ERROR,
		     YELP_ERROR_FAILED_OPEN,
		     _("The document '%s' could not be opened"), str_uri);
	window_error (window, error);
	g_free (str_uri);
	g_error_free (error);
    }

    g_object_ref (pager);
    if (priv->pager)
	g_object_unref (priv->pager);
    priv->pager = pager;

    switch (yelp_pager_get_state (pager)) {
    case YELP_PAGER_STATE_START:
	page = (YelpPage *) yelp_pager_lookup_page (pager, uri);
	loadnow  = (page ? TRUE : FALSE);
	startnow = FALSE;
	break;
    case YELP_PAGER_STATE_NEW:
    case YELP_PAGER_STATE_CANCEL:
	loadnow  = FALSE;
	startnow = TRUE;
	break;
    case YELP_PAGER_STATE_FINISH:
	page = (YelpPage *) yelp_pager_lookup_page (pager, uri);
	loadnow = TRUE;
	break;
    case YELP_PAGER_STATE_ERROR:
	error = yelp_pager_get_error (pager);
	window_error (window, error);
	return FALSE;
    default:
    	g_assert_not_reached ();
	break;
    }

    if (!loadnow) {
	gchar *loading = _("Loading...");
	yelp_html_clear (priv->html_view);

	gtk_window_set_title (GTK_WINDOW (window),
			      (const gchar *) loading);

	window_set_sections (window,
			     GTK_TREE_MODEL (yelp_pager_get_sections (pager)));

	yelp_html_printf
	    (priv->html_view,
	     "<html><head><meta http-equiv='Content-Type'"
	     " content='text/html=; charset=utf-8'>"
	     "<title>%s</title></head>"
	     "<body><center>%s</center></body>"
	     "</html>",
	     loading, loading);
	yelp_html_close (priv->html_view);

	yelp_toc_pager_pause (yelp_toc_pager_get ());
	while (gtk_events_pending ())
	    gtk_main_iteration ();
	yelp_toc_pager_unpause (yelp_toc_pager_get ());

	priv->page_handler =
	    g_signal_connect (pager,
			      "page",
			      G_CALLBACK (pager_page_cb),
			      window);
	priv->error_handler =
	    g_signal_connect (pager,
			      "error",
			      G_CALLBACK (pager_error_cb),
			      window);
	priv->finish_handler =
	    g_signal_connect (pager,
			      "finish",
			      G_CALLBACK (pager_finish_cb),
			      window);

	if (startnow)
	    yelp_pager_start (pager);
    } else {
	if (!page) {
	    gchar *str_uri = yelp_uri_to_string (uri);
	    g_set_error (&error,
			 YELP_ERROR,
			 YELP_ERROR_FAILED_OPEN,
			 _("The document '%s' could not be opened"), str_uri);

	    window_error (window, error);

	    g_free (str_uri);
	    g_error_free (error);

	    return FALSE;
	}

	window_set_sections (window,
			     GTK_TREE_MODEL (yelp_pager_get_sections (pager)));

	yelp_html_clear (priv->html_view);
	yelp_html_set_base_uri (priv->html_view, uri);

	gtk_window_set_title (GTK_WINDOW (window),
			      (const gchar *) page->title);

	yelp_html_write (priv->html_view,
			 page->chunk,
			 strlen (page->chunk));
    }

    return TRUE;
}

static gboolean
window_handle_html_uri (YelpWindow    *window,
			YelpURI       *uri)
{
    return FALSE;
}

static void
window_disconnect (YelpWindow *window)
{
    g_return_if_fail (YELP_IS_WINDOW (window));
    YelpWindowPriv *priv = window->priv;

    if (priv->page_handler) {
	g_signal_handler_disconnect (priv->pager,
				     priv->page_handler);
	priv->page_handler = 0;
    }
    if (priv->error_handler) {
	g_signal_handler_disconnect (priv->pager,
				     priv->error_handler);
	priv->error_handler = 0;
    }
    if (priv->finish_handler) {
	g_signal_handler_disconnect (priv->pager,
				     priv->finish_handler);
	priv->finish_handler = 0;
    }
}

static void
pager_page_cb (YelpPager *pager,
	       gchar     *page_id,
	       gpointer   user_data)
{
    YelpWindow *window = YELP_WINDOW (user_data);
    YelpURI    *uri;
    YelpPage   *page;

    uri  = yelp_window_get_current_uri (window);

    if (yelp_pager_uri_is_page (pager, page_id, uri)) {
	window_disconnect (window);
	page = (YelpPage *) yelp_pager_get_page (pager, page_id);

	yelp_html_clear (window->priv->html_view);
	yelp_html_set_base_uri (window->priv->html_view, uri);

	gtk_window_set_title (GTK_WINDOW (window),
			      (const gchar *) page->title);

	yelp_html_write (window->priv->html_view,
			 page->chunk,
			 strlen (page->chunk));
    }
}

static void
pager_error_cb (YelpPager   *pager,
		gpointer     user_data)
{
    YelpWindow *window = YELP_WINDOW (user_data);
    GError *error = yelp_pager_get_error (pager);

    window_disconnect (window);
    window_error (window, error);

    g_error_free (error);
}

static void
pager_finish_cb (YelpPager   *pager,
		 gpointer     user_data)
{
    GError *error = NULL;
    YelpWindow *window = YELP_WINDOW (user_data);
    YelpURI *uri;
    gchar   *str_uri;

    uri     = yelp_window_get_current_uri (window);
    str_uri = yelp_uri_to_string (uri);

    window_disconnect (window);

    g_set_error (&error,
		 YELP_ERROR,
		 YELP_ERROR_FAILED_OPEN,
		 _("The document '%s' could not be opened"), str_uri);

    window_error (window, error);

    g_free (str_uri);
    g_error_free (error);
}

static void
html_uri_selected_cb (YelpHtml  *html,
		      YelpURI   *uri,
		      gboolean   handled,
		      gpointer   user_data)
{
    YelpWindow *window = YELP_WINDOW (user_data);
    yelp_window_open_uri (window, uri);
}

static void
tree_selection_changed_cb (GtkTreeSelection *selection,
			   YelpWindow       *window)
{
    GtkTreeModel    *model;
    GtkTreeIter      iter;
    gchar           *id, *frag;
    YelpURI         *uri;

    YelpWindowPriv *priv = window->priv;

    if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->side_sects));
		
	gtk_tree_model_get (model, &iter,
			    0, &id,
			    -1);

	frag = g_strconcat ("#", id, NULL);
	uri = yelp_uri_new_relative
	    (yelp_window_get_current_uri (window), frag);

	yelp_window_open_uri (window, uri);

	g_free (frag);
    }
}


/******************************************************************************/

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
window_about_cb (gpointer data, guint section, GtkWidget *widget)
{
    static GtkWidget *about = NULL;

    if (about == NULL) {
	const gchar *authors[] = { 
	    "Mikael Hallendal <micke@imendio.com>",
	    "Alexander Larsson <alexl@redhat.com>",
	    "Shaun McCance <shaunm@gnome.org>",
	    NULL
	};
	/* Note to translators: put here your name (and address) so it
	 * will shop up in the "about" box */
	gchar       *translator_credits = _("translator_credits");

	about = gnome_about_new
	    (PACKAGE, VERSION,
	     "Copyright 2001-2003 Mikael Hallendal <micke@imendio.com>\n",
	     _("A Help Browser for GNOME"),
	     authors,
	     NULL,
	     strcmp (translator_credits, "translator_credits") != 0
	     ? translator_credits : NULL,
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

static void
window_back_clicked (GtkWidget  *button,
		     YelpWindow *window)
{
    g_return_if_fail (YELP_IS_WINDOW (window));

    window_history_action (window, YELP_WINDOW_ACTION_BACK);
}

static void
window_forward_clicked (GtkWidget  *button,
			YelpWindow *window)
{
    g_return_if_fail (YELP_IS_WINDOW (window));

    window_history_action (window, YELP_WINDOW_ACTION_FORWARD);
}

static void
window_home_button_clicked (GtkWidget  *button,
			    YelpWindow *window)
{
    YelpURI *uri;
	
    g_return_if_fail (YELP_IS_WINDOW (window));
	
    uri = yelp_uri_new ("toc:");

    yelp_window_open_uri (window, uri);

    g_object_unref (uri);
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
    if (menu_item)
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
    if (menu_item)
	gtk_widget_set_sensitive (menu_item, sensitive);
}

static void
window_history_action (YelpWindow        *window,
		       YelpHistoryAction  action)
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

