/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002 Richard Hult <rhult@codefactory.se>
 * Copyright (C) 2002 Mikael Hallendal <micke@codefactory.se>
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
 */

#include <gtk/gtktreemodel.h>
#include <libgnome/gnome-i18n.h>
#include "yelp-section.h"
#include "yelp-index-model.h"

struct _YelpIndexModelPriv {
        GList      *index_words;

        gint        stamp;
};

#define G_LIST(x) ((GList *) x)

static void          yim_init              (YelpIndexModel       *list_store);
static void          yim_class_init        (YelpIndexModelClass  *class);
static void          yim_tree_model_init   (GtkTreeModelIface    *iface);


static void          yim_finalize          (GObject              *object);
static gint          yim_get_n_columns     (GtkTreeModel         *tree_model);
static GType         yim_get_column_type   (GtkTreeModel         *tree_model,
					     gint                  index);

static gboolean      yim_get_iter          (GtkTreeModel         *tree_model,
					     GtkTreeIter          *iter,
					     GtkTreePath          *path);
static GtkTreePath * yim_get_path          (GtkTreeModel         *tree_model,
					     GtkTreeIter          *iter);
static void          yim_get_value         (GtkTreeModel         *tree_model,
					     GtkTreeIter          *iter,
					     gint                  column,
					     GValue               *value);
static gboolean      yim_iter_next         (GtkTreeModel         *tree_model,
					     GtkTreeIter          *iter);
static gboolean      yim_iter_children     (GtkTreeModel         *tree_model,
					     GtkTreeIter          *iter,
					     GtkTreeIter          *parent);
static gboolean      yim_iter_has_child    (GtkTreeModel         *tree_model,
					     GtkTreeIter          *iter);
static gint          yim_iter_n_children   (GtkTreeModel         *tree_model,
					     GtkTreeIter          *iter);
static gboolean      yim_iter_nth_child    (GtkTreeModel         *tree_model,
					     GtkTreeIter          *iter,
					     GtkTreeIter          *parent,
					     gint                  n);
static gboolean      yim_iter_parent       (GtkTreeModel         *tree_model,
					     GtkTreeIter          *iter,
					     GtkTreeIter          *child);

static GObjectClass *parent_class = NULL;


GtkType
yelp_index_model_get_type (void)
{
        static GType type = 0;
        
        if (!type) {
                static const GTypeInfo info =
                        {
                                sizeof (YelpIndexModelClass),
                                NULL,		/* base_init */
                                NULL,		/* base_finalize */
                                (GClassInitFunc) yim_class_init,
                                NULL,		/* class_finalize */
                                NULL,		/* class_data */
                                sizeof (YelpIndexModel),
                                0,
                                (GInstanceInitFunc) yim_init,
                        };
                
                static const GInterfaceInfo tree_model_info =
                        {
                                (GInterfaceInitFunc) yim_tree_model_init,
                                NULL,
                                NULL
                        };
                
                type = g_type_register_static (G_TYPE_OBJECT,
					       "YelpIndexModel", 
					       &info, 0);
      
                g_type_add_interface_static (type,
                                             GTK_TYPE_TREE_MODEL,
                                             &tree_model_info);
        }
        
        return type;
}

static void
yim_class_init (YelpIndexModelClass *class)
{
        GObjectClass *object_class;

        parent_class = g_type_class_peek_parent (class);
        object_class = (GObjectClass*) class;

        object_class->finalize = yim_finalize;
}

static void
yim_tree_model_init (GtkTreeModelIface *iface)
{
/*         iface->get_flags       = yim_get_flags; */
        iface->get_n_columns   = yim_get_n_columns;
        iface->get_column_type = yim_get_column_type;
        iface->get_iter        = yim_get_iter;
        iface->get_path        = yim_get_path;
        iface->get_value       = yim_get_value;
        iface->iter_next       = yim_iter_next;
        iface->iter_children   = yim_iter_children;
        iface->iter_has_child  = yim_iter_has_child;
        iface->iter_n_children = yim_iter_n_children;
        iface->iter_nth_child  = yim_iter_nth_child;
        iface->iter_parent     = yim_iter_parent;
}

static void
yim_init (YelpIndexModel *model)
{
        YelpIndexModelPriv *priv;
        
        priv = g_new0 (YelpIndexModelPriv, 1);
        
	do {
		priv->stamp = g_random_int ();
	} while (priv->stamp == 0);
        
        model->priv = priv;
}

static void
yim_finalize (GObject *object)
{
	YelpIndexModel *model = YELP_INDEX_MODEL (object);
        
        if (model->priv) {
		if (model->priv->index_words) {
			/* FIXME: Clean up the list */
		}

                g_free (model->priv);
                model->priv = NULL;
        }
        
        (* parent_class->finalize) (object);
}

static gint
yim_get_n_columns (GtkTreeModel *tree_model)
{
        return 1;
}

static GType
yim_get_column_type (GtkTreeModel *tree_model,
                     gint          column)
{
	return G_TYPE_STRING;
}

