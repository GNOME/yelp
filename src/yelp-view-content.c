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
#include "gtktreemodelfilter.h"
#include "yelp-html.h"
#include "yelp-marshal.h"
#include "yelp-view-content.h"

static void yvc_init                      (YelpViewContent         *html);
static void yvc_class_init                (YelpViewContentClass    *klass);
static void yvc_tree_selection_changed_cb (GtkTreeSelection        *selection,
					   YelpViewContent         *content);


enum {
	URL_SELECTED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

struct _YelpViewContentPriv {
	/* Content tree */
	GtkWidget    *content_tree;
	GtkTreeModel *tree_model;

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
	priv->tree_model   = NULL;
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
yelp_view_content_new (GtkTreeModel *tree_model)
{
	YelpViewContent     *view;
	YelpViewContentPriv *priv;
	GtkTreeSelection    *selection;
        GtkWidget           *html_sw;
	GtkWidget           *tree_sw;
	GtkWidget           *frame;
	
	view = g_object_new (YELP_TYPE_VIEW_CONTENT, NULL);
	priv = view->priv;

	priv->tree_model = tree_model;
	
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

void
yelp_view_content_show_uri (YelpViewContent *content, const gchar *uri)
{
	YelpSection *section;
	/* FIXME: Find the path in the tree */

	/* FIXME: This is a quite dubious way to load the url... */
	section = yelp_section_new (YELP_SECTION_DOCUMENT,
				    NULL, uri, NULL, NULL);
	yelp_html_open_section (YELP_HTML (content->priv->html_view), section);
	yelp_section_free (section);
}

