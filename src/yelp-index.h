/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 CodeFactory AB
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

#ifndef __YELP_INDEX_H__
#define __YELP_INDEX_H__

#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>

#define YELP_TYPE_INDEX		 (yelp_index_get_type ())
#define YELP_INDEX(obj)		 (GTK_CHECK_CAST ((obj), YELP_TYPE_INDEX, YelpIndex))
#define YELP_INDEX_CLASS(klass)	 (GTK_CHECK_CLASS_CAST ((klass), YELP_TYPE_INDEX, YelpIndexClass))
#define YELP_IS_INDEX(obj)	         (GTK_CHECK_TYPE ((obj), YELP_TYPE_INDEX))
#define YELP_IS_INDEX_CLASS(klass)      (GTK_CHECK_CLASS_TYPE ((obj), YELP_TYPE_INDEX))

typedef struct _YelpIndex       YelpIndex;
typedef struct _YelpIndexClass  YelpIndexClass;
typedef struct _YelpIndexPriv   YelpIndexPriv;

struct _YelpIndex
{
        GObject          parent;
        
        YelpIndexPriv   *priv;
};

struct _YelpIndexClass
{
        GObjectClass     parent_class;

        /* Signals */
	void  (*uri_selected)             (YelpIndex            *index,
					   GnomeVFSURI          *uri);
};

GType          yelp_index_get_type        (void);
YelpIndex *    yelp_index_new             (void);

GtkWidget *    yelp_index_get_entry       (YelpIndex            *index);
GtkWidget *    yelp_index_get_list        (YelpIndex            *index);

#endif /* __YELP_INDEX_H__ */
