/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003 Shaun McCance  <shaunm@gnome.org>
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
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "yelp-bookmarks.h"
#include "yelp-utils.h"
#include "yelp-window.h"
#include "yelp-debug.h"

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/xmlwriter.h>

#define COL_NAME   0
#define COL_LABEL  1
#define COL_EDIT   2
#define COL_HEADER 3
#define TOC_PATH "ui/menubar/BookmarksMenu/BookmarksTOC"
#define DOC_PATH "ui/menubar/BookmarksMenu/BookmarksDOC"
#define BK_CONFIG_BK_GROUP "Bookmarks"
#define BK_CONFIG_WIDTH  "width"
#define BK_CONFIG_HEIGHT "height"
#define BK_CONFIG_WIDTH_DEFAULT  360
#define BK_CONFIG_HEIGHT_DEFAULT 360

static GSList *windows;
static GtkTreeStore *actions_store;
static gchar *dup_title;

static GtkWidget *bookmarks_dialog = NULL;
static GtkWidget *edit_open_button = NULL;
static GtkWidget *edit_rename_button = NULL;
static GtkWidget *edit_remove_button = NULL;


static gboolean have_toc = FALSE;
static gboolean have_doc = FALSE;
static GtkTreeIter toc_iter;
static GtkTreeIter doc_iter;

typedef struct _YelpWindowData YelpWindowData;
struct _YelpWindowData {
    YelpWindow *window;
    guint merge_id;
    GtkActionGroup *toc_actions;
    GtkActionGroup *doc_actions;
};

static const char ui_description[] =
    "<ui>"
    "  <popup>"
    "    <menuitem action='OpenName'/>"
    "    <separator/>"
    "    <menuitem action='EditName'/>"
    "    <menuitem action='DeleteName'/>"
    "  </popup>"
    "</ui>";

static void      window_add_bookmark        (YelpWindowData *window,
					     gchar          *name,
					     const gchar    *label);
static void      bookmarks_add_bookmark     (const gchar    *uri,
					     const gchar    *title,
					     gboolean        save);
static void      bookmark_add_response_cb   (GtkDialog      *dialog,
					     gint            id,
					     gchar          *uri);
static void      window_remove_bookmark_menu(YelpWindowData *data);
static void      window_remove_action       (YelpWindowData *data,
					     gchar          *name);
static gboolean  bookmarks_unregister       (YelpWindow     *window,
					     GdkEvent       *event,
					     gpointer        user_data);
static void      action_activated_cb        (GtkActionGroup *group,
					     GtkAction      *action,
					     YelpWindowData *data);
static void      bookmarks_rebuild_menus    (void);
static void      bookmarks_key_event_cb     (GtkWidget         *widget,
					     GdkEventKey       *event,
					     gpointer           user_data);
static void      bookmarks_open_button_cb   (GtkWidget         *widget,
					     GtkTreeView       *view);
static void      bookmarks_rename_button_cb (GtkWidget         *widget,
					     GtkTreeView       *view);
static void      bookmarks_remove_button_cb (GtkWidget         *widget,
					     GtkTreeView       *view);
static void      bookmarks_ensure_valid     (void);
static gboolean  bookmarks_read             (const gchar       *filename);

static gboolean  bookmarks_dup_finder       (GtkTreeModel   *model,
					     GtkTreePath    *path,
					     GtkTreeIter    *iter,
					     gpointer        data);
static void      bookmarks_open_cb          (GtkTreeView         *view,
					     GtkTreePath         *path,
					     GtkTreeViewColumn   *col,
					     gpointer             user_data);
static void      bookmarks_cell_edited_cb   (GtkCellRendererText *cell,
					     const gchar         *path_string,
					     const gchar         *new_title,
					     gpointer             user_data);
static void      bookmarks_popup_menu       (GtkWidget           *widget,
					     GdkEventButton      *event);
static void      bookmarks_menu_edit_cb     (GtkAction           *action,
					     GtkWidget           *widget);
static void      bookmarks_menu_remove_cb   (GtkAction           *action,
					     GtkWidget           *widget);
static void      bookmarks_menu_open_cb     (GtkAction           *action,
					     GtkWidget           *widget);
static gboolean  bookmarks_button_press_cb  (GtkWidget           *widget,
					     GdkEventButton      *event);
static gboolean  bookmarks_configure_cb     (GtkWidget           *widget,
					     GdkEventConfigure   *event,
					     gpointer             data);
static void      selection_changed_cb       (GtkTreeSelection    *selection,
					     gpointer             data);

