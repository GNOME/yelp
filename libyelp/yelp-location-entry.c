/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

struct _YelpLocationEntryPrivate
{
  GtkWidget *text_entry;

  gint       icon_column;
  gint       flags_column;
  gboolean   enable_search;

  GtkTreeRowReference *row;

  gboolean   search_mode;

  GtkCellRenderer *icon_cell;
};

#define YELP_LOCATION_ENTRY_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE((object), YELP_TYPE_LOCATION_ENTRY, YelpLocationEntryPrivate))

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
static void     entry_icon_press_cb                 (GtkEntry          *entry,
                                                     GtkEntryIconPosition icon_pos,
                                                     GdkEvent          *event,
                                                     gpointer           user_data);
static gboolean entry_key_press_cb                  (GtkWidget         *widget,
                                                     GdkEventKey       *event,
                                                     gpointer           user_data);


static GtkComboBoxEntryClass *parent_class = NULL;

enum {
  LOCATION_SELECTED,
  SEARCH_ACTIVATED,
  LAST_SIGNAL
};

enum {  
  PROP_0,
  PROP_ICON_COLUMN,
  PROP_FLAGS_COLUMN,
  PROP_ENABLE_SEARCH
};

static guint location_entry_signals[LAST_SIGNAL] = {0,};

G_DEFINE_TYPE (YelpLocationEntry, yelp_location_entry, GTK_TYPE_COMBO_BOX_ENTRY)

static void
yelp_location_entry_class_init (YelpLocationEntryClass *klass)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkComboBoxEntryClass *entry_class;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);
  entry_class = GTK_COMBO_BOX_ENTRY_CLASS (klass);

  gobject_class->get_property = location_entry_get_property;
  gobject_class->set_property = location_entry_set_property;

  /**
   * YelpLocationEntry::location-selected
   * @widget: The object which received the signal
   */
  location_entry_signals[LOCATION_SELECTED] =
    g_signal_new ("location_selected",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * YelpLocationEntry::search_activated
   * @widget: The object which received the signal
   * @text: The search text
   */
  location_entry_signals[SEARCH_ACTIVATED] =
    g_signal_new ("search-activated",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);

  g_object_class_install_property (gobject_class,
                                   PROP_ICON_COLUMN,
                                   g_param_spec_int ("icon-column",
                                                     N_("Icon Column"),
                                                     N_("A column in the model to get icon names from"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_FLAGS_COLUMN,
                                   g_param_spec_int ("flags-column",
                                                     N_("Flags Column"),
                                                     N_("A column in the model with YelpLocationEntryFlags flags"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_ENABLE_SEARCH,
                                   g_param_spec_boolean ("enable-search",
                                                         N_("Enable Search"),
                                                         N_("Whether the location entry can be used as a search field"),
                                                         TRUE,
                                                         G_PARAM_READWRITE));

  g_type_class_add_private ((GObjectClass *) klass,
                            sizeof (YelpLocationEntryPrivate));
}

static void yelp_location_entry_init (YelpLocationEntry *entry)
{
  GList *cells;
  entry->priv = YELP_LOCATION_ENTRY_GET_PRIVATE (entry);

  entry->priv->enable_search = TRUE;
  entry->priv->search_mode = FALSE;

  entry->priv->text_entry = gtk_bin_get_child (GTK_BIN (entry));
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry->priv->text_entry),
                                     GTK_ENTRY_ICON_PRIMARY,
                                     "help-browser");
  gtk_entry_set_icon_activatable (GTK_ENTRY (entry->priv->text_entry),
                                  GTK_ENTRY_ICON_PRIMARY,
                                  FALSE);
  gtk_entry_set_icon_activatable (GTK_ENTRY (entry->priv->text_entry),
                                  GTK_ENTRY_ICON_PRIMARY,
                                  TRUE);

  /* Trying to get the text to line up with the text in the GtkEntry.
   * The text cell is the zeroeth cell right now, since we haven't
   * yet called reorder.  I realize using a guesstimate pixel value
   * won't be perfect all the time, but 4 looks niceish.
   */
  cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (entry));
  g_object_set (cells->data, "xpad", 4, NULL);
  g_list_free (cells);

  entry->priv->icon_cell = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (entry), entry->priv->icon_cell, FALSE);
  gtk_cell_layout_reorder (GTK_CELL_LAYOUT (entry), entry->priv->icon_cell, 0);

  g_signal_connect (entry, "changed",
                    G_CALLBACK (combo_box_changed_cb), NULL);

  g_signal_connect (entry->priv->text_entry, "focus-in-event",
                    G_CALLBACK (entry_focus_in_cb), entry);
  g_signal_connect (entry->priv->text_entry, "focus-out-event",
                    G_CALLBACK (entry_focus_out_cb), entry);
  g_signal_connect (entry->priv->text_entry, "icon-press",
                    G_CALLBACK (entry_icon_press_cb), entry);
  g_signal_connect (entry->priv->text_entry, "key-press-event",
                    G_CALLBACK (entry_key_press_cb), entry);
  g_signal_connect (entry->priv->text_entry, "activate",
                    G_CALLBACK (entry_activate_cb), entry);
}

