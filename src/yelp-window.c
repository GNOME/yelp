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

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <bonobo/bonobo-main.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-url.h>
#include <libgnome/gnome-program.h>
#include <libgnome/gnome-config.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <string.h>

#include "yelp-cache.h"
#include "yelp-db-pager.h"
#include "yelp-error.h"
#include "yelp-html.h"
#include "yelp-pager.h"
#include "yelp-settings.h"
#include "yelp-toc-pager.h"
#include "yelp-window.h"

#ifdef ENABLE_MAN
#include "yelp-man-pager.h"
#endif

#ifdef YELP_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define YELP_CONFIG_WIDTH          "/yelp/Geometry/width"
#define YELP_CONFIG_HEIGHT         "/yelp/Geometry/height"
#define YELP_CONFIG_WIDTH_DEFAULT  "600"
#define YELP_CONFIG_HEIGHT_DEFAULT "420"

#define BUFFER_SIZE 16384

typedef struct {
    YelpDocInfo *doc_info;

    gchar *frag_id;

    gchar *page_title;
    gchar *frag_title;
} YelpHistoryEntry;

typedef enum {
    YELP_WINDOW_FIND_PREV = 1,
    YELP_WINDOW_FIND_NEXT
} YelpFindAction;


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
static void        yelp_window_destroyed          (GtkWidget         *window,
						   gpointer           user_data);
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

/** UIManager Callbacks **/
static void    window_add_widget        (GtkUIManager *ui_manager,
					 GtkWidget    *widget,
					 GtkWidget    *vbox);
static void    window_new_window_cb     (GtkAction *action, YelpWindow *window);
static void    window_open_location_cb  (GtkAction *action, YelpWindow *window);
static void    window_close_window_cb   (GtkAction *action, YelpWindow *window);
static void    window_copy_cb           (GtkAction *action, YelpWindow *window);
static void    window_select_all_cb     (GtkAction *action, YelpWindow *window);
static void    window_find_cb           (GtkAction *action, YelpWindow *window);
static void    window_preferences_cb    (GtkAction *action, YelpWindow *window);
static void    window_reload_cb         (GtkAction *action, YelpWindow *window);
static void    window_go_back_cb        (GtkAction *action, YelpWindow *window);
static void    window_go_forward_cb     (GtkAction *action, YelpWindow *window);
static void    window_go_home_cb        (GtkAction *action, YelpWindow *window);
static void    window_go_previous_cb    (GtkAction *action, YelpWindow *window);
static void    window_go_next_cb        (GtkAction *action, YelpWindow *window);
static void    window_go_toc_cb         (GtkAction *action, YelpWindow *window);
static void    window_about_cb          (GtkAction *action, YelpWindow *window);
static void    window_copy_link_cb      (GtkAction *action, YelpWindow *window);
static void    window_open_link_cb      (GtkAction *action, YelpWindow *window);
static void    window_open_link_new_cb  (GtkAction *action, YelpWindow *window);

/** History Functions **/
static void               history_push_back      (YelpWindow       *window);
static void               history_push_forward   (YelpWindow       *window);
static void               history_clear_forward  (YelpWindow       *window);
static void               history_step_back      (YelpWindow       *window);
static YelpHistoryEntry * history_pop_back       (YelpWindow       *window);
static YelpHistoryEntry * history_pop_forward    (YelpWindow       *window);
static void               history_entry_free     (YelpHistoryEntry *entry);

static void               location_response_cb    (GtkDialog       *dialog,
						   gint             id,
						   YelpWindow      *window);

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
static void        window_find_save_settings      (YelpWindow        *window);
static void        window_find_buttons_set_sensitive (YelpWindow      *window,
						      gboolean        sensitive);
static void        window_find_clicked_cb         (GtkWidget         *button,
						   YelpWindow        *window);

static gboolean    tree_model_iter_following      (GtkTreeModel      *model,
						   GtkTreeIter       *iter);

static GConfClient *gconf_client = NULL;

enum {
    NEW_WINDOW_REQUESTED,
    LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };

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

    /* Open Location */
    GtkWidget      *location_dialog;
    GtkWidget      *location_entry;

    /* Popup menu*/
    GtkWidget      *popup;
    gchar          *uri;

    /* Location Information */
    YelpDocInfo    *current_doc;
    gchar          *current_frag;
    GSList         *history_back;
    GSList         *history_forward;

    /* Callbacks and Idles */
    gulong          start_handler;
    gulong          page_handler;
    gulong          error_handler;
    gulong          cancel_handler;
    gulong          finish_handler;
    guint           idle_write;

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

