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
#include "yelp-debug.h"


typedef struct _TagTableFix TagTableFix;

GtkTreeIter *         find_real_top                      (GtkTreeModel *model, 
							  GtkTreeIter *it);
GtkTreeIter *         find_real_sibling                  (GtkTreeModel *model,
							  GtkTreeIter *it, 
							  GtkTreeIter *comp);
xmlNodePtr            yelp_info_parse_menu               (GtkTreeStore *tree,
							  xmlNodePtr *node,
							  gchar *page_content,
							  gboolean notes);
gboolean              get_menuoptions                    (gchar *line, 
							  gchar **title, 
							  gchar **ref, 
							  gchar **desc, 
							  gchar **xref);
gboolean              resolve_frag_id                    (GtkTreeModel *model, 
							  GtkTreePath *path, 
							  GtkTreeIter *iter,
							  gpointer data);
void                  fix_tag_table                      (gchar *offset, 
							  gpointer page, 
							  TagTableFix *a);
void   		      info_process_text_notes            (xmlNodePtr *node, 
							  gchar *content,
							  GtkTreeStore *tree);


static GHashTable *
info_image_get_attributes (gchar const* string)
{
  GMatchInfo *match_info;
  GRegex *regex;
  GHashTable *h;

  h = 0;
  regex = g_regex_new ("([^\\s][^\\s=]+)=(?:([^\\s \"]+)|(?:\"((?:[^\\\"]|\\\\[\\\\\"])*)\"))", 0, 0, NULL);
  g_regex_match (regex, string, 0, &match_info);
  while (g_match_info_matches (match_info))
    {
      gchar *key;
      gchar *value;

      if (!h)
	h = g_hash_table_new (g_str_hash, g_str_equal);
      key = g_match_info_fetch (match_info, 1);
      value = g_match_info_fetch (match_info, 2);
      if (!*value)
	value = g_match_info_fetch (match_info, 3);
      g_hash_table_insert (h, key, value);
      g_match_info_next (match_info, NULL);
    }
  g_match_info_free (match_info);
  g_regex_unref (regex);

  return h;
}

/*
  info elements look like \0\b[<TAGNAME>\0\b] and take attribute=value
  pairs, i.e. for image: \0\b[image src="foo.png" \0\b]
*/
#define INFO_TAG_0 "\0"
#define INFO_TAG_1 "\b"
#define INFO_TAG_OPEN_2 INFO_TAG_1 "["
#define INFO_TAG_CLOSE_2 INFO_TAG_1 "]"
#define INFO_TAG_OPEN_2_RE INFO_TAG_1 "[[]"
#define INFO_TAG_CLOSE_2_RE INFO_TAG_1 "[]]"
#define INFO_TAG_OPEN INFO_TAG_0 INFO_TAG_1 INFO_TAG_OPEN_2
#define INFO_TAG_CLOSE INFO_TAG_0 INFO_TAG_1 INFO_TAG_CLOSE_2
#define INFO_TAG_OPEN_RE INFO_TAG_0 INFO_TAG_1 INFO_TAG_OPEN_2_RE
#define INFO_TAG_CLOSE_RE INFO_TAG_0 INFO_TAG_1 INFO_TAG_CLOSE_2_RE
/* C/glib * cannot really handle \0 in strings, convert to '@' */
#define INFO_C_TAG_0 "@"
#define INFO_C_TAG_OPEN INFO_C_TAG_0 INFO_TAG_OPEN_2
#define INFO_C_TAG_CLOSE INFO_C_TAG_0 INFO_TAG_CLOSE_2
#define INFO_C_TAG_OPEN_RE INFO_C_TAG_0 INFO_TAG_OPEN_2_RE
#define INFO_C_TAG_CLOSE_RE INFO_C_TAG_0 INFO_TAG_CLOSE_2_RE
#define INFO_C_IMAGE_TAG_OPEN INFO_C_TAG_OPEN "image"
#define INFO_C_IMAGE_TAG_OPEN_RE INFO_C_TAG_OPEN_RE "image"

static xmlNodePtr
info_insert_image (xmlNodePtr parent, GMatchInfo *match_info)
{
  GHashTable *h = info_image_get_attributes (g_match_info_fetch (match_info, 1));
  gchar *source;
  if (h)
    source = (gchar*)g_hash_table_lookup (h, "src");

  if (!h || !source || !*source)
    return xmlNewTextChild (parent, NULL, BAD_CAST "para1", BAD_CAST "[broken image]");

  gchar *title = (gchar*)g_hash_table_lookup (h, "title");
  gchar *text = (gchar*)g_hash_table_lookup (h, "text");
  gchar *alt = (gchar*)g_hash_table_lookup (h, "alt");
  g_hash_table_destroy (h);
  xmlNodePtr img = xmlNewChild (parent, NULL, BAD_CAST "img", NULL);
  xmlNewProp (img, BAD_CAST "src", BAD_CAST source);
  xmlNewProp (img, BAD_CAST "title", BAD_CAST (title ? title : ""));
  xmlNewProp (img, BAD_CAST "text", BAD_CAST (text ? text : ""));
  xmlNewProp (img, BAD_CAST "alt", BAD_CAST (alt ? alt : ""));
  g_free (source);
  g_free (title);
  g_free (alt);
  return parent;
}

