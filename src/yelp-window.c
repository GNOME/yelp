/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@imendio.com>
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
 * Author: Mikael Hallendal <micke@imendio.com>
 * Author: Shaun McCance <shaunm@gnome.org>
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
#include "yelp-toc-pager.h"
#include "yelp-theme.h"
#include "yelp-window.h"

#define d(x)
#define YELP_CONFIG_WIDTH  "/yelp/Geometry/width"
#define YELP_CONFIG_HEIGHT "/yelp/Geometry/height"
#define YELP_CONFIG_WIDTH_DEFAULT  "600"
#define YELP_CONFIG_HEIGHT_DEFAULT "420"

typedef enum {
    YELP_WINDOW_FIND_PREV = 1,
    YELP_WINDOW_FIND_NEXT
} YelpFindAction;

typedef enum {
    YELP_WINDOW_GO_BACK = 1,
    YELP_WINDOW_GO_FORWARD,
    YELP_WINDOW_GO_PREVIOUS,
    YELP_WINDOW_GO_NEXT,
    YELP_WINDOW_GO_TOC,

    YELP_WINDOW_GO_HOME
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
static void        window_handle_uri              (YelpWindow        *window,
						   GnomeVFSURI       *uri);
static gboolean    window_handle_pager_uri        (YelpWindow        *window,
						   GnomeVFSURI       *uri);
static gboolean    window_handle_html_uri         (YelpWindow        *window,
						   GnomeVFSURI       *uri);
static void        window_handle_page             (YelpWindow        *window,
						   YelpPage          *page,
						   GnomeVFSURI       *uri);
static void        window_disconnect              (YelpWindow        *window);

static void        pager_contents_cb              (YelpPager         *pager,
						   gpointer           user_data);
static void        pager_page_cb                  (YelpPager         *pager,
						   gchar             *page_id,
						   gpointer           user_data);
static void        pager_error_cb                 (YelpPager         *pager,
						   gpointer           user_data);
static void        pager_finish_cb                (YelpPager         *pager,
						   gpointer           user_data);

static void        html_uri_selected_cb           (YelpHtml          *html,
						   GnomeVFSURI       *uri,
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

static void        window_go_cb                   (gpointer           data,
						   guint              action,
						   GtkWidget         *widget);

static void        window_find_cb                 (gpointer           data,
						   guint              action,
						   GtkWidget         *widget);
static void        window_find_again_cb           (gpointer           data,
						   guint              action,
						   GtkWidget         *widget);
static gboolean    window_find_action             (YelpWindow        *window, 
						   YelpFindAction     action);
static gboolean    window_find_delete_event_cb    (GtkWidget         *widget,
						   GdkEvent          *event,
 			                           gpointer           data);
static void        window_find_entry_changed_cb   (GtkEditable       *editable,
						   gpointer           data);
static void        window_find_case_toggled_cb    (GtkWidget         *widget,
						   gpointer           data);
static void        window_find_wrap_toggled_cb    (GtkWidget         *widget,
						   gpointer           data);
static void        window_find_save_settings      (YelpWindow        *window);
static void        window_find_buttons_set_sensitive (YelpWindow      *window,
						      gboolean        sensitive);
static void        window_find_response_cb        (GtkWidget         *dialog ,
						   gint               response,
						   YelpWindow        *window);

static gboolean    tree_model_iter_following      (GtkTreeModel      *model,
						   GtkTreeIter       *iter);

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

    GtkWidget      *find_dialog;
    GtkWidget      *find_entry;
    GtkWidget      *case_checkbutton;
    GtkWidget      *wrap_checkbutton;
    GtkWidget      *find_prev_button;
    GtkWidget      *find_next_button;
    GtkWidget      *find_close_button;

    gchar          *find_string;
    gboolean        match_case;
    gboolean        wrap;

    YelpHistory    *history;

    YelpPager      *pager;
    gulong          contents_handler;
    gulong          page_handler;
    gulong          error_handler;
    gulong          finish_handler;

    GtkItemFactory *item_factory;

    GtkWidget      *forward_button;
    GtkWidget      *back_button;

    /* Don't free these */
    gchar          *prev;
    gchar          *next;
    gchar          *toc;
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
    {N_("/Edit/_Find..."),           "<Control>f",
     window_find_cb,                 0,
     "<StockItem>",                  GTK_STOCK_FIND             },
    {N_("/Edit/Find Ne_xt"),        "<Control>g",
     window_find_again_cb,           YELP_WINDOW_FIND_NEXT,
     "<Item>",                       NULL                       },
    {N_("/Edit/Find Pre_vious"),    "<Shift><Control>g",
     window_find_again_cb,           YELP_WINDOW_FIND_PREV, 
     "<Item>",                       NULL                       },

    {N_("/_Go"),                     NULL, 0, 0, "<Branch>"     },
    {N_("/Go/_Back"),                "<Alt>Left",
     window_go_cb,                   YELP_WINDOW_GO_BACK,
     "<StockItem>",                  GTK_STOCK_GO_BACK          },
    {N_("/Go/_Forward"),             "<Alt>Right",
     window_go_cb,                   YELP_WINDOW_GO_FORWARD,
     "<StockItem>",                  GTK_STOCK_GO_FORWARD       },
    {N_("/Go/_Home"),                NULL,
     window_go_cb,                   YELP_WINDOW_GO_HOME,
     "<StockItem>",                  GTK_STOCK_HOME             },
    {N_("/Go/"),                     NULL, 0, 0, "<Separator>"  },
    {N_("/Go/_Previous"),            "<Alt>Up",
     window_go_cb,                   YELP_WINDOW_GO_PREVIOUS,
     0,                              NULL                       },
    {N_("/Go/_Next"),                "<Alt>Down",
     window_go_cb,                   YELP_WINDOW_GO_NEXT,
     0,                              NULL                       },
    {N_("/Go/_Contents"),            NULL,
     window_go_cb,                   YELP_WINDOW_GO_TOC,
     0,                              NULL                       },

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
		      GnomeVFSURI *uri)
{
    YelpWindowPriv *priv;
    GnomeVFSURI    *cur_uri;

    g_return_if_fail (YELP_IS_WINDOW (window));
    g_return_if_fail (uri != NULL);

    priv = window->priv;

    cur_uri = yelp_history_get_current (window->priv->history);
    if (cur_uri && gnome_vfs_uri_equal (cur_uri, uri)) {
	const gchar *cur_frag, *frag;
	cur_frag = gnome_vfs_uri_get_fragment_identifier (cur_uri);
	frag     = gnome_vfs_uri_get_fragment_identifier (uri);

	if ((!cur_frag && !frag)
	    || (cur_frag && frag && !strcmp (cur_frag, frag)))
	    return;
    }

    yelp_history_goto (window->priv->history, uri);

    window_handle_uri (window, uri);
}

void
window_handle_uri (YelpWindow  *window,
		   GnomeVFSURI *uri)
{
    YelpWindowPriv *priv;
    GError         *error = NULL;
    gboolean        handled = FALSE;
    gchar          *str_uri;

    g_return_if_fail (YELP_IS_WINDOW (window));
    g_return_if_fail (uri != NULL);

    priv = window->priv;

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
	break;
    case YELP_URI_TYPE_HTML:
	handled = window_handle_html_uri (window, uri);
	break;
    case YELP_URI_TYPE_ERROR:
    default:
	str_uri = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
	g_set_error (&error,
		     YELP_ERROR,
		     YELP_ERROR_FAILED_OPEN,
		     _("The document '%s' could not be opened"), str_uri);
	g_free (str_uri);
	break;
    }
 
    if (error) {
	window_error (window, error);
	g_error_free (error);
    }

    window_find_buttons_set_sensitive (window, TRUE);    
}

