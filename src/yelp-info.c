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

#include <yelp-info.h>
#include <yelp-util.h>
#include <libgnome/gnome-i18n.h>

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

static void
yelp_info_populate_tree_for_subdir (GtkTreeStore *store,
				    const char *basedir,
				    GtkTreeIter *parent)
{
	DIR *dirh;
	struct dirent *dent;

	dirh = opendir (basedir);
	if (!dirh)
		return;

	readdir (dirh);		/* skip . & .. */
	readdir (dirh);

	while ((dent = readdir (dirh))) {
		char *ctmp = NULL;
		char uribuf[128], titlebuf[128];

		if (dent->d_name[0] == '.')
			continue;

		do {
			if (ctmp)
				*ctmp = '\0';
			ctmp = strrchr (dent->d_name, '.');
		} while (ctmp && strcmp (ctmp, ".info"));

		if (!ctmp)
			continue;

		*ctmp = '\0';

		strcpy (titlebuf, dent->d_name);
		strcat (titlebuf, " (info)");

		g_snprintf (uribuf, sizeof (uribuf), "info:%s", dent->d_name);

		yelp_util_contents_add_section (store, parent, 
						yelp_section_new (titlebuf, uribuf, 
								  NULL, NULL));
	}

	closedir (dirh);
}

gboolean
yelp_info_init (GtkTreeStore *store)
{
	GtkTreeIter *root;

	root = yelp_util_contents_add_section (store, NULL, 
					       yelp_section_new (_("Info Pages"), NULL, 
								  NULL, NULL));

	
	yelp_info_populate_tree_for_subdir (store, "/usr/info", root);
	yelp_info_populate_tree_for_subdir (store, "/usr/share/info", root);

	return TRUE;
}
