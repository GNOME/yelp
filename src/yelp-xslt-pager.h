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

#ifndef __YELP_XSLT_PAGER_H__
#define __YELP_XSLT_PAGER_H__

#include <glib-object.h>
#include <libxml/parser.h>

#include "yelp-pager.h"

#define YELP_TYPE_XSLT_PAGER         (yelp_xslt_pager_get_type ())
#define YELP_XSLT_PAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_XSLT_PAGER, YelpXsltPager))
#define YELP_XSLT_PAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_XSLT_PAGER, YelpXsltPagerClass))
#define YELP_IS_XSLT_PAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_XSLT_PAGER))
#define YELP_IS_XSLT_PAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_XSLT_PAGER))
#define YELP_XSLT_PAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_XSLT_PAGER, YelpXsltPagerClass))

typedef struct _YelpXsltPager      YelpXsltPager;
typedef struct _YelpXsltPagerClass YelpXsltPagerClass;
typedef struct _YelpXsltPagerPriv  YelpXsltPagerPriv;

struct _YelpXsltPager {
    YelpPager        parent;

    YelpXsltPagerPriv *priv;
};

struct _YelpXsltPagerClass {
    YelpPagerClass   parent_class;

    xmlDocPtr   (*parse)   (YelpPager *pager);
    gchar **    (*params)  (YelpPager *pager);

    gchar *stylesheet;
};

GType           yelp_xslt_pager_get_type     (void);

#endif /* __YELP_XSLT_PAGER_H__ */