static GtkTargetEntry row_targets[] = {
    { TARGET_TYPE_URI_LIST, 0, TARGET_URI_LIST }
};

static GtkActionEntry entries[] = {
    { "FileMenu", NULL, N_("_File") },
    { "EditMenu", NULL, N_("_Edit") },
    { "GoMenu",   NULL, N_("_Go")   },
    { "HelpMenu", NULL, N_("_Help") },

    { "NewWindow", GTK_STOCK_NEW,
      N_("_New window"),
      "<Control>N",
      NULL,
      G_CALLBACK (window_new_window_cb) },
    { "OpenLocation", NULL,
      N_("Open _Location"),
      "<Control>L",
      NULL,
      G_CALLBACK (window_open_location_cb) },
    { "CloseWindow", GTK_STOCK_CLOSE,
      N_("_Close window"),
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
    { "Preferences", GTK_STOCK_PREFERENCES,
      N_("_Preferences"),
      NULL, NULL,
      G_CALLBACK (window_preferences_cb) },

    { "Reload", NULL,
      N_("_Reload"),
      "<Control>R",
      NULL,
      G_CALLBACK (window_reload_cb) },

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
      N_("_Home"),
      NULL,
      N_("Go to home view"),
      G_CALLBACK (window_go_home_cb) },
    { "GoPrevious", NULL,
      N_("_Previous Page"),
      "<Alt>Up",
      NULL,
      G_CALLBACK (window_go_previous_cb) },
    { "GoNext", NULL,
      N_("_Next Page"),
      "<Alt>Down",
      NULL,
      G_CALLBACK (window_go_next_cb) },
    { "GoContents", NULL,
      N_("_Contents"),
      NULL,
      NULL,
      G_CALLBACK (window_go_toc_cb) },
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

    { "About", GNOME_STOCK_ABOUT,
      N_("_About"),
      NULL,
      NULL,
      G_CALLBACK (window_about_cb) }
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
    if (!gconf_client)
	gconf_client = gconf_client_get_default ();

    signals[NEW_WINDOW_REQUESTED] =
	g_signal_new ("new_window_requested",
		      G_TYPE_FROM_CLASS (klass),
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (YelpWindowClass,
				       new_window_requested),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__STRING,
		      G_TYPE_NONE, 1, G_TYPE_STRING);
}

/** History Functions *********************************************************/

static void
history_push_back (YelpWindow *window)
{
    YelpWindowPriv   *priv;
    YelpHistoryEntry *entry;
    GtkAction        *action;

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
}

static void
history_push_forward (YelpWindow *window)
{
    YelpWindowPriv   *priv;
    YelpHistoryEntry *entry;
    GtkAction        *action;

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
    }
}

static YelpHistoryEntry *
history_pop_back (YelpWindow *window)
{
    YelpWindowPriv   *priv;
    YelpHistoryEntry *entry;
    GtkAction        *action;

    g_return_val_if_fail (YELP_IS_WINDOW (window), NULL);
    g_return_val_if_fail (window->priv->history_back != NULL, NULL);

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
    g_return_val_if_fail (window->priv->history_forward != NULL, NULL);

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

    g_free (entry);
}

/******************************************************************************/

GtkWidget *
yelp_window_new (GNode *doc_tree, GList *index)
{
    YelpWindow     *window;
    YelpWindowPriv *priv;

    window = g_object_new (YELP_TYPE_WINDOW, NULL);

    priv   = window->priv;

    d (g_print ("yelp_window_new\n"));

    window_populate (window);

    g_signal_connect (G_OBJECT (window), "destroy",
		      G_CALLBACK (yelp_window_destroyed),
		      NULL);

    return GTK_WIDGET (window);
}