static const GtkActionEntry popup_entries[] = {
    { "OpenName", GTK_STOCK_OPEN,
      N_("Open Bookmark in New Window"),
      NULL, NULL,
      G_CALLBACK (bookmarks_menu_open_cb)},
    { "EditName", NULL,
      N_("Rename Bookmark"),
      NULL, NULL,
      G_CALLBACK (bookmarks_menu_edit_cb)},
    { "DeleteName", GTK_STOCK_REMOVE,
      N_("Remove Bookmark"),
      NULL, NULL,
      G_CALLBACK (bookmarks_menu_remove_cb)}
};


void
yelp_bookmarks_init (void)
{
    gchar   *filename = NULL;
    const gchar *override;

    windows = NULL;
    actions_store = gtk_tree_store_new (4,
					G_TYPE_STRING,
					G_TYPE_STRING,
					G_TYPE_BOOLEAN,
					G_TYPE_BOOLEAN);

    override = g_getenv ("GNOME22_USER_DIR");

    if (override)
        filename = g_build_filename (override, "yelp-bookmarks.xbel", NULL);
    else
        filename = g_build_filename (g_get_home_dir (), ".gnome2",
				     "yelp-bookmarks.xbel", NULL);

    if (g_file_test (filename, G_FILE_TEST_EXISTS))
	bookmarks_read (filename);

    g_free (filename);
}

void
yelp_bookmarks_register (YelpWindow *window)
{
    GtkUIManager   *ui_manager;
    YelpWindowData *data;
    GtkTreeIter     top_iter, sub_iter;
    gboolean        top_valid, sub_valid;

    ui_manager = yelp_window_get_ui_manager (window);

    g_signal_connect (window, "delete-event",
		      G_CALLBACK (bookmarks_unregister), NULL);

    data = g_new0 (YelpWindowData, 1);
    data->window = window;
    data->toc_actions = gtk_action_group_new ("BookmarksTOC");
    data->doc_actions = gtk_action_group_new ("BookmarksDOC");
    data->merge_id = gtk_ui_manager_new_merge_id(ui_manager);

    gtk_ui_manager_insert_action_group (ui_manager, data->toc_actions, 1);
    gtk_ui_manager_insert_action_group (ui_manager, data->doc_actions, 2);

    g_signal_connect (data->toc_actions,
		      "post-activate",
		      G_CALLBACK (action_activated_cb),
		      data);
    g_signal_connect (data->doc_actions,
		      "post-activate",
		      G_CALLBACK (action_activated_cb),
		      data);

    top_valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (actions_store),
					       &top_iter);
    while (top_valid) {
	if (gtk_tree_model_iter_has_child (GTK_TREE_MODEL (actions_store),
					   &top_iter)) {
	    sub_valid = gtk_tree_model_iter_children (GTK_TREE_MODEL (actions_store),
						      &sub_iter, &top_iter);
	    while (sub_valid) {
		gchar *label, *name;
		gtk_tree_model_get (GTK_TREE_MODEL (actions_store), &sub_iter,
				    COL_NAME,  &name,
				    COL_LABEL, &label,
				    -1);
		window_add_bookmark (data, name, label);
		g_free (name);
		g_free (label);

		sub_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (actions_store),
						      &sub_iter);
	    }
	}
	top_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (actions_store),
					      &top_iter);
    }

    windows = g_slist_append (windows, data);
}

static void
bookmarks_ensure_valid (void)
{
	if (have_toc &&
	    !gtk_tree_model_iter_has_child (GTK_TREE_MODEL (actions_store),
					    &toc_iter)) {
	    gtk_tree_store_remove (actions_store, &toc_iter);
	    have_toc = FALSE;
	}
	if (have_doc &&
	    !gtk_tree_model_iter_has_child (GTK_TREE_MODEL (actions_store),
					    &doc_iter)) {
	    gtk_tree_store_remove (actions_store, &doc_iter);
	    have_doc = FALSE;
	}
}

static gboolean
bookmarks_dup_finder (GtkTreeModel *model, GtkTreePath *path,
		      GtkTreeIter *iter, gpointer data)
{
    gchar *uri = data;
    gchar *current_uri;

    gtk_tree_model_get (model, iter,
			COL_NAME, &current_uri,
			-1);

    if (current_uri && g_str_equal (current_uri, uri)) {
	gtk_tree_model_get (model, iter,
			    COL_LABEL, &dup_title,
			    -1);
	g_free (current_uri);
	return TRUE;
    }

    g_free (current_uri);
    return FALSE;
}


