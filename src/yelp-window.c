/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
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
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <bonobo/bonobo-main.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnome/libgnome.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <string.h>

#include "yelp-bookmarks.h"
#include "yelp-db-pager.h"
#include "yelp-db-print-pager.h"
#include "yelp-error.h"
#include "yelp-html.h"
#include "yelp-pager.h"
#include "yelp-settings.h"
#include "yelp-toc-pager.h"
#include "yelp-window.h"
#include "yelp-print.h"
#include "yelp-debug.h"

#ifdef ENABLE_MAN
#include "yelp-man-pager.h"
#endif
#ifdef ENABLE_INFO
#include "yelp-info-pager.h"
#endif
#ifdef ENABLE_SEARCH
#include "yelp-search-pager.h"
#include "gtkentryaction.h"
#endif

#define YELP_CONFIG_WIDTH          "/yelp/Geometry/width"
#define YELP_CONFIG_HEIGHT         "/yelp/Geometry/height"
#define YELP_CONFIG_WIDTH_DEFAULT  "600"
#define YELP_CONFIG_HEIGHT_DEFAULT "420"

#define BUFFER_SIZE 16384

typedef struct {
    YelpWindow *window;
    gchar      *uri;
} YelpLoadData;

typedef struct {
    YelpDocInfo *doc_info;

    gchar *frag_id;
    GtkWidget *menu_entry;
    YelpWindow *window;
    gint callback;

    gchar *page_title;
    gchar *frag_title;
} YelpHistoryEntry;

static void        window_init		          (YelpWindow        *window);
static void        window_class_init	          (YelpWindowClass   *klass);

static void        window_error                   (YelpWindow        *window,
						   GError            *error,
						   gboolean           pop);
static void        window_populate                (YelpWindow        *window);
static void        window_populate_find           (YelpWindow        *window,
						   GtkWidget         *find_bar);
static void        window_set_sections            (YelpWindow        *window,
						   GtkTreeModel      *sections);
static void        window_do_load                 (YelpWindow        *window,
						   YelpDocInfo       *doc_info,
						   gchar             *frag_id);
static gboolean    window_do_load_pager           (YelpWindow        *window,
						   YelpDocInfo       *doc_info,
						   gchar             *frag_id);
static gboolean    window_do_load_html            (YelpWindow        *window,
						   YelpDocInfo       *doc_info,
						   gchar             *frag_id);
static void        window_set_loading             (YelpWindow        *window);
static void        window_handle_page             (YelpWindow        *window,
						   YelpPage          *page);
static void        window_disconnect              (YelpWindow        *window);

/** Window Callbacks **/
static gboolean    window_configure_cb            (GtkWidget         *widget,
						   GdkEventConfigure *event,
						   gpointer           data);

/** Pager Callbacks **/
static void        pager_start_cb                 (YelpPager         *pager,
						   gpointer           user_data);
static void        pager_page_cb                  (YelpPager         *pager,
						   gchar             *page_id,
						   gpointer           user_data);
static void        pager_error_cb                 (YelpPager         *pager,
						   gpointer           user_data);
static void        pager_cancel_cb                (YelpPager         *pager,
						   gpointer           user_data);
static void        pager_finish_cb                (YelpPager         *pager,
						   gpointer           user_data);

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

static gboolean           window_load_async      (YelpLoadData *data);

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

static void               load_data_free         (YelpLoadData     *data);

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
static void        window_find_next_cb            (GtkAction *action, 
						   YelpWindow *window);
static void        window_find_previous_cb        (GtkAction *action, 
						   YelpWindow *window);
static gboolean    tree_model_iter_following      (GtkTreeModel      *model,
						   GtkTreeIter       *iter);

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

    /* Find in Page */
    GtkToolItem    *find_prev;
    GtkToolItem    *find_next;
    gchar          *find_string;
    GtkAction      *find_next_menu;
    GtkAction      *find_prev_menu;

    /* Open Location */
    GtkWidget      *location_dialog;
    GtkWidget      *location_entry;

    /* Popup menu*/
    GtkWidget      *popup;
    gint            merge_id;
    GtkWidget      *maillink;
    gchar          *uri;

    /* Location Information */
    YelpDocInfo    *current_doc;
    gchar          *current_frag;
    GSList         *history_back;
    GSList         *history_forward;
    GtkWidget      *back_menu;
    GtkWidget      *forward_menu;

    /* Callbacks and Idles */
    gulong          start_handler;
    gulong          page_handler;
    gulong          error_handler;
    gulong          cancel_handler;
    gulong          finish_handler;
    guint           idle_write;

    gint            toc_pause;

    GtkActionGroup *action_group;
    GtkUIManager   *ui_manager;

    /* Don't free these */
    gchar          *prev_id;
    gchar          *next_id;
    gchar          *toc_id;
};

typedef struct _IdleWriterContext IdleWriterContext;
struct _IdleWriterContext {
    YelpWindow   *window;

    enum {
	IDLE_WRITER_MEMORY,
	IDLE_WRITER_VFS
    } type;

    const gchar  *buffer;
    gint          cur;
    gint          length;
};

static gboolean    idle_write    (IdleWriterContext *context);

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
      N_("Print This Document"),
      NULL,
      NULL,
      G_CALLBACK (window_print_document_cb) },
    { "PrintPage", NULL,
      N_("Print This Page"),
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
   
    window_populate (window);
}