/*
  Convert body text CONTENT to xml nodes, processing info image tags
  when found.  IWBN add a regex match for *Note: here and call the
  *Note ==> <a href> logic of info_process_text_notes from here.
 */
static xmlNodePtr
info_body_text (xmlNodePtr parent, xmlNsPtr ns, gchar const *name, gchar const *content)
{
  if (!strstr (content, INFO_C_IMAGE_TAG_OPEN))
    return xmlNewTextChild (parent, ns, BAD_CAST name, BAD_CAST content);

  gint content_len = strlen (content);
  gint pos = 0;
  GRegex *regex = g_regex_new ("(" INFO_C_IMAGE_TAG_OPEN_RE "((?:[^" INFO_TAG_1 "]|[^" INFO_C_TAG_0 "]+" INFO_TAG_1 ")*)" INFO_C_TAG_CLOSE_RE ")", 0, 0, NULL);
  GMatchInfo *match_info;
  g_regex_match (regex, content, 0, &match_info);
  while (g_match_info_matches (match_info))
    {
      gint image_start;
      gint image_end;
      gboolean image_found = g_match_info_fetch_pos (match_info, 0,
						     &image_start, &image_end);
      gchar *before = g_strndup (&content[pos], image_start - pos);
      pos = image_end + 1;
      xmlNewTextChild (parent, NULL, BAD_CAST "para1", BAD_CAST (before));
      g_free (before);
      if (image_found)
	info_insert_image (parent, match_info);
      g_match_info_next (match_info, NULL);
    }
  gchar *after = g_strndup (&content[pos], content_len - pos);
  xmlNewTextChild (parent, NULL, BAD_CAST "para1", BAD_CAST (after));
  g_free (after);
  return 0;
}

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
  if (g_ascii_strncasecmp (page, "Tag Table:\n", 11) == 0)
    return PAGE_TAG_TABLE;
  else if (g_ascii_strncasecmp (page, "Indirect:\n", 10) == 0)
    return PAGE_INDIRECT;
  else if (g_ascii_strncasecmp (page, "File:", 5) == 0 ||
	   g_ascii_strncasecmp (page, "Node:", 5) == 0)
    return PAGE_NODE;

  else
    return PAGE_OTHER;
}

static char
*open_info_file (char *file)
{
	GIOChannel *channel = NULL;
	int i;
	gsize len;
	char *str;
	GError *error = NULL;
	GIOStatus result = G_IO_STATUS_NORMAL;

	debug_print (DB_DEBUG, "!! Opening %s...\n", file);
	
	channel = yelp_io_channel_new_file (file, &error);
	if (!channel) {
	  return NULL;
	}
	result = g_io_channel_read_to_end (channel, &str, &len, &error);
	if (result != G_IO_STATUS_NORMAL) {
	  return NULL;
	}
	g_io_channel_shutdown (channel, FALSE, NULL);
	g_io_channel_unref (channel);

	/* C/glib * cannot really handle \0 in strings, convert. */
	for (i = 0; i < (len - 1); i++)
	  if (str[i] == INFO_TAG_OPEN[0] && str[i+1] == INFO_TAG_OPEN[1])
	    str[i] = INFO_C_TAG_OPEN[0];

	return str;
}

static gchar *
find_info_part (gchar *part_name, gchar *base)
{
  /* New and improved.  We now assume that all parts are
   * in the same subdirectory as the base file.  Makes
   * life much simpler and is (afaict) always true
   */
  gchar *path;
  gchar *tmp;
  gchar *bzfname, *gzfname, *lzfd, *fname;
  gchar *uri = NULL;
  tmp = g_strrstr (base, "/");
  path = g_strndup (base, tmp-base);

  bzfname = g_strconcat (path, "/", part_name, ".bz2", NULL);
  gzfname = g_strconcat (path, "/", part_name, ".gz", NULL);
  lzfd = g_strconcat (path, "/", part_name, ".lzma", NULL);
  fname = g_strconcat (path, "/", part_name, NULL);

  if (g_file_test (bzfname, G_FILE_TEST_EXISTS))
    uri = g_strdup (bzfname);
  else if (g_file_test (gzfname, G_FILE_TEST_EXISTS))
    uri = g_strdup (gzfname);
  else if (g_file_test (lzfd, G_FILE_TEST_EXISTS))
    uri = g_strdup (lzfd);
  else if (g_file_test (fname, G_FILE_TEST_EXISTS))
    uri = g_strdup (fname);

  g_free (bzfname);
  g_free (gzfname);
  g_free (lzfd);
  g_free (fname);
  g_free (path);
  return uri;

}

