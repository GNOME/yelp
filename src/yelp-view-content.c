/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Mikael Hallendal <micke@codefactory.se>
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
#include "yelp-scrollkeeper.h"
#include "yelp-util.h"
#include "yelp-view-content.h"

#define d(x)

static void yvc_init                      (YelpViewContent         *html);
static void yvc_class_init                (YelpViewContentClass    *klass);
static void yvc_tree_selection_changed_cb (GtkTreeSelection        *selection,
					   YelpViewContent         *content);
static void yvc_html_uri_selected_cb      (YelpHtml                *html,
					   YelpURI                 *uri,
					   gboolean                 handled,
					   YelpViewContent         *view);


enum {
	URI_SELECTED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

struct _YelpViewContentPriv {
	/* Content tree */
	GtkWidget    *tree_sw;
	GtkWidget    *content_tree;
	GtkTreeStore *tree_store;
	GNode        *doc_tree;

	/* Html view */
	YelpHtml     *html_view;

	gchar        *current_docpath;
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
                                (GClassInitFunc) yvc_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpViewContent),
                                0,
                                (GInstanceInitFunc) yvc_init,
                        };
                
                view_type = g_type_register_static (GTK_TYPE_HPANED,
                                                    "YelpViewContent", 
                                                    &view_info, 0);
        }
        
        return view_type;
}


static void
yvc_init (YelpViewContent *view)
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
	priv->current_docpath = g_strdup ("");
	
	g_signal_connect (priv->html_view, "uri_selected",
			  G_CALLBACK (yvc_html_uri_selected_cb), 
			  view);
}

static void
yvc_class_init (YelpViewContentClass *klass)
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
}

static void
yvc_html_uri_selected_cb (YelpHtml        *html,
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
yvc_tree_selection_changed_cb (GtkTreeSelection *selection, 
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
			  G_CALLBACK (yvc_tree_selection_changed_cb), 
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
        gtk_container_add (GTK_CONTAINER (html_sw), 
			   yelp_html_get_widget (priv->html_view));

	/* Add the tree and html view to the paned */
	gtk_paned_add1 (GTK_PANED (view), priv->tree_sw);
        gtk_paned_add2 (GTK_PANED (view), frame);
        gtk_paned_set_position (GTK_PANED (view), 250);

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
	
	g_return_if_fail (YELP_IS_VIEW_CONTENT (content));
	g_return_if_fail (uri != NULL);
	
	priv = content->priv;

	if (yelp_uri_get_type (uri) == YELP_URI_TYPE_DOCBOOK_XML) {
		/* ghelp uri-scheme /usr/share/gnome/help... */
		const gchar *docpath;

 		docpath = yelp_uri_get_path (uri);

		if (docpath && strcmp (docpath, priv->current_docpath)) {
			/* Try to find it in the scrollkeeper database,
			   doesn't have to exist here */
			node = yelp_scrollkeeper_get_toc_tree (docpath);
			
			if (node) {
				gtk_widget_show (priv->tree_sw);
				yelp_view_content_set_tree (content, node);
				
			} else {
				gtk_widget_hide (priv->tree_sw);
			}
			
			g_free (priv->current_docpath);
			priv->current_docpath = g_strdup (docpath);
		}
	} else {
		gtk_widget_hide (priv->tree_sw);
	}

	yelp_html_open_uri (priv->html_view, uri, error);
}

void
yelp_view_content_stop (YelpViewContent *content)
{
	g_return_if_fail (YELP_IS_VIEW_CONTENT (content));
	
	yelp_html_cancel_loading (content->priv->html_view);
}