GnomeVFSURI *
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
					     YELP_WINDOW_GO_BACK);
    if (menu_item)
	gtk_widget_set_sensitive (menu_item, FALSE);

    menu_item =
	gtk_item_factory_get_item_by_action (priv->item_factory,
					     YELP_WINDOW_GO_FORWARD);
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
	GError *error = NULL;
	GtkIconTheme *icon_theme = (GtkIconTheme *) yelp_theme_get_icon_theme ();

	pixbuf = gtk_icon_theme_load_icon (icon_theme,
					   "gnome-help",
					   36, 0,
					   &error);
	if (!pixbuf) {
	    g_warning ("Couldn't load icon: %s", error->message);
	    g_error_free (error);
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
window_handle_pager_uri (YelpWindow  *window,
			 GnomeVFSURI *uri)
{
    YelpWindowPriv *priv;
    GError         *error = NULL;
    gboolean     loadnow  = FALSE;
    gboolean     startnow = TRUE;
    gchar       *str_uri;
    gchar       *path;
    YelpPage    *page = NULL;
    YelpPager   *pager;
    YelpPagerState state;

    priv = window->priv;

    window_disconnect (window);

    // Grab the appropriate pager from the cache
    if (yelp_uri_get_resource_type (uri) == YELP_URI_TYPE_TOC) {
	pager = YELP_PAGER (yelp_toc_pager_get ());
    } else {
	path  = g_strdup (gnome_vfs_uri_get_path (uri));
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
	str_uri = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
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

    state = yelp_pager_get_state (pager);

    loadnow  = FALSE;
    startnow = TRUE;

    if (state & YELP_PAGER_STATE_STOPPED) {
	error = yelp_pager_get_error (pager);
	if (error) {
	    window_error (window, error);
	    return FALSE;
	}
    }

    if (state & YELP_PAGER_STATE_STARTED) {
	if (state & YELP_PAGER_STATE_CONTENTS) {
	    const gchar *frag_id = gnome_vfs_uri_get_fragment_identifier (uri);
	    gchar *page_id = yelp_pager_resolve_uri (pager, uri);

	    if (!page_id && (frag_id && strcmp (frag_id, ""))) {
		g_set_error (&error,
			     YELP_ERROR,
			     YELP_ERROR_FAILED_OPEN,
			     _("The page '%s' could not be found in this document."),
			     frag_id);
		window_error (window, error);

		g_free (page_id);
		g_error_free (error);

		return FALSE;
	    }

	    g_free (page_id);
	} else {
	    priv->contents_handler =
		g_signal_connect (pager,
				  "contents",
				  G_CALLBACK (pager_contents_cb),
				  window);
	}

	page = (YelpPage *) yelp_pager_lookup_page (pager, uri);
	loadnow  = (page ? TRUE : FALSE);
	startnow = FALSE;

	if (state & YELP_PAGER_STATE_FINISHED) {
	    if (!page) {
		const gchar *frag_id = gnome_vfs_uri_get_fragment_identifier (uri);
		g_set_error (&error,
			     YELP_ERROR,
			     YELP_ERROR_FAILED_OPEN,
			     _("The page '%s' could not be found in this document."),
			     frag_id);
		window_error (window, error);
		g_error_free (error);
		return FALSE;
	    }
	}
    } else {
	priv->contents_handler =
	    g_signal_connect (pager,
			      "contents",
			      G_CALLBACK (pager_contents_cb),
			      window);
    }

    window_set_sections (window,
			 GTK_TREE_MODEL (yelp_pager_get_sections (pager)));

    if (loadnow)
	window_handle_page (window, page, uri);

    else {
	GtkWidget *menu_item;
	gchar *loading = _("Loading...");
	GdkCursor *cursor = gdk_cursor_new (GDK_WATCH);

	yelp_html_clear (priv->html_view);

	gdk_window_set_cursor (GTK_WIDGET (window)->window, cursor);
	gdk_cursor_unref (cursor);

	menu_item =
	    gtk_item_factory_get_item_by_action (priv->item_factory,
						 YELP_WINDOW_GO_PREVIOUS);
	if (menu_item)
	    gtk_widget_set_sensitive (menu_item, FALSE);
	menu_item =
	    gtk_item_factory_get_item_by_action (priv->item_factory,
						 YELP_WINDOW_GO_NEXT);
	if (menu_item)
	    gtk_widget_set_sensitive (menu_item, FALSE);
	menu_item =
	    gtk_item_factory_get_item_by_action (priv->item_factory,
						 YELP_WINDOW_GO_TOC);
	if (menu_item)
	    gtk_widget_set_sensitive (menu_item, FALSE);

	gtk_window_set_title (GTK_WINDOW (window),
			      (const gchar *) loading);

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
    }

    return TRUE;
}

static gboolean
window_handle_html_uri (YelpWindow    *window,
			GnomeVFSURI   *uri)
{
    GtkWidget      *menu_item;
    YelpWindowPriv *priv;

    g_return_val_if_fail (YELP_IS_WINDOW (window), FALSE);
    g_return_val_if_fail (uri != NULL, FALSE);

    window_set_sections (window, NULL);

    menu_item =
	gtk_item_factory_get_item_by_action (priv->item_factory,
					     YELP_WINDOW_GO_PREVIOUS);
    if (menu_item)
	gtk_widget_set_sensitive (menu_item, FALSE);
    menu_item =
	gtk_item_factory_get_item_by_action (priv->item_factory,
					     YELP_WINDOW_GO_NEXT);
    if (menu_item)
	gtk_widget_set_sensitive (menu_item, FALSE);
    menu_item =
	gtk_item_factory_get_item_by_action (priv->item_factory,
					     YELP_WINDOW_GO_TOC);
    if (menu_item)
	gtk_widget_set_sensitive (menu_item, FALSE);

    /*
    gtk_window_set_title (GTK_WINDOW (window),
			  (const gchar *) page->title);

    yelp_html_clear (priv->html_view);
    yelp_html_set_base_uri (priv->html_view, uri);
    yelp_html_write (priv->html_view,
		     page->chunk,
		     strlen (page->chunk));
    */
    return FALSE;
}

static void
window_handle_page (YelpWindow   *window,
		    YelpPage     *page,
		    GnomeVFSURI  *uri)
{
    GtkWidget      *menu_item;
    GtkTreeModel   *model;
    GtkTreeIter     iter;
    YelpWindowPriv *priv;
    gchar          *id;
    gboolean        valid;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->side_sects));

    if (model) {
	valid = gtk_tree_model_get_iter_first (model, &iter);
	while (valid) {
	    gtk_tree_model_get (model, &iter,
				0, &id,
				-1);
	    if (yelp_pager_uri_is_page (priv->pager, id, uri)) {
		GtkTreePath *path = gtk_tree_model_get_path (model, &iter);

		gtk_tree_view_expand_to_path (GTK_TREE_VIEW (priv->side_sects),
					      path);
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->side_sects),
					  path, NULL, FALSE);

		gtk_tree_path_free (path);
		break;
	    }

	    valid = tree_model_iter_following (model, &iter);
	}
    }

    if (page->prev) {
	priv->prev = page->prev;
	menu_item =
	    gtk_item_factory_get_item_by_action (priv->item_factory,
						 YELP_WINDOW_GO_PREVIOUS);
	if (menu_item)
	    gtk_widget_set_sensitive (menu_item, TRUE);
    }
    if (page->next) {
	priv->next = page->next;
	menu_item =
	    gtk_item_factory_get_item_by_action (priv->item_factory,
						 YELP_WINDOW_GO_NEXT);
	if (menu_item)
	    gtk_widget_set_sensitive (menu_item, TRUE);
    }
    if (page->toc) {
	priv->toc = page->toc;
	menu_item =
	    gtk_item_factory_get_item_by_action (priv->item_factory,
						 YELP_WINDOW_GO_TOC);
	if (menu_item)
	    gtk_widget_set_sensitive (menu_item, TRUE);
    }

    gtk_window_set_title (GTK_WINDOW (window),
			  (const gchar *) page->title);

    yelp_html_clear (priv->html_view);
    yelp_html_set_base_uri (priv->html_view, uri);
    yelp_html_write (priv->html_view,
		     page->chunk,
		     strlen (page->chunk));
}

