/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Red Hat Inc.
 * Copyright (C) 2000 Sun Microsystems, Inc. 
 * Copyright (C) 2001 Eazel, Inc.
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
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 * This code was based on the hyperbola code in nautilus.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnome/gnome-i18n.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>

#include "yelp-section.h"
#include "yelp-util.h"
#include "yelp-info.h"


static void
yelp_info_read_info_dir (const char *basedir, GSList **info_list)
{
	DIR           *dirh;
	struct dirent *dent;
	YelpSection   *section;
	YelpURI       *uri;
	GHashTable    *dup_hash;

	dirh = opendir (basedir);
	if (!dirh) {
		return;
	}

	dup_hash = g_hash_table_new_full (g_str_hash, 
					  g_str_equal,
					  g_free,
					  NULL);

	while ((dent = readdir (dirh))) {
		char *ch = NULL;
		char *name = NULL;
		char *str_uri, *title;

		if (dent->d_name[0] == '.') {
			continue;
		}
		
		if ((ch = strrchr (dent->d_name, '-')) &&
		    ch[1] &&
		    isdigit (ch[1])) {
			continue;
		}
		    
		if ((ch = strstr (dent->d_name, ".info"))) {
			name = g_strndup (dent->d_name, ch - dent->d_name);
		} else {
			while ((ch = strrchr (dent->d_name, '.'))) {
				*ch = '\0';
			}
			name = g_strdup (dent->d_name);
		}

		if (g_hash_table_lookup_extended (dup_hash, name, NULL, NULL)) {
			g_free (name);
			continue;
		}
	       
		title = g_strdup_printf ("%s (info)", name);

		str_uri = g_strdup_printf ("info:%s", name);
		uri = yelp_uri_new (str_uri);
		g_free (str_uri);
		
		g_hash_table_insert (dup_hash, name, NULL);

		section = yelp_section_new (YELP_SECTION_DOCUMENT,
					    title, uri);
		g_free (title);
		
		yelp_uri_unref (uri);
		
		*info_list = g_slist_prepend (*info_list, section);
	}

	g_hash_table_destroy (dup_hash);

	closedir (dirh);
}

gboolean
yelp_info_init (GNode *tree, GList **index)
{
	GNode        *root;
	struct stat   stat_dir1;
	struct stat   stat_dir2;
	GSList       *info_list = NULL;
	GSList       *node;
	gchar       **infopathes;
	gchar        *infopath;
	gint          i;
	
	infopath = g_strdup (g_getenv ("INFOPATH"));
	
	if (infopath) {
		g_strstrip (infopath);
		infopathes = g_strsplit (infopath, ":", -1);
		g_free (infopath);
		
		for (i = 0; infopathes[i]; i++) {
			yelp_info_read_info_dir (infopathes[i], &info_list);
		}
	} else {
		stat ("/usr/info", &stat_dir1);
		stat ("/usr/share/info", &stat_dir2);
	
		yelp_info_read_info_dir ("/usr/info", &info_list);
	
		if (stat_dir1.st_ino != stat_dir2.st_ino) {
			yelp_info_read_info_dir  ("/usr/share/info", &info_list);
		}
	}
	
	if (g_slist_length (info_list) <= 0) {
		return FALSE;
	}

	root = g_node_append_data (tree,
				   yelp_section_new (YELP_SECTION_CATEGORY,
						     "info", NULL));

	info_list = g_slist_sort (info_list, yelp_section_compare);

	for (node = info_list; node; node = node->next) {
		g_node_append_data (root, node->data);

		*index = g_list_prepend (*index, node->data);
	}

	g_slist_free (info_list);

	return TRUE;
}
	
