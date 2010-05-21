/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2010 Shaun McCance <shaunm@gnome.org>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "yelp-location-entry.h"
#include "yelp-settings.h"
#include "yelp-uri.h"
#include "yelp-view.h"

#include "yelp-application.h"
#include "yelp-window.h"

static void          yelp_window_init             (YelpWindow         *window);
static void          yelp_window_class_init       (YelpWindowClass    *klass);
static void          yelp_window_dispose          (GObject            *object);
static void          yelp_window_finalize         (GObject            *object);
static void          yelp_window_get_property     (GObject            *object,
                                                   guint               prop_id,
                                                   GValue             *value,
                                                   GParamSpec         *pspec);
static void          yelp_window_set_property     (GObject            *object,
                                                   guint               prop_id,
                                                   const GValue       *value,
                                                   GParamSpec         *pspec);

static void          window_construct             (YelpWindow         *window);
static void          window_new                   (GtkAction          *action,
                                                   YelpWindow         *window);
static gboolean      window_configure_event       (YelpWindow         *window,
                                                   GdkEventConfigure  *event,
                                                   gpointer            user_data);
static void          window_drag_received         (YelpWindow         *window,
                                                   GdkDragContext     *context,
                                                   gint                x,
                                                   gint                y,
                                                   GtkSelectionData   *data,
                                                   guint               info,
                                                   guint               time,
                                                   gpointer            userdata);
