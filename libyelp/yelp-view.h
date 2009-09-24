/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009 Shaun McCance <shaunm@gnome.org>
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
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifndef __YELP_VIEW_H__
#define __YELP_VIEW_H__

#include <gtk/gtk.h>
#include <webkit/webkit.h>

#include "yelp-document.h"
#include "yelp-uri.h"

#define YELP_TYPE_VIEW            (yelp_view_get_type ())
#define YELP_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), YELP_TYPE_VIEW, YelpView))
#define YELP_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), YELP_TYPE_VIEW, YelpViewClass))
#define YELP_IS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), YELP_TYPE_VIEW))
#define YELP_IS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), YELP_TYPE_VIEW))

typedef struct _YelpView       YelpView;
typedef struct _YelpViewClass  YelpViewClass;
typedef struct _YelpViewPriv   YelpViewPriv;

struct _YelpView
{
    WebKitWebView       parent;
    YelpViewPriv       *priv;
};

struct _YelpViewClass
{
    WebKitWebViewClass  parent_class;
};

GType            yelp_view_get_type        (void);
GtkWidget *      yelp_view_new             (void);

void             yelp_view_load            (YelpView     *view,
					    const gchar  *uri);
void             yelp_view_load_uri        (YelpView     *view,
					    YelpUri      *uri);
void             yelp_view_load_document   (YelpView     *view,
					    YelpUri      *uri,
					    YelpDocument *document);

#endif /* __YELP_VIEW_H__ */