static void
window_dispose (GObject *object)
{
    YelpWindow *window = YELP_WINDOW (object);

    window_disconnect (YELP_WINDOW (window));

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
    
    if (priv->current_doc)
	    yelp_doc_info_unref (priv->current_doc);
    g_free (priv->current_frag);

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

/** History Functions *********************************************************/

static void
history_push_back (YelpWindow *window)
{
    YelpWindowPriv   *priv;
    YelpHistoryEntry *entry;
    GtkAction        *action;
    gchar            *title;

    g_return_if_fail (YELP_IS_WINDOW (window));
    g_return_if_fail (window->priv->current_doc != NULL);

    priv = window->priv;

    entry = g_new0 (YelpHistoryEntry, 1);
    entry->doc_info = yelp_doc_info_ref (priv->current_doc);
    entry->frag_id  = g_strdup (priv->current_frag);
    /* page_title, frag_title */

    priv->history_back = g_slist_prepend (priv->history_back, entry);

    action = gtk_action_group_get_action (priv->action_group, "GoBack");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", TRUE, NULL);
    title = (gchar *) gtk_window_get_title (GTK_WINDOW (window));

    if (g_str_equal (title, "Loading..."))
	entry->page_title = g_strdup ("Unknown Page");
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
    g_return_if_fail (window->priv->current_doc != NULL);

    priv = window->priv;

    entry = g_new0 (YelpHistoryEntry, 1);
    entry->doc_info = yelp_doc_info_ref (priv->current_doc);
    entry->frag_id  = g_strdup (priv->current_frag);
    /* page_title, frag_title */

    priv->history_forward = g_slist_prepend (priv->history_forward, entry);

    action = gtk_action_group_get_action (priv->action_group, "GoForward");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", TRUE, NULL);

    title = (gchar *) gtk_window_get_title (GTK_WINDOW (window));

    if (g_str_equal (title, "Loading..."))
	entry->page_title = g_strdup ("Unknown Page");
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

    if (priv->current_doc) {
	yelp_doc_info_unref (priv->current_doc);
	priv->current_doc = NULL;
    }
    if (priv->current_frag) {
	g_free (priv->current_frag);
	priv->current_frag = NULL;
    }

    if (entry) {
	priv->current_doc  = yelp_doc_info_ref (entry->doc_info);
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

    yelp_doc_info_unref (entry->doc_info);
    g_free (entry->frag_id);
    g_free (entry->page_title);
    g_free (entry->frag_title);

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

    if (priv->current_doc)
	yelp_doc_info_unref (priv->current_doc);
    if (priv->current_frag)
	g_free (priv->current_frag);

    priv->current_doc  = yelp_doc_info_ref (entry->doc_info);
    priv->current_frag = g_strdup (entry->frag_id);

    window_do_load (window, entry->doc_info, entry->frag_id);

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

    if (priv->current_doc)
	yelp_doc_info_unref (priv->current_doc);
    if (priv->current_frag)
	g_free (priv->current_frag);

    priv->current_doc  = yelp_doc_info_ref (entry->doc_info);
    priv->current_frag = g_strdup (entry->frag_id);

    window_do_load (window, entry->doc_info, entry->frag_id);

}

static void
load_data_free (YelpLoadData *data)
{
    g_return_if_fail (data != NULL);

    g_object_unref (data->window);
    g_free (data->uri);

    g_free (data);
}

/******************************************************************************/

GtkWidget *
yelp_window_new (GNode *doc_tree, GList *index)
{
    return GTK_WIDGET (g_object_new (YELP_TYPE_WINDOW, NULL));
}

void
yelp_window_load (YelpWindow *window, const gchar *uri)
{
    YelpWindowPriv *priv;
    YelpDocInfo    *doc_info;
    gchar          *frag_id;
    GtkAction      *action;
    gchar          *real_uri;
    g_return_if_fail (YELP_IS_WINDOW (window));

    if (g_str_has_prefix (uri, "info:") && g_str_has_suffix (uri, "dir")) {
	real_uri = g_strdup ("x-yelp-toc:#Info");
    } else {
	real_uri = g_strdup (uri);
    }
    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  uri = \"%s\"\n", uri);

    if (!real_uri) {
	GError *error = NULL;
	g_set_error (&error, YELP_ERROR, YELP_ERROR_NO_DOC,
		     _("The Uniform Resource Identifier for the file is "
		       "invalid."));
	window_error (window, error, FALSE);
	return;
    }

    priv = window->priv;

    doc_info = yelp_doc_info_get (real_uri, FALSE);
    if (!doc_info) {
	GError *error = NULL;
	g_set_error (&error, YELP_ERROR, YELP_ERROR_NO_DOC,
		     _("The Uniform Resource Identifier ‘%s’ is invalid "
		       "or does not point to an actual file."),
		     real_uri);
	window_error (window, error, FALSE);
	return;
    }
    frag_id  = yelp_uri_get_fragment (real_uri);

    if (priv->current_doc && yelp_doc_info_equal (priv->current_doc, doc_info)) {
	if (priv->current_frag) {
	    if (frag_id && g_str_equal (priv->current_frag, frag_id))
		goto load;
	}
	else if (!frag_id)
	    goto load;
    }

    if (priv->current_doc)
	history_push_back (window);
    history_clear_forward (window);

    if (priv->current_doc)
	yelp_doc_info_unref (priv->current_doc);
    if (priv->current_frag)
	g_free (priv->current_frag);

    action = gtk_action_group_get_action (priv->action_group, 
						  "PrintDocument");
    g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);


    priv->current_doc  = yelp_doc_info_ref (doc_info);
    priv->current_frag = g_strdup (frag_id);

 load:
    window_do_load (window, doc_info, frag_id);

    if (priv->current_frag != frag_id)
	g_free (frag_id);
    g_free (real_uri);
}

YelpDocInfo *
yelp_window_get_doc_info (YelpWindow *window)
{
    g_return_val_if_fail (YELP_IS_WINDOW (window), NULL);

    return window->priv->current_doc;
}

GtkUIManager *
yelp_window_get_ui_manager (YelpWindow *window)
{
    g_return_val_if_fail (YELP_IS_WINDOW (window), NULL);

    return window->priv->ui_manager;
}

