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

#include <yelp-man.h>
#include <yelp-util.h>
#include <libgnome/gnome-i18n.h>

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

struct TreeNode {
	char *name;

	GList *tree_nodes;

	GList *pages; /* YelpSections */
};

struct TreeData {
	char *name;
	struct TreeData *children;
	char *section;
};

/* Caller must free this */
static char *
extract_secnum_from_filename (const char *filename)
{
	char *end_string = NULL;
	char *start;
	int len;

#ifdef HAVE_LIBBZ2
	end_string =  g_strrstr (filename, ".bz2");
#endif
	if (end_string == NULL)
		end_string =  g_strrstr (filename, ".gz");

	if (end_string) {
		len = end_string - filename;
	} else {
		len = strlen (filename);
	}

	start = g_strrstr_len (filename, len, ".");

	if (start) {
		start++;
		len -= start - filename;
		return g_strndup (start, len);
	} else {
		return NULL;
	}
	
}

/* Caller must free this */
static char *
man_name_without_suffix (const char *filename)
{
	char *end_string = NULL;
	char *start;
	int len;

#ifdef HAVE_LIBBZ2
	end_string =  g_strrstr (filename, ".bz2");
#endif
	if (end_string == NULL)
		end_string =  g_strrstr (filename, ".gz");

	if (end_string) {
		len = end_string - filename;
	} else {
		len = strlen (filename);
	}

	start = g_strrstr_len (filename, len, ".");

	return g_strndup (filename, start - filename);
}

static void
yelp_man_populate_tree_for_subdir (GHashTable *section_hash,
				   const char *basedir,
				   char secnum)
{
	DIR *dirh;
	struct dirent *dent;
	char uribuf[128], titlebuf[128];
	struct TreeNode *node;

	dirh = opendir (basedir);
	if (!dirh)
		return;

	readdir (dirh);		/* skip . & .. */
	readdir (dirh);

	while ((dent = readdir (dirh))) {
		char *section;
		char *manname;
		char *filename;

		if (dent->d_name[0] == '.')
			continue;

		section = extract_secnum_from_filename (dent->d_name);
		if (!section)
			continue;

		if (section[0] != secnum) {
			g_free (section);
			continue;
		}

		manname = man_name_without_suffix (dent->d_name);
		filename = g_build_filename (basedir, dent->d_name, NULL);

		g_snprintf (titlebuf, sizeof (titlebuf), "%s (%s)", manname, section);
		g_snprintf (uribuf, sizeof (uribuf), "man:%s", filename);

		node = g_hash_table_lookup (section_hash, section);
		if (node == NULL) {
			char buf[2];
			buf[0] = secnum; buf[1] = 0;

			node = g_hash_table_lookup (section_hash, buf);
		}

		g_assert (node != NULL);
		
		node->pages = g_list_prepend (node->pages, 
					      yelp_section_new (YELP_SECTION_DOCUMENT,
								titlebuf, uribuf, 
								NULL, NULL));
		g_free (manname);
		g_free (section);
		g_free (filename);
	}

	closedir (dirh);
}

static void
yelp_man_populate_tree_for_dir (GHashTable *section_hash,
				const char *basedir)
{
	char cbuf[1024];

	g_snprintf (cbuf, sizeof (cbuf), "%s/man1", basedir);
	yelp_man_populate_tree_for_subdir (section_hash, cbuf, '1');

	g_snprintf (cbuf, sizeof (cbuf), "%s/man2", basedir);
	yelp_man_populate_tree_for_subdir (section_hash, cbuf, '2');

	g_snprintf (cbuf, sizeof (cbuf), "%s/man3", basedir);
	yelp_man_populate_tree_for_subdir (section_hash, cbuf, '3');

	g_snprintf (cbuf, sizeof (cbuf), "%s/man4", basedir);
	yelp_man_populate_tree_for_subdir (section_hash, cbuf, '4');

	g_snprintf (cbuf, sizeof (cbuf), "%s/man5", basedir);
	yelp_man_populate_tree_for_subdir (section_hash, cbuf, '5');

	g_snprintf (cbuf, sizeof (cbuf), "%s/man6", basedir);
	yelp_man_populate_tree_for_subdir (section_hash, cbuf, '6');

	g_snprintf (cbuf, sizeof (cbuf), "%s/man7", basedir);
	yelp_man_populate_tree_for_subdir (section_hash, cbuf, '7');

	g_snprintf (cbuf, sizeof (cbuf), "%s/man8", basedir);
	yelp_man_populate_tree_for_subdir (section_hash, cbuf, '8');

	g_snprintf (cbuf, sizeof (cbuf), "%s/man9", basedir);
	yelp_man_populate_tree_for_subdir (section_hash, cbuf, '9');
}

