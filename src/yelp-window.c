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

#include "pango/pango-layout.h"
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#include "yelp-search-entry.h"
#include "yelp-settings.h"
#include "yelp-uri.h"
#include "yelp-view.h"

#include "yelp-application.h"
#include "yelp-config.h"
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
static gboolean      yelp_window_close_request    (GtkWindow          *window);

static void          window_construct             (YelpWindow         *window);

static gboolean      window_on_drop               (GtkDropTarget      *target,
                                                   const GValue       *value,
                                                   double             x,
                                                   double             y,
                                                   YelpWindow         *window);
static gboolean      window_resize_signal         (YelpWindow         *window,
                                                   GParamSpec         *pspec,
                                                   gpointer            userdata);
static void          window_button_press          (GtkGestureClick    *event,
                                                   gint                n_press,
                                                   gdouble             x,
                                                   gdouble             y,
                                                   YelpWindow         *window);

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
static void          on_stop_search               (YelpSearchEntry    *search_entry,
                                                   YelpWindow         *window);
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


static gboolean      find_entry_key_press         (GtkEventControllerKey *event,
                                                   guint                  keyval,
                                                   guint                  keycode,
                                                   GdkModifierType       *state,
                                                   YelpWindow            *window);
static void          find_entry_changed           (GtkEntry           *entry,
                                                   YelpWindow         *window);
static void          find_prev_clicked            (GtkButton          *button,
                                                   YelpWindow         *window);
static void          find_next_clicked            (GtkButton          *button,
                                                   YelpWindow         *window);
static void          find_close_clicked           (GtkButton          *button,
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
static gboolean      ctrll_entry_key_press        (GtkEventControllerKey *event,
                                                   guint                  keyval,
                                                   guint                  keycode,
                                                   GdkModifierType       *state,
                                                   YelpWindow            *window);

static void          present_about_dialog         (YelpWindow *window);

enum {
    PROP_0,
    PROP_APPLICATION,
    PROP_ADAPTIVE_MODE
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
    gboolean     adaptive_mode;

    /* no refs on these, owned by containers */
    AdwBreakpoint *adaptive_mode_breakpoint;
    GtkWidget *header;
    GtkWidget *header_title;
    GtkWidget *search_bar;
    GtkWidget *search_entry_clamp;
    GtkWidget *search_entry;
    GtkWidget *find_bar;
    GtkWidget *find_entry;
    GtkWidget *find_prev_button;
    GtkWidget *find_next_button;
    GtkWidget *find_close_button;
    GtkWidget *bookmark_menu_button;
    GtkWidget *bookmark_menu;
    GtkWidget *bookmark_sw;
    GtkWidget *bookmark_list;
    GtkWidget *bookmark_add;
    GtkWidget *bookmark_remove;
    GtkWidget *font_adjustment_label;
    YelpView *view;
    GtkWidget *bottom_toolbar;

    GtkWidget *ctrll_entry;

    gchar *doc_uri;

    guint resize_signal;
    gint width;
    gint height;

    /* Event controllers */
    GtkGesture *gesture_click;
    GtkEventController *event_key;
    GtkEventController *event_find_entry_key;
    GtkEventController *event_ctrll_entry_key;
    GtkDropTarget *drop_target;
};

G_DEFINE_TYPE_WITH_PRIVATE (YelpWindow, yelp_window, ADW_TYPE_APPLICATION_WINDOW)

static void
update_font_scale (YelpWindow   *window,
                   YelpSettings *settings)
{
    double zoom_level;
    char *label;
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

    settings = yelp_settings_get_default ();
    zoom_level = yelp_settings_get_zoom_level (settings);

    label = g_strdup_printf ("%i%%", (int) round (zoom_level * 100));

    gtk_label_set_label (GTK_LABEL (priv->font_adjustment_label), label);

    g_free (label);
}

static void
on_font_scale_notify (YelpSettings       *settings,
                      GParamSpec         *pspec,
                      gpointer            user_data)
{
    YelpWindow *window = user_data;
    update_font_scale (window, settings);
}

static void
yelp_window_init (YelpWindow *window)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    YelpSettings *settings;
    GValue *true_val = g_new0 (GValue, 1);

    settings = yelp_settings_get_default ();

    gtk_widget_init_template (GTK_WIDGET (window));

    g_value_init (true_val, G_TYPE_BOOLEAN);
    g_value_set_boolean (true_val, TRUE);
    adw_breakpoint_add_setter(priv->adaptive_mode_breakpoint,
                              G_OBJECT (window),
                              "adaptive-mode",
                              true_val);

    g_signal_connect (settings, "notify::zoom-level", G_CALLBACK (on_font_scale_notify), window);

    update_font_scale (window, settings);
}