void
yelp_bookmarks_add (const gchar *uri, YelpWindow *window)
{
    GtkBuilder  *builder;
    GError      *error = NULL;
    GtkWidget   *dialog;
    GtkEntry    *entry;
    gchar       *title;
    gchar       *dup_uri;

    title = (gchar *) gtk_window_get_title (GTK_WINDOW (window));
    dup_uri = g_strdup (uri);

    if (dup_title)
	g_free (dup_title);
    dup_title = NULL;
    gtk_tree_model_foreach (GTK_TREE_MODEL (actions_store),
			    bookmarks_dup_finder, dup_uri);
    if (dup_title) {
	char *escaped_title, *markup;

	dialog = gtk_message_dialog_new_with_markup
	    (GTK_WINDOW (window),
	     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	     GTK_MESSAGE_INFO,
	     GTK_BUTTONS_OK,
	     NULL);

	escaped_title = g_markup_printf_escaped
		("<b>%s</b>", dup_title);
	markup = g_strdup_printf
		(_("A bookmark titled %s already exists for this page."),
		escaped_title);
	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), markup);
	g_free (markup);
	g_free (escaped_title);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	g_free (dup_title);
	dup_title = NULL;
	return;
    }

    builder = gtk_builder_new ();
    if (!gtk_builder_add_from_file (builder,
                                    DATADIR "/yelp/ui/yelp-bookmarks-add.ui",
                                    &error)) {
        g_warning ("Could not load builder file: %s", error->message);
        g_error_free(error);
        return;
    }

    dialog = GTK_WIDGET (gtk_builder_get_object (builder, "add_bookmark_dialog"));
    entry = GTK_ENTRY (gtk_builder_get_object (builder, "bookmark_title_entry"));
    gtk_entry_set_text (entry, title);
    gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);

    g_object_set_data (G_OBJECT (dialog), "title_entry", entry);

    g_signal_connect (G_OBJECT (dialog),
		      "response",
		      G_CALLBACK (bookmark_add_response_cb),
		      dup_uri);

    gtk_window_present (GTK_WINDOW (dialog));
}

static void
bookmark_add_response_cb (GtkDialog *dialog, gint id, gchar *uri)
{
    GtkEntry *entry;
    gchar *title;

    if (id == GTK_RESPONSE_OK) {
	entry = GTK_ENTRY (g_object_get_data (G_OBJECT (dialog), "title_entry"));
	title = (gchar *) gtk_entry_get_text (entry);

	bookmarks_add_bookmark (uri, title, TRUE);
    }

    g_free (uri);
    gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
bookmarks_add_bookmark (const gchar  *uri,
			const gchar  *title,
			gboolean      save)
{
    GtkTreeIter  iter;
    GSList      *cur;
    gboolean     tocQ;

    if (dup_title)
	g_free (dup_title);
    dup_title = NULL;
    gtk_tree_model_foreach (GTK_TREE_MODEL (actions_store),
			    bookmarks_dup_finder, (gpointer) uri);
    if (dup_title) {
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new_with_markup
	    (NULL,
	     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	     GTK_MESSAGE_ERROR,
	     GTK_BUTTONS_OK,
	     _("A bookmark titled <b>%s</b> already exists for this page."),
	     dup_title);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	g_free (dup_title);
	dup_title = NULL;
	return;
    }

    tocQ = g_str_has_prefix (uri, "x-yelp-toc:");
    if (tocQ) {
	if (!have_toc) {
	    gchar *label = g_markup_printf_escaped ("<b>%s</b>",
						    _("Help Topics"));
	    gtk_tree_store_prepend (actions_store, &toc_iter, NULL);
	    gtk_tree_store_set (actions_store, &toc_iter,
				COL_NAME,   "",
				COL_LABEL,  label,
				COL_EDIT,   FALSE,
				COL_HEADER, TRUE,
				-1);
	    have_toc = TRUE;
	    g_free (label);
	}
	gtk_tree_store_append (actions_store, &iter, &toc_iter);
    } else {
	if (!have_doc) {
	    gchar *label = g_markup_printf_escaped ("<b>%s</b>",
						    _("Document Sections"));
	    gtk_tree_store_append (actions_store, &doc_iter, NULL);
	    gtk_tree_store_set (actions_store, &doc_iter,
				COL_NAME,   "",
				COL_LABEL,  label,
				COL_EDIT,   FALSE,
				COL_HEADER, TRUE,
				-1);
	    have_doc = TRUE;
	    g_free (label);
	}
	gtk_tree_store_append (actions_store, &iter, &doc_iter);
    }

    gtk_tree_store_set (actions_store, &iter,
			COL_NAME,   uri,
			COL_LABEL,  title,
			COL_EDIT,   TRUE,
			COL_HEADER, FALSE,
			-1);
    if (save) {
	for (cur = windows; cur != NULL; cur = cur->next) {
	    window_add_bookmark ((YelpWindowData *) cur->data, (gchar *) uri,
				 title);
	}
	yelp_bookmarks_write ();
    }
}

static void
window_remove_bookmark_menu (YelpWindowData *data)
{
    GtkUIManager *ui_manager;

    ui_manager = yelp_window_get_ui_manager (data->window);

    gtk_ui_manager_remove_ui (ui_manager, data->merge_id);

    data->merge_id = gtk_ui_manager_new_merge_id (ui_manager);

    gtk_ui_manager_ensure_update (ui_manager);
}

static void
window_remove_action (YelpWindowData *data, gchar *name)
{
    GtkAction *action = gtk_action_group_get_action (data->toc_actions, name);
    if (action) {
	gtk_action_group_remove_action (data->toc_actions, action);
    }
    else {
	action = gtk_action_group_get_action (data->doc_actions, name);
	if (action) {
	    gtk_action_group_remove_action (data->doc_actions, action);
	}
    }
}

static void
bookmarks_rebuild_menus (void)
{
    GSList      *cur;
    GtkTreeIter top_iter, sub_iter;
    gboolean top_valid, sub_valid;

    for (cur = windows; cur != NULL; cur = cur->next) {
	window_remove_bookmark_menu ((YelpWindowData *) cur->data);
    }

    top_valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (actions_store),
					       &top_iter);
    while (top_valid) {
	if (gtk_tree_model_iter_has_child (GTK_TREE_MODEL (actions_store),
					   &top_iter)) {
	    sub_valid = gtk_tree_model_iter_children (GTK_TREE_MODEL (actions_store),
						      &sub_iter, &top_iter);
	    while (sub_valid) {
		gchar *name, *label;
		gtk_tree_model_get (GTK_TREE_MODEL (actions_store), &sub_iter,
				    COL_NAME,   &name,
				    COL_LABEL,  &label,
				    -1);
		for (cur = windows; cur != NULL; cur = cur->next) {
		    window_remove_action (cur->data, name);
		    window_add_bookmark (cur->data, name, label);
		}
		g_free (name);
		g_free (label);

		sub_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (actions_store),
						      &sub_iter);
	    }
	}
	top_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (actions_store),
					      &top_iter);
    }

    yelp_bookmarks_write ();
}