/** From 'man(7)':

       The manual sections are traditionally defined as follows:

              1 Commands
                      Those  commands that can be executed by the
                      user from within a shell.

              2 System calls
                      Those functions which must be performed  by
                      the kernel.

              3 Library calls
                      Most   of   the  libc  functions,  such  as
                      sort(3))

              4 Special files
                      Files found in /dev)

              5 File formats and conventions
                      The format for /etc/passwd and other human-
                      readable files.

              6 Games

              7 Macro packages and conventions
                      A  description  of the standard file system
                      layout, this man page, and other things.

              8 System management commands
                      Commands like mount(8), which only root can
                      execute.

              9 Kernel routines
                      This  is  a non-standard manual section and
                      is included because the source code to  the
                      Linux  kernel is freely available under the
                      GNU Public  License  and  many  people  are
                      working on changes to the kernel)

Looking at my RH 7.2 machine i also have the following subsections
	1m   Termcap / Terminfo
	1ssl OpenSSL
	1x   X11
	3pm  Perl
	3qt  Qt
	3ssl OpenSSL
	3t   TIFF
	3thr Pthread
	3x   X11
	4x   X11
	5ssl OpenSSL
	5x   X11
	6x   X11
	7ssl OpenSSL
	7x   X11
***/

struct TreeData application_children[] = {
	{
		N_("X11"),
		NULL,
		"1x"
	},
	{
		N_("OpenSSL"),
		NULL,
		"1ssl"
	},
	{
		N_("Termcap"),
		NULL,
		"1m"
	},
	{
		NULL
	}
};

struct TreeData library_children[] = {
	{
		N_("X11"),
		NULL,
		"3x"
	},
	{
		N_("Perl"),
		NULL,
		"3pm"
	},
	{
		N_("Qt"),
		NULL,
		"3qt"
	},
	{
		N_("OpenSSL"),
		NULL,
		"3ssl"
	},
	{
		N_("TIFF"),
		NULL,
		"3t"
	},
	{
		N_("PThreads"),
		NULL,
		"3thr"
	},
	{
		NULL
	}
};

struct TreeData devices_children[] = {
	{
		N_("X11"),
		NULL,
		"4x"
	},
	{
		NULL
	}
};

struct TreeData development_children[] = {
	{
		N_("System Calls"),
		NULL,
		"2"
	},
	{
		N_("Library Functions"),
		library_children,
		"3"
	},
	{
		N_("Kernel Routines"),
		NULL,
		"9"
	},
	{
		NULL
	}
};

struct TreeData configuration_children[] = {
	{
		N_("X11"),
		NULL,
		"5x"
	},
	{
		N_("OpenSSL"),
		NULL,
		"5ssl"
	},
	{
		NULL
	}
};

struct TreeData games_children[] = {
	{
		N_("X11"),
		NULL,
		"6x"
	},
	{
		NULL
	}
};

struct TreeData conventions_children[] = {
	{
		N_("X11"),
		NULL,
		"7x"
	},
	{
		N_("OpenSSL"),
		NULL,
		"7ssl"
	},
	{
		NULL
	}
};


struct TreeData root_children[] = {
	{
		N_("Applications"),
		application_children,
		"1"
	},
	{
		N_("Development"),
		development_children,
		NULL
	},
	{
		N_("Hardware Devices"),
		devices_children,
		"4"
	},
	{
		N_("Configuration Files"),
		configuration_children,
		"5"
	},
	{
		N_("Games"),
		games_children,
		"6"
	},
	{
		N_("Overviews"),
		conventions_children,
		"7"
	},
	{
		N_("System Administration"),
		NULL,
		"8"
	},
	{
		NULL
	}
};


