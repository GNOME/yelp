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
#include <string.h>
#include "yelp-html.h"
#include "yelp-marshal.h"
#include "yelp-scrollkeeper.h"
#include "yelp-util.h"
#include "yelp-view-content.h"

static void yvc_init                      (YelpViewContent         *html);
static void yvc_class_init                (YelpViewContentClass    *klass);
static void yvc_tree_selection_changed_cb (GtkTreeSelection        *selection,
					   YelpViewContent         *content);


enum {
	URL_SELECTED,
	TITLE_CHANGED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

struct _YelpViewContentPriv {
	/* Content tree */
	GtkWidget    *content_tree;
	GtkTreeStore *tree_store;
	GNode        *doc_tree;

	/* Html view */
	GtkWidget    *html_view;
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
yvc_html_url_selected_cb (YelpHtml        *html,
			  char            *url,
			  char            *base_url,
			  gboolean         handled,
			  YelpViewContent *view)
{
	/* Just propagate the signal to the view */
	g_signal_emit (view, signals[URL_SELECTED], 0,
		       url, base_url, handled);
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
	
	priv->html_view    = yelp_html_new ();

	g_signal_connect (priv->html_view, "url_selected",
			  G_CALLBACK (yvc_html_url_selected_cb), 
			  view);
}

static void
yvc_class_init (YelpViewContentClass *klass)
{
	signals[URL_SELECTED] = 
		g_signal_new ("url_selected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpViewContentClass,
					       url_selected),
			      NULL, NULL,
			      yelp_marshal_VOID__STRING_STRING_BOOLEAN,
			      G_TYPE_NONE,
			      3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);

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

		yelp_html_open_section (YELP_HTML (priv->html_view), section);
	}
	
	/* FIXME: Emit section_selected?? */
/* 	yelp_history_goto (priv->history, section); */
}

GtkWidget *
yelp_view_content_new (GNode *doc_tree)
{
	YelpViewContent     *view;
	YelpViewContentPriv *priv;
	GtkTreeSelection    *selection;
        GtkWidget           *html_sw;
	GtkWidget           *tree_sw;
	GtkWidget           *frame;
	
	view = g_object_new (YELP_TYPE_VIEW_CONTENT, NULL);
	priv = view->priv;

	priv->doc_tree = doc_tree;
	
	/* Setup the content tree */
        tree_sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (tree_sw),
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

        gtk_container_add (GTK_CONTAINER (tree_sw), priv->content_tree);

        /* Setup the Html view */
 	html_sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (html_sw),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
	frame = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (frame), html_sw);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	
        gtk_container_add (GTK_CONTAINER (html_sw), priv->html_view);

	/* Add the tree and html view to the paned */
	gtk_paned_add1 (GTK_PANED (view), tree_sw);
        gtk_paned_add2 (GTK_PANED (view), frame);
        gtk_paned_set_position (GTK_PANED (view), 250);

	return GTK_WIDGET (view);
}

#if 0
void
yelp_view_content_show_path (YelpViewContent *content_view,
			     GtkTreePath     *path)
{
	YelpViewContentPriv *priv;
	GtkTreeModel        *model;
	
	g_return_if_fail (YELP_IS_VIEW_CONTENT (content_view));

	priv = content_view->priv;

	model = gtk_tree_model_filter_new_with_model (priv->tree_model,
						      2, path);

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->content_tree), model);
	
}
#endif

static void
yelp_view_content_insert_tree (YelpViewContent *content,
			       GtkTreeIter     *parent,
			       GNode           *node)
{
	GtkTreeIter iter;
	YelpSection *section;
	GNode *child;
	
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
	
	gtk_tree_store_clear (content->priv->tree_store);

	child = node->children;
	while (child) {
		yelp_view_content_insert_tree (content, NULL, child);
		child = child->next;
	}
}

void
yelp_view_content_show_uri (YelpViewContent *content,
			    const gchar     *url)
{
	YelpViewContentPriv *priv;
	YelpSection         *section;
	gchar               *content_url;
	gchar               *title = NULL;
	GNode               *node;
	
	g_return_if_fail (YELP_IS_VIEW_CONTENT (content));

	priv = content->priv;
	
	if (strncmp (url, "path:", 5) == 0) {
		node = yelp_util_decompose_path_url (priv->doc_tree,
						     url,
						     &content_url);

		title = ((YelpSection *) node->data)->name;
		
		yelp_view_content_set_tree (content, node);
	} else if (strncmp (url, "ghelp:", 6) == 0) {
		const gchar *docpath;
		GNode       *doc_node;
		
		docpath = url + 6;
		
		node = yelp_scrollkeeper_get_toc_tree_model (docpath);

		doc_node = yelp_util_find_node_from_uri (priv->doc_tree, 
							 url);

		if (node) {
			yelp_view_content_set_tree (content, node);
		}
		
		content_url = (char *)url;
	} else {
	}

	if (title) {
		g_signal_emit (content, signals[TITLE_CHANGED], 0, title);
	}
	
	/* FIXME: This is a quite dubious way to load the url... */
	section = yelp_section_new (YELP_SECTION_DOCUMENT,
				    NULL, content_url, NULL, NULL);

	yelp_html_open_section (YELP_HTML (content->priv->html_view), section);
	yelp_section_free (section);

	if (content_url != url) {
		g_free (content_url);
	}
}

