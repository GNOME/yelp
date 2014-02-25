/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009 Shaun McCance <shaunm@gnome.org>
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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "yelp-location-entry.h"
#include "yelp-marshal.h"
#include "yelp-settings.h"

/**
 * SECTION:yelp-location-entry
 * @short_description: A location entry with history and search
 * @include: yelp.h
 *
 * #YelpLocationEntry is a #GtkComboBox designed to show the current location,
 * provide a drop-down menu of previous locations, and allow the user to perform
 * searches.
 *
 * The #GtkTreeModel used by a #YelpLocationEntry is expected to have at least
 * four columns: #GtkComboBox::entry-text-column contains the displayed name
 * of the location, #YelpLocationEntry::desc-column contains a description
 * for each entry, #YelpLocationEntry::icon-column contains an icon name for
 * the location, and #YelpLocationEntry::flags-column contains a bit field
 * of #YelpLocationEntryFlags.  These columns are specified when creating a
 * #YelpLocationEntry widget with yelp_location_entry_new_with_model().
 *
 * Usually, a single row in the #GtkTreeModel corresponds to a location.  When
 * a user selects a row from the drop-down menu, the icon and text for that
 * row will be placed in the embedded text entry, and the
 * #YelpLocationEntry:location-selected signal will be emitted.
 *
 * If a row has the %YELP_LOCATION_ENTRY_IS_SEARCH flag set, selecting that
 * row will not emit the #YelpLocationEntry::location-selected signal.  Instead,
 * the #YelpLocationEntry widget will be placed into search mode, as if by a
 * call to yelp_location_entry_start_search().
 *
 * When a row has the %YELP_LOCATION_ENTRY_CAN_BOOKMARK flag set, an icon
 * will be displayed in the secondary icon position of the embedded text
 * entry allowing the user to bookmark the location.  Clicking this icon
 * will cause the FIXME signal to be emitted.
 **/

static void     location_entry_constructed     (GObject           *object);
static void     location_entry_dispose         (GObject           *object);
static void     location_entry_finalize        (GObject           *object);
static void     location_entry_get_property    (GObject           *object,
                                                guint              prop_id,
                                                GValue            *value,
                                                GParamSpec        *pspec);
static void     location_entry_set_property    (GObject           *object,
                                                guint              prop_id,
                                                const GValue      *value,
                                                GParamSpec        *pspec);

/* Signals */
static void     location_entry_location_selected (YelpLocationEntry *entry);
static void     location_entry_search_activated  (YelpLocationEntry *entry);
static void     location_entry_bookmark_clicked  (YelpLocationEntry *entry);

/* Utilities */
static void     location_entry_start_search    (YelpLocationEntry *entry,
                                                gboolean           clear);
static void     location_entry_cancel_search   (YelpLocationEntry *entry);
static void     location_entry_set_entry       (YelpLocationEntry *entry,
                                                gboolean           emit);
static gboolean location_entry_start_pulse     (gpointer           data);
static gboolean location_entry_pulse           (gpointer           data);
static void     location_entry_set_completion  (YelpLocationEntry *entry,
                                                GtkTreeModel      *model);


/* GtkTreeModel callbacks */
static void     history_row_changed                 (GtkTreeModel      *model,
                                                     GtkTreePath       *path,
                                                     GtkTreeIter       *iter,
                                                     gpointer           user_data);
/* GtkComboBox callbacks */
static void     combo_box_changed_cb                (GtkComboBox       *widget,
                                                     gpointer           user_data);
static gboolean combo_box_row_separator_func        (GtkTreeModel      *model,
                                                     GtkTreeIter       *iter,
                                                     gpointer           user_data);
/* GtkEntry callbacks */
static gboolean entry_focus_in_cb                   (GtkWidget         *widget,
                                                     GdkEventFocus     *event,
                                                     gpointer           user_data);
static gboolean entry_focus_out_cb                  (GtkWidget         *widget,
                                                     GdkEventFocus     *event,
                                                     gpointer           user_data);
static void     entry_activate_cb                   (GtkEntry          *text_entry,
                                                     gpointer           user_data);
static void     entry_icon_press_cb                 (GtkEntry          *gtkentry,
                                                     GtkEntryIconPosition icon_pos,
                                                     GdkEvent          *event,
                                                     YelpLocationEntry *entry);
static gboolean entry_key_press_cb                  (GtkWidget         *widget,
                                                     GdkEventKey       *event,
                                                     YelpLocationEntry *entry);
/* GtkCellLayout callbacks */
static void     cell_set_text_cell                  (GtkCellLayout     *layout,
                                                     GtkCellRenderer   *cell,
                                                     GtkTreeModel      *model,
                                                     GtkTreeIter       *iter,
                                                     YelpLocationEntry *entry);
static void     cell_set_bookmark_icon              (GtkCellLayout     *layout,
                                                     GtkCellRenderer   *cell,
                                                     GtkTreeModel      *model,
                                                     GtkTreeIter       *iter,
                                                     YelpLocationEntry *entry);
/* GtkEntryCompletion callbacks */
static void     cell_set_completion_bookmark_icon   (GtkCellLayout     *layout,
                                                     GtkCellRenderer   *cell,
                                                     GtkTreeModel      *model,
                                                     GtkTreeIter       *iter,
                                                     YelpLocationEntry *entry);
static void     cell_set_completion_text_cell       (GtkCellLayout     *layout,
                                                     GtkCellRenderer   *cell,
                                                     GtkTreeModel      *model,
                                                     GtkTreeIter       *iter,
                                                     YelpLocationEntry *entry);
static gboolean entry_match_func                    (GtkEntryCompletion *completion,
                                                     const gchar        *key,
                                                     GtkTreeIter        *iter,
                                                     YelpLocationEntry  *entry);
static gint     entry_completion_sort               (GtkTreeModel       *model,
                                                     GtkTreeIter        *iter1,
                                                     GtkTreeIter        *iter2,
                                                     gpointer            user_data);
