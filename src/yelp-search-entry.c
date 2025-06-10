/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009-2020 Shaun McCance <shaunm@gnome.org>
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

#include "glib-object.h"
#include "yelp-bookmarks.h"
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "yelp-search-entry.h"
#include "yelp-marshal.h"
#include "yelp-settings.h"
#include "yelp-search-result.h"
#include "yelp-application.h"

static void     search_entry_constructed     (GObject           *object);
static void     search_entry_dispose         (GObject           *object);
static void     search_entry_finalize        (GObject           *object);
static void     search_entry_get_property    (GObject           *object,
					      guint              prop_id,
					      GValue            *value,
					      GParamSpec        *pspec);
static void     search_entry_set_property    (GObject           *object,
					      guint              prop_id,
					      const GValue      *value,
					      GParamSpec        *pspec);

static void     search_entry_size_allocate    (GtkWidget *widget,
                                               int           width,
                                               int           height,
                                               int           baseline);

/* Signals */
static void     search_entry_search_activated  (YelpSearchEntry *entry);
static void     search_entry_bookmark_clicked  (YelpSearchEntry *entry);


/* Callbacks */
static void     entry_changed_cb                    (YelpSearchEntry   *entry,
                                                     gpointer           user_data);
static void     entry_activate_cb                   (YelpSearchEntry   *entry,
                                                     gpointer           user_data);
static void     entry_focus_cb                      (YelpSearchEntry   *entry,
                                                     gpointer           user_data);
static void     completion_activate_cb              (YelpSearchEntry   *entry,
                                                     guint              position,
                                                     gpointer           user_data);

static gboolean  key_entry_cb                       (YelpSearchEntry        *entry,
                                                     guint                   keyval,
                                                     guint                   keycode,
                                                     GdkModifierType         state,
                                                     GtkEventControllerKey *controller);

static void bookmark_added_cb                       (YelpSearchEntry *entry,
                                                     gchar *doc_uri,
                                                     gchar *page_id);
static void bookmark_removed_cb                     (YelpSearchEntry *entry,
                                                     gchar *doc_uri,
                                                     gchar *page_id);

/* Filter/sorter functions */
static gboolean entry_match_func                    (YelpSearchResult *result,
                                                     YelpSearchEntry  *entry);
static gint     entry_completion_sort               (YelpSearchResult *res1,
                                                     YelpSearchResult *res2,
                                                     gpointer          user_data);

static gchar *  get_bookmark_icon                   (GtkListItem *item,
                                                     guint flags,
                                                     gpointer user_data);

/* YelpView callbacks */
static void          view_loaded                    (YelpView           *view,
                                                     YelpSearchEntry    *entry);


typedef struct _YelpSearchEntryPrivate  YelpSearchEntryPrivate;
struct _YelpSearchEntryPrivate
{
    YelpView *view;
    YelpBookmarks *bookmarks;

    gchar *completion_uri;
    gulong bookmark_added;
    gulong bookmark_removed;

    GtkWidget *completions_list;
    GtkWidget *completions_popover;
    GtkWidget *completions_scroll;
    GtkNoSelection *completions_selection_model;

    GtkSortListModel *completions_sort_model;
    YelpSearchResult *activate_search_result;

    GtkCustomSorter *completions_sorter;
    GtkCustomFilter *completions_filter;

    GtkEventController *focus_controller;
    GtkEventController *keyboard_controller;
    GtkEventController *popover_keyboard_controller;
};

enum {
    SEARCH_ACTIVATED,
    STOP_SEARCH,
    PREEDIT_CHANGED,
    LAST_SIGNAL
};

enum {  
    PROP_0,
    PROP_VIEW,
    PROP_BOOKMARKS
};

static GHashTable *completions;

static guint search_entry_signals[LAST_SIGNAL] = {0,};

G_DEFINE_TYPE_WITH_PRIVATE (YelpSearchEntry, yelp_search_entry, GTK_TYPE_ENTRY)

