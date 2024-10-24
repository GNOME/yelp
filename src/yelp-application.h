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

#ifndef __YELP_APPLICATION_H__
#define __YELP_APPLICATION_H__

#include <adwaita.h>
#include <glib-object.h>

#include "yelp-uri.h"
#include "yelp-bookmarks.h"

#define YELP_TYPE_APPLICATION            (yelp_application_get_type ())
#define YELP_APPLICATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), YELP_TYPE_APPLICATION, YelpApplication))
#define YELP_APPLICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), YELP_TYPE_APPLICATION, YelpApplicationClass))
#define YELP_IS_APPLICATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), YELP_TYPE_APPLICATION))
#define YELP_IS_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), YELP_TYPE_APPLICATION))

typedef struct _YelpApplication       YelpApplication;
typedef struct _YelpApplicationClass  YelpApplicationClass;

struct _YelpApplication
{
    AdwApplication       parent;
};

struct _YelpApplicationClass
{
    AdwApplicationClass  parent_class;
};

GType             yelp_application_get_type       (void);
YelpApplication*  yelp_application_new            (void);
void              yelp_application_new_window     (YelpApplication  *app,
                                                   const gchar      *uri);
void              yelp_application_new_window_uri (YelpApplication  *app,
                                                   YelpUri          *uri);
void              yelp_application_add_bookmark         (YelpBookmarks     *bookmarks,
                                                         const gchar       *doc_uri,
                                                         const gchar       *page_id,
                                                         const gchar       *icon,
                                                         const gchar       *title);
void              yelp_application_remove_bookmark      (YelpBookmarks     *bookmarks,
                                                         const gchar       *doc_uri,
                                                         const gchar       *page_id);
gboolean          yelp_application_is_bookmarked        (YelpBookmarks     *bookmarks,
                                                         const gchar       *doc_uri,
                                                         const gchar       *page_id);
void              yelp_application_update_bookmarks     (YelpApplication   *app,
                                                         const gchar       *doc_uri,
                                                         const gchar       *page_id,
                                                         const gchar       *icon,
                                                         const gchar       *title);
GVariant *        yelp_application_get_bookmarks        (YelpApplication   *app,
                                                         const gchar       *doc_uri);
void              yelp_application_window_close_request (YelpApplication       *app,
                                                         GtkWindow             *window);

#endif /* __YELP_APPLICATION_H__ */
