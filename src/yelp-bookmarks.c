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

#include <string.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreemodel.h>
#include <gdk/gdkkeysyms.h>
#include <glade/glade.h>
#include <libxml/xmlwriter.h>
#include <libxml/xmlreader.h>

#ifdef YELP_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define COL_NAME  0
#define COL_LABEL 1
#define COL_SEP   2
#define TOC_PATH "ui/menubar/BookmarksMenu/BookmarksTOC"
#define DOC_PATH "ui/menubar/BookmarksMenu/BookmarksDOC"

static GSList *windows;
static GtkListStore *actions_store;
static gboolean dup_flag;

static gboolean have_tocs = FALSE;
static gboolean have_docs = FALSE;
static GtkTreeIter *seperator_iter = NULL;

typedef struct _YelpWindowData YelpWindowData;
struct _YelpWindowData {
    YelpWindow *window;
    guint merge_id;
    GtkActionGroup *toc_actions;
    GtkActionGroup *doc_actions;
};

static void      window_add_bookmark        (YelpWindowData *window,
					     gchar          *name,
					     const gchar    *label);
static void      window_remove_bookmark_menu(YelpWindowData *data);
static void      window_remove_action       (YelpWindowData *data,
					     gchar          *name);
static gboolean  bookmarks_unregister       (YelpWindow     *window,
					     GdkEvent       *event,
					     gpointer        user_data);
static void      action_activated_cb        (GtkActionGroup *group,
					     GtkAction      *action,
					     YelpWindowData *data);
static gboolean  row_is_separator           (GtkTreeModel   *model,
					     GtkTreeIter    *iter,
					     gpointer        user_data);
void             bookmarks_rebuild_menus    (void);
void             bookmarks_edit_name_cb     (GtkTreeView       *treeview,
					     GtkTreePath       *path,
					     GtkTreeViewColumn *col,
					     gpointer           user_data);
void             bookmarks_edit_response_cb (GtkDialog         *dialog,
					     gint               response,
					     gpointer           user_data);
void             bookmarks_key_event_cb     (GtkWidget         *widget,
					     GdkEventKey       *event,
					     gpointer           user_data);

static gboolean  bookmarks_read             (void);

static void      bookmarks_add_seperator    (void);

static gboolean  bookmarks_dup_finder       (GtkTreeModel   *model,
					     GtkTreePath    *path,
					     GtkTreeIter    *iter,
					     gpointer        data);

void
yelp_bookmarks_init (void)
{
    GtkTreeIter  iter;
    gboolean read;
    
    windows = NULL;
    actions_store = gtk_list_store_new (3,
					G_TYPE_STRING,
					G_TYPE_STRING,
					G_TYPE_BOOLEAN);

    read = bookmarks_read ();
}

void
yelp_bookmarks_register (YelpWindow *window)
{
    GtkUIManager   *ui_manager;
    YelpWindowData *data;
    GtkTreeIter     iter;
    gboolean        valid;

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

    valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (actions_store), &iter);
    while (valid) {
	gchar *label, *name;
	gboolean sep;

	gtk_tree_model_get (GTK_TREE_MODEL (actions_store), &iter,
			    COL_NAME,  &name,
			    COL_LABEL, &label,
			    COL_SEP,   &sep,
			    -1);
	if (!sep)
	    window_add_bookmark (data, name, label);

	g_free (name);
	g_free (label);

	valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (actions_store), &iter);
    }

    windows = g_slist_append (windows, data);
}

static void
bookmarks_add_seperator (void)
{
    GtkTreeIter iter;

    if (seperator_iter == NULL) {
	if (have_tocs)
	    gtk_list_store_append (actions_store, &iter);
	else
	    gtk_list_store_prepend (actions_store, &iter);
	gtk_list_store_set (actions_store, &iter,
			    COL_NAME,  NULL,
			    COL_LABEL, NULL,
			    COL_SEP,   TRUE,
			    -1);
	seperator_iter = gtk_tree_iter_copy (&iter);
    }
}

static gboolean 
bookmarks_dup_finder (GtkTreeModel *model, GtkTreePath *path,
		      GtkTreeIter *iter, gpointer data)
{
    gchar *uri = data;
    gchar *current_uri;

    gtk_tree_model_get (model, iter,
			COL_NAME, &current_uri, -1);
    if (current_uri && g_str_equal (current_uri, uri)) {
	dup_flag = TRUE;
	return TRUE;
    }
    return FALSE;
}


