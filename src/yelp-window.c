/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2010-2020 Shaun McCance <shaunm@gnome.org>
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "yelp-search-entry.h"
#include "yelp-settings.h"
#include "yelp-uri.h"
#include "yelp-view.h"

#include "yelp-application.h"
#include "yelp-window.h"

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
static gboolean      window_key_press             (YelpWindow         *window,
                                                   GdkEventKey        *event,
                                                   gpointer            userdata);

static void          bookmark_activated           (GtkListBox         *box,
                                                   GtkListBoxRow      *row,
                                                   YelpWindow         *window);
static void          bookmark_removed             (GtkButton          *button,
                                                   YelpWindow         *window);
static void          bookmark_added               (GtkButton          *button,
                                                   YelpWindow         *window);

static void          app_bookmarks_changed        (YelpApplication    *app,
                                                   const gchar        *doc_uri,
                                                   YelpWindow         *window);
static void          window_set_bookmarks         (YelpWindow         *window,
                                                   const gchar        *doc_uri);
static void          window_set_bookmark_buttons  (YelpWindow         *window);
static void          window_search_mode           (GtkSearchBar       *search_bar,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);

static void          action_new_window            (GSimpleAction      *action,
                                                   GVariant           *parameter,
                                                   gpointer           userdata);
static void          action_close_window          (GSimpleAction      *action,
                                                   GVariant           *parameter,
                                                   gpointer           userdata);
static void          action_search                (GSimpleAction      *action,
                                                   GVariant           *parameter,
                                                   gpointer            userdata);
static void          action_find                  (GSimpleAction      *action,
                                                   GVariant           *parameter,
                                                   gpointer            userdata);
static void          action_go_all                (GSimpleAction      *action,
                                                   GVariant           *parameter,
                                                   gpointer            userdata);
static void          action_ctrll                 (GSimpleAction      *action,
                                                   GVariant           *parameter,
                                                   gpointer            userdata);


static gboolean      find_entry_key_press         (GtkEntry           *entry,
                                                   GdkEventKey        *event,
                                                   YelpWindow         *window);
static void          find_entry_changed           (GtkEntry           *entry,
                                                   YelpWindow         *window);
static void          find_prev_clicked            (GtkButton          *button,
                                                   YelpWindow         *window);
static void          find_next_clicked            (GtkButton          *button,
                                                   YelpWindow         *window);

static void          view_new_window              (YelpView           *view,
                                                   YelpUri            *uri,
                                                   YelpWindow         *window);
static void          view_loaded                  (YelpView           *view,
                                                   YelpWindow         *window);
static void          view_is_loading_changed      (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);
static void          view_uri_selected            (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);
static void          view_root_title              (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);
static void          ctrll_entry_activate         (GtkEntry           *entry,
                                                   YelpWindow         *window);
static gboolean      ctrll_entry_key_press        (GtkWidget          *widget,
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

#define MAX_FIND_MATCHES 1000

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct _YelpWindowPrivate YelpWindowPrivate;
struct _YelpWindowPrivate {
    YelpApplication *application;

    gulong        bookmarks_changed;

    /* no refs on these, owned by containers */
    GtkWidget *header;
    GtkWidget *vbox_view;
    GtkWidget *vbox_full;
    GtkWidget *search_bar;
    GtkWidget *search_entry;
    GtkWidget *find_bar;
    GtkWidget *find_entry;
    GtkWidget *bookmark_menu;
    GtkWidget *bookmark_sw;
    GtkWidget *bookmark_list;
    GtkWidget *bookmark_add;
    GtkWidget *bookmark_remove;
    YelpView *view;

    GtkWidget *ctrll_entry;

    gchar *doc_uri;

    guint resize_signal;
    gint width;
    gint height;

    gboolean configured;
};

G_DEFINE_TYPE_WITH_PRIVATE (YelpWindow, yelp_window, HDY_TYPE_APPLICATION_WINDOW)

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
							  "Application",
							  "A YelpApplication instance that controls this window",
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
}