static gboolean      window_resize_signal         (YelpWindow         *window);
static void          window_close                 (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_add_bookmark          (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_load_bookmark         (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_find_in_page          (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_start_search          (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_open_location         (GtkAction          *action,
                                                   YelpWindow         *window);

static void          app_bookmarks_changed        (YelpApplication    *app,
                                                   const gchar        *doc_uri,
                                                   YelpWindow         *window);
static void          window_set_bookmarks         (YelpWindow         *window,
                                                   const gchar        *doc_uri);
static void          window_set_bookmark_action   (YelpWindow         *window);
static void          window_set_bookmark_icons    (YelpWindow         *window);
static gboolean      find_animate_open            (YelpWindow         *window);
static gboolean      find_animate_close           (YelpWindow         *window);
static gboolean      find_entry_focus_out         (GtkEntry           *entry,
                                                   GdkEventFocus      *event,
                                                   YelpWindow         *window);
static gboolean      find_entry_key_press         (GtkEntry           *entry,
                                                   GdkEventKey        *event,
                                                   YelpWindow         *window);
static void          find_entry_changed           (GtkEntry           *entry,
                                                   YelpWindow         *window);

static void          entry_bookmark_clicked       (YelpLocationEntry  *entry,
                                                   YelpWindow         *window);
static void          entry_location_selected      (YelpLocationEntry  *entry,
                                                   YelpWindow         *window);
static gint          entry_completion_sort        (GtkTreeModel       *model,
                                                   GtkTreeIter        *iter1,
                                                   GtkTreeIter        *iter2,
                                                   gpointer            user_data);
static void          entry_completion_selected    (YelpLocationEntry  *entry,
                                                   GtkTreeModel       *model,
                                                   GtkTreeIter        *iter,
                                                   YelpWindow         *window);
static gboolean      entry_focus_in               (GtkEntry           *entry,
                                                   GdkEventFocus      *event,
                                                   YelpWindow         *window);
static gboolean      entry_focus_out              (YelpLocationEntry  *entry,
                                                   GdkEventFocus      *event,
                                                   YelpWindow         *window);

static void          view_external_uri            (YelpView           *view,
                                                   YelpUri            *uri,
                                                   YelpWindow         *window);
static void          view_new_window              (YelpView           *view,
                                                   YelpUri            *uri,
                                                   YelpWindow         *window);
static void          view_loaded                  (YelpView           *view,
                                                   YelpWindow         *window);
static void          view_uri_selected            (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);
static void          view_root_title              (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);
static void          view_page_title              (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);
static void          view_page_desc               (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);
static void          view_page_icon               (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);

static void          hidden_entry_activate        (GtkEntry           *entry,
                                                   YelpWindow         *window);
static void          hidden_entry_hide            (YelpWindow         *window);
static gboolean      hidden_key_press             (GtkWidget          *widget,
                                                   GdkEventKey        *event,
                                                   YelpWindow         *window);

enum {
    PROP_0,
    PROP_APPLICATION
};

enum {
    RESIZE_EVENT,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (YelpWindow, yelp_window, GTK_TYPE_WINDOW);
#define GET_PRIV(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_WINDOW, YelpWindowPrivate))

static GHashTable *completions;

enum {
    HISTORY_COL_TITLE,
    HISTORY_COL_DESC,
    HISTORY_COL_ICON,
    HISTORY_COL_URI,
    HISTORY_COL_DOC,
    HISTORY_COL_PAGE,
    HISTORY_COL_FLAGS,
    HISTORY_COL_TERMS
};

enum {
    COMPLETION_COL_TITLE,
    COMPLETION_COL_DESC,
    COMPLETION_COL_ICON,
    COMPLETION_COL_PAGE,
    COMPLETION_COL_FLAGS
};

static const gchar *YELP_UI =
    "<ui>"
    "<menubar>"
    "<menu action='PageMenu'>"
    "<menuitem action='NewWindow'/>"
    "<menuitem action='Find'/>"
    "<separator/>"
    "<menuitem action='YelpViewPrint'/>"
    "<separator/>"
    "<menuitem action='CloseWindow'/>"
    "</menu>"
    "<menu action='ViewMenu'>"
    "<menuitem action='LargerText'/>"
    "<menuitem action='SmallerText'/>"
    "<separator/>"
    "<menuitem action='ShowTextCursor'/>"
    "</menu>"
    "<menu action='GoMenu'>"
    "<menuitem action='YelpViewGoBack'/>"
    "<menuitem action='YelpViewGoForward'/>"
    "<separator/>"
    "<menuitem action='YelpViewGoPrevious'/>"
    "<menuitem action='YelpViewGoNext'/>"
    "</menu>"
    "<menu action='BookmarksMenu'>"
    "<menuitem action='AddBookmark'/>"
    "<separator/>"
    "<placeholder name='Bookmarks'/>"
    "</menu>"
    "</menubar>"
    "<accelerator action='Find'/>"
    "<accelerator action='Search'/>"
    "<accelerator action='OpenLocation'/>"
    "</ui>";

typedef struct _YelpWindowPrivate YelpWindowPrivate;
struct _YelpWindowPrivate {
    GtkListStore *history;
    GtkUIManager *ui_manager;
    GtkActionGroup *action_group;
    YelpApplication *application;

    /* no refs on these, owned by containers */
    YelpView *view;
    GtkWidget *vbox;
    GtkWidget *hbox;
    YelpLocationEntry *entry;
    GtkWidget *hidden_entry;
    GtkWidget *find_entry;
    GtkWidget *find_label;

    /* refs because we dynamically add & remove */
    GtkWidget *find_bar;
    GtkWidget *align_location;
    GtkWidget *align_hidden;

    gulong entry_location_selected;

    gchar *doc_uri;

    GtkActionGroup *bookmark_actions;
    guint bookmarks_merge_id;

    guint resize_signal;
    gint width;
    gint height;

    guint entry_color_animate;
    gfloat entry_color_step;

    guint find_animate;
    gint find_cur_height;
    gint find_entry_height;
    gint find_bar_height;
};

static const GtkActionEntry entries[] = {
    { "PageMenu",      NULL, N_("_Page")      },
    { "ViewMenu",      NULL, N_("_View")      },
    { "GoMenu",        NULL, N_("_Go")        },
    { "BookmarksMenu", NULL, N_("_Bookmarks")        },

    { "NewWindow", NULL,
      N_("_New Window"),
      "<Control>N",
      NULL,
      G_CALLBACK (window_new) },
    { "CloseWindow", NULL,
      N_("_Close"),
      "<Control>W",
      NULL,
      G_CALLBACK (window_close) },
    { "AddBookmark", NULL,
      N_("_Add Bookmark"),
      "<Control>D",
      NULL,
      G_CALLBACK (window_add_bookmark) },
    { "Find", NULL,
      N_("Find in Page..."),
      "<Control>F",
      NULL,
      G_CALLBACK (window_find_in_page) },
    { "Search", NULL,
      N_("Search..."),
      "<Control>S",
      NULL,
      G_CALLBACK (window_start_search) },
    { "OpenLocation", NULL,
      N_("Open Location"),
      "<Control>L",
      NULL,
      G_CALLBACK (window_open_location) }
};

static void
yelp_window_init (YelpWindow *window)
{
    g_signal_connect (window, "configure-event", G_CALLBACK (window_configure_event), NULL);
}

static void
yelp_window_class_init (YelpWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = yelp_window_dispose;
    object_class->finalize = yelp_window_finalize;
    object_class->get_property = yelp_window_get_property;
    object_class->set_property = yelp_window_set_property;

    completions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

    g_object_class_install_property (object_class,
                                     PROP_APPLICATION,
                                     g_param_spec_object ("application",
							  _("Application"),
							  _("A YelpApplication instance that controls this window"),
                                                          YELP_TYPE_APPLICATION,
                                                          G_PARAM_CONSTRUCT_ONLY |
							  G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
							  G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    signals[RESIZE_EVENT] =
        g_signal_new ("resized",
                      G_OBJECT_CLASS_TYPE (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    g_type_class_add_private (klass, sizeof (YelpWindowPrivate));
}

static void
yelp_window_dispose (GObject *object)
{
    YelpWindowPrivate *priv = GET_PRIV (object);

    if (priv->history) {
        g_object_unref (priv->history);
        priv->history = NULL;
    }

    if (priv->action_group) {
        g_object_unref (priv->action_group);
        priv->action_group = NULL;
    }

    if (priv->bookmark_actions) {
        g_object_unref (priv->bookmark_actions);
        priv->bookmark_actions = NULL;
    }

    if (priv->align_location) {
        g_object_unref (priv->align_location);
        priv->align_location = NULL;
    }

    if (priv->align_hidden) {
        g_object_unref (priv->align_hidden);
        priv->align_hidden = NULL;
    }

    if (priv->find_bar) {
        g_object_unref (priv->find_bar);
        priv->find_bar = NULL;
    }

    if (priv->find_animate != 0) {
        g_source_remove (priv->find_animate);
        priv->find_animate = 0;
    }

    if (priv->entry_color_animate != 0) {
        g_source_remove (priv->entry_color_animate);
        priv->entry_color_animate = 0;
    }

    G_OBJECT_CLASS (yelp_window_parent_class)->dispose (object);
}

static void
yelp_window_finalize (GObject *object)
{
    YelpWindowPrivate *priv = GET_PRIV (object);
    g_free (priv->doc_uri);
    G_OBJECT_CLASS (yelp_window_parent_class)->finalize (object);
}

static void
yelp_window_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
    YelpWindowPrivate *priv = GET_PRIV (object);
    switch (prop_id) {
    case PROP_APPLICATION:
        g_value_set_object (value, priv->application);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
yelp_window_set_property (GObject     *object,
                          guint        prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
    YelpWindowPrivate *priv = GET_PRIV (object);
    switch (prop_id) {
    case PROP_APPLICATION:
        priv->application = g_value_get_object (value);
        window_construct ((YelpWindow *) object);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
window_construct (YelpWindow *window)
{
    GtkWidget *scroll;
    GtkActionGroup *view_actions;
    GtkAction *action;
    GtkWidget *button;
    GtkTreeIter iter;
    YelpWindowPrivate *priv = GET_PRIV (window);

    gtk_window_set_icon_name (GTK_WINDOW (window), "help-browser");

    priv->view = (YelpView *) yelp_view_new ();

    priv->vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (window), priv->vbox);

    priv->action_group = gtk_action_group_new ("WindowActions");
    gtk_action_group_set_translation_domain (priv->action_group, GETTEXT_PACKAGE);
    gtk_action_group_add_actions (priv->action_group,
				  entries, G_N_ELEMENTS (entries),
				  window);

    priv->bookmark_actions = gtk_action_group_new ("BookmarkActions");
    gtk_action_group_set_translate_func (priv->bookmark_actions, NULL, NULL, NULL);

    priv->ui_manager = gtk_ui_manager_new ();
    gtk_ui_manager_insert_action_group (priv->ui_manager, priv->action_group, 0);
    gtk_ui_manager_insert_action_group (priv->ui_manager, priv->bookmark_actions, 1);
    gtk_ui_manager_insert_action_group (priv->ui_manager,
                                        yelp_application_get_action_group (priv->application),
                                        2);
    view_actions = yelp_view_get_action_group (priv->view);
    gtk_ui_manager_insert_action_group (priv->ui_manager, view_actions, 3);
    gtk_window_add_accel_group (GTK_WINDOW (window),
                                gtk_ui_manager_get_accel_group (priv->ui_manager));
    gtk_ui_manager_add_ui_from_string (priv->ui_manager, YELP_UI, -1, NULL);
    gtk_box_pack_start (GTK_BOX (priv->vbox),
                        gtk_ui_manager_get_widget (priv->ui_manager, "/ui/menubar"),
                        FALSE, FALSE, 0);

    priv->bookmarks_merge_id = gtk_ui_manager_new_merge_id (priv->ui_manager);
    g_signal_connect (priv->application, "bookmarks-changed", G_CALLBACK (app_bookmarks_changed), window);

    priv->hbox = gtk_hbox_new (FALSE, 0);
    g_object_set (priv->hbox, "border-width", 2, NULL);
    gtk_box_pack_start (GTK_BOX (priv->vbox), priv->hbox, FALSE, FALSE, 0);

    action = gtk_action_group_get_action (view_actions, "YelpViewGoBack");
    button = gtk_action_create_tool_item (action);
    gtk_box_pack_start (GTK_BOX (priv->hbox),
                        button,
                        FALSE, FALSE, 0);
    action = gtk_action_group_get_action (view_actions, "YelpViewGoForward");
    button = gtk_action_create_tool_item (action);
    gtk_box_pack_start (GTK_BOX (priv->hbox),
                        button,
                        FALSE, FALSE, 0);
    
    priv->history = gtk_list_store_new (8,
                                        G_TYPE_STRING,  /* title */
                                        G_TYPE_STRING,  /* desc */
                                        G_TYPE_STRING,  /* icon */
                                        G_TYPE_STRING,  /* uri */
                                        G_TYPE_STRING,  /* doc */
                                        G_TYPE_STRING,  /* page */
                                        G_TYPE_INT,     /* flags */
                                        G_TYPE_STRING   /* search terms */
                                        );
    gtk_list_store_append (priv->history, &iter);
    gtk_list_store_set (priv->history, &iter,
                        HISTORY_COL_FLAGS, YELP_LOCATION_ENTRY_IS_SEPARATOR,
                        -1);
    gtk_list_store_append (priv->history, &iter);
    gtk_list_store_set (priv->history, &iter,
                        HISTORY_COL_ICON, "system-search",
                        HISTORY_COL_TITLE, _("Search..."),
                        HISTORY_COL_FLAGS, YELP_LOCATION_ENTRY_IS_SEARCH,
                        -1);

    priv->entry = (YelpLocationEntry *)
        yelp_location_entry_new_with_model (GTK_TREE_MODEL (priv->history),
                                            HISTORY_COL_TITLE,
                                            HISTORY_COL_DESC,
                                            HISTORY_COL_ICON,
                                            HISTORY_COL_FLAGS);
    g_signal_connect (gtk_bin_get_child (GTK_BIN (priv->entry)), "focus-in-event",
                      G_CALLBACK (entry_focus_in), window);
    g_signal_connect (priv->entry, "focus-out-event",
                      G_CALLBACK (entry_focus_out), window);

    g_signal_connect (priv->entry, "completion-selected",
                      G_CALLBACK (entry_completion_selected), window);
    g_signal_connect (priv->entry, "bookmark-clicked",
                      G_CALLBACK (entry_bookmark_clicked), window);

    priv->entry_location_selected = g_signal_connect (priv->entry, "location-selected",
                                                      G_CALLBACK (entry_location_selected), window);
    priv->align_location = g_object_ref_sink (gtk_alignment_new (0.0, 0.5, 1.0, 0.0));
    gtk_box_pack_start (GTK_BOX (priv->hbox),
                        GTK_WIDGET (priv->align_location),
                        TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (priv->align_location), GTK_WIDGET (priv->entry));

    priv->hidden_entry = gtk_entry_new ();
    priv->align_hidden = g_object_ref_sink (gtk_alignment_new (0.0, 0.5, 1.0, 0.0));
    gtk_container_add (GTK_CONTAINER (priv->align_hidden), GTK_WIDGET (priv->hidden_entry));

    g_signal_connect (priv->hidden_entry, "activate",
                      G_CALLBACK (hidden_entry_activate), window);
    g_signal_connect_swapped (priv->hidden_entry, "focus-out-event",
                              G_CALLBACK (hidden_entry_hide), window);
    g_signal_connect (priv->hidden_entry, "key-press-event",
                      G_CALLBACK (hidden_key_press), window);

    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
                                         GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX (priv->vbox), scroll, TRUE, TRUE, 0);

    priv->find_bar = g_object_ref_sink (gtk_hbox_new (FALSE, 6));
    g_object_set (priv->find_bar, "border-width", 2, NULL);

    priv->find_entry = gtk_entry_new ();
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->find_entry),
                                       GTK_ENTRY_ICON_PRIMARY,
                                       "edit-find");
    g_signal_connect (priv->find_entry, "changed",
                      G_CALLBACK (find_entry_changed), window);
    g_signal_connect (priv->find_entry, "key-press-event",
                      G_CALLBACK (find_entry_key_press), window);
    g_signal_connect (priv->find_entry, "focus-out-event",
                      G_CALLBACK (find_entry_focus_out), window);
    g_object_set (priv->find_entry, "width-request", 300, NULL);
    gtk_box_pack_start (GTK_BOX (priv->find_bar), priv->find_entry, FALSE, FALSE, 0);

    priv->find_label = gtk_label_new ("");
    g_object_set (priv->find_label, "xalign", 0.0, NULL);
    gtk_box_pack_start (GTK_BOX (priv->find_bar), priv->find_label, FALSE, FALSE, 0);

    g_signal_connect (priv->view, "external-uri", G_CALLBACK (view_external_uri), window);
    g_signal_connect (priv->view, "new-view-requested", G_CALLBACK (view_new_window), window);
    g_signal_connect (priv->view, "loaded", G_CALLBACK (view_loaded), window);
    g_signal_connect (priv->view, "notify::yelp-uri", G_CALLBACK (view_uri_selected), window);
    g_signal_connect_swapped (priv->view, "notify::page-id",
                              G_CALLBACK (window_set_bookmark_action), window);
    g_signal_connect (priv->view, "notify::root-title", G_CALLBACK (view_root_title), window);
    g_signal_connect (priv->view, "notify::page-title", G_CALLBACK (view_page_title), window);
    g_signal_connect (priv->view, "notify::page-desc", G_CALLBACK (view_page_desc), window);
    g_signal_connect (priv->view, "notify::page-icon", G_CALLBACK (view_page_icon), window);
    gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (priv->view));
    gtk_widget_grab_focus (GTK_WIDGET (priv->view));

    gtk_drag_dest_set (GTK_WIDGET (window),
                       GTK_DEST_DEFAULT_ALL,
                       NULL, 0,
                       GDK_ACTION_COPY);
    gtk_drag_dest_add_uri_targets (GTK_WIDGET (window));
    g_signal_connect (window, "drag-data-received",
                      G_CALLBACK (window_drag_received), NULL);
}

/******************************************************************************/

YelpWindow *
yelp_window_new (YelpApplication *app)
{
    YelpWindow *window;

    window = (YelpWindow *) g_object_new (YELP_TYPE_WINDOW, "application", app, NULL);

    return window;
}

void
yelp_window_load_uri (YelpWindow  *window,
                      YelpUri     *uri)
{
    YelpWindowPrivate *priv = GET_PRIV (window);

    yelp_view_load_uri (priv->view, uri);
}

YelpUri *
yelp_window_get_uri (YelpWindow *window)
{
    YelpUri *uri;
    YelpWindowPrivate *priv = GET_PRIV (window);
    g_object_get (G_OBJECT (priv->view), "yelp-uri", &uri, NULL);
    return uri;
}

void
yelp_window_get_geometry (YelpWindow  *window,
                          gint        *width,
                          gint        *height)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    *width = priv->width;
    *height = priv->height;
}

/******************************************************************************/

static void
window_new (GtkAction *action, YelpWindow *window)
{
    YelpUri *yuri;
    gchar *uri = NULL;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (priv->view, "yelp-uri", &yuri, NULL);
    uri = yelp_uri_get_document_uri (yuri);

    yelp_application_new_window (priv->application, uri);

    g_free (uri);
    g_object_unref (yuri);
}

static void
window_drag_received (YelpWindow         *window,
                      GdkDragContext     *context,
                      gint                x,
                      gint                y,
                      GtkSelectionData   *data,
                      guint               info,
                      guint               time,
                      gpointer            userdata)
{
    gchar **uris = gtk_selection_data_get_uris (data);
    if (uris && uris[0]) {
        YelpUri *uri = yelp_uri_new (uris[0]);
        yelp_window_load_uri (window, uri);
        g_object_unref (uri);
        g_strfreev (uris);
        gtk_drag_finish (context, TRUE, FALSE, time);
    }
    gtk_drag_finish (context, FALSE, FALSE, time);
}

static gboolean
window_configure_event (YelpWindow         *window,
                        GdkEventConfigure  *event,
                        gpointer            user_data)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    priv->width = event->width;
    priv->height = event->height;
    if (priv->resize_signal > 0)
        g_source_remove (priv->resize_signal);
    priv->resize_signal = g_timeout_add (200,
                                         (GSourceFunc) window_resize_signal,
                                         window);
    g_object_set (priv->find_entry, "width-request", 2 * priv->width / 3, NULL);
    return FALSE;
}

static gboolean
window_resize_signal (YelpWindow *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    g_signal_emit (window, signals[RESIZE_EVENT], 0);
    priv->resize_signal = 0;
    return FALSE;
}

static void
window_close (GtkAction *action, YelpWindow *window)
{
    gboolean ret;
    g_signal_emit_by_name (window, "delete-event", NULL, &ret);
    gtk_widget_destroy (GTK_WIDGET (window));
}

static void
window_add_bookmark (GtkAction  *action,
                     YelpWindow *window)
{
    YelpUri *uri;
    gchar *doc_uri, *page_id, *icon, *title;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (priv->view,
                  "yelp-uri", &uri,
                  "page-id", &page_id,
                  "page-icon", &icon,
                  "page-title", &title,
                  NULL);
    doc_uri = yelp_uri_get_document_uri (uri);
    yelp_application_add_bookmark (priv->application, doc_uri, page_id, icon, title);
    g_free (doc_uri);
    g_free (page_id);
    g_free (icon);
    g_free (title);
    g_object_unref (uri);
}

static void
window_load_bookmark (GtkAction  *action,
                      YelpWindow *window)
{
    YelpUri *base, *uri;
    gchar *xref;
    YelpWindowPrivate *priv = GET_PRIV (window);

    /* Bookmark action names are prefixed with 'LoadBookmark-' */
    xref = g_strconcat ("xref:", gtk_action_get_name (action) + 13, NULL);
    g_object_get (priv->view, "yelp-uri", &base, NULL);
    uri = yelp_uri_new_relative (base, xref);

    yelp_view_load_uri (priv->view, uri);

    g_object_unref (base);
    g_object_unref (uri);
    g_free (xref);
}

static void
app_bookmarks_changed (YelpApplication *app,
                       const gchar     *doc_uri,
                       YelpWindow      *window)
{
    YelpUri *uri;
    gchar *this_doc_uri;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (priv->view, "yelp-uri", &uri, NULL);
    this_doc_uri = yelp_uri_get_document_uri (uri);

    if (g_str_equal (this_doc_uri, doc_uri))
        window_set_bookmarks (window, doc_uri);

    g_free (this_doc_uri);
    g_object_unref (uri);
}

typedef struct _YelpMenuEntry YelpMenuEntry;
struct _YelpMenuEntry {
    gchar *page_id;
    gchar *icon;
    gchar *title;
};

static gint
entry_compare (YelpMenuEntry *a, YelpMenuEntry *b)
{
    gint ret = yelp_settings_cmp_icons (a->icon, b->icon);
    if (ret != 0)
        return ret;

    if (a->title && b->title)
        return g_utf8_collate (a->title, b->title);
    else if (b->title == NULL)
        return -1;
    else if (a->title == NULL)
        return 1;

    return 0;
}

static void
window_set_bookmarks (YelpWindow  *window,
                      const gchar *doc_uri)
{
    GVariant *value;
    GVariantIter *iter;
    gchar *page_id, *icon, *title;
    YelpWindowPrivate *priv = GET_PRIV (window);
    GSList *entries = NULL;

    gtk_ui_manager_remove_ui (priv->ui_manager, priv->bookmarks_merge_id);

    value = yelp_application_get_bookmarks (priv->application, doc_uri);
    g_variant_get (value, "a(sss)", &iter);
    while (g_variant_iter_loop (iter, "(&s&s&s)", &page_id, &icon, &title)) {
        YelpMenuEntry *entry = g_new0 (YelpMenuEntry, 1);
        entry->page_id = page_id;
        entry->icon = icon;
        entry->title = title;
        entries = g_slist_insert_sorted (entries, entry, (GCompareFunc) entry_compare);
    }
    for ( ; entries != NULL; entries = g_slist_delete_link (entries, entries)) {
        GSList *cur;
        GtkAction *bookmark;
        YelpMenuEntry *entry = (YelpMenuEntry *) entries->data;
        gchar *action_id = g_strconcat ("LoadBookmark-", entry->page_id, NULL);

        bookmark = gtk_action_group_get_action (priv->bookmark_actions, action_id);
        if (bookmark == NULL) {
            bookmark = gtk_action_new (action_id, entry->title, NULL, NULL);
            g_signal_connect (bookmark, "activate",
                              G_CALLBACK (window_load_bookmark), window);
            gtk_action_set_icon_name (bookmark, entry->icon);
            gtk_action_group_add_action (priv->bookmark_actions, bookmark);
        }
        gtk_ui_manager_add_ui (priv->ui_manager,
                               priv->bookmarks_merge_id,
                               "ui/menubar/BookmarksMenu/Bookmarks",
                               action_id, action_id,
                               GTK_UI_MANAGER_MENUITEM,
                               FALSE);
        gtk_ui_manager_ensure_update (priv->ui_manager);
        for (cur = gtk_action_get_proxies (bookmark); cur != NULL; cur = cur->next) {
            if (GTK_IS_IMAGE_MENU_ITEM (cur->data))
                g_object_set (cur->data, "always-show-image", TRUE, NULL);
        }
        g_free (action_id);
        g_free (entry);
    }

    window_set_bookmark_icons (window);

    g_variant_iter_free (iter);
    g_variant_unref (value);
}

static void
window_set_bookmark_action (YelpWindow *window)
{
    YelpUri *uri;
    gchar *doc_uri, *page_id;
    gchar *curpage;
    GVariant *value;
    GVariantIter *viter;
    GtkAction *action;
    YelpWindowPrivate *priv = GET_PRIV (window);

    action = gtk_action_group_get_action (priv->action_group, "AddBookmark");

    g_object_get (priv->view,
                  "yelp-uri", &uri,
                  "page-id", &page_id,
                  NULL);
    if (page_id == NULL) {
        gtk_action_set_sensitive (action, FALSE);
        if (uri)
            g_object_unref (uri);
        return;
    }
    gtk_action_set_sensitive (action, TRUE);

    doc_uri = yelp_uri_get_document_uri (uri);
    value = yelp_application_get_bookmarks (priv->application, doc_uri);
    g_variant_get (value, "a(sss)", &viter);
    while (g_variant_iter_loop (viter, "(&s&s&s)", &curpage, NULL, NULL)) {
        if (g_str_equal (page_id, curpage)) {
            gtk_action_set_sensitive (action, FALSE);
            break;
        }
    }
    g_variant_iter_free (viter);
    g_variant_unref (value);

    g_free (page_id);
    g_free (doc_uri);
    g_object_unref (uri);
}

static void
window_set_bookmark_icons (YelpWindow *window)
{
    YelpUri *uri;
    gchar *doc_uri;
    GVariant *value;
    GVariantIter *viter;
    gboolean cont;
    GtkTreeIter iter;
    gchar *page_id;
    GHashTable *bookmarks;
    GtkTreeModel *completion;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (priv->view, "yelp-uri", &uri, NULL);
    doc_uri = yelp_uri_get_document_uri (uri);

    bookmarks = g_hash_table_new (g_str_hash, g_str_equal);
    value = yelp_application_get_bookmarks (priv->application, doc_uri);
    g_variant_get (value, "a(sss)", &viter);
    while (g_variant_iter_loop (viter, "(&s&s&s)", &page_id, NULL, NULL))
        g_hash_table_insert (bookmarks, page_id, page_id);
    g_variant_iter_free (viter);
    g_variant_unref (value);

    cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &iter);
    while (cont) {
        gchar *iter_doc, *iter_page;
        gint iter_flags;
        gtk_tree_model_get (GTK_TREE_MODEL (priv->history), &iter,
                            HISTORY_COL_DOC, &iter_doc,
                            HISTORY_COL_PAGE, &iter_page,
                            HISTORY_COL_FLAGS, &iter_flags,
                            -1);
        iter_flags &= ~YELP_LOCATION_ENTRY_CAN_BOOKMARK;
        iter_flags &= ~YELP_LOCATION_ENTRY_IS_BOOKMARKED;

        if (iter_flags & (YELP_LOCATION_ENTRY_IS_SEPARATOR | YELP_LOCATION_ENTRY_IS_SEARCH)) {
        }
        else if (iter_doc && g_str_equal (iter_doc, doc_uri)) {
            if (iter_page && g_hash_table_lookup (bookmarks, iter_page))
                iter_flags |= YELP_LOCATION_ENTRY_IS_BOOKMARKED;
            else if (iter_page)
                iter_flags |= YELP_LOCATION_ENTRY_CAN_BOOKMARK;
        }

        g_free (iter_doc);
        g_free (iter_page);
        gtk_list_store_set (priv->history, &iter, HISTORY_COL_FLAGS, iter_flags, -1);
        cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->history), &iter);
    }

    completion = (GtkTreeModel *) g_hash_table_lookup (completions, doc_uri);
    if (completion) {
        gchar *page_id;
        gint flags;
        completion = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (completion));
        cont = gtk_tree_model_get_iter_first (completion, &iter);
        while (cont) {
            gtk_tree_model_get (completion, &iter,
                                COMPLETION_COL_PAGE, &page_id,
                                COMPLETION_COL_FLAGS, &flags,
                                -1);
            if (page_id && g_hash_table_lookup (bookmarks, page_id)) {
                flags |= YELP_LOCATION_ENTRY_IS_BOOKMARKED;
            }
            else {
                flags &= ~YELP_LOCATION_ENTRY_IS_BOOKMARKED;
            }
            gtk_list_store_set (GTK_LIST_STORE (completion), &iter,
                                COMPLETION_COL_FLAGS, flags,
                                -1);
            cont = gtk_tree_model_iter_next (completion, &iter);
        }
    }

    g_hash_table_destroy (bookmarks);
    g_free (doc_uri);
    g_object_unref (uri);
}
    

static void
window_start_search (GtkAction *action, YelpWindow *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);

    yelp_location_entry_start_search (priv->entry);
}

static void
window_open_location (GtkAction *action, YelpWindow *window)
{
    YelpUri *yuri = NULL;
    gchar *uri = NULL;
    GdkColor yellow;
    gchar *color;
    YelpWindowPrivate *priv = GET_PRIV (window);

    gtk_container_remove (GTK_CONTAINER (priv->hbox),
                          priv->align_location);
    gtk_box_pack_start (GTK_BOX (priv->hbox),
                        priv->align_hidden,
                        TRUE, TRUE, 0);

    gtk_widget_show_all (priv->align_hidden);
    gtk_entry_set_text (GTK_ENTRY (priv->hidden_entry), "");
    gtk_widget_grab_focus (priv->hidden_entry);

    color = yelp_settings_get_color (yelp_settings_get_default (),
                                     YELP_SETTINGS_COLOR_YELLOW_BASE);
    if (gdk_color_parse (color, &yellow)) {
        gtk_widget_modify_base (priv->hidden_entry,
                                GTK_STATE_NORMAL,
                                &yellow);
    }
    g_free (color);

    g_object_get (priv->view, "yelp-uri", &yuri, NULL);
    if (yuri) {
        uri = yelp_uri_get_canonical_uri (yuri);
        g_object_unref (yuri);
    }
    if (uri) {
        gchar *c;
        gtk_entry_set_text (GTK_ENTRY (priv->hidden_entry), uri);
        c = strchr (uri, ':');
        if (c)
            gtk_editable_select_region (GTK_EDITABLE (priv->hidden_entry), c - uri + 1, -1);
        else
            gtk_editable_select_region (GTK_EDITABLE (priv->hidden_entry), 5, -1);
        g_free (uri);
    }
}

static gboolean
find_animate_open (YelpWindow *window) {
    YelpWindowPrivate *priv = GET_PRIV (window);

    priv->find_cur_height += 2;
    if (priv->find_cur_height >= priv->find_bar_height) {
        priv->find_cur_height = priv->find_bar_height;
        g_object_set (priv->find_bar, "height-request", -1, NULL);
        g_object_set (priv->find_entry, "height-request", -1, NULL);
        priv->find_animate = 0;
        gtk_editable_select_region (GTK_EDITABLE (priv->find_entry), 0, -1);
        return FALSE;
    }
    else {
        if (priv->find_cur_height >= 12)
            find_entry_changed (GTK_ENTRY (priv->find_entry), window);
        g_object_set (priv->find_bar, "height-request", priv->find_cur_height, NULL);
        g_object_set (priv->find_entry, "height-request",
                      MIN(priv->find_cur_height, priv->find_entry_height),
                      NULL);
        return TRUE;
    }
}

static gboolean
find_animate_close (YelpWindow *window) {
    YelpWindowPrivate *priv = GET_PRIV (window);

    priv->find_cur_height -= 2;
    if (priv->find_cur_height <= 0) {
        priv->find_cur_height = 0;
        g_object_set (priv->find_bar, "height-request", -1, NULL);
        g_object_set (priv->find_entry, "height-request", -1, NULL);
        gtk_container_remove (GTK_CONTAINER (priv->vbox), priv->find_bar); 
        priv->find_animate = 0;
        return FALSE;
    }
    else {
        if (priv->find_cur_height < 12)
            gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->find_entry),
                                               GTK_ENTRY_ICON_PRIMARY,
                                               NULL);
        g_object_set (priv->find_bar, "height-request", priv->find_cur_height, NULL);
        g_object_set (priv->find_entry, "height-request",
                      MIN(priv->find_cur_height, priv->find_entry_height),
                      NULL);
        return TRUE;
    }
}

