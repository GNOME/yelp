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

#ifndef __YELP_SECTION_H__ 
#define __YELP_SECTION_H__ 

#include <glib-object.h>
#include <libgnomevfs/gnome-vfs.h>

typedef struct _YelpSection   YelpSection;

struct _YelpSection {
	gchar *name;
	gchar *uri;
	gchar *reference;
};

YelpSection * yelp_section_new      (const gchar        *name,
				     const gchar        *uri,
				     const gchar        *reference);

GNode *       yelp_section_add_sub  (GNode              *parent,
				     YelpSection        *section);

YelpSection * yelp_section_copy     (const YelpSection  *section);

void          yelp_section_free     (YelpSection        *section);

#endif /* __YELP_BOOK_H__ */