static void
bookmarks_key_event_cb (GtkWidget   *widget,
			GdkEventKey *event,
			gpointer     user_data)
{
    GtkTreeView *view = GTK_TREE_VIEW (widget);
    GtkTreePath *path;
    GtkTreeSelection *sel;
    GtkTreeModel *model;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model (view);
    sel = gtk_tree_view_get_selection (view);

    if (gtk_tree_selection_get_selected (sel, &model, &iter)) {
	path = gtk_tree_model_get_path (model, &iter);

	switch (event->keyval) {
	case GDK_BackSpace:
	case GDK_Delete:
	    if (!gtk_tree_model_iter_has_child (GTK_TREE_MODEL (actions_store),
						&iter)) {
		gtk_tree_store_remove (actions_store, &iter);
		bookmarks_ensure_valid ();
		bookmarks_rebuild_menus ();
	    }
	    break;
	case GDK_Return:
	case GDK_KP_Enter:
	    bookmarks_open_cb (view, path, NULL, NULL);
	    break;
	case GDK_Up:
	    gtk_tree_path_prev (path);
	    gtk_tree_selection_select_path (sel, path);
	    break;
	case GDK_Down:
	    gtk_tree_path_next (path);
	    gtk_tree_selection_select_path (sel, path);
	    break;
	default:
	    break;
	}
    }
}

static void
bookmarks_open_cb (GtkTreeView *view, GtkTreePath *path,
		   GtkTreeViewColumn *col, gpointer user_data)
{
    GtkTreeIter iter;
    gchar *name;
    gchar *title;
    GSList *cur;
    YelpWindowData *data;

    gtk_tree_model_get_iter (GTK_TREE_MODEL (actions_store), &iter, path);
    if (!gtk_tree_model_iter_has_child (GTK_TREE_MODEL (actions_store),
					&iter)) {
	gtk_tree_model_get (GTK_TREE_MODEL (actions_store), &iter,
			    COL_NAME, &name,
			    COL_LABEL, &title, -1);

	cur = windows;
	data = cur->data;

	g_signal_emit_by_name (data->window, "new_window_requested", name, NULL);
    }
}


static void
bookmarks_cell_edited_cb (GtkCellRendererText *cell, const gchar *path_string,
			  const gchar *new_title, gpointer user_data)
{
    GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
    GtkTreeIter it;

    gtk_tree_model_get_iter (GTK_TREE_MODEL (actions_store), &it, path);

    gtk_tree_store_set (actions_store, &it,
			COL_LABEL, new_title, -1);
    bookmarks_rebuild_menus ();

}