static void
window_do_load (YelpWindow  *window,
		YelpDocInfo *doc_info,
		gchar       *frag_id)
{
    GtkAction      *action;
    GtkAction      *about;
    GError         *error = NULL;
    gchar          *uri;

    g_return_if_fail (YELP_IS_WINDOW (window));
    g_return_if_fail (doc_info != NULL);

    debug_print (DB_FUNCTION, "entering\n");

    action = gtk_action_group_get_action (window->priv->action_group, 
					  "PrintDocument");
    g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);

    about  = gtk_action_group_get_action (window->priv->action_group,
                                          "AboutDocument");
    g_object_set (G_OBJECT (about),  "sensitive", FALSE, NULL);
    
    switch (yelp_doc_info_get_type (doc_info)) {
    case YELP_DOC_TYPE_MAN:
#ifdef ENABLE_MAN
	window_do_load_pager (window, doc_info, frag_id);
	break;
#else
	g_set_error (&error, YELP_ERROR, YELP_ERROR_FORMAT,
		     _("Man pages are not supported in this version."));
	break;
#endif

    case YELP_DOC_TYPE_INFO:
#ifdef ENABLE_INFO
	window_do_load_pager (window, doc_info, frag_id);
	break;
#else
	g_set_error (&error, YELP_ERROR, YELP_ERROR_FORMAT,
		     _("GNU info pages are not supported in this version"));
	break;
#endif

    case YELP_DOC_TYPE_DOCBOOK_XML:
	g_object_set (G_OBJECT (action), "sensitive", TRUE, NULL);
	g_object_set (G_OBJECT (about),  "sensitive", TRUE, NULL);
    case YELP_DOC_TYPE_TOC:
	window_do_load_pager (window, doc_info, frag_id);
	break;
    case YELP_DOC_TYPE_SEARCH:
#ifdef ENABLE_SEARCH
	window_do_load_pager (window, doc_info, frag_id);
	break;
#else
	g_set_error (&error, YELP_ERROR, YELP_ERROR_FORMAT,
		     _("Search is not supported in this version."));
	break;
#endif
    case YELP_DOC_TYPE_DOCBOOK_SGML:
	g_set_error (&error, YELP_ERROR, YELP_ERROR_FORMAT,
		     _("SGML documents are no longer supported. Please ask "
		       "the author of the document to convert to XML."));
	break;
    case YELP_DOC_TYPE_HTML:
    case YELP_DOC_TYPE_XHTML:
	window_do_load_html (window, doc_info, frag_id);
	break;
    case YELP_DOC_TYPE_EXTERNAL:
	history_step_back (window);
	uri = yelp_doc_info_get_uri (doc_info, NULL, YELP_URI_TYPE_ANY);
	gnome_url_show (uri, &error);
	g_free (uri);
	break;
    case YELP_DOC_TYPE_ERROR:
    default:
	uri = yelp_doc_info_get_uri (doc_info, NULL, YELP_URI_TYPE_NO_FILE);
	if (!uri)
	    uri = yelp_doc_info_get_uri (doc_info, NULL, YELP_URI_TYPE_FILE);
	if (uri)
	    g_set_error (&error, YELP_ERROR, YELP_ERROR_NO_DOC,
			 _("The Uniform Resource Identifier ‘%s’ is invalid "
			   "or does not point to an actual file."),
			 uri);
	else
	    g_set_error (&error, YELP_ERROR, YELP_ERROR_NO_DOC,
			 _("The Uniform Resource Identifier for the file is "
			   "invalid."));
	g_free (uri);
	break;
    }
 
    if (error) {
	window_error (window, error, TRUE);
    }

    window_find_buttons_set_sensitive (window, TRUE, TRUE);    
}

/******************************************************************************/

static void
window_error (YelpWindow *window, GError *error, gboolean pop)
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
	 "%s", yelp_error_get_primary (error));
    gtk_message_dialog_format_secondary_markup
	(GTK_MESSAGE_DIALOG (dialog), "%s",
	 yelp_error_get_secondary (error));
    gtk_dialog_run (GTK_DIALOG (dialog));

    g_error_free (error);
    gtk_widget_destroy (dialog);
}

#ifdef ENABLE_SEARCH
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
    } else {
	uri = encode_search_uri (search_terms);
    }

    yelp_window_load (window, uri);

    g_free (uri);
}
#endif


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

    gtk_action_connect_proxy (action, b_proxy);

    action = gtk_action_group_get_action (priv->action_group, "GoForward");
    gtk_action_connect_proxy (action, f_proxy);

    gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (f_proxy), 
				   priv->forward_menu);
    
#ifdef ENABLE_SEARCH
    action =  gtk_entry_action_new ("Search",
				    _("_Search:"),
				    _("Search for other documentation"),
				    NULL);
    g_signal_connect (G_OBJECT (action), "activate",
		      G_CALLBACK (search_activated), window);
    gtk_action_group_add_action (priv->action_group, action);
#endif

    priv->ui_manager = gtk_ui_manager_new ();
    gtk_ui_manager_insert_action_group (priv->ui_manager, priv->action_group, 0);

    accel_group = gtk_ui_manager_get_accel_group (priv->ui_manager);
    gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);


    g_signal_connect (G_OBJECT (priv->ui_manager), "add-widget",
		      G_CALLBACK (window_add_widget), priv->main_box);

    if (!gtk_ui_manager_add_ui_from_file (priv->ui_manager,
					  DATADIR "/yelp/ui/yelp-ui.xml",
					  &error)) {
	window_error (window, error, FALSE);
    }

#ifdef ENABLE_SEARCH
    if (!gtk_ui_manager_add_ui_from_file (priv->ui_manager,
					  DATADIR "/yelp/ui/yelp-search-ui.xml",
					  &error)) {
	window_error (window, error, FALSE);
    }
#endif

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
	 "text", YELP_PAGER_COLUMN_TITLE,
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

static void
window_populate_find (YelpWindow *window, GtkWidget *find_bar)
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
    gtk_box_pack_start (GTK_BOX (box), priv->find_entry, TRUE, TRUE, 0);

    item = gtk_tool_item_new ();
    gtk_container_add (GTK_CONTAINER (item), box);
    gtk_toolbar_insert (GTK_TOOLBAR (find_bar), item, -1);

    gtk_label_set_mnemonic_widget (GTK_LABEL (label), priv->find_entry);

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

    if (sections)
	gtk_widget_show_all (priv->side_sw);
    else
	gtk_widget_hide (priv->side_sw);
}