static void
window_find_in_page (GtkAction  *action,
                     YelpWindow *window)
{
    GtkRequisition req;
    YelpWindowPrivate *priv = GET_PRIV (window);

    if (priv->find_animate != 0)
        return;

    if (gtk_widget_get_parent (priv->find_bar) != NULL) {
        gtk_widget_grab_focus (priv->find_entry);
        return;
    }

    g_object_set (priv->find_entry, "width-request", 2 * priv->width / 3, NULL);

    gtk_box_pack_end (GTK_BOX (priv->vbox), priv->find_bar, FALSE, FALSE, 0);
    g_object_set (priv->find_bar, "height-request", -1, NULL);
    g_object_set (priv->find_entry, "height-request", -1, NULL);
    gtk_widget_show_all (priv->find_bar);
    gtk_widget_grab_focus (priv->find_entry);

    gtk_widget_size_request (priv->find_bar, &req);
    priv->find_bar_height = req.height;
    gtk_widget_size_request (priv->find_entry, &req);
    priv->find_entry_height = req.height;

    priv->find_cur_height = 2;

    g_object_set (priv->find_bar, "height-request", 2, NULL);
    g_object_set (priv->find_entry, "height-request", 2, NULL);
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->find_entry),
                                       GTK_ENTRY_ICON_PRIMARY,
                                       NULL);
    priv->find_animate = g_timeout_add (2, (GSourceFunc) find_animate_open, window);
}

