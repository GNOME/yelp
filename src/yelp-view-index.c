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
#include "yelp-html.h"
#include "yelp-view-index.h"

static void yvi_init                      (YelpViewIndex         *view);
static void yvi_class_init                (YelpViewIndexClass    *klass);
static void yvi_tree_selection_changed_cb (GtkTreeSelection      *selection,
					   YelpViewIndex         *content);
static void yvi_section_selected_cb       (YelpViewIndex         *content,
					   YelpSection             *section);

struct _YelpViewIndexPriv {
	/* Content tree */
	GtkWidget    *content_tree;
	GtkTreeModel *tree_model;

	/* Query entry */
	GtkWidget    *entry;
	
	/* Html view */
	GtkWidget    *html_view;
};

GType
yelp_view_index_get_type (void)
{
        static GType view_type = 0;

        if (!view_type)
        {
                static const GTypeInfo view_info =
                        {
                                sizeof (YelpViewIndexClass),
                                NULL,
                                NULL,
                                (GClassInitFunc) yvi_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpViewIndex),
                                0,
                                (GInstanceInitFunc) yvi_init,
                        };
                
                view_type = g_type_register_static (GTK_TYPE_HPANED,
                                                    "YelpViewIndex", 
                                                    &view_info, 0);
        }
        
        return view_type;
}

static void
yvi_init (YelpViewIndex *view)
{
	YelpViewIndexPriv *priv;
	
	priv = g_new0 (YelpViewIndexPriv, 1);
	view->priv = priv;

	priv->content_tree = gtk_tree_view_new ();
	priv->tree_model   = NULL;
	priv->html_view    = yelp_html_new ();
}

static void
yvi_class_init (YelpViewIndexClass *klass)
{
	
}

static void
yvi_tree_selection_changed_cb (GtkTreeSelection *selection, 
			       YelpViewIndex  *content)
{
	YelpViewIndexPriv *priv;
 	GtkTreeIter          iter;
	YelpSection         *section;
	
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_return_if_fail (YELP_IS_VIEW_INDEX (content));

	priv = content->priv;

	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (priv->tree_model), &iter,
				    1, &section,
				    -1);

		yelp_html_open_section (YELP_HTML (priv->html_view), section);
	}
	
	/* FIXME: Emit section_selected?? */
/* 	yelp_history_goto (priv->history, section); */
}

static void
yvi_section_selected_cb (YelpViewIndex *content, YelpSection *section)
{
	g_return_if_fail (YELP_IS_VIEW_INDEX (content));
	g_return_if_fail (section != NULL);
	
	yelp_html_open_section (YELP_HTML (content->priv->html_view),
				section);

	/* FIXME: Emit section_selected?? */
}

GtkWidget *
yelp_view_index_new (GtkTreeModel *tree_model)
{
	YelpViewIndex     *view;
	YelpViewIndexPriv *priv;
        GtkCellRenderer   *cell;
        GtkTreeViewColumn *column;
	GtkTreeSelection  *selection;
        GtkWidget         *html_sw;
        GtkWidget         *list_sw;
	GtkWidget         *frame;
	GtkWidget         *box;
	
	view = g_object_new (YELP_TYPE_VIEW_INDEX, NULL);
	priv = view->priv;

	priv->tree_model = tree_model;

	/* Setup the index box */
	box = gtk_vbox_new (FALSE, 0);

	priv->entry = gtk_entry_new ();
	
	gtk_box_pack_start (GTK_BOX (box), priv->entry, 
			    FALSE, FALSE, 0);

        list_sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_sw),
                                        GTK_POLICY_AUTOMATIC, 
                                        GTK_POLICY_AUTOMATIC);

#if 0
        gtk_tree_view_set_model (GTK_TREE_VIEW (priv->content_tree), 
				 tree_model);
#endif

        cell   = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (_("Section"), cell,
                                                           "text", 0,
                                                           NULL);

        gtk_tree_view_column_set_sort_column_id (column, 0);
        gtk_tree_view_append_column (GTK_TREE_VIEW (priv->content_tree), 
				     column);

	selection = gtk_tree_view_get_selection (
		GTK_TREE_VIEW (priv->content_tree));

	g_signal_connect (selection, "changed",
			  G_CALLBACK (yvi_tree_selection_changed_cb), 
			  view);
	
        gtk_container_add (GTK_CONTAINER (list_sw), priv->content_tree);
	gtk_box_pack_end_defaults (GTK_BOX (box), list_sw);

        /* Setup the Html view */
 	html_sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (html_sw),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
	frame = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (frame), html_sw);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	
 	g_signal_connect_swapped (priv->html_view, "section_selected",
 				  G_CALLBACK (yvi_section_selected_cb),
 				  G_OBJECT (view));

        gtk_container_add (GTK_CONTAINER (html_sw), priv->html_view);

	/* Add the tree and html view to the paned */
	gtk_paned_add1 (GTK_PANED (view), box);
        gtk_paned_add2 (GTK_PANED (view), frame);
        gtk_paned_set_position (GTK_PANED (view), 250);

	return GTK_WIDGET (view);
}