void
yelp_bookmarks_edit (void)
{
    GtkBuilder *builder;
    GError *error = NULL;
    GtkTreeView *view;
    GtkTreeSelection *select;
    GtkCellRenderer *renderer;
    gint width, height;
    GKeyFile *keyfile;
    GError *config_error = NULL;
    gchar *config_path;
    const gchar *override;

    if (!bookmarks_dialog) {
        builder = gtk_builder_new ();
        if (!gtk_builder_add_from_file (builder,
                                        DATADIR "/yelp/ui/yelp-bookmarks.ui",
                                        &error)) {
            g_warning ("Could not load builder file: %s", error->message);
            g_error_free(error);
            return;
        }

        bookmarks_dialog = GTK_WIDGET (gtk_builder_get_object (builder, "bookmarks_dialog"));
	view = GTK_TREE_VIEW (gtk_builder_get_object (builder, "bookmarks_view"));
 	keyfile = g_key_file_new();
        override = g_getenv ("GNOME22_USER_DIR");
        if (override)
            config_path = g_build_filename (override, "yelp", NULL);
        else
            config_path = g_build_filename (g_get_home_dir(),
                                            ".gnome2", "yelp", NULL);

 	if( !g_key_file_load_from_file (keyfile, config_path,
					G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
					&config_error) ) {
	    g_warning ("Failed to load config file: %s\n", config_error->message);
	    g_error_free (config_error);

	    width = BK_CONFIG_WIDTH_DEFAULT;
	    height = BK_CONFIG_HEIGHT_DEFAULT;
	} else {
	    width = g_key_file_get_integer (keyfile, BK_CONFIG_BK_GROUP,
					    BK_CONFIG_WIDTH, NULL);
	    height = g_key_file_get_integer (keyfile, BK_CONFIG_BK_GROUP,
					     BK_CONFIG_HEIGHT, NULL);

	    if (width == 0)
		width = BK_CONFIG_WIDTH_DEFAULT;
	    if (height == 0)
		height = BK_CONFIG_HEIGHT_DEFAULT;
	}

 	g_free (config_path);
 	g_key_file_free (keyfile);
	gtk_window_set_default_size (GTK_WINDOW (bookmarks_dialog),
				     width, height);

	g_signal_connect (G_OBJECT (bookmarks_dialog), "response",
			  G_CALLBACK (gtk_widget_hide), NULL);
	g_signal_connect (G_OBJECT (bookmarks_dialog), "delete_event",
			  G_CALLBACK (gtk_widget_hide_on_delete), NULL);

	renderer = gtk_cell_renderer_text_new ();

	gtk_tree_view_insert_column_with_attributes
	    (view, -1,
	     NULL, renderer,
	     "markup", COL_LABEL,
	     "editable", COL_EDIT,
	     NULL);
	gtk_tree_view_set_model (view, GTK_TREE_MODEL (actions_store));

	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
	gtk_tree_view_expand_all (GTK_TREE_VIEW (view));

	g_signal_connect (G_OBJECT (view), "row-activated",
			  G_CALLBACK (bookmarks_open_cb),
			  NULL);
	g_signal_connect (G_OBJECT (view), "key-press-event",
			  G_CALLBACK (bookmarks_key_event_cb),
			  NULL);
	g_signal_connect (G_OBJECT (renderer), "edited",
			  G_CALLBACK (bookmarks_cell_edited_cb), NULL);

	edit_open_button = GTK_WIDGET (gtk_builder_get_object (builder, "open_button"));
	g_signal_connect (G_OBJECT (edit_open_button), "clicked",
			  G_CALLBACK (bookmarks_open_button_cb),
			  view);
	edit_rename_button = GTK_WIDGET (gtk_builder_get_object (builder, "rename_button"));
	g_signal_connect (G_OBJECT (edit_rename_button), "clicked",
			  G_CALLBACK (bookmarks_rename_button_cb),
			  view);
	edit_remove_button = GTK_WIDGET (gtk_builder_get_object (builder, "remove_button"));
	g_signal_connect (G_OBJECT (edit_remove_button), "clicked",
			  G_CALLBACK (bookmarks_remove_button_cb),
			  view);
	g_signal_connect (G_OBJECT (select), "changed",
			  G_CALLBACK (selection_changed_cb),
			  NULL);
	g_signal_connect (G_OBJECT (view), "button-press-event",
			  G_CALLBACK (bookmarks_button_press_cb),
			  NULL);
	g_signal_connect (G_OBJECT (bookmarks_dialog), "configure-event",
			  G_CALLBACK (bookmarks_configure_cb),
			  NULL);
    }

    gtk_window_present (GTK_WINDOW (bookmarks_dialog));
}