static gboolean entry_match_selected                (GtkEntryCompletion *completion,
                                                     GtkTreeModel       *model,
                                                     GtkTreeIter        *iter,
                                                     YelpLocationEntry  *entry);
/* YelpView callbacks */
static void          view_loaded                    (YelpView           *view,
                                                     YelpLocationEntry  *entry);
static void          view_uri_selected              (YelpView           *view,
                                                     GParamSpec         *pspec,
                                                     YelpLocationEntry  *entry);
static void          view_page_title                (YelpView           *view,
                                                     GParamSpec         *pspec,
                                                     YelpLocationEntry  *entry);
static void          view_page_desc                 (YelpView           *view,
                                                     GParamSpec         *pspec,
                                                     YelpLocationEntry  *entry);
static void          view_page_icon                 (YelpView           *view,
                                                     GParamSpec         *pspec,
                                                     YelpLocationEntry  *entry);
/* YelpBookmarks callbacks */
static void          bookmarks_changed              (YelpBookmarks      *bookmarks,
                                                     const gchar        *doc_uri,
                                                     YelpLocationEntry  *entry);



typedef struct _YelpLocationEntryPrivate  YelpLocationEntryPrivate;
struct _YelpLocationEntryPrivate
{
    YelpView *view;
    YelpBookmarks *bookmarks;
    GtkTreeRowReference *row;
    gchar *completion_uri;

    /* do not free below */
    GtkWidget          *text_entry;
    GtkCellRenderer    *icon_cell;
    GtkListStore       *history;
    GtkEntryCompletion *completion;

    gboolean   enable_search;
    gboolean   view_uri_selected;
    gboolean   search_mode;
    guint      pulse;
    gulong bookmarks_changed;

    gboolean   icon_is_clear;
};

enum {
    LOCATION_ENTRY_IS_LOADING    = 1 << 0,
    LOCATION_ENTRY_IS_SEPARATOR  = 1 << 1,
    LOCATION_ENTRY_IS_SEARCH     = 1 << 2
};

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

enum {
    COMPLETION_FLAG_ACTIVATE_SEARCH = 1<<0
};

enum {
    LOCATION_SELECTED,
    SEARCH_ACTIVATED,
    BOOKMARK_CLICKED,
    LAST_SIGNAL
};

enum {  
    PROP_0,
    PROP_VIEW,
    PROP_BOOKMARKS,
    PROP_ENABLE_SEARCH
};

static GHashTable *completions;

static guint location_entry_signals[LAST_SIGNAL] = {0,};

#define MAX_HISTORY 8

G_DEFINE_TYPE (YelpLocationEntry, yelp_location_entry, GTK_TYPE_COMBO_BOX)
#define GET_PRIV(object) (G_TYPE_INSTANCE_GET_PRIVATE((object), YELP_TYPE_LOCATION_ENTRY, YelpLocationEntryPrivate))

