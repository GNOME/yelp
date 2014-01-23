/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2007-2010 Shaun McCance <shaunm@gnome.org>
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
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifndef __YELP_MAN_DOCUMENT_H__
#define __YELP_MAN_DOCUMENT_H__

#include <glib-object.h>

#include "yelp-document.h"

#define YELP_TYPE_MAN_DOCUMENT         (yelp_man_document_get_type ())
#define YELP_MAN_DOCUMENT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_MAN_DOCUMENT, YelpManDocument))
#define YELP_MAN_DOCUMENT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_MAN_DOCUMENT, YelpManDocumentClass))
#define YELP_IS_MAN_DOCUMENT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_MAN_DOCUMENT))
#define YELP_IS_MAN_DOCUMENT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_MAN_DOCUMENT))
#define YELP_MAN_DOCUMENT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_MAN_DOCUMENT, YelpManDocumentClass))

typedef struct _YelpManDocument      YelpManDocument;
typedef struct _YelpManDocumentClass YelpManDocumentClass;

struct _YelpManDocument {
    YelpDocument      parent;
};

struct _YelpManDocumentClass {
    YelpDocumentClass parent_class;
};

GType           yelp_man_document_get_type     (void);
YelpDocument *  yelp_man_document_new          (YelpUri  *uri);

#endif /* __YELP_MAN_DOCUMENT_H__ */