static gboolean
window_do_load_pager (YelpWindow  *window,
		      YelpDocInfo *doc_info,
		      gchar       *frag_id)
{
    YelpWindowPriv *priv;
    YelpPagerState  state;
    YelpPager  *pager;
    GError     *error = NULL;
    YelpPage   *page  = NULL;
    gboolean    loadnow  = FALSE;
    gboolean    startnow = TRUE;
    gboolean    handled  = FALSE;

    gchar      *uri;

    g_return_val_if_fail (YELP_IS_WINDOW (window), FALSE);
    g_return_val_if_fail (doc_info != NULL, FALSE);

    priv = window->priv;

    uri = yelp_doc_info_get_uri (doc_info, frag_id, YELP_URI_TYPE_FILE);

    window_disconnect (window);

    pager = yelp_doc_info_get_pager (doc_info);

    if (!pager) {
	switch (yelp_doc_info_get_type (doc_info)) {
	case YELP_DOC_TYPE_DOCBOOK_XML:
	    pager = yelp_db_pager_new (doc_info);
	    break;
	case YELP_DOC_TYPE_INFO:
#ifdef ENABLE_INFO
	    pager = yelp_info_pager_new (doc_info);
	    break;
#else
	g_set_error (&error, YELP_ERROR, YELP_ERROR_FORMAT,
		     _("GNU info pages are not supported in this version"));
#endif

	case YELP_DOC_TYPE_MAN:
#ifdef ENABLE_MAN
	    pager = yelp_man_pager_new (doc_info);
	    break;
#else
	    g_set_error (&error, YELP_ERROR, YELP_ERROR_FORMAT,
			 _("Man pages are not supported in this version."));
	    break;
#endif

	case YELP_DOC_TYPE_TOC:
	    pager = YELP_PAGER (yelp_toc_pager_get ());
	    break;
	case YELP_DOC_TYPE_SEARCH:
#ifdef ENABLE_SEARCH
	    pager = YELP_PAGER (yelp_search_pager_get (doc_info));
	    break;
#else
	    g_set_error (&error, YELP_ERROR, YELP_ERROR_FORMAT,
			 _("Search is not supported in this version."));
	    break;
#endif
	default:
	    break;
	}
	if (pager)
	    yelp_doc_info_set_pager (doc_info, pager);
    }

    if (!pager) {
	if (!error)
	    g_set_error (&error, YELP_ERROR, YELP_ERROR_PROC,
			 _("A transformation context could not be created for "
			   "the file ‘%s’. The format may not be supported."),
			 uri);
	window_error (window, error, TRUE);
	handled = FALSE;
	goto done;
    }

    g_object_ref (pager);

    loadnow  = FALSE;
    startnow = FALSE;

    state = yelp_pager_get_state (pager);
    debug_print (DB_DEBUG, "pager state=%d\n", state);
    switch (state) {
    case YELP_PAGER_STATE_ERROR:
	error = yelp_pager_get_error (pager);
	if (error)
	    window_error (window, error, TRUE);
	handled = FALSE;
	goto done;
    case YELP_PAGER_STATE_RUNNING:
    case YELP_PAGER_STATE_FINISHED:
	/* Check if the page exists */
	pager_start_cb (pager, window);

	page = (YelpPage *) yelp_pager_get_page (pager, frag_id);
	if (!page && (state == YELP_PAGER_STATE_FINISHED)) {
	    g_set_error (&error, YELP_ERROR, YELP_ERROR_NO_PAGE,
			 _("The section ‘%s’ does not exist in this document. "
			   "If you were directed to this section from a Help "
			   "button in an application, please report this to "
			   "the maintainers of that application."),
			 frag_id);
	    window_error (window, error, TRUE);
	    handled = FALSE;
	    goto done;
	}
	loadnow  = (page ? TRUE : FALSE);
	startnow = FALSE;
	break;
    case YELP_PAGER_STATE_NEW:
    case YELP_PAGER_STATE_INVALID:
	startnow = TRUE;
	/* no break */
    case YELP_PAGER_STATE_STARTED:
    case YELP_PAGER_STATE_PARSING:
	priv->start_handler =
	    g_signal_connect (pager,
			      "start",
			      G_CALLBACK (pager_start_cb),
			      window);
	break;
    default:
	g_assert_not_reached ();
    }

    window_set_sections (window,
			 yelp_pager_get_sections (pager));

    if (loadnow) {
	window_handle_page (window, page);
	handled = TRUE;
	goto done;
    } else {
	window_set_loading (window);

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
	priv->cancel_handler =
	    g_signal_connect (pager,
			      "error",
			      G_CALLBACK (pager_cancel_cb),
			      window);
	priv->finish_handler =
	    g_signal_connect (pager,
			      "finish",
			      G_CALLBACK (pager_finish_cb),
			      window);

	if (startnow) {
	    handled = yelp_pager_start (pager);
	    if (handled) {
		yelp_toc_pager_pause (yelp_toc_pager_get ());
		priv->toc_pause++;
	    }
	}

	/* FIXME: error if !handled */
    }

 done:
    g_free (uri);

    return handled;
}

static gboolean
window_do_load_html (YelpWindow    *window,
		     YelpDocInfo   *doc_info,
		     gchar         *frag_id)
{
    YelpWindowPriv  *priv;
    GnomeVFSHandle  *handle;
    GnomeVFSResult   result;
    GnomeVFSFileSize n;
    gchar            buffer[BUFFER_SIZE];
    GtkAction       *action;

    gboolean  handled = TRUE;
    gchar    *uri;

    g_return_val_if_fail (YELP_IS_WINDOW (window), FALSE);
    g_return_val_if_fail (doc_info != NULL, FALSE);

    priv = window->priv;

    uri = yelp_doc_info_get_uri (doc_info, frag_id, YELP_URI_TYPE_FILE);

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

    result = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);

    if (result != GNOME_VFS_OK) {
	GError *error = NULL;
	g_set_error (&error, YELP_ERROR, YELP_ERROR_IO,
		     _("The file ‘%s’ could not be read.  This file might "
		       "be missing, or you might not have permissions to "
		       "read it."),
		     uri);
	window_error (window, error, TRUE);
	handled = FALSE;
	goto done;
    }

    yelp_html_set_base_uri (priv->html_view, uri);

    switch (yelp_doc_info_get_type (doc_info)) {
    case YELP_DOC_TYPE_HTML:
	yelp_html_open_stream (priv->html_view, "text/html");
	break;
    case YELP_DOC_TYPE_XHTML:
	yelp_html_open_stream (priv->html_view, "application/xhtml+xml");
	break;
    default:
	g_assert_not_reached ();
    }

    while ((result = gnome_vfs_read
	    (handle, buffer, BUFFER_SIZE, &n)) == GNOME_VFS_OK) {
	gchar *tmp;

	tmp = g_utf8_strup (buffer, n);
	if (strstr (tmp, "<FRAMESET"))
	    yelp_html_frames (priv->html_view, TRUE);
	g_free (tmp);

	yelp_html_write (priv->html_view, buffer, n);
    }

    yelp_html_close (priv->html_view);

 done:
    if (handle)
	gnome_vfs_close (handle);

    g_free (uri);

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
    gdk_window_set_cursor (GTK_WIDGET (window)->window, cursor);
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

    /*
    yelp_html_set_base_uri (priv->html_view, NULL);
    yelp_html_clear (priv->html_view);
    yelp_html_printf
	(priv->html_view,
	 "<html><head><meta http-equiv='Content-Type'"
	 " content='text/html=; charset=utf-8'>"
	 "<title>%s</title></head>"
	 "<body><center>%s</center></body>"
	 "</html>",
	 loading, loading);
    yelp_html_close (priv->html_view);
    */
}