static void
yelp_location_entry_class_init (YelpLocationEntryClass *klass)
{
    GObjectClass *object_class;

    klass->location_selected = location_entry_location_selected;
    klass->search_activated = location_entry_search_activated;
    klass->bookmark_clicked = location_entry_bookmark_clicked;

    object_class = G_OBJECT_CLASS (klass);
  
    object_class->constructed = location_entry_constructed;
    object_class->dispose = location_entry_dispose;
    object_class->finalize = location_entry_finalize;
    object_class->get_property = location_entry_get_property;
    object_class->set_property = location_entry_set_property;

    /**
     * YelpLocationEntry::location-selected
     * @widget: The #YelpLocationEntry for which the signal was emitted.
     * @user_data: User data set when the handler was connected.
     *
     * This signal will be emitted whenever a user selects a normal row from the
     * drop-down menu.  Note that if a row has the flag %YELP_LOCATION_ENTRY_IS_SEARCH,
     * clicking the row will cause @widget to enter search mode, and this signal
     * will not be emitted.
     **/
    location_entry_signals[LOCATION_SELECTED] =
        g_signal_new ("location-selected",
                      G_OBJECT_CLASS_TYPE (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (YelpLocationEntryClass, location_selected),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    /**
     * YelpLocationEntry::search-activated
     * @widget: The #YelpLocationEntry for which the signal was emitted.
     * @text: The search text.
     * @user_data: User data set when the handler was connected.
     *
     * This signal will be emitted whenever the user activates a search, generally
     * by pressing <keycap>Enter</keycap> in the embedded text entry while @widget
     * is in search mode.
     **/
    location_entry_signals[SEARCH_ACTIVATED] =
        g_signal_new ("search-activated",
                      G_OBJECT_CLASS_TYPE (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (YelpLocationEntryClass, search_activated),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE, 1,
                      G_TYPE_STRING);

    /**
     * YelpLocationEntry::bookmark-clicked
     * @widget: The #YelpLocationEntry for which the signal was emitted.
     * @user_data: User data set when the handler was connected.
     *
     * This signal will be emitted whenever a user clicks the bookmark icon
     * embedded in the location entry.
     **/
    location_entry_signals[BOOKMARK_CLICKED] =
        g_signal_new ("bookmark-clicked",
                      G_OBJECT_CLASS_TYPE (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (YelpLocationEntryClass, bookmark_clicked),
                      NULL, NULL,
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
							  _("View"),
							  _("A YelpView instance to control"),
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
							  _("Bookmarks"),
							  _("A YelpBookmarks implementation instance"),
                                                          YELP_TYPE_BOOKMARKS,
                                                          G_PARAM_CONSTRUCT_ONLY |
							  G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS));

    /**
     * YelpLocationEntry:enable-search
     *
     * Whether the location entry can act as a search entry.  If search is not
     * enabled, the user will not be able to initiate a search by clicking in
     * the embedded text entry or selecting a search row in the drop-down menu.
     **/
    g_object_class_install_property (object_class,
                                     PROP_ENABLE_SEARCH,
                                     g_param_spec_boolean ("enable-search",
                                                           N_("Enable Search"),
                                                           N_("Whether the location entry can be used as a search field"),
                                                           TRUE,
                                                           G_PARAM_CONSTRUCT |
                                                           G_PARAM_READWRITE |
                                                           G_PARAM_STATIC_STRINGS));

    g_type_class_add_private ((GObjectClass *) klass,
                              sizeof (YelpLocationEntryPrivate));

    completions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

static void
yelp_location_entry_init (YelpLocationEntry *entry)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    priv->search_mode = FALSE;
    g_object_set (entry, "entry-text-column", HISTORY_COL_TITLE, NULL);
}

static void
location_entry_constructed (GObject *object)
{
    GtkCellRenderer *bookmark_cell;
    GList *cells;
    GtkTreeIter iter;
    YelpLocationEntryPrivate *priv = GET_PRIV (object);

    /* Set up the text entry child */
    priv->text_entry = gtk_bin_get_child (GTK_BIN (object));
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                       GTK_ENTRY_ICON_PRIMARY,
                                       "help-browser");
    gtk_entry_set_icon_activatable (GTK_ENTRY (priv->text_entry),
                                    GTK_ENTRY_ICON_PRIMARY,
                                    FALSE);
    gtk_entry_set_icon_activatable (GTK_ENTRY (priv->text_entry),
                                    GTK_ENTRY_ICON_SECONDARY,
                                    TRUE);
    gtk_editable_set_editable (GTK_EDITABLE (priv->text_entry),
                               priv->enable_search);
    if (!priv->enable_search) {
        priv->search_mode = FALSE;
        location_entry_set_entry ((YelpLocationEntry *) object, FALSE);
    }

    /* Set up the history model */
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
    g_signal_connect (priv->history, "row-changed",
                      G_CALLBACK (history_row_changed),
                      object);
    g_object_set (object, "model", priv->history, NULL);
    if (priv->enable_search) {
        gtk_list_store_append (priv->history, &iter);
        gtk_list_store_set (priv->history, &iter,
                            HISTORY_COL_FLAGS, LOCATION_ENTRY_IS_SEPARATOR,
                            -1);
        gtk_list_store_append (priv->history, &iter);
        gtk_list_store_set (priv->history, &iter,
                            HISTORY_COL_ICON, "edit-find-symbolic",
                            HISTORY_COL_TITLE, _("Search..."),
                            HISTORY_COL_FLAGS, LOCATION_ENTRY_IS_SEARCH,
                            -1);
    }

    /* Set up the history drop-down */
    /* Trying to get the text to line up with the text in the GtkEntry.
     * The text cell is the zeroeth cell right now, since we haven't
     * yet called reorder.  I realize using a guesstimate pixel value
     * won't be perfect all the time, but 4 looks niceish.
     */
    cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (object));
    g_object_set (cells->data, "xpad", 4, NULL);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (object),
                                        GTK_CELL_RENDERER (cells->data),
                                        (GtkCellLayoutDataFunc) cell_set_text_cell,
                                        object, NULL);
    g_object_set (cells->data, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    g_list_free (cells);

    priv->icon_cell = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (object), priv->icon_cell, FALSE);
    g_object_set (priv->icon_cell, "yalign", 0.2, NULL);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (object),
                                    priv->icon_cell,
                                    "icon-name",
                                    HISTORY_COL_ICON,
                                    NULL);
    gtk_cell_layout_reorder (GTK_CELL_LAYOUT (object), priv->icon_cell, 0);

    if (priv->bookmarks) {
        bookmark_cell = gtk_cell_renderer_pixbuf_new ();
        gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (object), bookmark_cell, FALSE);
        gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (object),
                                            bookmark_cell,
                                            (GtkCellLayoutDataFunc) cell_set_bookmark_icon,
                                            object, NULL);
    }
    gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (object),
                                          (GtkTreeViewRowSeparatorFunc)
                                          combo_box_row_separator_func,
                                          object, NULL);
    /* Without this, you get a warning about the popup widget
     * being NULL the firt time you click the arrow.
     */
    gtk_widget_show_all (GTK_WIDGET (object));

    /* Connect signals */
    g_signal_connect (object, "changed",
                      G_CALLBACK (combo_box_changed_cb), NULL);

    g_signal_connect (priv->text_entry, "focus-in-event",
                      G_CALLBACK (entry_focus_in_cb), object);
    g_signal_connect (priv->text_entry, "focus-out-event",
                      G_CALLBACK (entry_focus_out_cb), object);
    g_signal_connect (priv->text_entry, "icon-press",
                      G_CALLBACK (entry_icon_press_cb), object);
    g_signal_connect (priv->text_entry, "key-press-event",
                      G_CALLBACK (entry_key_press_cb), object);
    g_signal_connect (priv->text_entry, "activate",
                      G_CALLBACK (entry_activate_cb), object);

    if (priv->bookmarks)
        priv->bookmarks_changed = g_signal_connect (priv->bookmarks,
                                                    "bookmarks-changed",
                                                    G_CALLBACK (bookmarks_changed),
                                                    object);

    g_signal_connect (priv->view, "loaded", G_CALLBACK (view_loaded), object);
    g_signal_connect (priv->view, "notify::yelp-uri", G_CALLBACK (view_uri_selected), object);
    g_signal_connect (priv->view, "notify::page-title", G_CALLBACK (view_page_title), object);
    g_signal_connect (priv->view, "notify::page-desc", G_CALLBACK (view_page_desc), object);
    g_signal_connect (priv->view, "notify::page-icon", G_CALLBACK (view_page_icon), object);
}

static void
location_entry_dispose (GObject *object)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (object);

    if (priv->view) {
        g_object_unref (priv->view);
        priv->view = NULL;
    }

    if (priv->bookmarks_changed) {
        g_signal_handler_disconnect (priv->bookmarks, priv->bookmarks_changed);
        priv->bookmarks_changed = 0;
    }

    if (priv->bookmarks) {
        g_object_unref (priv->bookmarks);
        priv->bookmarks = NULL;
    }

    if (priv->row) {
        gtk_tree_row_reference_free (priv->row);
        priv->row = NULL;
    }

    if (priv->pulse > 0) {
        g_source_remove (priv->pulse);
        priv->pulse = 0;
    }

    G_OBJECT_CLASS (yelp_location_entry_parent_class)->dispose (object);
}