static gboolean
find_entry_key_press (GtkEntry    *entry,
                      GdkEventKey *event,
                      YelpWindow  *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);

    if (priv->find_animate != 0)
        return TRUE;

    if (event->keyval == GDK_Escape) {
        gtk_widget_grab_focus (GTK_WIDGET (priv->view));
        return TRUE;
    }

    if (event->keyval == GDK_Return || event->keyval == GDK_KP_Enter) {
        gchar *text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
        webkit_web_view_search_text (WEBKIT_WEB_VIEW (priv->view),
                                     text, FALSE, TRUE, TRUE);
        g_free (text);
        return TRUE;
    }

    return FALSE;
}

static gboolean
find_entry_focus_out (GtkEntry      *entry,
                      GdkEventFocus *event,
                      YelpWindow    *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    webkit_web_view_unmark_text_matches (WEBKIT_WEB_VIEW (priv->view));
    webkit_web_view_set_highlight_text_matches (WEBKIT_WEB_VIEW (priv->view), FALSE);
    priv->find_cur_height = priv->find_bar_height;
    priv->find_animate = g_timeout_add (2, (GSourceFunc) find_animate_close, window);
    return FALSE;
}

static void
find_entry_changed (GtkEntry   *entry,
                    YelpWindow *window)
{
    gchar *text;
    gint count;
    YelpWindowPrivate *priv = GET_PRIV (window);

    webkit_web_view_unmark_text_matches (WEBKIT_WEB_VIEW (priv->view));

    text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);

    if (text[0] == '\0') {
        gtk_widget_modify_base (priv->find_entry, GTK_STATE_NORMAL, NULL);
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
                                           GTK_ENTRY_ICON_PRIMARY,
                                           "edit-find");
        gtk_label_set_text (GTK_LABEL (priv->find_label), "");
        return;
    }

    count = webkit_web_view_mark_text_matches (WEBKIT_WEB_VIEW (priv->view),
                                               text, FALSE, 0);
    if (count > 0) {
        gchar *label = g_strdup_printf (ngettext ("%i match", "%i matches", count), count);
        gtk_widget_modify_base (priv->find_entry, GTK_STATE_NORMAL, NULL);
        webkit_web_view_set_highlight_text_matches (WEBKIT_WEB_VIEW (priv->view), TRUE);
        webkit_web_view_search_text (WEBKIT_WEB_VIEW (priv->view),
                                     text, FALSE, TRUE, TRUE);
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
                                           GTK_ENTRY_ICON_PRIMARY,
                                           "edit-find");
        gtk_label_set_text (GTK_LABEL (priv->find_label), label);
        g_free (label);
    }
    else {
        gchar *color;
        GdkColor gdkcolor;

        color = yelp_settings_get_color (yelp_settings_get_default (),
                                         YELP_SETTINGS_COLOR_RED_BASE);
        if (gdk_color_parse (color, &gdkcolor))
            gtk_widget_modify_base (priv->find_entry, GTK_STATE_NORMAL, &gdkcolor);
        g_free (color);

        webkit_web_view_set_highlight_text_matches (WEBKIT_WEB_VIEW (priv->view), FALSE);
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
                                           GTK_ENTRY_ICON_PRIMARY,
                                           "dialog-warning");
        gtk_label_set_text (GTK_LABEL (priv->find_label), _("No matches"));
    }

    g_free (text);
}

