/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@codefactory.se>
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
 * Author: Mikael Hallendal <micke@codefactory.se>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnome/gnome-i18n.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkframe.h>

#include <string.h>

#include "yelp-html.h"
#include "yelp-marshal.h"
#include "yelp-reader.h"
#include "yelp-scrollkeeper.h"
#include "yelp-util.h"
#include "yelp-view-content.h"

#define d(x)

static void content_init                      (YelpViewContent      *html);
static void content_class_init                (YelpViewContentClass *klass);
static void content_tree_selection_changed_cb (GtkTreeSelection     *selection,
				   	       YelpViewContent      *content);
static void content_html_uri_selected_cb      (YelpHtml             *html,
					       YelpURI              *uri,
					       gboolean              handled,
					       YelpViewContent      *view);
static void content_html_title_changed_cb     (YelpHtml             *html,
					       const gchar          *title,
					       YelpViewContent      *view);
#if 0
static void content_reader_start_cb           (YelpReader           *reader,
					       YelpViewContent      *view);
#endif
static void content_reader_data_cb            (YelpReader           *reader,
					       const gchar          *data,
					       gint                  len,
					       YelpViewContent      *view);
static void content_reader_finished_cb        (YelpReader           *reader,
					       YelpViewContent      *view);
static void content_reader_error_cb           (YelpReader           *reader,
					       GError               *error,
					       YelpViewContent      *view);



enum {
	URI_SELECTED,
	TITLE_CHANGED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

struct _YelpViewContentPriv {
	/* Content tree */
	GtkWidget    *tree_sw;
	GtkWidget    *content_tree;
	GtkTreeStore *tree_store;
	GNode        *doc_tree;

	YelpReader   *reader;

	/* Html view */
	YelpHtml     *html_view;
	GtkWidget    *html_widget;

	YelpURI      *current_uri;
	
	gboolean      loading;
};

GType
yelp_view_content_get_type (void)
{
        static GType view_type = 0;

        if (!view_type)
        {
                static const GTypeInfo view_info =
                        {
                                sizeof (YelpViewContentClass),
                                NULL,
                                NULL,
                                (GClassInitFunc) content_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpViewContent),
                                0,
                                (GInstanceInitFunc) content_init,
                        };
                
                view_type = g_type_register_static (GTK_TYPE_HPANED,
                                                    "YelpViewContent", 
                                                    &view_info, 0);
        }
        
        return view_type;
}


static void
content_init (YelpViewContent *view)
{
	YelpViewContentPriv *priv;
	
	priv = g_new0 (YelpViewContentPriv, 1);
	view->priv = priv;

	priv->content_tree = gtk_tree_view_new ();
	priv->tree_store   = gtk_tree_store_new (2,
						 G_TYPE_STRING, 
						 G_TYPE_POINTER);
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->content_tree),
				 GTK_TREE_MODEL (priv->tree_store));
	
	priv->html_view       = yelp_html_new ();
	priv->html_widget     = yelp_html_get_widget (priv->html_view);
/* 	priv->current_docpath = g_strdup (""); */
	priv->current_uri     = NULL;
	priv->loading         = FALSE;
	
	g_signal_connect (priv->html_view, "uri_selected",
			  G_CALLBACK (content_html_uri_selected_cb), 
			  view);
	g_signal_connect (priv->html_view, "title_changed",
			  G_CALLBACK (content_html_title_changed_cb),
			  view);

	priv->reader      = yelp_reader_new ();
	
#if 0
	g_signal_connect (G_OBJECT (priv->reader), "start",
			  G_CALLBACK (content_reader_start_cb),
			  view);
#endif
	g_signal_connect (G_OBJECT (priv->reader), "data",
			  G_CALLBACK (content_reader_data_cb),
			  view);
	g_signal_connect (G_OBJECT (priv->reader), "finished",
			  G_CALLBACK (content_reader_finished_cb),
			  view);
	g_signal_connect (G_OBJECT (priv->reader), "error",
			  G_CALLBACK (content_reader_error_cb),
			  view);
}

static void
content_class_init (YelpViewContentClass *klass)
{
	signals[URI_SELECTED] = 
		g_signal_new ("uri_selected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpViewContentClass,
					       uri_selected),
			      NULL, NULL,
			      yelp_marshal_VOID__POINTER_BOOLEAN,
			      G_TYPE_NONE,
			      2, G_TYPE_POINTER, G_TYPE_BOOLEAN);

	signals[TITLE_CHANGED] = 
		g_signal_new ("title_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpViewContentClass,
					       title_changed),
			      NULL, NULL,
			      yelp_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);
}