void
yelp_window_load (YelpWindow *window, gchar *uri)
{
    YelpWindowPriv *priv;
    YelpDocInfo    *doc_info;
    gchar          *frag_id;

    g_return_if_fail (YELP_IS_WINDOW (window));

    d (g_print ("yelp_window_load\n"));
    d (g_print ("  uri = \"%s\"\n", uri));

    if (!uri) {
	GError *error = NULL;
	yelp_set_error (&error, YELP_ERROR_NO_DOC);
	window_error (window, error, FALSE);
	g_error_free (error);
	return;
    }

    priv = window->priv;

    doc_info = yelp_doc_info_get (uri);
    if (!doc_info) {
	GError *error = NULL;
	yelp_set_error (&error, YELP_ERROR_NO_DOC);
	window_error (window, error, FALSE);
	g_error_free (error);
	return;
    }
    frag_id  = yelp_uri_get_fragment (uri);

    if (priv->current_doc && yelp_doc_info_equal (priv->current_doc, doc_info)) {
	if (priv->current_frag) {
	    if (frag_id && g_str_equal (priv->current_frag, frag_id))
		goto done;
	}
	else if (!frag_id)
	    goto done;
    }

    if (priv->current_doc)
	history_push_back (window);
    history_clear_forward (window);

    if (priv->current_doc)
	yelp_doc_info_unref (priv->current_doc);
    if (priv->current_frag)
	g_free (priv->current_frag);

    priv->current_doc  = yelp_doc_info_ref (doc_info);
    priv->current_frag = g_strdup (frag_id);

    window_do_load (window, doc_info, frag_id);

 done:
    if (priv->current_frag != frag_id)
	g_free (frag_id);
}

YelpDocInfo *
yelp_window_get_doc_info (YelpWindow *window)
{
    g_return_val_if_fail (YELP_IS_WINDOW (window), NULL);

    return window->priv->current_doc;
}

static void
window_do_load (YelpWindow  *window,
		YelpDocInfo *doc_info,
		gchar       *frag_id)
{
    YelpWindowPriv *priv;
    GError         *error = NULL;
    gboolean        handled = FALSE;
    gchar          *uri;

    g_return_if_fail (YELP_IS_WINDOW (window));
    g_return_if_fail (doc_info != NULL);

    priv = window->priv;

    switch (yelp_doc_info_get_type (doc_info)) {
    case YELP_DOC_TYPE_DOCBOOK_XML:
    case YELP_DOC_TYPE_MAN:
    case YELP_DOC_TYPE_INFO:
    case YELP_DOC_TYPE_TOC:
	handled = window_do_load_pager (window, doc_info, frag_id);
	break;
    case YELP_DOC_TYPE_DOCBOOK_SGML:
	yelp_set_error (&error, YELP_ERROR_NO_SGML);
	break;
    case YELP_DOC_TYPE_HTML:
	handled = window_do_load_html (window, doc_info, frag_id);
	break;
    case YELP_DOC_TYPE_EXTERNAL:
	history_step_back (window);
	uri = yelp_doc_info_get_uri (doc_info, NULL, YELP_URI_TYPE_ANY);
	gnome_url_show (uri, &error);
	g_free (uri);
	break;
    case YELP_DOC_TYPE_ERROR:
    default:
	yelp_set_error (&error, YELP_ERROR_NO_DOC);
	break;
    }
 
    if (error) {
	window_error (window, error, TRUE);
	g_error_free (error);
    }

    window_find_buttons_set_sensitive (window, TRUE);    
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
    YelpWindowPriv    *priv;
    GtkAccelGroup     *accel_group;
    GError            *error = NULL;
    GtkAction         *action;
    GtkTreeSelection  *selection;

    priv = window->priv;

    priv->main_box = gtk_vbox_new (FALSE, 0);

    gtk_container_add (GTK_CONTAINER (window), priv->main_box);

    priv->action_group = gtk_action_group_new ("MenuActions");
    gtk_action_group_set_translation_domain (priv->action_group,
					     GETTEXT_PACKAGE);
    gtk_action_group_add_actions (priv->action_group,
				  entries, G_N_ELEMENTS (entries),
				  window);
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
	g_error_free (error);
    }

    gtk_ui_manager_ensure_update (priv->ui_manager);

    action = gtk_action_group_get_action (priv->action_group, "GoBack");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
    action = gtk_action_group_get_action (priv->action_group, "GoForward");
    if (action)
	g_object_set (G_OBJECT (action), "sensitive", FALSE, NULL);
    priv->popup = gtk_ui_manager_get_widget(priv->ui_manager, "ui/popup");

    priv->pane = gtk_hpaned_new ();
    gtk_widget_ref (priv->pane);
    // We should probably remember the last position and set to that.
    gtk_paned_set_position (GTK_PANED (priv->pane), 180);

    priv->side_sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_ref (priv->side_sw);
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
	 "text", 1,
	 NULL);

    gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (priv->side_sects),
					    GDK_BUTTON1_MASK,
					    row_targets,
					    G_N_ELEMENTS (row_targets),
					    GDK_ACTION_LINK);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->side_sects));

    g_signal_connect (selection, "changed",
		      G_CALLBACK (tree_selection_changed_cb),
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
    gtk_box_pack_start (GTK_BOX (priv->html_pane),
			priv->find_bar,
			FALSE, FALSE, 0);

    priv->html_view  = yelp_html_new ();
    g_signal_connect (priv->html_view,
		      "uri_selected",
		      G_CALLBACK (html_uri_selected_cb),
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
		      yelp_html_get_widget (priv->html_view),
		      TRUE, TRUE, 0);

    gtk_paned_add2     (GTK_PANED (priv->pane),
			priv->html_pane);
    gtk_box_pack_start (GTK_BOX (priv->main_box),
			priv->pane,
			TRUE, TRUE, 0);

    gtk_widget_show_all (yelp_html_get_widget (priv->html_view));
    gtk_widget_show (priv->html_pane);
    gtk_widget_show (priv->pane);
    gtk_widget_show (priv->main_box);
}

