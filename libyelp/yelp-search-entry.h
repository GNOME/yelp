/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009-2014 Shaun McCance <shaunm@gnome.org>
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

#ifndef __YELP_SEARCH_ENTRY_H__
#define __YELP_SEARCH_ENTRY_H__

#include <gtk/gtk.h>

#include "yelp-bookmarks.h"
#include "yelp-view.h"

G_BEGIN_DECLS

#define YELP_TYPE_SEARCH_ENTRY (yelp_search_entry_get_type ())
#define YELP_SEARCH_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), YELP_TYPE_SEARCH_ENTRY, \
			      YelpSearchEntry))
#define YELP_SEARCH_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), YELP_TYPE_SEARCH_ENTRY, \
			   YelpSearchEntryClass))
#define YELP_IS_SEARCH_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), YELP_TYPE_SEARCH_ENTRY))
#define YELP_IS_SEARCH_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), YELP_TYPE_SEARCH_ENTRY))
#define YELP_SEARCH_ENTRY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), YELP_TYPE_SEARCH_ENTRY, \
			     YelpSearchEntryClass))

/**
 * YelpSearchEntry:
 **/
typedef struct _YelpSearchEntry         YelpSearchEntry;
typedef struct _YelpSearchEntryClass    YelpSearchEntryClass;

struct _YelpSearchEntry
{
    GtkEntry parent;
};

struct _YelpSearchEntryClass
{
    GtkEntryClass parent;

    void     (* search_activated)           (YelpSearchEntry *entry);
    void     (* bookmark_clicked)           (YelpSearchEntry *entry);

    /* Padding for future expansion */
    void (*_gtk_reserved0) (void);
    void (*_gtk_reserved1) (void);
    void (*_gtk_reserved2) (void);
    void (*_gtk_reserved3) (void);
};

GType           yelp_search_entry_get_type          (void);
GtkWidget *     yelp_search_entry_new               (YelpView           *window,
						     YelpBookmarks      *bookmarks);

G_END_DECLS

#endif /* __YELP_SEARCH_ENTRY_H__ */