static void
location_entry_finalize (GObject *object)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (object);

    g_free (priv->completion_uri);

    G_OBJECT_CLASS (yelp_location_entry_parent_class)->finalize (object);
}

static void
location_entry_get_property   (GObject      *object,
                               guint         prop_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (object);

    switch (prop_id) {
    case PROP_VIEW:
        g_value_set_object (value, priv->view);
        break;
    case PROP_BOOKMARKS:
        g_value_set_object (value, priv->bookmarks);
        break;
    case PROP_ENABLE_SEARCH:
        g_value_set_boolean (value, priv->enable_search);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
location_entry_set_property   (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (object);

    switch (prop_id) {
    case PROP_VIEW:
        priv->view = g_value_dup_object (value);
        break;
    case PROP_BOOKMARKS:
        priv->bookmarks = g_value_dup_object (value);
        break;
    case PROP_ENABLE_SEARCH:
        priv->enable_search = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
location_entry_location_selected (YelpLocationEntry *entry)
{
    GtkTreeIter iter;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    if (priv->view_uri_selected)
        return;

    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (entry), &iter)) {
        gchar *uri;
        gtk_tree_model_get (GTK_TREE_MODEL (priv->history), &iter,
                            HISTORY_COL_URI, &uri,
                            -1);
        yelp_view_load (priv->view, uri);
        g_free (uri);
    }
}

static void
location_entry_search_activated  (YelpLocationEntry *entry)
{
    YelpUri *base, *uri;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    g_object_get (priv->view, "yelp-uri", &base, NULL);
    if (base == NULL)
        return;
    uri = yelp_uri_new_search (base,
                               gtk_entry_get_text (GTK_ENTRY (priv->text_entry)));
    g_object_unref (base);
    yelp_view_load_uri (priv->view, uri);
    gtk_widget_grab_focus (GTK_WIDGET (priv->view));
}

static void
location_entry_bookmark_clicked  (YelpLocationEntry *entry)
{
    YelpUri *uri;
    gchar *doc_uri, *page_id;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

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
location_entry_start_search (YelpLocationEntry *entry,
                             gboolean           clear)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    if (!priv->enable_search)
        return;
    if (clear && !priv->search_mode) {
        const gchar *icon = gtk_entry_get_icon_name (GTK_ENTRY (priv->text_entry),
                                                     GTK_ENTRY_ICON_PRIMARY);
        if (!g_str_equal (icon, "folder-saved-search"))
            gtk_entry_set_text (GTK_ENTRY (priv->text_entry), "");
    }
    priv->search_mode = TRUE;
    location_entry_set_entry (entry, FALSE);
    gtk_widget_grab_focus (priv->text_entry);
}

static void
location_entry_cancel_search (YelpLocationEntry *entry)
{
    gboolean ret;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);
    GdkEventFocus *event = g_new0 (GdkEventFocus, 1);
    priv->search_mode = FALSE;
    location_entry_set_entry (entry, FALSE);
    event->type = GDK_FOCUS_CHANGE;
    event->window = gtk_widget_get_window (GTK_WIDGET (entry));
    event->send_event = FALSE;
    event->in = FALSE;
    g_signal_emit_by_name (entry, "focus-out-event", event, &ret);
    g_free (event);
    /* Hack: This makes the popup disappear when you hit Esc. */
    if (priv->completion) {
        g_object_ref (priv->completion);
        gtk_entry_set_completion (GTK_ENTRY (priv->text_entry), NULL);
        gtk_entry_set_completion (GTK_ENTRY (priv->text_entry),
                                  priv->completion);
        g_object_unref (priv->completion);
    }
    gtk_editable_select_region (GTK_EDITABLE (priv->text_entry), 0, 0);
}

static void
location_entry_set_completion (YelpLocationEntry *entry,
                               GtkTreeModel *model)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);
    GList *cells;
    GtkCellRenderer *icon_cell, *bookmark_cell;

    priv->completion = gtk_entry_completion_new ();
    gtk_entry_completion_set_minimum_key_length (priv->completion, 3);
    gtk_entry_completion_set_model (priv->completion, model);
    gtk_entry_completion_set_text_column (priv->completion, COMPLETION_COL_TITLE);
    gtk_entry_completion_set_match_func (priv->completion,
                                         (GtkEntryCompletionMatchFunc) entry_match_func,
                                         entry, NULL);
    g_signal_connect (priv->completion, "match-selected",
                      G_CALLBACK (entry_match_selected), entry);

    cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (priv->completion));
    g_object_set (cells->data, "xpad", 4, NULL);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (priv->completion),
                                        GTK_CELL_RENDERER (cells->data),
                                        (GtkCellLayoutDataFunc) cell_set_completion_text_cell,
                                        entry, NULL);
    g_object_set (cells->data, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    g_list_free (cells);

    icon_cell = gtk_cell_renderer_pixbuf_new ();
    g_object_set (priv->icon_cell, "yalign", 0.2, NULL);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->completion), icon_cell, FALSE);
    gtk_cell_layout_reorder (GTK_CELL_LAYOUT (priv->completion), icon_cell, 0);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->completion),
                                    icon_cell,
                                    "icon-name",
                                    COMPLETION_COL_ICON,
                                    NULL);
    if (priv->bookmarks) {
        bookmark_cell = gtk_cell_renderer_pixbuf_new ();
        gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (priv->completion), bookmark_cell, FALSE);
        gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (priv->completion),
                                            bookmark_cell,
                                            (GtkCellLayoutDataFunc) cell_set_completion_bookmark_icon,
                                            entry, NULL);
    }
    gtk_entry_set_completion (GTK_ENTRY (priv->text_entry),
                              priv->completion);
}

