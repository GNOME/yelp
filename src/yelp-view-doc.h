/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Mikael Hallendal <micke@codefactory.se>
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

#ifndef __YELP_VIEW_DOC_H__
#define __YELP_VIEW_DOC_H__

#include <gtk/gtkobject.h>
#include <gtk/gtktypeutils.h>
#include <gtk/gtkmarshal.h>
#include <libgtkhtml/gtkhtml.h>
#include "yelp-section.h"

#define YELP_TYPE_VIEW_DOC    (yelp_view_doc_get_type ())
#define YELP_VIEW_DOC(o)      (GTK_CHECK_CAST ((o), YELP_TYPE_VIEW_DOC, YelpViewDoc))
#define YELP_VIEW_DOC_CLASS(k)(GTK_CHECK_FOR_CAST((k), YELP_TYPE_VIEW_DOC, YelpViewDocClass))
#define YELP_IS_VIEW_DOC(o)       (GTK_CHECK_TYPE ((o), YELP_TYPE_VIEW_DOC))
#define YELP_IS_VIEW_DOC_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), YELP_TYPE_VIEW_DOC))

typedef struct _YelpViewDoc        YelpViewDoc;
typedef struct _YelpViewDocClass   YelpViewDocClass;
typedef struct _YelpViewDocPriv    YelpViewDocPriv;

struct _YelpViewDoc {
	HtmlView          parent;
	
	YelpViewDocPriv  *priv;
};

struct _YelpViewDocClass {
        HtmlViewClass    parent_class;

	/* Signals */
	void (*uri_selected) (YelpViewDoc    *view,
			      const gchar    *uri,
			      const gchar    *anchor);
};

GType           yelp_view_doc_get_type       (void);
GtkWidget      *yelp_view_doc_new            (void);
 
void            yelp_view_doc_open_section   (YelpViewDoc         *view,
					      const YelpSection   *section);

#endif /* __YELP_VIEW_DOC_H__ */