static void
entry_bookmark_clicked (YelpLocationEntry  *entry,
                        YelpWindow         *window)
{
    window_add_bookmark (NULL, window);
}

static void
entry_location_selected (YelpLocationEntry  *entry,
                         YelpWindow         *window)
{
    GtkTreeIter iter;
    gchar *uri;
    YelpWindowPrivate *priv = GET_PRIV (window);

    gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->entry), &iter);
    gtk_tree_model_get (GTK_TREE_MODEL (priv->history), &iter,
                        HISTORY_COL_URI, &uri,
                        -1);
    yelp_view_load (priv->view, uri);
    g_free (uri);
}

static gint
entry_completion_sort (GtkTreeModel *model,
                       GtkTreeIter  *iter1,
                       GtkTreeIter  *iter2,
                       gpointer      user_data)
{
    gint ret = 0;
    gchar *key1, *key2;

    gtk_tree_model_get (model, iter1, COMPLETION_COL_ICON, &key1, -1);
    gtk_tree_model_get (model, iter2, COMPLETION_COL_ICON, &key2, -1);
    ret = yelp_settings_cmp_icons (key1, key2);
    g_free (key1);
    g_free (key2);

    if (ret)
        return ret;

    gtk_tree_model_get (model, iter1, COMPLETION_COL_TITLE, &key1, -1);
    gtk_tree_model_get (model, iter2, COMPLETION_COL_TITLE, &key2, -1);
    if (key1 && key2)
        ret = g_utf8_collate (key1, key2);
    else if (key2 == NULL)
        return -1;
    else if (key1 == NULL)
        return 1;
    g_free (key1);
    g_free (key2);

    return ret;
}