static void
location_entry_set_entry (YelpLocationEntry *entry, gboolean emit)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);
    GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (entry));
    GtkTreePath *path = NULL;
    GtkTreeIter iter;
    gchar *icon_name;

    if (priv->search_mode) {
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                           GTK_ENTRY_ICON_PRIMARY,
                                           "edit-find-symbolic");
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                           GTK_ENTRY_ICON_SECONDARY,
                                           "edit-clear-symbolic");
        gtk_entry_set_icon_tooltip_text (GTK_ENTRY (priv->text_entry),
                                         GTK_ENTRY_ICON_SECONDARY,
                                         _("Clear the search text"));
        priv->icon_is_clear = TRUE;
        return;
    }
    else {
        priv->icon_is_clear = FALSE;
    }


    if (priv->row)
        path = gtk_tree_row_reference_get_path (priv->row);

    if (path) {
        gchar *text, *doc_uri, *page_id;
        gint flags;
        gtk_tree_model_get_iter (model, &iter, path);
        gtk_tree_model_get (model, &iter,
                            HISTORY_COL_TITLE, &text,
                            HISTORY_COL_ICON, &icon_name,
                            HISTORY_COL_FLAGS, &flags,
                            HISTORY_COL_DOC, &doc_uri,
                            HISTORY_COL_PAGE, &page_id,
                            -1);
        if (flags & LOCATION_ENTRY_IS_LOADING) {
            /* Would be nice to have a "loading" icon. I was using image-loading,
             * but it look out of place with symbolic page icons. Plus, using
             * image-loading-symbolic shows a broken image, because GtkEntry
             * doesn't correctly use symbolic icons as of GNOME 3.0.
             */
            gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                               GTK_ENTRY_ICON_PRIMARY,
                                               "yelp-page-symbolic");
            if (priv->pulse == 0)
                priv->pulse = g_timeout_add (500, location_entry_start_pulse, entry);
        }
        else {
            gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                               GTK_ENTRY_ICON_PRIMARY,
                                               icon_name);
        }
        if (priv->bookmarks && doc_uri && page_id) {
            GdkPixbuf *pixbuf;
            if (!yelp_bookmarks_is_bookmarked (priv->bookmarks, doc_uri, page_id)) {
                gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                                   GTK_ENTRY_ICON_SECONDARY,
                                                   "yelp-bookmark-add-symbolic");
                gtk_entry_set_icon_tooltip_text (GTK_ENTRY (priv->text_entry),
                                                 GTK_ENTRY_ICON_SECONDARY,
                                                 _("Bookmark this page"));
            }
            else {
                gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                                   GTK_ENTRY_ICON_SECONDARY,
                                                   "yelp-bookmark-remove-symbolic");
                gtk_entry_set_icon_tooltip_text (GTK_ENTRY (priv->text_entry),
                                                 GTK_ENTRY_ICON_SECONDARY,
                                                 _("Remove bookmark"));
            }
        }
        else {
            gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                               GTK_ENTRY_ICON_SECONDARY,
                                               NULL);
        }
        g_free (doc_uri);
        g_free (page_id);
        gtk_entry_set_text (GTK_ENTRY (priv->text_entry), text);
        if (emit)
            g_signal_emit (entry, location_entry_signals[LOCATION_SELECTED], 0);
        g_free (text);
        gtk_tree_path_free (path);
    }
    else {
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                           GTK_ENTRY_ICON_PRIMARY,
                                           "help-browser");
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                           GTK_ENTRY_ICON_SECONDARY,
                                           NULL);
    }
}

static gboolean
location_entry_start_pulse (gpointer data)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (data);
    priv->pulse = g_timeout_add (80, location_entry_pulse, data);
    return FALSE;
}

static gboolean
location_entry_pulse (gpointer data)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (data);
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;
    gint flags;

    model = gtk_tree_row_reference_get_model (priv->row);
    path = gtk_tree_row_reference_get_path (priv->row);
    if (path) {
        gtk_tree_model_get_iter (model, &iter, path);
        gtk_tree_model_get (model, &iter,
                            HISTORY_COL_FLAGS, &flags,
                            -1);
        gtk_tree_path_free (path);
    }

    if (flags & LOCATION_ENTRY_IS_LOADING && !priv->search_mode) {
        gtk_entry_progress_pulse (GTK_ENTRY (priv->text_entry));
    }
    else {
        gtk_entry_set_progress_fraction (GTK_ENTRY (priv->text_entry), 0.0);
        priv->pulse = 0;
    }

    return flags & LOCATION_ENTRY_IS_LOADING;
}

static void
combo_box_changed_cb (GtkComboBox  *widget,
                      gpointer      user_data)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (widget);
    GtkTreeIter iter;
    GtkTreeModel *model;

    model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));

    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter)) {
        gint flags;
        gtk_tree_model_get (model, &iter,
                            HISTORY_COL_FLAGS, &flags,
                            -1);
        if (flags & LOCATION_ENTRY_IS_SEARCH) {
            location_entry_start_search ((YelpLocationEntry *) widget, TRUE);
        }
        else {            
            GtkTreePath *cur, *path;
            path = gtk_tree_model_get_path (model, &iter);
            if (priv->row) {
                cur = gtk_tree_row_reference_get_path (priv->row);
                if (gtk_tree_path_compare (cur, path) == 0) {
                    gtk_tree_path_free (cur);
                    gtk_tree_path_free (path);
                    return;
                }
                gtk_tree_path_free (cur);
                gtk_tree_row_reference_free (priv->row);
            }
            priv->row = gtk_tree_row_reference_new (model, path);
            gtk_tree_path_free (path);
            priv->search_mode = FALSE;
            location_entry_set_entry ((YelpLocationEntry *) widget, TRUE);
        }
    }
}

static gboolean
combo_box_row_separator_func (GtkTreeModel  *model,
                              GtkTreeIter   *iter,
                              gpointer       user_data)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (user_data);
    gint flags;
    gtk_tree_model_get (model, iter,
                        HISTORY_COL_FLAGS, &flags,
                        -1);
    return (flags & LOCATION_ENTRY_IS_SEPARATOR);
}

