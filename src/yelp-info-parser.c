/*
 * Copyright (C) 2005 Davyd Madeley  <davyd@madeley.id.au>
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
 * Author: Davyd Madeley  <davyd@madeley.id.au>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>

#include "yelp-io-channel.h"
#include "yelp-info-parser.h"
#include "yelp-utils.h"

/* Part 1: Parse File Into Tree Store */

enum
{
	PAGE_TAG_TABLE,
	PAGE_NODE,
	PAGE_INDIRECT,
	PAGE_OTHER
};

enum
{
	COLUMN_PAGE_NO,
	COLUMN_PAGE_NAME,
	COLUMN_PAGE_CONTENT,
	
	N_COLUMNS
};

static int
page_type (char *page)
{
	if (strncmp (page, "Tag Table:\n", 11) == 0)
		return PAGE_TAG_TABLE;
	else if (strncmp (page, "Indirect:\n", 10) == 0)
		return PAGE_INDIRECT;
	else if (strncmp (page, "File: ", 6) == 0)
		return PAGE_NODE;
	else
		return PAGE_OTHER;
}

static char
*open_info_file (char *file)
{
	GIOChannel *channel;
	int i;
	int len;
	char *str;

	g_print ("!! Opening %s...\n", file);
	
	channel = yelp_io_channel_new_file (file, NULL);
	g_io_channel_read_to_end (channel, &str, &len, NULL);
	g_io_channel_shutdown (channel, FALSE, NULL);

	for (i = 0; i < len - 1; i++)
	{
		if (str[i] == '\0' && str[i+1] == '\b')
		{
			g_print ("=> got a NULL, replacing\n");
			str[i] = ' '; str[i+1] = ' ';
		}
	}

	return str;
}

static char
*process_indirect_map (char *page)
{
	char **lines;
	char **ptr;
	char *composite;
	
	lines = g_strsplit (page, "\n", 0);
	composite = NULL;

	for (ptr = lines + 1; *ptr != NULL; ptr++);
	for (ptr--; ptr != lines; ptr--)
	{
		char **items;
		char *filename;
		char *str;
		char **pages;
		int offset;
		int plength;

		g_print ("Line: %s\n", *ptr);
		items = g_strsplit (*ptr, ": ", 2);

		if (items[0])
		{
			filename = g_strdup_printf (
					/* FIXME */
					"/usr/share/info/%s.gz", items[0]);
			str = open_info_file (filename);
	
			pages = g_strsplit (str, "", 2);
			g_free (str);

			offset =  atoi(items[1]);
			plength = strlen(pages[1]);
			
			g_print ("Need to make string %s+%i bytes = %i\n",
					items[1], plength,
					offset + plength);
			
			if (!composite) /* not yet created, malloc it */
			{
				int length;

				length = offset + plength;
				composite = g_malloc (sizeof (char) *
						length + 1);
				memset (composite, '-', length);
				composite[length + 1] = '\0';
			}
			composite[offset] = '';
			memcpy (composite + offset + 1, pages[1], plength);
			
			g_free (filename);
			g_strfreev (pages);
		}
		
		g_strfreev (items);
	}

	g_strfreev (lines);

	return composite;
}

static GHashTable
*process_tag_table (char *page)
{
	/* Let's assume we've been passed a valid page */

	GHashTable *table;
	char **lines;
	char **ptr;
	char **items;

	table = g_hash_table_new (g_str_hash, g_str_equal);
	lines = g_strsplit (page, "\n", 0);

	for (ptr = lines; *ptr != NULL; ptr++)
	{
		if (strncmp (*ptr, "Node: ", 6) == 0)
		{
			items = g_strsplit (*ptr, "", 2);
			g_print ("Node: %s Offset: %s\n",
					items[0] + 6, items[1]);
			g_hash_table_insert (table,
					g_strdup (items[0] + 6),
					g_strdup (items[1]));
			g_strfreev (items);
		}
	}

	g_strfreev (lines);

	return table;
}

static char
*get_value_after (char *source, char *required)
{
	char *ret, *ret_cp;
	char *source_cp;
	char *ptr;

	source_cp = g_strdup (source);
	
	ptr = g_strstr_len (source_cp, strlen (source_cp), required);
	if (!ptr)
		return NULL;
	ret = ptr + strlen (required);
	ptr = g_strstr_len (ret, strlen (ret), ",");
	/* if there is no pointer, we're at the end of the string */
	if (ptr)
		ret_cp = g_strndup (ret, ptr - ret);
	else
		ret_cp = g_strdup (ret);

	g_free (source_cp);

	return ret_cp;
}

static int
node2page (GHashTable *nodes2offsets, GHashTable *offsets2pages, char *node)
{
	char *offset;

	offset = g_hash_table_lookup (nodes2offsets, node);
	return GPOINTER_TO_INT (g_hash_table_lookup (offsets2pages, offset));
}

