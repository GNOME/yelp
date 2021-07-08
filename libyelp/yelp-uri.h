/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009 Shaun McCance  <shaunm@gnome.org>
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
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#ifndef __YELP_URI_H__
#define __YELP_URI_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define YELP_TYPE_URI         (yelp_uri_get_type ())
#define YELP_URI(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_URI, YelpUri))
#define YELP_URI_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_URI, YelpUriClass))
#define YELP_IS_URI(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_URI))
#define YELP_IS_URI_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_URI))
#define YELP_URI_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_URI, YelpUriClass))

typedef struct _YelpUri      YelpUri;
typedef struct _YelpUriClass YelpUriClass;

typedef enum {
    YELP_URI_DOCUMENT_TYPE_UNRESOLVED,
    YELP_URI_DOCUMENT_TYPE_DOCBOOK,
    YELP_URI_DOCUMENT_TYPE_MALLARD,
    YELP_URI_DOCUMENT_TYPE_MAN,
    YELP_URI_DOCUMENT_TYPE_INFO,
    YELP_URI_DOCUMENT_TYPE_TEXT,
    YELP_URI_DOCUMENT_TYPE_HTML,
    YELP_URI_DOCUMENT_TYPE_XHTML,
    YELP_URI_DOCUMENT_TYPE_HELP_LIST,
    YELP_URI_DOCUMENT_TYPE_NOT_FOUND,
    YELP_URI_DOCUMENT_TYPE_EXTERNAL,
    YELP_URI_DOCUMENT_TYPE_ERROR
} YelpUriDocumentType;

struct _YelpUri {
    GObject       parent;
};

struct _YelpUriClass {
    GObjectClass  parent_class;
};


GType                yelp_uri_get_type           (void);

YelpUri *            yelp_uri_new                (const gchar  *arg);
YelpUri *            yelp_uri_new_relative       (YelpUri      *base,
                                                  const gchar  *arg);
YelpUri *            yelp_uri_new_search         (YelpUri      *base,
                                                  const gchar  *text);

void                 yelp_uri_resolve            (YelpUri      *uri);
void                 yelp_uri_resolve_sync       (YelpUri      *uri);

gboolean             yelp_uri_is_resolved        (YelpUri      *uri);
YelpUriDocumentType  yelp_uri_get_document_type  (YelpUri      *uri);

/* Both of these functions return a non-null answer, provided that
 * the uri has been resolved. */
gchar *              yelp_uri_get_canonical_uri  (YelpUri      *uri);
gchar *              yelp_uri_get_document_uri   (YelpUri      *uri);

GFile *              yelp_uri_get_file           (YelpUri      *uri);

gchar **             yelp_uri_get_search_path    (YelpUri      *uri);
gchar *              yelp_uri_get_page_id        (YelpUri      *uri);
gchar *              yelp_uri_get_frag_id        (YelpUri      *uri);
gchar *              yelp_uri_get_query          (YelpUri      *uri,
                                                  const gchar  *key);

gchar *              yelp_uri_locate_file_uri    (YelpUri      *uri,
                                                  const gchar  *filename);

G_END_DECLS

#endif /* __YELP_URI_H__ */
