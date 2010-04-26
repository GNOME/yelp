/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2003,2004 Shaun McCance <shaunm@gnome.org>
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

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <string.h>

#include "yelp-bookmarks.h"
#include "yelp-utils.h"
#include "yelp-html.h"
#include "yelp-settings.h"
#include "yelp-toc.h"
#include "yelp-docbook.h"
#include "yelp-db-print.h"
#include "yelp-mallard.h"
#include "yelp-window.h"
#include "yelp-print.h"
#include "yelp-debug.h"


#include "yelp-man.h"
#include "yelp-info.h"
#include "yelp-search.h"
#include "gtkentryaction.h"

#define YELP_CONFIG_PATH           "/.gnome2/yelp"
#define YELP_CONFIG_GEOMETRY_GROUP "Geometry"
#define YELP_CONFIG_WIDTH          "width"
#define YELP_CONFIG_HEIGHT         "height"
#define YELP_CONFIG_WIDTH_DEFAULT  600
#define YELP_CONFIG_HEIGHT_DEFAULT 420

#define BUFFER_SIZE 16384

typedef struct {
    YelpDocument *doc;
    YelpRrnType type;

    gchar *uri;
    gchar *req_uri;
    gchar *frag_id;
    gchar *base_uri;
    GtkWidget *menu_entry;
    YelpWindow *window;
    gint callback;

    gchar *page_title;
    gchar *frag_title;
} YelpHistoryEntry;

typedef struct {
    YelpPage *page;
    YelpWindow *window;

} YelpLoadData;

static void        window_init		          (YelpWindow        *window);
static void        window_class_init	          (YelpWindowClass   *klass);

static void        window_error                   (YelpWindow        *window,
						   gchar             *title,
						   gchar             *message,
						   gboolean           pop);
static void        window_populate                (YelpWindow        *window);
static void        window_populate_find           (YelpWindow        *window,
						   GtkWidget         *find_bar);
static void        window_set_sections            (YelpWindow        *window,
						   GtkTreeModel      *sections);
static void        window_set_section_cursor      (YelpWindow        *window,
						   GtkTreeModel      *model);

static gboolean    window_do_load_html            (YelpWindow        *window,
						   gchar             *uri,
						   gchar             *frag_id,
						   YelpRrnType        type,
						   gboolean           need_history);
static void        window_set_loading             (YelpWindow        *window);
static void        window_setup_window            (YelpWindow        *window,
						   YelpRrnType        type,
						   gchar             *loading_uri,
						   gchar             *frag,
						   gchar             *req_uri,
						   gchar             *base_uri,
						   gboolean           add_history);

/** Window Callbacks **/
static gboolean    window_configure_cb            (GtkWidget         *widget,
						   GdkEventConfigure *event,
						   gpointer           data);

/** Gecko Callbacks **/
static void        html_uri_selected_cb           (YelpHtml          *html,
						   gchar             *uri,
						   gboolean           handled,
						   gpointer           user_data);
static gboolean    html_frame_selected_cb         (YelpHtml          *html,
						   gchar             *uri,
						   gboolean           handled,
						   gpointer           user_data);
static void        html_title_changed_cb          (YelpHtml          *html,
						   gchar             *title,
						   gpointer           user_data);
static void        html_popupmenu_requested_cb    (YelpHtml *html,
						   gchar *uri,
						   gpointer user_data);
static gboolean    window_key_event_cb            (GtkWidget   *widget,
						   GdkEventKey *event,
						   YelpWindow  *window);
/** GtkTreeView Callbacks **/
static void        tree_selection_changed_cb      (GtkTreeSelection  *selection,
						   YelpWindow        *window);
static void        tree_drag_data_get_cb          (GtkWidget         *widget,
						   GdkDragContext    *context,
						   GtkSelectionData  *selection,
						   guint              info,
						   guint32            time,
						   YelpWindow        *window);
static void        tree_row_expand_cb             (GtkTreeView       *view,
						   GtkTreePath       *path,
						   GtkTreeViewColumn *column,
						   YelpWindow        *window);

/** UIManager Callbacks **/
static void    window_add_widget        (GtkUIManager *ui_manager,
					 GtkWidget    *widget,
					 GtkWidget    *vbox);
static void    window_new_window_cb     (GtkAction *action, YelpWindow *window);
static void    window_about_document_cb (GtkAction *action, YelpWindow *window);
static void    window_print_document_cb (GtkAction *action, YelpWindow *window);
static void    window_print_page_cb    (GtkAction *action, YelpWindow *window);
static void    window_open_location_cb  (GtkAction *action, YelpWindow *window);
static void    window_close_window_cb   (GtkAction *action, YelpWindow *window);
static void    window_copy_cb           (GtkAction *action, YelpWindow *window);
static void    window_select_all_cb     (GtkAction *action, YelpWindow *window);
static void    window_find_cb           (GtkAction *action, YelpWindow *window);
static void    window_preferences_cb    (GtkAction *action, YelpWindow *window);
static void    window_reload_cb         (GtkAction *action, YelpWindow *window);
static void    window_enable_cursor_cb  (GtkAction *action, YelpWindow *window);
static void    window_go_back_cb        (GtkAction *action, YelpWindow *window);
static void    window_go_forward_cb     (GtkAction *action, YelpWindow *window);
static void    window_go_home_cb        (GtkAction *action, YelpWindow *window);
static void    window_go_previous_cb    (GtkAction *action, YelpWindow *window);
static void    window_go_next_cb        (GtkAction *action, YelpWindow *window);
static void    window_go_toc_cb         (GtkAction *action, YelpWindow *window);
static void    window_add_bookmark_cb   (GtkAction *action, YelpWindow *window);
static void    window_help_contents_cb  (GtkAction *action, YelpWindow *window);
static void    window_about_cb          (GtkAction *action, YelpWindow *window);
static void    window_copy_link_cb      (GtkAction *action, YelpWindow *window);
static void    window_open_link_cb      (GtkAction *action, YelpWindow *window);
static void    window_open_link_new_cb  (GtkAction *action, YelpWindow *window);
static void    window_copy_mail_cb      (GtkAction *action, YelpWindow *window);

/** History Functions **/
static void               history_push_back      (YelpWindow       *window);
static void               history_push_forward   (YelpWindow       *window);
static void               history_clear_forward  (YelpWindow       *window);
static void               history_step_back      (YelpWindow       *window);
static YelpHistoryEntry * history_pop_back       (YelpWindow       *window);
static YelpHistoryEntry * history_pop_forward    (YelpWindow       *window);
static void               history_entry_free     (YelpHistoryEntry *entry);
static void               history_back_to        (GtkMenuItem      *menuitem,
						  YelpHistoryEntry *entry);
static void               history_forward_to     (GtkMenuItem      *menuitem,
						  YelpHistoryEntry *entry);
static void               history_load_entry     (YelpWindow       *window,
						  YelpHistoryEntry *entry);

static void               location_response_cb    (GtkDialog       *dialog,
						   gint             id,
						   YelpWindow      *window);

static gboolean    window_find_action             (YelpWindow        *window);
static void        window_find_entry_changed_cb   (GtkEditable       *editable,
						   gpointer           data);
static void        window_find_entry_activate_cb   (GtkEditable       *editable,
						    gpointer         data);
static gboolean    window_find_entry_key_pressed_cb (GtkWidget       *widget,
						     GdkEventKey     *key,
						     gpointer         data);
static void        window_find_save_settings      (YelpWindow        *window);
static void        window_find_buttons_set_sensitive (YelpWindow      *window,
						      gboolean        prev,
						      gboolean        next);
static void        window_find_clicked_cb         (GtkWidget         *button,
						   YelpWindow        *window);
static gboolean    window_find_hide_cb            (GtkWidget *widget,
						   GdkEventFocus *event,
						   YelpWindow *window);
static void        window_find_next_cb            (GtkAction *action,
						   YelpWindow *window);
static void        window_find_previous_cb        (GtkAction *action,
						   YelpWindow *window);
static gboolean    tree_model_iter_following      (GtkTreeModel      *model,
						   GtkTreeIter       *iter);
static gboolean    window_write_html              (YelpLoadData      *data);
static void        window_write_print_html        (YelpHtml          *html,
						   YelpPage          *page);

enum {
    NEW_WINDOW_REQUESTED,
    LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

#define YELP_WINDOW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_WINDOW, YelpWindowPriv))

struct _YelpWindowPriv {
    /* Main Widgets */
    GtkWidget      *main_box;
    GtkWidget      *pane;
    GtkWidget      *side_sects;
    GtkWidget      *html_pane;
    GtkWidget      *find_bar;
    GtkWidget      *find_entry;
    YelpHtml       *html_view;
    GtkWidget      *side_sw;
    GtkWidget      *search_action;

    /* Find in Page */
    GtkToolItem    *find_prev;
    GtkToolItem    *find_next;
    gchar          *find_string;
    GtkAction      *find_next_menu;
    GtkAction      *find_prev_menu;
    GtkToolItem    *find_sep;
    GtkToolItem    *find_not_found;

    /* Open Location */
    GtkWidget      *location_dialog;
    GtkWidget      *location_entry;

    /* Popup menu*/
    GtkWidget      *popup;
    gint            merge_id;
    GtkWidget      *maillink;

    /* Location Information */
    gchar          *uri;
    YelpRrnType     current_type;
    gchar          *req_uri;
    gchar          *base_uri;
    gint            current_request;
    YelpDocument   *current_document;
    gchar          *current_frag;
    GSList         *history_back;
    GSList         *history_forward;
    GtkWidget      *back_menu;
    GtkWidget      *forward_menu;
    GtkTreeModel   *current_sidebar;

    /* Callbacks and Idles */
    gulong          start_handler;
    gulong          page_handler;
    gulong          error_handler;
    gulong          cancel_handler;
    gulong          finish_handler;

    gint            toc_pause;

    GtkActionGroup *action_group;
    GtkUIManager   *ui_manager;

