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

#ifndef __YELP_DOC_VIEW_H__
#define __YELP_DOC_VIEW_H__

#include <gtk/gtkobject.h>
#include <gtk/gtktypeutils.h>
#include <gtk/gtkmarshal.h>
#include <libgtkhtml/gtkhtml.h>
#include "yelp-section.h"

#define YELP_TYPE_DOC_VIEW    (yelp_doc_view_get_type ())
#define YELP_DOC_VIEW(o)      (GTK_CHECK_CAST ((o), YELP_TYPE_DOC_VIEW, YelpDocView))
#define YELP_DOC_VIEW_CLASS(k)(GTK_CHECK_FOR_CAST((k), YELP_TYPE_DOC_VIEW, YelpDocViewClass))
#define YELP_IS_DOC_VIEW(o)       (GTK_CHECK_TYPE ((o), YELP_TYPE_DOC_VIEW))
#define YELP_IS_DOC_VIEW_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), YELP_TYPE_DOC_VIEW))

typedef struct _YelpDocView        YelpDocView;
typedef struct _YelpDocViewClass   YelpDocViewClass;
typedef struct _YelpDocViewPriv    YelpDocViewPriv;

struct _YelpDocView {
	HtmlView          parent;
	
	YelpDocViewPriv  *priv;
};

struct _YelpDocViewClass {
        HtmlViewClass    parent_class;

	/* Signals */
	void (*uri_selected) (YelpDocView    *view,
			      const gchar *uri,
			      const gchar *anchor);
};

GType           yelp_doc_view_get_type       (void);
GtkWidget      *yelp_doc_view_new            (void);
 
#if 0
void            yelp_doc_view_open_uri       (YelpDocView         *view, 
					      const gchar         *uri);
#endif
 
void            yelp_doc_view_open_section   (YelpDocView         *view,
					      const YelpSection   *section);


#endif /* __YELP_DOC_VIEW_H__ */

