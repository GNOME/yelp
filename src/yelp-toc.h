/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2007 Don Scorgie <Don@Scorgie.org>
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
 * Author: Don Scorgie <Don@Scorgie.org>
 */

#ifndef __YELP_TOC_H__
#define __YELP_TOC_H__

#include <glib-object.h>

#include "yelp-document.h"

#define YELP_TYPE_TOC         (yelp_toc_get_type ())
#define YELP_TOC(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_TOC, YelpToc))
#define YELP_TOC_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_TOC, YelpTocClass))
#define YELP_IS_TOC(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_TOC))
#define YELP_IS_TOC_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_TOC))
#define YELP_TOC_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_TOC, YelpTocClass))

typedef struct _YelpToc      YelpToc;
typedef struct _YelpTocClass YelpTocClass;
typedef struct _YelpTocPriv  YelpTocPriv;

struct _YelpToc {
    YelpDocument      parent;
    YelpTocPriv  *priv;
};

struct _YelpTocClass {
    YelpDocumentClass parent_class;
};

GType           yelp_toc_get_type     (void);
YelpDocument *  yelp_toc_new          (void);
YelpDocument *  yelp_toc_get          (void);

#endif /* __YELP_TOC_H__ */