static void
window_disconnect (YelpWindow *window)
{
    YelpWindowPriv *priv = window->priv;
    g_return_if_fail (YELP_IS_WINDOW (window));

    gdk_window_set_cursor (GTK_WIDGET (window)->window, NULL);

    if (priv->contents_handler) {
	g_signal_handler_disconnect (priv->pager,
				     priv->contents_handler);
	priv->contents_handler = 0;
    }
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
pager_contents_cb (YelpPager   *pager,
		   gpointer     user_data)
{
    YelpWindow  *window = YELP_WINDOW (user_data);
    GnomeVFSURI *uri    = NULL;
    GError      *error  = NULL;
    const gchar *frag;
    gchar       *page;

    uri  = yelp_window_get_current_uri (window);
    frag = gnome_vfs_uri_get_fragment_identifier (uri);
    page = yelp_pager_resolve_uri (pager, uri);

    if (!page && (frag && strcmp (frag, ""))) {
	const gchar *frag_id = gnome_vfs_uri_get_fragment_identifier (uri);

	window_disconnect (window);

	g_set_error (&error,
		     YELP_ERROR,
		     YELP_ERROR_FAILED_OPEN,
		     _("The page '%s' could not be found in this document."),
		     frag_id);
	window_error (window, error);
	g_error_free (error);
    }
    g_free (page);
}

static void
pager_page_cb (YelpPager *pager,
	       gchar     *page_id,
	       gpointer   user_data)
{
    YelpWindow  *window = YELP_WINDOW (user_data);
    GnomeVFSURI *uri;
    YelpPage    *page;

    uri  = yelp_window_get_current_uri (window);

    if (yelp_pager_uri_is_page (pager, page_id, uri)) {
	window_disconnect (window);

	page = (YelpPage *) yelp_pager_get_page (pager, page_id);
	window_handle_page (window, page, uri);
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

    // FIXME: Remove the URI from the history and go back
}

static void
pager_finish_cb (YelpPager   *pager,
		 gpointer     user_data)
{
    GError *error = NULL;
    YelpWindow  *window = YELP_WINDOW (user_data);
    GnomeVFSURI *uri;
    const gchar *frag;

    uri  = yelp_window_get_current_uri (window);
    frag = gnome_vfs_uri_get_fragment_identifier (uri);

    window_disconnect (window);

    g_set_error (&error,
		 YELP_ERROR,
		 YELP_ERROR_FAILED_OPEN,
		 _("The page '%s' could not be found in this document."),
		 frag);
    window_error (window, error);

    g_error_free (error);

    // FIXME: Remove the URI from the history and go back
}

static void
html_uri_selected_cb (YelpHtml    *html,
		      GnomeVFSURI *uri,
		      gboolean     handled,
		      gpointer     user_data)
{
    YelpWindow *window = YELP_WINDOW (user_data);

    if (!handled) {
	yelp_window_open_uri (window, uri);
	gnome_vfs_uri_unref (uri);
    }
}

static void
tree_selection_changed_cb (GtkTreeSelection *selection,
			   YelpWindow       *window)
{
    GtkTreeModel    *model;
    GtkTreeIter      iter;
    gchar           *id, *frag;
    GnomeVFSURI     *uri;

    YelpWindowPriv *priv = window->priv;

    if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->side_sects));
		
	gtk_tree_model_get (model, &iter,
			    0, &id,
			    -1);

	frag = g_strconcat ("#", id, NULL);
	uri = yelp_uri_resolve_relative
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

    window_history_action (window, YELP_WINDOW_GO_BACK);
}