gboolean
on_escape (GtkWidget* widget,
           GVariant* args,
           gpointer user_data)
{
    g_signal_emit (YELP_SEARCH_ENTRY (widget), search_entry_signals[STOP_SEARCH], 0);
    return TRUE;
}

static void
yelp_search_entry_class_init (YelpSearchEntryClass *klass)
{
    GObjectClass *object_class;
    GtkWidgetClass *widget_class;

    klass->search_activated = search_entry_search_activated;
    klass->bookmark_clicked = search_entry_bookmark_clicked;

    object_class = G_OBJECT_CLASS (klass);
    widget_class = GTK_WIDGET_CLASS (klass);
  
    object_class->constructed = search_entry_constructed;
    object_class->dispose = search_entry_dispose;
    object_class->finalize = search_entry_finalize;
    object_class->get_property = search_entry_get_property;
    object_class->set_property = search_entry_set_property;

    widget_class->size_allocate = search_entry_size_allocate;

    /**
     * YelpSearchEntry::search-activated
     * @widget: The #YelpLocationEntry for which the signal was emitted.
     * @text: The search text.
     * @user_data: User data set when the handler was connected.
     *
     * This signal will be emitted whenever the user activates a search, generally
     * by pressing <keycap>Enter</keycap> in the embedded text entry while @widget
     * is in search mode.
     **/
    search_entry_signals[SEARCH_ACTIVATED] =
        g_signal_new ("search-activated",
                      G_OBJECT_CLASS_TYPE (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (YelpSearchEntryClass, search_activated),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE, 1,
                      G_TYPE_STRING);

    search_entry_signals[STOP_SEARCH] =
        g_signal_new ("stop-search",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    search_entry_signals[PREEDIT_CHANGED] =
        g_signal_new ("preedit-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    /**
     * YelpLocationEntry:view
     *
     * The YelpView instance that this location entry controls.
     **/
    g_object_class_install_property (object_class,
                                     PROP_VIEW,
                                     g_param_spec_object ("view",
							  "View",
							  "A YelpView instance to control",
                                                          YELP_TYPE_VIEW,
                                                          G_PARAM_CONSTRUCT_ONLY |
							  G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS));

    /**
     * YelpLocationEntry:bookmarks
     *
     * An instance of an implementation of YelpBookmarks to provide
     * bookmark information for this location entry.
     **/
    g_object_class_install_property (object_class,
                                     PROP_BOOKMARKS,
                                     g_param_spec_object ("bookmarks",
							  "Bookmarks",
							  "A YelpBookmarks implementation instance",
                                                          YELP_TYPE_BOOKMARKS,
                                                          G_PARAM_CONSTRUCT_ONLY |
							  G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS));

    completions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

    gtk_widget_class_add_binding (widget_class,
                                  GDK_KEY_Escape, 0,
                                  (GtkShortcutFunc) on_escape,
                                  NULL);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/yelp/yelp-search-entry.ui");

    gtk_widget_class_bind_template_child_private (widget_class, YelpSearchEntry, completions_popover);
    gtk_widget_class_bind_template_child_private (widget_class, YelpSearchEntry, completions_list);
    gtk_widget_class_bind_template_child_private (widget_class, YelpSearchEntry, completions_scroll);
    gtk_widget_class_bind_template_child_private (widget_class, YelpSearchEntry, completions_selection_model);

    gtk_widget_class_bind_template_callback (widget_class, entry_activate_cb);
    gtk_widget_class_bind_template_callback (widget_class, entry_changed_cb);
    gtk_widget_class_bind_template_callback (widget_class, completion_activate_cb);
    gtk_widget_class_bind_template_callback (widget_class, get_bookmark_icon);
}

static void
preedit_changed_cb (YelpSearchEntry *entry, gchar *preedit) {
    g_signal_emit_by_name (entry, "preedit-changed", preedit);
}

static void
yelp_search_entry_init (YelpSearchEntry *entry)
{
    YelpSearchEntryPrivate *priv = yelp_search_entry_get_instance_private (entry);
    GListStore *activate_search_model, *search_concat_model;
    GtkFlattenListModel *search_flatten_model;
    GtkFilterListModel *completions_filter_model;
    GtkSliceListModel *completions_slice_model;

    gtk_widget_init_template (GTK_WIDGET (entry));

    /* Search icon */
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
                                       GTK_ENTRY_ICON_PRIMARY,
                                       "system-search-symbolic");

    /* GtkSearchBar requires the preedit-changed property */
    g_signal_connect_swapped (gtk_editable_get_delegate (GTK_EDITABLE (entry)),
                              "preedit-changed",
                              G_CALLBACK (preedit_changed_cb),
                              entry);

    /* Focus controller used to open/close the popover */
    priv->focus_controller = gtk_event_controller_focus_new ();
    g_signal_connect_swapped (priv->focus_controller, "notify::contains-focus",
                              G_CALLBACK (entry_focus_cb),
                              entry);
    gtk_widget_add_controller (GTK_WIDGET (entry), priv->focus_controller);

    /* Hack to work around entry not bubbling down events to delegate correctly */
    priv->keyboard_controller = gtk_event_controller_key_new ();
    g_signal_connect_swapped (priv->keyboard_controller, "key-pressed",
                              G_CALLBACK (key_entry_cb), entry);
    g_signal_connect_swapped (priv->keyboard_controller, "key-released",
                              G_CALLBACK (key_entry_cb), entry);
    gtk_widget_add_controller (GTK_WIDGET (entry), priv->keyboard_controller);

    /* Keyboard controller to redirect typing events from the popover to the entry */
    priv->popover_keyboard_controller = gtk_event_controller_key_new ();
    g_signal_connect_swapped (priv->popover_keyboard_controller, "key-pressed",
                              G_CALLBACK (key_entry_cb), entry);
    g_signal_connect_swapped (priv->popover_keyboard_controller, "key-released",
                              G_CALLBACK (key_entry_cb), entry);
    gtk_widget_add_controller (priv->completions_popover,
                               priv->popover_keyboard_controller);

    /* Sorter and filter used for the completions list */
    priv->completions_sorter =
        gtk_custom_sorter_new ((GCompareDataFunc) entry_completion_sort,
                               entry, NULL);
    priv->completions_filter =
        gtk_custom_filter_new ((GtkCustomFilterFunc) entry_match_func,
                               entry, NULL);

    priv->completions_sort_model =
        g_object_new (GTK_TYPE_SORT_LIST_MODEL,
                      "sorter",
                      GTK_SORTER (priv->completions_sorter),
                      NULL);

    completions_filter_model =
        gtk_filter_list_model_new(G_LIST_MODEL (priv->completions_sort_model),
                                  GTK_FILTER (priv->completions_filter));

    search_concat_model = g_list_store_new (G_TYPE_LIST_MODEL);

    /*
     * Completions are sliced to only show up to 6 items at a time (a 7th item,
     * the "Search for X" option, is appended at the end later).
     */
    completions_slice_model = gtk_slice_list_model_new(G_LIST_MODEL (completions_filter_model), 0, 6);

    g_list_store_append (search_concat_model, completions_slice_model);

    /*
     * The "Search for X" result is stored in a separate ListStore, then
     * concatenated to the end of the full completion model with a GtkFlattenListModel.
     */
    activate_search_model = g_list_store_new (YELP_TYPE_SEARCH_RESULT);
    priv->activate_search_result = g_object_new (YELP_TYPE_SEARCH_RESULT,
                                            /* Title is set dynamically */
                                            "desc", _("Browse all search results"),
                                            "icon", "edit-find-symbolic",
                                            "flags", COMPLETION_FLAG_ACTIVATE_SEARCH,
                                            NULL);
    g_list_store_append (activate_search_model, priv->activate_search_result);

    g_list_store_append (search_concat_model, activate_search_model);

    search_flatten_model = gtk_flatten_list_model_new (G_LIST_MODEL (search_concat_model));

    gtk_no_selection_set_model(priv->completions_selection_model, G_LIST_MODEL (search_flatten_model));
}

static void
search_entry_constructed (GObject *object)
{
    YelpSearchEntryPrivate *priv =
        yelp_search_entry_get_instance_private (YELP_SEARCH_ENTRY (object));

    g_signal_connect (priv->view, "loaded", G_CALLBACK (view_loaded), object);
}

static void
search_entry_dispose (GObject *object)
{
    YelpSearchEntryPrivate *priv =
        yelp_search_entry_get_instance_private (YELP_SEARCH_ENTRY (object));

    if (priv->bookmark_added) {
        g_signal_handler_disconnect (priv->bookmarks, priv->bookmark_added);
        priv->bookmark_added = 0;
    }

    if (priv->bookmark_removed) {
        g_signal_handler_disconnect (priv->bookmarks, priv->bookmark_removed);
        priv->bookmark_removed = 0;
    }

    if (priv->view) {
        g_object_unref (priv->view);
        priv->view = NULL;
    }

    if (priv->bookmarks) {
        g_object_unref (priv->bookmarks);
        priv->bookmarks = NULL;
    }

    gtk_widget_dispose_template (GTK_WIDGET (object), YELP_TYPE_SEARCH_ENTRY);

    G_OBJECT_CLASS (yelp_search_entry_parent_class)->dispose (object);
}

static void
search_entry_size_allocate (GtkWidget *widget,
                                               int           width,
                                               int           height,
                                               int           baseline)
{
    YelpSearchEntryPrivate *priv =
        yelp_search_entry_get_instance_private (YELP_SEARCH_ENTRY (widget));

    GTK_WIDGET_CLASS (yelp_search_entry_parent_class)->size_allocate (widget,
                                                                      width,
                                                                      height,
                                                                      baseline);

    gtk_widget_set_size_request (priv->completions_popover,
                                 gtk_widget_get_width (widget), -1);
    gtk_widget_queue_resize (priv->completions_popover);
    gtk_popover_present (GTK_POPOVER (priv->completions_popover));
}

static void
search_entry_finalize (GObject *object)
{
    YelpSearchEntryPrivate *priv =
        yelp_search_entry_get_instance_private (YELP_SEARCH_ENTRY (object));

    g_free (priv->completion_uri);

    G_OBJECT_CLASS (yelp_search_entry_parent_class)->finalize (object);
}

static void
search_entry_get_property   (GObject      *object,
                               guint         prop_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
    YelpSearchEntryPrivate *priv =
        yelp_search_entry_get_instance_private (YELP_SEARCH_ENTRY (object));

    switch (prop_id) {
    case PROP_VIEW:
        g_value_set_object (value, priv->view);
        break;
    case PROP_BOOKMARKS:
        g_value_set_object (value, priv->bookmarks);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
search_entry_set_property   (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    YelpSearchEntryPrivate *priv =
        yelp_search_entry_get_instance_private (YELP_SEARCH_ENTRY (object));

    switch (prop_id) {
    case PROP_VIEW:
        priv->view = g_value_dup_object (value);
        break;
    case PROP_BOOKMARKS:
        priv->bookmarks = g_value_dup_object (value);

        /* Set up bookmark change bindings */
        if (priv->bookmark_added)
            g_signal_handler_disconnect (priv->bookmarks, priv->bookmark_added);

        priv->bookmark_added = g_signal_connect_swapped (priv->bookmarks,
                                                         "bookmark-added",
                                                         G_CALLBACK (bookmark_added_cb),
                                                         YELP_SEARCH_ENTRY (object));

        if (priv->bookmark_removed)
            g_signal_handler_disconnect (priv->bookmarks, priv->bookmark_removed);

        priv->bookmark_removed = g_signal_connect_swapped (priv->bookmarks,
                                                           "bookmark-removed",
                                                           G_CALLBACK (bookmark_removed_cb),
                                                           YELP_SEARCH_ENTRY (object));

        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
search_entry_search_activated  (YelpSearchEntry *entry)
{
    YelpUri *base, *uri;
    YelpSearchEntryPrivate *priv = yelp_search_entry_get_instance_private (entry);

    g_object_get (priv->view, "yelp-uri", &base, NULL);
    if (base == NULL)
        return;
    uri = yelp_uri_new_search (base,
                               gtk_editable_get_text (GTK_EDITABLE (entry)));
    g_object_unref (base);
    yelp_view_load_uri (priv->view, uri);
    gtk_widget_grab_focus (GTK_WIDGET (priv->view));
}

static void
search_entry_bookmark_clicked  (YelpSearchEntry *entry)
{
    YelpUri *uri;
    gchar *doc_uri, *page_id;
    YelpSearchEntryPrivate *priv = yelp_search_entry_get_instance_private (entry);

    g_object_get (priv->view,
                  "yelp-uri", &uri,
                  "page-id", &page_id,
                  NULL);
    doc_uri = yelp_uri_get_document_uri (uri);
    if (priv->bookmarks && doc_uri && page_id) {
        if (!yelp_bookmarks_is_bookmarked (priv->bookmarks, doc_uri, page_id)) {
            gchar *icon, *title;
            g_object_get (priv->view,
                          "page-icon", &icon,
                          "page-title", &title,
                          NULL);
            yelp_bookmarks_add_bookmark (priv->bookmarks, doc_uri, page_id, icon, title);
            g_free (icon);
            g_free (title);
        }
        else {
            yelp_bookmarks_remove_bookmark (priv->bookmarks, doc_uri, page_id);
        }
    }
    g_free (doc_uri);
    g_free (page_id);
    g_object_unref (uri);
}

static void
entry_changed_cb (YelpSearchEntry  *entry,
                   gpointer   user_data)
{
    YelpSearchEntryPrivate *priv = yelp_search_entry_get_instance_private (entry);
    gchar *text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (entry)));
    GtkAdjustment *vadjust;
    gchar *title;

    if (text == NULL || strlen(text) == 0) {
        gtk_popover_popdown (GTK_POPOVER (priv->completions_popover));
        g_free (text);
        return;
    }

    if (!gtk_widget_get_visible(priv->completions_popover))
        gtk_popover_popup (GTK_POPOVER (priv->completions_popover));

    gtk_filter_changed (GTK_FILTER (priv->completions_filter),
                        GTK_FILTER_CHANGE_DIFFERENT);

    /* Scroll to the top of the list */
    vadjust = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->completions_scroll));
    gtk_adjustment_set_value (vadjust, gtk_adjustment_get_lower (vadjust));

    /* Update label on the "Search for X" result */
    title = g_strdup_printf (_("Search for “%s”"), text);
    g_object_set (priv->activate_search_result, "title", title, NULL);

    g_free (title);
    g_free (text);
}

static void
entry_focus_cb (YelpSearchEntry  *entry,
                   gpointer   user_data)
{
    YelpSearchEntryPrivate *priv = yelp_search_entry_get_instance_private (entry);
    gchar *text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (entry)));
    gboolean has_focus;

    has_focus = gtk_event_controller_focus_contains_focus (
                        GTK_EVENT_CONTROLLER_FOCUS (priv->focus_controller));

    if (!(text == NULL || strlen(text) == 0) && has_focus)
        gtk_popover_popup (GTK_POPOVER (priv->completions_popover));
    else if (!has_focus)
        gtk_popover_popdown (GTK_POPOVER (priv->completions_popover));
}

