/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Red Hat Inc.
 * Copyright (C) 2000 Sun Microsystems, Inc. 
 * Copyright (C) 2001 Eazel, Inc.
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

	dirh = opendir (basedir);
	if (!dirh) {
		return;
	}

	readdir (dirh);		/* skip . & .. */
	readdir (dirh);

	while ((dent = readdir (dirh))) {
		char *ctmp = NULL;
		char uribuf[128], titlebuf[128];

		if (dent->d_name[0] == '.') {
			continue;
		}

		do {
			if (ctmp) {
				*ctmp = '\0';
			}
			
			ctmp = strrchr (dent->d_name, '.');
		} while (ctmp && strcmp (ctmp, ".info"));

		if (!ctmp) {
			continue;
		}

		*ctmp = '\0';

		strcpy (titlebuf, dent->d_name);
		strcat (titlebuf, " (info)");

		g_snprintf (uribuf, sizeof (uribuf), "info:%s", dent->d_name);

		section = yelp_section_new (YELP_SECTION_DOCUMENT,
					    titlebuf, uribuf, 
					    NULL, NULL);
		
		*info_list = g_slist_prepend (*info_list, section);
						      
	}

	closedir (dirh);
}

gboolean
yelp_info_init (GNode *tree)
{
	GNode       *root;
	struct stat  stat_dir1;
	struct stat  stat_dir2;
	GSList      *info_list = NULL;
	GSList      *node;
	
	root = g_node_append_data (tree, 
				   yelp_section_new (YELP_SECTION_CATEGORY,
						     _("info"), NULL, 
						     NULL, NULL));

	stat ("/usr/info", &stat_dir1);
	stat ("/usr/share/info", &stat_dir2);
	
	yelp_info_read_info_dir ("/usr/info", &info_list);
	
	if (stat_dir1.st_ino != stat_dir2.st_ino) {
		yelp_info_read_info_dir  ("/usr/share/info", &info_list);
	}

	info_list = g_slist_sort (info_list, yelp_section_compare);

	for (node = info_list; node; node = node->next) {
		g_node_append_data (root, node->data);
	}

	g_slist_free (info_list);

	return TRUE;
}