static char
*process_indirect_map (char *page, gchar * file)
{
	char **lines;
	char **ptr;
	char *composite = NULL;
	
	lines = g_strsplit (page, "\n", 0);

	for (ptr = lines + 1; *ptr != NULL; ptr++);
	for (ptr--; ptr != lines; ptr--)
	{
		char **items;
		char *filename;
		char *str;
		char **pages;
		int offset;
		int plength;

		debug_print (DB_DEBUG, "Line: %s\n", *ptr);
		items = g_strsplit (*ptr, ": ", 2);

		if (items[0])
		{
		  filename = find_info_part (items[0], file);
		  str = open_info_file (filename);
		  if (!str) {
			g_strfreev (items);
		  	continue;
		  }
			pages = g_strsplit (str, "", 2);
			g_free (str);
			if (!pages[1]) {
				g_strfreev (items);
				g_strfreev (pages);
		  		continue;
			}

			offset =  atoi(items[1]);
			plength = strlen(pages[1]);
			
			debug_print (DB_DEBUG, "Need to make string %s+%i bytes = %i\n",
				    items[1], plength,
				    offset + plength);
			
			if (!composite) /* not yet created, malloc it */
			{
				int length;
				length = offset + plength;
				composite = g_malloc (sizeof (char) *
						      (length + 1));
				memset (composite, '-', length);
				composite[length] = '\0';
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

	table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, 
				       g_free);
	lines = g_strsplit (page, "\n", 0);

	for (ptr = lines; *ptr != NULL; ptr++)
	{
		if (strncmp (*ptr, "Node: ", 6) == 0)
		{
			items = g_strsplit (*ptr, "", 2);
			debug_print (DB_DEBUG, "Node: %s Offset: %s\n",
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
	if (!ptr) {
	  g_free (source_cp);
		return NULL;
	}
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
	char *offset = NULL;
	gint page;

	offset = g_hash_table_lookup (nodes2offsets, node);
	page = GPOINTER_TO_INT (g_hash_table_lookup (offsets2pages, offset));

	return page;
}

static GtkTreeIter
*node2iter (GHashTable *nodes2iters, char *node)
{
	GtkTreeIter *iter;

	iter = g_hash_table_lookup (nodes2iters, node);
	d (if (!iter) debug_print (DB_WARN, "Could not retrieve iter for node !%s!\n", node));
	return iter;
}

GtkTreeIter 
*find_real_top (GtkTreeModel *model, GtkTreeIter *it)
{
  GtkTreeIter *r = NULL;
  GtkTreeIter *tmp = NULL;
  
  if (!it)
    return NULL;

  r = gtk_tree_iter_copy (it);
  tmp = g_malloc0 (sizeof (GtkTreeIter));
  while (gtk_tree_model_iter_parent (model, tmp, r)) {
    gtk_tree_iter_free (r);
    r = gtk_tree_iter_copy (tmp);
  }
  g_free (tmp);

  return r;
}

GtkTreeIter * find_real_sibling (GtkTreeModel *model,
				 GtkTreeIter *it, GtkTreeIter *comp)
{
  GtkTreeIter *r;
  GtkTreeIter *tmp = NULL;
  gboolean result = FALSE;
  gchar *title;
  gchar *reftitle;

  if (!it) {
    return NULL;
  }

  r = gtk_tree_iter_copy (it);
  tmp = gtk_tree_iter_copy (it);

  reftitle = gtk_tree_model_get_string_from_iter (model, comp);

  result = gtk_tree_model_iter_parent (model, r, it);
  if (!result)
    return it;

  title = gtk_tree_model_get_string_from_iter (model, r);

  while (!g_str_equal (title, reftitle) && result) {
    gtk_tree_iter_free (tmp);
    tmp = gtk_tree_iter_copy (r);
    result = gtk_tree_model_iter_parent (model, r, tmp);
    if (result)
      title = gtk_tree_model_get_string_from_iter (model, r);
  }

  if (!g_str_equal (title, reftitle))
    {
      gtk_tree_iter_free (tmp);
      tmp = NULL;
    }

  gtk_tree_iter_free (r);
  g_free (title);
  g_free (reftitle);
  return tmp;

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
	gchar *tmp;

	int page;
	
	/* split out the header line and the text */
	parts = g_strsplit (page_text, "\n", 3);

	node = get_value_after (parts[0], "Node: ");
	up = get_value_after (parts[0], "Up: ");
	prev = get_value_after (parts[0], "Prev: ");
	next = get_value_after (parts[0], "Next: ");

	if (next && g_str_equal (next, "Top")) {
	  g_free (next);
	  next = NULL;
	}
	if (g_str_equal (node, "Top") && prev != NULL) {
	  g_free (prev);
	  prev = NULL;
	}

	/* check to see if this page has been processed already */
	page = node2page (nodes2offsets, offsets2pages, node);
	if (processed_table[page]) {
		return;
	}
	processed_table[page] = 1;
	
	debug_print (DB_DEBUG, "-- Processing Page %s\n\tParent: %s\n", node, up);

	iter = g_slice_alloc0 (sizeof (GtkTreeIter));
	/* check to see if we need to process our parent and siblings */
	if (up && g_ascii_strncasecmp (up, "(dir)", 5) && strcmp (up, "Top"))
	{
		page = node2page (nodes2offsets, offsets2pages, up);
		if (!processed_table[page])
		{
		  debug_print (DB_DEBUG, "%% Processing Node %s\n", up);
			process_page (tree, nodes2offsets, offsets2pages,
				nodes2iters, processed_table, page_list,
				page_list[page]);
		}
	}
	if (prev && g_ascii_strncasecmp (prev, "(dir)", 5))
	  {
	    if (strncmp (node, "Top", 3)) {
	      /* Special case the Top node to always appear first */
	    } else {
	      page = node2page (nodes2offsets, offsets2pages, prev);
	      if (!processed_table[page])
		{
		  debug_print (DB_DEBUG, "%% Processing Node %s\n", prev);
		  process_page (tree, nodes2offsets, offsets2pages,
				nodes2iters, processed_table, page_list,
				page_list[page]);
		}
	    }
	  }
	
	/* by this point our parent and older sibling should be processed */
	if (!up || !g_ascii_strcasecmp (up, "(dir)") || !strcmp (up, "Top"))
	{
	  debug_print (DB_DEBUG, "\t> no parent\n");
		if (!prev || !g_ascii_strcasecmp (prev, "(dir)"))
		{
		  debug_print (DB_DEBUG, "\t> no previous\n");
			gtk_tree_store_append (tree, iter, NULL);
		}
		else if (prev) {
		  GtkTreeIter *real;
		  real = find_real_top (GTK_TREE_MODEL (tree), 
					node2iter (nodes2iters, prev));
		  if (real) {
		    gtk_tree_store_insert_after (tree, iter, NULL,
						 real);
		    gtk_tree_iter_free (real);
		  }
		  else 
		    gtk_tree_store_append (tree, iter, NULL);
		}
	}
	else if (!prev || !g_ascii_strcasecmp (prev, "(dir)") || !strcmp (prev, up))
	{
	  debug_print (DB_DEBUG, "\t> no previous\n");
		gtk_tree_store_append (tree, iter,
			node2iter (nodes2iters, up));
	}
	else if (up && prev)
	{
	  GtkTreeIter *upit = node2iter (nodes2iters, up);
	  GtkTreeIter *previt = node2iter (nodes2iters, prev);
	  GtkTreeIter *nit = NULL;
	  debug_print (DB_DEBUG, "+++ Parent: %s Previous: %s\n", up, prev);
	  
	  d (if (upit) debug_print (DB_DEBUG, "++++ Have parent node!\n"));
	  d (if (previt) debug_print (DB_DEBUG, "++++ Have previous node!\n"));
	  nit = find_real_sibling (GTK_TREE_MODEL (tree), previt, upit);
	  if (nit) {
	    gtk_tree_store_insert_after (tree, iter,
					 upit,
					 nit);
	    gtk_tree_iter_free (nit);
	  }
	  else
	    gtk_tree_store_append (tree, iter, upit);
	}
	else
	{
	  debug_print (DB_DEBUG, "# node %s was not put in tree\n", node);
	  return;
	}

	d (if (iter) debug_print (DB_DEBUG, "Have a valid iter, storing for %s\n", node));

	g_hash_table_insert (nodes2iters, g_strdup (node), iter);
	debug_print (DB_DEBUG, "size: %i\n", g_hash_table_size (nodes2iters));

	/*tmp = g_strdup_printf ("%i",
	  node2page (nodes2offsets, offsets2pages, node));*/
	tmp = g_strdup (node);
	tmp = g_strdelimit (tmp, " ", '_');
	gtk_tree_store_set (tree, iter,
			    COLUMN_PAGE_NO, tmp,
			    COLUMN_PAGE_NAME, node,
			    COLUMN_PAGE_CONTENT, parts[2],
			    -1);

	g_free (tmp);
	g_free (node);
	g_free (up);
	g_free (prev);
	g_free (next);
	g_strfreev (parts);
}

/* These are used to fix the tag tables to the correct offsets.
 * Assuming out offsets are correct (because we calculated them, these values
 * are used to overwrite the offsets declared in the info file */
struct _TagTableFix {
  GHashTable *nodes2offsets;
  GHashTable *pages2nodes;
};

void
fix_tag_table (gchar *offset, gpointer page, TagTableFix *a)
{
  gchar *node_name = NULL;

  node_name = g_hash_table_lookup (a->pages2nodes, page);

  if (!node_name) 
    return;

  g_hash_table_replace (a->nodes2offsets, g_strdup (node_name), g_strdup (offset));



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
	GHashTable *nodes2offsets = NULL;
	GHashTable *offsets2pages = NULL;
	GHashTable *pages2nodes = NULL;
	GHashTable *nodes2iters = NULL;
	int *processed_table;
	GtkTreeStore *tree;
	int pt;
	char *str = NULL;
	gboolean chained_info;
	TagTableFix *ttf;
	
	str = open_info_file (file);
	if (!str) {
		return NULL;
	}	
	page_list = g_strsplit (str, "\n", 0);

	g_free (str);
	str = NULL;
	
	pages = 0;
	offset = 0;
	chained_info = FALSE;

	offsets2pages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
					       NULL);
	pages2nodes = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, 
					     g_free);

	for (ptr = page_list; *ptr != NULL; ptr++)
	{
	  gchar *name = NULL;
	  debug_print (DB_DEBUG, "page %i at offset %i\n", pages, offset);

		g_hash_table_insert (offsets2pages,
				g_strdup_printf ("%i", offset), 
				GINT_TO_POINTER (pages));
		name = get_value_after (*ptr, "Node: ");
		if (name)
		  g_hash_table_insert (pages2nodes,
				       GINT_TO_POINTER (pages), name);
		
		offset += strlen (*ptr);
		if (pages) offset += 2;
		pages++;
		pt = page_type (*ptr);
		if (pt == PAGE_TAG_TABLE)
		{
		  debug_print (DB_DEBUG, "Have the Tag Table\n");
			/* this needs to be freed later too */
			nodes2offsets = process_tag_table (*ptr);
			break;
		}
		else if (pt == PAGE_INDIRECT)
		{
		  debug_print (DB_DEBUG, "Have the indirect mapping table\n");
			chained_info = TRUE;
			str = process_indirect_map (*ptr, file);
			if (!str) {
				return NULL;
			}
		}
	}

	if (chained_info)
	{
		/* this is a chained info file, and therefore will require
		 * more processing */
		g_strfreev (page_list);
		g_hash_table_destroy (offsets2pages);
		offsets2pages = g_hash_table_new_full (g_str_hash, 
						       g_str_equal, g_free,
						       NULL);
		
		pages = 0;
		offset = 0;

		page_list = g_strsplit (str, "\n", 0);
		
		g_free (str);
		
		for (ptr = page_list; *ptr != NULL; ptr++)
		{
		  debug_print (DB_DEBUG, "page %i at offset %i\n", pages, offset);
			g_hash_table_insert (offsets2pages,
					g_strdup_printf ("%i", offset),
					 GINT_TO_POINTER (pages));
			offset += strlen (*ptr);
			if (pages) offset += 2;
			pages++;
		}
	}
	if (!nodes2offsets)
	  return NULL;
	/* We now go through the offsets2pages dictionary and correct the entries
	 * as the tag tables are known to lie.  Yes, they do.  Consistantly and
	 * maliciously
	 */	
	ttf = g_new0 (TagTableFix, 1);
	ttf->nodes2offsets = nodes2offsets;
	ttf->pages2nodes = pages2nodes;
	g_hash_table_foreach (offsets2pages, (GHFunc) fix_tag_table, ttf);
	g_free (ttf);


	/* at this point we have two dictionaries,
	 * 	node names:offsets, and
	 * 	offsets:page numbers
	 * rather then consolidating these into one dictionary, we'll just
	 * chain our lookups */
	processed_table = g_malloc0 (pages * sizeof (int));
	tree = gtk_tree_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_STRING);
	nodes2iters = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
					     (GDestroyNotify) gtk_tree_iter_free);

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

	char *page_no = NULL;
	char *page_name = NULL;
	char *page_content = NULL;
	gboolean notes = FALSE;

	debug_print (DB_DEBUG, "Decended\n");
	do
	{
		gtk_tree_model_get (GTK_TREE_MODEL (tree), &iter,
				COLUMN_PAGE_NO, &page_no,
				COLUMN_PAGE_NAME, &page_name,
				COLUMN_PAGE_CONTENT, &page_content,
				-1);
		debug_print (DB_DEBUG, "Got Section: %s\n", page_name);
		if (strstr (page_content, "*Note") || 
		    strstr (page_content, "*note")) {
		  notes = TRUE;
		}
		if (strstr (page_content, "* Menu:")) {
		  newnode = yelp_info_parse_menu (tree, node, page_content, notes);
		} else {
		  newnode = xmlNewTextChild (*node, NULL,
					     BAD_CAST "Section",
					     NULL);
		  if (!notes)
		    info_body_text (newnode, NULL, "para", page_content);
		  
		  else {
		    /* Handle notes here */
		    info_process_text_notes (&newnode, page_content, tree);
		  }
		}
		/* if we free the page content, now it's in the XML, we can
		 * save some memory */
		g_free (page_content);
		page_content = NULL;

		xmlNewProp (newnode, BAD_CAST "id", 
			    BAD_CAST page_no);
		xmlNewProp (newnode, BAD_CAST "name", 
			    BAD_CAST page_name);
		if (gtk_tree_model_iter_children (GTK_TREE_MODEL (tree),
				&children,
				&iter))
		  parse_tree_level (tree, &newnode, children);
		g_free (page_no);
		g_free (page_name);
	}
	while (gtk_tree_model_iter_next (GTK_TREE_MODEL (tree), &iter));
	debug_print (DB_DEBUG, "Ascending\n");
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

	doc = xmlNewDoc (BAD_CAST "1.0");
	node = xmlNewNode (NULL, BAD_CAST "Info");
	xmlDocSetRootElement (doc, node);

	/* functions I will want:
	gtk_tree_model_get_iter_first;
	gtk_tree_model_iter_next;
	gtk_tree_model_iter_children;
	*/

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (tree), &iter))
		parse_tree_level (tree, &node, iter);
	d (else debug_print (DB_DEBUG, "Empty tree?\n"));

	/*
	xmlDocDumpFormatMemory (doc, &xmlbuf, &bufsiz, 1);
	g_print ("XML follows:\n%s\n", xmlbuf);
	*/

	return doc;
}

