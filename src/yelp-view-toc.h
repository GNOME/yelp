/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@codefactory.se>
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

#ifndef __YELP_VIEW_TOC_H__
#define __YELP_VIEW_TOC_H__

#include <gtk/gtktreemodel.h>
#include <gtk/gtkwidget.h>

#include "yelp-section.h"
#include "yelp-uri.h"

#define YELP_TYPE_VIEW_TOC    (yelp_view_toc_get_type ())
#define YELP_VIEW_TOC(o)      (GTK_CHECK_CAST ((o), YELP_TYPE_VIEW_TOC, YelpViewTOC))
#define YELP_VIEW_TOC_CLASS(k)(GTK_CHECK_FOR_CAST((k), YELP_TYPE_VIEW_TOC, YelpViewTOCClass))
#define YELP_IS_VIEW_TOC(o)       (GTK_CHECK_TYPE ((o), YELP_TYPE_VIEW_TOC))
#define YELP_IS_VIEW_TOC_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), YELP_TYPE_VIEW_TOC))

typedef struct _YelpViewTOC        YelpViewTOC;
typedef struct _YelpViewTOCClass   YelpViewTOCClass;
typedef struct _YelpViewTOCPriv    YelpViewTOCPriv;

struct _YelpViewTOC {
	GObject          parent;

	YelpViewTOCPriv *priv;
};

struct _YelpViewTOCClass {
	GObjectClass     parent_class;

	/* Signals */

	void (*uri_selected)   (YelpViewTOC   *view,
				YelpURI       *uri,
				gboolean       handled);
};

GType           yelp_view_toc_get_type       (void);
YelpViewTOC    *yelp_view_toc_new            (GNode        *doc_tree);
void            yelp_view_toc_open_uri       (YelpViewTOC  *view,
					      YelpURI      *uri);
GtkWidget      *yelp_view_toc_get_widget     (YelpViewTOC  *view);

#endif /* __YELP_VIEW_TOC__ */
