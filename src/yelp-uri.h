/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Mikael Hallendal <micke@codefactory.se>
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
 * Author: Mikael Hallendal <micke@codefactory.se>
 */

#ifndef __YELP_URI_H__
#define __YELP_URI_H__

#include <glib.h>

#define YELP_URI(x) ((YelpURI *) x)

typedef enum {
	YELP_URI_TYPE_NON_EXISTENT,
        YELP_URI_TYPE_UNKNOWN,
        YELP_URI_TYPE_DOCBOOK_XML,
	YELP_URI_TYPE_DOCBOOK_SGML,
        YELP_URI_TYPE_HTML,
        YELP_URI_TYPE_MAN,
        YELP_URI_TYPE_INFO,
	YELP_URI_TYPE_TOC
} YelpURIType;

typedef struct _YelpURI YelpURI;

typedef int (*YelpURIReaderOpenCallback)  (gpointer       user_data,
					   GError        *error);
typedef int (*YelpURIReaderReadCallback)  (gpointer       user_data,
					   const gchar   *buffer,
					   gint           len,
					   GError        *error);
typedef int (*YelpURIReaderCloseCallback) (gpointer       user_data,
					   GError        *error);
typedef struct {
        YelpURIReaderOpenCallback  open_callback;
        YelpURIReaderReadCallback  read_callback;
        YelpURIReaderCloseCallback close_callback;
        gpointer                   user_data;
} YelpURIReader;

YelpURI *       yelp_uri_new         (const gchar                 *str_uri);
gboolean        yelp_uri_exists      (YelpURI                     *uri);

YelpURIType     yelp_uri_get_type    (YelpURI                     *uri);
const gchar *   yelp_uri_get_path    (YelpURI                     *uri);
const gchar *   yelp_uri_get_section (YelpURI                     *uri);

gboolean        yelp_uri_read        (YelpURI                     *uri,
				      YelpURIReader               *reader,
				      GError                     **error);

gboolean        yelp_uri_read_async  (YelpURI                     *uri,
				      YelpURIReader               *reader,
				      GError                     **error);

void            yelp_uri_ref         (YelpURI                     *uri);
void            yelp_uri_unref       (YelpURI                     *uri);


/* Convenience function for creating a Reader-struct. */
YelpURIReader * yelp_uri_reader_new  (YelpURIReaderOpenCallback    open_cb,
				      YelpURIReaderReadCallback    read_cb,
				      YelpURIReaderCloseCallback   close_cb,
				      gpointer                     user_data);

#endif /* __YELP_URI_H__ */