static void
history_row_changed (GtkTreeModel  *model,
                     GtkTreePath   *path,
                     GtkTreeIter   *iter,
                     gpointer       user_data)
{
    /* FIXME: Should we bother checking path/iter against priv->row?
       It doesn't really make a difference, since set_entry is pretty much
       safe to call whenever.  Does it make a performance impact?
    */
    location_entry_set_entry (YELP_LOCATION_ENTRY (user_data), FALSE);
}

static gboolean
entry_focus_in_cb (GtkWidget     *widget,
                   GdkEventFocus *event,
                   gpointer       user_data)
{
    GtkEntry *text_entry = GTK_ENTRY (widget);
    YelpLocationEntryPrivate *priv = GET_PRIV (user_data);

    if (priv->enable_search && !priv->search_mode)
        location_entry_start_search ((YelpLocationEntry *) user_data, TRUE);

    return FALSE;
}

static gboolean
entry_focus_out_cb (GtkWidget         *widget,
                    GdkEventFocus     *event,
                    gpointer           user_data)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (user_data);

    if (gtk_entry_get_text_length (GTK_ENTRY (widget)) == 0) {
        priv->search_mode = FALSE;
        location_entry_set_entry ((YelpLocationEntry *) user_data, FALSE);
    }

    return FALSE;
}

static void
entry_activate_cb (GtkEntry  *text_entry,
                   gpointer   user_data)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (user_data);
    gchar *text = g_strdup (gtk_entry_get_text (text_entry));

    if (!priv->enable_search)
        return;

    if (!priv->search_mode || text == NULL || strlen(text) == 0)
        return;

    g_signal_emit (user_data, location_entry_signals[SEARCH_ACTIVATED], 0, text);

    g_free (text);
}

static void
entry_icon_press_cb (GtkEntry            *gtkentry,
                     GtkEntryIconPosition icon_pos,
                     GdkEvent            *event,
                     YelpLocationEntry   *entry)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    if (icon_pos == GTK_ENTRY_ICON_SECONDARY) {
        if (priv->icon_is_clear)
            location_entry_cancel_search (entry);
        else
            g_signal_emit (entry, location_entry_signals[BOOKMARK_CLICKED], 0);
    }
}

static gboolean
entry_key_press_cb (GtkWidget   *widget,
                    GdkEventKey *event,
                    YelpLocationEntry *entry)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);
    if (event->keyval == GDK_KEY_Escape) {
        location_entry_cancel_search (entry);
        return TRUE;
    }
    else if (!priv->search_mode) {
        location_entry_start_search (entry, FALSE);
    }

    return FALSE;
}

static void
cell_set_text_cell (GtkCellLayout     *layout,
                    GtkCellRenderer   *cell,
                    GtkTreeModel      *model,
                    GtkTreeIter       *iter,
                    YelpLocationEntry *entry)
{
    gchar *title, *desc, *color, *text;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    gtk_tree_model_get (model, iter,
                        HISTORY_COL_TITLE, &title,
                        HISTORY_COL_DESC, &desc,
                        -1);
    if (desc) {
        color = yelp_settings_get_color (yelp_settings_get_default (),
                                         YELP_SETTINGS_COLOR_TEXT_LIGHT);
        text = g_markup_printf_escaped ("<span size='larger'>%s</span>\n<span color='%s'>%s</span>",
                                        title, color, desc);
        g_free (color);
        g_free (desc);
    }
    else {
        text = g_markup_printf_escaped ("<span size='larger'>%s</span>", title);
    }

    g_object_set (cell, "markup", text, NULL);
    g_free (text);
    g_free (title);
}

static void
cell_set_bookmark_icon (GtkCellLayout     *layout,
                        GtkCellRenderer   *cell,
                        GtkTreeModel      *model,
                        GtkTreeIter       *iter,
                        YelpLocationEntry *entry)
{
    gint flags;
    gchar *doc_uri, *page_id;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    if (priv->bookmarks == NULL) {
        g_object_set (cell, "icon-name", NULL, NULL);
        return;
    }

    gtk_tree_model_get (model, iter, HISTORY_COL_FLAGS, &flags, -1);
    if (flags & (LOCATION_ENTRY_IS_SEPARATOR | LOCATION_ENTRY_IS_SEARCH)) {
        g_object_set (cell, "icon-name", NULL, NULL);
        return;
    }
    
    gtk_tree_model_get (model, iter,
                        HISTORY_COL_DOC, &doc_uri,
                        HISTORY_COL_PAGE, &page_id,
                        -1);
    if (doc_uri && page_id &&
        yelp_bookmarks_is_bookmarked (priv->bookmarks, doc_uri, page_id))
        g_object_set (cell, "icon-name", "yelp-bookmark-remove-symbolic", NULL);
    else
        g_object_set (cell, "icon-name", NULL, NULL);

    g_free (doc_uri);
    g_free (page_id);
}

static void
cell_set_completion_bookmark_icon (GtkCellLayout     *layout,
                                   GtkCellRenderer   *cell,
                                   GtkTreeModel      *model,
                                   GtkTreeIter       *iter,
                                   YelpLocationEntry *entry)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    if (priv->completion_uri) {
        gchar *page_id = NULL;
        gtk_tree_model_get (model, iter,
                            COMPLETION_COL_PAGE, &page_id,
                            -1);

        if (page_id && yelp_bookmarks_is_bookmarked (priv->bookmarks,
                                                     priv->completion_uri, page_id))
            g_object_set (cell, "icon-name", "yelp-bookmark-remove-symbolic", NULL);
        else
            g_object_set (cell, "icon-name", NULL, NULL);

        g_free (page_id);
    }
}

