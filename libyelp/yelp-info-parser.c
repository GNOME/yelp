/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2005 Davyd Madeley <davyd@madeley.id.au>
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Davyd Madeley  <davyd@madeley.id.au>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>

#include "yelp-info-parser.h"
#include "yelp-magic-decompressor.h"
#include "yelp-debug.h"


static GtkTreeIter *  find_real_top                      (GtkTreeModel *model, 
							  GtkTreeIter *it);
static GtkTreeIter *  find_real_sibling                  (GtkTreeModel *model,
							  GtkTreeIter *it, 
							  GtkTreeIter *comp);
static xmlNodePtr     yelp_info_parse_menu               (GtkTreeStore *tree,
							  xmlNodePtr *node,
							  gchar *page_content,
							  gboolean notes);
static gboolean       get_menuoptions                    (gchar *line, 
							  gchar **title, 
							  gchar **ref, 
							  gchar **desc, 
							  gchar **xref);
static gboolean       resolve_frag_id                    (GtkTreeModel *model, 
							  GtkTreePath *path, 
							  GtkTreeIter *iter,
							  gpointer data);
static void	      info_process_text_notes            (xmlNodePtr *node, 
							  gchar *content,
							  GtkTreeStore
							  *tree);

/*
  Used to output the correct <heading level="?" /> tag.
 */
static const gchar* level_headings[] = { NULL, "1", "2", "3" };

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
  gchar *title;
  gchar *text;
  gchar *alt;
  xmlNodePtr img;
  GHashTable *h = info_image_get_attributes (g_match_info_fetch (match_info, 1));
  gchar *source;
  if (h)
    source = (gchar*)g_hash_table_lookup (h, "src");

  if (!h || !source || !*source)
    return xmlNewTextChild (parent, NULL, BAD_CAST "para",
                            BAD_CAST "[broken image]");

  title = (gchar*)g_hash_table_lookup (h, "title");
  text = (gchar*)g_hash_table_lookup (h, "text");
  alt = (gchar*)g_hash_table_lookup (h, "alt");
  g_hash_table_destroy (h);
  img = xmlNewChild (parent, NULL, BAD_CAST "img", NULL);
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
  If every element of `str' is `ch' then return TRUE, else FALSE.
 */
static gboolean
string_all_char_p (const gchar* str, gchar ch)
{
  for (; *str; str++) {
    if (*str != ch) return FALSE;
  }
  return TRUE;
}

/*
  If `line' is a line of '*', '=' or '-', return 1,2,3 respectively
  for the heading level. If it's anything else, return 0.
 */
static int
header_underline_level (const gchar* line)
{
  if (*line != '*' && *line != '=' && *line != '-')
    return 0;

  if (string_all_char_p (line, '*')) return 1;
  if (string_all_char_p (line, '=')) return 2;
  if (string_all_char_p (line, '-')) return 3;

  return 0;
}

/*
  Use g_strjoinv to join up the strings from `strings', but they might
  not actually be a null-terminated array. `end' should be strings+n,
  where I want the first n strings (strings+0, ..., strings+(n-1)). It
  shouldn't point outside of the array allocated, but it can point at
  the null string at the end.
 */
static gchar*
join_strings_subset (const gchar *separator,
                     gchar** strings, gchar** end)
{
  gchar *ptr;
  gchar *glob;

  g_assert(end > strings);

  ptr = *end;
  *end = NULL;
  
  glob = g_strjoinv (separator, strings);
  *end = ptr;
  return glob;
}

/*
  Create a text node, child of `parent', with the lines strictly
  between `first' and `last'.
*/
static void
lines_subset_text_child (xmlNodePtr parent, xmlNsPtr ns,
                         gchar** first, gchar** last)
{
  /* TODO? Currently we're copying the split strings again, which is
     less efficient than somehow storing lengths and using a sort of
     window on `content'. But that's much more difficult, so unless
     there's a problem, let's go with the stupid approach. */
  gchar *glob;

  if (last > first) {
    glob = join_strings_subset ("\n", first, last);
    xmlAddChild (parent, xmlNewText (BAD_CAST glob));
    g_free (glob);
  }
}