static void
yelp_window_dispose (GObject *object)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (YELP_WINDOW (object));

    if (priv->bookmarks_changed) {
        g_signal_handler_disconnect (priv->application, priv->bookmarks_changed);
        priv->bookmarks_changed = 0;
    }

    if (priv->ctrll_entry) {
        g_object_unref (priv->ctrll_entry);
        priv->ctrll_entry = NULL;
    }

    G_OBJECT_CLASS (yelp_window_parent_class)->dispose (object);
}

static void
yelp_window_finalize (GObject *object)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (YELP_WINDOW (object));
    g_free (priv->doc_uri);
    G_OBJECT_CLASS (yelp_window_parent_class)->finalize (object);
}

static void
yelp_window_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (YELP_WINDOW (object));
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
    YelpWindowPrivate *priv = yelp_window_get_instance_private (YELP_WINDOW (object));
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

static gboolean
window_button_press (YelpWindow *window, GdkEventButton *event, gpointer userdata)
{
    switch (event->button) {
        case 8:
            g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window),
                                                           "yelp-view-go-back"),
                               NULL);
            return TRUE;

        case 9:
            g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window),
                                                           "yelp-view-go-forward"),
                               NULL);
            return TRUE;

        default:
            return FALSE;
    }
}

static void
window_construct (YelpWindow *window)
{
    GtkWidget *box, *button;
    GtkWidget *frame;
    GtkCssProvider *css;
    GtkSizeGroup *size_group;
    GMenu *menu, *section;
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

    const GActionEntry entries[] = {
        { "yelp-window-new",    action_new_window,   NULL, NULL, NULL },
        { "yelp-window-close",  action_close_window, NULL, NULL, NULL },
        { "yelp-window-search", action_search,       NULL, NULL, NULL },
        { "yelp-window-find",   action_find,         NULL, NULL, NULL },
        { "yelp-window-go-all", action_go_all,       NULL, NULL, NULL },
        { "yelp-window-ctrll",  action_ctrll,        NULL, NULL, NULL },
    };

    gtk_window_set_icon_name (GTK_WINDOW (window), "org.gnome.Yelp");

    priv->view = (YelpView *) yelp_view_new ();

    g_action_map_add_action_entries (G_ACTION_MAP (window),
                                     entries, G_N_ELEMENTS (entries), window);
    yelp_view_register_actions (priv->view, G_ACTION_MAP (window));

    priv->vbox_full = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add (GTK_CONTAINER (window), priv->vbox_full);

    priv->header = hdy_header_bar_new ();
    hdy_header_bar_set_show_close_button (HDY_HEADER_BAR (priv->header), TRUE);
    gtk_container_add (GTK_CONTAINER (priv->vbox_full), priv->header);

    /** Back/Forward **/
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class (gtk_widget_get_style_context (box), "linked");
    hdy_header_bar_pack_start (HDY_HEADER_BAR (priv->header), box);

    button = gtk_button_new_from_icon_name ("go-previous-symbolic", GTK_ICON_SIZE_MENU);
    gtk_widget_set_tooltip_text (button, _("Back"));
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
    g_object_set (button, "action-name", "win.yelp-view-go-back", NULL);

    button = gtk_button_new_from_icon_name ("go-next-symbolic", GTK_ICON_SIZE_MENU);
    gtk_widget_set_tooltip_text (button, _("Forward"));
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
    g_object_set (button, "action-name", "win.yelp-view-go-forward", NULL);

    /** Gear Menu **/
    button = gtk_menu_button_new ();
    gtk_menu_button_set_direction (GTK_MENU_BUTTON (button), GTK_ARROW_NONE);
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
    gtk_widget_set_tooltip_text (button, _("Menu"));
    hdy_header_bar_pack_end (HDY_HEADER_BAR (priv->header), button);

    menu = g_menu_new ();
    section = g_menu_new ();
    g_menu_append (section, _("New Window"), "win.yelp-window-new");
    g_menu_append (section, _("Find…"), "win.yelp-window-find");
    g_menu_append (section, _("Print…"), "win.yelp-view-print");
    g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
    g_object_unref (section);

    section = g_menu_new ();
    g_menu_append (section, _("Previous Page"), "win.yelp-view-go-previous");
    g_menu_append (section, _("Next Page"), "win.yelp-view-go-next");
    g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
    g_object_unref (section);

    section = g_menu_new ();
    g_menu_append (section, _("Larger Text"), "app.yelp-application-larger-text");
    g_menu_append (section, _("Smaller Text"), "app.yelp-application-smaller-text");
    g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
    g_object_unref (section);

    section = g_menu_new ();
    g_menu_append (section, _("All Help"), "win.yelp-window-go-all");
    g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
    g_object_unref (section);

    gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), G_MENU_MODEL (menu));

    /** Search **/
    priv->vbox_view = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start (GTK_BOX (priv->vbox_full), priv->vbox_view, TRUE, TRUE, 0);

    priv->search_bar = gtk_search_bar_new ();
    gtk_box_pack_start (GTK_BOX (priv->vbox_view), priv->search_bar, FALSE, FALSE, 0);
    priv->search_entry = yelp_search_entry_new (priv->view,
                                                YELP_BOOKMARKS (priv->application));
    gtk_entry_set_width_chars (GTK_ENTRY (priv->search_entry), 50);
    gtk_container_add (GTK_CONTAINER (priv->search_bar), priv->search_entry);
    button = gtk_toggle_button_new ();
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
    gtk_button_set_image (GTK_BUTTON (button),
                          gtk_image_new_from_icon_name ("edit-find-symbolic",
                                                        GTK_ICON_SIZE_MENU));
    gtk_widget_set_tooltip_text (button, _("Search (Ctrl+S)"));
    g_object_bind_property (button, "active",
                            priv->search_bar, "search-mode-enabled",
                            G_BINDING_BIDIRECTIONAL);
    g_signal_connect (priv->search_bar, "notify::search-mode-enabled",
                      G_CALLBACK (window_search_mode), window);
    hdy_header_bar_pack_end (HDY_HEADER_BAR (priv->header), button);

    g_signal_connect (window, "key-press-event", G_CALLBACK (window_key_press), NULL);

    /** Bookmarks **/
    button = gtk_menu_button_new ();
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
    gtk_button_set_image (GTK_BUTTON (button),
                          gtk_image_new_from_icon_name ("user-bookmarks-symbolic",
                                                        GTK_ICON_SIZE_MENU));
    gtk_widget_set_tooltip_text (button, _("Bookmarks"));
    hdy_header_bar_pack_end (HDY_HEADER_BAR (priv->header), button);

    priv->bookmark_menu = gtk_popover_new (button);
    g_object_set (priv->bookmark_menu, "border-width", 12, NULL);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), priv->bookmark_menu);

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add (GTK_CONTAINER (priv->bookmark_menu), box);
    priv->bookmark_sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->bookmark_sw),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_AUTOMATIC);
    g_object_set (priv->bookmark_sw, "height-request", 200, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (priv->bookmark_sw), GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX (box), priv->bookmark_sw, TRUE, TRUE, 0);
    priv->bookmark_list = gtk_list_box_new ();
    button = gtk_label_new (_("No bookmarks"));
    gtk_widget_show (button);
    gtk_list_box_set_placeholder (GTK_LIST_BOX (priv->bookmark_list), button);
    g_object_set (priv->bookmark_list, "selection-mode", GTK_SELECTION_NONE, NULL);
    g_signal_connect (priv->bookmark_list, "row-activated",
                      G_CALLBACK (bookmark_activated), window);
    gtk_container_add (GTK_CONTAINER (priv->bookmark_sw), priv->bookmark_list);

    priv->bookmark_add = gtk_button_new_with_label (_("Add Bookmark"));
    g_signal_connect (priv->bookmark_add, "clicked",
                      G_CALLBACK (bookmark_added), window);
    gtk_box_pack_end (GTK_BOX (box), priv->bookmark_add, FALSE, FALSE, 0);
    gtk_widget_show_all (box);

    priv->bookmark_remove = gtk_button_new_with_label (_("Remove Bookmark"));
    g_signal_connect (priv->bookmark_remove, "clicked",
                      G_CALLBACK (bookmark_removed), window);
    gtk_box_pack_end (GTK_BOX (box), priv->bookmark_remove, FALSE, FALSE, 0);
    gtk_widget_show_all (box);

    priv->bookmarks_changed =
        g_signal_connect (priv->application, "bookmarks-changed",
                          G_CALLBACK (app_bookmarks_changed), window);

    /** Find **/
    css = gtk_css_provider_new ();
    /* FIXME: Connect to parsing-error signal. */
    gtk_css_provider_load_from_data (css,
                                     ".yelp-find-frame {"
                                     "    background-color: @theme_base_color;"
                                     "    padding: 6px;"
                                     "    border-color: @borders;"
                                     "    border-radius: 0 0 3px 3px;"
                                     "    border-width: 0 1px 1px 1px;"
                                     "    border-style: solid;"
                                     "}",
                                     -1, NULL);
    priv->find_bar = gtk_revealer_new ();
    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
    gtk_style_context_add_class (gtk_widget_get_style_context (frame),
                                 "yelp-find-frame");
    gtk_style_context_add_provider (gtk_widget_get_style_context (frame),
                                    GTK_STYLE_PROVIDER (css),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    g_object_set (priv->find_bar,
                  "halign", GTK_ALIGN_END,
                  "margin-end", 6,
                  "valign", GTK_ALIGN_START,
                  NULL);
    gtk_style_context_add_class (gtk_widget_get_style_context (box), "linked");
    gtk_container_add (GTK_CONTAINER (frame), box);
    gtk_container_add (GTK_CONTAINER (priv->find_bar), frame);

    g_object_unref (css);

    size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

    priv->find_entry = gtk_search_entry_new ();
    gtk_entry_set_width_chars (GTK_ENTRY (priv->find_entry), 30);
    gtk_size_group_add_widget (size_group, priv->find_entry);
    gtk_box_pack_start (GTK_BOX (box), priv->find_entry, TRUE, TRUE, 0);
    g_signal_connect (priv->find_entry, "changed",
                      G_CALLBACK (find_entry_changed), window);
    g_signal_connect (priv->find_entry, "key-press-event",
                      G_CALLBACK (find_entry_key_press), window);

    button = gtk_button_new_from_icon_name ("go-up-symbolic", GTK_ICON_SIZE_MENU);
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_style_context_add_class (gtk_widget_get_style_context (button), "raised");
    gtk_size_group_add_widget (size_group, button);
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
    g_signal_connect (button, "clicked", G_CALLBACK (find_prev_clicked), window);

    button = gtk_button_new_from_icon_name ("go-down-symbolic", GTK_ICON_SIZE_MENU);
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_style_context_add_class (gtk_widget_get_style_context (button), "raised");
    gtk_size_group_add_widget (size_group, button);
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
    g_signal_connect (button, "clicked", G_CALLBACK (find_next_clicked), window);

    g_object_unref (size_group);

    /** View **/
    box = gtk_overlay_new ();
    gtk_overlay_add_overlay (GTK_OVERLAY (box), GTK_WIDGET (priv->find_bar));

    gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (priv->view));
    gtk_box_pack_start (GTK_BOX (priv->vbox_view), box, TRUE, TRUE, 0);

    g_signal_connect (priv->view, "new-view-requested", G_CALLBACK (view_new_window), window);
    g_signal_connect (priv->view, "loaded", G_CALLBACK (view_loaded), window);
    g_signal_connect (priv->view, "notify::is-loading", G_CALLBACK (view_is_loading_changed), window);
    g_signal_connect (priv->view, "notify::yelp-uri", G_CALLBACK (view_uri_selected), window);
    g_signal_connect_swapped (priv->view, "notify::page-id",
                              G_CALLBACK (window_set_bookmark_buttons), window);
    window_set_bookmark_buttons (window);
    g_signal_connect (priv->view, "notify::root-title", G_CALLBACK (view_root_title), window);
    gtk_widget_grab_focus (GTK_WIDGET (priv->view));

    gtk_drag_dest_set (GTK_WIDGET (window),
                       GTK_DEST_DEFAULT_ALL,
                       NULL, 0,
                       GDK_ACTION_COPY);
    gtk_drag_dest_add_uri_targets (GTK_WIDGET (window));
    g_signal_connect (window, "drag-data-received",
                      G_CALLBACK (window_drag_received), NULL);

    g_signal_connect (window, "button-press-event", G_CALLBACK (window_button_press), NULL);
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
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

    yelp_view_load_uri (priv->view, uri);
}