static GtkTreeIter
*node2iter (GHashTable *nodes2iters, char *node)
{
	GtkTreeIter *iter;
	
	iter = g_hash_table_lookup (nodes2iters, node);
	if (!iter) g_print ("Could not retrieve iter for node %s\n", node);
	return iter;
}

static void
process_page (GtkTreeStore *tree, GHashTable *nodes2offsets,
		GHashTable *offsets2pages, GHashTable *nodes2iters,
		int *processed_table, char **page_list, char *page_text)
{
	GtkTreeIter *iter;
	
	char **parts;
	char *node;
	char *up;
	char *prev;
	char *next;

	int page;
	
	/* split out the header line and the text */
	parts = g_strsplit (page_text, "\n", 3);

	node = get_value_after (parts[0], "Node: ");
	up = get_value_after (parts[0], "Up: ");
	prev = get_value_after (parts[0], "Prev: ");
	next = get_value_after (parts[0], "Next: ");

	/* check to see if this page has been processed already */
	page = node2page (nodes2offsets, offsets2pages, node);
	if (processed_table[page])
		return;
	processed_table[page] = 1;
	
	g_print ("-- Processing Page %s\n\tParent: %s\n", node, up);

	iter = g_malloc0 (sizeof (GtkTreeIter));
	/* check to see if we need to process our parent and siblings */
	if (up && strcmp (up, "(dir)") && strcmp (up, "Top"))
	{
		page = node2page (nodes2offsets, offsets2pages, up);
		if (!processed_table[page])
		{
			g_print ("%% Processing Node %s\n", up);
			process_page (tree, nodes2offsets, offsets2pages,
				nodes2iters, processed_table, page_list,
				page_list[page]);
		}
	}
	if (prev && strcmp (prev, "(dir)"))
	{
		page = node2page (nodes2offsets, offsets2pages, prev);
		if (!processed_table[page])
		{
			g_print ("%% Processing Node %s\n", prev);
			process_page (tree, nodes2offsets, offsets2pages,
				nodes2iters, processed_table, page_list,
				page_list[page]);
		}
	}
	
	/* by this point our parent and older sibling should be processed */
	if (!up || !strcmp (up, "(dir)") || !strcmp (up, "Top"))
	{
		g_print ("\t> no parent\n");
		if (!prev || !strcmp (prev, "(dir)"))
		{
			g_print ("\t> no previous\n");
			gtk_tree_store_append (tree, iter, NULL);
		}
		else if (prev)
			gtk_tree_store_insert_after (tree, iter, NULL,
				node2iter (nodes2iters, prev));
	}
	else if (!prev || !strcmp (prev, "(dir)") || !strcmp (prev, up))
	{
		g_print ("\t> no previous\n");
		gtk_tree_store_append (tree, iter,
			node2iter (nodes2iters, up));
	}
	else if (up && prev)
	{
		g_print ("+++ Parent: %s Previous: %s\n", up, prev);
		if (node2iter (nodes2iters, up))
			g_print ("++++ Have parent node!\n");
		if (node2iter (nodes2iters, prev))
			g_print ("++++ Have previous node!\n");
			gtk_tree_store_insert_after (tree, iter,
				node2iter (nodes2iters, up),
				node2iter (nodes2iters, prev));
	}
	else
	{
		g_print ("# node %s was not put in tree\n", node);
		return;
	}

	if (iter) g_print ("Have a valid iter, storing for %s\n", node);
	g_hash_table_insert (nodes2iters, g_strdup (node), iter);
	g_print ("size: %i\n", g_hash_table_size (nodes2iters));
	gtk_tree_store_set (tree, iter,
			COLUMN_PAGE_NO, g_strdup_printf ("%i",
				node2page (nodes2offsets, offsets2pages, node)),
			COLUMN_PAGE_NAME, g_strdup (node),
			COLUMN_PAGE_CONTENT, g_strdup (parts[2]),
			-1);

	g_free (node);
	g_free (up);
	g_free (prev);
	g_free (next);
	g_strfreev (parts);
}

/**
 * Parse file into a GtkTreeStore containing useful information that we can
 * later convert into a nice XML document or something else.
 */