    /* Don't free these */
    gchar          *prev_id;
    gchar          *next_id;
    gchar          *toc_id;
};

#define TARGET_TYPE_URI_LIST     "text/uri-list"
enum {
    TARGET_URI_LIST
};

static const GtkActionEntry entries[] = {
    { "FileMenu",      NULL, N_("_File")      },
    { "EditMenu",      NULL, N_("_Edit")      },
    { "GoMenu",        NULL, N_("_Go")        },
    { "BookmarksMenu", NULL, N_("_Bookmarks") },
    { "HelpMenu",      NULL, N_("_Help")      },

    { "NewWindow", GTK_STOCK_NEW,
      N_("_New Window"),
      "<Control>N",
      NULL,
      G_CALLBACK (window_new_window_cb) },
    { "PrintDocument", NULL,
      N_("Print This Document ..."),
      NULL,
      NULL,
      G_CALLBACK (window_print_document_cb) },
    { "PrintPage", NULL,
      N_("Print This Page ..."),
      NULL,
      NULL,
      G_CALLBACK (window_print_page_cb) },
    { "AboutDocument", NULL,
      N_("About This Document"),
      NULL,
      NULL,
      G_CALLBACK (window_about_document_cb) },
    { "OpenLocation", NULL,
      N_("Open _Location"),
      "<Control>L",
      NULL,
      G_CALLBACK (window_open_location_cb) },
    { "CloseWindow", GTK_STOCK_CLOSE,
      N_("_Close Window"),
      "<Control>W",
      NULL,
      G_CALLBACK (window_close_window_cb) },

    { "Copy", GTK_STOCK_COPY,
      N_("_Copy"),
      "<Control>C",
      NULL,
      G_CALLBACK (window_copy_cb) },

    { "SelectAll", NULL,
      N_("_Select All"),
      "<Control>A",
      NULL,
      G_CALLBACK (window_select_all_cb) },
    { "Find", GTK_STOCK_FIND,
      N_("_Find..."),
      "<Control>F",
      NULL,
      G_CALLBACK (window_find_cb) },
    { "FindPrev", NULL,
      N_("Find Pre_vious"),
      "<Control><shift>G",
      N_("Find previous occurrence of the word or phrase"),
      G_CALLBACK (window_find_previous_cb) },
    { "FindNext", NULL,
      N_("Find Ne_xt"),
      "<Control>G",
      N_("Find next occurrence of the word or phrase"),
      G_CALLBACK (window_find_next_cb) },
    { "Preferences", GTK_STOCK_PREFERENCES,
      N_("_Preferences"),
      NULL, NULL,
      G_CALLBACK (window_preferences_cb) },

    { "Reload", NULL,
      N_("_Reload"),
      "<Control>R",
      NULL,
      G_CALLBACK (window_reload_cb) },

    { "TextCursor", NULL,
      "TextCursor",
      "F7",
      NULL,
      G_CALLBACK (window_enable_cursor_cb) },

    { "GoBack", GTK_STOCK_GO_BACK,
      N_("_Back"),
      "<Alt>Left",
      N_("Show previous page in history"),
      G_CALLBACK (window_go_back_cb) },
    { "GoForward", GTK_STOCK_GO_FORWARD,
      N_("_Forward"),
      "<Alt>Right",
      N_("Show next page in history"),
      G_CALLBACK (window_go_forward_cb) },
    { "GoHome", GTK_STOCK_HOME,
      N_("_Help Topics"),
      "<Alt>Home",
      N_("Go to the listing of help topics"),
      G_CALLBACK (window_go_home_cb) },
    { "GoPrevious", NULL,
      N_("_Previous Section"),
      "<Alt>Up",
      NULL,
      G_CALLBACK (window_go_previous_cb) },
    { "GoNext", NULL,
      N_("_Next Section"),
      "<Alt>Down",
      NULL,
      G_CALLBACK (window_go_next_cb) },
    { "GoContents", NULL,
      N_("_Contents"),
      NULL,
      NULL,
      G_CALLBACK (window_go_toc_cb) },

    { "AddBookmark", NULL,
      N_("_Add Bookmark"),
      "<Control>D",
      NULL,
      G_CALLBACK (window_add_bookmark_cb) },
    { "EditBookmarks", NULL,
      N_("_Edit Bookmarks..."),
      "<Control>B",
      NULL,
      G_CALLBACK (yelp_bookmarks_edit) },

    { "OpenLink", NULL,
      N_("_Open Link"),
      NULL,
      NULL,
      G_CALLBACK (window_open_link_cb) },
    { "OpenLinkNewWindow", NULL,
      N_("Open Link in _New Window"),
      NULL,
      NULL,
      G_CALLBACK (window_open_link_new_cb) },
    { "CopyLocation", NULL,
      N_("_Copy Link Address"),
      NULL,
      NULL,
      G_CALLBACK (window_copy_link_cb) },
    { "Contents", GTK_STOCK_HELP,
      N_("_Contents"),
      "F1",
      N_("Help On this application"),
      G_CALLBACK (window_help_contents_cb) },
    { "About", GTK_STOCK_ABOUT,
      N_("_About"),
      NULL,
      NULL,
      G_CALLBACK (window_about_cb) },
    { "CopyMail", NULL,
      N_("Copy _Email Address"),
      NULL,
      NULL,
      G_CALLBACK (window_copy_mail_cb) }
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
    gint width, height;

    window->priv = YELP_WINDOW_GET_PRIVATE (window);

    GKeyFile *keyfile = g_key_file_new();
    GError *config_error = NULL;
    gchar* config_path = g_strconcat( g_get_home_dir(), YELP_CONFIG_PATH, NULL);

    if( !g_key_file_load_from_file (keyfile, config_path,
			    G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
			    &config_error)) {
	g_warning ("Failed to load config file: %s\n", config_error->message);
	g_error_free (config_error);

	width = YELP_CONFIG_WIDTH_DEFAULT;
	height = YELP_CONFIG_HEIGHT_DEFAULT;
    } else {
	width = g_key_file_get_integer (keyfile, YELP_CONFIG_GEOMETRY_GROUP,
			YELP_CONFIG_WIDTH, NULL);
	height = g_key_file_get_integer (keyfile, YELP_CONFIG_GEOMETRY_GROUP,
			YELP_CONFIG_HEIGHT, NULL);

	if (width == 0)
	    width = YELP_CONFIG_WIDTH_DEFAULT;
	if (height == 0)
	    height = YELP_CONFIG_HEIGHT_DEFAULT;
    }

    g_free (config_path);
    g_key_file_free (keyfile);

    gtk_window_set_default_size (GTK_WINDOW (window), width, height);
    g_signal_connect (window,
		      "configure-event",
		      G_CALLBACK (window_configure_cb),
		      NULL);

    gtk_window_set_title (GTK_WINDOW (window), _("Help Browser"));

    window_populate (window);
}

static void
window_dispose (GObject *object)
{
    parent_class->dispose (object);
}

static void
window_finalize (GObject *object)
{
    YelpWindow *window = YELP_WINDOW (object);
    YelpWindowPriv *priv = window->priv;

    g_object_unref (priv->action_group);
    g_object_unref (priv->ui_manager);

    g_free (priv->find_string);

    g_free (priv->current_frag);

    g_free (priv->uri);
    g_free (priv->base_uri);
    g_free (priv->req_uri);

    /* FIXME there are many more things to free */

    parent_class->finalize (object);
}

static void
window_class_init (YelpWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

    object_class->dispose = window_dispose;
    object_class->finalize = window_finalize;

    signals[NEW_WINDOW_REQUESTED] =
	g_signal_new ("new_window_requested",
		      G_TYPE_FROM_CLASS (klass),
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (YelpWindowClass,
				       new_window_requested),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__STRING,
		      G_TYPE_NONE, 1, G_TYPE_STRING);

    g_type_class_add_private (klass, sizeof (YelpWindowPriv));
}

const gchar *
yelp_window_get_uri (YelpWindow *window)
{

    return ((const gchar *) window->priv->uri);
}

/** History Functions *********************************************************/

static void
history_push_back (YelpWindow *window)
{
    YelpWindowPriv   *priv;
    YelpHistoryEntry *entry;
    GtkAction        *action;
    gchar            *title;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

    entry = g_new0 (YelpHistoryEntry, 1);

    entry->frag_id  = g_strdup (priv->current_frag);
    entry->uri = g_strdup (priv->uri);
    entry->req_uri = g_strdup(priv->req_uri);
    entry->doc = priv->current_document;
    entry->type = priv->current_type;
    entry->base_uri = g_strdup (priv->base_uri);

    /* page_title, frag_title */

    priv->history_back = g_slist_prepend (priv->history_back, entry);

    action = gtk_action_group_get_action (priv->action_group, "GoBack");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", TRUE, NULL);
    title = (gchar *) gtk_window_get_title (GTK_WINDOW (window));

    if (g_str_equal (title, _("Loading...")))
	entry->page_title = g_strdup (_("Unknown Page"));
    else
	entry->page_title = g_strdup (title);

    entry->window = window;

    entry->menu_entry = gtk_menu_item_new_with_label (entry->page_title);
    g_object_ref (entry->menu_entry);

    entry->callback = g_signal_connect (G_OBJECT (entry->menu_entry),
		      "activate",
		      G_CALLBACK (history_back_to),
		      entry);


    gtk_menu_shell_prepend (GTK_MENU_SHELL (window->priv->back_menu),
			    entry->menu_entry);
    gtk_widget_show (entry->menu_entry);
}

static void
history_push_forward (YelpWindow *window)
{
    YelpWindowPriv   *priv;
    YelpHistoryEntry *entry;
    GtkAction        *action;
    gchar            *title;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

    entry = g_new0 (YelpHistoryEntry, 1);

    entry->frag_id  = g_strdup (priv->current_frag);
    entry->uri = g_strdup (priv->uri);
    entry->req_uri = g_strdup(priv->req_uri);
    entry->doc = priv->current_document;
    entry->type = priv->current_type;
    entry->base_uri = g_strdup (priv->base_uri);

   /* page_title, frag_title */

    priv->history_forward = g_slist_prepend (priv->history_forward, entry);

    action = gtk_action_group_get_action (priv->action_group, "GoForward");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", TRUE, NULL);

    title = (gchar *) gtk_window_get_title (GTK_WINDOW (window));

    if (g_str_equal (title, _("Loading...")))
	entry->page_title = g_strdup (_("Unknown Page"));
    else
	entry->page_title = g_strdup (title);

    entry->window = window;
    entry->menu_entry = gtk_menu_item_new_with_label (entry->page_title);
    g_object_ref (entry->menu_entry);

    entry->callback = g_signal_connect (G_OBJECT (entry->menu_entry),
		      "activate",
		      G_CALLBACK (history_forward_to),
		      entry);


    gtk_menu_shell_prepend (GTK_MENU_SHELL (window->priv->forward_menu),
			    entry->menu_entry);
    gtk_widget_show (entry->menu_entry);
}

static void
history_clear_forward (YelpWindow *window)
{
    YelpWindowPriv *priv;
    GtkAction      *action;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

    if (priv->history_forward) {
	g_slist_foreach (priv->history_forward,
			 (GFunc) history_entry_free,
			 NULL);
	g_slist_free (priv->history_forward);
	priv->history_forward = NULL;
    }
    action = gtk_action_group_get_action (priv->action_group, "GoForward");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
}

static void
history_step_back (YelpWindow *window)
{
    YelpWindowPriv   *priv;
    YelpHistoryEntry *entry;

    /** FIXME: would be nice to re-select the correct entry on the left **/

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;
    entry = history_pop_back (window);

    g_free (priv->current_frag);
    priv->current_frag = NULL;

    if (entry) {
	priv->current_frag = g_strdup (entry->frag_id);
	history_entry_free (entry);
    } else {
	yelp_window_load (window, "x-yelp-toc:");
    }
}

static YelpHistoryEntry *
history_pop_back (YelpWindow *window)
{
    YelpWindowPriv   *priv;
    YelpHistoryEntry *entry;
    GtkAction        *action;

    g_return_val_if_fail (YELP_IS_WINDOW (window), NULL);
    if (!window->priv->history_back)
	return NULL;

    priv = window->priv;

    entry = (YelpHistoryEntry *) priv->history_back->data;
    priv->history_back = g_slist_delete_link (priv->history_back,
					      priv->history_back);

    action = gtk_action_group_get_action (priv->action_group, "GoBack");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive",
		      priv->history_back ? TRUE : FALSE,
		      NULL);
    return entry;
}

