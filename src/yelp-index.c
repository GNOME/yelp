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
#include "yelp-index.h"

static void yelp_index_init	       (YelpIndex        *index);
static void yelp_index_class_init      (YelpIndexClass   *klass);
static void yelp_index_entry_active_cb (GtkEntry         *entry,
					gpointer          user_data);

struct _YelpIndexPriv {
        GtkWidget    *entry;
        GtkWidget    *list;
        GtkListStore *model;
        
};

enum {
	SECTION_SELECTED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

GType
yelp_index_get_type (void)
{
        static GType index_type = 0;

        if (!index_type)
        {
                static const GTypeInfo index_info =
                        {
                                sizeof (YelpIndexClass),
                                NULL,
                                NULL,
                                (GClassInitFunc) yelp_index_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpIndex),
                                0,
                                (GInstanceInitFunc) yelp_index_init,
                        };
                
                index_type = g_type_register_static (G_TYPE_OBJECT,
                                                     "YelpIndex", 
                                                     &index_info, 0);
        }
        
        return index_type;
}

static void
yelp_index_init (YelpIndex *index)
{
        YelpIndexPriv *priv;
        
        priv        = g_new0 (YelpIndexPriv, 1);
        index->priv = priv;
        
        priv->model = gtk_list_store_new (1, G_TYPE_STRING);
        priv->entry = NULL;
        priv->list  = NULL;
}

static void
yelp_index_class_init (YelpIndexClass *klass)
{
	signals[SECTION_SELECTED] = 
		g_signal_new ("section_selected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpIndexClass, 
					       section_selected),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
}

void
yelp_index_entry_active_cb (GtkEntry *entry, gpointer data)
{
	const gchar *text;
	GSList      *hit_list;
	
	g_return_if_fail (GTK_IS_ENTRY (entry));
	g_return_if_fail (YELP_IS_INDEX (data));
	
	text = gtk_entry_get_text (entry);

	g_print ("Index searching isn't supported yet\n");
}

YelpIndex *
yelp_index_new (void)
{
        return g_object_new (YELP_TYPE_INDEX, NULL);
}

GtkWidget *
yelp_index_get_entry (YelpIndex *index)
{
        YelpIndexPriv *priv;
        
        priv = index->priv;
        
        if (!priv->entry) {
                priv->entry = gtk_entry_new ();
		g_signal_connect (priv->entry,
				  "activate",
				  G_CALLBACK (yelp_index_entry_active_cb),
				  G_OBJECT (index));
        }

        return priv->entry;
}

GtkWidget *
yelp_index_get_list (YelpIndex *index)
{
        YelpIndexPriv     *priv;
        GtkCellRenderer   *cell;
        GtkTreeViewColumn *column;
        
        priv = index->priv;
        
        if (!priv->list) {
                priv->list = gtk_tree_view_new_with_model (
                        GTK_TREE_MODEL (priv->model));
        
                cell   = gtk_cell_renderer_text_new ();
                column = gtk_tree_view_column_new_with_attributes (NULL, cell,
                                                                   "text", 0,
                                                                   NULL);
                gtk_tree_view_column_set_sort_column_id (column, 0);

                gtk_tree_view_append_column (GTK_TREE_VIEW (priv->list), 
                                             column);
        }

        return priv->list;
}



