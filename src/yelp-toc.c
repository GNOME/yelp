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

#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include "yelp-toc.h"

static void yelp_toc_init	 (YelpToc            *toc);
static void yelp_toc_class_init  (YelpTocClass       *klass);
static void yelp_toc_selected_cb (GtkTreeSelection   *selection,
				  YelpToc            *toc);

struct _YelpTocPriv {
        GtkTreeStore *model;
};

enum {
        SECTION_SELECTED,
        LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

GType
yelp_toc_get_type (void)
{
        static GType toc_type = 0;

        if (!toc_type)
        {
                static const GTypeInfo toc_info =
                        {
                                sizeof (YelpTocClass),
                                NULL,
                                NULL,
                                (GClassInitFunc) yelp_toc_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpToc),
                                0,
                                (GInstanceInitFunc) yelp_toc_init,
                        };
                
                toc_type = g_type_register_static (GTK_TYPE_TREE_VIEW, 
                                                   "YelpToc", &toc_info, 0);
        }
        
        return toc_type;
}

static void
yelp_toc_init (YelpToc *toc)
{
        YelpTocPriv *priv;
        
        priv        = g_new0 (YelpTocPriv, 1);
	priv->model = NULL;
        toc->priv   = priv;

}

static void
yelp_toc_class_init (YelpTocClass *klass)
{
        signals[SECTION_SELECTED] = 
                g_signal_new ("section_selected",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (YelpTocClass, section_selected),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE,
                              1, G_TYPE_POINTER);
}

static void
yelp_toc_selected_cb (GtkTreeSelection *selection, YelpToc *toc)
{
	YelpTocPriv *priv;
 	GtkTreeIter  iter;
	YelpSection *section;
	
/* 	GnomeVFSURI *uri; */
	
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_return_if_fail (YELP_IS_TOC (toc));

        priv = toc->priv;
	
	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {

		gtk_tree_model_get (GTK_TREE_MODEL (priv->model), &iter,
				    1, &section,
				    -1);

                g_signal_emit (G_OBJECT (toc), signals[SECTION_SELECTED], 
			       0, section);
	}
}

GtkWidget *
yelp_toc_new (GtkTreeStore *model)
{
        YelpToc           *toc;
        YelpTocPriv       *priv;
        GtkCellRenderer   *cell;
        GtkTreeViewColumn *column;
	
        toc         = g_object_new (YELP_TYPE_TOC, NULL);
        priv        = toc->priv;
	priv->model = model;
        
        gtk_tree_view_set_model (GTK_TREE_VIEW (toc), 
                                 GTK_TREE_MODEL (model));

        cell   = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (_("Section"), cell,
                                                           "text", 0,
                                                           NULL);

        gtk_tree_view_column_set_sort_column_id (column, 0);
        gtk_tree_view_append_column (GTK_TREE_VIEW (toc), column);

	g_signal_connect (G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (toc))),
			  "changed",
			  G_CALLBACK (yelp_toc_selected_cb), toc);

        return GTK_WIDGET (toc);
}

gboolean
yelp_toc_open (YelpToc *toc, GnomeVFSURI *uri)
{
/* 	YelpTocPriv *priv; */
/* 	GtkTreeIter  iter; */
	
	g_print ("======> Looking for children... ");

/* 	if (gtk_tree_model_iter_children (priv->model, &iter, NULL)) { */
/* 		g_print ("found\n"); */
/* 	} else { */
/* 		g_print ("not found\n"); */
/* 	} */
	
	/* FIX: Try to find the correct node and expand it */

	return TRUE;
}

