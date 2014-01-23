/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2011 Shaun McCance  <shaunm@gnome.org>
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#ifndef __YELP_STORAGE_H__
#define __YELP_STORAGE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define YELP_TYPE_STORAGE             (yelp_storage_get_type ())
#define YELP_STORAGE(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_STORAGE, YelpStorage))
#define YELP_IS_STORAGE(o)            (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_STORAGE))
#define YELP_STORAGE_GET_INTERFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), YELP_TYPE_STORAGE, YelpStorageInterface))

typedef struct _YelpStorage          YelpStorage;
typedef struct _YelpStorageInterface YelpStorageInterface;

struct _YelpStorageInterface {
    GTypeInterface base_iface;

    void          (*update)         (YelpStorage   *storage,
                                     const gchar   *doc_uri,
                                     const gchar   *full_uri,
                                     const gchar   *title,
                                     const gchar   *desc,
                                     const gchar   *icon,
                                     const gchar   *text);
    GVariant *    (*search)         (YelpStorage   *storage,
                                     const gchar   *doc_uri,
                                     const gchar   *text);
    gchar *       (*get_root_title) (YelpStorage   *storage,
                                     const gchar   *doc_uri);
    void          (*set_root_title) (YelpStorage   *storage,
                                     const gchar   *doc_uri,
                                     const gchar   *title);
};

GType             yelp_storage_get_type       (void);

void              yelp_storage_set_default    (YelpStorage   *storage);
YelpStorage *     yelp_storage_get_default    (void);

void              yelp_storage_update         (YelpStorage   *storage,
                                               const gchar   *doc_uri,
                                               const gchar   *full_uri,
                                               const gchar   *title,
                                               const gchar   *desc,
                                               const gchar   *icon,
                                               const gchar   *text);
GVariant *        yelp_storage_search         (YelpStorage   *storage,
                                               const gchar   *doc_uri,
                                               const gchar   *text);
gchar *           yelp_storage_get_root_title (YelpStorage   *storage,
                                               const gchar   *doc_uri);
void              yelp_storage_set_root_title (YelpStorage   *storage,
                                               const gchar   *doc_uri,
                                               const gchar   *title);

G_END_DECLS

#endif /* __YELP_STORAGE_H__ */