static void
entry_completion_selected (YelpLocationEntry  *entry,
                           GtkTreeModel       *model,
                           GtkTreeIter        *iter,
                           YelpWindow         *window)
{
    YelpUri *base, *uri;
    gchar *page, *xref;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (priv->view, "yelp-uri", &base, NULL);
    gtk_tree_model_get (model, iter, COMPLETION_COL_PAGE, &page, -1);

    xref = g_strconcat ("xref:", page, NULL);
    uri = yelp_uri_new_relative (base, xref);

    yelp_view_load_uri (priv->view, uri);

    g_free (page);
    g_free (xref);
    g_object_unref (uri);
    g_object_unref (base);

    gtk_widget_grab_focus (GTK_WIDGET (priv->view));
}

static gboolean
entry_color_animate (YelpWindow *window) {
    gchar *color;
    GdkColor yellow, base;
    YelpWindowPrivate *priv = GET_PRIV (window);

    color = yelp_settings_get_color (yelp_settings_get_default (),
                                     YELP_SETTINGS_COLOR_YELLOW_BASE);
    gdk_color_parse (color, &yellow);
    g_free (color);

    color = yelp_settings_get_color (yelp_settings_get_default (),
                                     YELP_SETTINGS_COLOR_BASE);
    gdk_color_parse (color, &base);
    g_free (color);

    yellow.red = priv->entry_color_step * yellow.red + (1.0 - priv->entry_color_step) * base.red;
    yellow.green = priv->entry_color_step * yellow.green + (1.0 - priv->entry_color_step) * base.green;
    yellow.blue = priv->entry_color_step * yellow.blue + (1.0 - priv->entry_color_step) * base.blue;

    gtk_widget_modify_base (gtk_bin_get_child (GTK_BIN (priv->entry)), GTK_STATE_NORMAL, &yellow);

    priv->entry_color_step -= 0.05;

    if (priv->entry_color_step < 0.0) {
        priv->entry_color_animate = 0;
        return FALSE;
    }

    return TRUE;
}