static void
cell_set_completion_text_cell (GtkCellLayout     *layout,
                               GtkCellRenderer   *cell,
                               GtkTreeModel      *model,
                               GtkTreeIter       *iter,
                               YelpLocationEntry *entry)
{
    gchar *title, *desc, *color, *text;
    gint flags;
    GList *cells, *cur;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    gtk_tree_model_get (model, iter, COMPLETION_COL_FLAGS, &flags, -1);
    if (flags & COMPLETION_FLAG_ACTIVATE_SEARCH) {
        title = g_strdup_printf (_("Search for “%s”"),
                                 gtk_entry_get_text (GTK_ENTRY (priv->text_entry)));
        text = g_markup_printf_escaped ("<span size='larger' font_weight='bold'>%s</span>", title);
        g_object_set (cell, "markup", text, NULL);
        g_free (text);
        g_free (title);

        color = yelp_settings_get_color (yelp_settings_get_default (),
                                         YELP_SETTINGS_COLOR_YELLOW_BASE);
        cells = gtk_cell_layout_get_cells (layout);
        for (cur = cells; cur; cur = cur->next) {
            g_object_set (cur->data,
                          "cell-background", color,
                          "cell-background-set", TRUE,
                          NULL);
        }
        g_list_free (cells);
        g_free (color);
        return;
    }

    gtk_tree_model_get (model, iter,
                        COMPLETION_COL_TITLE, &title,
                        COMPLETION_COL_DESC, &desc,
                        -1);
    if (desc) {
        color = yelp_settings_get_color (yelp_settings_get_default (),
                                         YELP_SETTINGS_COLOR_TEXT_LIGHT);
        text = g_markup_printf_escaped ("<span size='larger'>%s</span>\n<span color='%s'>%s</span>",
                                        title, color, desc);
        g_free (color);
        g_free (desc);
    }
    else {
        text = g_markup_printf_escaped ("<span size='larger'>%s</span>", title);
    }

    g_object_set (cell, "markup", text, NULL);
    g_free (text);
    g_free (title);

    cells = gtk_cell_layout_get_cells (layout);
    for (cur = cells; cur; cur = cur->next)
        g_object_set (cur->data, "cell-background-set", FALSE, NULL);
    g_list_free (cells);
}

static gboolean
entry_match_func (GtkEntryCompletion *completion,
                  const gchar        *key,
                  GtkTreeIter        *iter,
                  YelpLocationEntry  *entry)
{
    gint stri;
    gchar *title, *desc, *titlecase = NULL, *desccase = NULL;
    gboolean ret = FALSE;
    gchar **strs;
    gint flags;
    GtkTreeModel *model = gtk_entry_completion_get_model (completion);
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);
    static GRegex *nonword = NULL;

    if (nonword == NULL)
        nonword = g_regex_new ("\\W", 0, 0, NULL);
    if (nonword == NULL)
        return FALSE;

    gtk_tree_model_get (model, iter, COMPLETION_COL_FLAGS, &flags, -1);
    if (flags & COMPLETION_FLAG_ACTIVATE_SEARCH)
        return TRUE;

    gtk_tree_model_get (model, iter,
                        COMPLETION_COL_TITLE, &title,
                        COMPLETION_COL_DESC, &desc,
                        -1);
    if (title) {
        titlecase = g_utf8_casefold (title, -1);
        g_free (title);
    }
    if (desc) {
        desccase = g_utf8_casefold (desc, -1);
        g_free (desc);
    }

    strs = g_regex_split (nonword, key, 0);
    ret = TRUE;
    for (stri = 0; strs[stri]; stri++) {
        if (!titlecase || !strstr (titlecase, strs[stri])) {
            if (!desccase || !strstr (desccase, strs[stri])) {
                ret = FALSE;
                break;
            }
        }
    }

    g_free (titlecase);
    g_free (desccase);
    g_strfreev (strs);

    return ret;
}

static gint
entry_completion_sort (GtkTreeModel *model,
                       GtkTreeIter  *iter1,
                       GtkTreeIter  *iter2,
                       gpointer      user_data)
{
    gint ret = 0;
    gint flags1, flags2;
    gchar *key1, *key2;

    gtk_tree_model_get (model, iter1, COMPLETION_COL_FLAGS, &flags1, -1);
    gtk_tree_model_get (model, iter2, COMPLETION_COL_FLAGS, &flags2, -1);
    if (flags1 & COMPLETION_FLAG_ACTIVATE_SEARCH)
        return 1;
    else if (flags2 & COMPLETION_FLAG_ACTIVATE_SEARCH)
        return -1;

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

static gboolean
entry_match_selected (GtkEntryCompletion *completion,
                      GtkTreeModel       *model,
                      GtkTreeIter        *iter,
                      YelpLocationEntry  *entry)
{
    YelpUri *base, *uri;
    gchar *page, *xref;
    gint flags;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    gtk_tree_model_get (model, iter, COMPLETION_COL_FLAGS, &flags, -1);
    if (flags & COMPLETION_FLAG_ACTIVATE_SEARCH) {
        entry_activate_cb (GTK_ENTRY (priv->text_entry), entry);
        return TRUE;
    }

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
    return TRUE;
}

static void
view_loaded (YelpView          *view,
             YelpLocationEntry *entry)
{
    gchar **ids;
    gint i;
    GtkTreeIter iter;
    gint flags;
    YelpUri *uri;
    gchar *doc_uri, *page_id;
    GtkTreeModel *completion;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);
    YelpDocument *document = yelp_view_get_document (view);

    gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &iter);
    gtk_tree_model_get (GTK_TREE_MODEL (priv->history), &iter,
                        HISTORY_COL_FLAGS, &flags,
                        -1);
    if (flags & LOCATION_ENTRY_IS_LOADING) {
        flags = flags & ~LOCATION_ENTRY_IS_LOADING;
        gtk_list_store_set (priv->history, &iter, HISTORY_COL_FLAGS, flags, -1);
    }

    g_object_get (view,
                  "yelp-uri", &uri,
                  "page-id", &page_id,
                  NULL);
    doc_uri = yelp_uri_get_document_uri (uri);
    gtk_list_store_set (priv->history, &iter,
                        HISTORY_COL_DOC, doc_uri,
                        HISTORY_COL_PAGE, page_id,
                        -1);
    g_free (page_id);

    if ((priv->completion_uri == NULL) || 
        !g_str_equal (doc_uri, priv->completion_uri)) {
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
            g_hash_table_insert (completions, g_strdup (doc_uri), completion);
            if (document != NULL) {
                GtkTreeIter iter;
                ids = yelp_document_list_page_ids (document);
                for (i = 0; ids[i]; i++) {
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
                g_strfreev (ids);
                gtk_list_store_insert (GTK_LIST_STORE (base), &iter, 0);
                gtk_list_store_set (base, &iter,
                                    COMPLETION_COL_ICON, "edit-find-symbolic",
                                    COMPLETION_COL_FLAGS, COMPLETION_FLAG_ACTIVATE_SEARCH,
                                    -1);
            }
            g_object_unref (base);
        }
        g_free (priv->completion_uri);
        priv->completion_uri = doc_uri;
        location_entry_set_completion (entry, completion);
    }

    g_object_unref (uri);
}

