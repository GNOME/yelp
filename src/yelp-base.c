/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 CodeFactory AB
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

#include "metadata.h"
#include "yelp-window.h"
#include "yelp-book.h"
#include "yelp-keyword-db.h"
#include "yelp-base.h"
typedef struct {
	YelpBase     *base;
	GtkTreeIter *parent;
} ForeachData;

static void          yelp_base_init             (YelpBase        *base);
static void          yelp_base_class_init       (YelpBaseClass   *klass);
static void          yelp_base_new_book_cb      (MetaDataParser  *parser, 
                                                 YelpBook        *book, 
                                                 YelpBase        *base);
static GtkTreeIter * yelp_base_insert_node      (YelpBase        *base, 
                                                 GtkTreeIter     *parent, 
                                                 YelpSection     *section);
static void          yelp_base_section_foreach  (GNode           *node, 
                                                 ForeachData     *fdata);

struct _YelpBasePriv {
        YelpKeywordDb *keyword_db;

        GtkTreeStore  *bookshelf;
};

GType
yelp_base_get_type (void)
{
        static GType base_type = 0;

        if (!base_type) {
                static const GTypeInfo base_info = {
                        sizeof (YelpBaseClass),
                        NULL,
                        NULL,
                        (GClassInitFunc) yelp_base_class_init,
                        NULL,
                        NULL,
                        sizeof (YelpBase),
                        0,
                        (GInstanceInitFunc) yelp_base_init,
                };
                
                base_type = g_type_register_static (G_TYPE_OBJECT,
						    "YelpBase", 
						    &base_info, 0);
        }
        
        return base_type;
}

static void
yelp_base_init (YelpBase *base)
{
        YelpBasePriv   *priv;

        priv = g_new0 (YelpBasePriv, 1);
        
        priv->keyword_db = NULL;
        priv->bookshelf  = gtk_tree_store_new (2, 
                                               G_TYPE_STRING, G_TYPE_POINTER);
        
        base->priv       = priv;
}

static void
yelp_base_class_init (YelpBaseClass *klass)
{
}

static void
yelp_base_new_book_cb (MetaDataParser *parser, 
                       YelpBook       *book, 
                       YelpBase       *base)
{
	GtkTreeIter *iter;
	ForeachData *fdata;

	g_print ("New yelp-book: %s\n", ((YelpSection *)book->root->data)->name);
	
	iter = yelp_base_insert_node (base, NULL, 
                                      ((YelpSection *) book->root->data));
	
	fdata         = g_new0 (ForeachData, 1);
	fdata->base   = base;
	fdata->parent = iter;

	g_node_children_foreach (book->root, G_TRAVERSE_ALL,
				 (GNodeForeachFunc) yelp_base_section_foreach,
				 fdata);
	
	g_free (fdata);
}

static GtkTreeIter *
yelp_base_insert_node (YelpBase    *base, 
                       GtkTreeIter *parent, 
                       YelpSection *section)
{
	YelpBasePriv *priv;
	GtkTreeIter *iter;
	
	g_return_val_if_fail (YELP_IS_BASE (base), NULL);
	g_return_val_if_fail (section != NULL, NULL);
	
	priv = base->priv;

	iter = g_new0 (GtkTreeIter, 1);

	gtk_tree_store_append (priv->bookshelf, iter, parent);
        
	gtk_tree_store_set (priv->bookshelf, iter,
			    0, section->name,
			    1, section,
			    -1);

	return iter;
}

static void
yelp_base_section_foreach (GNode *node, ForeachData *fdata)
{
	GtkTreeIter *iter;
	GtkTreeIter *my_parent;
	
	my_parent = fdata->parent;
	
	iter = yelp_base_insert_node (fdata->base, my_parent, node->data);

	fdata->parent = iter;
	
	g_node_children_foreach (node, 
				 G_TRAVERSE_ALL, 
				 (GNodeForeachFunc) yelp_base_section_foreach,
				 fdata);

	fdata->parent = my_parent;
}

YelpBase *
yelp_base_new (void)
{
        YelpBase       *base;
        MetaDataParser *parser;
        gchar          *path;
        
        base = g_object_new (YELP_TYPE_BASE, NULL);

        path = g_strconcat (g_getenv("HOME"), "/.devhelp/specs", NULL);
        
/*         parser = devhelp_parser_new (path); */
 	parser = scrollkeeper_parser_new ();
        
        g_signal_connect (G_OBJECT (parser), "new_book",
                          G_CALLBACK (yelp_base_new_book_cb),
                          base);
        
        metadata_parser_parse (parser);

        return base;
}

GtkTreeStore *
yelp_base_get_bookshelf (YelpBase *base)
{
        g_return_val_if_fail (YELP_IS_BASE (base), NULL);
        
        return base->priv->bookshelf;
}

GtkWidget *
yelp_base_new_window (YelpBase *base)
{
        GtkWidget *window;
        
        g_return_val_if_fail (YELP_IS_BASE (base), NULL);
        
        window = yelp_window_new (base);
        
        return window;
}