void yelp_bookmarks_add (gchar *uri, const gchar *title, gboolean save)
{
    GtkTreeIter  iter;
    GSList      *cur;
    gboolean     tocQ;
    
    dup_flag = FALSE;
    gtk_tree_model_foreach (GTK_TREE_MODEL (actions_store),
			    bookmarks_dup_finder, uri);
    if (!dup_flag) {
	tocQ = g_str_has_prefix (uri, "x-yelp-toc:");

	if (tocQ) {
	    if (seperator_iter == NULL) {
		if (have_docs) {
		    bookmarks_add_seperator ();
		    gtk_list_store_prepend (actions_store, &iter);
		} else {
		    gtk_list_store_append (actions_store, &iter);
		}
	    } else {
		gtk_list_store_insert_before (actions_store,
					      &iter, seperator_iter);
	    }
	    have_tocs = TRUE;
	} else {
	    if (seperator_iter == NULL && have_tocs)
		bookmarks_add_seperator ();
	    gtk_list_store_append (actions_store, &iter);
	    have_docs = TRUE;
	}

	gtk_list_store_set (actions_store, &iter,
			    COL_NAME,  uri,
			    COL_LABEL, title,
			    COL_SEP,   FALSE,
			    -1);
	
	if (save) {
	    for (cur = windows; cur != NULL; cur = cur->next) {
		window_add_bookmark ((YelpWindowData *) cur->data, uri, title);
	    }
	    yelp_bookmarks_write ();
	}
    }
}

void
bookmarks_edit_response_cb (GtkDialog *dialog, gint response, 
			    gpointer user_data)
{
    gtk_widget_hide (GTK_WIDGET (dialog));
    if ( response == GTK_RESPONSE_ACCEPT){
	GList *children = 
	    gtk_container_get_children (GTK_CONTAINER (dialog->vbox));
	GtkEntry *entry = children->data;
	gchar *new_title = (gchar *) gtk_entry_get_text (entry);
	GtkTreePath *path = (GtkTreePath *) user_data;
	GtkTreeIter it;
	
	gtk_tree_model_get_iter (GTK_TREE_MODEL (actions_store), &it, path);
	gtk_list_store_set (actions_store, &it,
			    COL_LABEL, new_title,
			    -1);
	bookmarks_rebuild_menus ();
    }	
}

void
window_remove_bookmark_menu (YelpWindowData *data)
{
    GtkUIManager *ui_manager;

    ui_manager = yelp_window_get_ui_manager (data->window);

    gtk_ui_manager_remove_ui (ui_manager, data->merge_id);

    data->merge_id = gtk_ui_manager_new_merge_id (ui_manager);

    gtk_ui_manager_ensure_update (ui_manager);
}

void
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

void
bookmarks_rebuild_menus (void)
{
    GSList      *cur;
    gboolean ret;
    GtkTreeIter iter;

    for (cur = windows; cur != NULL; cur = cur->next) {
	window_remove_bookmark_menu ((YelpWindowData *) cur->data);
    }

    ret = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (actions_store), 
					 &iter);
    while (ret) {
	gchar *name, *label;
	gboolean sep;

	gtk_tree_model_get (GTK_TREE_MODEL (actions_store), &iter,
			    COL_NAME,  &name,
			    COL_LABEL, &label,
			    COL_SEP,   &sep,
			    -1);
	if (!sep) {
	    for (cur = windows; cur != NULL; cur = cur->next) {
		window_remove_action (cur->data, name);
		window_add_bookmark (cur->data, name, label);
	    }
	}
	g_free (name);
	g_free (label);

	ret = gtk_tree_model_iter_next (GTK_TREE_MODEL (actions_store), 
					&iter);

    }
    yelp_bookmarks_write ();
}

void bookmarks_key_event_cb (GtkWidget *widget, GdkEventKey *event,
			     gpointer user_data)
{
    GtkTreeView *view = GTK_TREE_VIEW (widget);
    GtkTreePath *path;
    GtkTreeSelection *sel;
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    model = gtk_tree_view_get_model (view);
    sel = gtk_tree_view_get_selection (view);
    
    gtk_tree_selection_get_selected (sel, &model, &iter);
    path = gtk_tree_model_get_path (model, &iter);

    switch (event->keyval) {
    case GDK_BackSpace:
    case GDK_Delete:
	gtk_list_store_remove (actions_store, &iter);
	bookmarks_rebuild_menus ();
	break;
    case GDK_Return:
    case GDK_KP_Enter:
	bookmarks_edit_name_cb (view, path, NULL, NULL);
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


void
bookmarks_edit_name_cb (GtkTreeView *treeview, GtkTreePath *path,
			GtkTreeViewColumn *col,
			gpointer user_data)
{
    GtkTreeIter iter;
    gchar *name;
    gchar *title;
    GtkDialog *edit;
    GtkWidget *entry;
    
    gtk_tree_model_get_iter (GTK_TREE_MODEL (actions_store), &iter, path);

    gtk_tree_model_get (GTK_TREE_MODEL (actions_store), &iter,
			COL_NAME, &name,
			COL_LABEL, &title,
			-1);

    edit = gtk_dialog_new_with_buttons ("Edit bookmark name",
					NULL,
					GTK_DIALOG_MODAL,
					GTK_STOCK_OK,
					GTK_RESPONSE_ACCEPT,
					GTK_STOCK_CANCEL,
					GTK_RESPONSE_REJECT,
					NULL);
    entry = gtk_entry_new ();
    gtk_entry_set_text (GTK_ENTRY (entry), title);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG(edit)->vbox),
                      entry);
    g_signal_connect (edit, "response", 
		      G_CALLBACK (bookmarks_edit_response_cb), 
		      gtk_tree_path_copy(path));
    gtk_widget_show_all (GTK_WIDGET (edit));

    return;
}