static void
view_uri_selected (YelpView          *view,
                   GParamSpec        *pspec,
                   YelpLocationEntry *entry)
{
    GtkTreeIter iter;
    gchar *iter_uri;
    gboolean cont;
    YelpUri *uri;
    gchar *struri, *doc_uri;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    g_object_get (G_OBJECT (view), "yelp-uri", &uri, NULL);
    if (uri == NULL)
        return;

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
                            HISTORY_COL_FLAGS, LOCATION_ENTRY_IS_LOADING,
                            -1);
        /* Limit to MAX_HISTORY entries. There are two extra for the search entry
         * and the separator above it.
         */
        num = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->history), NULL);
        if (num > MAX_HISTORY + 2) {
            if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (priv->history),
                                                               &last, NULL,
                                                               num - 3)) {
                gtk_list_store_remove (priv->history, &last);
            }
        }
    }
    priv->view_uri_selected = TRUE;
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (entry), &iter);
    priv->view_uri_selected = FALSE;

    g_free (struri);
    g_object_unref (uri);
}

static void
view_page_title (YelpView          *view,
                 GParamSpec        *pspec,
                 YelpLocationEntry *entry)
{
    GtkTreeIter first;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &first)) {
        gchar *title, *frag = NULL;
        YelpViewState state;
        YelpUri *uri;

        g_object_get (view,
                      "page-title", &title,
                      "state", &state,
                      NULL);
        if (title == NULL)
            return;

        if (state != YELP_VIEW_STATE_ERROR) {
            YelpUri *uri;
            g_object_get (view, "yelp-uri", &uri, NULL);
            frag = yelp_uri_get_frag_id (uri);
            g_object_unref (uri);
        }

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
    }
}

static void
view_page_desc (YelpView          *view,
                GParamSpec        *pspec,
                YelpLocationEntry *entry)
{
    GtkTreeIter first;
    gchar *desc;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    g_object_get (view, "page-desc", &desc, NULL);
    if (desc == NULL)
        return;

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &first))
        gtk_list_store_set (priv->history, &first,
                            HISTORY_COL_DESC, desc,
                            -1);
    g_free (desc);
}

static void
view_page_icon (YelpView          *view,
                GParamSpec        *pspec,
                YelpLocationEntry *entry)
{
    GtkTreeIter first;
    gchar *icon;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    g_object_get (view, "page-icon", &icon, NULL);
    if (icon == NULL)
        return;

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &first))
        gtk_list_store_set (priv->history, &first,
                            HISTORY_COL_ICON, icon,
                            -1);
    g_free (icon);
}

static void
bookmarks_changed (YelpBookmarks      *bookmarks,
                   const gchar        *doc_uri,
                   YelpLocationEntry  *entry)
{
    GtkTreePath *path;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    if (priv->row)
        path = gtk_tree_row_reference_get_path (priv->row);

    if (path) {
        GtkTreeIter iter;
        gchar *this_uri, *page_id;
        gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->history), &iter, path);
        gtk_tree_model_get (GTK_TREE_MODEL (priv->history), &iter,
                            HISTORY_COL_DOC, &this_uri,
                            HISTORY_COL_PAGE, &page_id,
                            -1);
        if (this_uri && g_str_equal (this_uri, doc_uri) && page_id) {
            if (!yelp_bookmarks_is_bookmarked (priv->bookmarks, doc_uri, page_id)) {
                gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                                   GTK_ENTRY_ICON_SECONDARY,
                                                   "yelp-bookmark-add-symbolic");
                gtk_entry_set_icon_tooltip_text (GTK_ENTRY (priv->text_entry),
                                                 GTK_ENTRY_ICON_SECONDARY,
                                                 _("Bookmark this page"));
            }
            else {
                gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                                   GTK_ENTRY_ICON_SECONDARY,
                                                   "yelp-bookmark-remove-symbolic");
                gtk_entry_set_icon_tooltip_text (GTK_ENTRY (priv->text_entry),
                                                 GTK_ENTRY_ICON_SECONDARY,
                                                 _("Remove bookmark"));
            }
        }
        else {
            gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                               GTK_ENTRY_ICON_SECONDARY,
                                               NULL);
        }
        g_free (this_uri);
        g_free (page_id);
    }
}

/**
 * yelp_location_entry_new:
 * @view: A #YelpView.
 *
 * Creates a new #YelpLocationEntry widget to control @view.
 *
 * Returns: A new #YelpLocationEntry.
 **/
GtkWidget*
yelp_location_entry_new (YelpView      *view,
                         YelpBookmarks *bookmarks)
{
    GtkWidget *ret;
    g_return_val_if_fail (YELP_IS_VIEW (view), NULL);

    ret = GTK_WIDGET (g_object_new (YELP_TYPE_LOCATION_ENTRY,
                                    "has-entry", TRUE,
                                    "view", view,
                                    "bookmarks", bookmarks,
                                    NULL));

    return ret;
}

/**
 * yelp_location_entry_start_search:
 * @entry: A #YelpLocationEntry.
 *
 * Puts @entry into search mode.  This focuses the entry and clears its text
 * contents.  When the user activates the search, the
 * #YelpLocationEntry::search-activated signal will be emitted.
 **/
void
yelp_location_entry_start_search (YelpLocationEntry *entry)
{
    location_entry_start_search (entry, TRUE);
}