static void
entry_activate_cb (YelpSearchEntry  *entry,
                   gpointer   user_data)
{
    YelpSearchEntryPrivate *priv = yelp_search_entry_get_instance_private (entry);
    gchar *text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (entry)));

    gtk_popover_popdown (GTK_POPOVER (priv->completions_popover));

    if (text == NULL || strlen(text) == 0)
        return;

    g_signal_emit_by_name (priv->completions_list, "activate", 0);
    g_signal_emit (entry, search_entry_signals[STOP_SEARCH], 0, text);

    g_free (text);
}

static gboolean
key_entry_cb (YelpSearchEntry *entry, guint keyval, guint keycode, GdkModifierType state, GtkEventControllerKey *controller)
{
    if (keyval == GDK_KEY_Tab       || keyval == GDK_KEY_KP_Tab ||
        keyval == GDK_KEY_Up        || keyval == GDK_KEY_KP_Up ||
        keyval == GDK_KEY_Down      || keyval == GDK_KEY_KP_Down ||
        keyval == GDK_KEY_Left      || keyval == GDK_KEY_KP_Left ||
        keyval == GDK_KEY_Right     || keyval == GDK_KEY_KP_Right ||
        keyval == GDK_KEY_Home      || keyval == GDK_KEY_KP_Home ||
        keyval == GDK_KEY_End       || keyval == GDK_KEY_KP_End ||
        keyval == GDK_KEY_Page_Up   || keyval == GDK_KEY_KP_Page_Up ||
        keyval == GDK_KEY_Page_Down || keyval == GDK_KEY_KP_Page_Down ||
        keyval == GDK_KEY_Escape ||
        ((state & (GDK_CONTROL_MASK | GDK_ALT_MASK)) != 0)) {
            return GDK_EVENT_PROPAGATE;
    }

    gtk_event_controller_key_forward (controller,
                                      GTK_WIDGET (gtk_editable_get_delegate (GTK_EDITABLE (entry))));

    return GDK_EVENT_STOP;
}