static void
window_populate_find (YelpWindow *window, GtkWidget *find_bar)
{
    GtkWidget *label;
    GtkToolItem *item;
    YelpWindowPriv *priv = window->priv;
    
    g_return_if_fail (GTK_IS_TOOLBAR (find_bar));

    label = gtk_label_new_with_mnemonic (_("Fin_d"));
    item = gtk_tool_item_new ();
    gtk_container_add (GTK_CONTAINER (item), label);
    gtk_toolbar_insert (GTK_TOOLBAR (find_bar), item, -1);

    priv->find_entry = gtk_entry_new ();
    g_signal_connect (G_OBJECT (priv->find_entry), "changed",
		      G_CALLBACK (window_find_entry_changed_cb), window);	
    item = gtk_tool_item_new ();
    gtk_container_add (GTK_CONTAINER (item), priv->find_entry);
    gtk_toolbar_insert (GTK_TOOLBAR (find_bar), item, -1);

    gtk_label_set_mnemonic_widget (GTK_LABEL (label), priv->find_entry);

    priv->find_next = gtk_tool_button_new_from_stock (GTK_STOCK_FIND);
    gtk_tool_item_set_is_important (item, TRUE);
    g_signal_connect (priv->find_next,
		      "clicked",
		      G_CALLBACK (window_find_clicked_cb),
		      window);
    gtk_toolbar_insert (GTK_TOOLBAR (find_bar), priv->find_next, -1);

    priv->find_prev = gtk_tool_button_new_from_stock (GTK_STOCK_FIND);
    gtk_tool_item_set_is_important (item, TRUE);
    g_signal_connect (priv->find_prev,
		      "clicked",
		      G_CALLBACK (window_find_clicked_cb),
		      window);
    gtk_toolbar_insert (GTK_TOOLBAR (find_bar), priv->find_prev, -1);

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
#ifdef ENABLE_INFO
	case YELP_DOC_TYPE_INFO:
	    // FIXME: yelp_info_pager_new (doc_info);
	    break;
#endif
#ifdef ENABLE_MAN
	case YELP_DOC_TYPE_MAN:
	    pager = yelp_man_pager_new (doc_info);
	    break;
#endif
	case YELP_DOC_TYPE_TOC:
	    pager = YELP_PAGER (yelp_toc_pager_get ());
	    break;
	default:
	    break;
	}
	if (pager)
	    yelp_doc_info_set_pager (doc_info, pager);
    }

    if (!pager) {
	yelp_set_error (&error, YELP_ERROR_NO_DOC);
	window_error (window, error, TRUE);
	g_error_free (error);
	handled = FALSE;
	goto done;
    }

    g_object_ref (pager);

    loadnow  = FALSE;
    startnow = FALSE;

    state = yelp_pager_get_state (pager);
    switch (state) {
    case YELP_PAGER_STATE_ERROR:
	error = yelp_pager_get_error (pager);
	if (error)
	    window_error (window, error, TRUE);
	handled = FALSE;
	goto done;
	break;
    case YELP_PAGER_STATE_RUNNING:
    case YELP_PAGER_STATE_FINISHED:
	/* Check if the page exists */
	pager_start_cb (pager, window);

	page = (YelpPage *) yelp_pager_get_page (pager, frag_id);
	if (!page && (state == YELP_PAGER_STATE_FINISHED)) {
	    yelp_set_error (&error, YELP_ERROR_NO_PAGE);
	    window_error (window, error, TRUE);
	    g_error_free (error);
	    handled = FALSE;
	    goto done;
	}
	loadnow  = (page ? TRUE : FALSE);
	startnow = FALSE;
	break;
    case YELP_PAGER_STATE_NEW:
    case YELP_PAGER_STATE_INVALID:
	startnow = TRUE;
	// no break
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

	if (startnow)
	    handled = yelp_pager_start (pager);

	// FIXME: error if !handled
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
	yelp_set_error (&error, YELP_ERROR_NO_DOC);
	window_error (window, error, TRUE);
	g_error_free (error);
	handled = FALSE;
	goto done;
    }

    yelp_html_set_base_uri (priv->html_view, uri);
    yelp_html_clear (priv->html_view);

    while ((result = gnome_vfs_read
	    (handle, buffer, BUFFER_SIZE, &n)) == GNOME_VFS_OK) {
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

    d (g_print ("window_handle_page\n"));
    d (g_print ("  page->page_id  = \"%s\"\n", page->page_id));
    d (g_print ("  page->title    = \"%s\"\n", page->title));
    d (g_print ("  page->contents = %i bytes\n", strlen (page->contents)));

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->side_sects));
    pager = yelp_doc_info_get_pager (priv->current_doc);

    if (model) {
	valid = gtk_tree_model_get_iter_first (model, &iter);
	while (valid) {
	    gtk_tree_model_get (model, &iter,
				0, &id,
				-1);
	    if (yelp_pager_page_contains_frag (pager,
					       id,
					       priv->current_frag)) {
		GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
		GtkTreeSelection *selection =
		    gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->side_sects));

		g_signal_handlers_block_by_func (selection,
						 tree_selection_changed_cb,
						 window);

		gtk_tree_view_expand_to_path (GTK_TREE_VIEW (priv->side_sects),
					      path);
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->side_sects),
					  path, NULL, FALSE);

		g_signal_handlers_unblock_by_func (selection,
						   tree_selection_changed_cb,
						   window);
		gtk_tree_path_free (path);
		g_free (id);
		break;
	    }

	    g_free (id);

	    valid = tree_model_iter_following (model, &iter);
	}
    }

    priv->prev_id = page->prev_id;
    action = gtk_action_group_get_action (priv->action_group, "GoPrevious");
    if (action)
	g_object_set (G_OBJECT (action),
		      "sensitive",
		      priv->prev_id ? TRUE : FALSE,
		      NULL);
    priv->prev_id = page->prev_id;
    action = gtk_action_group_get_action (priv->action_group, "GoNext");
    if (action)
	g_object_set (G_OBJECT (action),
		      "sensitive",
		      priv->next_id ? TRUE : FALSE,
		      NULL);
    priv->prev_id = page->prev_id;
    action = gtk_action_group_get_action (priv->action_group, "GoContents");
    if (action)
	g_object_set (G_OBJECT (action),
		      "sensitive",
		      priv->toc_id ? TRUE : FALSE,
		      NULL);

    gtk_window_set_title (GTK_WINDOW (window),
			  (const gchar *) page->title);

    context = g_new0 (IdleWriterContext, 1);
    context->window = window;
    context->type   = IDLE_WRITER_MEMORY;
    context->buffer = page->contents;
    context->length = strlen (page->contents);

    uri = yelp_doc_info_get_uri (priv->current_doc,
				 page->page_id,
				 YELP_URI_TYPE_FILE);

    d (g_print ("  uri            = %s\n", uri));

    yelp_html_set_base_uri (priv->html_view, uri);
    yelp_html_clear (priv->html_view);

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
    YelpPager      *pager;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;
    pager = yelp_doc_info_get_pager (priv->current_doc);

    if (GTK_WIDGET (window)->window)
	gdk_window_set_cursor (GTK_WIDGET (window)->window, NULL);

    if (window->priv->current_doc) {
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
    if (priv->idle_write) {
	gtk_idle_remove (priv->idle_write);
	priv->idle_write = 0;
    }
}

