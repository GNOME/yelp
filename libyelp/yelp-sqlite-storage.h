/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 
 * Copyright (C) 2011 Shaun McCance <shaunm@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifndef __YELP_SQLITE_STORAGE_H__
#define __YELP_SQLITE_STORAGE_H__

#include <glib-object.h>

#include "yelp-storage.h"

G_BEGIN_DECLS

#define YELP_TYPE_SQLITE_STORAGE         (yelp_sqlite_storage_get_type ())
#define YELP_SQLITE_STORAGE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_SQLITE_STORAGE, YelpSqliteStorage))
#define YELP_SQLITE_STORAGE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), YELP_TYPE_SQLITE_STORAGE, YelpSqliteStorageClass))
#define YELP_IS_SQLITE_STORAGE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_SQLITE_STORAGE))
#define YELP_IS_SQLITE_STORAGE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_SQLITE_STORAGE))
#define YELP_SQLITE_STORAGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_SQLITE_STORAGE, YelpSqliteStorageClass))

typedef struct _YelpSqliteStorage        YelpSqliteStorage;
typedef struct _YelpSqliteStorageClass   YelpSqliteStorageClass;

struct _YelpSqliteStorage {
    GObject parent_instance;
};

struct _YelpSqliteStorageClass {
    GObjectClass parent_class;
};

GType                 yelp_sqlite_storage_get_type    (void);

YelpStorage *         yelp_sqlite_storage_new         (const gchar   *filename);

G_END_DECLS

#endif /* __YELP_SQLITE_STORAGE_H__ */
