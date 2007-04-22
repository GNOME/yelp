/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2007 Shaun McCance  <shaunm@gnome.org>
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

#ifndef __YELP_DOCBOOK_H__
#define __YELP_DOCBOOK_H__

#include <glib-object.h>

#include "yelp-document.h"

#define YELP_TYPE_DOCBOOK         (yelp_docbook_get_type ())
#define YELP_DOCBOOK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_DOCBOOK, YelpDocbook))
#define YELP_DOCBOOK_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_DOCBOOK, YelpDocbookClass))
#define YELP_IS_DOCBOOK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_DOCBOOK))
#define YELP_IS_DOCBOOK_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_DOCBOOK))
#define YELP_DOCBOOK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_DOCBOOK, YelpDocbookClass))

typedef struct _YelpDocbook      YelpDocbook;
typedef struct _YelpDocbookClass YelpDocbookClass;
typedef struct _YelpDocbookPriv  YelpDocbookPriv;

struct _YelpDocbook {
    YelpDocument      parent;
    YelpDocbookPriv  *priv;
};

struct _YelpDocbookClass {
    YelpDocumentClass parent_class;
};

GType           yelp_docbook_get_type     (void);
YelpDocument *  yelp_docbook_new          (gchar  *uri);

#endif /* __YELP_DOCBOOK_H__ */
