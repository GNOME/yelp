/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2007 Don Scorgie <Don@Scorgie.org>
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
 * Author: Don Scorgie <Don@Scorgie.org>
 */

#ifndef __YELP_SEARCH_H__
#define __YELP_SEARCH_H__

#include <glib-object.h>

#include "yelp-document.h"

#define YELP_TYPE_SEARCH         (yelp_search_get_type ())
#define YELP_SEARCH(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_SEARCH, YelpSearch))
#define YELP_SEARCH_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_SEARCH, YelpSearchClass))
#define YELP_IS_SEARCH(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_SEARCH))
#define YELP_IS_SEARCH_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_SEARCH))
#define YELP_SEARCH_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_SEARCH, YelpSearchClass))

typedef struct _YelpSearch      YelpSearch;
typedef struct _YelpSearchClass YelpSearchClass;
typedef struct _YelpSearchPriv  YelpSearchPriv;

struct _YelpSearch {
    YelpDocument      parent;
    YelpSearchPriv  *priv;
};

struct _YelpSearchClass {
    YelpDocumentClass parent_class;
};

GType           yelp_search_get_type     (void);
YelpDocument *  yelp_search_new          (gchar  *uri);

#endif /* __YELP_SEARCH_H__ */
