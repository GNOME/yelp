/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
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

#ifndef __YELP_WINDOW_H__
#define __YELP_WINDOW_H__

#include <gtk/gtktreemodel.h>
#include <gtk/gtkwindow.h>

#include "yelp-base.h"
#include "yelp-uri.h"

#define YELP_TYPE_WINDOW		(yelp_window_get_type ())
#define YELP_WINDOW(obj)		(GTK_CHECK_CAST ((obj), YELP_TYPE_WINDOW, YelpWindow))
#define YELP_WINDOW_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), YELP_TYPE_WINDOW, YelpWindowClass))
#define YELP_IS_WINDOW(obj)		(GTK_CHECK_TYPE ((obj), YELP_TYPE_WINDOW))
#define YELP_IS_WINDOW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), YELP_TYPE_WINDOW))

typedef struct _YelpWindow       YelpWindow;
typedef struct _YelpWindowClass  YelpWindowClass;
typedef struct _YelpWindowPriv   YelpWindowPriv;

struct _YelpWindow
{
    GtkWindow       parent;

    YelpWindowPriv *priv;
};

struct _YelpWindowClass
{
    GtkWindowClass  parent_class;

    /* Signals */
    void (*new_window_requested) (YelpWindow *window);
};

GType            yelp_window_get_type        (void);
GtkWidget *      yelp_window_new             (GNode         *doc_tree,
					      GList         *index);
void             yelp_window_open_uri        (YelpWindow    *window,
					      YelpURI       *uri);
YelpURI *        yelp_window_get_current_uri (YelpWindow    *window);

#endif /* __YELP_WINDOW_H__ */