static gboolean
entry_match_func (YelpSearchResult *result, YelpSearchEntry *entry)
{
    const gchar *key = g_utf8_casefold (gtk_editable_get_text (GTK_EDITABLE (entry)), -1);
    gint stri;
    gchar *title, *desc, *keywords, *titlecase = NULL, *desccase = NULL, *keywordscase = NULL;
    gboolean ret = FALSE;
    gchar **strs;
    gint flags;
    static GRegex *nonword = NULL;

    if (nonword == NULL)
        nonword = g_regex_new ("\\W", 0, 0, NULL);
    if (nonword == NULL)
        return FALSE;

    g_object_get(result,
                 "title", &title,
                 "desc", &desc,
                 "keywords", &keywords,
                 "flags", &flags,
                 NULL);

    if (title) {
        titlecase = g_utf8_casefold (title, -1);
        g_free (title);
    }
    if (desc) {
        desccase = g_utf8_casefold (desc, -1);
        g_free (desc);
    }
    if (keywords) {
        keywordscase = g_utf8_casefold (keywords, -1);
        g_free (keywords);
    }

    strs = g_regex_split (nonword, key, 0);
    ret = TRUE;
    for (stri = 0; strs[stri]; stri++) {
        if (!titlecase || !strstr (titlecase, strs[stri])) {
            if (!desccase || !strstr (desccase, strs[stri])) {
                if (!keywordscase || !strstr (keywordscase, strs[stri])) {
                    ret = FALSE;
                    break;
                }
            }
        }
    }

    g_free (titlecase);
    g_free (desccase);
    g_strfreev (strs);

    return ret;
}

