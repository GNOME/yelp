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

static void yvi_init                       (YelpViewIndex         *view);
static void yvi_class_init                 (YelpViewIndexClass    *klass);
static void yvi_index_selection_changed_cb (GtkTreeSelection      *selection,
					    YelpViewIndex         *content);
static void yvi_html_url_selected_cb       (YelpViewIndex         *content,
					    char                  *url,
					    char                  *base_url,
					    gboolean               handled);

struct _YelpViewIndexPriv {
	GList        *index;

	/* List of keywords */
	GtkWidget    *index_view;
	GtkListStore *list_store;

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

	priv->index_view = gtk_tree_view_new ();
	priv->list_store = gtk_list_store_new (2, 
					       G_TYPE_STRING, G_TYPE_POINTER);

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->index_view),
				 GTK_TREE_MODEL (priv->list_store));

	priv->html_view    = yelp_html_new ();
	
	g_signal_connect (priv->html_view, "url_selected",
			  G_CALLBACK (yvi_html_url_selected_cb),
			  view);
}

static void
yvi_class_init (YelpViewIndexClass *klass)
{
	
}

static void
yvi_index_selection_changed_cb (GtkTreeSelection *selection, 
				YelpViewIndex    *view)
{
	YelpViewIndexPriv *priv;
 	GtkTreeIter        iter;
	YelpSection       *section;
	
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_return_if_fail (YELP_IS_VIEW_INDEX (view));

	priv = view->priv;

	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (priv->list_store), &iter,
				    1, &section,
				    -1);

		/* FIXME: Emit index:string */
		
		yelp_html_open_uri (YELP_HTML (priv->html_view),
				    section->uri, 
				    section->reference);
		
/*  		g_signal_emit (view, signals[URL_SELECTED], 0, */
/* 			       section->reference, section->uri, FALSE); */
	}
}

static void
yvi_html_url_selected_cb (YelpViewIndex *content,
			  char          *url,
			  char          *base_url,
			  gboolean       handled)
{
	g_return_if_fail (YELP_IS_VIEW_INDEX (content));

	/* TODO: What to do here? */
	
	/* FIXME: Emit section_selected?? */
}

GtkWidget *
yelp_view_index_new (GList *index)
{
	YelpViewIndex     *view;
	YelpViewIndexPriv *priv;
	GtkTreeSelection  *selection;
        GtkWidget         *html_sw;
        GtkWidget         *list_sw;
	GtkWidget         *frame;
	GtkWidget         *box;
	
	view = g_object_new (YELP_TYPE_VIEW_INDEX, NULL);
	priv = view->priv;

	priv->index = index;

	/* Setup the index box */
	box = gtk_vbox_new (FALSE, 0);

	priv->entry = gtk_entry_new ();
	
	gtk_box_pack_start (GTK_BOX (box), priv->entry, 
			    FALSE, FALSE, 0);

        list_sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_sw),
                                        GTK_POLICY_AUTOMATIC, 
                                        GTK_POLICY_AUTOMATIC);

	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (priv->index_view), -1,
		_("Section"), gtk_cell_renderer_text_new (),
		"text", 0,
		NULL);

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->index_view),
					   FALSE);

	selection = gtk_tree_view_get_selection (
		GTK_TREE_VIEW (priv->index_view));

	g_signal_connect (selection, "changed",
			  G_CALLBACK (yvi_index_selection_changed_cb),
			  view);
	
	gtk_container_add (GTK_CONTAINER (list_sw), priv->index_view);

	gtk_box_pack_end_defaults (GTK_BOX (box), list_sw);

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
	gtk_paned_add1 (GTK_PANED (view), box);
        gtk_paned_add2 (GTK_PANED (view), frame);
        gtk_paned_set_position (GTK_PANED (view), 250);

	return GTK_WIDGET (view);
}
