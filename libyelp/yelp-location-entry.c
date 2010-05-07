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
 * #YelpLocationEntry is a #GtkComboBoxEntry designed to show the current location,
 * provide a drop-down menu of previous locations, and allow the user to perform
 * searches.
 *
 * The #GtkTreeModel used by a #YelpLocationEntry is expected to have at least
 * four columns: #GtkComboBoxEntry::text-column contains the displayed name
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

static void     location_entry_dispose         (GObject            *object);
static void     location_entry_finalize        (GObject            *object);
static void     location_entry_get_property    (GObject           *object,
                                                guint              prop_id,
                                                GValue            *value,
                                                GParamSpec        *pspec);
static void     location_entry_set_property    (GObject           *object,
                                                guint              prop_id,
                                                const GValue      *value,
                                                GParamSpec        *pspec);
static void     location_entry_start_search    (YelpLocationEntry *entry,
                                                gboolean           clear);
static void     location_entry_cancel_search   (YelpLocationEntry *entry);
static void     location_entry_set_entry       (YelpLocationEntry *entry,
                                                gboolean           emit);
static gboolean location_entry_pulse           (gpointer           data);

/* GtkTreeModel callbacks */
static void     yelp_location_entry_row_changed     (GtkTreeModel      *model,
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
static gboolean entry_match_selected                (GtkEntryCompletion *completion,
                                                     GtkTreeModel       *model,
                                                     GtkTreeIter        *iter,
                                                     YelpLocationEntry  *entry);


typedef struct _YelpLocationEntryPrivate  YelpLocationEntryPrivate;
struct _YelpLocationEntryPrivate
{
    GtkWidget          *text_entry;

    gint       desc_column;
    gint       icon_column;
    gint       flags_column;
    gboolean   enable_search;

    gboolean   search_mode;
    guint      pulse;

    GtkCellRenderer *icon_cell;

    GtkEntryCompletion *completion;
    gint                completion_desc_column;
    gint                completion_flags_column;

    /* free this, and only this */
    GtkTreeRowReference *row;
};

enum {
  LOCATION_SELECTED,
  COMPLETION_SELECTED,
  SEARCH_ACTIVATED,
  BOOKMARK_CLICKED,
  LAST_SIGNAL
};

enum {  
  PROP_0,
  PROP_DESC_COLUMN,
  PROP_ICON_COLUMN,
  PROP_FLAGS_COLUMN,
  PROP_ENABLE_SEARCH
};

static guint location_entry_signals[LAST_SIGNAL] = {0,};

G_DEFINE_TYPE (YelpLocationEntry, yelp_location_entry, GTK_TYPE_COMBO_BOX_ENTRY)
#define GET_PRIV(object) (G_TYPE_INSTANCE_GET_PRIVATE((object), YELP_TYPE_LOCATION_ENTRY, YelpLocationEntryPrivate))

static void
yelp_location_entry_class_init (YelpLocationEntryClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
  
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
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    /**
     * YelpLocationEntry::completion-selected
     * @widget: The #YelpLocationEntry for which the signal was emitted.
     * @model: The #GtkTreeModel contianing the completion.
     * @iter: A #GtkTreeIter positioned at the completion.
     * @user_data: User data set when the handler was connected.
     *
     * This signal will be emitted whenever a user selects an entry in the
     * completion drop-down while typing.
     **/
    location_entry_signals[COMPLETION_SELECTED] =
        g_signal_new ("completion-selected",
                      G_OBJECT_CLASS_TYPE (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      yelp_marshal_VOID__OBJECT_BOXED,
                      G_TYPE_NONE, 2, GTK_TYPE_TREE_MODEL, GTK_TYPE_TREE_ITER);

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
                      0, NULL, NULL,
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
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    /**
     * YelpLocationEntry:desc-column
     *
     * The column in the #GtkTreeModel containing a description for each row.
     **/
    g_object_class_install_property (object_class,
                                     PROP_DESC_COLUMN,
                                     g_param_spec_int ("desc-column",
                                                       N_("Description Column"),
                                                       N_("A column in the model to get descriptions from"),
                                                       -1,
                                                       G_MAXINT,
                                                       -1,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
    /**
     * YelpLocationEntry:icon-column
     *
     * The column in the #GtkTreeModel containing an icon name for each row.
     * This must be a name that can be looked up through #GtkIconTheme.
     **/
    g_object_class_install_property (object_class,
                                     PROP_ICON_COLUMN,
                                     g_param_spec_int ("icon-column",
                                                       N_("Icon Column"),
                                                       N_("A column in the model to get icon names from"),
                                                       -1,
                                                       G_MAXINT,
                                                       -1,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
    /**
     * YelpLocationEntry:flags-column
     *
     * The column in the #GtkTreeModel containing #YelpLocationEntryFlags flags
     * for each row.
     **/
    g_object_class_install_property (object_class,
                                     PROP_FLAGS_COLUMN,
                                     g_param_spec_int ("flags-column",
                                                       N_("Flags Column"),
                                                       N_("A column in the model with YelpLocationEntryFlags flags"),
                                                       -1,
                                                       G_MAXINT,
                                                       -1,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
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
                                                           G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                           G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_type_class_add_private ((GObjectClass *) klass,
                              sizeof (YelpLocationEntryPrivate));
}

static void
yelp_location_entry_init (YelpLocationEntry *entry)
{
    GtkCellRenderer *bookmark_cell;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);
    GList *cells;

    priv->enable_search = TRUE;
    priv->search_mode = FALSE;

    priv->text_entry = gtk_bin_get_child (GTK_BIN (entry));
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                       GTK_ENTRY_ICON_PRIMARY,
                                       "help-browser");
    gtk_entry_set_icon_activatable (GTK_ENTRY (priv->text_entry),
                                    GTK_ENTRY_ICON_PRIMARY,
                                    FALSE);
    gtk_entry_set_icon_activatable (GTK_ENTRY (priv->text_entry),
                                    GTK_ENTRY_ICON_SECONDARY,
                                    TRUE);

    /* Trying to get the text to line up with the text in the GtkEntry.
     * The text cell is the zeroeth cell right now, since we haven't
     * yet called reorder.  I realize using a guesstimate pixel value
     * won't be perfect all the time, but 4 looks niceish.
     */
    cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (entry));
    g_object_set (cells->data, "xpad", 4, NULL);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (entry),
                                        GTK_CELL_RENDERER (cells->data),
                                        (GtkCellLayoutDataFunc) cell_set_text_cell,
                                        entry, NULL);
    g_object_set (cells->data, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    g_list_free (cells);

    priv->icon_cell = gtk_cell_renderer_pixbuf_new ();
    g_object_set (priv->icon_cell, "yalign", 0.2, NULL);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (entry), priv->icon_cell, FALSE);

    gtk_cell_layout_reorder (GTK_CELL_LAYOUT (entry), priv->icon_cell, 0);

    bookmark_cell = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (entry), bookmark_cell, FALSE);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (entry),
                                        bookmark_cell,
                                        (GtkCellLayoutDataFunc) cell_set_bookmark_icon,
                                        entry, NULL);

    g_signal_connect (entry, "changed",
                      G_CALLBACK (combo_box_changed_cb), NULL);

    g_signal_connect (priv->text_entry, "focus-in-event",
                      G_CALLBACK (entry_focus_in_cb), entry);
    g_signal_connect (priv->text_entry, "focus-out-event",
                      G_CALLBACK (entry_focus_out_cb), entry);
    g_signal_connect (priv->text_entry, "icon-press",
                      G_CALLBACK (entry_icon_press_cb), entry);
    g_signal_connect (priv->text_entry, "key-press-event",
                      G_CALLBACK (entry_key_press_cb), entry);
    g_signal_connect (priv->text_entry, "activate",
                      G_CALLBACK (entry_activate_cb), entry);
}

static void
location_entry_dispose (GObject *object)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (object);

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
    case PROP_DESC_COLUMN:
        g_value_set_int (value, priv->desc_column);
        break;
    case PROP_ICON_COLUMN:
        g_value_set_int (value, priv->icon_column);
        break;
    case PROP_FLAGS_COLUMN:
        g_value_set_int (value, priv->flags_column);
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
    case PROP_DESC_COLUMN:
        priv->desc_column = g_value_get_int (value);
        break;
    case PROP_ICON_COLUMN:
        priv->icon_column = g_value_get_int (value);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (object),
                                        priv->icon_cell,
                                        "icon-name",
                                        priv->icon_column,
                                        NULL);
        break;
    case PROP_FLAGS_COLUMN:
        priv->flags_column = g_value_get_int (value);
        /* If this is in yelp_location_entry_init, it gets called the first
         * time before flags_column is set, and isn't called again until the
         * "row-changed" signal on the model.  The prevents application code
         * from populating the model before initializing the widget.
         */
        gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (object),
                                              (GtkTreeViewRowSeparatorFunc)
                                              combo_box_row_separator_func,
                                              object, NULL);
        break;
    case PROP_ENABLE_SEARCH:
        priv->enable_search = g_value_get_boolean (value);
        gtk_editable_set_editable (GTK_EDITABLE (priv->text_entry),
                                   priv->enable_search);
        if (!priv->enable_search) {
            priv->search_mode = FALSE;
            location_entry_set_entry ((YelpLocationEntry *) object, FALSE);
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
location_entry_start_search (YelpLocationEntry *entry,
                             gboolean           clear)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    if (!priv->enable_search)
        return;
    if (clear && !priv->search_mode) {
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
    g_object_ref (priv->completion);
    gtk_entry_set_completion (GTK_ENTRY (priv->text_entry), NULL);
    gtk_entry_set_completion (GTK_ENTRY (priv->text_entry),
                              priv->completion);
    g_object_unref (priv->completion);
}

void
yelp_location_entry_set_completion_model (YelpLocationEntry *entry,
                                          GtkTreeModel *model,
                                          gint text_column,
                                          gint desc_column,
                                          gint icon_column,
                                          gint flags_column)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);
    GList *cells;
    GtkCellRenderer *icon_cell, *bookmark_cell;

    priv->completion = gtk_entry_completion_new ();
    priv->completion_desc_column = desc_column;
    priv->completion_flags_column = flags_column;
    gtk_entry_completion_set_minimum_key_length (priv->completion, 3);
    gtk_entry_completion_set_model (priv->completion, model);
    gtk_entry_completion_set_text_column (priv->completion, text_column);
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
    /* We use multi-line text, and GTK+ gets heights wrong without this. */
    gtk_cell_renderer_text_set_fixed_height_from_font (GTK_CELL_RENDERER_TEXT (cells->data), 2);
    g_list_free (cells);

    icon_cell = gtk_cell_renderer_pixbuf_new ();
    g_object_set (priv->icon_cell, "yalign", 0.2, NULL);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->completion), icon_cell, FALSE);
    gtk_cell_layout_reorder (GTK_CELL_LAYOUT (priv->completion), icon_cell, 0);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->completion),
                                    icon_cell,
                                    "icon-name",
                                    icon_column,
                                    NULL);

    bookmark_cell = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (priv->completion), bookmark_cell, FALSE);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (priv->completion),
                                        bookmark_cell,
                                        (GtkCellLayoutDataFunc) cell_set_completion_bookmark_icon,
                                        entry, NULL);

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
                                           "system-search");
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                           GTK_ENTRY_ICON_SECONDARY,
                                           "edit-clear");
        gtk_entry_set_icon_tooltip_text (GTK_ENTRY (priv->text_entry),
                                         GTK_ENTRY_ICON_SECONDARY,
                                         "Clear the search text");
        return;
    }

    if (priv->row)
        path = gtk_tree_row_reference_get_path (priv->row);

    if (path) {
        gchar *text;
        gint flags;
        gtk_tree_model_get_iter (model, &iter, path);
        gtk_tree_model_get (model, &iter,
                            gtk_combo_box_entry_get_text_column (GTK_COMBO_BOX_ENTRY (entry)),
                            &text,
                            priv->icon_column, &icon_name,
                            priv->flags_column, &flags,
                            -1);
        if (flags & YELP_LOCATION_ENTRY_IS_LOADING) {
            gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                               GTK_ENTRY_ICON_PRIMARY,
                                               "image-loading");
            if (priv->pulse > 0)
                g_source_remove (priv->pulse);
            priv->pulse = g_timeout_add (80, location_entry_pulse, entry);
        }
        else {
            gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                               GTK_ENTRY_ICON_PRIMARY,
                                               icon_name);
        }
        if (flags & YELP_LOCATION_ENTRY_CAN_BOOKMARK) {
            gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                               GTK_ENTRY_ICON_SECONDARY,
                                               "bookmark-new");
            gtk_entry_set_icon_tooltip_text (GTK_ENTRY (priv->text_entry),
                                             GTK_ENTRY_ICON_SECONDARY,
                                             "Bookmark this page");
        }
        else {
            gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->text_entry),
                                               GTK_ENTRY_ICON_SECONDARY,
                                               NULL);
        }
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
                            priv->flags_column, &flags,
                            -1);
        gtk_tree_path_free (path);
    }

    if (flags & YELP_LOCATION_ENTRY_IS_LOADING && !priv->search_mode) {
        gtk_entry_progress_pulse (GTK_ENTRY (priv->text_entry));
    }
    else {
        gtk_entry_set_progress_fraction (GTK_ENTRY (priv->text_entry), 0.0);
    }

    return flags & YELP_LOCATION_ENTRY_IS_LOADING;
}