static void
window_handle_page (YelpWindow   *window,
		    YelpPage     *page)
{
    YelpWindowPriv *priv;
    YelpPager      *pager;
    GtkAction      *action;
    GtkTreeModel   *model;
    GtkTreeIter     iter;

    gchar      *id;
    gchar      *uri;
    gboolean    valid;

    IdleWriterContext *context;

    g_return_if_fail (YELP_IS_WINDOW (window));
    priv = window->priv;
    window_disconnect (window);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  page->page_id  = \"%s\"\n", page->page_id);
    debug_print (DB_ARG, "  page->title    = \"%s\"\n", page->title);
    debug_print (DB_ARG, "  page->contents = %i bytes\n", strlen (page->contents));

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->side_sects));
    pager = yelp_doc_info_get_pager (priv->current_doc);

    if (model) {
	GtkTreeSelection *selection =
	    gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->side_sects));
	g_signal_handlers_block_by_func (selection,
					 tree_selection_changed_cb,
					 window);
	gtk_tree_selection_unselect_all (selection);

	valid = gtk_tree_model_get_iter_first (model, &iter);
	while (valid) {
	    gtk_tree_model_get (model, &iter,
				YELP_PAGER_COLUMN_ID, &id,
				-1);
	    if (yelp_pager_page_contains_frag (pager,
					       id,
					       priv->current_frag)) {
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

    priv->prev_id = page->prev_id;
    action = gtk_action_group_get_action (priv->action_group, "GoPrevious");
    if (action)
	g_object_set (G_OBJECT (action),
		      "sensitive",
		      priv->prev_id ? TRUE : FALSE,
		      NULL);
    priv->next_id = page->next_id;
    action = gtk_action_group_get_action (priv->action_group, "GoNext");
    if (action)
	g_object_set (G_OBJECT (action),
		      "sensitive",
		      priv->next_id ? TRUE : FALSE,
		      NULL);
    priv->toc_id = page->toc_id;
    action = gtk_action_group_get_action (priv->action_group, "GoContents");
    if (action)
	g_object_set (G_OBJECT (action),
		      "sensitive",
		      priv->toc_id ? TRUE : FALSE,
		      NULL);

    context = g_new0 (IdleWriterContext, 1);
    context->window = window;
    context->type   = IDLE_WRITER_MEMORY;
    context->buffer = page->contents;
    context->length = strlen (page->contents);

    uri = yelp_doc_info_get_uri (priv->current_doc,
				 priv->current_frag,
				 YELP_URI_TYPE_FILE);

    debug_print (DB_ARG, "  uri            = %s\n", uri);

    yelp_html_set_base_uri (priv->html_view, uri);
    yelp_html_open_stream (priv->html_view, "application/xhtml+xml");

    priv->idle_write = gtk_idle_add ((GtkFunction) idle_write, context);

    /*
    if (gnome_vfs_uri_get_fragment_identifier (uri->uri)) {
	yelp_html_jump_to_anchor
	    (priv->html_view,
	     (gchar *) gnome_vfs_uri_get_fragment_identifier (uri->uri));
    }
    */

    g_free (uri);
}

static void
window_disconnect (YelpWindow *window)
{
    YelpWindowPriv *priv;
    YelpPager      *pager = NULL;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;
    
    if (priv && priv->current_doc) {
	pager = yelp_doc_info_get_pager (priv->current_doc);
    }

    if (GTK_WIDGET (window)->window)
	gdk_window_set_cursor (GTK_WIDGET (window)->window, NULL);

    if (priv && priv->toc_pause > 0) {
	priv->toc_pause--;
	yelp_toc_pager_unpause (yelp_toc_pager_get ());
    }

    if (priv && priv->current_doc) {
	if (priv->start_handler) {
	    g_signal_handler_disconnect (pager,
					 priv->start_handler);
	    priv->start_handler = 0;
	}
	if (priv->page_handler) {
	    g_signal_handler_disconnect (pager,
					 priv->page_handler);
	    priv->page_handler = 0;
	}
	if (priv->error_handler) {
	    g_signal_handler_disconnect (pager,
					 priv->error_handler);
	    priv->error_handler = 0;
	}
	if (priv->cancel_handler) {
	    g_signal_handler_disconnect (pager,
					 priv->cancel_handler);
	    priv->cancel_handler = 0;
	}
	if (priv->finish_handler) {
	    g_signal_handler_disconnect (pager,
					 priv->finish_handler);
	    priv->finish_handler = 0;
	}
    }
    if (priv && priv->idle_write) {
	gtk_idle_remove (priv->idle_write);
	priv->idle_write = 0;
    }
}

/** Window Callbacks **********************************************************/

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

/** Pager Callbacks ***********************************************************/

static void
pager_start_cb (YelpPager   *pager,
		gpointer     user_data)
{
    YelpWindow  *window = YELP_WINDOW (user_data);
    GError      *error  = NULL;
    const gchar *page_id;

    page_id = yelp_pager_resolve_frag (pager,
				       window->priv->current_frag);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  page_id=\"%s\"\n", page_id);

    window_set_sections (window,
			 yelp_pager_get_sections (pager));

    if (!page_id && window->priv->current_frag &&
	strcmp (window->priv->current_frag, "")) {

	window_disconnect (window);

	g_set_error (&error, YELP_ERROR, YELP_ERROR_NO_PAGE,
		     _("The section ‘%s’ does not exist in this document. "
		       "If you were directed to this section from a Help "
		       "button in an application, please report this to "
		       "the maintainers of that application."),
		     window->priv->current_frag);
	window_error (window, error, TRUE);
    }
}

