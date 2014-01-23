/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2009 Shaun McCance  <shaunm@gnome.org>
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

#ifndef __YELP_DOCBOOK_DOCUMENT_H__
#define __YELP_DOCBOOK_DOCUMENT_H__

#include <glib-object.h>

#include "yelp-document.h"

#define YELP_TYPE_DOCBOOK_DOCUMENT         (yelp_docbook_document_get_type ())
#define YELP_DOCBOOK_DOCUMENT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_DOCBOOK_DOCUMENT, YelpDocbookDocument))
#define YELP_DOCBOOK_DOCUMENT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_DOCBOOK_DOCUMENT, YelpDocbookDocumentClass))
#define YELP_IS_DOCBOOK_DOCUMENT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_DOCBOOK_DOCUMENT))
#define YELP_IS_DOCBOOK_DOCUMENT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_DOCBOOK_DOCUMENT))
#define YELP_DOCBOOK_DOCUMENT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_DOCBOOK_DOCUMENT, YelpDocbookDocumentClass))

typedef struct _YelpDocbookDocument      YelpDocbookDocument;
typedef struct _YelpDocbookDocumentClass YelpDocbookDocumentClass;

struct _YelpDocbookDocument {
    YelpDocument      parent;
};

struct _YelpDocbookDocumentClass {
    YelpDocumentClass parent_class;
};

GType           yelp_docbook_document_get_type     (void);
YelpDocument *  yelp_docbook_document_new          (YelpUri  *uri);

#endif /* __YELP_DOCBOOK_DOCUMENT_H__ */