/*
  Convert body text CONTENT to xml nodes. This function is responsible
  for spotting headings etc and splitting them out correctly.

  paragraph is as described in info_body_text, but cannot be null.

  If `inline_p' is true, end with a <para1> tag. Otherwise, end with a
  <para> tag.

  TODO: IWBN add a regex match for *Note: here and call the *Note ==>
  <a href> logic of info_process_text_notes from here.
 */
static void
info_body_parse_text (xmlNodePtr parent, xmlNodePtr *paragraph,
                      xmlNsPtr ns,
                      gboolean inline_p, const gchar *content)
{
  /* The easiest things to spot are headings: they look like a line of
   * '*','=' or '-', corresponding to heading levels 1,2 or 3. To spot
   * them, we split content into single lines and work with them. */
  gchar **lines = g_strsplit (content, "\n", 0);
  gchar **first = lines, **last = lines;
  int header_level;
  xmlNodePtr header_node;

  /* Deal with the possibility that `content' is empty */
  if (*lines == NULL) {
    if (!inline_p) {
      xmlNewTextChild (parent, NULL, BAD_CAST "para", BAD_CAST "");
    }
    return;
  }

  /* Use a pair of pointers, first and last, which point to two lines,
   * the chunk of the body we're displaying (inclusive) */
  for (; *last; last++) {

    /* Check for a blank line */
    if (**last == '\0') {
      if (last != first) {
        if (!*paragraph) {
          *paragraph = xmlNewChild (parent, ns, BAD_CAST "para", NULL);
        }
        lines_subset_text_child (*paragraph, ns, first, last);
      }
      /* On the next iteration, last==first both pointing at the next
         line. */
      first = last+1;
      *paragraph = NULL;

      continue;
    }

    /* Check for a header */
    header_level = header_underline_level (*last);
    if (header_level) {
      /* Write out any lines beforehand */
      lines_subset_text_child (parent, ns, first, last-1);
      /* Now write out the actual header line */
      header_node = xmlNewTextChild (parent, ns, BAD_CAST "header",
                                     BAD_CAST *(last-1));
      xmlNewProp (header_node, BAD_CAST "level",
                  BAD_CAST level_headings[header_level]);
      
      first = last+1;
      last = first-1;
    }
  }

  /* Write out any lines left */
  if (!*paragraph) {
    *paragraph = xmlNewChild (parent, ns, BAD_CAST "para", NULL);
  }
  lines_subset_text_child (*paragraph, ns, first, last);
  
  g_strfreev (lines);
}

/*
  info_body_text is responsible for taking a hunk of the info page's
  body and turning it into paragraph tags. It searches out images and
  marks them up properly if necessary.

  parent should be the node in which we're currently storing text and
  paragraph a pointer to a <para> tag or NULL. At blank lines, we
  finish with the current para tag and switch to a new one.

  It uses info_body_parse_text to mark up the actual bits of text.
 */
static void
info_body_text (xmlNodePtr parent, xmlNodePtr *paragraph, xmlNsPtr ns,
                gboolean inline_p, gchar const *content)
{
  xmlNodePtr thepara = NULL;
  gint content_len;
  gint pos;
  GRegex *regex;
  GMatchInfo *match_info;
  gchar *after;
  if (paragraph == NULL) paragraph = &thepara;

  if (!strstr (content, INFO_C_IMAGE_TAG_OPEN)) {
    info_body_parse_text (parent, paragraph, ns, inline_p, content);
    return;
  }

  content_len = strlen (content);
  pos = 0;
  regex = g_regex_new ("(" INFO_C_IMAGE_TAG_OPEN_RE "((?:[^" INFO_TAG_1 "]|[^" INFO_C_TAG_0 "]+" INFO_TAG_1 ")*)" INFO_C_TAG_CLOSE_RE ")", 0, 0, NULL);

  g_regex_match (regex, content, 0, &match_info);
  while (g_match_info_matches (match_info))
    {
      gint image_start;
      gint image_end;
      gboolean image_found = g_match_info_fetch_pos (match_info, 0,
						     &image_start, &image_end);
      gchar *before = g_strndup (&content[pos], image_start - pos);
      pos = image_end + 1;
      info_body_parse_text (parent, paragraph, NULL, TRUE, before);
      g_free (before);

      /* End the paragraph that was before */
      *paragraph = NULL;

      if (image_found)
	info_insert_image (parent, match_info);
      g_match_info_next (match_info, NULL);
    }
  after = g_strndup (&content[pos], content_len - pos);
  info_body_parse_text (parent, paragraph, NULL, TRUE, after);
  g_free (after);
}