static void
location_entry_get_property   (GObject      *object,
                               guint         prop_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  YelpLocationEntry *entry = YELP_LOCATION_ENTRY (object);

  switch (prop_id)
    {
    case PROP_ICON_COLUMN:
      g_value_set_int (value, entry->priv->icon_column);
      break;
    case PROP_FLAGS_COLUMN:
      g_value_set_int (value, entry->priv->flags_column);
      break;
    case PROP_ENABLE_SEARCH:
      g_value_set_boolean (value, entry->priv->enable_search);
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
  YelpLocationEntry *entry = YELP_LOCATION_ENTRY (object);

  switch (prop_id)
    {
    case PROP_ICON_COLUMN:
      entry->priv->icon_column = g_value_get_int (value);
      gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (entry),
                                      entry->priv->icon_cell,
                                      "icon-name",
                                      entry->priv->icon_column,
                                      NULL);
      break;
    case PROP_FLAGS_COLUMN:
      entry->priv->flags_column = g_value_get_int (value);
      /* If this is in yelp_location_entry_init, it gets called the first
       * time before flags_column is set, and isn't called again until the
       * "row-changed" signal on the model.  The prevents application code
       * from populating the model before initializing the widget.
       */
      gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (entry),
                                            (GtkTreeViewRowSeparatorFunc)
                                            combo_box_row_separator_func,
                                            entry, NULL);
      break;
    case PROP_ENABLE_SEARCH:
      entry->priv->enable_search = g_value_get_boolean (value);
      gtk_editable_set_editable (GTK_EDITABLE (entry->priv->text_entry),
                                 entry->priv->enable_search);
      if (!entry->priv->enable_search)
        {
          entry->priv->search_mode = FALSE;
          location_entry_set_entry (entry, FALSE);
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
  if (!entry->priv->enable_search)
    return;
  if (clear && !entry->priv->search_mode)
    {
      gtk_entry_set_text (GTK_ENTRY (entry->priv->text_entry), "");
    }
  entry->priv->search_mode = TRUE;
  location_entry_set_entry (entry, FALSE);
  gtk_widget_grab_focus (entry->priv->text_entry);
}

static void
location_entry_set_entry (YelpLocationEntry *entry, gboolean emit)
{
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (entry));
  GtkTreePath *path = NULL;
  GtkTreeIter iter;
  gchar *icon_name;

  if (entry->priv->search_mode)
    {
      gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry->priv->text_entry),
                                         GTK_ENTRY_ICON_PRIMARY,
                                         "system-search");
      gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry->priv->text_entry),
                                         GTK_ENTRY_ICON_SECONDARY,
                                         "edit-clear");
      gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry->priv->text_entry),
                                       GTK_ENTRY_ICON_SECONDARY,
                                       "Clear the search text");
      return;
    }

  if (entry->priv->row)
    path = gtk_tree_row_reference_get_path (entry->priv->row);

  if (path)
    {
      gchar *text;
      gint flags;
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_get (model, &iter,
                          gtk_combo_box_entry_get_text_column (GTK_COMBO_BOX_ENTRY (entry)),
                          &text,
                          entry->priv->icon_column, &icon_name,
                          entry->priv->flags_column, &flags,
                          -1);
      if (flags & YELP_LOCATION_ENTRY_IS_LOADING)
        {
          gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry->priv->text_entry),
                                             GTK_ENTRY_ICON_PRIMARY,
                                             "image-loading");
          g_timeout_add (80, location_entry_pulse, entry);
        }
      else
        {
          gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry->priv->text_entry),
                                             GTK_ENTRY_ICON_PRIMARY,
                                             icon_name);
        }
      if (flags & YELP_LOCATION_ENTRY_CAN_BOOKMARK)
        {
          gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry->priv->text_entry),
                                             GTK_ENTRY_ICON_SECONDARY,
                                             "bookmark-new");
          gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry->priv->text_entry),
                                           GTK_ENTRY_ICON_SECONDARY,
                                           "Bookmark this page");
        }
      else
        {
          gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry->priv->text_entry),
                                             GTK_ENTRY_ICON_SECONDARY,
                                             NULL);
        }
      gtk_entry_set_text (GTK_ENTRY (entry->priv->text_entry), text);
      if (emit)
        g_signal_emit (entry, location_entry_signals[LOCATION_SELECTED], 0);
      g_free (text);
      gtk_tree_path_free (path);
    }
  else
    {
      gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry->priv->text_entry),
                                         GTK_ENTRY_ICON_PRIMARY,
                                         "help-browser");
      gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry->priv->text_entry),
                                         GTK_ENTRY_ICON_SECONDARY,
                                         NULL);
    }
}