static void
combo_box_changed_cb (GtkComboBox  *widget,
                      gpointer      user_data)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (widget);
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreePath *path;

    model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));

    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter)) {
        gint flags;
        gtk_tree_model_get (model, &iter,
                            priv->flags_column, &flags,
                            -1);
        if (flags & YELP_LOCATION_ENTRY_IS_SEARCH) {
            location_entry_start_search ((YelpLocationEntry *) widget, TRUE);
        }
        else {
            path = gtk_tree_model_get_path (model, &iter);
            if (priv->row)
                gtk_tree_row_reference_free (priv->row);
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
                        priv->flags_column, &flags,
                        -1);
    return (flags & YELP_LOCATION_ENTRY_IS_SEPARATOR);
}

static void
yelp_location_entry_row_changed (GtkTreeModel  *model,
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
        const gchar *name = gtk_entry_get_icon_name (gtkentry, icon_pos);
        if (g_str_equal (name, "edit-clear")) {
            location_entry_cancel_search (entry);
        }
        else if  (g_str_equal (name, "bookmark-new")) {
            g_signal_emit (entry, location_entry_signals[BOOKMARK_CLICKED], 0);
        }
    }
}

static gboolean
entry_key_press_cb (GtkWidget   *widget,
                    GdkEventKey *event,
                    YelpLocationEntry *entry)
{
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);
    if (event->keyval == GDK_Escape) {
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
    gint text_col;
    gchar *title, *desc, *color, *text;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    g_object_get (entry, "text-column", &text_col, NULL);
    if (text_col >= 0) {
        gtk_tree_model_get (model, iter,
                            text_col, &title,
                            priv->desc_column, &desc,
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
}

static void
cell_set_bookmark_icon (GtkCellLayout     *layout,
                        GtkCellRenderer   *cell,
                        GtkTreeModel      *model,
                        GtkTreeIter       *iter,
                        YelpLocationEntry *entry)
{
    gint flags;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    gtk_tree_model_get (model, iter,
                        priv->flags_column, &flags,
                        -1);
    if (!(flags & YELP_LOCATION_ENTRY_IS_SEPARATOR) &&
        !(flags & YELP_LOCATION_ENTRY_IS_SEARCH) &&
        (flags & YELP_LOCATION_ENTRY_IS_BOOKMARKED))
        g_object_set (cell, "icon-name", "bookmark", NULL);
    else
        g_object_set (cell, "icon-name", NULL, NULL);
}

static void
cell_set_completion_bookmark_icon (GtkCellLayout     *layout,
                                   GtkCellRenderer   *cell,
                                   GtkTreeModel      *model,
                                   GtkTreeIter       *iter,
                                   YelpLocationEntry *entry)
{
    gint flags;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    gtk_tree_model_get (model, iter,
                        priv->completion_flags_column, &flags,
                        -1);
    if (flags & YELP_LOCATION_ENTRY_IS_BOOKMARKED)
        g_object_set (cell, "icon-name", "bookmark", NULL);
    else
        g_object_set (cell, "icon-name", NULL, NULL);
}

static void
cell_set_completion_text_cell (GtkCellLayout     *layout,
                               GtkCellRenderer   *cell,
                               GtkTreeModel      *model,
                               GtkTreeIter       *iter,
                               YelpLocationEntry *entry)
{
    gint text_col;
    gchar *title, *desc, *color, *text;
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    g_object_get (priv->completion, "text-column", &text_col, NULL);
    if (text_col >= 0) {
        gtk_tree_model_get (model, iter,
                            text_col, &title,
                            priv->completion_desc_column, &desc,
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
}

static gboolean
entry_match_func (GtkEntryCompletion *completion,
                  const gchar        *key,
                  GtkTreeIter        *iter,
                  YelpLocationEntry  *entry)
{
    gint text_col, stri;
    gchar *title, *desc, *titlecase = NULL, *desccase = NULL;
    gboolean ret = FALSE;
    gchar **strs;
    GtkTreeModel *model = gtk_entry_completion_get_model (completion);
    YelpLocationEntryPrivate *priv = GET_PRIV (entry);

    g_object_get (entry, "text-column", &text_col, NULL);
    gtk_tree_model_get (model, iter,
                        text_col, &title,
                        priv->desc_column, &desc,
                        -1);
    if (title) {
        titlecase = g_utf8_casefold (title, -1);
        g_free (title);
    }
    if (desc) {
        desccase = g_utf8_casefold (desc, -1);
        g_free (desc);
    }

    strs = g_strsplit (key, " ", -1);
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

static gboolean
entry_match_selected (GtkEntryCompletion *completion,
                      GtkTreeModel       *model,
                      GtkTreeIter        *iter,
                      YelpLocationEntry  *entry)
{
    g_signal_emit (entry, location_entry_signals[COMPLETION_SELECTED], 0, model, iter);
    return TRUE;
}

/**
 * yelp_location_entry_new_with_model:
 * @model: A #GtkTreeModel.
 * @text_column: The column in @model containing the title of each entry.
 * @desc_column: The column in @model containing the description of each entry.
 * @icon_column: The column in @model containing the icon name of each entry.
 * @flags_column: The column in @model containing #YelpLocationEntryFlags.
 *
 * Creates a new #YelpLocationEntry widget.
 *
 * Returns: A new #YelpLocationEntry.
 **/
GtkWidget*
yelp_location_entry_new_with_model (GtkTreeModel *model,
                                    gint          text_column,
                                    gint          desc_column,
                                    gint          icon_column,
                                    gint          flags_column)
{
    GtkWidget *ret;
    g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);

    ret = GTK_WIDGET (g_object_new (YELP_TYPE_LOCATION_ENTRY,
                                    "model", model,
                                    "text-column", text_column,
                                    "desc-column", desc_column,
                                    "icon-column", icon_column,
                                    "flags-column", flags_column,
                                    NULL));

    /* FIXME: This isn't a good place for this.  Probably should
     * put it in a notify handler for the model property.
     */
    g_signal_connect (model, "row-changed",
                      G_CALLBACK (yelp_location_entry_row_changed),
                      YELP_LOCATION_ENTRY (ret));
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