static gint
entry_completion_sort (YelpSearchResult *res1,
                       YelpSearchResult *res2,
                       gpointer      user_data)
{
    gint ret = 0;
    gint flags1, flags2;
    gchar *title1, *title2, *icon1, *icon2;

    g_object_get (res1,
                  "flags", &flags1,
                  "icon", &icon1,
                  "title", &title1,
                  NULL);

    g_object_get (res2,
                  "flags", &flags2,
                  "icon", &icon2,
                  "title", &title2,
                  NULL);

    ret = yelp_settings_cmp_icons (icon1, icon2);

    if (ret)
        goto out;

    if (flags1 & COMPLETION_FLAG_BOOKMARKED)
        ret = -1;
    else if (flags2 & COMPLETION_FLAG_BOOKMARKED)
        ret = 1;

    if (ret)
        goto out;

    if (title1 && title2)
        ret = g_utf8_collate (title1, title2);
    else if (title2 == NULL)
        ret = -1;
    else if (title1 == NULL)
        ret = 1;

out:
    g_free (icon1);
    g_free (icon2);
    g_free (title1);
    g_free (title2);
    return ret;
}

static void
completion_activate_cb (YelpSearchEntry *entry, guint position, gpointer user_data)
{
    YelpUri *base, *uri;
    gchar *page, *xref;
    gint flags;
    YelpSearchResult *result;
    YelpSearchEntryPrivate *priv = yelp_search_entry_get_instance_private (entry);

    result = g_list_model_get_item (G_LIST_MODEL (priv->completions_selection_model), position);

    g_object_get(result,
                 "page", &page,
                 "flags", &flags,
                 NULL);

    if (flags & COMPLETION_FLAG_ACTIVATE_SEARCH) {
        gchar *text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (entry)));
        g_signal_emit (entry, search_entry_signals[SEARCH_ACTIVATED], 0, text);
        g_signal_emit (entry, search_entry_signals[STOP_SEARCH], 0, text);
        g_free (text);
        g_free (page);
        return;
    }

    g_object_get (priv->view, "yelp-uri", &base, NULL);

    xref = g_strconcat ("xref:", page, NULL);
    uri = yelp_uri_new_relative (base, xref);

    yelp_view_load_uri (priv->view, uri);

    g_free (page);
    g_free (xref);
    g_object_unref (uri);
    g_object_unref (base);

    gtk_widget_grab_focus (GTK_WIDGET (priv->view));
    return;
}