static gboolean
location_entry_pulse (gpointer data)
{
  YelpLocationEntry *entry = YELP_LOCATION_ENTRY (data);
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  gint flags;

  model = gtk_tree_row_reference_get_model (entry->priv->row);
  path = gtk_tree_row_reference_get_path (entry->priv->row);
  if (path)
    {
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_get (model, &iter,
                          entry->priv->flags_column, &flags,
                          -1);
      gtk_tree_path_free (path);
    }

  if (flags & YELP_LOCATION_ENTRY_IS_LOADING && !entry->priv->search_mode)
    {
      gtk_entry_progress_pulse (GTK_ENTRY (entry->priv->text_entry));
    }
  else
    {
      gtk_entry_set_progress_fraction (GTK_ENTRY (entry->priv->text_entry), 0.0);
    }

  return flags & YELP_LOCATION_ENTRY_IS_LOADING;
}

static void
combo_box_changed_cb (GtkComboBox  *widget,
                      gpointer      user_data)
{
  YelpLocationEntry *entry = YELP_LOCATION_ENTRY (widget);
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreePath *path;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (entry));

  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (entry), &iter))
    {
      gint flags;
      gtk_tree_model_get (model, &iter,
                          entry->priv->flags_column, &flags,
                          -1);
      if (flags & YELP_LOCATION_ENTRY_IS_SEARCH)
        {
          location_entry_start_search (entry, TRUE);
        }
      else
        {
          path = gtk_tree_model_get_path (model, &iter);
          if (entry->priv->row)
            gtk_tree_row_reference_free (entry->priv->row);
          entry->priv->row = gtk_tree_row_reference_new (model, path);
          gtk_tree_path_free (path);
          entry->priv->search_mode = FALSE;
          location_entry_set_entry (YELP_LOCATION_ENTRY (widget), TRUE);
        }
    }
}

