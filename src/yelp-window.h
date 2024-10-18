/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2010 Shaun McCance  <shaunm@gnome.org>
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#ifndef __YELP_WINDOW_H__
#define __YELP_WINDOW_H__

#include <adwaita.h>
#include <gtk/gtk.h>

#include "yelp-uri.h"

#define YELP_TYPE_WINDOW            (yelp_window_get_type ())
#define YELP_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), YELP_TYPE_WINDOW, YelpWindow))
#define YELP_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), YELP_TYPE_WINDOW, YelpWindowClass))
#define YELP_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), YELP_TYPE_WINDOW))
#define YELP_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), YELP_TYPE_WINDOW))

typedef struct _YelpWindow       YelpWindow;
typedef struct _YelpWindowClass  YelpWindowClass;

struct _YelpWindow
{
    AdwApplicationWindow       parent;
};

struct _YelpWindowClass
{
    AdwApplicationWindowClass  parent_class;
};

GType             yelp_window_get_type     (void);
YelpWindow *      yelp_window_new          (YelpApplication *app);
void              yelp_window_load_uri     (YelpWindow      *window,
                                            YelpUri         *uri);
YelpUri *         yelp_window_get_uri      (YelpWindow      *window);
void              yelp_window_get_geometry (YelpWindow      *window,
                                            gint            *width,
                                            gint            *height);

#endif /* __YELP_WINDOW_H__ */
