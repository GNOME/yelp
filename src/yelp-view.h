/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Mikael Hallendal <micke@codefactory.se>
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

#ifndef __YELP_INDEX_H__
#define __YELP_INDEX_H__

#include <gtk/gtkhpaned.h>
#include <gtk/gtktreemodel.h>
#include "yelp-section.h"
#include "yelp-html.h"

#define YELP_TYPE_VIEW         (yelp_view_get_type ())
#define YELP_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_VIEW, YelpView))
#define YELP_VIEW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), YELP_TYPE_VIEW, YelpViewClass))
#define YELP_IS_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_VIEW))
#define YELP_IS_VIEW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_VIEW))
#define YELP_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_VIEW, YelpViewClass))

typedef struct _YelpView        YelpView;
typedef struct _YelpViewClass   YelpViewClass;
typedef struct _YelpViewPriv    YelpViewPriv;

struct _YelpView {
	GObject    parent;

	GtkWidget *widget;
};

struct _YelpViewClass {
        GObjectClass       parent_class;

	/* Signals */
	void (*uri_selected)   (YelpView      *view,
				YelpURI       *uri,
				gboolean       handled);
	void (*title_changed)  (YelpView      *view,
				const gchar   *new_title);

	/* Virtual functions */
	void        (*show_uri)       (YelpView      *view,
				       YelpURI       *uri,
				       GError      **error);
	YelpHtml *  (*get_html)       (YelpView      *view);
};

GType           yelp_view_get_type     (void);

void            yelp_view_show_uri     (YelpView  *view,
					YelpURI   *uri,
					GError   **error);

YelpHtml *      yelp_view_get_html     (YelpView  *view);

#endif /* __YELP_INDEX__ */