static void
yelp_window_class_init (YelpWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GtkWindowClass *window_class = GTK_WINDOW_CLASS (klass);

    object_class->dispose = yelp_window_dispose;
    object_class->finalize = yelp_window_finalize;
    object_class->get_property = yelp_window_get_property;
    object_class->set_property = yelp_window_set_property;
    window_class->close_request = yelp_window_close_request;

    g_object_class_install_property (object_class,
                                     PROP_APPLICATION,
                                     g_param_spec_object ("application",
							  "Application",
							  "A YelpApplication instance that controls this window",
                                                          YELP_TYPE_APPLICATION,
                                                          G_PARAM_CONSTRUCT_ONLY |
							  G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
							  G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property (object_class,
                                     PROP_ADAPTIVE_MODE,
                                     g_param_spec_boolean ("adaptive-mode",
                                     "Adaptive mode",
                                     "If true, makes the UI adapt to smaller screens.",
                                     FALSE,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                     G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    signals[RESIZE_EVENT] =
        g_signal_new ("resized",
                      G_OBJECT_CLASS_TYPE (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/yelp/yelp-window.ui");

    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, adaptive_mode_breakpoint);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, header);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, header_title);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, search_bar);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, search_entry_clamp);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, find_bar);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, find_entry);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, find_prev_button);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, find_next_button);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, find_close_button);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, bookmark_menu_button);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, bookmark_menu);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, bookmark_sw);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, bookmark_list);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, bookmark_add);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, bookmark_remove);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, view);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, font_adjustment_label);
    gtk_widget_class_bind_template_child_private (widget_class, YelpWindow, bottom_toolbar);

    gtk_widget_class_bind_template_callback (widget_class, window_resize_signal);
    gtk_widget_class_bind_template_callback (widget_class, window_search_mode);
    gtk_widget_class_bind_template_callback (widget_class, find_entry_changed);
    gtk_widget_class_bind_template_callback (widget_class, find_next_clicked);
    gtk_widget_class_bind_template_callback (widget_class, find_prev_clicked);
    gtk_widget_class_bind_template_callback (widget_class, find_close_clicked);
    gtk_widget_class_bind_template_callback (widget_class, view_new_window);
    gtk_widget_class_bind_template_callback (widget_class, view_loaded);
    gtk_widget_class_bind_template_callback (widget_class, view_is_loading_changed);
    gtk_widget_class_bind_template_callback (widget_class, view_uri_selected);
    gtk_widget_class_bind_template_callback (widget_class, window_set_bookmark_buttons);
    gtk_widget_class_bind_template_callback (widget_class, view_root_title);
    gtk_widget_class_bind_template_callback (widget_class, bookmark_activated);
    gtk_widget_class_bind_template_callback (widget_class, bookmark_added);
    gtk_widget_class_bind_template_callback (widget_class, bookmark_removed);

    gtk_widget_class_install_action (widget_class, "win.yelp-show-about-dialog", NULL,
                                     (GtkWidgetActionActivateFunc) present_about_dialog);
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
    case PROP_ADAPTIVE_MODE:
        g_value_set_boolean (value, priv->adaptive_mode);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
window_toggle_adaptive_mode (YelpWindow *window, gboolean enabled)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    priv->adaptive_mode = enabled;

    if (enabled) {
        gtk_widget_unparent (priv->bookmark_menu_button);
        gtk_action_bar_pack_end (GTK_ACTION_BAR (priv->bottom_toolbar), priv->bookmark_menu_button);
    } else {
        gtk_widget_unparent (priv->bookmark_menu_button);
        adw_header_bar_pack_end (ADW_HEADER_BAR (priv->header), priv->bookmark_menu_button);
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
    case PROP_ADAPTIVE_MODE:
        window_toggle_adaptive_mode (YELP_WINDOW (object), g_value_get_boolean (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
window_button_press (GtkGestureClick *event,
                     gint             n_press,
                     gdouble          x,
                     gdouble          y,
                     YelpWindow      *window)
{
    switch (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (event))) {
        case 8:
            g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window),
                                                           "yelp-view-go-back"),
                               NULL);
            break;

        case 9:
            g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window),
                                                           "yelp-view-go-forward"),
                               NULL);
            break;

        default:
            break;
    }
}