static void
_bookmark_add_remove (YelpSearchEntry *entry, gchar *doc_uri, gchar *page_id, gboolean added)
{
    YelpSearchEntryPrivate *priv = yelp_search_entry_get_instance_private (entry);
    GListModel *model;
    guint n_items, i;
    gboolean page_found = FALSE;

    model = (GListModel *) g_hash_table_lookup (completions, doc_uri);

    n_items = g_list_model_get_n_items(model);

    for (i = 0; i < n_items; i++) {
        YelpSearchResult *result;
        gchar *this_page;
        guint flags;

        result = g_list_model_get_item(model, i);

        if (!result)
            break;

        g_object_get (result, "page", &this_page, "flags", &flags, NULL);

        if (!g_strcmp0 (this_page, page_id)) {
            if (added)
                g_object_set (result,
                              "flags", flags | COMPLETION_FLAG_BOOKMARKED,
                              NULL);
            else
                g_object_set (result,
                              "flags", flags & ~COMPLETION_FLAG_BOOKMARKED,
                              NULL);
            page_found = TRUE;
            g_free (this_page);
            break;
        }

        g_free (this_page);
    }

    if (page_found) {
        gtk_sorter_changed (GTK_SORTER (priv->completions_sorter),
                            GTK_SORTER_CHANGE_DIFFERENT);
        gtk_filter_changed (GTK_FILTER (priv->completions_filter),
                            GTK_FILTER_CHANGE_DIFFERENT);
    }

}