void
yelp_bookmarks_edit (void)
{
    GladeXML    *glade;
    GtkWidget   *dialog;
    GtkTreeView *view;
    GtkTreeSelection *select;

    glade = glade_xml_new (DATADIR "/yelp/ui/yelp.glade",
			   "bookmarks_dialog",
			   NULL);
    if (!glade) {
	g_warning ("Could not find necessary glade file "
		   DATADIR "/yelp/ui/yelp.glade");
	return;
    }

    dialog = glade_xml_get_widget (glade, "bookmarks_dialog");
    view = GTK_TREE_VIEW (glade_xml_get_widget (glade, "bookmarks_view"));

    g_signal_connect (G_OBJECT (dialog), "response",
		      G_CALLBACK (gtk_widget_destroy), NULL);

    gtk_tree_view_insert_column_with_attributes
	(view, -1,
	 NULL, gtk_cell_renderer_text_new (),
	 "text", COL_LABEL,
	 NULL);
    gtk_tree_view_set_row_separator_func (view, row_is_separator, NULL, NULL);

    gtk_tree_view_set_model (view, GTK_TREE_MODEL (actions_store));

    select = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

    gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

    g_signal_connect (G_OBJECT (view), "row-activated",
		      G_CALLBACK (bookmarks_edit_name_cb),
		      NULL); 
    g_signal_connect (G_OBJECT (view), "key-press-event",
		      G_CALLBACK (bookmarks_key_event_cb),
		      NULL);

    g_object_unref (glade);

    gtk_window_present (GTK_WINDOW (dialog));
}

void
yelp_bookmarks_write (void)
{
    xmlTextWriterPtr file;
    gint rc;
    GtkTreeIter iter;
    gboolean res;
    gchar *filename;

    filename = g_build_filename (g_get_home_dir (), ".gnome2", 
				 "yelp-bookmarks", NULL);

    file = xmlNewTextWriterFilename (filename, 
				     0);
    if(!file) {
	g_warning ("Could not create bookmark file %s", filename);
	return;
    }

    rc = xmlTextWriterStartDocument (file, NULL, NULL, NULL);
    rc = xmlTextWriterStartElement (file, BAD_CAST "Body");
    rc = xmlTextWriterWriteAttribute(file, BAD_CAST "version",
                                     BAD_CAST "1.0");
    rc = xmlTextWriterWriteAttribute(file, BAD_CAST "xml:lang",
                                     BAD_CAST "en_GB");

    rc = xmlTextWriterWriteComment (file, BAD_CAST "Yelp Bookmark file - "
    "Do not edit directly");

    rc = xmlTextWriterStartElement (file, BAD_CAST "Bookmarks");

    res = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (actions_store),
						  &iter);

    while (res) 
	{
	    gchar *name;
	    gchar *label;
	    gboolean sep;
	    gtk_tree_model_get (GTK_TREE_MODEL (actions_store), &iter,
				COL_NAME, &name, 
				COL_LABEL, &label,
				COL_SEP, &sep, -1);
	    
	    xmlTextWriterStartElement (file, BAD_CAST "Entry");
 
	    if (!sep) {
		xmlTextWriterWriteAttribute (file, "name", name);
		xmlTextWriterWriteAttribute (file, "label", label);
	    } else {
		xmlTextWriterWriteAttribute (file, "seperator", NULL);
	    }

	    xmlTextWriterEndElement (file);

	    res = gtk_tree_model_iter_next (GTK_TREE_MODEL (actions_store), &iter);
	    g_free (name);
	    g_free (label);
	}

    rc = xmlTextWriterEndDocument(file);
    xmlFreeTextWriter(file);
    g_free (filename);
    return;
}

static gboolean
bookmarks_read (void)
{
    xmlTextReaderPtr file;
    gint ret;
    gchar *filename;

    filename = g_build_filename (g_get_home_dir (), ".gnome2", 
				 "yelp-bookmarks", NULL);

    file = xmlReaderForFile(filename, NULL, 0);

    if (file == NULL)
	return FALSE;

    ret = xmlTextReaderRead (file);

    while (ret) {
	
	if (xmlTextReaderHasAttributes (file)) {
	    gchar *name;
	    gchar *title;
	    gchar *sep;
	    
	    name = xmlTextReaderGetAttribute (file, "name");
	    title = xmlTextReaderGetAttribute (file, "label");
	    sep = xmlTextReaderGetAttribute (file, "seperator");
	    
	    if (name && title) {
		yelp_bookmarks_add (name, title, FALSE);
	    }
	    g_free (name);
	    g_free (title);
	    g_free (sep);
	}
	ret = xmlTextReaderRead (file);	
    }
    
    xmlTextReaderClose (file);
    return TRUE;
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

static gboolean
row_is_separator (GtkTreeModel *model,
		  GtkTreeIter  *iter,
		  gpointer      user_data)
{
    gboolean sep;

    gtk_tree_model_get (model, iter, COL_SEP, &sep, -1);

    return sep;
}
