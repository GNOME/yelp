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

#include "yelp-window.h"
#include "yelp-section.h"
#include "yelp-scrollkeeper.h"
#include "yelp-base.h"

typedef struct {
	YelpBase     *base;
	GtkTreeIter *parent;
} ForeachData;

static void          yelp_base_init               (YelpBase        *base);
static void          yelp_base_class_init         (YelpBaseClass   *klass);

struct _YelpBasePriv {
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
        
        priv->bookshelf  = gtk_tree_store_new (2, 
                                               G_TYPE_STRING, G_TYPE_POINTER);
        
        base->priv       = priv;
}

static void
yelp_base_class_init (YelpBaseClass *klass)
{
}

YelpBase *
yelp_base_new (void)
{
        YelpBase *base;
	gboolean  result;

        base = g_object_new (YELP_TYPE_BASE, NULL);

	result = yelp_scrollkeeper_init (base->priv->bookshelf);

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