static YelpHistoryEntry *
history_pop_forward (YelpWindow *window)
{
    YelpWindowPriv   *priv;
    YelpHistoryEntry *entry;
    GtkAction        *action;

    g_return_val_if_fail (YELP_IS_WINDOW (window), NULL);
    if (!window->priv->history_forward)
	return NULL;

    priv = window->priv;

    entry = (YelpHistoryEntry *) priv->history_forward->data;
    priv->history_forward = g_slist_delete_link (priv->history_forward,
						 priv->history_forward);

    action = gtk_action_group_get_action (priv->action_group, "GoForward");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive",
		      priv->history_forward ? TRUE : FALSE,
		      NULL);
    return entry;
}

static void
history_entry_free (YelpHistoryEntry *entry)
{
    g_return_if_fail (entry != NULL);

    g_free (entry->frag_id);
    g_free (entry->page_title);
    g_free (entry->frag_title);
    g_free (entry->base_uri);
    g_free (entry->uri);
    g_free (entry->req_uri);

    if (entry->menu_entry) {
	gtk_widget_destroy (entry->menu_entry);
    }

    g_free (entry);
}

static void
history_back_to (GtkMenuItem *menuitem, YelpHistoryEntry *entry)
{
    YelpWindow       *window;
    YelpWindowPriv   *priv;
    YelpHistoryEntry *latest;

    window = entry->window;

    priv = window->priv;

    latest = history_pop_back (window);
    history_push_forward (window);

    while (!g_str_equal (latest->page_title, entry->page_title)) {
	priv->history_forward = g_slist_prepend (priv->history_forward, latest);
	gtk_container_remove (GTK_CONTAINER (priv->back_menu),
			      latest->menu_entry);

	g_signal_handler_disconnect (latest->menu_entry, latest->callback);
	latest->callback = g_signal_connect (G_OBJECT (latest->menu_entry),
					     "activate",
					     G_CALLBACK (history_forward_to),
					     latest);


	gtk_menu_shell_prepend (GTK_MENU_SHELL (window->priv->forward_menu),
				latest->menu_entry);
	gtk_widget_show (latest->menu_entry);


	latest = history_pop_back (window);
    }
    if (latest->menu_entry) {
	gtk_widget_destroy (latest->menu_entry);
	latest->menu_entry = NULL;
    }

    history_load_entry (window, entry);
    history_entry_free (entry);
}

static void
history_forward_to (GtkMenuItem *menuitem, YelpHistoryEntry *entry)
{
    YelpWindow       *window;
    YelpWindowPriv   *priv;
    YelpHistoryEntry *latest;

    window = entry->window;

    priv = window->priv;

    latest = history_pop_forward (window);
    history_push_back (window);

    while (!g_str_equal (latest->page_title, entry->page_title)) {
	priv->history_back = g_slist_prepend (priv->history_back, latest);
	gtk_container_remove (GTK_CONTAINER (priv->forward_menu),
			      latest->menu_entry);

	g_signal_handler_disconnect (latest->menu_entry, latest->callback);
	latest->callback = g_signal_connect (G_OBJECT (latest->menu_entry),
					     "activate",
					     G_CALLBACK (history_back_to),
					     latest);

	gtk_menu_shell_prepend (GTK_MENU_SHELL (window->priv->back_menu),
				latest->menu_entry);
	gtk_widget_show (latest->menu_entry);


	latest = history_pop_forward (window);
    }
    if (latest->menu_entry) {
	gtk_widget_destroy (latest->menu_entry);
	latest->menu_entry = NULL;
    }

    history_load_entry (window, entry);
    history_entry_free (entry);


}

/******************************************************************************/


GtkWidget *
yelp_window_new (GNode *doc_tree, GList *index)
{
    return GTK_WIDGET (g_object_new (YELP_TYPE_WINDOW, NULL));
}

static void
page_request_cb (YelpDocument       *document,
	       YelpDocumentSignal  signal,
	       gint                req_id,
	       gpointer           *func_data,
	       YelpWindow         *window)
{
    YelpError *error;
    YelpLoadData *data = NULL;

    switch (signal) {
    case YELP_DOCUMENT_SIGNAL_PAGE:
	window_set_sections (window, yelp_document_get_sections (document));

	data = g_new0 (YelpLoadData, 1);
	data->window = window;
	data->page = (YelpPage *) func_data;

	window_write_html (data);

	window->priv->current_request = -1;
	yelp_page_free ((YelpPage *) func_data);
	gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)),
	                       NULL);
	g_free (data);
	break;
    case YELP_DOCUMENT_SIGNAL_TITLE:
	/* We don't need to actually handle title signals as gecko
	 * is wise enough to not annoy me by not handling it
	 */
	g_free (func_data);
	break;
    case YELP_DOCUMENT_SIGNAL_ERROR:
	error = (YelpError *) func_data;
	window_error (window, (gchar *) yelp_error_get_title (error),
		      (gchar *) yelp_error_get_message (error), FALSE);
	yelp_error_free (error);
	gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)),
			       NULL);
	break;
    default:
	g_assert_not_reached();
    }
}

static void
window_setup_window (YelpWindow *window, YelpRrnType type,
		     gchar *loading_uri, gchar *frag, gchar *req_uri,
		     gchar *base_uri, gboolean add_history)
{
    /* Before asking the YelpDocument to find
     * a page, this should be called to set up various
     * things (such as fixing history, setting
     * menu items to sensitive etc.)
     * These are all read from the YelpWindow struct
     * so they must be set BEFORE calling this.
     */
    YelpWindowPriv *priv;
    GtkAction      *action;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

    if (priv->current_request != -1) {
	yelp_document_cancel_page (priv->current_document, priv->current_request);
	priv->current_request = -1;
    } else if (add_history) {
	gchar *tmp = window->priv->base_uri;
	window->priv->base_uri = base_uri;
	history_push_back(window);
	window->priv->base_uri = tmp;
    }


    window_set_loading (window);

    priv->current_type = type;
    g_free (priv->uri);
    priv->uri = g_strdup (loading_uri);
    g_free (priv->current_frag);
    priv->current_frag = g_strdup (frag);
    g_free (priv->req_uri);
    priv->req_uri = g_strdup (req_uri);

    switch (priv->current_type) {
    case YELP_RRN_TYPE_DOC:
	action = gtk_action_group_get_action (window->priv->action_group,
					      "PrintDocument");
	g_object_set (G_OBJECT (action), "sensitive", TRUE, NULL);

	action  = gtk_action_group_get_action (window->priv->action_group,
					       "AboutDocument");
	g_object_set (G_OBJECT (action),  "sensitive", TRUE, NULL);
	break;
    default:
	action = gtk_action_group_get_action (window->priv->action_group,
					      "PrintDocument");
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);

	action  = gtk_action_group_get_action (window->priv->action_group,
					       "AboutDocument");
	g_object_set (G_OBJECT (action),  "sensitive", FALSE, NULL);
	break;

    }
}


void
yelp_window_load (YelpWindow *window, const gchar *uri)
{
    YelpWindowPriv *priv;
    gchar          *frag_id = NULL;
    gchar          *real_uri = NULL;
    gchar          *trace_uri = NULL;
    YelpRrnType  type = YELP_RRN_TYPE_ERROR;
    YelpDocument   *doc = NULL;
    gchar *current_base = NULL;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

    current_base = g_strdup (priv->base_uri);

    /* If someone asks for info:dir, they WILL get redirected to
     * our index.  Tough.
     */
    if (g_str_has_prefix (uri, "info:") && g_str_has_suffix (uri, "dir")) {
	trace_uri = g_strdup ("x-yelp-toc:#Info");
    } else {
	trace_uri = g_strdup (uri);
    }

    /* The way this route was taken, we need to clear the
     * forward history now
     */
    history_clear_forward (window);

    type = yelp_uri_resolve (trace_uri, &real_uri, &frag_id);

    if (type == YELP_RRN_TYPE_ERROR) {
	gchar *message = g_strdup_printf (_("The requested URI \"%s\" is invalid"), trace_uri);
	window_error (window, _("Unable to load page"), message, FALSE);

	goto Exit;
    }

    if (priv->uri && g_str_equal (real_uri, priv->uri)) {
	doc = priv->current_document;
    } else {
	gchar *old_base_uri = priv->base_uri;
	priv->base_uri = NULL;

	switch (type) {
	case YELP_RRN_TYPE_TOC:
	    doc = yelp_toc_get ();
	    priv->base_uri = g_strdup ("file:///fakefile");
	    break;
        case YELP_RRN_TYPE_MAL:
            priv->base_uri = g_filename_to_uri (real_uri, NULL, NULL);
            doc = yelp_mallard_new (real_uri);
            break;
	case YELP_RRN_TYPE_MAN:
	    priv->base_uri = g_filename_to_uri (real_uri, NULL, NULL);
	    doc = yelp_man_new (real_uri);
	    break;
	case YELP_RRN_TYPE_INFO:
	    priv->base_uri = g_filename_to_uri (real_uri, NULL, NULL);
	    if (!frag_id) {
		frag_id = g_strdup ("Top");
	    } else {
		g_strdelimit (frag_id, " ", '_');
	    }
	    doc = yelp_info_new (real_uri);
	    break;
	case YELP_RRN_TYPE_DOC:
	    priv->base_uri = g_filename_to_uri (real_uri, NULL, NULL);
	    doc = yelp_docbook_new (real_uri);
	    break;
	case YELP_RRN_TYPE_SEARCH:
	    doc = yelp_search_new (&real_uri[14]); /* to remove x-yelp-search:*/
	    priv->base_uri = g_strdup ("file:///fakefile");

	    break;
	case YELP_RRN_TYPE_HTML:
	case YELP_RRN_TYPE_XHTML:
	case YELP_RRN_TYPE_TEXT:
 	    {
		gchar *call_uri;
		priv->base_uri = g_strdup ("file:///fakefile");
		call_uri = g_filename_to_uri (real_uri, NULL, NULL);
		window_do_load_html (window, call_uri, frag_id, type, TRUE);
		g_free (call_uri);
 	    }
	    break;
	case YELP_RRN_TYPE_EXTERNAL:
	    {
		GError *error = NULL;

		priv->base_uri = old_base_uri;

		/* disallow external links when running in the GDM greeter session */
		if (g_getenv ("RUNNING_UNDER_GDM") != NULL) {
		    gchar *message = g_strdup_printf (_("The requested URI \"%s\" is invalid"), trace_uri);

		    window_error (window, _("Unable to load page"), message, FALSE);
		    g_free (message);

		    goto Exit;
		}

		if (!gtk_show_uri (NULL, trace_uri, gtk_get_current_event_time (), &error)) {
		    gchar *message = g_strdup_printf (_("The requested URI \"%s\" is invalid"), trace_uri);
		    window_error (window, _("Unable to load page"), message, FALSE);
		    /* todo: Freeing error here causes an immediate 
		     * seg fault.  Why?  Who knows. For now, we'll
		     * leak it
		     */
		    /* g_free (error); */
		    error = NULL;
		    if (!priv->current_document) {
			/* recurse to load the index if we're not
			 * already somewhere
			 */
			yelp_window_load (window, "x-yelp-toc:");
		    }
		    goto Exit;
		}
	    }
	    break;

	default:
	    priv->base_uri = old_base_uri;
	    break;
	}
	if (old_base_uri != priv->base_uri)
	    g_free (old_base_uri);
    }

    if (doc) {
        gchar *faux_frag_id, *slash;
	gboolean need_hist = FALSE;
	if (!frag_id)
	  frag_id = g_strdup ("x-yelp-index");

	if (priv->current_document || (priv->current_type == YELP_RRN_TYPE_HTML ||
				       priv->current_type == YELP_RRN_TYPE_XHTML ||
				       priv->current_type == YELP_RRN_TYPE_TEXT))
	    need_hist = TRUE;

        /* FIXME: Super hacky.  We want the part before the slash for mallard IDs,
           because right now we're outputting "#page_id/section_id".
         */
        faux_frag_id = g_strdup (frag_id);
        if ( priv->current_type == YELP_RRN_TYPE_MAL )
        {
        slash = strchr (faux_frag_id, '/');
        if (slash)
            *slash = '\0';
        if (strlen (faux_frag_id) == 0) {
            gchar *new_frag_id, *newslash;
            if (slash)
                slash = g_strdup (slash + 1);
            g_free (faux_frag_id);
            faux_frag_id = g_strdup (priv->current_frag);
            newslash = strchr (faux_frag_id, '/');
            if (newslash)
                *newslash = '\0';
            new_frag_id = g_strconcat (faux_frag_id, "/", slash, NULL);
            g_free (frag_id);
            g_free (slash);
            frag_id = new_frag_id;
        }
        }
	window_setup_window (window, type, real_uri, frag_id,
			     (gchar *) uri, current_base, need_hist);
        priv->current_request = yelp_document_get_page (doc,
                                                        faux_frag_id,
                                                        (YelpDocumentFunc) page_request_cb,
                                                        (void *) window);
        g_free (faux_frag_id);
	priv->current_document = doc;
    }

 Exit:
    g_free (frag_id);
    g_free(real_uri);
    g_free(trace_uri);
    g_free(current_base);
}