YelpUri *
yelp_window_get_uri (YelpWindow *window)
{
    YelpUri *uri;
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    g_object_get (G_OBJECT (priv->view), "yelp-uri", &uri, NULL);
    return uri;
}

void
yelp_window_get_geometry (YelpWindow  *window,
                          gint        *width,
                          gint        *height)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    *width = priv->width;
    *height = priv->height;
}

/******************************************************************************/

static void
action_new_window (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       userdata)
{
    YelpUri *yuri;
    gchar *uri = NULL;
    YelpWindow *window = YELP_WINDOW (userdata);
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

    g_object_get (priv->view, "yelp-uri", &yuri, NULL);
    uri = yelp_uri_get_document_uri (yuri);

    yelp_application_new_window (priv->application, uri);

    g_free (uri);
    g_object_unref (yuri);
}

static void
action_close_window (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       userdata)
{
    gtk_window_close (GTK_WINDOW (userdata));
}

static void
action_search (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       userdata)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (userdata);

    gtk_revealer_set_reveal_child (GTK_REVEALER (priv->find_bar), FALSE);
    gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (priv->search_bar), TRUE);
    gtk_widget_grab_focus (priv->search_entry);
}

static void
action_find (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       userdata)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (userdata);

    gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (priv->search_bar), FALSE);
    gtk_revealer_set_reveal_child (GTK_REVEALER (priv->find_bar), TRUE);
    gtk_widget_grab_focus (priv->find_entry);
}

