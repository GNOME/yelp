/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003 Shaun McCance <shaunm@gnome.org>
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
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include "yelp-cache.h"

GHashTable *cache_table;
GMutex     *cache_mutex;

void
yelp_cache_init (void)
{
    cache_mutex = g_mutex_new ();
    cache_table = g_hash_table_new_full (g_str_hash,
					 g_str_equal,
					 g_free,
					 g_object_unref);
}

GObject *
yelp_cache_lookup (const gchar *path)
{
    GObject *object;

    g_mutex_lock (cache_mutex);

    object = (GObject *) g_hash_table_lookup (cache_table, path);

    g_mutex_unlock (cache_mutex);

    return object;
}

void
yelp_cache_add (const gchar *path, GObject *object)
{
    g_mutex_lock (cache_mutex);

    g_hash_table_insert (cache_table, (gchar *) path, object);

    g_mutex_unlock (cache_mutex);
}