GtkUIManager *
yelp_window_get_ui_manager (YelpWindow *window)
{
    g_return_val_if_fail (YELP_IS_WINDOW (window), NULL);

    return window->priv->ui_manager;
}

/******************************************************************************/

static void
window_error (YelpWindow *window, gchar *title, gchar *message, gboolean pop)
{
    YelpWindowPriv *priv;
    GtkWidget *dialog;
    GtkAction *action;

    priv = window->priv;

    if (pop)
	history_step_back (window);

    action = gtk_action_group_get_action (priv->action_group, "GoPrevious");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
    action = gtk_action_group_get_action (priv->action_group, "GoNext");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
    action = gtk_action_group_get_action (priv->action_group, "GoContents");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);

    dialog = gtk_message_dialog_new
	(GTK_WINDOW (window),
	 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	 GTK_MESSAGE_ERROR,
	 GTK_BUTTONS_OK,
	 "%s", title);
    gtk_message_dialog_format_secondary_markup
	(GTK_MESSAGE_DIALOG (dialog), "%s", message);
	 gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);
}

static char *
encode_search_uri (const char *search_terms)
{
    return g_strdup_printf ("x-yelp-search:%s", search_terms);
}

static void
search_activated (GtkAction *action,
                 YelpWindow *window)
{
    const char *search_terms;
    char *uri;

    search_terms = gtk_entry_action_get_text (GTK_ENTRY_ACTION (action));

    /* Do some basic checking here - if someones looking for a man
     * or info page, save doing the search and just bring them directly to
     * the relevant page
     * Trigger it using "man:<foo>", "man <foo>" or "info:<foo>"
     */
    if (g_str_has_prefix (search_terms, "man:") ||
	g_str_has_prefix (search_terms, "info:")) {
	uri = g_strdup (search_terms);
    } else if (g_str_has_prefix (search_terms, "man ") &&
	       !strstr (&(search_terms[4])," ")) {
	uri = g_strdup (search_terms);
	uri[3]=':';
    } else if (g_str_has_prefix (search_terms, "info ")) {
	gint count = 0;
	gchar *spaces;

	spaces = strchr (search_terms, ' ');
	while (spaces) {
	    count++;
	    spaces = strchr (spaces+1, ' ');
	}
	if (count == 1) {
	    uri = g_strdup (search_terms);
	    uri[4] = ':';
	} else {
	    uri = encode_search_uri (search_terms);
	}
    } else {
	uri = encode_search_uri (search_terms);
    }
    yelp_window_load (window, uri);

    g_free (uri);
}


static void
window_populate (YelpWindow *window)
{
    YelpWindowPriv    *priv;
    GtkAccelGroup     *accel_group;
    GError            *error = NULL;
    GtkAction         *action;
    GtkTreeSelection  *selection;
    GtkWidget         *toolbar;
    GtkWidget         *toolitem;
    GtkWidget         *b_proxy;
    GtkWidget         *f_proxy;


    priv = window->priv;

    priv->current_request = -1;

    priv->main_box = gtk_vbox_new (FALSE, 0);

    gtk_container_add (GTK_CONTAINER (window), priv->main_box);

    priv->action_group = gtk_action_group_new ("MenuActions");
    gtk_action_group_set_translation_domain (priv->action_group,
					     GETTEXT_PACKAGE);
    gtk_action_group_add_actions (priv->action_group,
				  entries, G_N_ELEMENTS (entries),
				  window);

    b_proxy = GTK_WIDGET (gtk_menu_tool_button_new (NULL, NULL));

    priv->back_menu = gtk_menu_new ();

    priv->forward_menu = gtk_menu_new ();

    f_proxy = GTK_WIDGET (gtk_menu_tool_button_new (NULL, NULL));

    gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (b_proxy),
				   priv->back_menu);

    action = gtk_action_group_get_action(priv->action_group, "GoBack");
    gtk_activatable_set_related_action (GTK_ACTIVATABLE (b_proxy), action);

    action = gtk_action_group_get_action (priv->action_group, "GoForward");
    gtk_activatable_set_related_action (GTK_ACTIVATABLE (f_proxy), action);

    gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (f_proxy),
				   priv->forward_menu);

    priv->search_action =  (GtkWidget * )gtk_entry_action_new ("Search",
				    _("_Search:"),
				    _("Search for other documentation"),
				    NULL);
    g_signal_connect (G_OBJECT (priv->search_action), "activate",
		      G_CALLBACK (search_activated), window);
    gtk_action_group_add_action (priv->action_group,
				 (GtkAction *) priv->search_action);

    priv->ui_manager = gtk_ui_manager_new ();
    gtk_ui_manager_insert_action_group (priv->ui_manager, priv->action_group, 0);

    accel_group = gtk_ui_manager_get_accel_group (priv->ui_manager);
    gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);


    g_signal_connect (G_OBJECT (priv->ui_manager), "add-widget",
		      G_CALLBACK (window_add_widget), priv->main_box);

    if (!gtk_ui_manager_add_ui_from_file (priv->ui_manager,
					  DATADIR "/yelp/ui/yelp-ui.xml",
					  &error)) {
	window_error (window, _("Cannot create window"), error->message, FALSE);
    }

    if (!gtk_ui_manager_add_ui_from_file (priv->ui_manager,
					  DATADIR "/yelp/ui/yelp-search-ui.xml",
					  &error)) {
	window_error (window, _("Cannot create search component"), error->message, FALSE);
    }

    yelp_bookmarks_register (window);

    action = gtk_action_group_get_action (priv->action_group, "GoBack");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
    action = gtk_action_group_get_action (priv->action_group, "GoForward");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);

    gtk_ui_manager_ensure_update (priv->ui_manager);

    toolbar = gtk_ui_manager_get_widget(priv->ui_manager, "ui/tools");

    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (f_proxy), 0);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (b_proxy), 0);

    /*To keep the seperator but get rid of the extra widget*/
    toolitem = gtk_ui_manager_get_widget(priv->ui_manager, "ui/tools/GoFor");

    gtk_container_remove (GTK_CONTAINER (toolbar), toolitem);

    priv->popup = gtk_ui_manager_get_widget(priv->ui_manager, "ui/main_popup");
    priv->maillink = gtk_ui_manager_get_widget(priv->ui_manager, "ui/mail_popup");
    priv->merge_id = gtk_ui_manager_new_merge_id (priv->ui_manager);

    priv->find_next_menu = gtk_action_group_get_action (priv->action_group,
							"FindNext");
    priv->find_prev_menu = gtk_action_group_get_action (priv->action_group,
						      "FindPrev");

    priv->pane = gtk_hpaned_new ();
    /* We should probably remember the last position and set to that. */
    gtk_paned_set_position (GTK_PANED (priv->pane), 180);

    priv->side_sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->side_sw),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (priv->side_sw),
					 GTK_SHADOW_IN);

    priv->side_sects = gtk_tree_view_new ();
    g_object_set (priv->side_sects,
		  "headers-visible",   FALSE,
		  NULL);
    gtk_tree_view_insert_column_with_attributes
	(GTK_TREE_VIEW (priv->side_sects), -1,
	 NULL, gtk_cell_renderer_text_new (),
	 "text", YELP_DOCUMENT_COLUMN_TITLE,
	 NULL);

    /* DISABLE FOR NOW
    gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (priv->side_sects),
					    GDK_BUTTON1_MASK,
					    row_targets,
					    G_N_ELEMENTS (row_targets),
					    GDK_ACTION_LINK);
    */

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->side_sects));

    g_signal_connect (selection, "changed",
		      G_CALLBACK (tree_selection_changed_cb),
		      window);
    g_signal_connect (priv->side_sects, "row-activated",
		      G_CALLBACK (tree_row_expand_cb),
		      window);
    g_signal_connect (priv->side_sects, "drag_data_get",
		      G_CALLBACK (tree_drag_data_get_cb),
		      window);

    gtk_container_add (GTK_CONTAINER (priv->side_sw),     priv->side_sects);
    gtk_paned_add1    (GTK_PANED (priv->pane), priv->side_sw);

    priv->html_pane = gtk_vbox_new (FALSE, 0);

    priv->find_bar = gtk_toolbar_new ();
    gtk_toolbar_set_style (GTK_TOOLBAR (priv->find_bar), GTK_TOOLBAR_BOTH_HORIZ);
    gtk_toolbar_set_icon_size (GTK_TOOLBAR (priv->find_bar), GTK_ICON_SIZE_MENU);
    window_populate_find (window, priv->find_bar);
    gtk_box_pack_end (GTK_BOX (priv->html_pane),
		      priv->find_bar,
		      FALSE, FALSE, 0);

    priv->html_view  = yelp_html_new ();
    g_signal_connect (priv->html_view,
		      "uri_selected",
		      G_CALLBACK (html_uri_selected_cb),
		      window);
    g_signal_connect (priv->html_view,
		      "frame_selected",
		      G_CALLBACK (html_frame_selected_cb),
		      window);
    g_signal_connect (priv->html_view,
		      "title_changed",
		      G_CALLBACK (html_title_changed_cb),
		      window);
    g_signal_connect (priv->html_view,
		      "popupmenu_requested",
		      G_CALLBACK (html_popupmenu_requested_cb),
		      window);
    /* Connect to look for /'s */
    g_signal_connect (window,
		      "key-press-event",
		      G_CALLBACK (window_key_event_cb),
		      window);
    gtk_box_pack_end (GTK_BOX (priv->html_pane),
		      GTK_WIDGET (priv->html_view),
		      TRUE, TRUE, 0);

    gtk_paned_add2     (GTK_PANED (priv->pane),
			priv->html_pane);
    gtk_box_pack_start (GTK_BOX (priv->main_box),
			priv->pane,
			TRUE, TRUE, 0);

    gtk_widget_show (GTK_WIDGET (priv->html_view));
    gtk_widget_show (priv->html_pane);
    gtk_widget_show (priv->pane);
    gtk_widget_show (priv->main_box);
    gtk_container_set_focus_child (GTK_CONTAINER (window),
				   GTK_WIDGET (priv->html_view));
}