static void
action_go_all (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       userdata)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (userdata);
    yelp_view_load (priv->view, "help-list:");
}

static void
action_ctrll (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       userdata)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (userdata);
    YelpUri *yuri;
    gchar *uri = NULL;

    if (priv->ctrll_entry == NULL) {
        priv->ctrll_entry = gtk_entry_new ();
        g_object_ref_sink (priv->ctrll_entry);

        g_signal_connect (priv->ctrll_entry, "activate",
                          G_CALLBACK (ctrll_entry_activate), userdata);
        g_signal_connect (priv->ctrll_entry, "key-press-event",
                          G_CALLBACK (ctrll_entry_key_press), userdata);
    }

    g_object_set (priv->ctrll_entry, "width-request", priv->width / 2, NULL);

    gtk_entry_set_text (GTK_ENTRY (priv->ctrll_entry), "");

    hdy_header_bar_set_custom_title (HDY_HEADER_BAR (priv->header), priv->ctrll_entry);
    gtk_widget_show (priv->ctrll_entry);
    gtk_widget_grab_focus (priv->ctrll_entry);

    g_object_get (priv->view, "yelp-uri", &yuri, NULL);
    if (yuri) {
        uri = yelp_uri_get_canonical_uri (yuri);
        g_object_unref (yuri);
    }
    if (uri) {
        gchar *c;
        gtk_entry_set_text (GTK_ENTRY (priv->ctrll_entry), uri);
        c = strchr (uri, ':');
        if (c)
            gtk_editable_select_region (GTK_EDITABLE (priv->ctrll_entry), c - uri + 1, -1);
        else
            gtk_editable_select_region (GTK_EDITABLE (priv->ctrll_entry), 5, -1);
        g_free (uri);
    }
}