struct TreeData root_data = {
	N_("man"),
	root_children,
	NULL
};

static struct TreeNode *
yelp_man_make_initial_tree (struct TreeData *data,
			    GHashTable *section_hash)
{
	struct TreeNode *node;
	struct TreeNode *child_node;
	struct TreeData *child;

	node = g_new (struct TreeNode, 1);
	node->name = _(data->name);
	node->tree_nodes = NULL;
	node->pages = NULL;
	
	if (data->section) {
		g_hash_table_insert (section_hash,
				     data->section,
				     node);
	}

	if (data->children) {
		for (child = data->children; child->name != NULL; child++) {
			child_node = yelp_man_make_initial_tree (child, section_hash);
			node->tree_nodes = g_list_append (node->tree_nodes, child_node);
		}
	}

	return node;
}

static void
yelp_man_cleanup_initial_tree (struct TreeNode *node)
{
	GList *l;
	struct TreeNode *child;

	g_list_foreach (node->tree_nodes,
			(GFunc)yelp_man_cleanup_initial_tree,
			NULL);
	
	l = node->tree_nodes;
	while (l) {
		child = l->data;
		l = l->next;
		
		if (child->pages == NULL && child->tree_nodes == NULL)
			node->tree_nodes = g_list_remove (node->tree_nodes, child);
	}

	node->pages = g_list_sort (node->pages, (GCompareFunc)yelp_section_compare);
}

static void
yelp_man_free_initial_tree (struct TreeNode *node)
{
	g_list_foreach (node->tree_nodes,
			(GFunc)yelp_man_free_initial_tree,
			NULL);

	g_list_free (node->tree_nodes);
	g_list_free (node->pages);
}

static void
yelp_man_push_initial_tree (struct TreeNode *node, GtkTreeStore *store, GtkTreeIter *parent)
{
	struct TreeNode *child;
	YelpSection *page;
	GtkTreeIter *iter;
	GList *l;

	iter = yelp_util_contents_add_section (store, parent, 
					       yelp_section_new (YELP_SECTION_CATEGORY,
								 node->name, NULL, 
								 NULL, NULL));
	
	l = node->tree_nodes;
	while (l) {
		child = l->data;
		l = l->next;

		yelp_man_push_initial_tree (child, store, iter);
	}
	
	l = node->pages;
	while (l) {
		page = l->data;
		l = l->next;

		yelp_util_contents_add_section (store, iter, page);
	}
}

gboolean
yelp_man_init (GtkTreeStore *store)
{
	FILE *fh;
	char aline[1024];
	char **manpath = NULL;
	int i;
	GHashTable *section_hash;
	struct TreeNode *root;

	/* Go through all the man pages:
	 * 1. Determine the places to search (run 'manpath').
	 * 2. Go through all subdirectories to find individual files.
	 * 3. For each file, add it onto the tree at the right place.
	 */

	section_hash = g_hash_table_new (g_str_hash, g_str_equal);
	
	root = yelp_man_make_initial_tree (&root_data, section_hash);

	fh = popen ("manpath", "r");
	g_return_val_if_fail (fh, FALSE);

	if (fgets (aline, sizeof (aline), fh)) {
		g_strstrip (aline);
		manpath = g_strsplit (aline, ":", -1);
	} else {
		g_warning ("Couldn't get manpath");
	}
	pclose (fh);

	i = 0;
	if (manpath) {
		for (; manpath[i]; i++)
			yelp_man_populate_tree_for_dir (section_hash, manpath[i]);
	}
	if (!manpath || !manpath[0]) {
		yelp_man_populate_tree_for_dir (section_hash, "/usr/man");
		yelp_man_populate_tree_for_dir (section_hash, "/usr/share/man");
	}


	yelp_man_cleanup_initial_tree (root);
	
	yelp_man_push_initial_tree (root, store, NULL);
	
	yelp_man_free_initial_tree (root);
	g_hash_table_destroy (section_hash);
	
	return TRUE;
}