static gboolean
bookmarks_configure_cb (GtkWidget *widget, GdkEventConfigure *event,
			gpointer data)
{
    gint width, height;
    GKeyFile *keyfile;
    gchar *config_path, *sdata;
    GError *config_error = NULL;
    gsize config_size;
    const gchar *override;

    gtk_window_get_size (GTK_WINDOW (widget), &width, &height);

    keyfile = g_key_file_new ();
    override = g_getenv ("GNOME22_USER_DIR");
    if (override)
        config_path = g_build_filename (override, "yelp", NULL);
    else
        config_path = g_build_filename (g_get_home_dir (),
                                        ".gnome2", "yelp", NULL);

    g_key_file_set_integer (keyfile, BK_CONFIG_BK_GROUP,
			    BK_CONFIG_WIDTH, width);
    g_key_file_set_integer (keyfile, BK_CONFIG_BK_GROUP,
			    BK_CONFIG_HEIGHT, height);

    sdata = g_key_file_to_data (keyfile, &config_size, NULL);

    if ( !g_file_set_contents (config_path, sdata, config_size,
			       &config_error) ) {
	g_warning ("Failed to save config file: %s\n", config_error->message);
	g_error_free (config_error);
    }

    g_free (sdata);
    g_free (config_path);
    g_key_file_free (keyfile);

    return FALSE;
}

static void
selection_changed_cb (GtkTreeSelection *selection, gpointer data)
{
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected (selection, NULL, &iter) &&
	!gtk_tree_model_iter_has_child (GTK_TREE_MODEL (actions_store),
					&iter)) {
	/*A row is highlighted - sensitise the various widgets*/
	gtk_widget_set_sensitive (GTK_WIDGET (edit_open_button), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (edit_rename_button), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (edit_remove_button), TRUE);
    }
    else {
	/*No row is highlighted - desensitise the various widgets*/
	gtk_widget_set_sensitive (GTK_WIDGET (edit_open_button), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (edit_rename_button), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (edit_remove_button), FALSE);

    }
}


static void
bookmarks_open_button_cb (GtkWidget *widget, GtkTreeView *view)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeModel *model;
    GtkTreeSelection *select;

    model = gtk_tree_view_get_model (view);

    select = gtk_tree_view_get_selection (view);
    if (gtk_tree_selection_get_selected (select, NULL, &iter)) {
	path = gtk_tree_model_get_path (model, &iter);

	bookmarks_open_cb (view, path, NULL, NULL);

	gtk_tree_path_free (path);
    }
}

static void
bookmarks_rename_button_cb (GtkWidget *widget, GtkTreeView *view)
{
    GtkTreePath *path;
    GtkTreeSelection *sel;
    GtkTreeIter iter;
    GtkTreeViewColumn *col;
    GtkTreeModel *model = GTK_TREE_MODEL (actions_store);
    gboolean selection;

    sel = gtk_tree_view_get_selection (view);
    selection = gtk_tree_selection_get_selected (sel, &model, &iter);
    if (selection) {
	path = gtk_tree_model_get_path (model, &iter);
	col = gtk_tree_view_get_column (view, 0);

	gtk_tree_view_set_cursor (view, path, col, TRUE);
    }
}

static void
bookmarks_remove_button_cb (GtkWidget *widget, GtkTreeView *view)
{
    GtkTreeIter iter;
    GtkTreeSelection *select;

    select = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
    if (gtk_tree_selection_get_selected (select, NULL, &iter)) {

	gtk_tree_store_remove (actions_store, &iter);
	bookmarks_ensure_valid ();
	bookmarks_rebuild_menus ();
    }
}

static void
bookmarks_popup_menu (GtkWidget *widget,GdkEventButton *event)
{
    GtkWidget *menu;
    gint button, event_time;
    GtkActionGroup *action_group;
    GtkUIManager *ui_manager;
    GError *error;

    action_group = gtk_action_group_new ("PopupActions");
    gtk_action_group_set_translation_domain (action_group,
                                            GETTEXT_PACKAGE);
    gtk_action_group_add_actions (action_group, popup_entries,
                                 G_N_ELEMENTS (popup_entries), widget);

    ui_manager = gtk_ui_manager_new ();
    gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

    if (!gtk_ui_manager_add_ui_from_string (ui_manager,
                                           ui_description, -1, &error)) {
       g_message ("Building menus failed: %s", error->message);
       g_error_free (error);
       exit (EXIT_FAILURE);
    }
    menu = gtk_ui_manager_get_widget (ui_manager, "/popup");

    if (event) {
       button = event->button;
       event_time = event->time;
    } else {
       button = 0;
       event_time = gtk_get_current_event_time ();
    }
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                   button, event_time);

}