static void
window_construct (YelpWindow *window)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

    const GActionEntry entries[] = {
        { "yelp-window-new",    action_new_window,   NULL, NULL, NULL },
        { "yelp-window-close",  action_close_window, NULL, NULL, NULL },
        { "yelp-window-search", action_search,       NULL, NULL, NULL },
        { "yelp-window-find",   action_find,         NULL, NULL, NULL },
        { "yelp-window-go-all", action_go_all,       NULL, NULL, NULL },
        { "yelp-window-ctrll",  action_ctrll,        NULL, NULL, NULL },
    };

    g_action_map_add_action_entries (G_ACTION_MAP (window),
                                     entries, G_N_ELEMENTS (entries), window);
    yelp_view_register_actions (priv->view, G_ACTION_MAP (window));

    /* Search */
    priv->search_entry = yelp_search_entry_new (priv->view,
                                                YELP_BOOKMARKS (priv->application));
    g_object_set (priv->search_entry, "hexpand", TRUE, NULL);
    adw_clamp_set_child (ADW_CLAMP (priv->search_entry_clamp), priv->search_entry);
    gtk_search_bar_connect_entry (GTK_SEARCH_BAR (priv->search_bar), GTK_EDITABLE (priv->search_entry));
    gtk_search_bar_set_key_capture_widget (GTK_SEARCH_BAR (priv->search_bar), GTK_WIDGET (window));

    g_signal_connect (priv->search_entry, "stop-search",
                      G_CALLBACK (on_stop_search), window);

    /** Bookmarks **/
    priv->bookmarks_changed =
        g_signal_connect (priv->application, "bookmarks-changed",
                          G_CALLBACK (app_bookmarks_changed), window);

    /** Find **/
    priv->event_find_entry_key = gtk_event_controller_key_new();
    g_signal_connect (priv->event_find_entry_key, "key-pressed", G_CALLBACK (find_entry_key_press), window);
    gtk_widget_add_controller (priv->find_entry, GTK_EVENT_CONTROLLER (priv->event_find_entry_key));

    /** View **/
    window_set_bookmark_buttons (window);
    view_root_title (priv->view, NULL, window);
    gtk_widget_grab_focus (GTK_WIDGET (priv->view));

    /** Drag-and-drop **/
    priv->drop_target = gtk_drop_target_new (G_TYPE_STRING, GDK_ACTION_COPY);
    g_signal_connect (priv->drop_target, "drop", G_CALLBACK(window_on_drop), window);
    gtk_widget_add_controller (GTK_WIDGET (window), GTK_EVENT_CONTROLLER (priv->drop_target));

    /** Click/keyboard handlers **/
    priv->gesture_click = gtk_gesture_click_new();
    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->gesture_click), GTK_PHASE_CAPTURE);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->gesture_click), 0);
    g_signal_connect (priv->gesture_click, "pressed", G_CALLBACK (window_button_press), window);
    gtk_widget_add_controller (GTK_WIDGET (window), GTK_EVENT_CONTROLLER (priv->gesture_click));
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