GtkTreeStore
*yelp_info_parser_parse_file (char *file)
{
	char **page_list;
	char **ptr;
	int pages;
	int offset;
	GHashTable *nodes2offsets;
	GHashTable *offsets2pages;
	GHashTable *nodes2iters;
	int *processed_table;
	GtkTreeStore *tree;
	int pt;
	char *str;
	gboolean chained_info;
	
	str = open_info_file (file);
	page_list = g_strsplit (str, "\n", 0);

	g_free (str);
	
	pages = 0;
	offset = 0;
	chained_info = FALSE;

	offsets2pages = g_hash_table_new (g_str_hash, g_str_equal);
	
	for (ptr = page_list; *ptr != NULL; ptr++)
	{
		g_print ("page %i at offset %i\n", pages, offset);
/*		g_print ("page starts:\n%s...\n", *ptr); */

		g_hash_table_insert (offsets2pages,
				g_strdup_printf ("%i", offset), 
				GINT_TO_POINTER (pages));
		
		
		offset += strlen (*ptr);
		if (pages) offset += 2;
		pages++;
		pt = page_type (*ptr);
		if (pt == PAGE_TAG_TABLE)
		{
			g_print ("Have the Tag Table\n");
			/* this needs to be freed later too */
			nodes2offsets = process_tag_table (*ptr);
			break;
		}
		else if (pt == PAGE_INDIRECT)
		{
			g_print ("Have the indirect mapping table\n");
			chained_info = TRUE;
			str = process_indirect_map (*ptr);
		}
	}

	if (chained_info)
	{
		/* this is a chained info file, and therefore will require
		 * more processing */
		g_strfreev (page_list);
		g_hash_table_destroy (offsets2pages);
		
		pages = 0;
		offset = 0;
		
		page_list = g_strsplit (str, "\n", 0);
		offsets2pages = g_hash_table_new (g_str_hash, g_str_equal);
		
		g_free (str);
		
		for (ptr = page_list; *ptr != NULL; ptr++)
		{
			g_print ("page %i at offset %i\n", pages, offset);
			g_hash_table_insert (offsets2pages,
					g_strdup_printf ("%i", offset),
					 GINT_TO_POINTER (pages));
			offset += strlen (*ptr);
			if (pages) offset += 2;
			pages++;
		}
	}

	/* at this point we have two dictionaries,
	 * 	node names:offsets, and
	 * 	offsets:page numbers
	 * rather then consolidating these into one dictionary, we'll just
	 * chain our lookups */
	processed_table = g_malloc0 (pages * sizeof (int));
	tree = gtk_tree_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_STRING);
	nodes2iters = g_hash_table_new (g_str_hash, g_str_equal);

	pages = 0;
	for (ptr = page_list; *ptr != NULL; ptr++)
	{
		if (page_type (*ptr) != PAGE_NODE) continue;
		process_page (tree, nodes2offsets, offsets2pages, nodes2iters,
				processed_table, page_list, *ptr);
	}

	g_strfreev (page_list);

	g_hash_table_destroy (nodes2offsets);
	g_hash_table_destroy (offsets2pages);
	g_hash_table_destroy (nodes2iters);
	g_free (processed_table);

	return tree;
}

/* End Part 1 */
/* Part 2: Parse Tree into XML */
static void
parse_tree_level (GtkTreeStore *tree, xmlNodePtr *node, GtkTreeIter iter)
{
	GtkTreeIter children;
	xmlNodePtr newnode;

	char *page_no;
	char *page_name;
	char *page_content;
	
	g_print ("Decended\n");
	do
	{
		gtk_tree_model_get (GTK_TREE_MODEL (tree), &iter,
				COLUMN_PAGE_NO, &page_no,
				COLUMN_PAGE_NAME, &page_name,
				COLUMN_PAGE_CONTENT, &page_content,
				-1);
		g_print ("Got Section: %s\n", page_name);
		newnode = xmlNewTextChild (*node, NULL,
				BAD_CAST "Section",
				page_content);
		/* if we free the page content, now it's in the XML, we can
		 * save some memory */
		g_free (page_content);
		page_content = NULL;

		xmlNewProp (newnode, "id", g_strdup (page_no));
		xmlNewProp (newnode, "name", g_strdup (page_name));
		if (gtk_tree_model_iter_children (GTK_TREE_MODEL (tree),
				&children,
				&iter))
			parse_tree_level (tree, &newnode, children);
	}
	while (gtk_tree_model_iter_next (GTK_TREE_MODEL (tree), &iter));
	g_print ("Ascending\n");
}

xmlDocPtr
yelp_info_parser_parse_tree (GtkTreeStore *tree)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	GtkTreeIter iter;

	/*
	xmlChar *xmlbuf;
	int bufsiz;
	*/

	doc = xmlNewDoc ("1.0");
	node = xmlNewNode (NULL, BAD_CAST "Info");
	xmlDocSetRootElement (doc, node);

	/* functions I will want:
	gtk_tree_model_get_iter_first;
	gtk_tree_model_iter_next;
	gtk_tree_model_iter_children;
	*/

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (tree), &iter))
		parse_tree_level (tree, &node, iter);
	else
		g_print ("Empty tree?\n");

	/*
	xmlDocDumpFormatMemory (doc, &xmlbuf, &bufsiz, 1);
	g_print ("XML follows:\n%s\n", xmlbuf);
	*/

	return doc;
}
