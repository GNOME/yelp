#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>

#include "yelp-io-channel.h"
#include "yelp-info-parser.h"
#include "yelp-utils.h"

enum
{
	PAGE_TAG_TABLE,
	PAGE_NODE,
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
	else if (strncmp (page, "File: ", 6) == 0)
		return PAGE_NODE;
	else
		return PAGE_OTHER;
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
	if (up && strcmp (up, "(dir)"))
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
	if (!up || !strcmp (up, "(dir)"))
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
			gtk_tree_store_insert_after (tree, iter,
				node2iter (nodes2iters, up),
				node2iter (nodes2iters, prev));
	else
	{
		g_print ("# node %s was not put in tree\n", node);
		return;
	}

	if (iter) g_print ("Have a valid iter, storing for %s\n", node);
	g_hash_table_insert (nodes2iters, g_strdup (node), iter);
	g_print ("size: %i\n", g_hash_table_size (nodes2iters));
	gtk_tree_store_set (tree, iter,
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
	char *str;
	int len;
	GIOChannel *channel;
	
	channel = yelp_io_channel_new_file (file, NULL);
	g_io_channel_read_to_end (channel, &str, &len, NULL);
	g_io_channel_shutdown (channel, FALSE, NULL);
	
	page_list = g_strsplit (str, "\n", 0);

	g_free (str);
	
	pages = 0;
	offset = 0;

	offsets2pages = g_hash_table_new (g_str_hash, g_str_equal);
	
	for (ptr = page_list; *ptr != NULL; ptr++)
	{
		g_print ("page %i at offset %i\n", pages, offset);

		g_hash_table_insert (offsets2pages,
				g_strdup_printf ("%i", offset), 
				GINT_TO_POINTER (pages));
		
		
		offset += strlen (*ptr);
		if (pages) offset += 2;
		pages++;
		if (page_type (*ptr) == PAGE_TAG_TABLE)
		{
			g_print ("Have the Tag Table\n");
			/* this needs to be freed later too */
			nodes2offsets = process_tag_table (*ptr);
			break;
		}
	}

	/* at this point we have two dictionaries,
	 * 	node names:offsets, and
	 * 	offsets:page numbers
	 * rather then consolidating these into one dictionary, we'll just
	 * chain our lookups */
	processed_table = g_malloc0 (pages * sizeof (int));
	tree = gtk_tree_store_new (N_COLUMNS, G_TYPE_INT, G_TYPE_STRING,
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