/** Window Callbacks **********************************************************/

static void
yelp_window_destroyed (GtkWidget *window,
		       gpointer   user_data)
{
    YelpWindowPriv *priv;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = YELP_WINDOW(window)->priv;

    window_disconnect (YELP_WINDOW (window));

    if (priv->current_doc)
	yelp_doc_info_unref (priv->current_doc);
    g_free (priv->current_frag);

    g_object_unref (priv->pane);
    g_object_unref (priv->side_sw);
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

    d (g_print ("pager_start_cb\n"));
    d (g_print ("  page_id=\"%s\"\n", page_id));

    if (!page_id && (window->priv->current_frag && strcmp (window->priv->current_frag, ""))) {
	window_disconnect (window);

	yelp_set_error (&error, YELP_ERROR_NO_PAGE);
	window_error (window, error, TRUE);
	g_error_free (error);
    }
}

static void
pager_page_cb (YelpPager *pager,
	       gchar     *page_id,
	       gpointer   user_data)
{
    YelpWindow  *window = YELP_WINDOW (user_data);
    YelpPage    *page;

    d (g_print ("pager_page_cb\n"));
    d (g_print ("  page_id=\"%s\"\n", page_id));

    if (yelp_pager_page_contains_frag (pager,
				       page_id,
				       window->priv->current_frag)) {
	window_disconnect (window);

	page = (YelpPage *) yelp_pager_get_page (pager, page_id);
	window_handle_page (window, page);
    }
}

