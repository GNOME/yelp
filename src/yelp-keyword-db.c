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

#include "yelp-keyword-db.h"

static void     hkd_init                 (YelpKeywordDb        *db);
static void     hkd_class_init           (YelpKeywordDbClass   *klass);

static gchar *  hkd_complete_cb          (gpointer            data);

struct _YelpKeywordDbPriv {
	GSList      *keywords;
	GCompletion *keywords_completion;
};

GType
yelp_keyword_db_get_type (void)
{
        static GType db_type = 0;

        if (!db_type) {
                static const GTypeInfo db_info = {
                        sizeof (YelpKeywordDbClass),
                        NULL,
                        NULL,
                        (GClassInitFunc) hkd_class_init,
                        NULL,
                        NULL,
                        sizeof (YelpKeywordDb),
                        0,
                        (GInstanceInitFunc) hkd_init,
                };
                
                db_type = g_type_register_static (G_TYPE_OBJECT,
						  "YelpKeywordDb", 
						  &db_info, 0);
        }
        
        return db_type;
}

static void
hkd_init (YelpKeywordDb *db)
{
	YelpKeywordDbPriv *priv;
	
	priv = g_new0 (YelpKeywordDbPriv, 1);
	priv->keywords = NULL;
	priv->keywords_completion = g_completion_new (hkd_complete_cb);

	db->priv = priv;
}

static void
hkd_class_init (YelpKeywordDbClass *klass)
{
}

static gchar *
hkd_complete_cb (gpointer data)
{
	YelpKeyword *keyword;
	
	g_return_val_if_fail (data != NULL, NULL);
	
	keyword = (YelpKeyword *) data;
	
	return keyword->name;
}

YelpKeywordDb *
yelp_keyword_db_new (void)
{
	YelpKeywordDb *db;
	
	db = g_object_new (YELP_TYPE_KEYWORD_DB, NULL);

	return db;
}

static gint
hkd_keyword_compare (gconstpointer a,
		     gconstpointer b)
{
	YelpKeyword  *ka, *kb;
	
	ka = (YelpKeyword *) ((GSList *) a)->data;
	kb = (YelpKeyword *) ((GSList *) b)->data;
	
	return strcmp (ka->name, kb->name);
}

void
yelp_keyword_db_add_keywords (YelpKeywordDb *db, GSList *keywords)
{
}

GSList *
yelp_keyword_db_search (YelpKeywordDb *db, const gchar *str)
{
	YelpKeywordDbPriv *priv;
	GSList            *hit_list = NULL;
	GSList            *node;
	YelpKeyword       *keyword;
	
	g_return_val_if_fail (YELP_IS_KEYWORD_DB (db), NULL);
	g_return_val_if_fail (str != NULL, NULL);

	priv = db->priv;

	for (node = priv->keywords; node; node = node->next) {
		keyword = (YelpKeyword *) node->data;
		
		hit_list = g_slist_prepend (hit_list, keyword);
	}

	hit_list = g_slist_sort (hit_list, hkd_keyword_compare);
	
	return hit_list;
}

gchar *
yelp_keyword_db_get_completion (YelpKeywordDb *db, const gchar *str)
{
	YelpKeywordDbPriv *priv;
	gchar             *completion;
	
	g_return_val_if_fail (YELP_IS_KEYWORD_DB (db), NULL);
	g_return_val_if_fail (str != NULL, NULL);
	
	priv = db->priv;
	
	g_completion_complete (priv->keywords_completion, 
			       (gchar *) str,
			       &completion);

	return completion;
}

YelpKeyword *
yelp_keyword_db_new_keyword (const gchar       *name,
			     const GnomeVFSURI *base_uri,
			     const gchar       *link)
{
	YelpKeyword *keyword;
	
	keyword = g_new0 (YelpKeyword, 1);
	
	keyword->name = g_strdup (name);
	keyword->uri  = gnome_vfs_uri_append_path (base_uri, link);
	
	return keyword;
}
