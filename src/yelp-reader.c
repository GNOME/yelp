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

#include "yelp-reader.h"

struct _YelpReaderPriv {
        YelpURI *current_uri;
        
};


static void reader_class_init (YelpReaderClass     *klass);
static void reader_init       (YelpReader          *reader);


GType
yelp_reader_get_type (void)
{
        static GType type = 0;

        if (!type) {
                static const GTypeInfo info =
                        {
                                sizeof (YelpReaderClass),
                                NULL,
                                NULL,
                                (GClassInitFunc) reader_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpReader),
                                0,
                                (GInstanceInitFunc) reader_init,
                        };
                
                type = g_type_register_static (G_TYPE_OBJECT,
                                               "YelpReader", 
                                               &info, 0);
        }
        
        return type;
}

static void
reader_class_init (YelpReaderClass *klass)
{
}

static void
reader_init (YelpReader *reader)
{
}

YelpReader *
yelp_reader_new (gboolean async)
{
        YelpReader *reader;
        
        reader = g_new0 (YelpReader, 1);
        
        return reader;
}

gboolean
yelp_reader_read (YelpURI *uri)
{
        return TRUE;
}


