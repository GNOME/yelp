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
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/xmlwriter.h>

#ifdef YELP_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define COL_NAME   0
#define COL_LABEL  1
#define COL_EDIT   2
#define COL_HEADER 3
#define TOC_PATH "ui/menubar/BookmarksMenu/BookmarksTOC"
#define DOC_PATH "ui/menubar/BookmarksMenu/BookmarksDOC"

static GSList *windows;
static GtkTreeStore *actions_store;
static gchar *dup_title;

static GtkWidget *bookmarks_dialog = NULL;

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

static gboolean  bookmarks_read             (void);

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

void
yelp_bookmarks_init (void)
{
    gboolean read;

    windows = NULL;
    actions_store = gtk_tree_store_new (4,
					G_TYPE_STRING,
					G_TYPE_STRING,
					G_TYPE_BOOLEAN,
					G_TYPE_BOOLEAN);

    read = bookmarks_read ();
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
    GladeXML    *glade;
    GtkWidget   *dialog;
    GtkEntry    *entry;
    gchar       *title;
    gchar       *dup_uri;

    title = gtk_window_get_title (GTK_WINDOW (window));
    dup_uri = g_strdup (uri);
    
    if (dup_title)
	g_free (dup_title);
    dup_title = NULL;
    gtk_tree_model_foreach (GTK_TREE_MODEL (actions_store),
			    bookmarks_dup_finder, dup_uri);
    if (dup_title) {
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new_with_markup
	    (GTK_WINDOW (window),
	     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	     GTK_MESSAGE_INFO,
	     GTK_BUTTONS_OK,
	     _("A bookmark titled <b>%s</b> already exists for this page."),
	     dup_title);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	g_free (dup_title);
	dup_title = NULL;
	return;
    }

    glade = glade_xml_new (DATADIR "/yelp/ui/yelp.glade",
			   "add_bookmark_dialog",
			   NULL);
    if (!glade) {
	g_warning ("Could not find necessary glade file "
		   DATADIR "/yelp/ui/yelp.glade");
	return;
    }

    dialog = glade_xml_get_widget (glade, "add_bookmark_dialog");
    entry = GTK_ENTRY (glade_xml_get_widget (glade, "bookmark_title_entry"));
    gtk_entry_set_text (entry, title);
    gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);

    g_object_set_data (G_OBJECT (dialog), "title_entry", entry);

    g_signal_connect (G_OBJECT (dialog),
		      "response",
		      G_CALLBACK (bookmark_add_response_cb),
		      dup_uri);

    g_object_unref (glade);
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
	    window_add_bookmark ((YelpWindowData *) cur->data, uri, title);
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
    
    gtk_tree_selection_get_selected (sel, &model, &iter);
    path = gtk_tree_model_get_path (model, &iter);

    switch (event->keyval) {
    case GDK_BackSpace:
    case GDK_Delete:
	gtk_tree_store_remove (actions_store, &iter);
	bookmarks_rebuild_menus ();
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
    gtk_tree_model_get (GTK_TREE_MODEL (actions_store), &iter,
			COL_NAME, &name,
			COL_LABEL, &title, -1);

    cur = windows;
    data = cur->data;

    g_signal_emit_by_name (data->window, "new_window_requested", name, NULL);

}


static void
bookmarks_cell_edited_cb (GtkCellRendererText *cell, const gchar *path_string,
			  const gchar *new_title, gpointer user_data)
{
    GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
    GtkTreeView *view = GTK_TREE_VIEW (user_data);
    GtkTreeIter it;

    gtk_tree_model_get_iter (GTK_TREE_MODEL (actions_store), &it, path);

    gtk_tree_store_set (actions_store, &it,
			COL_LABEL, new_title, -1);
    bookmarks_rebuild_menus ();

}

