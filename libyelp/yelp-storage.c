/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2011 Shaun McCance <shaunm@gnome.org>
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
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#include "yelp-storage.h"
#include "yelp-sqlite-storage.h"

G_DEFINE_INTERFACE (YelpStorage, yelp_storage, G_TYPE_OBJECT)

static YelpStorage *default_storage;

static void
yelp_storage_default_init (YelpStorageInterface *iface)
{
    default_storage = NULL;
}

void
yelp_storage_set_default (YelpStorage *storage)
{
    default_storage = g_object_ref (storage);
}

YelpStorage *
yelp_storage_get_default (void)
{
    static GMutex mutex;
    g_mutex_lock (&mutex);
    if (default_storage == NULL)
        default_storage = yelp_sqlite_storage_new (":memory:");
    g_mutex_unlock (&mutex);
    return default_storage;
}

void
yelp_storage_update (YelpStorage   *storage,
                     const gchar   *doc_uri,
                     const gchar   *full_uri,
                     const gchar   *title,
                     const gchar   *desc,
                     const gchar   *icon,
                     const gchar   *text)
{
    YelpStorageInterface *iface;

    g_return_if_fail (YELP_IS_STORAGE (storage));

    iface = YELP_STORAGE_GET_INTERFACE (storage);

    if (iface->update)
        (*iface->update) (storage,
                          doc_uri, full_uri,
                          title, desc, icon,
                          text);
}

GVariant *
yelp_storage_search (YelpStorage   *storage,
                     const gchar   *doc_uri,
                     const gchar   *text)
{
    YelpStorageInterface *iface;

    g_return_val_if_fail (YELP_IS_STORAGE (storage), NULL);

    iface = YELP_STORAGE_GET_INTERFACE (storage);

    if (iface->search)
        return (*iface->search) (storage, doc_uri, text);
    else
        return NULL;
}

gchar *
yelp_storage_get_root_title (YelpStorage *storage,
                             const gchar *doc_uri)
{
    YelpStorageInterface *iface;

    g_return_val_if_fail (YELP_IS_STORAGE (storage), NULL);

    iface = YELP_STORAGE_GET_INTERFACE (storage);

    if (iface->search)
        return (*iface->get_root_title) (storage, doc_uri);
    else
        return NULL;
}

void
yelp_storage_set_root_title (YelpStorage *storage,
                             const gchar *doc_uri,
                             const gchar *title)
{
    YelpStorageInterface *iface;

    g_return_if_fail (YELP_IS_STORAGE (storage));

    iface = YELP_STORAGE_GET_INTERFACE (storage);

    if (iface->search)
        (*iface->set_root_title) (storage, doc_uri, title);
}