static void
pager_page_cb (YelpPager *pager,
	       gchar     *page_id,
	       gpointer   user_data)
{
    YelpWindow  *window = YELP_WINDOW (user_data);
    YelpPage    *page;

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  page_id=\"%s\"\n", page_id);

    if (yelp_pager_page_contains_frag (pager,
				       page_id,
				       window->priv->current_frag)) {
	page = (YelpPage *) yelp_pager_get_page (pager, page_id);

	/* now that yelp automatically inserts the id="index" attribute
	 * on the root element of a document, the _contains_frag function
	 * is no longer a good indication of whether a section exists.
	 * Therefore if the returned page is NULL, then the stylesheets
	 * were not able to create a "page" from this document through the
	 * exsl:document extension (see yelp_xslt_document())
	 * -Brent Smith, 1/4/2006 */
	if (page) {
	    window_disconnect (window);
	    window_handle_page (window, page);
	}
    }
}

static void
pager_error_cb (YelpPager   *pager,
		gpointer     user_data)
{
    YelpWindow *window = YELP_WINDOW (user_data);
    GError *error = yelp_pager_get_error (pager);

    debug_print (DB_FUNCTION, "entering\n");

    window_disconnect (window);
    window_error (window, error, TRUE);

    history_step_back (window);
}

static void
pager_cancel_cb (YelpPager   *pager,
		gpointer     user_data)
{
    YelpWindow *window = YELP_WINDOW (user_data);
    debug_print (DB_FUNCTION, "entering\n");

    window_disconnect (window);
}

static void
pager_finish_cb (YelpPager   *pager,
		 gpointer     user_data)
{
    GError *error = NULL;
    YelpWindow  *window = YELP_WINDOW (user_data);
    const gchar *page_id;
    
    page_id = yelp_pager_resolve_frag (pager,
				       window->priv->current_frag);

    debug_print (DB_FUNCTION, "entering\n");
    
    if (!page_id && window->priv->current_frag &&
	strcmp (window->priv->current_frag, "")) {

	window_disconnect (window);

	g_set_error (&error, YELP_ERROR, YELP_ERROR_NO_PAGE,
		     _("The section ‘%s’ does not exist in this document. "
		       "If you were directed to this section from a Help "
		       "button in an application, please report this to "
		       "the maintainers of that application."),
		     window->priv->current_frag);
	window_error (window, error, TRUE);
    }

    /* FIXME: Remove the URI from the history and go back */
}

/** Gecko Callbacks ***********************************************************/

static void
html_uri_selected_cb (YelpHtml  *html,
		      gchar     *uri,
		      gboolean   handled,
		      gpointer   user_data)
{
    YelpWindow *window = YELP_WINDOW (user_data);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  uri = \"%s\"\n", uri);

    if (!handled) {
	yelp_window_load (window, uri);
    }
}

static gboolean
html_frame_selected_cb (YelpHtml *html, gchar *uri, gboolean handled,
			gpointer user_data)
{
    YelpWindow *window = YELP_WINDOW (user_data);
    gboolean handle;
    YelpDocInfo *info = yelp_doc_info_get (uri, FALSE);
    switch (yelp_doc_info_get_type (info)) {
    case YELP_DOC_TYPE_HTML:
    case YELP_DOC_TYPE_XHTML:
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
			    YELP_PAGER_COLUMN_ID, &id,
			    -1);
	uri = yelp_doc_info_get_uri (priv->current_doc, id, YELP_URI_TYPE_ANY);
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
			    YELP_PAGER_COLUMN_ID, &id,
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
    gulong page_handler;
    gulong error_handler;
    gulong cancel_handler;
    gulong finish_handler;
    YelpPager *pager;
    YelpWindow *window;
} PrintStruct;

static void
print_disconnect (PrintStruct *data)
{
    debug_print (DB_FUNCTION, "entering\n");
    if (data->page_handler) {
	g_signal_handler_disconnect (data->pager,
				     data->page_handler);
	data->page_handler = 0;
    }
    if (data->error_handler) {
	g_signal_handler_disconnect (data->pager,
				     data->error_handler);
	data->error_handler = 0;
    }
    if (data->cancel_handler) {
	g_signal_handler_disconnect (data->pager,
				     data->cancel_handler);
	data->cancel_handler = 0;
    }
    if (data->finish_handler) {
	g_signal_handler_disconnect (data->pager,
				     data->finish_handler);
	data->finish_handler = 0;
    }
    g_free (data);
}

static void
print_pager_page_cb (YelpPager *pager,
		     gchar     *page_id,
		     gpointer   user_data)
{
    PrintStruct *data = user_data;
    YelpPage    *page;
    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  page_id=\"%s\"\n", page_id);

    page = (YelpPage *) yelp_pager_get_page (pager, page_id);

    if (page) {
	YelpHtml *html;
	GtkWidget *gtk_window;
	int length, offset;
	char *uri;
	GtkWidget *vbox = gtk_vbox_new (FALSE, FALSE);
	debug_print (DB_DEBUG, page->contents);

	gtk_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	html = yelp_html_new ();

	gtk_container_add (GTK_CONTAINER (gtk_window), GTK_WIDGET (vbox));
	gtk_box_pack_end (GTK_BOX (vbox), GTK_WIDGET (html), TRUE, TRUE, 0);

	gtk_widget_show (gtk_window);
	gtk_widget_show (GTK_WIDGET (html));
	gtk_widget_show (vbox);
	gtk_widget_hide (gtk_window);
	uri = yelp_doc_info_get_uri (yelp_pager_get_doc_info (pager),
				     page_id,
				     YELP_URI_TYPE_FILE);

	debug_print (DB_ARG, "  uri            = %s\n", uri);

	yelp_html_set_base_uri (html, uri);
	g_free (uri);

	yelp_html_open_stream (html, "application/xhtml+xml");
	for (length = strlen (page->contents), offset = 0; length > 0; length -= BUFFER_SIZE, offset += BUFFER_SIZE) {
	    debug_print (DB_DEBUG, "data: %.*s\n", MIN (length, BUFFER_SIZE), page->contents + offset);
	    yelp_html_write (html, page->contents + offset, MIN (length, BUFFER_SIZE));
	}
	yelp_html_close (html);

	yelp_print_run (data->window, html, gtk_window, vbox);

	print_disconnect (data);

    }
}

static void
print_pager_error_cb (YelpPager   *pager,
		      gpointer     user_data)
{
    PrintStruct *data = user_data;
    /*     GError *error = yelp_pager_get_error (pager);*/

    debug_print (DB_FUNCTION, "entering\n");

    print_disconnect (data);
}