/******************************************************************************/

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
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    priv->configured = TRUE;
    return FALSE;
}

static gboolean
window_configure_event (YelpWindow         *window,
                        GdkEventConfigure  *event,
                        gpointer            user_data)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
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
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    g_signal_emit (window, signals[RESIZE_EVENT], 0);
    priv->resize_signal = 0;
    return FALSE;
}

static gboolean
window_key_press (YelpWindow  *window,
                  GdkEventKey *event,
                  gpointer     userdata)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

    if (gtk_revealer_get_reveal_child (GTK_REVEALER (priv->find_bar)))
        return FALSE;

    if (hdy_header_bar_get_custom_title (HDY_HEADER_BAR (priv->header)))
        return FALSE;

    return gtk_search_bar_handle_event (GTK_SEARCH_BAR (priv->search_bar),
                                        (GdkEvent *) event);
}

static void
bookmark_activated (GtkListBox    *box,
                    GtkListBoxRow *row,
                    YelpWindow    *window)
{
    YelpUri *base, *uri;
    gchar *xref;
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

    gtk_widget_hide (priv->bookmark_menu);

    xref = g_strconcat ("xref:",
                        (gchar *) g_object_get_data (G_OBJECT (row), "page-id"),
                        NULL);
    g_object_get (priv->view, "yelp-uri", &base, NULL);
    uri = yelp_uri_new_relative (base, xref);

    yelp_view_load_uri (priv->view, uri);

    g_object_unref (base);
    g_object_unref (uri);
    g_free (xref);
}

