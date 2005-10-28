/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003 Shaun McCance  <shaunm@gnome.org>
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
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#ifndef __YELP_SEARCH_PAGER_H__
#define __YELP_SEARCH_PAGER_H__

#include <glib-object.h>

#include "yelp-pager.h"

#define YELP_TYPE_SEARCH_PAGER         (yelp_search_pager_get_type ())
#define YELP_SEARCH_PAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_SEARCH_PAGER, YelpSearchPager))
#define YELP_SEARCH_PAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_SEARCH_PAGER, YelpSearchPagerClass))
#define YELP_IS_SEARCH_PAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_SEARCH_PAGER))
#define YELP_IS_SEARCH_PAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_SEARCH_PAGER))
#define YELP_SEARCH_PAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_SEARCH_PAGER, YelpSearchPagerClass))

typedef struct _YelpSearchPager      YelpSearchPager;
typedef struct _YelpSearchPagerClass YelpSearchPagerClass;
typedef struct _YelpSearchPagerPriv  YelpSearchPagerPriv;

struct _YelpSearchPager {
    YelpPager         parent;

    YelpSearchPagerPriv *priv;
};

struct _YelpSearchPagerClass {
    YelpPagerClass    parent_class;
};

GType            yelp_search_pager_get_type     (void);

void             yelp_search_pager_init         (void);
YelpSearchPager *yelp_search_pager_get          (YelpDocInfo *doc_info);

#endif /* __YELP_SEARCH_PAGER_H__ */