/* Part 1: Parse File Into Tree Store */

enum
{
	PAGE_TAG_TABLE,
	PAGE_NODE,
	PAGE_INDIRECT,
	PAGE_OTHER
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
*open_info_file (const gchar *file)
{
    GFile *gfile;
    GConverter *converter;
    GFileInputStream *file_stream;
    GInputStream *stream;
    gchar buf[1024];
    gssize bytes;
    GString *string;
    gchar *str;
    gsize i;

    gfile = g_file_new_for_path (file);
    file_stream = g_file_read (gfile, NULL, NULL);
    converter = (GConverter *) yelp_magic_decompressor_new ();
    stream = g_converter_input_stream_new ((GInputStream *) file_stream, converter);
    string = g_string_new (NULL);

    while ((bytes = g_input_stream_read (stream, buf, 1024, NULL, NULL)) > 0)
        g_string_append_len (string, buf, bytes);

    g_object_unref (stream);

    str = string->str;

    /* C/glib * cannot really handle \0 in strings, convert. */
    for (i = 0; i < (string->len - 1); i++)
        if (str[i] == INFO_TAG_OPEN[0] && str[i+1] == INFO_TAG_OPEN[1])
            str[i] = INFO_C_TAG_OPEN[0];

    g_string_free (string, FALSE);

    return str;
}

static gchar *
find_info_part (gchar *part_name, const gchar *base)
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
*process_indirect_map (char *page, const gchar *file)
{
	char **lines;
	char **ptr;
	char *composite = NULL;
        size_t composite_len = 0;

	lines = g_strsplit (page, "\n", 0);

        /*
          Go backwards down the list so that we allocate composite
          big enough the first time around.
        */
	for (ptr = lines + 1; *ptr != NULL; ptr++);
	for (ptr--; ptr != lines; ptr--)
	{
		char **items;
		char *filename;
		char *str;
		char **pages;
		gsize offset;
		gsize plength;

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

			offset = (gsize) atoi (items[1]);
			plength = strlen(pages[1]);
			
			debug_print (DB_DEBUG, "Need to make string %s+%li bytes = %li\n",
				    items[1], plength,
				    offset + plength);
			
			if (!composite) /* not yet created, malloc it */
			{
				composite_len = offset + plength;
				composite = g_malloc (sizeof (char) *
						      (composite_len + 1));
				memset (composite, '-', composite_len);
				composite[composite_len] = '\0';
			}

                        /* Because we're going down the list
                         * backwards, plength should always be short
                         * enough to fit in the memory allocated. But
                         * in case something's broken/malicious, we
                         * should check anyway.
                         */
                        if (offset > composite_len)
                          continue;
                        if (plength + offset + 1 > composite_len)
                          plength = composite_len - offset - 1;

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

/*
  Open up the relevant info file and read it all into memory. If there
  is an indirect table thingy, we resolve that as we go.

  Returns a NULL-terminated list of pointers to pages on success and
  NULL otherwise.
 */
static gchar**
expanded_info_file (const gchar *file)
{
  gchar *slurp = open_info_file (file);
  gchar **page_list;
  gchar **page;

  if (!slurp) return NULL;

  /* TODO: There's a lot of copying of bits of memory here. With a bit
   * more effort we could avoid it. Either we should fix this or
   * measure the time taken and decide it's irrelevant...
   *
   * Note: \x1f\n is ^_\n
   */
  page_list = g_strsplit (slurp, "\x1f\n", 0);

  g_free (slurp);

  for (page = page_list; *page != NULL; page++) {
    if (page_type (*page) == PAGE_INDIRECT) {

      slurp = process_indirect_map (*page, file);
      g_strfreev (page_list);

      if (!slurp)
        return NULL;

      page_list = g_strsplit (slurp, "\x1f\n", 0);
      g_free (slurp);
      break;
    }
  }

  return page_list;
}

/*
  Look for strings in source by key. For example, we extract "blah"
  from "Node: blah," when the key is "Node: ". To know when to stop,
  there are two strings: end and cancel.

  If we find a character from end first, return a copy of the string
  up to (not including) that character. If we find a character of
  cancel first, return NULL. If we find neither, return the rest of
  the string.

  cancel can be NULL, in which case, we don't do its test.
 */
static char*
get_value_after_ext (const char *source, const char *key,
                     const char *end, const char *cancel)
{
  char *start;
  size_t not_end, not_cancel;

  start = strstr (source, key);
  if (!start) return NULL;

  start += strlen (key);

  not_end = strcspn (start, end);
  not_cancel = (cancel) ? strcspn (start, cancel) : not_end + 1;

  if (not_cancel < not_end)
    return NULL;

  return g_strndup (start, not_end);
}

static char*
get_value_after (const char* source, const char *key)
{
  return get_value_after_ext (source, key, ",", "\n\x7f");
}

static int
node2page (GHashTable *nodes2pages, char *node)
{
  gpointer p;

  if (g_hash_table_lookup_extended (nodes2pages, node,
                                    NULL, &p))
    return GPOINTER_TO_INT(p);

  /* This shouldn't happen: we should only ever have to look up pages
   * that exist. */
  g_return_val_if_reached (0);
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
process_page (GtkTreeStore *tree,
              GHashTable *nodes2pages, GHashTable *nodes2iters,
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
	if (node && g_str_equal (node, "Top") && prev != NULL) {
	  g_free (prev);
	  prev = NULL;
	}

	/* check to see if this page has been processed already */
	page = node2page (nodes2pages, node);
	if (processed_table[page]) {
		return;
	}
	processed_table[page] = 1;
	
	debug_print (DB_DEBUG, "-- Processing Page %s\n\tParent: %s\n", node, up);

	iter = g_slice_alloc0 (sizeof (GtkTreeIter));
	/* check to see if we need to process our parent and siblings */
	if (up && g_ascii_strncasecmp (up, "(dir)", 5) && strcmp (up, "Top"))
	{
		page = node2page (nodes2pages, up);
		if (!processed_table[page])
		{
		  debug_print (DB_DEBUG, "%% Processing Node %s\n", up);
                  process_page (tree, nodes2pages,
				nodes2iters, processed_table, page_list,
				page_list[page]);
		}
	}
	if (prev && g_ascii_strncasecmp (prev, "(dir)", 5))
	  {
	    if (node && strncmp (node, "Top", 3)) {
	      /* Special case the Top node to always appear first */
	    } else {
	      page = node2page (nodes2pages, prev);
	      if (!processed_table[page])
		{
		  debug_print (DB_DEBUG, "%% Processing Node %s\n", prev);
		  process_page (tree, nodes2pages,
				nodes2iters, processed_table, page_list,
				page_list[page]);
		}
	    }
	  }
	
	/* by this point our parent and older sibling should be processed */
	if (!up || !g_ascii_strcasecmp (up, "(dir)"))
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
	  node2page (nodes2pages, node));*/
	tmp = g_strdup (node);
	tmp = g_strdelimit (tmp, " ", '_');
	gtk_tree_store_set (tree, iter,
			    INFO_PARSER_COLUMN_PAGE_NO, tmp,
			    INFO_PARSER_COLUMN_PAGE_NAME, node,
			    INFO_PARSER_COLUMN_PAGE_CONTENT, parts[2],
			    -1);

	g_free (tmp);
	g_free (node);
	g_free (up);
	g_free (prev);
	g_free (next);
	g_strfreev (parts);
}

struct TagTableFix {
  GHashTable *nodes2pages; /* Build this... */
  GHashTable *pages2nodes; /* ... using this. */
};

static void
use_offset2page (gpointer o, gpointer p, gpointer ud)
{
  struct TagTableFix* ttf = (struct TagTableFix*)ud;

  const gchar* node = g_hash_table_lookup (ttf->pages2nodes, p);
  if (node) {
    g_hash_table_insert (ttf->nodes2pages, g_strdup (node), p);
  }
}

/*
  We had a nodes2offsets hash table, but sometimes these things
  lie. How terribly rude. Anyway, use offsets2pages and pages2nodes
  (and injectivity!) to construct the nodes2pages hash table.
*/
static GHashTable *
make_nodes2pages (GHashTable* offsets2pages,
                  GHashTable* pages2nodes)
{
  struct TagTableFix ttf;

  ttf.nodes2pages =
    g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  ttf.pages2nodes = pages2nodes;

  g_hash_table_foreach (offsets2pages, use_offset2page, &ttf);

  return ttf.nodes2pages;
}

/**
 * Parse file into a GtkTreeStore containing useful information that we can
 * later convert into a nice XML document or something else.
 */
GtkTreeStore
*yelp_info_parser_parse_file (char *file)
{
	gchar **page_list;
	char **ptr;
	int pages;
	int offset;
	GHashTable *offsets2pages = NULL;
	GHashTable *pages2nodes = NULL;
        GHashTable *nodes2pages = NULL;
	GHashTable *nodes2iters = NULL;
	int *processed_table;
	GtkTreeStore *tree;
	int pt;
	
	page_list = expanded_info_file (file);
	if (!page_list)
          return NULL;
	
	pages = 0;
	offset = 0;

	offsets2pages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
					       NULL);
	pages2nodes = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, 
					     g_free);

	for (ptr = page_list; *ptr != NULL; ptr++)
	{
	  gchar *name = NULL;

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
          if (pt == PAGE_INDIRECT) {
            g_warning ("Found an indirect page in a file "
                       "we thought we'd expanded.");
          }
	}

        /* Now consolidate (and correct) the two hash tables */
        nodes2pages = make_nodes2pages (offsets2pages, pages2nodes);

	g_hash_table_destroy (offsets2pages);
        g_hash_table_destroy (pages2nodes);

	processed_table = g_malloc0 (pages * sizeof (int));
	tree = gtk_tree_store_new (INFO_PARSER_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_STRING);
	nodes2iters = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
					     (GDestroyNotify) gtk_tree_iter_free);