static gboolean
combo_box_row_separator_func (GtkTreeModel  *model,
                              GtkTreeIter   *iter,
                              gpointer       user_data)
{
  YelpLocationEntry *entry = YELP_LOCATION_ENTRY (user_data);
  gint flags;
  gtk_tree_model_get (model, iter,
                      entry->priv->flags_column, &flags,
                      -1);
  return (flags & YELP_LOCATION_ENTRY_IS_SEPARATOR);
}

static void
yelp_location_entry_row_changed (GtkTreeModel  *model,
                                 GtkTreePath   *path,
                                 GtkTreeIter   *iter,
                                 gpointer       user_data)
{
  /* FIXME: Should we bother checking path/iter against entry->priv->row?
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
  YelpLocationEntry *entry = YELP_LOCATION_ENTRY (user_data);

  if (entry->priv->enable_search && !entry->priv->search_mode)
    location_entry_start_search (entry, TRUE);

  return FALSE;
}

static gboolean
entry_focus_out_cb (GtkWidget         *widget,
                    GdkEventFocus     *event,
                    gpointer           user_data)
{
  YelpLocationEntry *entry = YELP_LOCATION_ENTRY (user_data);

  if (gtk_entry_get_text_length (GTK_ENTRY (widget)) == 0)
    {
      entry->priv->search_mode = FALSE;
      location_entry_set_entry (entry, FALSE);
    }
}

static void
entry_activate_cb (GtkEntry  *text_entry,
                   gpointer   user_data)
{
  YelpLocationEntry *entry = YELP_LOCATION_ENTRY (user_data);
  gchar *text = g_strdup (gtk_entry_get_text (text_entry));

  if (!entry->priv->enable_search)
    return;

  if (!entry->priv->search_mode || text == NULL || strlen(text) == 0)
    return;

  g_signal_emit (entry, location_entry_signals[SEARCH_ACTIVATED], 0, text);

  g_free (text);
}

static void
entry_icon_press_cb (GtkEntry            *text_entry,
                     GtkEntryIconPosition icon_pos,
                     GdkEvent            *event,
                     gpointer             user_data)
{
  YelpLocationEntry *entry = YELP_LOCATION_ENTRY (user_data);
  if (icon_pos == GTK_ENTRY_ICON_SECONDARY)
    {
      const gchar *name = gtk_entry_get_icon_name (text_entry, icon_pos);
      if (g_str_equal (name, "edit-clear"))
        {
          entry->priv->search_mode = FALSE;
          location_entry_set_entry (entry, FALSE);
        }
      else if  (g_str_equal (name, "bookmark-new"))
        {
          /* FIXME: emit bookmark signal */
        }
    }
}

static gboolean
entry_key_press_cb (GtkWidget   *widget,
                    GdkEventKey *event,
                    gpointer     user_data)
{
  YelpLocationEntry *entry = YELP_LOCATION_ENTRY (user_data);
  if (event->keyval == GDK_Escape)
    {
      entry->priv->search_mode = FALSE;
      location_entry_set_entry (entry, FALSE);
      return TRUE;
    }
  else if (!entry->priv->search_mode)
    {
      location_entry_start_search (entry, FALSE);
    }

  return FALSE;
}

/**
 * yelp_location_entry_ew_with_model:
 * @model: A #GtkTreeModel.
 * @text_column: The column in @model containing the title of each entry.
 * @icon_column: The column in @model containing the icon name of each entry.
 * @bookmark_column: The column in @model containing a gboolean specifying
 * whether or not each entry is bookmarked.
 *
 * Creates a new #YelpLocationEntry widget.
 *
 * Return value: A new #YelpLocationEntry.
 */
GtkWidget*
yelp_location_entry_new_with_model (GtkTreeModel *model,
                                    gint          text_column,
                                    gint          icon_column,
                                    gint          flags_column)
{
  GtkWidget *ret;
  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);

  ret = GTK_WIDGET (g_object_new (YELP_TYPE_LOCATION_ENTRY,
                                  "model", model,
                                  "text-column", text_column,
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

void
yelp_location_entry_start_search (YelpLocationEntry *entry)
{
  location_entry_start_search (entry, TRUE);
}