static gboolean
entry_focus_in (GtkEntry           *entry,
                GdkEventFocus      *event,
                YelpWindow         *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);

    if (priv->entry_color_animate != 0)
        return FALSE;

    priv->entry_color_step = 1.0;
    priv->entry_color_animate = g_timeout_add (40, (GSourceFunc) entry_color_animate, window);

    return FALSE;
}

static gboolean
entry_focus_out (YelpLocationEntry  *entry,
                 GdkEventFocus      *event,
                 YelpWindow         *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    gtk_widget_grab_focus (GTK_WIDGET (priv->view));
    return FALSE;
}

static void
view_external_uri (YelpView   *view,
                   YelpUri    *uri,
                   YelpWindow *window)
{
    gchar *struri = yelp_uri_get_canonical_uri (uri);
    if (g_str_has_prefix (struri, "install:")) {
        YelpWindowPrivate *priv = GET_PRIV (window);
        gchar *pkg = struri + 8;
        yelp_application_install_package (priv->application, pkg, "");
    }
    else
        g_app_info_launch_default_for_uri (struri, NULL, NULL);

    g_free (struri);
}

static void
view_new_window (YelpView   *view,
                 YelpUri    *uri,
                 YelpWindow *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    yelp_application_new_window_uri (priv->application, uri);
}

static void
view_loaded (YelpView   *view,
             YelpWindow *window)
{
    gchar **ids;
    gint i;
    GtkTreeIter iter;
    gint flags;
    YelpUri *uri;
    gchar *doc_uri, *page_id, *icon, *title;
    GtkTreeModel *completion;
    YelpWindowPrivate *priv = GET_PRIV (window);
    YelpDocument *document = yelp_view_get_document (view);

    gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &iter);
    gtk_tree_model_get (GTK_TREE_MODEL (priv->history), &iter,
                        HISTORY_COL_FLAGS, &flags,
                        -1);
    if (flags & YELP_LOCATION_ENTRY_IS_LOADING) {
        flags = flags & ~YELP_LOCATION_ENTRY_IS_LOADING;
        gtk_list_store_set (priv->history, &iter, HISTORY_COL_FLAGS, flags, -1);
    }

    g_object_get (view,
                  "yelp-uri", &uri,
                  "page-id", &page_id,
                  "page-icon", &icon,
                  "page-title", &title,
                  NULL);
    doc_uri = yelp_uri_get_document_uri (uri);
    gtk_list_store_set (priv->history, &iter,
                        HISTORY_COL_DOC, doc_uri,
                        HISTORY_COL_PAGE, page_id,
                        -1);
    yelp_application_update_bookmarks (priv->application,
                                       doc_uri,
                                       page_id,
                                       icon,
                                       title);
    g_free (page_id);
    g_free (icon);
    g_free (title);

    completion = (GtkTreeModel *) g_hash_table_lookup (completions, doc_uri);
    if (completion == NULL) {
        GtkListStore *base = gtk_list_store_new (5,
                                                 G_TYPE_STRING,  /* title */
                                                 G_TYPE_STRING,  /* desc */
                                                 G_TYPE_STRING,  /* icon */
                                                 G_TYPE_STRING,  /* uri */
                                                 G_TYPE_INT      /* flags */
                                                 );
        completion = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (base));
        gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (completion),
                                                 entry_completion_sort,
                                                 NULL, NULL);
        g_hash_table_insert (completions, doc_uri, completion);
        ids = yelp_document_list_page_ids (document);
        for (i = 0; ids[i]; i++) {
            GtkTreeIter iter;
            gchar *title, *desc, *icon;
            gtk_list_store_insert (GTK_LIST_STORE (base), &iter, 0);
            title = yelp_document_get_page_title (document, ids[i]);
            desc = yelp_document_get_page_desc (document, ids[i]);
            icon = yelp_document_get_page_icon (document, ids[i]);
            gtk_list_store_set (base, &iter,
                                COMPLETION_COL_TITLE, title,
                                COMPLETION_COL_DESC, desc,
                                COMPLETION_COL_ICON, icon,
                                COMPLETION_COL_PAGE, ids[i],
                                -1);
            g_free (icon);
            g_free (desc);
            g_free (title);
        }
        g_object_unref (base);
        g_strfreev (ids);
    }
    else {
        g_free (doc_uri);
    }

    yelp_location_entry_set_completion_model (YELP_LOCATION_ENTRY (priv->entry),
                                              GTK_TREE_MODEL (completion),
                                              COMPLETION_COL_TITLE,
                                              COMPLETION_COL_DESC,
                                              COMPLETION_COL_ICON,
                                              COMPLETION_COL_FLAGS);

    window_set_bookmark_icons (window);

    g_object_unref (uri);
}

