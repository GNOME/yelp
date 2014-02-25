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
static gboolean      window_map_event             (YelpWindow         *window,
                                                   GdkEvent           *event,
                                                   gpointer            user_data);
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
static void          window_go_all                (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_add_bookmark          (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_remove_bookmark       (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_load_bookmark         (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_find_in_page          (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_start_search          (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_open_location         (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_read_later            (GtkAction          *action,
                                                   YelpWindow         *window);
static gboolean      read_later_clicked           (GtkLinkButton      *button,
                                                   YelpWindow         *window);
static void          app_read_later_changed       (YelpApplication    *app,
                                                   const gchar        *doc_uri,
                                                   YelpWindow         *window);
static void          app_bookmarks_changed        (YelpApplication    *app,
                                                   const gchar        *doc_uri,
                                                   YelpWindow         *window);
static void          window_set_bookmarks         (YelpWindow         *window,
                                                   const gchar        *doc_uri);
static void          window_set_bookmark_action   (YelpWindow         *window);
static gboolean      find_entry_focus_out         (GtkEntry           *entry,
                                                   GdkEventFocus      *event,
                                                   YelpWindow         *window);
static gboolean      find_entry_key_press         (GtkEntry           *entry,
                                                   GdkEventKey        *event,
                                                   YelpWindow         *window);
static void          find_entry_changed           (GtkEntry           *entry,
                                                   YelpWindow         *window);

static gboolean      entry_focus_in               (GtkEntry           *entry,
                                                   GdkEventFocus      *event,
                                                   YelpWindow         *window);
static gboolean      entry_focus_out              (YelpLocationEntry  *entry,
                                                   GdkEventFocus      *event,
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
static gboolean      view_is_xref_uri             (YelpView           *view,
                                                   GtkAction          *action,
                                                   const gchar        *uri,
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
    "<separator/>"
    "<menuitem action='GoAll'/>"
    "</menu>"
    "<menu action='BookmarksMenu'>"
    "<menuitem action='AddBookmark'/>"
    "<menuitem action='RemoveBookmark'/>"
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

    gulong        bookmarks_changed;
    gulong        read_later_changed;

    /* no refs on these, owned by containers */
    YelpView *view;
    GtkWidget *vbox_view;
    GtkWidget *vbox_full;
    GtkWidget *hbox;
    YelpLocationEntry *entry;
    GtkWidget *hidden_entry;
    GtkWidget *find_entry;
    GtkWidget *find_label;
    GtkWidget *read_later_vbox;

    /* refs because we dynamically add & remove */
    GtkWidget *find_bar;
    GtkWidget *align_location;
    GtkWidget *align_hidden;
    GtkWidget *read_later;

    gchar *doc_uri;

    GtkActionGroup *bookmark_actions;
    guint bookmarks_merge_id;

    guint resize_signal;
    gint width;
    gint height;

    guint entry_color_animate;
    gfloat entry_color_step;

    gboolean configured;
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
    { "GoAll", NULL,
      N_("_All Documents"),
      NULL, NULL,
      G_CALLBACK (window_go_all) },
    { "AddBookmark", NULL,
      N_("_Add Bookmark"),
      "<Control>D",
      NULL,
      G_CALLBACK (window_add_bookmark) },
    { "RemoveBookmark", NULL,
      N_("_Remove Bookmark"),
      NULL, NULL,
      G_CALLBACK (window_remove_bookmark) },
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
    g_signal_connect (window, "map-event", G_CALLBACK (window_map_event), NULL);
}

static void
yelp_window_class_init (YelpWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = yelp_window_dispose;
    object_class->finalize = yelp_window_finalize;
    object_class->get_property = yelp_window_get_property;
    object_class->set_property = yelp_window_set_property;

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

    if (priv->read_later_changed) {
        g_signal_handler_disconnect (priv->application, priv->read_later_changed);
        priv->read_later_changed = 0;
    }

    if (priv->bookmarks_changed) {
        g_signal_handler_disconnect (priv->application, priv->bookmarks_changed);
        priv->bookmarks_changed = 0;
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
    GtkWidget *vbox, *button, *label;
    gchar *color, *text;
    YelpWindowPrivate *priv = GET_PRIV (window);

    gtk_window_set_icon_name (GTK_WINDOW (window), "help-browser");

    priv->view = (YelpView *) yelp_view_new ();

    action = gtk_action_new ("ReadLinkLater", _("Read Link _Later"), NULL, NULL);
    g_signal_connect (action, "activate", G_CALLBACK (window_read_later), window);
    yelp_view_add_link_action (priv->view, action,
                               (YelpViewActionValidFunc) view_is_xref_uri,
                               window);
    priv->read_later_changed =
        g_signal_connect (priv->application, "read-later-changed",
                          G_CALLBACK (app_read_later_changed), window);

    priv->vbox_full = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
    gtk_container_add (GTK_CONTAINER (window), priv->vbox_full);

    priv->vbox_view = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start (GTK_BOX (priv->vbox_full), priv->vbox_view, TRUE, TRUE, 0);

    priv->action_group = gtk_action_group_new ("YelpWindowActions");
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
    gtk_box_pack_start (GTK_BOX (priv->vbox_view),
                        gtk_ui_manager_get_widget (priv->ui_manager, "/ui/menubar"),
                        FALSE, FALSE, 0);

    priv->bookmarks_merge_id = gtk_ui_manager_new_merge_id (priv->ui_manager);
    priv->bookmarks_changed =
        g_signal_connect (priv->application, "bookmarks-changed",
                          G_CALLBACK (app_bookmarks_changed), window);

    priv->hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    g_object_set (priv->hbox, "border-width", 2, NULL);
    gtk_box_pack_start (GTK_BOX (priv->vbox_view), priv->hbox, FALSE, FALSE, 0);

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

    priv->entry = (YelpLocationEntry *) yelp_location_entry_new (priv->view,
                                                                 YELP_BOOKMARKS (priv->application));
    g_signal_connect (gtk_bin_get_child (GTK_BIN (priv->entry)), "focus-in-event",
                      G_CALLBACK (entry_focus_in), window);
    g_signal_connect (priv->entry, "focus-out-event",
                      G_CALLBACK (entry_focus_out), window);

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
    gtk_box_pack_start (GTK_BOX (priv->vbox_view), scroll, TRUE, TRUE, 0);

    priv->find_bar = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6));
    g_object_set (priv->find_bar,
                  "border-width", 2,
                  "margin-right", 16,
                  NULL);

    label = gtk_label_new (_("Find:"));
    gtk_box_pack_start (GTK_BOX (priv->find_bar), label, FALSE, FALSE, 6);

    priv->find_entry = gtk_entry_new ();
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

    priv->read_later = g_object_ref_sink (gtk_info_bar_new ());
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    color = yelp_settings_get_color (yelp_settings_get_default (),
                                     YELP_SETTINGS_COLOR_TEXT_LIGHT);
    text = g_markup_printf_escaped ("<span weight='bold' color='%s'>%s</span>",
                                    color, _("Read Later"));
    button = gtk_label_new (text);
    g_object_set (button, "use-markup", TRUE, "xalign", 0.0, NULL);
    g_free (color);
    g_free (text);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (gtk_info_bar_get_content_area (GTK_INFO_BAR (priv->read_later))),
                        vbox,
                        FALSE, FALSE, 0);
    priv->read_later_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start (GTK_BOX (vbox), priv->read_later_vbox, FALSE, FALSE, 0);

    g_signal_connect (priv->view, "new-view-requested", G_CALLBACK (view_new_window), window);
    g_signal_connect (priv->view, "loaded", G_CALLBACK (view_loaded), window);
    g_signal_connect (priv->view, "notify::yelp-uri", G_CALLBACK (view_uri_selected), window);
    g_signal_connect_swapped (priv->view, "notify::page-id",
                              G_CALLBACK (window_set_bookmark_action), window);
    window_set_bookmark_action (window);
    g_signal_connect (priv->view, "notify::root-title", G_CALLBACK (view_root_title), window);
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
window_map_event (YelpWindow  *window,
                  GdkEvent    *event,
                  gpointer     user_data)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    priv->configured = TRUE;
    return FALSE;
}

static gboolean
window_configure_event (YelpWindow         *window,
                        GdkEventConfigure  *event,
                        gpointer            user_data)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    gboolean skip = TRUE;
    if (priv->width != event->width) {
        skip = FALSE;
        priv->width = event->width;
    }
    if (priv->height != event->height) {
        skip = FALSE;
        priv->height = event->height;
    }
    /* Skip the configure-event signals that GTK+ sends as it's mapping
     * the window, and also skip if the event didn't change the size of
     * the window (i.e. it was just a move).
     */
    if (!priv->configured || skip)
        return FALSE;

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
window_go_all (GtkAction  *action,
               YelpWindow *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    yelp_view_load (priv->view, "help-list:");
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
    yelp_application_add_bookmark (YELP_BOOKMARKS (priv->application),
                                   doc_uri, page_id, icon, title);
    g_free (doc_uri);
    g_free (page_id);
    g_free (icon);
    g_free (title);
    g_object_unref (uri);
}

static void
window_remove_bookmark (GtkAction  *action,
                        YelpWindow *window)
{
    YelpUri *uri;
    gchar *doc_uri, *page_id;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (priv->view,
                  "yelp-uri", &uri,
                  "page-id", &page_id,
                  NULL);
    doc_uri = yelp_uri_get_document_uri (uri);
    yelp_application_remove_bookmark (YELP_BOOKMARKS (priv->application),
                                      doc_uri, page_id);
    g_free (doc_uri);
    g_free (page_id);
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

    if (g_str_equal (this_doc_uri, doc_uri)) {
        window_set_bookmarks (window, doc_uri);
    }

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

    window_set_bookmark_action (window);

    gtk_ui_manager_remove_ui (priv->ui_manager, priv->bookmarks_merge_id);

    value = yelp_application_get_bookmarks (priv->application, doc_uri);
    g_variant_get (value, "a(sss)", &iter);
    while (g_variant_iter_loop (iter, "(&s&s&s)", &page_id, &icon, &title)) {
        YelpMenuEntry *entry = g_new0 (YelpMenuEntry, 1);
        entry->page_id = page_id;
        entry->icon = g_strdup (icon);
        entry->title = title;
        entries = g_slist_insert_sorted (entries, entry, (GCompareFunc) entry_compare);
    }
    for ( ; entries != NULL; entries = g_slist_delete_link (entries, entries)) {
        GSList *cur;
        GtkAction *bookmark;
        YelpMenuEntry *entry = (YelpMenuEntry *) entries->data;
        gchar *action_id = g_strconcat ("LoadBookmark-", entry->page_id, NULL);

        bookmark = gtk_action_group_get_action (priv->bookmark_actions, action_id);
        if (bookmark) {
            /* The action might have been set by a different document using
             * the same page ID. We can just reuse the action, since it's
             * just a page ID relative to the current URI, but we need to
             * reset the title and icon.
             */
            g_object_set (bookmark,
                          "label", entry->title,
                          "icon-name", entry->icon,
                          NULL);
        } else {
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
        g_free (entry->icon);
        g_free (entry);
    }

    g_variant_iter_free (iter);
    g_variant_unref (value);
}

static void
window_set_bookmark_action (YelpWindow *window)
{
    YelpUri *uri = NULL;
    gchar *doc_uri = NULL, *page_id = NULL;
    GtkAction *action_add, *action_del;
    gboolean bookmarked;
    YelpWindowPrivate *priv = GET_PRIV (window);

    action_add = gtk_action_group_get_action (priv->action_group, "AddBookmark");
    action_del = gtk_action_group_get_action (priv->action_group, "RemoveBookmark");

    g_object_get (priv->view,
                  "yelp-uri", &uri,
                  "page-id", &page_id,
                  NULL);
    if (page_id == NULL || uri == NULL) {
        gtk_action_set_sensitive (action_add, FALSE);
        gtk_action_set_sensitive (action_del, FALSE);
        goto done;
    }
    doc_uri = yelp_uri_get_document_uri (uri);
    bookmarked = yelp_application_is_bookmarked (YELP_BOOKMARKS (priv->application),
                                                 doc_uri, page_id);
    gtk_action_set_sensitive (action_add, !bookmarked);
    gtk_action_set_sensitive (action_del, bookmarked);

  done:
    g_free (page_id);
    g_free (doc_uri);
    if (uri)
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

static void
read_later_resolved (YelpUri    *uri,
                     YelpWindow *window)
{
    gchar *fulluri;
    const gchar *text = (const gchar *) g_object_get_data ((GObject *) uri, "link-text");
    YelpWindowPrivate *priv = GET_PRIV (window);
    YelpUri *base;
    gchar *doc_uri;

    g_object_get (priv->view, "yelp-uri", &base, NULL);
    doc_uri = yelp_uri_get_document_uri (uri);
    fulluri = yelp_uri_get_canonical_uri (uri);

    yelp_application_add_read_later (priv->application, doc_uri, fulluri, text);

    g_object_unref (base);
    g_free (doc_uri);
    g_free (fulluri);
}

static void
window_read_later (GtkAction   *action,
                   YelpWindow  *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    YelpUri *uri;
    gchar *text;

    uri = yelp_view_get_active_link_uri (priv->view);
    text = yelp_view_get_active_link_text (priv->view);

    g_object_set_data_full ((GObject *) uri, "link-text", text, g_free);

    if (!yelp_uri_is_resolved (uri)) {
        g_signal_connect (uri, "resolved",
                          G_CALLBACK (read_later_resolved),
                          window);
        yelp_uri_resolve (uri);
    }
    else {
        read_later_resolved (uri, window);
    }
}

static gboolean
read_later_clicked (GtkLinkButton  *button,
                    YelpWindow     *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    YelpUri *base;
    gchar *doc_uri;
    gchar *fulluri;

    fulluri = g_strdup (gtk_link_button_get_uri (button));

    g_object_get (priv->view, "yelp-uri", &base, NULL);
    doc_uri = yelp_uri_get_document_uri (base);

    yelp_application_remove_read_later (priv->application, doc_uri, fulluri);

    g_object_unref (base);
    g_free (doc_uri);

    yelp_view_load (priv->view, fulluri);

    g_free (fulluri);
    return TRUE;
}

static void
app_read_later_changed (YelpApplication *app,
                        const gchar     *doc_uri,
                        YelpWindow      *window)
{
    GVariant *value;
    GVariantIter *viter;
    gchar *uri, *title; /* do not free */
    GList *children;
    gboolean has_children = FALSE;
    YelpWindowPrivate *priv = GET_PRIV (window);

    children = gtk_container_get_children (GTK_CONTAINER (priv->read_later_vbox));
    while (children) {
        gtk_container_remove (GTK_CONTAINER (priv->read_later_vbox),
                              GTK_WIDGET (children->data));
        children = g_list_delete_link (children, children);
    }

    value = yelp_application_get_read_later (priv->application, doc_uri);
    g_variant_get (value, "a(ss)", &viter);
    while (g_variant_iter_loop (viter, "(&s&s)", &uri, &title)) {
        GtkWidget *align, *link;

        align = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
        g_object_set (align, "left-padding", 6, NULL);
        gtk_box_pack_start (GTK_BOX (priv->read_later_vbox), align, FALSE, FALSE, 0);

        link = gtk_link_button_new_with_label (uri, title);
        g_object_set (link, "xalign", 0.0, NULL);
        g_signal_connect (link, "activate-link", G_CALLBACK (read_later_clicked), window);
        gtk_container_add (GTK_CONTAINER (align), link);

        gtk_widget_show_all (align);
        has_children = TRUE;
    }
    g_variant_iter_free (viter);
    g_variant_unref (value);

    if (has_children) {
        if (gtk_widget_get_parent (priv->read_later) == NULL) {
            gtk_box_pack_end (GTK_BOX (priv->vbox_full), priv->read_later, FALSE, FALSE, 0);
            gtk_widget_show_all (priv->read_later);
        }
    }
    else {
        if (gtk_widget_get_parent (priv->read_later) != NULL)
            gtk_container_remove (GTK_CONTAINER (priv->vbox_full), priv->read_later);
    }
}

static void
window_find_in_page (GtkAction  *action,
                     YelpWindow *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);

    if (gtk_widget_get_parent (priv->find_bar) != NULL) {
        gtk_widget_grab_focus (priv->find_entry);
        return;
    }

    g_object_set (priv->find_entry, "width-request", 2 * priv->width / 3, NULL);

    gtk_box_pack_end (GTK_BOX (priv->vbox_view), priv->find_bar, FALSE, FALSE, 0);
    gtk_widget_show_all (priv->find_bar);
    gtk_widget_grab_focus (priv->find_entry);
}

static gboolean
find_entry_key_press (GtkEntry    *entry,
                      GdkEventKey *event,
                      YelpWindow  *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);

    if (event->keyval == GDK_KEY_Escape) {
        gtk_widget_grab_focus (GTK_WIDGET (priv->view));
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
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
    if (gtk_widget_get_parent (priv->find_bar) != NULL)
        gtk_container_remove (GTK_CONTAINER (priv->vbox_view), priv->find_bar); 
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
        gtk_label_set_text (GTK_LABEL (priv->find_label), "");
        return;
    }

    count = webkit_web_view_mark_text_matches (WEBKIT_WEB_VIEW (priv->view),
                                               text, FALSE, 0);
    if (count > 0) {
        gchar *label = g_strdup_printf (ngettext ("%i match", "%i matches", count), count);
        webkit_web_view_set_highlight_text_matches (WEBKIT_WEB_VIEW (priv->view), TRUE);
        webkit_web_view_search_text (WEBKIT_WEB_VIEW (priv->view),
                                     text, FALSE, TRUE, TRUE);
        gtk_label_set_text (GTK_LABEL (priv->find_label), label);
        g_free (label);
    }
    else {
        webkit_web_view_set_highlight_text_matches (WEBKIT_WEB_VIEW (priv->view), FALSE);
        gtk_label_set_text (GTK_LABEL (priv->find_label), _("No matches"));
    }

    g_free (text);
}

static gboolean
entry_color_animate (YelpWindow *window)
{
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

    YelpUri *uri;
    gchar *doc_uri;
    YelpViewState state;
    GdkWindow *gdkwin;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (view,
                  "yelp-uri", &uri,
                  "state", &state,
                  NULL);
    doc_uri = yelp_uri_get_document_uri (uri);

    gdkwin = gtk_widget_get_window (GTK_WIDGET (window));
    if (gdkwin != NULL)
        gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), NULL);

    if (state != YELP_VIEW_STATE_ERROR) {
        gchar *page_id, *icon, *title;
        g_object_get (view,
                      "page-id", &page_id,
                      "page-icon", &icon,
                      "page-title", &title,
                      NULL);
        yelp_application_update_bookmarks (priv->application,
                                           doc_uri,
                                           page_id,
                                           icon,
                                           title);
        app_read_later_changed (priv->application, doc_uri, window);
        g_free (page_id);
        g_free (icon);
        g_free (title);
    }

    g_free (doc_uri);
    g_object_unref (uri);
}

static void
view_uri_selected (YelpView     *view,
                   GParamSpec   *pspec,
                   YelpWindow   *window)
{
    YelpUri *uri;
    gchar *doc_uri;
    GdkWindow *gdkwin;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (G_OBJECT (view), "yelp-uri", &uri, NULL);
    if (uri == NULL)
        return;

    gdkwin = gtk_widget_get_window (GTK_WIDGET (window));
    if (gdkwin != NULL)
        gdk_window_set_cursor (gdkwin,
                               gdk_cursor_new_for_display (gdk_window_get_display (gdkwin),
                                                           GDK_WATCH));

    doc_uri = yelp_uri_get_document_uri (uri);
    if (priv->doc_uri == NULL || !g_str_equal (priv->doc_uri, doc_uri)) {
        window_set_bookmarks (window, doc_uri);
        g_free (priv->doc_uri);
        priv->doc_uri = doc_uri;
    }
    else {
        g_free (doc_uri);
    }

    g_object_unref (uri);
}

static gboolean
view_is_xref_uri (YelpView    *view,
                  GtkAction   *action,
                  const gchar *uri,
                  YelpWindow  *window)
{
    return g_str_has_prefix (uri, "xref:");
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

    if (gtk_widget_get_parent (priv->align_hidden) != NULL) {
        gtk_container_remove (GTK_CONTAINER (priv->hbox),
                              priv->align_hidden);
        gtk_box_pack_start (GTK_BOX (priv->hbox),
                            priv->align_location,
                            TRUE, TRUE, 0);
    }
}

static gboolean
hidden_key_press (GtkWidget    *widget,
                  GdkEventKey  *event,
                  YelpWindow   *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    if (event->keyval == GDK_KEY_Escape) {
        gtk_widget_grab_focus (GTK_WIDGET (priv->view));
        return TRUE;
    }
    return FALSE;
}