static void
bookmark_added_cb (YelpSearchEntry *entry, gchar *doc_uri, gchar *page_id)
{
    _bookmark_add_remove(entry, doc_uri, page_id, true);
}

static void
bookmark_removed_cb (YelpSearchEntry *entry, gchar *doc_uri, gchar *page_id)
{
    _bookmark_add_remove(entry, doc_uri, page_id, false);
}

static gchar *
get_bookmark_icon (GtkListItem *item, guint flags, gpointer user_data)
{
    gchar *icon_name = NULL;

    if (flags & COMPLETION_FLAG_BOOKMARKED)
        icon_name = g_strdup ("user-bookmarks-symbolic");

    return icon_name;
}

static void
view_loaded (YelpView          *view,
             YelpSearchEntry *entry)
{
    gchar **ids;
    gint i;
    YelpUri *uri;
    gchar *doc_uri;
    GListStore *completion;
    YelpSearchEntryPrivate *priv = yelp_search_entry_get_instance_private (entry);
    YelpDocument *document = yelp_view_get_document (view);

    g_object_get (view, "yelp-uri", &uri, NULL);
    doc_uri = yelp_uri_get_document_uri (uri);

    if ((priv->completion_uri == NULL) || 
        !g_str_equal (doc_uri, priv->completion_uri)) {
        completion = (GListStore *) g_hash_table_lookup (completions, doc_uri);
        if (completion == NULL) {
            completion = g_list_store_new (YELP_TYPE_SEARCH_RESULT);

            g_hash_table_insert (completions, g_strdup (doc_uri), completion);
            if (document != NULL) {
                ids = yelp_document_list_page_ids (document);
                for (i = 0; ids[i]; i++) {
                    YelpSearchResult *search_result;
                    gchar *title, *desc, *icon, *keywords;
                    gboolean is_bookmarked;

                    title = yelp_document_get_page_title (document, ids[i]);
                    desc = yelp_document_get_page_desc (document, ids[i]);
                    icon = yelp_document_get_page_icon (document, ids[i]);
                    keywords = yelp_document_get_page_keywords (document, ids[i]);
                    is_bookmarked = yelp_bookmarks_is_bookmarked(priv->bookmarks, doc_uri, ids[i]);

                    search_result = g_object_new (YELP_TYPE_SEARCH_RESULT,
                                                  "title", title,
                                                  "desc", desc,
                                                  "icon", icon,
                                                  "keywords", keywords,
                                                  "page", ids[i],
                                                  "flags", is_bookmarked ? COMPLETION_FLAG_BOOKMARKED : 0,
                                                  NULL);

                    g_list_store_append(completion, search_result);

                    g_free (keywords);
                    g_free (icon);
                    g_free (desc);
                    g_free (title);
                }
                g_strfreev (ids);
            }

            gtk_sort_list_model_set_model (priv->completions_sort_model,
                                           G_LIST_MODEL (completion));
        }

        gtk_sorter_changed (GTK_SORTER (priv->completions_sorter),
                            GTK_SORTER_CHANGE_DIFFERENT);
        gtk_filter_changed (GTK_FILTER (priv->completions_filter),
                            GTK_FILTER_CHANGE_DIFFERENT);

        g_free (priv->completion_uri);
        priv->completion_uri = doc_uri;
    }

    g_object_unref (uri);
}

/**
 * yelp_search_entry_new:
 * @view: A #YelpView.
 *
 * Creates a new #YelpSearchEntry widget to control @view.
 *
 * Returns: A new #YelpSearchEntry.
 **/
GtkWidget*
yelp_search_entry_new (YelpView      *view,
		       YelpBookmarks *bookmarks)
{
    GtkWidget *ret;
    g_return_val_if_fail (YELP_IS_VIEW (view), NULL);

    ret = GTK_WIDGET (g_object_new (YELP_TYPE_SEARCH_ENTRY,
                                    "view", view,
                                    "bookmarks", bookmarks,
                                    NULL));

    return ret;
}
