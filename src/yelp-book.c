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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "yelp-book.h"

static void    yelp_book_init           (YelpBook           *book);
static void    yelp_book_class_init     (YelpBookClass      *klass);

GType
yelp_book_get_type (void)
{
        static GType book_type = 0;

        if (!book_type) {
                static const GTypeInfo book_info = {
                        sizeof (YelpBookClass),
                        NULL,
                        NULL,
                        (GClassInitFunc) yelp_book_class_init,
                        NULL,
                        NULL,
                        sizeof (YelpBook),
                        0,
                        (GInstanceInitFunc) yelp_book_init,
                };
                
                book_type = g_type_register_static (G_TYPE_OBJECT,
						    "YelpBook", 
						    &book_info, 0);
        }
        
        return book_type;
}

static void
yelp_book_init (YelpBook *book)
{
	book->keywords = NULL;
}

static void
yelp_book_class_init (YelpBookClass *klass)
{

}

YelpBook *
yelp_book_new (const gchar *name, const gchar *index_uri)
{
	YelpBook *book;
	
	book       = g_object_new (YELP_TYPE_BOOK, NULL);
	book->root = yelp_book_add_section (NULL, name, index_uri, NULL);
	
	return book;
}

static YelpSection *
yelp_book_section_new (const gchar *name,
		       const gchar *uri,
		       const gchar *reference)
{
	YelpSection *section;

	g_return_val_if_fail (name != NULL, NULL);
	
	section = g_new0 (YelpSection, 1);
	
	section->name      = g_strdup (name);
	section->uri       = g_strdup (uri);
	section->reference = g_strdup (reference);
	
	return section;
}

GNode *
yelp_book_add_section (GNode       *parent,
		       const gchar *name,
		       const gchar *uri,
		       const gchar *reference)
{
	GNode *node;
	
	g_return_val_if_fail (name != NULL, NULL);

/* 	g_print ("Adding section: %s\n", name); */
	
	node = g_node_new (yelp_book_section_new (name, uri, reference));

	if (!parent) {
		return node;
	}

	return g_node_insert (parent, -1, node);
}