static void
bookmark_removed (GtkButton  *button,
                  YelpWindow *window)
{
    YelpUri *uri;
    gchar *doc_uri;
    gchar *page_id = NULL;
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

    g_object_get (priv->view, "yelp-uri", &uri, NULL);
    doc_uri = yelp_uri_get_document_uri (uri);

    /* The 'Remove Bookmark' button removes a bookmark for the current page.
       The buttons next to each bookmark have page_id attached to them.
     */
    if ((gpointer) button == (gpointer) priv->bookmark_remove)
        g_object_get (priv->view,
                      "page-id", &page_id,
                      NULL);

    yelp_application_remove_bookmark (YELP_BOOKMARKS (priv->application),
                                      doc_uri,
                                      page_id ? page_id :
                                      g_object_get_data (G_OBJECT (button), "page-id"));
    if (page_id)
        g_free (page_id);
    g_free (doc_uri);
    g_object_unref (uri);
}

static void
bookmark_added (GtkButton  *button,
                YelpWindow *window)
{
    YelpUri *uri;
    gchar *doc_uri, *page_id, *icon, *title;
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

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
app_bookmarks_changed (YelpApplication *app,
                       const gchar     *doc_uri,
                       YelpWindow      *window)
{
    YelpUri *uri;
    gchar *this_doc_uri;
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

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
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    GList *children, *cur;
    GSList *entries = NULL;

    window_set_bookmark_buttons (window);

    children = gtk_container_get_children (GTK_CONTAINER (priv->bookmark_list));
    for (cur = children ; cur != NULL; cur = cur->next) {
        gtk_container_remove (GTK_CONTAINER (priv->bookmark_list),
                              GTK_WIDGET (cur->data));
    }
    g_list_free (children);

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
        GtkWidget *row, *box, *button;
        YelpMenuEntry *entry = (YelpMenuEntry *) entries->data;

        row = gtk_list_box_row_new ();
        g_object_set_data_full (G_OBJECT (row), "page-id",
                                g_strdup (entry->page_id), (GDestroyNotify) g_free);
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_container_add (GTK_CONTAINER (row), box);
        button = gtk_label_new (entry->title);
        g_object_set (button, "halign", GTK_ALIGN_START, NULL);
        gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
        button = gtk_button_new_from_icon_name ("edit-delete-symbolic", GTK_ICON_SIZE_MENU);
        g_object_set (button,
                      "relief", GTK_RELIEF_NONE,
                      "focus-on-click", FALSE,
                      NULL);
        g_object_set_data_full (G_OBJECT (button), "page-id",
                                g_strdup (entry->page_id), (GDestroyNotify) g_free);
        g_signal_connect (button, "clicked", G_CALLBACK (bookmark_removed), window);
        gtk_box_pack_end (GTK_BOX (box), button, FALSE, FALSE, 0);
        gtk_box_pack_end (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);
        gtk_widget_show_all (row);
        gtk_container_add (GTK_CONTAINER (priv->bookmark_list), row);
        g_free (entry->icon);
        g_free (entry);
    }

    g_variant_iter_free (iter);
    g_variant_unref (value);
}