static void
content_html_uri_selected_cb (YelpHtml        *html,
			  YelpURI         *uri,
			  gboolean         handled,
			  YelpViewContent *view)
{
	/* Just propagate the signal to the view */
	d(g_print ("***** URI Clicked: %s [%d]\n", 
		   yelp_uri_to_string (uri),
		   handled));
	
	g_signal_emit (view, signals[URI_SELECTED], 0, uri, handled);
}

static void
content_html_title_changed_cb (YelpHtml        *html,
			       const gchar     *title,
			       YelpViewContent *view)
{
	g_print ("Title changed to: %s\n", title);

	g_signal_emit (view, signals[TITLE_CHANGED], 0, title);
}

static void
content_tree_selection_changed_cb (GtkTreeSelection *selection, 
			       YelpViewContent  *content)
{
	YelpViewContentPriv *priv;
 	GtkTreeIter          iter;
	YelpSection         *section;
	GtkTreeModel        *model;
	
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_return_if_fail (YELP_IS_VIEW_CONTENT (content));

	priv = content->priv;

	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->content_tree));
		
		gtk_tree_model_get (model, &iter,
				    1, &section,
				    -1);

 		g_signal_emit (content, signals[URI_SELECTED], 0,
 			       section->uri, FALSE);
	}
}


#if 0
static void
content_reader_start_cb (YelpReader *reader, YelpViewContent *view)
{
	GdkCursor           *cursor;
	YelpViewContentPriv *priv;
	gchar               *loading = _("Loading...");

	g_return_if_fail (YELP_IS_READER (reader));
	g_return_if_fail (YELP_IS_VIEW_CONTENT (view));

	priv = view->priv;

	d(g_print ("Start_cb\n"));
	
	cursor = gdk_cursor_new (GDK_WATCH);
	
	gdk_window_set_cursor (priv->html_widget->window, cursor);
	gdk_cursor_unref (cursor);
	
	yelp_html_clear (priv->html_view);
	
	priv->loading = TRUE;
	
	yelp_html_printf (priv->html_view, 
			  "<html><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><body bgcolor=\"white\"><center>%s</center></body></html>", 
			  loading);
 	yelp_html_close (priv->html_view);
}
#endif

static void
content_reader_data_cb (YelpReader      *reader,
			const gchar     *data,
			gint             len,
			YelpViewContent *view)
{
	YelpViewContentPriv *priv;
	
	g_return_if_fail (YELP_IS_READER (reader));
	g_return_if_fail (YELP_IS_VIEW_CONTENT (view));
	
	priv = view->priv;

	if (priv->loading) {
		yelp_html_clear (priv->html_view);
		priv->loading = FALSE;
	}

	if (len == -1) {
		len = strlen (data);
	}

	if (len <= 0) {
		return;
	}

	yelp_html_write (priv->html_view, data, len);
}

static void
content_reader_finished_cb (YelpReader *reader, YelpViewContent *view)
{
	YelpViewContentPriv *priv;
	
	g_return_if_fail (YELP_IS_READER (reader));
	g_return_if_fail (YELP_IS_VIEW_CONTENT (view));

	priv = view->priv;

	yelp_html_close (priv->html_view);

	gdk_window_set_cursor (priv->html_widget->window, NULL);
	gtk_widget_grab_focus (priv->html_widget);
}

static void
content_reader_error_cb (YelpReader      *reader,
			 GError          *error, 
			 YelpViewContent *view)
{
	YelpViewContentPriv *priv;
	
	g_return_if_fail (YELP_IS_READER (reader));
	g_return_if_fail (YELP_IS_VIEW_CONTENT (view));

	priv = view->priv;

	/* Popup window */
	
	g_warning ("%s\n", error->message);
}