static void
view_uri_selected (YelpView     *view,
                   GParamSpec   *pspec,
                   YelpWindow   *window)
{
    GtkTreeIter iter;
    gchar *iter_uri;
    gboolean cont;
    YelpUri *uri;
    gchar *struri, *doc_uri;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (G_OBJECT (view), "yelp-uri", &uri, NULL);
    if (uri == NULL)
        return;

    doc_uri = yelp_uri_get_document_uri (uri);
    if (priv->doc_uri == NULL || !g_str_equal (priv->doc_uri, doc_uri)) {
        window_set_bookmarks (window, doc_uri);
        g_free (priv->doc_uri);
        priv->doc_uri = doc_uri;
    }
    else {
        g_free (doc_uri);
    }

    struri = yelp_uri_get_canonical_uri (uri);

    cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &iter);
    while (cont) {
        gtk_tree_model_get (GTK_TREE_MODEL (priv->history), &iter,
                            HISTORY_COL_URI, &iter_uri,
                            -1);
        if (iter_uri && g_str_equal (struri, iter_uri)) {
            g_free (iter_uri);
            break;
        }
        else {
            g_free (iter_uri);
            cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->history), &iter);
        }
    }
    if (cont) {
        GtkTreeIter first;
        gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &first);
        gtk_list_store_move_before (priv->history, &iter, &first);
    }
    else {
        gint num;
        GtkTreeIter last;
        gtk_list_store_prepend (priv->history, &iter);
        gtk_list_store_set (priv->history, &iter,
                            HISTORY_COL_TITLE, _("Loading"),
                            HISTORY_COL_ICON, "help-contents",
                            HISTORY_COL_URI, struri,
                            HISTORY_COL_FLAGS, YELP_LOCATION_ENTRY_IS_LOADING,
                            -1);
        /* Limit to 15 entries. There are two extra for the search entry and
         * the separator above it.
         */
        num = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->history), NULL);
        if (num > 17) {
            if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (priv->history),
                                                               &last, NULL,
                                                               num - 3)) {
                gtk_list_store_remove (priv->history, &last);
            }
        }
        window_set_bookmark_icons (window);
    }
    g_signal_handler_block (priv->entry,
                            priv->entry_location_selected);
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->entry), &iter);
    g_signal_handler_unblock (priv->entry,
                              priv->entry_location_selected);

    g_free (struri);
    g_object_unref (uri);
}

static void
view_root_title (YelpView    *view,
                 GParamSpec  *pspec,
                 YelpWindow  *window)
{
    gchar *title;
    g_object_get (view, "root-title", &title, NULL);

    if (title) {
        gtk_window_set_title (GTK_WINDOW (window), title);
        g_free (title);
    } else {
        gtk_window_set_title (GTK_WINDOW (window), _("Help"));
    }
}

static void
view_page_title (YelpView    *view,
                 GParamSpec  *pspec,
                 YelpWindow  *window)
{
    GtkTreeIter first;
    gchar *title, *frag;
    YelpUri *uri;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (view, "page-title", &title, NULL);
    if (title == NULL)
        return;

    g_object_get (view, "yelp-uri", &uri, NULL);
    frag = yelp_uri_get_frag_id (uri);

    gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &first);
    if (frag) {
        gchar *tmp = g_strdup_printf ("%s (#%s)", title, frag);
        gtk_list_store_set (priv->history, &first,
                            HISTORY_COL_TITLE, tmp,
                            -1);
        g_free (tmp);
        g_free (frag);
    }
    else {
        gtk_list_store_set (priv->history, &first,
                            HISTORY_COL_TITLE, title,
                            -1);
    }

    g_free (title);
    g_object_unref (uri);
}

static void
view_page_desc (YelpView    *view,
                GParamSpec  *pspec,
                YelpWindow  *window)
{
    GtkTreeIter first;
    gchar *desc;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (view, "page-desc", &desc, NULL);
    if (desc == NULL)
        return;

    gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &first);
    gtk_list_store_set (priv->history, &first,
                        HISTORY_COL_DESC, desc,
                        -1);
    g_free (desc);
}

static void
view_page_icon (YelpView   *view,
                GParamSpec *pspec,
                YelpWindow *window)
{
    GtkTreeIter first;
    gchar *icon;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (view, "page-icon", &icon, NULL);
    if (icon == NULL)
        return;

    gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &first);
    gtk_list_store_set (priv->history, &first,
                        HISTORY_COL_ICON, icon,
                        -1);
    g_free (icon);
}

static void
hidden_entry_activate (GtkEntry    *entry,
                       YelpWindow  *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    YelpUri *uri = yelp_uri_new (gtk_entry_get_text (entry));

    yelp_window_load_uri (window, uri);
    g_object_unref (uri);

    gtk_widget_grab_focus (GTK_WIDGET (priv->view));
}

static void
hidden_entry_hide (YelpWindow  *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);

    gtk_container_remove (GTK_CONTAINER (priv->hbox),
                          priv->align_hidden);
    gtk_box_pack_start (GTK_BOX (priv->hbox),
                        priv->align_location,
                        TRUE, TRUE, 0);
}

static gboolean
hidden_key_press (GtkWidget    *widget,
                  GdkEventKey  *event,
                  YelpWindow   *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    if (event->keyval == GDK_Escape) {
        gtk_widget_grab_focus (GTK_WIDGET (priv->view));
        return TRUE;
    }
    return FALSE;
}
