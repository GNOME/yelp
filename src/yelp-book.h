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

#ifndef __YELP_BOOK_H__ 
#define __YELP_BOOK_H__ 

#include <glib-object.h>
#include <libgnomevfs/gnome-vfs.h>

#define YELP_TYPE_BOOK         (yelp_book_get_type ())
#define YELP_BOOK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_BOOK, YelpBook))
#define YELP_BOOK_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_BOOK, YelpBookClass))
#define YELP_IS_BOOK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_BOOK))
#define YELP_IS_BOOK_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_BOOK))
#define YELP_BOOK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_BOOK, YelpBookClass))

typedef struct _YelpBook      YelpBook;
typedef struct _YelpBookClass YelpBookClass;
typedef struct _YelpBookPriv  YelpBookPriv;
typedef struct _YelpSection   YelpSection;

struct _YelpBook {
	GObject       parent;

	GNode        *root;
	GSList       *keywords;
};

struct _YelpBookClass {
	GObjectClass  parent_class;

	/* Signals */
	/* enabled/disabled */
};

struct _YelpSection {
	gchar       *name;
	GnomeVFSURI *uri;
	gchar       *reference;
};

GType        yelp_book_get_type      (void);
YelpBook *   yelp_book_new           (const gchar        *name, 
				      GnomeVFSURI        *index_uri);

GNode *      yelp_book_add_section   (GNode              *parent,
				      const gchar        *name,
				      GnomeVFSURI        *uri,
				      const gchar        *reference);

#endif /* __YELP_BOOK_H__ */


