/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
 *
 * Author: Mikael Hallendal <micke@codefactory.se>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <atk/atk.h>
#include <gtk/gtkaccessible.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

#include "yelp-index-model.h"
#include "yelp-html.h"
#include "yelp-marshal.h"
#include "yelp-reader.h"
#include "yelp-view.h"

#define d(x)

static void     view_init                       (YelpView      *view);
static void     view_class_init                 (YelpViewClass *klass);

enum {
	URI_SELECTED,
	TITLE_CHANGED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

GType
yelp_view_get_type (void)
{
        static GType view_type = 0;

        if (!view_type)
        {
                static const GTypeInfo view_info =
                        {
                                sizeof (YelpViewClass),
                                NULL,
                                NULL,
                                (GClassInitFunc) view_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpView),
                                0,
                                (GInstanceInitFunc) view_init,
                        };
                
                view_type = g_type_register_static (G_TYPE_OBJECT,
                                                    "YelpView", 
                                                    &view_info, 0);
        }
        
        return view_type;
}

static void
view_init (YelpView *view)
{
}

static void
view_class_init (YelpViewClass *klass)
{
	signals[URI_SELECTED] =
		g_signal_new ("uri_selected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpViewClass,
					       uri_selected),
			      NULL, NULL,
			      yelp_marshal_VOID__POINTER_BOOLEAN,
			      G_TYPE_NONE,
			      2, G_TYPE_POINTER, G_TYPE_BOOLEAN);

	signals[TITLE_CHANGED] = 
		g_signal_new ("title_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpViewClass,
					       title_changed),
			      NULL, NULL,
			      yelp_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);

	klass->show_uri   = NULL;
}

void
yelp_view_show_uri (YelpView *view, YelpURI *uri, GError **error)
{
	if (YELP_VIEW_GET_CLASS (view)->show_uri) {
		YELP_VIEW_GET_CLASS (view)->show_uri (view, uri, error);
	}
}

