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
        cache_table = g_hash_table_new (g_str_hash, g_str_equal);
}

const gchar *
yelp_cache_lookup (const gchar *path)
{ 
        const gchar *ret_val;
        
        g_mutex_lock (cache_mutex);
        
        ret_val = (const gchar *) g_hash_table_lookup (cache_table, path);
        
        g_mutex_unlock (cache_mutex);

        return ret_val;
}

void
yelp_cache_add (const gchar *path, const gchar *html)
{
        g_mutex_lock (cache_mutex);
        
        g_hash_table_insert (cache_table, (gchar *) path, g_strdup (html));
        
        g_mutex_unlock (cache_mutex);
}
