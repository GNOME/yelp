/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef __YELP_LOCATION_ENTRY_H__
#define __YELP_LOCATION_ENTRY_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define YELP_TYPE_LOCATION_ENTRY (yelp_location_entry_get_type ())
#define YELP_LOCATION_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), YELP_TYPE_LOCATION_ENTRY, \
			      YelpLocationEntry))
#define YELP_LOCATION_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), YELP_TYPE_LOCATION_ENTRY, \
			   YelpLocationEntryClass))
#define YELP_IS_LOCATION_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), YELP_TYPE_LOCATION_ENTRY))
#define YELP_IS_LOCATION_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), YELP_TYPE_LOCATION_ENTRY))
#define YELP_LOCATION_ENTRY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), YELP_TYPE_LOCATION_ENTRY, \
			     YelpLocationEntryClass))

/**
 * YelpLocationEntry:
 **/
typedef struct _YelpLocationEntry         YelpLocationEntry;
typedef struct _YelpLocationEntryClass    YelpLocationEntryClass;

struct _YelpLocationEntry
{
    GtkComboBoxEntry          parent;
};

struct _YelpLocationEntryClass
{
    GtkComboBoxEntryClass     parent;
};

/**
 * YelpLocationEntryFlags:
 * @YELP_LOCATION_ENTRY_CAN_BOOKMARK: This location can be bookmarked.  When a
 * bookmarkable location is selected, the secondary icon of the embedded text
 * entry will be a clickable bookmark icon.
 * @YELP_LOCATION_ENTRY_IS_LOADING: Page data for this location is still loading.
 * The #YelpLocationEntry widget will display an indeterminate progress indicator.
 * @YELP_LOCATION_ENTRY_IS_SEPARATOR: This row should be displayed as a separator.
 * @YELP_LOCATION_ENTRY_IS_SEARCH: Selecting this row initiates a search instead
 * of selecting a location.
 *
 * Flags which can be used to provide additional information about rows
 * to be displayed by a #YelpLocationEntry.
 **/
typedef enum {
    YELP_LOCATION_ENTRY_CAN_BOOKMARK  = 1 << 0,
    YELP_LOCATION_ENTRY_IS_BOOKMARKED = 1 << 1,
    YELP_LOCATION_ENTRY_IS_LOADING    = 1 << 2,
    YELP_LOCATION_ENTRY_IS_SEPARATOR  = 1 << 3,
    YELP_LOCATION_ENTRY_IS_SEARCH     = 1 << 4
} YelpLocationEntryFlags;

GType           yelp_location_entry_get_type             (void);
GtkWidget*      yelp_location_entry_new_with_model       (GtkTreeModel      *model,
                                                          gint               text_column,
                                                          gint               desc_column,
                                                          gint               icon_column,
                                                          gint               flags_column);

void            yelp_location_entry_set_completion_model (YelpLocationEntry *entry,
                                                          GtkTreeModel      *model,
                                                          gint               text_column,
                                                          gint               desc_column,
                                                          gint               icon_column,
                                                          gint               flags_column);

void            yelp_location_entry_start_search         (YelpLocationEntry  *entry);

G_END_DECLS

#endif /* __YELP_LOCATION_ENTRY_H__ */
