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

#ifndef __YELP_READER_H__
#define __YELP_READER_H__

#include <glib-object.h>

#include "yelp-uri.h"

#define YELP_TYPE_READER         (yelp_reader_get_type ())
#define YELP_READER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_READER, YelpReader))
#define YELP_READER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_READER, YelpReaderClass))
#define YELP_IS_READER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_READER))
#define YELP_IS_READER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_READER))
#define YELP_READER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_READER, YelpReaderClass))

typedef struct _YelpReader      YelpReader;
typedef struct _YelpReaderClass YelpReaderClass;
typedef struct _YelpReaderPriv  YelpReaderPriv;

struct _YelpReader {
        GObject         parent;
        
        YelpReaderPriv *priv;
};

struct _YelpReaderClass {
        GObjectClass    parent_class;

        /* Signals */
        void (*start)     (YelpReader         *reader);
        void (*data)      (YelpReader         *reader,
			   const gchar        *buffer,
			   gint                len);
        void (*finished)  (YelpReader         *reader);
	void (*cancelled) (YelpReader         *reader);
        void (*error)     (YelpReader         *reader,
			   GError             *error);
};

GType            yelp_reader_get_type     (void);
YelpReader *     yelp_reader_new          (gboolean    async);

void             yelp_reader_start        (YelpReader *reader,
					   YelpURI    *uri);
void             yelp_reader_cancel       (YelpReader *reader);

#endif /* __YELP_READER_H__ */
