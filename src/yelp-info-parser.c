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

#ifdef YELP_DEBUG
#define d(x) x
#else
#define d(x)
#endif

GtkTreeIter *         find_real_top                      (GtkTreeModel *model, 
							  GtkTreeIter *it);
GtkTreeIter *         find_real_sibling                  (GtkTreeModel *model,
							  GtkTreeIter *it, 
							  GtkTreeIter *comp);
xmlNodePtr            yelp_info_parse_menu               (GtkTreeStore *tree,
							  xmlNodePtr *node,
							  gchar *page_content);
gboolean              get_menuoptions                    (gchar *line, 
							  gchar **title, 
							  gchar **ref, 
							  gchar **desc, 
							  gchar **xref);
gboolean              resolve_frag_id                    (GtkTreeModel *model, 
							  GtkTreePath *path, 
							  GtkTreeIter *iter,
							  gpointer data);

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

	d (g_print ("!! Opening %s...\n", file));
	
	channel = yelp_io_channel_new_file (file, &error);
	/* TODO: Actually handle the errors sanely.  Don't crash */
	if (!channel) {
	  g_error ("Error opening file %s: %s\n", file, error->message);
	  g_error_free (error);
	  error = NULL;
	  exit (655);
	}
	result = g_io_channel_read_to_end (channel, &str, &len, &error);
	/* TODO: Ditto above */
	if (result != G_IO_STATUS_NORMAL) {
	  g_error ("Error reading file: %s\n", error->message);
	  g_error_free (error);
	  error = NULL;
	  exit (666);
	}
	g_io_channel_shutdown (channel, FALSE, NULL);

	for (i = 0; i < (len - 1); i++)
	{
		if (str[i] == '\0' && str[i+1] == '\b')
		{
		  d (g_print ("=> got a NULL, replacing\n"));
		  str[i] = ' '; str[i+1] = ' ';
		}
	}

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
  gchar *bzfname, *gzfname;
  gchar *uri = NULL;
  tmp = g_strrstr (base, "/");
  path = g_strndup (base, tmp-base);

  bzfname = g_strconcat (path, "/", part_name, ".bz2", NULL);
  gzfname = g_strconcat (path, "/", part_name, ".gz", NULL);
  
  if (g_file_test (bzfname, G_FILE_TEST_EXISTS))
    uri = g_strdup (bzfname);
  else if (g_file_test (gzfname, G_FILE_TEST_EXISTS))
    uri = g_strdup (gzfname);

  g_free (bzfname);
  g_free (gzfname);
  g_free (path);
  return uri;

}

static char
*process_indirect_map (char *page, gchar * file)
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

		d (g_print ("Line: %s\n", *ptr));
		items = g_strsplit (*ptr, ": ", 2);

		if (items[0])
		{
		  filename = find_info_part (items[0], file);
		  str = open_info_file (filename);
		  
			pages = g_strsplit (str, "", 2);
			g_free (str);

			offset =  atoi(items[1]);
			plength = strlen(pages[1]);
			
			d (g_print ("Need to make string %s+%i bytes = %i\n",
				    items[1], plength,
				    offset + plength));
			
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
			d (g_print ("Node: %s Offset: %s\n",
				    items[0] + 6, items[1]));
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
	char *offset;
	gint int_offset;
	gint page;

	offset = g_hash_table_lookup (nodes2offsets, node);
	page = GPOINTER_TO_INT (g_hash_table_lookup (offsets2pages, offset));
	if (!page) {
	  /* We got a badly formed tag table.  Most probably bash info page :(
	   * Have to find the correct page.
	   * The bash info file assumess 3 bytes more per node than reality
	   * hence we check backwards in steps of 3
	   * The first one we come across should be the correct node
	   * (fingers crossed at least)
	   */
	  gchar *new_offset = NULL;
	  int_offset = atoi (offset);

	  while (!page && int_offset > 0) {
	    int_offset-=3;
	    g_free (new_offset);
	    new_offset = g_strdup_printf ("%d", int_offset);
	    page = GPOINTER_TO_INT (g_hash_table_lookup (offsets2pages, new_offset));	    
	  }
	  g_free (new_offset);
	}
	return page;
}

