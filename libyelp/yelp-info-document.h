/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2007 Don Scorgie <dscorgie@svn.gnome.org>
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
 * Author: Don Scorgie <dscorgie@svn.gnome.org>
 */

#ifndef __YELP_INFO_DOCUMENT_H__
#define __YELP_INFO_DOCUMENT_H__

#include <glib-object.h>

#include "yelp-document.h"

#define YELP_TYPE_INFO_DOCUMENT         (yelp_info_document_get_type ())
#define YELP_INFO_DOCUMENT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_INFO_DOCUMENT, YelpInfoDocument))
#define YELP_INFO_DOCUMENT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_INFO_DOCUMENT, YelpInfoDocumentClass))
#define YELP_IS_INFO_DOCUMENT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_INFO_DOCUMENT))
#define YELP_IS_INFO_DOCUMENT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_INFO_DOCUMENT))
#define YELP_INFO_DOCUMENT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_INFO_DOCUMENT, YelpInfoDocumentClass))

typedef struct _YelpInfoDocument      YelpInfoDocument;
typedef struct _YelpInfoDocumentClass YelpInfoDocumentClass;

struct _YelpInfoDocument {
    YelpDocument      parent;
};

struct _YelpInfoDocumentClass {
    YelpDocumentClass parent_class;
};

GType           yelp_info_document_get_type     (void);
YelpDocument *  yelp_info_document_new          (YelpUri  *uri);

#endif /* __YELP_INFO_DOCUMENT_H__ */
