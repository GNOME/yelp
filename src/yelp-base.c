/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@imendio.com>
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
 * Author: Mikael Hallendal <micke@imendio.com>
 */

#include <config.h>

#include <bonobo/bonobo-main.h>

#include <string.h>

#include "yelp-cache.h"
#include "yelp-window.h"
#include "yelp-section.h"
#include "yelp-scrollkeeper.h"
#include "yelp-man.h"
#include "yelp-info.h"
#include "yelp-base.h"

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
	
	yelp_base = YELP_BASE (bonobo_object (servant));

	yelp_base_new_window (yelp_base, url);
}

static GNOME_Yelp_WindowList *
impl_Yelp_getWindows (PortableServer_Servant  servant,
		      CORBA_Environment      *ev)
{
	YelpBase              *base;
	YelpBasePriv          *priv;
	GNOME_Yelp_WindowList *list;
	gint                   len, i;
	GSList                *node;
	YelpURI               *uri;
	
	base = YELP_BASE (bonobo_object (servant));
	priv = base->priv;

	len  = g_slist_length (priv->windows);
	
	list = GNOME_Yelp_WindowList__alloc ();
	list->_buffer = CORBA_sequence_CORBA_string_allocbuf (len);
	list->_length = len;
	list->_maximum = len;
	CORBA_sequence_set_release (list, CORBA_TRUE);
	
	for (node = priv->windows, i = 0; node; node = node->next, i++) {
		gchar *str_uri;

		uri = yelp_window_get_current_uri (YELP_WINDOW (node->data));
		str_uri = yelp_uri_to_string (uri);
		list->_buffer[i] = CORBA_string_dup (str_uri);
		g_free (str_uri);
	}
	
	return list;
}

static void
yelp_base_init (YelpBase *base)
{
        YelpBasePriv *priv;

        priv = g_new0 (YelpBasePriv, 1);
        
	priv->toc_tree = g_node_new (NULL);
	priv->index    = NULL;
	priv->windows  = NULL;
        base->priv     = priv;

	yelp_cache_init ();
}

static void
yelp_base_class_init (YelpBaseClass *klass)
{
	POA_GNOME_Yelp__epv *epv = &klass->epv;
	
	parent_class = gtk_type_class (PARENT_TYPE);

	epv->newWindow  = impl_Yelp_newWindow;
	epv->getWindows = impl_Yelp_getWindows;
}

static void
yelp_base_new_window_cb (YelpWindow *window, YelpBase *base)
{
	GtkWidget *new_window;
	
	g_return_if_fail (YELP_IS_WINDOW (window));
	g_return_if_fail (YELP_IS_BASE (base));
	
	new_window = yelp_base_new_window (base, NULL);
	
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
	
        base = g_object_new (YELP_TYPE_BASE, NULL);
	priv = base->priv;
	
	yelp_scrollkeeper_init (priv->toc_tree, &priv->index);
	yelp_man_init (base->priv->toc_tree, &priv->index);
	yelp_info_init (base->priv->toc_tree, &priv->index);
	
	priv->index = g_list_sort (priv->index, yelp_section_compare);

        return base;
}

GtkWidget *
yelp_base_new_window (YelpBase *base, const gchar *str_uri)
{
	YelpBasePriv *priv;
	YelpURI      *uri;
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


	gtk_widget_show_all (window);

	if (str_uri && strcmp (str_uri, ""))
		uri = yelp_uri_new (str_uri);
	else
		uri = yelp_uri_new ("toc:");

	yelp_window_open_uri (YELP_WINDOW (window), uri);

	return window;
}

BONOBO_TYPE_FUNC_FULL (YelpBase, GNOME_Yelp, PARENT_TYPE, yelp_base);