static void
bookmarks_menu_edit_cb (GtkAction *action, GtkWidget *widget)
{
    GtkTreeView *view = GTK_TREE_VIEW (widget);
    GtkTreePath *path;
    GtkTreeSelection *sel;
    GtkTreeIter iter;
    GtkTreeViewColumn *col;
    GtkTreeModel *model = GTK_TREE_MODEL (actions_store);

    sel = gtk_tree_view_get_selection (view);
    if (gtk_tree_selection_get_selected (sel, &model, &iter)) {
	path = gtk_tree_model_get_path (model, &iter);
	col = gtk_tree_view_get_column (view, 0);

	gtk_tree_view_set_cursor (view, path, col, TRUE);
    }
}

static void
bookmarks_menu_remove_cb (GtkAction *action, GtkWidget *widget)
{
    GtkTreeView *view = GTK_TREE_VIEW (widget);
    GtkTreeSelection *sel;
    GtkTreeIter iter;

    sel = gtk_tree_view_get_selection (view);
    if (gtk_tree_selection_get_selected (sel, NULL, &iter)) {

	gtk_tree_store_remove (actions_store, &iter);
	bookmarks_ensure_valid ();
	bookmarks_rebuild_menus ();
    }
}

static void
bookmarks_menu_open_cb (GtkAction *action, GtkWidget *widget)
{
    GtkTreeView *view = GTK_TREE_VIEW (widget);
    GtkTreeSelection *sel = gtk_tree_view_get_selection (view);
    GtkTreeIter iter;
    GtkTreeModel *model = GTK_TREE_MODEL (actions_store);
    GtkTreePath *path;

    if (gtk_tree_selection_get_selected (sel, &model, &iter)) {
	path = gtk_tree_model_get_path (model, &iter);

	bookmarks_open_cb (view, path, NULL, NULL);
    }
}

static gboolean
bookmarks_button_press_cb (GtkWidget *widget, GdkEventButton *event)
{
    GtkTreeView *view = GTK_TREE_VIEW (widget);
    GtkTreeSelection *sel = gtk_tree_view_get_selection (view);
    gboolean selection;
    GtkTreeIter iter;
    GtkTreeModel *model = GTK_TREE_MODEL (actions_store);
    gboolean editable;

    selection = gtk_tree_selection_get_selected (sel,
                                                &model,
                                                &iter);
    if (selection) {
	gtk_tree_model_get (GTK_TREE_MODEL (actions_store), &iter,
			    COL_EDIT, &editable, -1);
       if (event->button == 3 && event->type == GDK_BUTTON_PRESS &&
	   editable) {
           bookmarks_popup_menu (widget, event);
	   return TRUE;
       }
    }
    return FALSE;
}


void
yelp_bookmarks_write (void)
{
    xmlTextWriterPtr file;
    GtkTreeIter top_iter, sub_iter;
    gboolean top_valid, sub_valid;
    gchar *filename;
    const gchar *override;

    override = g_getenv ("GNOME22_USER_DIR");

    if (override)
        filename = g_build_filename (override, "yelp-bookmarks.xbel", NULL);
    else
        filename = g_build_filename (g_get_home_dir (), ".gnome2",
				     "yelp-bookmarks.xbel", NULL);

    file = xmlNewTextWriterFilename (filename,
				     0);
    if(!file) {
	g_warning ("Could not create bookmark file %s", filename);
	return;
    }

    xmlTextWriterStartDocument (file, NULL, NULL, NULL);
    xmlTextWriterStartElement (file, BAD_CAST "xbel");
    xmlTextWriterWriteAttribute(file, BAD_CAST "version",
                                     BAD_CAST "1.0");

    xmlTextWriterStartElement (file, BAD_CAST "info");
    xmlTextWriterStartElement (file, BAD_CAST "metadata");
    xmlTextWriterWriteAttribute(file, BAD_CAST "owner",
                                     BAD_CAST "http://live.gnome.org/Yelp");
    xmlTextWriterEndElement (file);
    xmlTextWriterEndElement (file);

    top_valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (actions_store),
					       &top_iter);

    while (top_valid) {
	if (gtk_tree_model_iter_has_child (GTK_TREE_MODEL (actions_store),
					   &top_iter)) {
	    sub_valid = gtk_tree_model_iter_children (GTK_TREE_MODEL (actions_store),
						      &sub_iter, &top_iter);
	    while (sub_valid) {
		gchar *name;
		gchar *label;
		gtk_tree_model_get (GTK_TREE_MODEL (actions_store), &sub_iter,
				    COL_NAME, &name,
				    COL_LABEL, &label,
				    -1);
		xmlTextWriterStartElement (file, BAD_CAST "bookmark");
 		xmlTextWriterWriteAttribute (file, BAD_CAST "href",
					     BAD_CAST name);

		xmlTextWriterWriteElement (file,
					   BAD_CAST "title",
					   BAD_CAST label);

		xmlTextWriterStartElement (file, BAD_CAST "info");
		xmlTextWriterStartElement (file, BAD_CAST "metadata");
		xmlTextWriterWriteAttribute
		    (file, BAD_CAST "owner",
		     BAD_CAST "http://live.gnome.org/Yelp");
		xmlTextWriterEndElement (file); /* metadata */
		xmlTextWriterEndElement (file); /* info */
		xmlTextWriterEndElement (file); /* bookmark */

		g_free (name);
		g_free (label);

		sub_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (actions_store),
						      &sub_iter);
	    }
	}
	top_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (actions_store),
					      &top_iter);
    }

    xmlTextWriterEndDocument(file);
    xmlFreeTextWriter(file);
    g_free (filename);
    return;
}