GtkWidget *
yelp_view_content_new (GNode *doc_tree)
{
	YelpViewContent     *view;
	YelpViewContentPriv *priv;
	GtkTreeSelection    *selection;
        GtkWidget           *html_sw;
	GtkWidget           *frame;
	
	view = g_object_new (YELP_TYPE_VIEW_CONTENT, NULL);
	priv = view->priv;

	priv->doc_tree = doc_tree;
	
	/* Setup the content tree */
        priv->tree_sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->tree_sw),
                                        GTK_POLICY_AUTOMATIC, 
                                        GTK_POLICY_AUTOMATIC);

	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (priv->content_tree), -1,
		_("Section"), gtk_cell_renderer_text_new (),
		"text", 0, 
		NULL);

	selection = gtk_tree_view_get_selection (
		GTK_TREE_VIEW (priv->content_tree));

	g_signal_connect (selection, "changed",
			  G_CALLBACK (content_tree_selection_changed_cb), 
			  view);

        gtk_container_add (GTK_CONTAINER (priv->tree_sw), priv->content_tree);

        /* Setup the Html view */
 	html_sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (html_sw),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	
	gtk_container_add (GTK_CONTAINER (frame), html_sw);
        gtk_container_add (GTK_CONTAINER (html_sw), priv->html_widget);


	/* Add the tree and html view to the paned */
	gtk_paned_add1 (GTK_PANED (view), priv->tree_sw);
        gtk_paned_add2 (GTK_PANED (view), frame);
        gtk_paned_set_position (GTK_PANED (view), 175);

	return GTK_WIDGET (view);
}

static void
yelp_view_content_insert_tree (YelpViewContent *content,
			       GtkTreeIter     *parent,
			       GNode           *node)
{
	GtkTreeIter  iter;
	YelpSection *section;
	GNode       *child;
	
	gtk_tree_store_append (content->priv->tree_store,
			       &iter, parent);

	section = node->data;
	gtk_tree_store_set (content->priv->tree_store,
			    &iter,
			    0, section->name,
			    1, section,
			    -1);

	child = node->children;

	while (child) {
		yelp_view_content_insert_tree (content, &iter, child);
		
		child = child->next;
	}
}

static void 
yelp_view_content_set_tree (YelpViewContent *content,
			    GNode           *node)
{
	GNode *child;

	g_return_if_fail (YELP_IS_VIEW_CONTENT (content));
	g_return_if_fail (node != NULL);

	gtk_tree_store_clear (content->priv->tree_store);

	child = node->children;

	while (child) {
		yelp_view_content_insert_tree (content, NULL, child);
		child = child->next;
	}
}

void
yelp_view_content_show_uri (YelpViewContent  *content,
			    YelpURI          *uri,
			    GError          **error)
{
	YelpViewContentPriv *priv;
	GNode               *node;
	GdkCursor           *cursor;
	
	g_return_if_fail (YELP_IS_VIEW_CONTENT (content));
	g_return_if_fail (uri != NULL);
	
	priv = content->priv;

	if (yelp_uri_get_type (uri) == YELP_URI_TYPE_DOCBOOK_XML) {
		/* ghelp uri-scheme /usr/share/gnome/help... */
/* 		const gchar *docpath; */

/*  		docpath = yelp_uri_get_path (uri); */

		if (!priv->current_uri || 
		    !yelp_uri_equal_path (uri, priv->current_uri)) {
			/* Try to find it in the scrollkeeper database,
			   doesn't have to exist here */
			node = yelp_scrollkeeper_get_toc_tree (yelp_uri_get_path (uri));
			
			if (node) {
				gtk_widget_show (priv->tree_sw);
				yelp_view_content_set_tree (content, node);
				
			} else {
				gtk_widget_hide (priv->tree_sw);
			}
		}
	} else {
		gtk_widget_hide (priv->tree_sw);
		
	}
	
	priv->loading = TRUE;
	
	cursor = gdk_cursor_new (GDK_WATCH);
		
	gdk_window_set_cursor (priv->html_widget->window, cursor);
	gdk_cursor_unref (cursor);

	if (priv->current_uri) {
		yelp_uri_unref (priv->current_uri);
	}
	
	priv->current_uri = yelp_uri_ref (uri);
	
	yelp_html_set_base_uri (priv->html_view, uri);
		
	if (!yelp_reader_start (priv->reader, uri)) {
		gchar     *loading = _("Loading...");

		yelp_html_clear (priv->html_view);

		yelp_html_printf (priv->html_view, 
				  "<html><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><title>..</title><body bgcolor=\"white\"><center>%s</center></body></html>", 
				  loading);
		yelp_html_close (priv->html_view);
	}
}