static void
print_pager_cancel_cb (YelpPager   *pager,
		       gpointer     user_data)
{
    PrintStruct *data = user_data;
    debug_print (DB_FUNCTION, "entering\n");

    print_disconnect (data);
}

static void
print_pager_finish_cb (YelpPager   *pager,
		       gpointer     user_data)
{
    PrintStruct *data = user_data;

    debug_print (DB_FUNCTION, "entering\n");

    print_disconnect (data);
}

static void
window_print_document_cb (GtkAction *action, YelpWindow *window)
{
    PrintStruct *data;
    YelpPager *pager;

    if (!window->priv->current_doc)
	return;

    pager = yelp_db_print_pager_new (window->priv->current_doc);

    if (!pager) {
	return;
    }

    data = g_new0 (PrintStruct, 1);
    data->pager = pager;
    data->window = window;

    data->page_handler =
	g_signal_connect (data->pager,
			  "page",
			  G_CALLBACK (print_pager_page_cb),
			  data);
    data->error_handler =
	g_signal_connect (data->pager,
			  "error",
			  G_CALLBACK (print_pager_error_cb),
			  data);
    data->cancel_handler =
	g_signal_connect (data->pager,
			  "error",
			  G_CALLBACK (print_pager_cancel_cb),
			  data);
    data->finish_handler =
	g_signal_connect (data->pager,
			  "finish",
			  G_CALLBACK (print_pager_finish_cb),
			  data);

    /* handled = */ yelp_pager_start (data->pager);
}

static void
window_print_page_cb (GtkAction *action, YelpWindow *window)
{
    GtkWidget *gtk_win;
    YelpPager  *pager;
    YelpPage   *page  = NULL;
    YelpHtml *html;
    int length, offset;
    gchar *uri;
    GtkWidget *vbox = gtk_vbox_new (FALSE, FALSE);

    gtk_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    html = yelp_html_new ();
    
    gtk_container_add (GTK_CONTAINER (gtk_win), GTK_WIDGET (vbox));
    gtk_box_pack_end (GTK_BOX (vbox), GTK_WIDGET (html), TRUE, TRUE, 0);
    gtk_widget_show (gtk_win);
    gtk_widget_show (vbox);
    gtk_widget_show (GTK_WIDGET (html));
    gtk_widget_hide (gtk_win);

    pager = yelp_doc_info_get_pager (window->priv->current_doc);

    uri = yelp_doc_info_get_uri (window->priv->current_doc, NULL, YELP_URI_TYPE_FILE);
    
    yelp_html_set_base_uri (html, uri);

    if (pager) {
	page = (YelpPage *) yelp_pager_get_page (pager, window->priv->current_frag);
	
	yelp_html_open_stream (html, "application/xhtml+xml");
	for (length = strlen (page->contents), offset = 0; length > 0; length -= BUFFER_SIZE, offset += BUFFER_SIZE) {
	    debug_print (DB_DEBUG, "data: %.*s\n", MIN (length, BUFFER_SIZE), page->contents + offset);
	    yelp_html_write (html, page->contents + offset, MIN (length, BUFFER_SIZE));
	}
	yelp_html_close (html);
    } else { /*html file.  Dump file to window the easy way*/
	GnomeVFSHandle  *handle;
	GnomeVFSResult   result;
	GnomeVFSFileSize n;
	gchar            buffer[BUFFER_SIZE];	

	result = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);
	
	if (result != GNOME_VFS_OK) {
	    GError *error = NULL;
	    g_set_error (&error, YELP_ERROR, YELP_ERROR_IO,
			 _("The file ‘%s’ could not be read.  This file might "
			   "be missing, or you might not have permissions to "
			   "read it."),
			 uri);
	    window_error (window, error, TRUE);
	    return;
	}
	/* Assuming the file exists.  If it doesn't how did we get this far?
	 * There are more sinister forces at work...
	 */

	switch (yelp_doc_info_get_type (window->priv->current_doc)) {
	case YELP_DOC_TYPE_HTML:
	    yelp_html_open_stream (html, "text/html");
	    break;
	case YELP_DOC_TYPE_XHTML:
	    yelp_html_open_stream (html, "application/xhtml+xml");
	    break;
	default:
	    g_assert_not_reached ();
	}
	
	while ((result = gnome_vfs_read
		(handle, buffer, BUFFER_SIZE, &n)) == GNOME_VFS_OK) {
	    yelp_html_write (html, buffer, n);
	}
	
	yelp_html_close (html);

	
    
    }
    g_free (uri);
    yelp_print_run (window, html, gtk_win, vbox);
}

static void
window_about_document_cb (GtkAction *action, YelpWindow *window)
{
    YelpWindowPriv *priv;
    gchar *uri;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

    uri = yelp_doc_info_get_uri (priv->current_doc,
				 "x-yelp-titlepage",
				 YELP_URI_TYPE_ANY);
    yelp_window_load (window, uri);
    g_free (uri);
}

static void
window_open_location_cb (GtkAction *action, YelpWindow *window)
{
    YelpWindowPriv *priv;
    GladeXML       *glade;
    GtkWidget      *dialog;
    GtkWidget      *entry;
    gchar          *uri;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

    if (priv->current_doc) {
	uri = yelp_doc_info_get_uri (priv->current_doc,
				     priv->current_frag,
				     YELP_URI_TYPE_NO_FILE);
	if (!uri)
	    uri = yelp_doc_info_get_uri (priv->current_doc,
					 priv->current_frag,
					 YELP_URI_TYPE_FILE);
    } else {
	uri = NULL;
    }

    glade = glade_xml_new (DATADIR "/yelp/ui/yelp.glade",
			   "location_dialog",
			   NULL);
    if (!glade) {
	g_warning ("Could not find necessary glade file "
		   DATADIR "/yelp/ui/yelp.glade");
	return;
    }

    dialog = glade_xml_get_widget (glade, "location_dialog");
    entry  = glade_xml_get_widget (glade, "location_entry");

    priv->location_dialog = dialog;
    priv->location_entry  = entry;

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

    g_object_unref (glade);
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
    YelpPager *pager;

    g_return_if_fail (YELP_IS_WINDOW (window));

    debug_print (DB_FUNCTION, "entering\n");

    if (window->priv->current_doc) {
	YelpLoadData *data;
	gchar *uri;
	pager = yelp_doc_info_get_pager (window->priv->current_doc);

	if (!pager)
	    return;

	yelp_pager_cancel (pager);

	uri = yelp_doc_info_get_uri (window->priv->current_doc,
				     window->priv->current_frag,
				     YELP_URI_TYPE_ANY);
	data = g_new0 (YelpLoadData, 1);
	data->window = g_object_ref (window);
	data->uri = uri;
	g_idle_add ((GSourceFunc) window_load_async, data);
    }
}