static gboolean
yim_get_iter (GtkTreeModel *tree_model,
              GtkTreeIter  *iter,
              GtkTreePath  *path)
{
        YelpIndexModel     *model;
        YelpIndexModelPriv *priv;
        GList               *node;
        gint                 i;
        
        g_return_val_if_fail (MG_IS_RESOURCE_MODEL (tree_model), FALSE);
        g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

        model = YELP_INDEX_MODEL (tree_model);
        priv  = model->priv;
        
        i = gtk_tree_path_get_indices (path)[0];
        
        if (i >= g_list_length (priv->index_words)) {
                return FALSE;
        }
        
        node = g_list_nth (priv->index_words, i);
        
        iter->stamp     = priv->stamp;
        iter->user_data = node;
        
        return TRUE;
}

static GtkTreePath *
yim_get_path (GtkTreeModel *tree_model,
              GtkTreeIter  *iter)
{
        YelpIndexModel     *model = YELP_INDEX_MODEL (tree_model);
        YelpIndexModelPriv *priv;
        GtkTreePath         *path;
        GList               *node;
        gint                 i = 0;

        g_return_val_if_fail (MG_IS_RESOURCE_MODEL (tree_model), NULL);
        g_return_val_if_fail (iter->stamp == model->priv->stamp, NULL);

        priv = model->priv;

        for (node = priv->index_words; node; node = node->next)
        {
                if ((gpointer)node->data == (gpointer)iter->user_data)
                        break;
                i++;
        }

        if (node == NULL) {
                return NULL;
        }
        
        path = gtk_tree_path_new ();
        gtk_tree_path_append_index (path, i);

        return path;
}

static void
yim_get_value (GtkTreeModel *tree_model,
               GtkTreeIter  *iter,
               gint          column,
               GValue       *value)
{
        YelpSection *section;
	
        g_return_if_fail (MG_IS_RESOURCE_MODEL (tree_model));
        g_return_if_fail (iter != NULL);

	section = (YelpSection *) (G_LIST(iter->user_data)->data);
	
        switch (column) {
        case 0:
		g_value_set_string (value, section->name);
		break;
        default:
                g_warning ("Bad column %d requested", column);
        }
}

static gboolean
yim_iter_next (GtkTreeModel  *tree_model,
               GtkTreeIter   *iter)
{
	YelpIndexModel *model = YELP_INDEX_MODEL (tree_model);
        
        g_return_val_if_fail (MG_IS_RESOURCE_MODEL (tree_model), FALSE);
        g_return_val_if_fail (model->priv->stamp == iter->stamp, FALSE);

        iter->user_data = G_LIST(iter->user_data)->next;
        
        return (iter->user_data != NULL);
}

static gboolean
yim_iter_children (GtkTreeModel *tree_model,
                   GtkTreeIter  *iter,
                   GtkTreeIter  *parent)
{
        YelpIndexModelPriv *priv;
        
        g_return_val_if_fail (MG_IS_RESOURCE_MODEL (tree_model), FALSE);
        
        priv = YELP_INDEX_MODEL(tree_model)->priv;
        
        /* this is a list, nodes have no children */
        if (parent) {
                return FALSE;
        }

        /* but if parent == NULL we return the list itself as children of the
         * "root"
         */
        
        if (priv->index_words) {
                iter->stamp = priv->stamp;
                iter->user_data = priv->index_words;
                return TRUE;
        } 
        
        return FALSE;
}

static gboolean
yim_iter_has_child (GtkTreeModel *tree_model,
                    GtkTreeIter  *iter)
{
        return FALSE;
}

static gint
yim_iter_n_children (GtkTreeModel *tree_model,
                     GtkTreeIter  *iter)
{
        YelpIndexModelPriv *priv;
        
        g_return_val_if_fail (MG_IS_RESOURCE_MODEL (tree_model), -1);

        priv = YELP_INDEX_MODEL(tree_model)->priv;

        if (iter == NULL) {
                return g_list_length (priv->index_words);
        }
        
        g_return_val_if_fail (priv->stamp == iter->stamp, -1);

        return 0;
}

static gboolean
yim_iter_nth_child (GtkTreeModel *tree_model,
                    GtkTreeIter  *iter,
                    GtkTreeIter  *parent,
                    gint          n)
{
        YelpIndexModelPriv *priv;
        GList               *child;

        g_return_val_if_fail (MG_IS_RESOURCE_MODEL (tree_model), FALSE);

        priv = YELP_INDEX_MODEL(tree_model)->priv;
        
        if (parent) {
                return FALSE;
        }
        
        child = g_list_nth (priv->index_words, n);
        
        if (child) {
                iter->stamp     = priv->stamp;
                iter->user_data = child;
                return TRUE;
        }

        return FALSE;
}

static gboolean
yim_iter_parent (GtkTreeModel *tree_model,
                 GtkTreeIter  *iter,
                 GtkTreeIter  *child)
{
        return FALSE;
}

YelpIndexModel *
yelp_index_model_new (GList *index_words)
{
        YelpIndexModel     *model;
        YelpIndexModelPriv *priv;
        
        model = g_object_new (YELP_TYPE_INDEX_MODEL, NULL);
        
        priv = model->priv;
        
        priv->index_words = index_words;

        return model;
}