static void
window_forward_clicked (GtkWidget  *button,
			YelpWindow *window)
{
    g_return_if_fail (YELP_IS_WINDOW (window));

    window_history_action (window, YELP_WINDOW_GO_FORWARD);
}

static void        
window_go_cb (gpointer   data,
	      guint      action,
	      GtkWidget *widget)
{
    YelpWindow  *window;
    GnomeVFSURI *uri;

    g_return_if_fail (YELP_IS_WINDOW (YELP_WINDOW (data)));

    window = YELP_WINDOW (data);

    switch (action) {
    case YELP_WINDOW_GO_BACK:
    case YELP_WINDOW_GO_FORWARD:
	window_history_action (YELP_WINDOW (data), action);
	break;
    case YELP_WINDOW_GO_HOME:
	window_home_button_clicked (NULL, YELP_WINDOW (data));
	break;

    case YELP_WINDOW_GO_PREVIOUS:
	uri = yelp_uri_resolve_relative (yelp_window_get_current_uri (window),
					 window->priv->prev);
	yelp_window_open_uri (window, uri);
	gnome_vfs_uri_unref (uri);
	break;
    case YELP_WINDOW_GO_NEXT:
	uri = yelp_uri_resolve_relative (yelp_window_get_current_uri (window),
					 window->priv->next);
	yelp_window_open_uri (window, uri);
	gnome_vfs_uri_unref (uri);
	break;
    case YELP_WINDOW_GO_TOC:
	uri = yelp_uri_resolve_relative (yelp_window_get_current_uri (window),
					 window->priv->toc);
	yelp_window_open_uri (window, uri);
	gnome_vfs_uri_unref (uri);
	break;
    default:
	break;
    }
}