gboolean
resolve_frag_id (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
		 gpointer data)
{
  gchar *page_no = NULL;
  gchar *page_name = NULL;
  gchar **xref = data;

  gtk_tree_model_get (GTK_TREE_MODEL (model), iter,
		      COLUMN_PAGE_NO, &page_no,
		      COLUMN_PAGE_NAME, &page_name,
		      -1);
  if (g_str_equal (page_name, *xref)) {
    g_free (*xref);
    *xref = g_strdup (page_name);
    *xref = g_strdelimit (*xref, " ", '_');

    g_free (page_name);
    g_free (page_no);
    return TRUE;
  }
  g_free (page_name);
  g_free (page_no);

  return FALSE;
}

gboolean
get_menuoptions (gchar *line, gchar **title, gchar **ref, gchar **desc, 
		 gchar **xref)
{
  /* Since info is actually braindead and allows .s in 
   * its references, we gotta carefully extract things
   * as .s can be in either the title or desc
   */
  gchar *tmp = line;
  gchar *tfind = NULL;

  if (!g_str_has_prefix (line, "* "))
    return FALSE;

  tfind = strchr (tmp, ':');

  if (!tfind) /* No : on the line, bail out */
    return FALSE;

  (*title) = g_strndup (tmp, tfind-tmp);

  if (tfind[1] == ':') { /* This happens if the title and ref are the same
			 * Most menus are of this type 
			 */

    (*ref) = NULL; /* There is no second part.  The rest is description */

    tmp++;
    (*xref) = g_strndup (tmp, tfind-tmp);
    g_strstrip (*xref);

    tfind+=2;
    (*desc) = g_strdup (tfind);
  } else { /* The other type of menu option */
    gchar *td = NULL;

    tfind++;
    td = strchr (tfind, '.');
    if (!td)
      return FALSE;
    (*ref) = g_strndup (tfind, td-tfind);
    (*xref) = g_strdup (*ref);
    g_strstrip (*xref);
    
    td++;
    (*desc) = g_strdup (td);
  }
  return TRUE;
}