void
yelp_bookmarks_edit (void)
{
    GladeXML    *glade;
    GtkTreeView *view;
    GtkTreeSelection *select;
    GtkCellRenderer *renderer;
    GtkWidget *button;

    if (!bookmarks_dialog) {
	glade = glade_xml_new (DATADIR "/yelp/ui/yelp.glade",
			       "bookmarks_dialog",
			       NULL);
	if (!glade) {
	    g_warning ("Could not find necessary glade file "
		       DATADIR "/yelp/ui/yelp.glade");
	    return;
	}

	bookmarks_dialog = glade_xml_get_widget (glade, "bookmarks_dialog");
	view = GTK_TREE_VIEW (glade_xml_get_widget (glade, "bookmarks_view"));

	g_signal_connect (G_OBJECT (bookmarks_dialog), "response",
			  G_CALLBACK (gtk_widget_hide), NULL);
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

	button = glade_xml_get_widget (glade, "open_button");
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (bookmarks_open_button_cb),
			  view);
	button = glade_xml_get_widget (glade, "rename_button");
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (bookmarks_rename_button_cb),
			  view);
	button = glade_xml_get_widget (glade, "remove_button");
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (bookmarks_remove_button_cb),
			  view);

	g_object_unref (glade);
    }

    gtk_window_present (GTK_WINDOW (bookmarks_dialog));
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
    gtk_tree_selection_get_selected (select, NULL, &iter);

    path = gtk_tree_model_get_path (model, &iter);

    bookmarks_open_cb (view, path, NULL, NULL);

    gtk_tree_path_free (path);
}

static void
bookmarks_rename_button_cb (GtkWidget *widget, GtkTreeView *view)
{
}

static void
bookmarks_remove_button_cb (GtkWidget *widget, GtkTreeView *view)
{
    GtkTreeIter iter;
    GtkTreeSelection *select;

    select = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
    gtk_tree_selection_get_selected (select, NULL, &iter);

    gtk_tree_store_remove (actions_store, &iter);
    bookmarks_rebuild_menus ();
}

void
yelp_bookmarks_write (void)
{
    xmlTextWriterPtr file;
    gint rc;
    GtkTreeIter top_iter, sub_iter;
    gboolean top_valid, sub_valid;
    gchar *filename;

    filename = g_build_filename (g_get_home_dir (), ".gnome2", 
				 "yelp-bookmarks.xbel", NULL);

    file = xmlNewTextWriterFilename (filename, 
				     0);
    if(!file) {
	g_warning ("Could not create bookmark file %s", filename);
	return;
    }

    rc = xmlTextWriterStartDocument (file, NULL, NULL, NULL);
    rc = xmlTextWriterStartElement (file, BAD_CAST "xbel");
    rc = xmlTextWriterWriteAttribute(file, BAD_CAST "version",
                                     BAD_CAST "1.0");

    rc = xmlTextWriterStartElement (file, BAD_CAST "info");
    rc = xmlTextWriterStartElement (file, BAD_CAST "metadata");
    rc = xmlTextWriterWriteAttribute(file, BAD_CAST "owner",
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
 		xmlTextWriterWriteAttribute (file, "href", name);

		rc = xmlTextWriterWriteElement (file,
						BAD_CAST "title",
						label);

		rc = xmlTextWriterStartElement (file, BAD_CAST "info");
		rc = xmlTextWriterStartElement (file, BAD_CAST "metadata");
		rc = xmlTextWriterWriteAttribute
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

    rc = xmlTextWriterEndDocument(file);
    xmlFreeTextWriter(file);
    g_free (filename);
    return;
}

static gboolean
bookmarks_read (void)
{
    xmlParserCtxtPtr   parser;
    xmlXPathContextPtr xpath;
    xmlXPathObjectPtr  obj;
    xmlDocPtr          doc;

    gboolean  ret = TRUE;
    gchar    *filename;
    gint      i;

    filename = g_build_filename (g_get_home_dir (), ".gnome2", 
				 "yelp-bookmarks.xbel", NULL);

    parser = xmlNewParserCtxt ();
    doc = xmlCtxtReadFile (parser, filename, NULL,
			   XML_PARSE_NOBLANKS | XML_PARSE_NOCDATA  |
			   XML_PARSE_NOENT    | XML_PARSE_NOERROR  |
			   XML_PARSE_NONET    );

    if (doc == NULL) {
	return FALSE;
	goto done;
    }

    xpath = xmlXPathNewContext (doc);
    obj = xmlXPathEvalExpression ("/xbel/bookmark", xpath);
    for (i = 0; i < obj->nodesetval->nodeNr; i++) {
	xmlNodePtr node = obj->nodesetval->nodeTab[i];
	gchar *uri;
	uri = xmlGetProp (node, "href");
	if (uri) {
	    xmlXPathObjectPtr title_obj;
	    xpath->node = node;
	    title_obj = xmlXPathEvalExpression ("string(title[1])", xpath);
	    xpath->node = NULL;

	    if (title_obj->stringval)
		bookmarks_add_bookmark (uri, title_obj->stringval, FALSE);

	    xmlXPathFreeObject (title_obj);
	    xmlFree (uri);
	}
    }
    
 done:
    g_free (filename);
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