static void
window_enable_cursor_cb (GtkAction *action, YelpWindow *window)
{
    gboolean cursor;
    GConfClient *gconf_client = gconf_client_get_default ();

    cursor = gconf_client_get_bool (gconf_client,
				    "/apps/yelp/use_caret",
				    NULL);
    gconf_client_set_bool (gconf_client,
			   "/apps/yelp/use_caret",
			   !cursor,
			   NULL);
}

static gboolean
window_load_async (YelpLoadData *data)
{
    yelp_window_load (data->window, data->uri);

    load_data_free (data);

    return FALSE;
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

    if (priv->current_doc)
	yelp_doc_info_unref (priv->current_doc);
    if (priv->current_frag)
	g_free (priv->current_frag);

    priv->current_doc  = yelp_doc_info_ref (entry->doc_info);
    priv->current_frag = g_strdup (entry->frag_id);

    window_do_load (window, entry->doc_info, entry->frag_id);

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

    if (priv->current_doc)
	yelp_doc_info_unref (priv->current_doc);
    if (priv->current_frag)
	g_free (priv->current_frag);

    priv->current_doc  = yelp_doc_info_ref (entry->doc_info);
    priv->current_frag = g_strdup (entry->frag_id);

    window_do_load (window, entry->doc_info, entry->frag_id);

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
}

static void
window_go_next_cb (GtkAction *action, YelpWindow *window)
{
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
}

static void
window_go_toc_cb (GtkAction *action, YelpWindow *window)
{
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
}

static void
window_add_bookmark_cb (GtkAction *action, YelpWindow *window)
{
    gchar *uri;
    YelpWindowPriv *priv = window->priv;

    debug_print (DB_FUNCTION, "entering\n");

    uri = yelp_doc_info_get_uri (priv->current_doc, priv->current_frag,
				 YELP_URI_TYPE_NO_FILE);
    if (!uri)
	uri = yelp_doc_info_get_uri (priv->current_doc, priv->current_frag,
				     YELP_URI_TYPE_FILE);

    if (!uri)
	return;

    yelp_bookmarks_add (uri, window);

    g_free (uri);
}

static void window_copy_link_cb (GtkAction *action, YelpWindow *window) 
{
    gtk_clipboard_set_text (gtk_clipboard_get (gdk_atom_intern ("CLIPBOARD", 
								TRUE)),
			    window->priv->uri,
			    -1);
    g_free (window->priv->uri);
}

static void
window_open_link_cb (GtkAction *action, YelpWindow *window)
{
    yelp_window_load (window, window->priv->uri);
    g_free (window->priv->uri);
}

static void
window_open_link_new_cb (GtkAction *action, YelpWindow *window)
{
    g_signal_emit (window, signals[NEW_WINDOW_REQUESTED], 0,
		   window->priv->uri);
    g_free (window->priv->uri);
}

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
    GnomeProgram *program = gnome_program_get ();
    GError *error = NULL;

    g_return_if_fail (YELP_IS_WINDOW (window));

    gnome_help_display_desktop (program, "user-guide", 
                                "user-guide", "yelp", &error);

    if (error) {
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
 	                                 0,
	                                 GTK_MESSAGE_ERROR,
	                                 GTK_BUTTONS_CLOSE,
	                                 _("Could not display help for Yelp.\n"
	                                 "%s"),
	                                 error->message);

	g_signal_connect_swapped (dialog, "response",
	                          G_CALLBACK (gtk_widget_destroy),
	                          dialog);
	gtk_widget_show (dialog);

	g_error_free (error);
    }
}

static void
window_about_cb (GtkAction *action, YelpWindow *window)
{
    const gchar *copyright =
	"Copyright © 2001-2003 Mikael Hallendal\n"
	"Copyright © 2003-2005 Shaun McCance\n"
	"Copyright © 2005-2006 Don Scorgie\n"
	"Copyright © 2005-2006 Brent Smith";
    const gchar *authors[] = { 
	"Mikael Hallendal <micke@imendio.com>",
	"Alexander Larsson <alexl@redhat.com>",
	"Shaun McCance <shaunm@gnome.org>",
	"Don Scorgie <DonScorgie@Blueyonder.co.uk>",
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
			   "logo-icon-name", "gnome-help",
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
	    window_find_buttons_set_sensitive (window, FALSE, TRUE);
	} else {
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

/* Writing incrementally in an idle function has a number of advantages.  First,
 * it keeps the interface responsive for really big pages.  Second, it prevents
 * some weird rendering artifacts in gtkhtml2.  Third, and most important, it
 * helps me beat the relayout race condition that I discuss at length in a big
 * comment in yelp-html-gtkhtml2.c.
 */

static gboolean
idle_write (IdleWriterContext *context)
{
    YelpWindowPriv *priv;

    g_return_val_if_fail (context != NULL, FALSE);
    g_return_val_if_fail (context->window != NULL, FALSE);

    debug_print (DB_FUNCTION, "entering\n");

    priv = context->window->priv;

    switch (context->type) {
    case IDLE_WRITER_MEMORY:
	debug_print (DB_DEBUG, "  context->buffer = %i bytes\n", strlen (context->buffer));
	debug_print (DB_DEBUG, "  context->cur    = %i\n", context->cur);
	debug_print (DB_DEBUG, "  context->length = %i\n", context->length);

	if (context->cur + BUFFER_SIZE < context->length) {
	    yelp_html_write (priv->html_view,
			     context->buffer + context->cur,
			     BUFFER_SIZE);
	    context->cur += BUFFER_SIZE;
	    return TRUE;
	} else {
	    if (context->length > context->cur)
		yelp_html_write (priv->html_view,
				 context->buffer + context->cur,
				 context->length - context->cur);
	    yelp_html_close (priv->html_view);
	    g_free (context);
	    priv->idle_write = 0;
	    return FALSE;
	}
	break;
    case IDLE_WRITER_VFS:
    default:
	g_assert_not_reached ();
    }

    return FALSE;
}

