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

#ifndef __YELP_VIEW_INDEX_H__
#define __YELP_VIEW_INDEX_H__

#include <gtk/gtkhpaned.h>
#include <gtk/gtktreemodel.h>
#include "yelp-section.h"

#define YELP_TYPE_VIEW_INDEX        (yelp_view_index_get_type ())
#define YELP_VIEW_INDEX(o)          (GTK_CHECK_CAST ((o), YELP_TYPE_VIEW_INDEX, YelpViewIndex))
#define YELP_VIEW_INDEX_CLASS(k)    (GTK_CHECK_FOR_CAST((k), YELP_TYPE_VIEW_INDEX, YelpViewIndexClass))
#define YELP_IS_VIEW_INDEX(o)       (GTK_CHECK_TYPE ((o), YELP_TYPE_VIEW_INDEX))
#define YELP_IS_VIEW_INDEX_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), YELP_TYPE_VIEW_INDEX))

typedef struct _YelpViewIndex        YelpViewIndex;
typedef struct _YelpViewIndexClass   YelpViewIndexClass;
typedef struct _YelpViewIndexPriv    YelpViewIndexPriv;

struct _YelpViewIndex {
	GtkHPaned          parent;
	
	YelpViewIndexPriv *priv;
};

struct _YelpViewIndexClass {
        GtkHPanedClass    parent_class;

	/* Signals */

	void (*uri_selected)   (YelpViewIndex *view,
				YelpURI       *uri,
				gboolean       handled);
};

GType           yelp_view_index_get_type     (void);
GtkWidget      *yelp_view_index_new          (GList            *index);

void            yelp_view_index_show_uri     (YelpViewIndex    *view,
					      YelpURI          *uri,
					      GError          **error);

#endif /* __YELP_VIEW_INDEX__ */