static void
window_set_bookmark_buttons (YelpWindow *window)
{
    YelpUri *uri = NULL;
    gchar *doc_uri = NULL, *page_id = NULL;
    gboolean bookmarked;
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);


    g_object_get (priv->view,
                  "yelp-uri", &uri,
                  "page-id", &page_id,
                  NULL);
    if (page_id == NULL || uri == NULL) {
        gtk_widget_hide (priv->bookmark_add);
        gtk_widget_hide (priv->bookmark_remove);
        goto done;
    }
    doc_uri = yelp_uri_get_document_uri (uri);
    bookmarked = yelp_application_is_bookmarked (YELP_BOOKMARKS (priv->application),
                                                 doc_uri, page_id);

    gtk_widget_set_visible (priv->bookmark_add, !bookmarked);
    gtk_widget_set_visible (priv->bookmark_remove, bookmarked);

  done:
    g_free (page_id);
    g_free (doc_uri);
    if (uri)
        g_object_unref (uri);
}

static void
window_search_mode (GtkSearchBar  *search_bar,
                    GParamSpec    *pspec,
                    YelpWindow    *window)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

    if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (search_bar)))
        gtk_revealer_set_reveal_child (GTK_REVEALER (priv->find_bar), FALSE);
}

static gboolean
find_entry_key_press (GtkEntry    *entry,
                      GdkEventKey *event,
                      YelpWindow  *window)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    WebKitFindController *find_controller;

    find_controller = webkit_web_view_get_find_controller (WEBKIT_WEB_VIEW (priv->view));

    if (event->keyval == GDK_KEY_Escape) {
        gtk_revealer_set_reveal_child (GTK_REVEALER (priv->find_bar), FALSE);
        gtk_widget_grab_focus (GTK_WIDGET (priv->view));
        webkit_find_controller_search_finish (find_controller);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        webkit_find_controller_search_next (find_controller);
        return TRUE;
    }

    return FALSE;
}

static void
find_entry_changed (GtkEntry   *entry,
                    YelpWindow *window)
{
    gchar *text;
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    WebKitFindController *find_controller;

    find_controller = webkit_web_view_get_find_controller (WEBKIT_WEB_VIEW (priv->view));

    text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);

    webkit_find_controller_search (find_controller, text,
      WEBKIT_FIND_OPTIONS_WRAP_AROUND | WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE,
      MAX_FIND_MATCHES);

    g_free (text);
}

