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

#ifndef __YELP_KEYWORD_DB_H__
#define __YELP_KEYWORD_DB_H__

#include <gobject/gobject.h>
#include <libgnomevfs/gnome-vfs.h>

#define YELP_TYPE_KEYWORD_DB         (yelp_keyword_db_get_type ())
#define YELP_KEYWORD_DB(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_KEYWORD_DB, YelpKeywordDb))
#define YELP_KEYWORD_DB_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_KEYWORD_DB, YelpKeywordDbClass))
#define YELP_IS_KEYWORD_DB(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_KEYWORD_DB))
#define YELP_IS_KEYWORD_DB_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_KEYWORD_DB))
#define YELP_KEYWORD_DB_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_KEYWORD_DB, YelpKeywordDbClass))

typedef struct _YelpKeywordDb      YelpKeywordDb;
typedef struct _YelpKeywordDbClass YelpKeywordDbClass;
typedef struct _YelpKeywordDbPriv  YelpKeywordDbPriv;
typedef struct _YelpKeyword        YelpKeyword;

struct _YelpKeywordDb {
	GObject        parent;

	YelpKeywordDbPriv *priv;
};

struct _YelpKeywordDbClass {
	GObjectClass   parent_class;
	
};

struct _YelpKeyword {
	gchar       *name;
	GnomeVFSURI *uri;
};

GType           yelp_keyword_db_get_type        (void);
YelpKeywordDb * yelp_keyword_db_new             (void);

void            yelp_keyword_db_add_keywords    (YelpKeywordDb     *db,
						 GSList            *keywords);

GSList *        yelp_keyword_db_search          (YelpKeywordDb     *db,
						 const gchar       *str);

gchar *         yelp_keyword_db_get_completion  (YelpKeywordDb     *db,
						 const gchar       *str);

YelpKeyword *   yelp_keyword_db_new_keyword     (const gchar       *name,
						 const GnomeVFSURI *base_uri,
						 const gchar       *link);

#endif /* __YELP_KEYWORD_H__ */
