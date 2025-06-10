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

#include "yelp-bookmarks.h"

enum {
    BOOKMARKS_CHANGED,
    BOOKMARK_ADDED,
    BOOKMARK_REMOVED,
    LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_INTERFACE (YelpBookmarks, yelp_bookmarks, G_TYPE_OBJECT)

static void
yelp_bookmarks_default_init (YelpBookmarksInterface *iface)
{
    signals[BOOKMARKS_CHANGED] =
        g_signal_new ("bookmarks-changed",
                      G_TYPE_FROM_INTERFACE (iface),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE, 1, G_TYPE_STRING);

    signals[BOOKMARK_ADDED] =
        g_signal_new ("bookmark-added",
                      G_TYPE_FROM_INTERFACE (iface),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      NULL,
                      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);

    signals[BOOKMARK_REMOVED] =
        g_signal_new ("bookmark-removed",
                      G_TYPE_FROM_INTERFACE (iface),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      NULL,
                      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
}

void
yelp_bookmarks_add_bookmark (YelpBookmarks *bookmarks,
                             const gchar   *doc_uri,
                             const gchar   *page_id,
                             const gchar   *icon,
                             const gchar   *title)
{
    YelpBookmarksInterface *iface;

    g_return_if_fail (YELP_IS_BOOKMARKS (bookmarks));

    iface = YELP_BOOKMARKS_GET_INTERFACE (bookmarks);

    if (iface->add_bookmark)
        (* iface->add_bookmark) (bookmarks,
                                 doc_uri, page_id,
                                 icon, title);
}

void
yelp_bookmarks_remove_bookmark (YelpBookmarks *bookmarks,
                                const gchar   *doc_uri,
                                const gchar   *page_id)
{
    YelpBookmarksInterface *iface;

    g_return_if_fail (YELP_IS_BOOKMARKS (bookmarks));

    iface = YELP_BOOKMARKS_GET_INTERFACE (bookmarks);

    if (iface->remove_bookmark)
        (* iface->remove_bookmark) (bookmarks, doc_uri, page_id);
}

gboolean
yelp_bookmarks_is_bookmarked (YelpBookmarks *bookmarks,
                              const gchar   *doc_uri,
                              const gchar   *page_id)
{
    YelpBookmarksInterface *iface;

    g_return_val_if_fail (YELP_IS_BOOKMARKS (bookmarks), FALSE);

    iface = YELP_BOOKMARKS_GET_INTERFACE (bookmarks);

    if (iface->is_bookmarked)
        return (* iface->is_bookmarked) (bookmarks, doc_uri, page_id);
    else
        return FALSE;
}