xmlNodePtr
yelp_info_parse_menu (GtkTreeStore *tree, xmlNodePtr *node, 
		      gchar *page_content, gboolean notes)
{
  gchar **split;
  gchar **menuitems;
  gchar *tmp = NULL;
  xmlNodePtr newnode;
  int i=0;

  split = g_strsplit (page_content, "* Menu:", 2);
  
  newnode = xmlNewChild (*node, NULL,
			 BAD_CAST "Section", NULL);
    

  tmp = g_strconcat (split[0], "\n* Menu:", NULL);
  if (!notes)
    info_body_text (newnode, NULL, "para", tmp);
  else {
    info_process_text_notes (&newnode, tmp, tree);
  }
  g_free (tmp);

  menuitems = g_strsplit (split[1], "\n", -1);
  g_strfreev (split);

  while (menuitems[i] != NULL) {
    gboolean menu = FALSE;
    gchar *title = NULL;
    gchar *ref = NULL;
    gchar *desc = NULL;
    gchar *xref = NULL;
    xmlNodePtr mholder;
    xmlNodePtr ref1;

    menu = get_menuoptions (menuitems[i], &title, &ref, &desc, &xref);
 
    if (menu) {
      mholder = xmlNewChild (newnode, NULL, BAD_CAST "menuholder", NULL);
      gtk_tree_model_foreach (GTK_TREE_MODEL (tree), resolve_frag_id, &xref);
      
      if (ref == NULL) { /* A standard type menu */
	tmp = g_strconcat (title, "::", NULL);
	ref1 = xmlNewTextChild (mholder, NULL, BAD_CAST "a",
				BAD_CAST tmp);
	g_free (tmp);
	tmp = g_strconcat ("#", xref, NULL);
	xmlNewProp (ref1, BAD_CAST "href", BAD_CAST tmp);
	g_free (tmp);
      } else { /* Indexy type menu  - we gotta do a  little work to fix the
		* spacing
		*/
	gchar *spacing = ref;
	gint c=0;
	gchar *sp = NULL;

	while (*spacing ==' ') {
	  c++;
	  spacing++;
	}
	sp = g_strndup (ref, c);
	
	ref1 = xmlNewTextChild (mholder, NULL, BAD_CAST "a",
					BAD_CAST title);
	tmp = g_strconcat ("#", xref, NULL);
	xmlNewProp (ref1, BAD_CAST "href", BAD_CAST tmp);
	g_free (tmp);
	xmlNewTextChild (mholder, NULL, BAD_CAST "spacing",
			 BAD_CAST sp);
	tmp = g_strconcat (g_strstrip(ref), ".", NULL);
	ref1 = xmlNewTextChild (mholder, NULL, BAD_CAST "a",
				BAD_CAST tmp);
	g_free (tmp);
	tmp = g_strconcat ("#", xref, NULL);
	xmlNewProp (ref1, BAD_CAST "href", BAD_CAST tmp);

	g_free (tmp);	
	g_free (sp);
      }
      xmlNewTextChild (mholder, NULL, BAD_CAST "para",
		       BAD_CAST desc);
    } else {
      xmlNewTextChild (newnode, NULL, BAD_CAST "para",
		       BAD_CAST menuitems[i]);
      
    }
    i++;
    g_free (title);
    g_free (ref);
    g_free (desc);
    g_free (xref);

  }
  g_strfreev (menuitems);
  
  return newnode;
}