static gboolean
window_key_event_cb (GtkWidget *widget, GdkEventKey *event,
		     YelpWindow *window)
{
    if ((window->priv->search_action &&
	 gtk_entry_action_has_focus ((GtkEntryAction *) window->priv->search_action)) ||
	gtk_widget_has_focus (window->priv->find_entry))
	return FALSE;

    if (event->keyval == GDK_slash) {
	window_find_cb (NULL, window);
	return TRUE;
    }

    return FALSE;
}

static gboolean
window_find_hide_cb (GtkWidget *widget, GdkEventFocus *event,
		     YelpWindow *window)
{
    YelpWindowPriv *priv;

    g_return_val_if_fail (YELP_IS_WINDOW (window), FALSE);

    priv = window->priv;

    gtk_widget_hide ((GtkWidget *) priv->find_bar);
    return FALSE;
}


static void
window_populate_find (YelpWindow *window,
		      GtkWidget *find_bar)
{
    GtkWidget *box;
    GtkWidget *label;
    GtkToolItem *item;
    GtkWidget *arrow;
    YelpWindowPriv *priv = window->priv;

    g_return_if_fail (GTK_IS_TOOLBAR (find_bar));

    box = gtk_hbox_new (FALSE, 0);
    label = gtk_label_new_with_mnemonic (_("Fin_d:"));
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 6);

    priv->find_entry = gtk_entry_new ();
    g_signal_connect (G_OBJECT (priv->find_entry), "changed",
		      G_CALLBACK (window_find_entry_changed_cb), window);
    g_signal_connect (G_OBJECT (priv->find_entry), "activate",
		      G_CALLBACK (window_find_entry_activate_cb), window);
    g_signal_connect (G_OBJECT (priv->find_entry), "key-press-event",
		      G_CALLBACK (window_find_entry_key_pressed_cb), window);
    g_signal_connect (G_OBJECT (priv->find_entry), "focus-out-event",
		      G_CALLBACK (window_find_hide_cb), window);
    gtk_box_pack_start (GTK_BOX (box), priv->find_entry, TRUE, TRUE, 0);

    item = gtk_tool_item_new ();
    gtk_container_add (GTK_CONTAINER (item), box);
    gtk_toolbar_insert (GTK_TOOLBAR (find_bar), item, -1);

    gtk_label_set_mnemonic_widget (GTK_LABEL (label), priv->find_entry);

    box = gtk_hbox_new (FALSE, 0);
    arrow = gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_NONE);
    label = gtk_label_new_with_mnemonic (_("Find _Previous"));
    gtk_box_pack_start (GTK_BOX (box), arrow, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
    priv->find_prev = gtk_tool_button_new (box, NULL);
    g_signal_connect (priv->find_prev,
                      "clicked",
                      G_CALLBACK (window_find_clicked_cb),
                      window);
    gtk_toolbar_insert (GTK_TOOLBAR (find_bar), priv->find_prev, -1);

    box = gtk_hbox_new (FALSE, 0);
    arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
    label = gtk_label_new_with_mnemonic (_("Find _Next"));
    gtk_box_pack_start (GTK_BOX (box), arrow, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
    priv->find_next = gtk_tool_button_new (box, NULL);
    g_signal_connect (priv->find_next,
		      "clicked",
		      G_CALLBACK (window_find_clicked_cb),
		      window);
    gtk_toolbar_insert (GTK_TOOLBAR (find_bar), priv->find_next, -1);

    priv->find_sep = gtk_separator_tool_item_new ();
    gtk_toolbar_insert (GTK_TOOLBAR (find_bar), priv->find_sep, -1);

    label = gtk_label_new (_("Phrase not found"));
    priv->find_not_found = gtk_tool_item_new ();

    gtk_tool_item_set_expand (priv->find_not_found, TRUE);
    gtk_container_add (GTK_CONTAINER (priv->find_not_found), label);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_toolbar_insert (GTK_TOOLBAR (find_bar), priv->find_not_found, -1);

    item = gtk_separator_tool_item_new ();
    gtk_tool_item_set_expand (item, TRUE);
    gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (item), FALSE);
    gtk_toolbar_insert (GTK_TOOLBAR (find_bar), item, -1);

    item = gtk_tool_button_new_from_stock (GTK_STOCK_CLOSE);
    gtk_tool_item_set_is_important (item, FALSE);
    g_signal_connect_swapped (item,
			      "clicked",
			      G_CALLBACK (gtk_widget_hide),
			      priv->find_bar);
    gtk_toolbar_insert (GTK_TOOLBAR (find_bar), item, -1);
}

static void
window_set_sections (YelpWindow   *window,
		     GtkTreeModel *sections)
{
    YelpWindowPriv *priv;

    g_return_if_fail (YELP_IS_WINDOW (window));
    priv = window->priv;

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->side_sects), sections);

	if (sections) {
	    gtk_widget_show_all (priv->side_sw);
	    window_set_section_cursor (window, sections);
	} else
	    gtk_widget_hide (priv->side_sw);

}

static void
window_set_section_cursor (YelpWindow * window, GtkTreeModel *model)
{
    gboolean    valid;
    gchar *id = NULL;
    GtkTreeIter     iter;
    YelpWindowPriv *priv = window->priv;
    GtkTreeSelection *selection =
	gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->side_sects));
    g_signal_handlers_block_by_func (selection,
				     tree_selection_changed_cb,
				     window);
    gtk_tree_selection_unselect_all (selection);

    valid = gtk_tree_model_get_iter_first (model, &iter);
    while (valid) {
	gtk_tree_model_get (model, &iter,
			    YELP_DOCUMENT_COLUMN_ID, &id,
			    -1);
	if (g_str_equal (id, priv->current_frag)) {
	    GtkTreePath *path = NULL;
	    GtkTreeIter parent;
	    if (gtk_tree_model_iter_parent (model, &parent, &iter)) {
		path = gtk_tree_model_get_path (model, &parent);
		gtk_tree_view_expand_to_path (GTK_TREE_VIEW (priv->side_sects),
					      path);
		gtk_tree_path_free(path);
	    }
	    path = gtk_tree_model_get_path (model, &iter);
	    gtk_tree_selection_select_path (selection, path);

	    gtk_tree_path_free (path);
	    g_free (id);
	    break;
	}

	g_free (id);

	valid = tree_model_iter_following (model, &iter);
    }
    g_signal_handlers_unblock_by_func (selection,
				       tree_selection_changed_cb,
				       window);
}

static gboolean
window_do_load_html (YelpWindow    *window,
		     gchar         *uri,
		     gchar         *frag_id,
		     YelpRrnType    type,
		     gboolean       need_history)
{
    YelpWindowPriv   *priv;
    GFile            *file;
    GFileInputStream *stream;
    gsize             n;
    gchar             buffer[BUFFER_SIZE];
    GtkAction        *action;
    gchar *real_uri = NULL;

    gboolean  handled = TRUE;

    g_return_val_if_fail (YELP_IS_WINDOW (window), FALSE);

    priv = window->priv;

    window_set_sections (window, NULL);

    action = gtk_action_group_get_action (priv->action_group, "GoPrevious");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
    action = gtk_action_group_get_action (priv->action_group, "GoNext");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
    action = gtk_action_group_get_action (priv->action_group, "GoContents");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);

    window_setup_window (window, type, uri, frag_id, uri, priv->base_uri, need_history);

    if (uri[0] == '/')
	file = g_file_new_for_path (uri);
    else
	file = g_file_new_for_uri (uri);
    stream = g_file_read (file, NULL, NULL);

    if (stream == NULL) {
	gchar *message;

	message = g_strdup_printf (_("The file ‘%s’ could not be read.  This file might "
				     "be missing, or you might not have permissions to "
				     "read it."), uri);
	window_error (window, _("Could Not Read File"), message, TRUE);
	g_free (message);
	handled = FALSE;
	goto done;
    }

    if (frag_id) {
	real_uri = g_strconcat (uri, "#", frag_id, NULL);
    } else {
	real_uri = g_strdup (uri);
    }
    yelp_html_set_base_uri (priv->html_view, real_uri);

    switch (type) {
    case YELP_RRN_TYPE_HTML:
	yelp_html_open_stream (priv->html_view, "text/html");
	break;
    case YELP_RRN_TYPE_XHTML:
	yelp_html_open_stream (priv->html_view, "application/xhtml+xml");
	break;
    case YELP_RRN_TYPE_TEXT:
	yelp_html_open_stream (priv->html_view, "text/plain");
	break;
    default:
	g_assert_not_reached ();
    }

    while ((g_input_stream_read_all
	    ((GInputStream *)stream, buffer, BUFFER_SIZE, &n, NULL, NULL)) && n) {
	gchar *tmp;

	if (n == 0)
		break;

	tmp = g_utf8_strup (buffer, n);
	if (strstr (tmp, "<FRAMESET")) {
	    yelp_html_frames (priv->html_view, TRUE);
	}
	g_free (tmp);

	yelp_html_write (priv->html_view, buffer, n);
    }
    yelp_html_close (priv->html_view);

 done:
    if (file)
        g_object_unref (file);
    if (stream)
        g_object_unref (stream);

    g_free (real_uri);
    gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)),
                           NULL);

    return handled;
}