static void
window_home_button_clicked (GtkWidget  *button,
			    YelpWindow *window)
{
    GnomeVFSURI *uri;
	
    g_return_if_fail (YELP_IS_WINDOW (window));
	
    uri = yelp_pager_get_uri (YELP_PAGER (yelp_toc_pager_get ()));

    yelp_window_open_uri (window, uri);
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
						     YELP_WINDOW_GO_BACK);
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
						     YELP_WINDOW_GO_FORWARD);
    if (menu_item)
	gtk_widget_set_sensitive (menu_item, sensitive);
}

static void
window_history_action (YelpWindow        *window,
		       YelpHistoryAction  action)
{
    YelpWindowPriv *priv;
    GnomeVFSURI    *uri;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

    switch (action) {
    case YELP_WINDOW_GO_BACK:
	uri = yelp_history_go_back (priv->history);
	break;
    case YELP_WINDOW_GO_FORWARD:
	uri = yelp_history_go_forward (priv->history);
	break;
    default:
	return;
    }
	
    if (uri) {
	window_handle_uri (window, uri);
    }
}

static gboolean
window_find_delete_event_cb (GtkWidget *widget,
			     GdkEvent  *event,
			     gpointer  data)
{
    gtk_widget_hide (widget);
    
    return TRUE;
}