void
info_process_text_notes (xmlNodePtr *node, gchar *content, GtkTreeStore *tree)
{
  gchar **notes;
  gchar **current;
  xmlNodePtr holder;
  xmlNodePtr ref1;
  gboolean first = TRUE;

  notes = g_strsplit (content, "*Note", -1);
  holder = xmlNewChild (*node, NULL, BAD_CAST "noteholder", NULL);

  for (current = notes; *current != NULL; current++) {
    /* Since the notes can be either *Note or *note, we handle the second 
     * variety here
     */
    gchar **subnotes;
    gchar **current_real;

    subnotes = g_strsplit (*current, "*note", -1);
    for (current_real = subnotes; *current_real != NULL; current_real++) {
      gchar *url, **urls, **ulink;
      gchar *append;
      gchar *alt_append, *alt_append1;
      gchar *link_text;
      gchar *href = NULL;
      gchar *break_point = NULL;
      gboolean broken = FALSE;
      if (first) {
	/* The first node is special.  It doesn't have a note ref at the 
	 * start, so we can just add it and forget about it.
	 */
	first = FALSE;
	info_body_text (holder, NULL, "para1", (*current_real));
	continue;
      }
      /* If we got to here, we now gotta parse the note reference */

      if (*current_real[0] == '_') {
	/* Special type of note that isn't really a note, but pretends
	 * it is
	 */
	info_body_text (holder, NULL, "para1",
			g_strconcat ("*Note", *current_real, NULL));
	continue;
      }
      append = strchr (*current_real, ':');
      if (!append) {
	info_body_text (holder, NULL, "para1", *current_real);
	continue;
      }
      append++;
      alt_append = append;
      alt_append1 = alt_append;
      append = strchr (append, ':');
      alt_append = strchr (alt_append, '.');
      if (alt_append && g_str_has_prefix (alt_append, ".info")) {
	broken = TRUE;
	alt_append++;
	alt_append = strchr (alt_append, '.');
      }
      alt_append1 = strchr (alt_append1, ',');
      if (!append && !alt_append && !alt_append1) {
	info_body_text (holder, NULL, "para1", *current_real);
	continue;
      }
      if (!append || alt_append || alt_append1) {
	if (!append) {
	  if (alt_append) append = alt_append;
	  else append = alt_append1;
	}
	if ((alt_append && alt_append < append))
	  append = alt_append;
	if (alt_append1 && alt_append1 < append)
	  append = alt_append1;
      }
      append++;
      url = g_strndup (*current_real, append - (*current_real));

      /* By now, we got 2 things.  First, is append which is the (hopefully)
       * non-link text.  Second, we got a url.
       * The url can be in several forms:
       * 1. linkend::
       * 2. linkend:(infofile)Linkend.
       * 3. Title: Linkend.
       * 4. Title: Linkend, (pretty sure this is just broken)
       * 5. Title: (infofile.info)Linkend.
       * All possibilities should have been picked up.
       * Here:
       * Clean up the split.  Should be left with a real url and
       * a list of fragments that should be linked
       * Also goes through and removes extra spaces, leaving only one 
       * space in place of many
       */
      urls = g_strsplit (url, "\n", -1);
      break_point = strchr (url, '\n');
      while (break_point) {
	*break_point = ' ';
	break_point = strchr (++break_point, '\n');
      }
      break_point = strchr (url, ' ');
      while (break_point) {
	if (*(break_point+1) == ' ') {
	  /* Massive space.  Fix. */
	  gchar *next = break_point;
	  gchar *url_copy;
	  while (*next == ' ')
	    next++;
	  next--;
	  url_copy = g_strndup (url, break_point-url);
	  g_free (url);
	  url = g_strconcat (url_copy, next, NULL);
	  break_point = strchr (url, ' ');
	  g_free (url_copy);
	} else {
	  break_point++;
	  break_point = strchr (break_point, ' ');
	}
      }
      if (url[strlen(url)-1] == '.') { /* The 2nd or 3rd sort of link */ 
	gchar *stop = NULL;
	gchar *lurl = NULL;
	gchar *zloc = NULL;
	stop = strchr (url, ':');
	lurl = strchr (stop, '(');
	if (!lurl) { /* 3rd type of link */
	  gchar *link;
	  gint length;
	  stop++;
	  link = g_strdup (stop);
	  link = g_strstrip (link);
	  length = strlen (link) - 1;
	  link[length] = '\0';	  
	  href = g_strconcat ("#", link, NULL);
	  link[length] = 'a';
	  g_free (link);


	} else { /* 2nd type of link.  Easy. Provided .info is neglected ;) */
	  if (broken) {
	    gchar *new_url;
	    gchar *info;
	    gchar *stripped;

	    new_url = g_strdup (lurl);
	    info = strstr (new_url, ".info)");
	    stripped = g_strndup (new_url, info-new_url);
	    info +=5;
	    lurl = g_strconcat (stripped, info, NULL);
	    g_free (stripped);
	    g_free (new_url);
	  }
	  zloc = &(lurl[strlen(lurl)-1]);
	  *zloc = '\0';
	  href = g_strconcat ("info:", lurl, NULL);
	  *zloc = 'a';
	}
      } else { /* First kind of link */
	gchar *tmp1;
	gchar *frag;

	tmp1 = strchr (url, ':');
	if (!tmp1)
	  frag = g_strdup (url);
	else 
	  frag = g_strndup (url, tmp1 - url);
	g_strstrip (frag);
	gtk_tree_model_foreach (GTK_TREE_MODEL (tree), resolve_frag_id, &frag);
	href = g_strconcat ("#", frag, NULL);
	g_free (frag);
      }
      for (ulink = urls; *ulink != NULL; ulink++) {
	if (ulink == urls)
	  link_text = g_strconcat ("*Note", *ulink, NULL);
	else {
	  gchar *spacing = *ulink;
	  gchar *tmp;
	  gint count = 0;
	  while (*spacing == ' ') {
	    spacing++;
	    count++;
	  }
	  if (spacing != *ulink) {
	    if (count > 1)
	      spacing-=2;
	    tmp = g_strndup (*ulink, spacing-*ulink);
	    if (count > 1)
	      spacing+=2;
	    xmlNewTextChild (holder, NULL, BAD_CAST "spacing",
			     BAD_CAST tmp);
	    g_free (tmp);
	    link_text = g_strdup (spacing);
	  } else {
	    link_text = g_strdup (*ulink);
	  }
	}
	ref1 = xmlNewTextChild (holder, NULL, BAD_CAST "a",
				BAD_CAST link_text);
	if (*(ulink+1) != NULL)
	  info_body_text (holder, NULL, "para", "");

	g_free (link_text);
	xmlNewProp (ref1, BAD_CAST "href", BAD_CAST href);
      }
      g_strfreev (urls);
      /* Finally, we can add the text as required */
      info_body_text (holder, NULL, "para1", append);
      g_free (url);
      g_free (href);
    }
    g_strfreev (subnotes);
  }
  g_strfreev (notes);
}
