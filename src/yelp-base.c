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

#include <config.h>

#include <bonobo/bonobo-main.h>

#include <string.h>

#include "yelp-window.h"
#include "yelp-section.h"
#include "yelp-scrollkeeper.h"
#include "yelp-man.h"
#include "yelp-info.h"
#include "yelp-base.h"

typedef struct {
	YelpBase     *base;
	GtkTreeIter *parent;
} ForeachData;

struct _YelpBasePriv {
	GNode  *toc_tree;

	GList  *index;
	GSList *windows;
};


static void           yelp_base_init                (YelpBase       *base);
static void           yelp_base_class_init          (YelpBaseClass  *klass);
static void           yelp_base_new_window_cb       (YelpWindow     *window,
						     YelpBase       *base);
static void           yelp_base_window_finalized_cb (YelpBase       *base,
						     YelpWindow     *window);


#define PARENT_TYPE BONOBO_OBJECT_TYPE
static BonoboObjectClass *parent_class;

static void
impl_Yelp_newWindow (PortableServer_Servant  servant,
		     const CORBA_char       *url,
		     CORBA_Environment      *ev)
{
	YelpBase  *yelp_base;
	GtkWidget *window;
	
	yelp_base = YELP_BASE (bonobo_object (servant));
	
	window = yelp_base_new_window (yelp_base);

	gtk_widget_show_all (window);

	if (url && strcmp (url, "")) {
		yelp_window_open_uri (YELP_WINDOW (window), url);
	}
}

static void
yelp_base_init (YelpBase *base)
{
        YelpBasePriv *priv;

        priv = g_new0 (YelpBasePriv, 1);
        
	priv->toc_tree = g_node_new (NULL);

	priv->windows = NULL;
        base->priv    = priv;
}

static void
yelp_base_class_init (YelpBaseClass *klass)
{
	POA_GNOME_Yelp__epv *epv = &klass->epv;
	
	parent_class = gtk_type_class (PARENT_TYPE);

	epv->newWindow = impl_Yelp_newWindow;
}

static void
yelp_base_new_window_cb (YelpWindow *window, YelpBase *base)
{
	GtkWidget *new_window;
	
	g_return_if_fail (YELP_IS_WINDOW (window));
	g_return_if_fail (YELP_IS_BASE (base));
	
	new_window = yelp_base_new_window (base);
	
	gtk_widget_show_all (new_window);
}

static void
yelp_base_window_finalized_cb (YelpBase *base, YelpWindow *window)
{
	YelpBasePriv *priv;
	
	g_return_if_fail (YELP_IS_BASE (base));
	
	priv = base->priv;
	
	priv->windows = g_slist_remove (priv->windows, window);

	if (g_slist_length (priv->windows) == 0) {
		bonobo_main_quit ();
	}
}

YelpBase *
yelp_base_new (void)
{
        YelpBase     *base;
	YelpBasePriv *priv;
	gboolean      result;

        base = g_object_new (YELP_TYPE_BASE, NULL);
	priv = base->priv;
	
	result = yelp_scrollkeeper_init (priv->toc_tree,
					 &priv->index);
	result = yelp_man_init (base->priv->toc_tree);
	result = yelp_info_init (base->priv->toc_tree);
	
        return base;
}

GtkWidget *
yelp_base_new_window (YelpBase *base)
{
	YelpBasePriv *priv;
	GtkWidget    *window;
        
        g_return_val_if_fail (YELP_IS_BASE (base), NULL);

	priv = base->priv;
        
        window = yelp_window_new (priv->toc_tree, priv->index);
        
	priv->windows = g_slist_prepend (priv->windows, window);

	g_object_weak_ref (G_OBJECT (window),
			   (GWeakNotify) yelp_base_window_finalized_cb,
			   base);
	

	g_signal_connect (window, "new_window_requested",
			  G_CALLBACK (yelp_base_new_window_cb),
			  base);

	return window;
}

BONOBO_TYPE_FUNC_FULL (YelpBase, GNOME_Yelp, PARENT_TYPE, yelp_base);
