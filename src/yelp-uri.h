/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2003 Shaun McCance <shaunm@gnome.org>
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
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifndef __YELP_URI_H__
#define __YELP_URI_H__

#include <libgnomevfs/gnome-vfs.h>

typedef enum {
    YELP_URI_TYPE_ERROR = 0,

    YELP_URI_TYPE_DOCBOOK_XML,
    YELP_URI_TYPE_DOCBOOK_SGML,
    YELP_URI_TYPE_HTML,
    YELP_URI_TYPE_MAN,
    YELP_URI_TYPE_INFO,
    YELP_URI_TYPE_TOC,
    YELP_URI_TYPE_EXTERNAL
} YelpURIType;

typedef struct _YelpURI YelpURI;
struct _YelpURI {
    GnomeVFSURI *uri;

    gint         refcount;

    gchar       *src_uri;

    YelpURIType  resource_type;
};

YelpURI *     yelp_uri_new                (const gchar   *uri_str);
YelpURI *     yelp_uri_resolve_relative   (YelpURI       *uri,
					   const gchar   *uri_str);
YelpURIType   yelp_uri_get_resource_type  (YelpURI       *uri);

YelpURI *     yelp_uri_ref                (YelpURI       *uri);
void          yelp_uri_unref              (YelpURI       *uri);

#endif /* __YELP_URI_H__ */