gboolean
yelp_window_close_request(GtkWindow *window)
{
    yelp_application_window_close_request (YELP_APPLICATION (gtk_window_get_application (window)),
                                           window);

    return GTK_WINDOW_CLASS (yelp_window_parent_class)->close_request (window);
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

    gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (priv->search_bar), TRUE);
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
        priv->event_ctrll_entry_key = gtk_event_controller_key_new();
        g_signal_connect (priv->event_ctrll_entry_key, "key-pressed", G_CALLBACK (ctrll_entry_key_press), userdata);
        gtk_widget_add_controller (priv->ctrll_entry, GTK_EVENT_CONTROLLER (priv->event_ctrll_entry_key));
    }

    g_object_set (priv->ctrll_entry, "width-request", priv->width / 2, NULL);

    gtk_editable_set_text (GTK_EDITABLE (priv->ctrll_entry), "");

    adw_header_bar_set_title_widget (ADW_HEADER_BAR (priv->header), priv->ctrll_entry);
    gtk_widget_grab_focus (priv->ctrll_entry);

    g_object_get (priv->view, "yelp-uri", &yuri, NULL);
    if (yuri) {
        uri = yelp_uri_get_canonical_uri (yuri);
        g_object_unref (yuri);
    }
    if (uri) {
        gchar *c;
        gtk_editable_set_text (GTK_EDITABLE (priv->ctrll_entry), uri);
        c = strchr (uri, ':');
        if (c)
            gtk_editable_select_region (GTK_EDITABLE (priv->ctrll_entry), c - uri + 1, -1);
        else
            gtk_editable_select_region (GTK_EDITABLE (priv->ctrll_entry), 5, -1);
        g_free (uri);
    }
}


/******************************************************************************/

static gboolean
window_on_drop (GtkDropTarget *target,
                const GValue  *value,
                double         x,
                double         y,
                YelpWindow    *window)
{
    if (!G_VALUE_HOLDS (value, G_TYPE_STRING) || g_str_has_prefix(g_value_get_string (value), "xref:")) {
        return FALSE;
    }

    YelpUri *uri = yelp_uri_new (g_value_get_string (value));
    yelp_window_load_uri (window, uri);
    g_object_unref (uri);

    return TRUE;
}

static gboolean
window_resize_signal (YelpWindow *window, GParamSpec *pspec, gpointer userdata)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    int width, height;

    gtk_window_get_default_size (GTK_WINDOW (window), &width, &height);

    priv->width = width;
    priv->height = height;

    g_signal_emit (window, signals[RESIZE_EVENT], 0);
    priv->resize_signal = 0;
    return FALSE;
}

static void
bookmark_activated (GtkListBox    *box,
                    GtkListBoxRow *row,
                    YelpWindow    *window)
{
    YelpUri *base, *uri;
    gchar *xref;
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

    gtk_widget_set_visible (priv->bookmark_menu, FALSE);

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
    GSList *entries = NULL;

    window_set_bookmark_buttons (window);

    gtk_list_box_remove_all (GTK_LIST_BOX (priv->bookmark_list));

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
        GtkWidget *row, *box, *button, *label;
        YelpMenuEntry *entry = (YelpMenuEntry *) entries->data;

        row = gtk_list_box_row_new ();
        g_object_set_data_full (G_OBJECT (row), "page-id",
                                g_strdup (entry->page_id), (GDestroyNotify) g_free);
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), GTK_WIDGET (box));
        label = gtk_label_new (entry->title);
        gtk_widget_set_tooltip_text (label, entry->title);
        gtk_widget_set_hexpand (label, TRUE);
        gtk_label_set_xalign (GTK_LABEL (label), 0.0);
        gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
        gtk_widget_set_margin_start (label, 9);
        gtk_widget_set_margin_end (label, 9);
        gtk_widget_set_hexpand (label, true);
        gtk_box_append (GTK_BOX (box), label);
        button = gtk_button_new_from_icon_name ("edit-delete-symbolic");
        gtk_widget_add_css_class (button, "flat");
        g_object_set (button,
                      "focus-on-click", FALSE,
                      NULL);
        g_object_set_data_full (G_OBJECT (button), "page-id",
                                g_strdup (entry->page_id), (GDestroyNotify) g_free);
        g_signal_connect (button, "clicked", G_CALLBACK (bookmark_removed), window);
        gtk_box_append (GTK_BOX (box), button);
        gtk_list_box_append (GTK_LIST_BOX (priv->bookmark_list), row);
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
        gtk_widget_set_visible (priv->bookmark_add, FALSE);
        gtk_widget_set_visible (priv->bookmark_remove, FALSE);
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
on_stop_search (YelpSearchEntry  *search_entry,
                YelpWindow       *window)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

    gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (priv->search_bar), FALSE);
}

