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
#include <glade/glade.h>

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

typedef struct _YelpWindowData YelpWindowData;
struct _YelpWindowData {
    YelpWindow *window;
    GtkActionGroup *toc_actions;
    GtkActionGroup *doc_actions;
};

static void      window_add_bookmark        (YelpWindowData *window,
					     gchar          *name,
					     gchar          *label);
static gboolean  bookmarks_unregister       (YelpWindow     *window,
					     GdkEvent       *event,
					     gpointer        user_data);
static void      action_activated_cb        (GtkActionGroup *group,
					     GtkAction      *action,
					     YelpWindowData *data);
static gboolean  row_is_separator           (GtkTreeModel   *model,
					     GtkTreeIter    *iter,
					     gpointer        user_data);

void
yelp_bookmarks_init (void)
{
    GtkTreeIter  iter;

    windows = NULL;
    actions_store = gtk_list_store_new (3,
					G_TYPE_STRING,
					G_TYPE_STRING,
					G_TYPE_BOOLEAN);

    gtk_list_store_append (actions_store, &iter);
    gtk_list_store_set (actions_store, &iter,
			COL_NAME,  NULL,
			COL_LABEL, NULL,
			COL_SEP,   TRUE,
			-1);
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


void yelp_bookmarks_add (gchar *uri, gchar *title)
{
    GtkTreeIter  iter;
    GSList      *cur;

    g_print ("I know bookmarks aren't being saved.  Don't file a bug.\n");

    gtk_list_store_append (actions_store, &iter);
    gtk_list_store_set (actions_store, &iter,
			COL_NAME,  uri,
			COL_LABEL, title,
			COL_SEP,   FALSE,
			-1);

    for (cur = windows; cur != NULL; cur = cur->next)
	window_add_bookmark ((YelpWindowData *) cur->data, uri, title);
}

void
yelp_bookmarks_edit (void)
{
    GladeXML    *glade;
    GtkWidget   *dialog;
    GtkTreeView *view;

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

    gtk_tree_view_set_model (view, actions_store);

    g_object_unref (glade);

    gtk_window_present (GTK_WINDOW (dialog));
}

void
yelp_bookmarks_write (void)
{
    return;
}

static void
window_add_bookmark (YelpWindowData *data,
		     gchar          *name,
		     gchar          *label)
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
			   gtk_ui_manager_new_merge_id (ui_manager),
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