static void
window_find_save_settings (YelpWindow * window)
{
    YelpWindowPriv *priv;
    const gchar    *tmp;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

    tmp = gtk_entry_get_text (GTK_ENTRY (priv->find_entry));

    priv->match_case = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->case_checkbutton));
    priv->wrap       = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->wrap_checkbutton));

    g_free (priv->find_string);
		
    if (!priv->match_case) {
	priv->find_string = g_utf8_casefold (tmp, -1);
    } else {
	priv->find_string = g_strdup (tmp);
    }				  
}

static gboolean
window_find_action (YelpWindow *window, YelpFindAction action)
{
    YelpWindowPriv *priv;

    priv = window->priv;

    window_find_save_settings (window);

    return yelp_html_find (priv->html_view,
			   priv->find_string,
			   priv->match_case,
			   priv->wrap,
			   (action == YELP_WINDOW_FIND_NEXT) ? TRUE: FALSE);
}

static void 
window_find_again_cb (gpointer   data,
                      guint      action,
		      GtkWidget *widget)
{
    YelpWindow *window;

    g_return_if_fail (YELP_IS_WINDOW (data));
 
    window = YELP_WINDOW (data);

    window_find_action (window, (YelpFindAction)action);
}

static void
window_find_response_cb (GtkWidget  *dialog,
			 gint        response,
			 YelpWindow *window)
{
    YelpWindowPriv * priv;

    priv = window->priv;

    switch (response) {
    case GTK_RESPONSE_CLOSE:
	gtk_widget_hide (dialog);
	break;
	
    case YELP_WINDOW_FIND_NEXT:
    case YELP_WINDOW_FIND_PREV:
	if (!window_find_action (window, response)) {
	    if (YELP_WINDOW_FIND_NEXT == response) {
		gtk_widget_set_sensitive (priv->find_next_button, FALSE);
		gtk_widget_set_sensitive (priv->find_prev_button, TRUE);
	    } else {
		gtk_widget_set_sensitive (priv->find_next_button, TRUE);
		gtk_widget_set_sensitive (priv->find_prev_button, FALSE);
	    }
	} else {
	    window_find_buttons_set_sensitive (window, TRUE);
	}

	break;

    default:
	break;
    }
}

static void
window_find_entry_changed_cb (GtkEditable *editable,
			      gpointer     data)
{
    YelpWindow     *window;
    gchar          *text;

    g_return_if_fail (YELP_IS_WINDOW(data));
	
    window = YELP_WINDOW (data);

    text = gtk_editable_get_chars (editable, 0, -1);

    if (!strlen (text)) {
	window_find_buttons_set_sensitive (window, FALSE);
    } else {
	window_find_buttons_set_sensitive (window, TRUE);
    }
 
    g_free (text);
}

static void
window_find_case_toggled_cb (GtkWidget *widget,
			     gpointer   data)
{
    YelpWindow     *window;
    YelpWindowPriv *priv;

    g_return_if_fail (YELP_IS_WINDOW (data));

    window = YELP_WINDOW (data);
    priv   = window->priv;

    window_find_entry_changed_cb (GTK_EDITABLE (priv->find_entry),
				  window);
}

static void
window_find_wrap_toggled_cb (GtkWidget *widget,
			     gpointer   data)
{
    YelpWindow     *window;
    YelpWindowPriv *priv;

    g_return_if_fail (YELP_IS_WINDOW (data));

    window = YELP_WINDOW (data);
    priv   = window->priv;

    window_find_entry_changed_cb (GTK_EDITABLE (priv->find_entry),
				  window);
}

