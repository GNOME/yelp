/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Mikael Hallendal <micke@imendio.com>
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
 * Author: Mikael Hallendal <micke@imendio.com>
 */

#ifndef __YELP_CACHE_H__
#define __YELP_CACHE_H__

#include <glib.h>

#include "yelp-uri.h"

typedef struct _YelpNavLinks YelpNavLinks;

#define YELP_NAV_LINKS(x) ((YelpNavLinks *) x)

struct _YelpNavLinks {
	gchar *prev_link_uri;
	gchar *next_link_uri;
	gchar *prev_link_title;
	gchar *next_link_title;
	gchar *prev_link_text;
	gchar *next_link_text;
	gchar *up_link_uri;
	gchar *up_link_title;
};

void           yelp_cache_init          (void);

const gchar *  yelp_cache_lookup        (const gchar *path);

void           yelp_cache_add           (const gchar *path,
                                         const gchar *html);

YelpNavLinks * yelp_cache_lookup_links  (const gchar        *path);

void           yelp_cache_add_links     (const gchar        *path,
					 const YelpNavLinks *links);

#endif /* __YELP_CACHE_H__ */
