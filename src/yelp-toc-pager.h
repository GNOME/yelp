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

#ifndef __YELP_TOC_PAGER_H__
#define __YELP_TOC_PAGER_H__

#include <glib-object.h>

#include "yelp-pager.h"

#define YELP_TYPE_TOC_PAGER         (yelp_toc_pager_get_type ())
#define YELP_TOC_PAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_TOC_PAGER, YelpTocPager))
#define YELP_TOC_PAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_TOC_PAGER, YelpTocPagerClass))
#define YELP_IS_TOC_PAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_TOC_PAGER))
#define YELP_IS_TOC_PAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_TOC_PAGER))
#define YELP_TOC_PAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_TOC_PAGER, YelpTocPagerClass))

typedef struct _YelpTocPager      YelpTocPager;
typedef struct _YelpTocPagerClass YelpTocPagerClass;
typedef struct _YelpTocPagerPriv  YelpTocPagerPriv;

struct _YelpTocPager {
    YelpPager         parent;

    YelpTocPagerPriv *priv;
};

struct _YelpTocPagerClass {
    YelpPagerClass    parent_class;
};

GType           yelp_toc_pager_get_type     (void);

void            yelp_toc_pager_init         (void);
YelpTocPager *  yelp_toc_pager_get          (void);

void            yelp_toc_pager_pause        (YelpTocPager *pager);
void            yelp_toc_pager_unpause      (YelpTocPager *pager);

#endif /* __YELP_TOC_PAGER_H__ */