static void
window_find_buttons_set_sensitive (YelpWindow *window,
				  gboolean    sensitive)
{
    YelpWindowPriv *priv;

    priv = window->priv;

    if (priv->find_dialog) {
	gtk_widget_set_sensitive (priv->find_next_button, sensitive);
	gtk_widget_set_sensitive (priv->find_prev_button, sensitive);
    }
}

static void
window_find_cb (gpointer   data,     
		guint      action,
		GtkWidget *widget)
{
    YelpWindow     *window;
    YelpWindowPriv *priv;
    GladeXML       *glade;
    GtkWidget      *gnome_entry;

    g_return_if_fail (YELP_IS_WINDOW (data));
    
    window = YELP_WINDOW (data);

    priv = window->priv;

    if (!priv->find_dialog) {
	glade = glade_xml_new (DATADIR "/yelp/ui/yelp.glade", "find_dialog", NULL);
	
	if (!glade) {
	    g_warning ("Couldn't find necessary glade find " DATADIR "/yelp/ui/yelp.glade");
	    return;
	}

	priv->find_dialog       = glade_xml_get_widget (glade, "find_dialog");	
	priv->find_entry        = glade_xml_get_widget (glade, "find_entry");
	priv->case_checkbutton  = glade_xml_get_widget (glade, "case_check");
	priv->wrap_checkbutton  = glade_xml_get_widget (glade, "wrap_check");
	priv->find_next_button  = glade_xml_get_widget (glade, "next_button");
	priv->find_prev_button  = glade_xml_get_widget (glade, "prev_button");
	priv->find_close_button = glade_xml_get_widget (glade, "close_button");	
	gnome_entry             = glade_xml_get_widget (glade, "find_gnome_entry");

	gtk_window_set_transient_for (GTK_WINDOW (priv->find_dialog), 
				      GTK_WINDOW (window));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (priv->find_dialog),
					    TRUE);

	gtk_combo_set_use_arrows (GTK_COMBO (gnome_entry), FALSE);
	
	gtk_entry_set_activates_default (GTK_ENTRY (priv->find_entry), TRUE);
	gtk_widget_grab_default (glade_xml_get_widget (glade, "next_button"));

	gtk_widget_set_sensitive (priv->find_next_button, FALSE);
	gtk_widget_set_sensitive (priv->find_prev_button, FALSE);

	g_signal_connect (G_OBJECT (priv->find_entry), "changed",
			  G_CALLBACK (window_find_entry_changed_cb), window);	

	g_signal_connect (G_OBJECT (priv->wrap_checkbutton), "toggled",
			  G_CALLBACK (window_find_wrap_toggled_cb), window);

	g_signal_connect (G_OBJECT (priv->case_checkbutton), "toggled",
			  G_CALLBACK (window_find_case_toggled_cb), window);

	g_signal_connect (G_OBJECT (priv->find_dialog), "delete_event",
			  G_CALLBACK (window_find_delete_event_cb), NULL);

 	g_signal_connect (G_OBJECT (priv->find_dialog), "response", 
 			  G_CALLBACK (window_find_response_cb), window); 

	g_object_unref (glade);
    }

    gtk_editable_select_region (GTK_EDITABLE (priv->find_entry), 0, -1);

    gtk_window_present (GTK_WINDOW (priv->find_dialog));
}

// This would be nice to have in GTK+
static gboolean
tree_model_iter_following (GtkTreeModel  *model,
			   GtkTreeIter   *iter)
{
    gboolean     valid;
    GtkTreeIter *old_iter = gtk_tree_iter_copy (iter);

    if (gtk_tree_model_iter_has_child (model, iter))
	return gtk_tree_model_iter_children (model, iter, old_iter);
    else do {
	valid = gtk_tree_model_iter_next (model, iter);

	if (valid) {
	    gtk_tree_iter_free (old_iter);
	    return TRUE;
	} else {
	    *iter = *old_iter;

	    valid = gtk_tree_model_iter_parent (model, iter, old_iter);

	    if (!valid) {
		gtk_tree_iter_free (old_iter);
		return FALSE;
	    }
	}
    } while (TRUE);

    g_assert_not_reached ();
    return FALSE;
}
