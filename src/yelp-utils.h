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

#ifndef __YELP_UTILS_H__
#define __YELP_UTILS_H__

typedef struct _YelpDocumentInfo YelpDocumentInfo;
typedef struct _YelpDocumentPage YelpDocumentPage;

typedef enum {
    YELP_TYPE_ERROR = 0,
    YELP_TYPE_DOCBOOK_XML,
    YELP_TYPE_DOCBOOK_SGML,
    YELP_TYPE_HTML,
    YELP_TYPE_MAN,
    YELP_TYPE_INFO,
    YELP_TYPE_TOC,
    YELP_TYPE_EXTERNAL
} YelpDocumentType;

struct _YelpDocumentInfo {
    gchar *uri;

    YelpDocumentType type;
};

struct _YelpDocumentPage {
    YelpDocumentInfo *document;
    gchar *page_id;
    gchar *title;
    gchar *contents;

    gchar *prev_id;
    gchar *next_id;
    gchar *toc_id;
};

YelpDocumentInfo *   yelp_document_info_new     (gchar            *uri);
YelpDocumentInfo *   yelp_document_info_get     (gchar            *uri);
void                 yelp_document_info_free    (YelpDocumentInfo *doc);

void                 yelp_document_page_free    (YelpDocumentPage *page);

#endif /* __YELP_PAGER_H__ */