static void
window_set_loading (YelpWindow *window)
{
    YelpWindowPriv *priv;
    GtkAction *action;
    GdkCursor *cursor;
    gchar *loading = _("Loading...");

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

    cursor = gdk_cursor_new (GDK_WATCH);
    gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)),
                           cursor);
    gdk_cursor_unref (cursor);

    action = gtk_action_group_get_action (priv->action_group, "GoPrevious");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
    action = gtk_action_group_get_action (priv->action_group, "GoNext");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
    action = gtk_action_group_get_action (priv->action_group, "GoContents");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);

    gtk_window_set_title (GTK_WINDOW (window),
			  (const gchar *) loading);

}

/** Window Callbacks **********************************************************/

static gboolean
window_configure_cb (GtkWidget         *widget,
		     GdkEventConfigure *event,
		     gpointer           data)
{
    gint width, height;
    GKeyFile *keyfile;
    GError *config_error = NULL;
    gchar *sdata, *config_path;
    gsize config_size;

    gtk_window_get_size (GTK_WINDOW (widget), &width, &height);

    keyfile = g_key_file_new();

    config_path = g_strconcat (g_get_home_dir(), YELP_CONFIG_PATH, NULL);

    g_key_file_set_integer (keyfile, YELP_CONFIG_GEOMETRY_GROUP,
			    YELP_CONFIG_WIDTH, width);
    g_key_file_set_integer (keyfile, YELP_CONFIG_GEOMETRY_GROUP,
			    YELP_CONFIG_HEIGHT, height);

    sdata = g_key_file_to_data (keyfile, &config_size, NULL);

    if ( !g_file_set_contents (config_path, sdata,
			       config_size, &config_error) ) {
	g_warning ("Failed to save config file: %s\n", config_error->message);
	g_error_free (config_error);
    }

    g_free (sdata);
    g_free (config_path);
    g_key_file_free (keyfile);

    return FALSE;
}

/** Gecko Callbacks ***********************************************************/

static void
html_uri_selected_cb (YelpHtml  *html,
		      gchar     *uri,
		      gboolean   handled,
		      gpointer   user_data)
{
    gchar *new_uri = uri;
    YelpWindow *window = YELP_WINDOW (user_data);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  uri = \"%s\"\n", uri);

    if (g_str_has_prefix (uri, "xref:"))
        new_uri = g_strconcat (window->priv->base_uri, "#", uri + 5, NULL);

    if (!handled) {
	yelp_window_load (window, new_uri);
    }

    if (new_uri != uri)
        g_free (new_uri);
}

static gboolean
html_frame_selected_cb (YelpHtml *html, gchar *uri, gboolean handled,
			gpointer user_data)
{
    YelpWindow *window = YELP_WINDOW (user_data);
    gboolean handle;

    switch (window->priv->current_type) {
    case YELP_RRN_TYPE_XHTML:
    case YELP_RRN_TYPE_HTML:
    case YELP_RRN_TYPE_TEXT:
	handle = TRUE;
	break;
    default:
	handle = FALSE;
	break;
    }
    if (handle) {
	return FALSE;
    } else {
	yelp_window_load (window, uri);
    }
    return TRUE;
}

static void
html_title_changed_cb (YelpHtml  *html,
		       gchar     *title,
		       gpointer   user_data)
{
    gtk_window_set_title (GTK_WINDOW (user_data), title);
}

static void
html_popupmenu_requested_cb (YelpHtml *html,
			     gchar *uri,
			     gpointer user_data)
{
    YelpWindow *window = YELP_WINDOW (user_data);

    gtk_ui_manager_remove_ui (window->priv->ui_manager,
			      window->priv->merge_id);


    if (g_str_has_prefix(uri, "mailto:")) {
	gtk_ui_manager_add_ui (window->priv->ui_manager,
			       window->priv->merge_id,
			       "/ui/main_popup",
			       "CpMail",
			       "CopyMail",
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

    }
    g_free (window->priv->uri);
    window->priv->uri = g_strdup (uri);
    gtk_menu_popup (GTK_MENU (window->priv->popup),
		    NULL, NULL, NULL, NULL, 3, gtk_get_current_event_time());
}

/** GtkTreeView Callbacks *****************************************************/

static void
tree_selection_changed_cb (GtkTreeSelection *selection,
			   YelpWindow       *window)
{
    YelpWindowPriv *priv;
    GtkTreeModel   *model;
    GtkTreeIter     iter;
    gchar *id, *uri;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

    if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->side_sects));
	gtk_tree_model_get (model, &iter,
			    YELP_DOCUMENT_COLUMN_ID, &id,
			    -1);
	uri = g_strdup_printf ("%s#%s", priv->base_uri, id);
	yelp_window_load (window, uri);
	g_free (uri);
    }
}

static void
tree_drag_data_get_cb (GtkWidget         *widget,
		       GdkDragContext    *context,
		       GtkSelectionData  *selection,
		       guint              info,
		       guint32            time,
		       YelpWindow        *window)
{
    /* TODO: This is disabled by default anyway */
#if 0
    YelpWindowPriv   *priv;
    GtkTreeSelection *tree_selection;
    GtkTreeModel     *model;
    GtkTreeIter       iter;
    gchar *id, *uri = NULL;

    g_return_if_fail (YELP_IS_WINDOW (window));

    debug_print (DB_FUNCTION, "entering\n");

    priv = window->priv;

    tree_selection =
	gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->side_sects));

    if (gtk_tree_selection_get_selected (tree_selection, NULL, &iter)) {
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->side_sects));
	gtk_tree_model_get (model, &iter,
			    YELP_DOCUMENT_COLUMN_ID, &id,
			    -1);
	uri = yelp_doc_info_get_uri (priv->current_doc, id,
				     YELP_URI_TYPE_NO_FILE);
	if (!uri)
	    uri = yelp_doc_info_get_uri (priv->current_doc, id,
					 YELP_URI_TYPE_FILE);
    }

    switch (info) {
    case TARGET_URI_LIST:
	gtk_selection_data_set (selection,
				selection->target,
				8, (const guchar *) uri, strlen (uri));
	break;
    default:
	g_assert_not_reached ();
    }

    g_free (uri);
#endif
}

void
tree_row_expand_cb (GtkTreeView *view, GtkTreePath *path,
		    GtkTreeViewColumn *column, YelpWindow *window)
{
	if (gtk_tree_view_row_expanded (view, path)) {
		gtk_tree_view_collapse_row (view, path);
	} else {
		gtk_tree_view_expand_to_path (view, path);
	}

}


/** UIManager Callbacks *******************************************************/

static void
window_add_widget (GtkUIManager *ui_manager,
		   GtkWidget    *widget,
		   GtkWidget    *vbox)
{
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
}

static void
window_new_window_cb (GtkAction *action, YelpWindow *window)
{
    g_return_if_fail (YELP_IS_WINDOW (window));

    g_signal_emit (window, signals[NEW_WINDOW_REQUESTED], 0, NULL);
}


typedef struct {
    YelpWindow *window;
    GtkWindow *gtk_win;
    GtkVBox   *vbox;
    YelpHtml  *html;
} PrintStruct;

static void
window_print_signal (YelpDocument       *document,
		     YelpDocumentSignal  signal,
		     gint                req_id,
		     gpointer           *func_data,
		     PrintStruct        *print)
{
    YelpError *error;

    switch (signal) {
    case YELP_DOCUMENT_SIGNAL_PAGE:
	window_write_print_html (print->html, (YelpPage *) func_data);

	yelp_page_free ((YelpPage *) func_data);

	yelp_print_run (print->window, print->html, print->gtk_win, print->vbox);
	break;
    case YELP_DOCUMENT_SIGNAL_TITLE:
	g_free (func_data);
	break;
    case YELP_DOCUMENT_SIGNAL_ERROR:
	error = (YelpError *) func_data;
	window_error (print->window, (gchar *) yelp_error_get_title (error),
		      (gchar *) yelp_error_get_message (error), FALSE);
	yelp_error_free (error);
	break;
    default:
	g_assert_not_reached();
    }

}


static void
window_print_document_cb (GtkAction *action, YelpWindow *window)
{
    YelpWindowPriv *priv;
    GtkWidget *gtk_win;
    YelpHtml *html;
    GtkWidget *vbox = gtk_vbox_new (FALSE, FALSE);
    PrintStruct *print;
    YelpDocument *doc = NULL;

    priv = window->priv;

    switch (priv->current_type) {
    case YELP_RRN_TYPE_DOC:
	doc = yelp_dbprint_new (priv->uri);
	break;
    default:
	g_assert_not_reached ();
	return;
    }


    gtk_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    html = yelp_html_new ();

    gtk_container_add (GTK_CONTAINER (gtk_win), GTK_WIDGET (vbox));
    gtk_box_pack_end (GTK_BOX (vbox), GTK_WIDGET (html), TRUE, TRUE, 0);
    gtk_widget_show (gtk_win);
    gtk_widget_show (vbox);
    gtk_widget_show (GTK_WIDGET (html));
    gtk_widget_hide (gtk_win);

    print = g_new0 (PrintStruct, 1);

    print->window = window;
    print->gtk_win = (GtkWindow *) gtk_win;
    print->vbox = (GtkVBox *) vbox;
    print->html = html;

    yelp_document_get_page (doc,
			    "x-yelp-index",
			    (YelpDocumentFunc) window_print_signal,
			    (void *) print);
}