static void
pager_error_cb (YelpPager   *pager,
		gpointer     user_data)
{
    YelpWindow *window = YELP_WINDOW (user_data);
    GError *error = yelp_pager_get_error (pager);

    d (g_print ("pager_error_cb\n"));

    window_disconnect (window);
    window_error (window, error, TRUE);

    g_error_free (error);

    history_step_back (window);
}

static void
pager_cancel_cb (YelpPager   *pager,
		gpointer     user_data)
{
    YelpWindow *window = YELP_WINDOW (user_data);
    d (g_print ("pager_cancel_cb\n"));

    window_disconnect (window);
}

static void
pager_finish_cb (YelpPager   *pager,
		 gpointer     user_data)
{
    GError *error = NULL;
    YelpWindow  *window = YELP_WINDOW (user_data);

    d (g_print ("pager_finish_cb\n"));

    window_disconnect (window);

    yelp_set_error (&error, YELP_ERROR_NO_PAGE);
    window_error (window, error, TRUE);

    g_error_free (error);

    // FIXME: Remove the URI from the history and go back
}

/** Gecko Callbacks ***********************************************************/

static void
html_uri_selected_cb (YelpHtml  *html,
		      gchar     *uri,
		      gboolean   handled,
		      gpointer   user_data)
{
    YelpWindow *window = YELP_WINDOW (user_data);

    d (g_print ("html_uri_selected_cb\n"));
    d (g_print ("  uri = \"%s\"\n", uri));

    if (!handled) {
	yelp_window_load (window, uri);
    }
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

    window->priv->uri = g_strdup (uri);
    gtk_menu_popup (window->priv->popup,
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
			    0, &id,
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
    gchar *id, *uri;

    g_return_if_fail (YELP_IS_WINDOW (window));

    d (g_print ("tree_drag_data_get_cb\n"));

    priv = window->priv;

    tree_selection =
	gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->side_sects));

    if (gtk_tree_selection_get_selected (tree_selection, NULL, &iter)) {
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->side_sects));
	gtk_tree_model_get (model, &iter,
			    0, &id,
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
				8, uri, strlen (uri));
	break;
    default:
	g_assert_not_reached ();
    }

    g_free (uri);
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