static void
find_prev_clicked (GtkButton  *button,
                   YelpWindow *window)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    WebKitFindController *find_controller;

    find_controller = webkit_web_view_get_find_controller (WEBKIT_WEB_VIEW (priv->view));
    webkit_find_controller_search_previous (find_controller);
}

static void
find_next_clicked (GtkButton  *button,
                   YelpWindow *window)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    WebKitFindController *find_controller;

    find_controller = webkit_web_view_get_find_controller (WEBKIT_WEB_VIEW (priv->view));
    webkit_find_controller_search_next (find_controller);
}

static void
view_new_window (YelpView   *view,
                 YelpUri    *uri,
                 YelpWindow *window)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    yelp_application_new_window_uri (priv->application, uri);
}

static void
view_loaded (YelpView   *view,
             YelpWindow *window)
{

    YelpUri *uri;
    gchar *doc_uri;
    YelpViewState state;
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

    g_object_get (view,
                  "yelp-uri", &uri,
                  "state", &state,
                  NULL);
    doc_uri = yelp_uri_get_document_uri (uri);

    if (state != YELP_VIEW_STATE_ERROR) {
        gchar *page_id, *icon, *title;
        g_object_get (view,
                      "page-id", &page_id,
                      "page-icon", &icon,
                      "page-title", &title,
                      NULL);
        if (!g_str_has_prefix (page_id, "search=")) {
            gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (priv->search_bar), FALSE);
        }
        yelp_application_update_bookmarks (priv->application,
                                           doc_uri,
                                           page_id,
                                           icon,
                                           title);
        g_free (page_id);
        g_free (icon);
        g_free (title);
    }

    g_free (doc_uri);
    g_object_unref (uri);
}

static void
view_is_loading_changed (YelpView   *view,
                         GParamSpec *pspec,
                         YelpWindow *window)
{
    GdkWindow *gdkwin;

    gdkwin = gtk_widget_get_window (GTK_WIDGET (window));
    if (!gdkwin)
        return;

    if (webkit_web_view_is_loading (WEBKIT_WEB_VIEW (view))) {
        gdk_window_set_cursor (gdkwin,
                               gdk_cursor_new_for_display (gdk_window_get_display (gdkwin),
                                                           GDK_WATCH));
    } else {
        gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), NULL);
    }
}

static void
view_uri_selected (YelpView     *view,
                   GParamSpec   *pspec,
                   YelpWindow   *window)
{
    YelpUri *uri;
    gchar *doc_uri;
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

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

    g_object_unref (uri);
}

static void
view_root_title (YelpView    *view,
                 GParamSpec  *pspec,
                 YelpWindow  *window)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    gchar *root_title, *page_title;
    g_object_get (view, "root-title", &root_title, "page-title", &page_title, NULL);

    if (page_title)
        hdy_header_bar_set_title (HDY_HEADER_BAR (priv->header), page_title);
    else
        hdy_header_bar_set_title (HDY_HEADER_BAR (priv->header), _("Help"));

    if (root_title && (page_title == NULL || strcmp (root_title, page_title)))
        hdy_header_bar_set_subtitle (HDY_HEADER_BAR (priv->header), root_title);
    else
        hdy_header_bar_set_subtitle (HDY_HEADER_BAR (priv->header), NULL);
}

static void
ctrll_entry_activate (GtkEntry    *entry,
                      YelpWindow  *window)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    YelpUri *uri = yelp_uri_new (gtk_entry_get_text (entry));

    yelp_window_load_uri (window, uri);
    g_object_unref (uri);

    hdy_header_bar_set_custom_title (HDY_HEADER_BAR (priv->header), NULL);
}

static gboolean
ctrll_entry_key_press (GtkWidget    *widget,
                       GdkEventKey  *event,
                       YelpWindow   *window)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

    if (event->keyval == GDK_KEY_Escape) {
        hdy_header_bar_set_custom_title (HDY_HEADER_BAR (priv->header), NULL);
        return TRUE;
    }
    return FALSE;
}
