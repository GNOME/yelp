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

#ifndef __YELP_VIEW_CONTENT_H__
#define __YELP_VIEW_CONTENT_H__

#include <gtk/gtkhpaned.h>
#include <gtk/gtktreemodel.h>
#include "yelp-section.h"

#define YELP_TYPE_VIEW_CONTENT        (yelp_view_content_get_type ())
#define YELP_VIEW_CONTENT(o)          (GTK_CHECK_CAST ((o), YELP_TYPE_VIEW_CONTENT, YelpViewContent))
#define YELP_VIEW_CONTENT_CLASS(k)    (GTK_CHECK_FOR_CAST((k), YELP_TYPE_VIEW_CONTENT, YelpViewContentClass))
#define YELP_IS_VIEW_CONTENT(o)       (GTK_CHECK_TYPE ((o), YELP_TYPE_VIEW_CONTENT))
#define YELP_IS_VIEW_CONTENT_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), YELP_TYPE_VIEW_CONTENT))

typedef struct _YelpViewContent        YelpViewContent;
typedef struct _YelpViewContentClass   YelpViewContentClass;
typedef struct _YelpViewContentPriv    YelpViewContentPriv;

struct _YelpViewContent {
	GtkHPaned         parent;
	
	YelpViewContentPriv *priv;
};

struct _YelpViewContentClass {
        GtkHPanedClass    parent_class;

	/* Signals */
	void (*url_selected)   (YelpViewContent *view,
				gchar           *url,
				gchar           *base_url,
				gboolean         handled);

	void (*title_changed)  (YelpViewContent *view,
				const gchar     *title);

        /* Signal when icon is clicked. */
};

GType           yelp_view_content_get_type     (void);
GtkWidget      *yelp_view_content_new          (GNode             *doc_tree);

#if 0
void            yelp_view_content_show_path    (YelpViewContent   *content,
						GtkTreePath       *path);
#endif
void            yelp_view_content_show_uri     (YelpViewContent   *content,
						const gchar       *uri);

#if 0
gboolean        yelp_view_content_set_root     (YelpViewContent   *content,
						GtkTreePath       *path);
#endif 
#endif /* __YELP_VIEW_CONTENT__ */