static gboolean
bookmarks_read (const gchar *filename)
{
    xmlParserCtxtPtr   parser;
    xmlXPathContextPtr xpath;
    xmlXPathObjectPtr  obj;
    xmlDocPtr          doc;

    gboolean  ret = TRUE;
    gint      i;

    parser = xmlNewParserCtxt ();
    doc = xmlCtxtReadFile (parser, filename, NULL,
			   XML_PARSE_NOBLANKS | XML_PARSE_NOCDATA  |
			   XML_PARSE_NOENT    | XML_PARSE_NOERROR  |
			   XML_PARSE_NONET    );

    if (doc == NULL) {
	if (parser)
		xmlFreeParserCtxt (parser);
	return FALSE;
    }

    xpath = xmlXPathNewContext (doc);
    obj = xmlXPathEvalExpression (BAD_CAST "/xbel/bookmark", xpath);
    for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	xmlNodePtr node = obj->nodesetval->nodeTab[i];
	gchar *uri;
	uri = (gchar *) xmlGetProp (node, BAD_CAST "href");
	if (uri) {
	    xmlXPathObjectPtr title_obj;
	    xpath->node = node;
	    title_obj = xmlXPathEvalExpression (BAD_CAST "string(title[1])",
						xpath);
	    xpath->node = NULL;

	    if (title_obj->stringval)
		bookmarks_add_bookmark ((const gchar *) uri,
					(const gchar *) title_obj->stringval,
					FALSE);

	    xmlXPathFreeObject (title_obj);
	    xmlFree (uri);
	}
    }

    if (obj)
	xmlXPathFreeObject (obj);
    if (xpath)
	xmlXPathFreeContext (xpath);
    if (parser)
	xmlFreeParserCtxt (parser);
    if (doc)
	xmlFreeDoc (doc);
    return ret;
}

static void
window_add_bookmark (YelpWindowData *data,
		     gchar          *name,
		     const gchar    *label)
{
    GtkUIManager *ui_manager;
    GtkAction    *action;
    gboolean tocQ;

    tocQ = g_str_has_prefix (name, "x-yelp-toc:");
    ui_manager = yelp_window_get_ui_manager (data->window);
    action = gtk_action_new (name, label, label, NULL);

    gtk_action_group_add_action (tocQ ? data->toc_actions : data->doc_actions,
				 action);

    gtk_ui_manager_add_ui (ui_manager,
			   data->merge_id,
			   tocQ ? TOC_PATH : DOC_PATH,
			   label, name,
			   GTK_UI_MANAGER_MENUITEM,
			   FALSE);
    g_object_unref (action);
}

static gboolean
bookmarks_unregister (YelpWindow *window,
		      GdkEvent   *event,
		      gpointer    user_data)
{
    GSList *cur;

    for (cur = windows; cur != NULL; cur = cur->next) {
	YelpWindowData *data = (YelpWindowData *) cur->data;;
	if (data->window == window) {
	    g_object_unref (data->toc_actions);
	    g_object_unref (data->doc_actions);
	    g_free (data);
	    windows = g_slist_delete_link (windows, cur);
	    break;
	}
    }

    return FALSE;
}

static void
action_activated_cb (GtkActionGroup *group,
		     GtkAction      *action,
		     YelpWindowData *data)
{
    gchar *uri;

    g_object_get (action, "name", &uri, NULL);
    yelp_window_load (data->window, uri);

    g_free (uri);
}