static void
window_open_location_cb (GtkAction *action, YelpWindow *window)
{
    YelpWindowPriv *priv;
    GladeXML       *glade;
    GtkWidget      *dialog;
    GtkWidget      *entry;

    g_return_if_fail (YELP_IS_WINDOW (window));

    priv = window->priv;

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

    d (g_print ("window_find_cb\n"));

    gtk_widget_show_all (priv->find_bar);
    gtk_widget_grab_focus (priv->find_entry);
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

    d (g_print ("window_reload_cb\n"));

    if (window->priv->current_doc) {
	pager = yelp_doc_info_get_pager (window->priv->current_doc);

	if (!pager)
	    return;

	yelp_pager_cancel (pager);
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
    d (g_print ("window_go_home_cb\n"));
    g_return_if_fail (YELP_IS_WINDOW (window));
    yelp_window_load (window, "x-yelp-toc:");
}

static void
window_go_previous_cb (GtkAction *action, YelpWindow *window)
{
    YelpWindowPriv *priv;
    gchar *base, *uri;

    d (g_print ("window_go_previous_cb\n"));
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

    d (g_print ("window_go_next_cb\n"));
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

    d (g_print ("window_go_toc_cb\n"));
    g_return_if_fail (YELP_IS_WINDOW (window));
    g_return_if_fail (window->priv->current_doc);
    priv = window->priv;

    base = yelp_doc_info_get_uri (priv->current_doc, NULL, YELP_URI_TYPE_ANY);
    uri = yelp_uri_get_relative (base, priv->toc_id);

    yelp_window_load (window, uri);

    g_free (uri);
    g_free (base);
}

static void window_copy_link_cb (GtkAction *action, YelpWindow *window) 
{
    gtk_clipboard_set_text (gtk_clipboard_get (gdk_atom_intern ("CLIPBOARD", 
								TRUE)),
			    window->priv->uri,
			    -1);
    g_free (window->priv->uri);
};

static void
window_open_link_cb (GtkAction *action, YelpWindow *window)
{
    yelp_window_load (window, window->priv->uri);
    g_free (window->priv->uri);
};

static void
window_open_link_new_cb (GtkAction *action, YelpWindow *window)
{
    g_signal_emit (window, signals[NEW_WINDOW_REQUESTED], 0,
		   window->priv->uri);
    g_free (window->priv->uri);
};

static void
window_about_cb (GtkAction *action, YelpWindow *window)
{
    const gchar *copyright =
	"Copyright © 2001-2003 Mikael Hallendal\n"
	"Copyright © 2003-2004 Shaun McCance";
    const gchar *authors[] = { 
	"Mikael Hallendal <micke@imendio.com>",
	"Alexander Larsson <alexl@redhat.com>",
	"Shaun McCance <shaunm@gnome.org>",
	NULL
    };
    /* Note to translators: put here your name (and address) so it
     * will shop up in the "about" box */
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

    d (g_print ("location_response_cb\n"));
    d (g_print ("  id = %i\n", id));

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
window_find_action (YelpWindow *window, YelpFindAction action)
{
    YelpWindowPriv *priv;

    priv = window->priv;

    window_find_save_settings (window);

    return yelp_html_find (priv->html_view,
			   priv->find_string,
			   FALSE, FALSE,
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
window_find_clicked_cb (GtkWidget  *widget,
			YelpWindow *window)
{
    YelpWindowPriv * priv;

    g_return_if_fail (GTK_IS_TOOL_ITEM (widget));

    priv = window->priv;

    if (GTK_TOOL_ITEM (widget) == priv->find_next) {
	if (!window_find_action (window, YELP_WINDOW_FIND_NEXT)) {
	    gtk_widget_set_sensitive (GTK_WIDGET (priv->find_next), FALSE);
	    gtk_widget_set_sensitive (GTK_WIDGET (priv->find_prev), TRUE);
	} else {
	    window_find_buttons_set_sensitive (window, TRUE);
	}
    }
    else if (GTK_TOOL_ITEM (widget) == priv->find_prev) {
	if (!window_find_action (window, YELP_WINDOW_FIND_PREV)) {
	    gtk_widget_set_sensitive (GTK_WIDGET (priv->find_next), TRUE);
	    gtk_widget_set_sensitive (GTK_WIDGET (priv->find_prev), FALSE);
	} else {
	    window_find_buttons_set_sensitive (window, TRUE);
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

    g_return_if_fail (YELP_IS_WINDOW(data));
	
    window = YELP_WINDOW (data);
    priv = window->priv;

    text = gtk_editable_get_chars (editable, 0, -1);

    if (!window_find_action (window, YELP_WINDOW_FIND_NEXT)) {
	gtk_widget_set_sensitive (GTK_WIDGET (priv->find_next), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->find_prev), TRUE);
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

    gtk_widget_set_sensitive (GTK_WIDGET (priv->find_next), sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (priv->find_prev), sensitive);
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

    d (g_print ("idle_write\n"));

    priv = context->window->priv;

    switch (context->type) {
    case IDLE_WRITER_MEMORY:
	d (g_print ("  context->buffer = %i bytes\n", strlen (context->buffer)));
	d (g_print ("  context->cur    = %i\n", context->cur));
	d (g_print ("  context->length = %i\n", context->length));

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