	for (ptr = page_list; *ptr != NULL; ptr++)
	{
	  if (page_type (*ptr) != PAGE_NODE) continue;
	  process_page (tree, nodes2pages, nodes2iters,
			processed_table, page_list, *ptr);
	}

	g_strfreev (page_list);

	g_hash_table_destroy (nodes2iters);
	g_hash_table_destroy (nodes2pages);

	g_free (processed_table);

	return tree;
}

/* End Part 1 */
/* Part 2: Parse Tree into XML */
static void
parse_tree_level (GtkTreeStore *tree, xmlNodePtr *node, GtkTreeIter iter)
{
    GtkTreeIter children, parent;
	xmlNodePtr newnode;

	char *page_no = NULL;
	char *page_name = NULL;
	char *page_content = NULL;
	gboolean notes = FALSE;

	debug_print (DB_DEBUG, "Decended\n");
	do
	{
		gtk_tree_model_get (GTK_TREE_MODEL (tree), &iter,
				INFO_PARSER_COLUMN_PAGE_NO, &page_no,
				INFO_PARSER_COLUMN_PAGE_NAME, &page_name,
				INFO_PARSER_COLUMN_PAGE_CONTENT, &page_content,
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
		    info_body_text (newnode, NULL, NULL, FALSE, page_content);

		  else {
		    /* Handle notes here */
		    info_process_text_notes (&newnode, page_content, tree);
		  }
		}
		/* if we free the page content, now it's in the XML, we can
		 * save some memory */
		g_free (page_content);
		page_content = NULL;

                if (gtk_tree_model_iter_parent (GTK_TREE_MODEL (tree), &parent, &iter)) {
                    gchar *parent_id;
                    gtk_tree_model_get (GTK_TREE_MODEL (tree), &parent,
                                        INFO_PARSER_COLUMN_PAGE_NO, &parent_id,
                                        -1);
                    xmlNewProp (newnode, BAD_CAST "up", BAD_CAST parent_id);
                    g_free (parent_id);
                }

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
		      INFO_PARSER_COLUMN_PAGE_NO, &page_no,
		      INFO_PARSER_COLUMN_PAGE_NAME, &page_name,
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

/* Find the first non whitespace character in str or return pointer to the
 * '\0' if there isn't one. */
static gchar*
first_non_space (gchar* str)
{
  /* As long as str is null terminated, this is ok! */
  while (g_ascii_isspace (*str)) str++;
  return str;
}

static xmlNodePtr
yelp_info_parse_menu (GtkTreeStore *tree, xmlNodePtr *node, 
		      gchar *page_content, gboolean notes)
{
  gchar **split;
  gchar **menuitems;
  gchar *tmp = NULL;
  xmlNodePtr newnode, menu_node = NULL, mholder = NULL;
  int i=0;

  split = g_strsplit (page_content, "* Menu:", 2);
  
  newnode = xmlNewChild (*node, NULL,
			 BAD_CAST "Section", NULL);
    

  if (!notes)
    info_body_text (newnode, NULL, NULL, FALSE, split[0]);
  else {
    info_process_text_notes (&newnode, split[0], tree);
  }

  menuitems = g_strsplit (split[1], "\n", -1);
  g_strfreev (split);

  /* The output xml should look something like the following:

     <menu>
       <menuholder>
         <a href="xref:Help-Inv">Help-Inv</a>
         <para1>Invisible text in Emacs Info.</para1>
       </menuholder>
       <menuholder>
         <a href="xref:Help-M">Help-M</a>
         <para1>Menus.</para1>
       </menuholder>
       ...
     </menu>

     (from the top page of info:info). Note the absence of *'s and
     ::'s on the links.

     If there's a line with no "* Blah::", it looks like a child of
     the previous menu item so (for i > 0) deal with that correctly by
     not "closing" the <menuholder> tag until we find the next
     start.
  */

  if (menuitems[0] != NULL) {
    /* If there are any menu items, make the <menu> node */
    menu_node = xmlNewChild (newnode, NULL, BAD_CAST "menu", NULL);
  }

  while (menuitems[i] != NULL) {
    gboolean menu = FALSE;
    gchar *title = NULL;
    gchar *ref = NULL;
    gchar *desc = NULL;
    gchar *xref = NULL;
    gchar *link_text = NULL;
    xmlNodePtr ref1;

    menu = get_menuoptions (menuitems[i], &title, &ref, &desc, &xref);

    if (menu && (*title == '\0' || *(title + 1) == '\0')) {
      g_warning ("Info title unexpectedly short for menu item (%s)",
                 menuitems[i]);
      menu = FALSE;
    }

    if (menu) {
      mholder = xmlNewChild (menu_node, NULL, BAD_CAST "menuholder", NULL);
      gtk_tree_model_foreach (GTK_TREE_MODEL (tree), resolve_frag_id, &xref);
      
      if (ref == NULL) { /* A standard type menu */
        /* title+2 skips the "* ". We know we haven't jumped over the
           end of the string because strlen (title) >= 3 */
        link_text = g_strdup (title+2);

        ref1 = xmlNewTextChild (mholder, NULL, BAD_CAST "a",
                                BAD_CAST link_text);

        tmp = g_strconcat ("xref:", xref, NULL);
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
	
        link_text = g_strdup (title);

	ref1 = xmlNewTextChild (mholder, NULL, BAD_CAST "a",
                                BAD_CAST link_text);
        tmp = g_strconcat ("xref:", xref, NULL);
	xmlNewProp (ref1, BAD_CAST "href", BAD_CAST tmp);
        g_free (tmp);
	xmlNewTextChild (mholder, NULL, BAD_CAST "spacing",
			 BAD_CAST sp);
	tmp = g_strconcat (g_strstrip(ref), ".", NULL);
	ref1 = xmlNewTextChild (mholder, NULL, BAD_CAST "a",
				BAD_CAST tmp);
	g_free (tmp);
        tmp = g_strconcat ("xref:", xref, NULL);
	xmlNewProp (ref1, BAD_CAST "href", BAD_CAST tmp);

        g_free (tmp);
	g_free (sp);
      }

      tmp = g_strconcat ("\n", first_non_space (desc), NULL);

      /*
        Don't print the link text a second time, because that looks
        really stupid.

        We don't do a straight check for equality because lots of
        .info files have something like

          * Foo::    Foo.

        Obviously if the longer explanation has more afterwards, we
        don't want to omit it, which is why there's the strlen test.
      */
      if (strncmp (link_text, tmp + 1, strlen (link_text)) ||
          strlen (link_text) + 1 < strlen (tmp + 1)) {
        xmlNewTextChild (mholder, NULL,
                         BAD_CAST "para1", BAD_CAST tmp);
      }

      g_free (tmp);
      g_free (link_text);
    }
    else if (*(menuitems[i]) != '\0') {
      tmp = g_strconcat ("\n", first_non_space (menuitems[i]), NULL);
      xmlNewTextChild (mholder ? mholder : menu_node,
                       NULL, BAD_CAST "para1",
		       BAD_CAST tmp);
      g_free (tmp);
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
  xmlNodePtr ref1;
  xmlNodePtr paragraph = NULL;
  gboolean first = TRUE;

  /*
    Split using the regular expression

      \*[Nn]ote(?!_)

    which deals with either case and the last bit is a lookahead so
    that we don't split on things of the form *Note:_, which aren't
    real notes.
  */
  notes = g_regex_split_simple ("\\*[Nn]ote(?!_)", content, 0, 0);

  for (current = notes; *current != NULL; current++) {
    gchar *url, **urls;
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
      info_body_text (*node, &paragraph, NULL, TRUE, (*current));
      continue;
    }

    /* If we got to here, we now gotta parse the note reference */
    append = strchr (*current, ':');
    if (!append) {
      info_body_text (*node, &paragraph, NULL, TRUE, *current);
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
      info_body_text (*node, &paragraph, NULL, TRUE, *current);
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
    url = g_strndup (*current, append - (*current));

    /* Save a copy of the unadulterated link text for later. */
    link_text = g_strconcat ("*Note", url, NULL);

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
        gchar *old = url;
        while (*next == ' ')
          next++;
        next--;
        url_copy = g_strndup (url, break_point-url);
        url = g_strconcat (url_copy, next, NULL);
        g_free (old);
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
        href = g_strconcat ("xref:", link, NULL);
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
      href = g_strconcat ("xref:", frag, NULL);
      g_free (frag);
    }

    /* Check we've got a valid paragraph node */
    if (!paragraph) {
      paragraph = xmlNewChild (*node, NULL, BAD_CAST "para", NULL);
    }

    /*
      Now we're supposed to actually render the link. I have a list of
      bits of URL and actually this is really easy - I want to have
      the link *text* exactly the same as it appeared in the .info
      file, so don't use the list of strings urls, instead use the
      whole lot: url (complete with embedded newlines etc.)
    */
    ref1 = xmlNewTextChild (paragraph, NULL, BAD_CAST "a",
                            BAD_CAST link_text);
    g_free (link_text);
    xmlNewProp (ref1, BAD_CAST "href", BAD_CAST href);

    g_strfreev (urls);

    /* Finally, we can add the following text as required */
    info_body_text (*node, &paragraph, NULL, TRUE, append);

    g_free (url);
    g_free (href);
  }
  g_strfreev (notes);
}
