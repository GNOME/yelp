/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2010 Shaun McCance <shaunm@gnome.org>
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
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifndef __YELP_BOOKMARKS_H__
#define __YELP_BOOKMARKS_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define YELP_TYPE_BOOKMARKS               (yelp_bookmarks_get_type ())
#define YELP_BOOKMARKS(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), YELP_TYPE_BOOKMARKS, YelpBookmarks))
#define YELP_IS_BOOKMARKS(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), YELP_TYPE_BOOKMARKS))
#define YELP_BOOKMARKS_GET_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), YELP_TYPE_BOOKMARKS, YelpBookmarksInterface))

typedef struct _YelpBookmarks           YelpBookmarks;
typedef struct _YelpBookmarksInterface  YelpBookmarksInterface;

struct _YelpBookmarksInterface
{
    GTypeInterface g_iface;

    /* Signals */
    void      (* bookmarks_changed)     (YelpBookmarks *bookmarks);
    void      (* bookmark_added)        (YelpBookmarks *bookmarks,
                                         gchar *doc_uri,
                                         gchar *page_id);
    void      (* bookmark_removed)      (YelpBookmarks *bookmarks,
                                         gchar *doc_uri,
                                         gchar *page_id);

    /* Virtual Table */
    void      (* add_bookmark)          (YelpBookmarks *bookmarks,
                                         const gchar   *doc_uri,
                                         const gchar   *page_id,
                                         const gchar   *icon,
                                         const gchar   *title);
    void      (* remove_bookmark)       (YelpBookmarks *bookmarks,
                                         const gchar   *doc_uri,
                                         const gchar   *page_id);
    gboolean  (* is_bookmarked)         (YelpBookmarks *bookmarks,
                                         const gchar   *doc_uri,
                                         const gchar   *page_id);
};

GType    yelp_bookmarks_get_type        (void);
void     yelp_bookmarks_add_bookmark    (YelpBookmarks *bookmarks,
                                         const gchar   *doc_uri,
                                         const gchar   *page_id,
                                         const gchar   *icon,
                                         const gchar   *title);
void     yelp_bookmarks_remove_bookmark  (YelpBookmarks *bookmarks,
                                          const gchar   *doc_uri,
                                          const gchar   *page_id);
gboolean yelp_bookmarks_is_bookmarked    (YelpBookmarks *bookmarks,
                                          const gchar   *doc_uri,
                                          const gchar   *page_id);

G_END_DECLS

#endif /* __YELP_BOOKMARKS_H__ */