static void
window_print_page_cb (GtkAction *action, YelpWindow *window)
{
    YelpWindowPriv *priv;
    GtkWidget *gtk_win;
    YelpHtml *html;
    GtkWidget *vbox = gtk_vbox_new (FALSE, FALSE);
    PrintStruct *print;

    priv = window->priv;

    gtk_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    html = yelp_html_new ();

    gtk_container_add (GTK_CONTAINER (gtk_win), GTK_WIDGET (vbox));
    gtk_box_pack_end (GTK_BOX (vbox), GTK_WIDGET (html), TRUE, TRUE, 0);
    gtk_widget_show (gtk_win);
    gtk_widget_show (vbox);
    gtk_widget_show (GTK_WIDGET (html));
    gtk_widget_hide (gtk_win);

    print = g_new0 (PrintStruct, 1);

    print->window = window;
    print->gtk_win = (GtkWindow *) gtk_win;
    print->vbox = (GtkVBox *) vbox;
    print->html = html;


    if (priv->current_document) {
	/* Need to go through the paging system */
	yelp_document_get_page (priv->current_document,
				priv->current_frag,
				(YelpDocumentFunc) window_print_signal,
				(void *) print);

    } else {
	/* HTML file */

	GFile            *file;
	GFileInputStream *stream;
	gsize             n;
	gchar             buffer[BUFFER_SIZE];

    file   = g_file_new_for_uri (priv->uri);
    stream = g_file_read (file, NULL, NULL);

	if (stream == NULL) {
	    /*GError *error = NULL;
	    g_set_error (&error, YELP_ERROR, YELP_ERROR_IO,
			 _("The file ‘%s’ could not be read.  This file might "
			   "be missing, or you might not have permissions to "
			   "read it."),
			 uri);
			 window_error (window, error, TRUE);*/
	    /* TODO: Proper errors */

	    if (file)
	        g_object_unref (file);
	    if (stream)
	        g_object_unref (stream);
	    return;
	}
	/* Assuming the file exists.  If it doesn't how did we get this far?
	 * There are more sinister forces at work...
	 */

	switch (priv->current_type) {
	case YELP_RRN_TYPE_HTML:
	    yelp_html_open_stream (html, "text/html");
	    break;
	case YELP_RRN_TYPE_XHTML:
	    yelp_html_open_stream (html, "application/xhtml+xml");
	    break;
	case YELP_RRN_TYPE_TEXT:
	    yelp_html_open_stream (html, "text/plain");
	    break;
	default:
	    g_assert_not_reached ();
	}

	while ((g_input_stream_read_all
	    ((GInputStream *)stream, buffer, BUFFER_SIZE, &n, NULL, NULL))) {
	    yelp_html_write (html, buffer, n);
	}

    if (file)
        g_object_unref (file);
    if (stream)
        g_object_unref (stream);

	yelp_html_close (html);

	yelp_print_run (window, html, gtk_win, vbox);

    }
}

static void
window_about_document_cb (GtkAction *action, YelpWindow *window)
{
    YelpWindowPriv *priv;
    gchar *uri;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

    uri = g_strdup_printf("%s#__yelp_info", priv->uri);

    yelp_window_load (window, uri);
    g_free (uri);
}

static void
window_open_location_cb (GtkAction *action, YelpWindow *window)
{
    YelpWindowPriv *priv;
    GtkBuilder     *builder;
    GError         *error = NULL;
    GtkWidget      *dialog;
    GtkWidget      *entry;
    gchar          *uri = NULL;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

    builder = gtk_builder_new ();
    if (!gtk_builder_add_from_file (builder,
                                    DATADIR "/yelp/ui/yelp-open-location.ui",
                                    &error)) {
        g_warning ("Could not load builder file: %s", error->message);
        g_error_free(error);
        return;
    }

    dialog = GTK_WIDGET (gtk_builder_get_object (builder, "location_dialog"));
    entry  = GTK_WIDGET (gtk_builder_get_object (builder, "location_entry"));

    priv->location_dialog = dialog;
    priv->location_entry  = entry;

    uri = priv->req_uri;
    if (uri) {
	gtk_entry_set_text (GTK_ENTRY (entry), uri);
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
    }

    gtk_window_set_transient_for (GTK_WINDOW (dialog),
				  GTK_WINDOW (window));
    g_signal_connect (G_OBJECT (dialog),
		      "response",
		      G_CALLBACK (location_response_cb),
		      window);

    gtk_window_present (GTK_WINDOW (dialog));
}

static void
window_close_window_cb (GtkAction *action, YelpWindow *window)
{
    gtk_widget_destroy (GTK_WIDGET (window));
}

static void
window_copy_cb (GtkAction *action, YelpWindow *window)
{
    GtkWidget *widget;

    g_return_if_fail (YELP_IS_WINDOW (window));

    widget = gtk_window_get_focus (GTK_WINDOW (window));

    if (GTK_IS_EDITABLE (widget)) {
	gtk_editable_copy_clipboard (GTK_EDITABLE (widget));
    } else {
	yelp_html_copy_selection (window->priv->html_view);
    }
}

static void
window_select_all_cb (GtkAction *action, YelpWindow *window)
{
    GtkWidget *widget;

    g_return_if_fail (YELP_IS_WINDOW (window));

    widget = gtk_window_get_focus (GTK_WINDOW (window));

    if (GTK_IS_EDITABLE (widget)) {
	gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);
    } else {
	yelp_html_select_all (window->priv->html_view);
    }
}

static void
window_find_cb (GtkAction *action, YelpWindow *window)
{
    YelpWindowPriv *priv;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

    debug_print (DB_FUNCTION, "entering\n");

    gtk_widget_show_all (priv->find_bar);
    gtk_widget_hide (GTK_WIDGET (priv->find_not_found));
    gtk_widget_hide (GTK_WIDGET (priv->find_sep));
    gtk_widget_grab_focus (priv->find_entry);
    window_find_entry_changed_cb (GTK_EDITABLE (priv->find_entry), window);
}

static void
window_preferences_cb (GtkAction *action, YelpWindow *window)
{
    g_return_if_fail (YELP_IS_WINDOW (window));

    yelp_settings_open_preferences ();
}

static void
window_reload_cb (GtkAction *action, YelpWindow *window)
{
    if (window->priv->current_document) {
	if (window->priv->current_request > -1) {
	    yelp_document_cancel_page (window->priv->current_document, window->priv->current_request);
	}
	g_free (window->priv->uri);
	window->priv->uri = NULL;
	g_object_unref (window->priv->current_document);
	window->priv->current_document = NULL;
	yelp_window_load (window, window->priv->req_uri);
    }
}

static void
window_enable_cursor_cb (GtkAction *action, YelpWindow *window)
{
    yelp_settings_toggle_caret ();
}

static void
history_load_entry (YelpWindow *window, YelpHistoryEntry *entry)
{
    gchar *slash, *frag_id;
    g_return_if_fail (YELP_IS_WINDOW (window));

    if (entry->type == YELP_RRN_TYPE_HTML || entry->type == YELP_RRN_TYPE_XHTML || entry->type == YELP_RRN_TYPE_TEXT) {
	window_do_load_html (window, entry->uri, entry->frag_id, entry->type, FALSE);
    } else {
	g_assert (entry->doc != NULL);
	window_setup_window (window, entry->type, entry->uri, entry->frag_id, entry->req_uri,
			     window->priv->base_uri, FALSE);
	if (window->priv->base_uri)
	    g_free (window->priv->base_uri);
	window->priv->base_uri = g_strdup (entry->base_uri);
	window->priv->current_document = entry->doc;

        /* FIXME: Super hacky.  We want the part before the slash for mallard IDs,
           because right now we're outputting "#page_id/section_id".  We ought to
           be scrolling to the section as well.
         */
        slash = strchr (entry->frag_id, '/');
        if (entry->type == YELP_RRN_TYPE_MAL && slash)
            frag_id = g_strndup (entry->frag_id, slash - entry->frag_id);
        else
            frag_id = g_strdup (entry->frag_id);
	window->priv->current_request = yelp_document_get_page (entry->doc,
                                                                frag_id,
                                                                (YelpDocumentFunc) page_request_cb,
                                                                (void *) window);
        g_free (frag_id);
    }

}


static void
window_go_back_cb (GtkAction *action, YelpWindow *window)
{
    YelpWindowPriv   *priv;
    YelpHistoryEntry *entry;

    g_return_if_fail (YELP_IS_WINDOW (window));
    g_return_if_fail (window->priv->history_back != NULL);

    priv = window->priv;

    history_push_forward (window);

    entry = history_pop_back (window);

    history_load_entry (window, entry);

    history_entry_free (entry);
}

static void
window_go_forward_cb (GtkAction *action, YelpWindow *window)
{
    YelpWindowPriv   *priv;
    YelpHistoryEntry *entry;

    g_return_if_fail (YELP_IS_WINDOW (window));
    g_return_if_fail (window->priv->history_forward != NULL);

    priv = window->priv;

    history_push_back (window);

    entry = history_pop_forward (window);

    history_load_entry (window, entry);

    history_entry_free (entry);
}

static void
window_go_home_cb (GtkAction *action, YelpWindow *window)
{
    debug_print (DB_FUNCTION, "entering\n");
    g_return_if_fail (YELP_IS_WINDOW (window));
    yelp_window_load (window, "x-yelp-toc:");
}

static void
window_go_previous_cb (GtkAction *action, YelpWindow *window)
{
    printf ("Prev: %s\n", window->priv->prev_id);
#if 0
    YelpWindowPriv *priv;
    gchar *base, *uri;

    debug_print (DB_FUNCTION, "entering\n");
    g_return_if_fail (YELP_IS_WINDOW (window));
    g_return_if_fail (window->priv->current_doc);
    priv = window->priv;

    base = yelp_doc_info_get_uri (priv->current_doc, NULL, YELP_URI_TYPE_ANY);
    uri = yelp_uri_get_relative (base, priv->prev_id);

    yelp_window_load (window, uri);

    g_free (uri);
    g_free (base);
#endif
}

static void
window_go_next_cb (GtkAction *action, YelpWindow *window)
{
    printf ("Next: %s\n", window->priv->next_id);
#if 0
    YelpWindowPriv *priv;
    gchar *base, *uri;

    debug_print (DB_FUNCTION, "entering\n");
    g_return_if_fail (YELP_IS_WINDOW (window));
    g_return_if_fail (window->priv->current_doc);
    priv = window->priv;

    base = yelp_doc_info_get_uri (priv->current_doc, NULL, YELP_URI_TYPE_ANY);
    uri = yelp_uri_get_relative (base, priv->next_id);

    yelp_window_load (window, uri);

    g_free (uri);
    g_free (base);
#endif
}

static void
window_go_toc_cb (GtkAction *action, YelpWindow *window)
{
    printf ("index toc: %s\n", window->priv->toc_id);
#if 0
    YelpWindowPriv *priv;
    gchar *base, *uri;

    debug_print (DB_FUNCTION, "entering\n");
    g_return_if_fail (YELP_IS_WINDOW (window));
    g_return_if_fail (window->priv->current_doc);
    priv = window->priv;

    base = yelp_doc_info_get_uri (priv->current_doc, NULL, YELP_URI_TYPE_ANY);
    uri = yelp_uri_get_relative (base, priv->toc_id);

    yelp_window_load (window, uri);

    g_free (uri);
    g_free (base);
#endif
}

static void
window_add_bookmark_cb (GtkAction *action, YelpWindow *window)
{
    YelpWindowPriv *priv = window->priv;

    debug_print (DB_FUNCTION, "entering\n");

    if (!priv->req_uri)
	return;

    yelp_bookmarks_add (priv->req_uri, window);

}

