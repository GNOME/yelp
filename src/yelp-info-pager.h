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

#ifndef __YELP_INFO_PAGER_H__
#define __YELP_INFO_PAGER_H__

#include <glib-object.h>

#include "yelp-pager.h"
#include "yelp-xslt-pager.h"

#define YELP_TYPE_INFO_PAGER         (yelp_info_pager_get_type ())
#define YELP_INFO_PAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_INFO_PAGER, YelpInfoPager))
#define YELP_INFO_PAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_INFO_PAGER, YelpInfoPagerClass))
#define YELP_IS_INFO_PAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_INFO_PAGER))
#define YELP_IS_INFO_PAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_INFO_PAGER))
#define YELP_INFO_PAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_INFO_PAGER, YelpInfoPagerClass))

typedef struct _YelpInfoPager      YelpInfoPager;
typedef struct _YelpInfoPagerClass YelpInfoPagerClass;
typedef struct _YelpInfoPagerPriv  YelpInfoPagerPriv;

struct _YelpInfoPager {
    YelpXsltPager    parent;

    YelpInfoPagerPriv *priv;
};

struct _YelpInfoPagerClass {
    YelpXsltPagerClass   parent_class;
};

GType           yelp_info_pager_get_type     (void);
YelpPager *     yelp_info_pager_new          (YelpDocInfo *doc_info);

#endif /* __YELP_INFO_PAGER_H__ */