static GtkTreeIter
*node2iter (GHashTable *nodes2iters, char *node)
{
	GtkTreeIter *iter;

	iter = g_hash_table_lookup (nodes2iters, node);
	d (if (!iter) g_print ("Could not retrieve iter for node !%s!\n", node));
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

  if (!it || !comp)
    return NULL;

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

  g_free (r);
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
	if (processed_table[page])
		return;
	processed_table[page] = 1;
	
	d (g_print ("-- Processing Page %s\n\tParent: %s\n", node, up));

	iter = g_malloc0 (sizeof (GtkTreeIter));
	/* check to see if we need to process our parent and siblings */
	if (up && g_ascii_strncasecmp (up, "(dir)", 5) && strcmp (up, "Top"))
	{
		page = node2page (nodes2offsets, offsets2pages, up);
		if (!processed_table[page])
		{
		  d (g_print ("%% Processing Node %s\n", up));
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
		  d (g_print ("%% Processing Node %s\n", prev));
		  process_page (tree, nodes2offsets, offsets2pages,
				nodes2iters, processed_table, page_list,
				page_list[page]);
		}
	    }
	  }
	
	/* by this point our parent and older sibling should be processed */
	if (!up || !g_ascii_strcasecmp (up, "(dir)") || !strcmp (up, "Top"))
	{
	  d (g_print ("\t> no parent\n"));
		if (!prev || !g_ascii_strcasecmp (prev, "(dir)"))
		{
		  d (g_print ("\t> no previous\n"));
			gtk_tree_store_append (tree, iter, NULL);
		}
		else if (prev) {
		  GtkTreeIter *real;
		  real = find_real_top (GTK_TREE_MODEL (tree), 
					node2iter (nodes2iters, prev));
		  if (real) {
		    gtk_tree_store_insert_after (tree, iter, NULL,
						 real);
		    g_free (real);
		  }
		  else 
		    gtk_tree_store_append (tree, iter, NULL);
		}
	}
	else if (!prev || !g_ascii_strcasecmp (prev, "(dir)") || !strcmp (prev, up))
	{
	  d (g_print ("\t> no previous\n"));
		gtk_tree_store_append (tree, iter,
			node2iter (nodes2iters, up));
	}
	else if (up && prev)
	{
	  GtkTreeIter *upit = node2iter (nodes2iters, up);
	  GtkTreeIter *previt = node2iter (nodes2iters, prev);
	  GtkTreeIter *nit;
	  d (g_print ("+++ Parent: %s Previous: %s\n", up, prev));
	  
	  d (if (upit) g_print ("++++ Have parent node!\n"));
	  d (if (previt) g_print ("++++ Have previous node!\n"));
	  nit = find_real_sibling (GTK_TREE_MODEL (tree), previt, upit);
	  if (nit) {
	    gtk_tree_store_insert_after (tree, iter,
					 upit,
					 nit);
	    g_free (nit);
	  }
	  else
	    gtk_tree_store_append (tree, iter, upit);
	}
	else
	{
	  d (g_print ("# node %s was not put in tree\n", node));
	  return;
	}

	d (if (iter) g_print ("Have a valid iter, storing for %s\n", node));
	g_hash_table_insert (nodes2iters, g_strdup (node), iter);
	d (g_print ("size: %i\n", g_hash_table_size (nodes2iters)));
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
	GHashTable *nodes2offsets = NULL;
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
	str = NULL;
	
	pages = 0;
	offset = 0;
	chained_info = FALSE;

	offsets2pages = g_hash_table_new (g_str_hash, g_str_equal);
	
	for (ptr = page_list; *ptr != NULL; ptr++)
	{
	  d (g_print ("page %i at offset %i\n", pages, offset));
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
		  d (g_print ("Have the Tag Table\n"));
			/* this needs to be freed later too */
			nodes2offsets = process_tag_table (*ptr);
			break;
		}
		else if (pt == PAGE_INDIRECT)
		{
		  d (g_print ("Have the indirect mapping table\n"));
			chained_info = TRUE;
			str = process_indirect_map (*ptr, file);
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
		  d (g_print ("page %i at offset %i\n", pages, offset));
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

	char *page_no = NULL;
	char *page_name = NULL;
	char *page_content = NULL;
	
	d (g_print ("Decended\n"));
	do
	{
		gtk_tree_model_get (GTK_TREE_MODEL (tree), &iter,
				COLUMN_PAGE_NO, &page_no,
				COLUMN_PAGE_NAME, &page_name,
				COLUMN_PAGE_CONTENT, &page_content,
				-1);
		d (g_print ("Got Section: %s\n", page_name));
		if (strstr (page_content, "* Menu:")) {
		  newnode = yelp_info_parse_menu (tree, node, page_content);
		} else {
		  newnode = xmlNewTextChild (*node, NULL,
					     BAD_CAST "Section",
					     NULL);
		  xmlNewTextChild (newnode, NULL,
				   BAD_CAST "para",
				   BAD_CAST page_content);
		  
		}
		/* if we free the page content, now it's in the XML, we can
		 * save some memory */
		g_free (page_content);
		page_content = NULL;

		xmlNewProp (newnode, BAD_CAST "id", 
			    BAD_CAST g_strdup (page_no));
		xmlNewProp (newnode, BAD_CAST "name", 
			    BAD_CAST g_strdup (page_name));
		if (gtk_tree_model_iter_children (GTK_TREE_MODEL (tree),
				&children,
				&iter))
			parse_tree_level (tree, &newnode, children);
	}
	while (gtk_tree_model_iter_next (GTK_TREE_MODEL (tree), &iter));
	d (g_print ("Ascending\n"));
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
	d (else g_print ("Empty tree?\n"));

	/*
	xmlDocDumpFormatMemory (doc, &xmlbuf, &bufsiz, 1);
	g_print ("XML follows:\n%s\n", xmlbuf);
	*/

	return doc;
}