static void
window_search_mode (GtkSearchBar  *search_bar,
                    GParamSpec    *pspec,
                    YelpWindow    *window)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

    if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (search_bar))) {
        gtk_revealer_set_reveal_child (GTK_REVEALER (priv->find_bar), FALSE);
        gtk_entry_grab_focus_without_selecting (GTK_ENTRY (priv->search_entry));
    }
}

static gboolean
find_entry_key_press (GtkEventControllerKey *event,
                      guint                  keyval,
                      guint                  keycode,
                      GdkModifierType       *state,
                      YelpWindow            *window)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    WebKitFindController *find_controller;

    find_controller = webkit_web_view_get_find_controller (WEBKIT_WEB_VIEW (priv->view));

    if (keyval == GDK_KEY_Escape) {
        gtk_revealer_set_reveal_child (GTK_REVEALER (priv->find_bar), FALSE);
        gtk_widget_grab_focus (GTK_WIDGET (priv->view));
        webkit_find_controller_search_finish (find_controller);
        return TRUE;
    }

    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
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
find_close_clicked (GtkButton  *button,
                   YelpWindow *window)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    WebKitFindController *find_controller;

    find_controller = webkit_web_view_get_find_controller (WEBKIT_WEB_VIEW (priv->view));

    gtk_revealer_set_reveal_child (GTK_REVEALER (priv->find_bar), FALSE);
    gtk_widget_grab_focus (GTK_WIDGET (priv->view));
    webkit_find_controller_search_finish (find_controller);
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
    if (webkit_web_view_is_loading (WEBKIT_WEB_VIEW (view)))
        gtk_widget_set_cursor_from_name (GTK_WIDGET (window), "progress");
    else
        gtk_widget_set_cursor_from_name (GTK_WIDGET (window), "default");
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
        adw_window_title_set_title (ADW_WINDOW_TITLE (priv->header_title), page_title);
    else
        adw_window_title_set_title (ADW_WINDOW_TITLE (priv->header_title), _("Help"));

    if (root_title && (page_title == NULL || strcmp (root_title, page_title)))
        adw_window_title_set_subtitle (ADW_WINDOW_TITLE (priv->header_title), root_title);
    else
        adw_window_title_set_subtitle (ADW_WINDOW_TITLE (priv->header_title), NULL);
}

static void
ctrll_entry_activate (GtkEntry    *entry,
                      YelpWindow  *window)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);
    YelpUri *uri = yelp_uri_new (gtk_editable_get_text (GTK_EDITABLE (entry)));

    yelp_window_load_uri (window, uri);
    g_object_unref (uri);

    adw_header_bar_set_title_widget (ADW_HEADER_BAR (priv->header), priv->header_title);
}

static gboolean
ctrll_entry_key_press (GtkEventControllerKey *event,
                       guint                  keyval,
                       guint                  keycode,
                       GdkModifierType       *state,
                       YelpWindow            *window)
{
    YelpWindowPrivate *priv = yelp_window_get_instance_private (window);

    if (keyval == GDK_KEY_Escape) {
        adw_header_bar_set_title_widget (ADW_HEADER_BAR (priv->header), priv->header_title);
        return TRUE;
    }
    return FALSE;
}

static void
present_about_dialog (YelpWindow *window)
{
    static const gchar *developers[] = {
      "Shaun McCance <shaunm@gnome.org>",
      "Mikael Hallendal <micke@imendio.com>",
      "Alexander Larsson <alexl@redhat.com>",
      "Brent Smith <gnome@nextreality.net>",
      "Don Scorgie <Don@Scorgie.org>",
      "David King <amigadave@amigadave.com>",
      NULL
    };

    adw_show_about_dialog (GTK_WIDGET (window),
                           "application-name", _("Help"),
                           "application-icon", "org.gnome.Yelp",
                           "website", "https://apps.gnome.org/Yelp",
                           "issue-url", "https://gitlab.gnome.org/GNOME/yelp/issues",
                           "license-type", GTK_LICENSE_GPL_2_0,
                           "developer-name", _("The GNOME Project"),
                           "developers", developers,
                           "version", PACKAGE_VERSION,
                           /* TRANSLATORS Credit yourself here. Appears in about dialog. */
                           "translator-credits", _("translator-credits"),
                           NULL);
}