static void window_copy_link_cb (GtkAction *action, YelpWindow *window)
{
    gtk_clipboard_set_text (gtk_clipboard_get (gdk_atom_intern ("CLIPBOARD",
								TRUE)),
			    window->priv->uri,
			    -1);
}

static void
window_open_link_cb (GtkAction *action, YelpWindow *window)
{
    yelp_window_load (window, window->priv->uri);
}

static void
window_open_link_new_cb (GtkAction *action, YelpWindow *window)
{
    g_signal_emit (window, signals[NEW_WINDOW_REQUESTED], 0,
		   window->priv->uri);
}

/* TODO: This doesn't work... */
static void
window_copy_mail_cb (GtkAction *action, YelpWindow *window)
{
    /*Do things the simple way.
     * Split the string and remove any query bit then
     * remove the first 7 chars as they should be mailto:
     */
    gchar **split_string = g_strsplit (window->priv->uri, "?", 2);

    gchar *mail_address = &split_string[0][7];

    gtk_clipboard_set_text (gtk_clipboard_get (gdk_atom_intern ("CLIPBOARD",
								TRUE)),
			    mail_address,
			    -1);
    g_free (window->priv->uri);
    g_strfreev(split_string);
}

static void
window_help_contents_cb (GtkAction *action, YelpWindow *window)
{
    /* Since we are already running, and are calling ourselves,
     * we can make this function easy by just creating a new window
     * and avoiding calling gnome_help_display_desktop
     */
    g_signal_emit (window, signals[NEW_WINDOW_REQUESTED], 0,
		   "ghelp:user-guide#yelp");
}

static void
window_about_cb (GtkAction *action, YelpWindow *window)
{
    const gchar *copyright =
	"Copyright © 2001-2003 Mikael Hallendal\n"
	"Copyright © 2003-2009 Shaun McCance\n"
	"Copyright © 2005-2006 Don Scorgie\n"
	"Copyright © 2005-2006 Brent Smith";
    const gchar *authors[] = {
	"Mikael Hallendal <micke@imendio.com>",
	"Alexander Larsson <alexl@redhat.com>",
	"Shaun McCance <shaunm@gnome.org>",
	"Don Scorgie <Don@Scorgie.org>",
	"Brent Smith <gnome@nextreality.net>",
	NULL
    };
    /* Note to translators: put here your name (and address) so it
     * will show up in the "about" box */
    const gchar *translator_credits = _("translator-credits");

    gtk_show_about_dialog (GTK_WINDOW (window),
			   "name", _("Yelp"),
			   "version", VERSION,
			   "comments", _("A documentation browser and "
					 "viewer for the Gnome Desktop."),
			   "copyright", copyright,
			   "authors", authors,
			   "translator-credits", translator_credits,
			   "logo-icon-name", "help-browser",
			   NULL);
}

/******************************************************************************/

static void
location_response_cb (GtkDialog *dialog, gint id, YelpWindow *window)
{
    g_return_if_fail (YELP_IS_WINDOW (window));

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  id = %i\n", id);

    if (id == GTK_RESPONSE_OK) {
	const gchar *uri =
	    gtk_entry_get_text (GTK_ENTRY (window->priv->location_entry));

	yelp_window_load (window, uri);
    }

    window->priv->location_dialog = NULL;
    window->priv->location_entry  = NULL;
    gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
window_find_save_settings (YelpWindow *window)
{
    YelpWindowPriv *priv;
    const gchar    *tmp;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

    tmp = gtk_entry_get_text (GTK_ENTRY (priv->find_entry));

    g_free (priv->find_string);

    priv->find_string = g_utf8_casefold (tmp, -1);
}

static gboolean
window_find_action (YelpWindow *window)
{
    YelpWindowPriv *priv = window->priv;

    window_find_save_settings (window);

    yelp_html_set_find_props (priv->html_view, priv->find_string, FALSE, FALSE);

    return yelp_html_find (priv->html_view, priv->find_string);
}

static gboolean
window_find_again (YelpWindow *window, gboolean forward)
{
    YelpWindowPriv *priv = window->priv;

    return yelp_html_find_again (priv->html_view, forward);
}

static void
window_find_previous_cb (GtkAction *action, YelpWindow *window)
{
    YelpWindowPriv * priv;

    priv = window->priv;
    if (!window_find_again (window, FALSE)) {
	window_find_buttons_set_sensitive (window, FALSE, TRUE);
    } else {
	window_find_buttons_set_sensitive (window, TRUE, TRUE);
    }
}

static void
window_find_next_cb (GtkAction *action, YelpWindow *window)
{
    YelpWindowPriv * priv;

    priv = window->priv;
    if (!window_find_again (window, TRUE)) {
	window_find_buttons_set_sensitive (window, TRUE, FALSE);
    } else {
	window_find_buttons_set_sensitive (window, TRUE, TRUE);
    }
}

static void
window_find_clicked_cb (GtkWidget  *widget,
			YelpWindow *window)
{
    YelpWindowPriv * priv;

    g_return_if_fail (GTK_IS_TOOL_ITEM (widget));

    priv = window->priv;

    if (GTK_TOOL_ITEM (widget) == priv->find_next) {
	if (!window_find_again (window, TRUE)) {
	    window_find_buttons_set_sensitive (window, TRUE, FALSE);
	} else {
	    window_find_buttons_set_sensitive (window, TRUE, TRUE);
	}
    }
    else if (GTK_TOOL_ITEM (widget) == priv->find_prev) {
	if (!window_find_again (window, FALSE)) {
	    window_find_buttons_set_sensitive (window, FALSE, TRUE);
	} else {
	    window_find_buttons_set_sensitive (window, TRUE, TRUE);
	}
    }
    else
	g_assert_not_reached ();
}

static void
window_find_entry_changed_cb (GtkEditable *editable,
			      gpointer     data)
{
    YelpWindow     *window;
    YelpWindowPriv *priv;
    gchar          *text;
    gboolean        found;

    g_return_if_fail (YELP_IS_WINDOW(data));

    window = YELP_WINDOW (data);
    priv = window->priv;

    text = gtk_editable_get_chars (editable, 0, -1);

    found = window_find_action (window);

    if (text == NULL || text[0] == '\0')
	window_find_buttons_set_sensitive (window, FALSE, FALSE);
    else
	if (found) {
	    gtk_widget_hide (GTK_WIDGET (priv->find_sep));
	    gtk_widget_hide (GTK_WIDGET (priv->find_not_found));
	    window_find_buttons_set_sensitive (window, FALSE, TRUE);
	} else {
	    gtk_widget_show_all (GTK_WIDGET (priv->find_sep));
	    gtk_widget_show_all (GTK_WIDGET (priv->find_not_found));
	    window_find_buttons_set_sensitive (window, TRUE, TRUE);
	}

    g_free (text);
}

static void
window_find_entry_activate_cb (GtkEditable *editable,
				gpointer data)
{
    YelpWindow *window;
    YelpWindowPriv *priv;

    g_return_if_fail (YELP_IS_WINDOW (data));

    window = YELP_WINDOW (data);
    priv = window->priv;

    /* search forward and update the buttons */
    if (!window_find_again (window, TRUE)) {
	window_find_buttons_set_sensitive (window, TRUE, FALSE);
    } else {
	window_find_buttons_set_sensitive (window, TRUE, TRUE);
    }
}

static gboolean
window_find_entry_key_pressed_cb (GtkWidget    *widget,
				  GdkEventKey  *key,
				  gpointer     data)
{
    YelpWindow     *window;
    YelpWindowPriv *priv;

    debug_print (DB_FUNCTION, "entering\n");

    if (key->keyval == GDK_Escape) {
	g_return_val_if_fail (YELP_IS_WINDOW (data), FALSE);

	window = YELP_WINDOW (data);
	priv = window->priv;

	gtk_widget_hide_all (priv->find_bar);

	return TRUE;
    }

    return FALSE;
}

static void
window_find_buttons_set_sensitive (YelpWindow  *window,
				   gboolean    prev, gboolean next)
{
    YelpWindowPriv *priv;

    priv = window->priv;

    gtk_widget_set_sensitive (GTK_WIDGET (priv->find_next), next);
    gtk_widget_set_sensitive (GTK_WIDGET (priv->find_prev), prev);
    g_object_set (G_OBJECT (priv->find_next_menu), "sensitive", next,
		  NULL);
    g_object_set (G_OBJECT (priv->find_prev_menu), "sensitive", prev,
		  NULL);
}

static gboolean
tree_model_iter_following (GtkTreeModel  *model,
			   GtkTreeIter   *iter)
{
    gboolean     valid;
    GtkTreeIter *old_iter = gtk_tree_iter_copy (iter);

    if (gtk_tree_model_iter_has_child (model, iter)) {
	gboolean ret_val;
	ret_val = gtk_tree_model_iter_children (model, iter, old_iter);
	gtk_tree_iter_free (old_iter);
	return ret_val;
    }
    else do {
	valid = gtk_tree_model_iter_next (model, iter);

	if (valid) {
	    gtk_tree_iter_free (old_iter);
	    return TRUE;
	}

	valid = gtk_tree_model_iter_parent (model, iter, old_iter);

	if (!valid) {
	    gtk_tree_iter_free (old_iter);
	    return FALSE;
	}

	*old_iter = *iter;

    } while (TRUE);

    g_assert_not_reached ();
    return FALSE;
}

static gboolean
window_write_html (YelpLoadData *data)
{
    gchar *uri = NULL;
    gsize read;
    YelpHtml *html = data->window->priv->html_view;
    gchar contents[BUFFER_SIZE];

    /* Use a silly fake URI to stop gecko doing silly things */
    if (data->window->priv->current_frag) {
        /* FIXME: more of this terrible slash hack */
        gchar *slash, *jump_id;
        slash = strchr (data->window->priv->current_frag, '/');
        if (slash)
            jump_id = slash + 1;
        else
            jump_id = data->window->priv->current_frag;
	uri = g_strdup_printf ("%s#%s", data->window->priv->base_uri, jump_id);
    }
    else
	uri = g_strdup (data->window->priv->base_uri);

    yelp_html_set_base_uri (html, uri);
    g_free (uri);
    yelp_html_open_stream (html, "application/xhtml+xml");

    do {
	yelp_page_read (data->page, contents, BUFFER_SIZE, &read, NULL);
	yelp_html_write (html, contents, read);
    } while (read == BUFFER_SIZE);
    yelp_html_close (html);
    return FALSE;
}

static void
window_write_print_html (YelpHtml *html, YelpPage * page)
{
    gsize read;
    gchar contents[BUFFER_SIZE];

    /* Use a silly fake URI to stop gecko doing silly things */
    yelp_html_set_base_uri (html, "file:///foobar");
    yelp_html_open_stream (html, "application/xhtml+xml");

    do {
	yelp_page_read (page, contents, BUFFER_SIZE, &read, NULL);
	yelp_html_write (html, contents, read);
    } while (read == BUFFER_SIZE);
    yelp_html_close (html);
}