static gchar *section_id;

gboolean
resolve_frag_id (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
		 gpointer data)
{
  gchar *page_no;
  gchar *page_name;

  gtk_tree_model_get (GTK_TREE_MODEL (model), iter,
		      COLUMN_PAGE_NO, &page_no,
		      COLUMN_PAGE_NAME, &page_name,
		      -1);

  if (g_str_equal (page_name, data)) {
    section_id = g_strdup (page_no);
    return TRUE;
  }

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
		      gchar *page_content)
{
  gchar **split;
  gchar **menuitems;
  xmlNodePtr newnode;
  int i=0;

  split = g_strsplit (page_content, "* Menu:", 2);

  newnode = xmlNewChild (*node, NULL,
			 BAD_CAST "Section", NULL);

  xmlNewTextChild (newnode, NULL,
	       BAD_CAST "para", BAD_CAST g_strconcat (split[0],
						      "\n* Menu:",
						      NULL));

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
      gtk_tree_model_foreach (GTK_TREE_MODEL (tree), resolve_frag_id, xref);
      
      if (ref == NULL) { /* A standard type menu */
	ref1 = xmlNewTextChild (mholder, NULL, BAD_CAST "a",
					BAD_CAST g_strconcat (title, "::", 
							      NULL));
	xmlNewProp (ref1, BAD_CAST "href", BAD_CAST g_strconcat ("#", section_id, 
							      NULL));
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
	xmlNewProp (ref1, BAD_CAST "href", BAD_CAST g_strconcat ("#", section_id, 
							      NULL));
	xmlNewTextChild (mholder, NULL, BAD_CAST "spacing",
			 BAD_CAST sp);
	ref1 = xmlNewTextChild (mholder, NULL, BAD_CAST "a",
					 BAD_CAST g_strconcat (g_strstrip(ref), 
							       ".", NULL));
	xmlNewProp (ref1, BAD_CAST "href", BAD_CAST g_strconcat ("#", section_id, 
							       NULL));
	
	g_free (sp);
      }
      xmlNewTextChild (mholder, NULL, BAD_CAST "para",
		       BAD_CAST desc);
    } else {
      xmlNewTextChild (newnode, NULL, BAD_CAST "para",
		       BAD_CAST menuitems[i]);
      
    }
    i++;
    
  }
  g_free (section_id);
  
  return newnode;
}
