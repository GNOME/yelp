/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#include "yelp-util.h"

#include <libgnomevfs/gnome-vfs-utils.h>
#include <string.h>

GtkTreeIter *
yelp_util_contents_add_section (GtkTreeStore *store,
                                GtkTreeIter  *parent,
                                YelpSection  *section)
{
	GtkTreeIter *iter;
	
	g_return_val_if_fail (GTK_IS_TREE_STORE (store), NULL);
	g_return_val_if_fail (section != NULL, NULL);

	iter = g_new0 (GtkTreeIter, 1);
	
	gtk_tree_store_append (store, iter, parent);
	
	gtk_tree_store_set (store, iter,
			    0, section->name,
			    1, section,
			    2, TRUE,
			    -1);
	return iter;
}

/* This code comes from gnome vfs: */


static gboolean
is_uri_relative (const char *uri)
{
	const char *current;

	/* RFC 2396 section 3.1 */
	for (current = uri ; 
		*current
		&& 	((*current >= 'a' && *current <= 'z')
			 || (*current >= 'A' && *current <= 'Z')
			 || (*current >= '0' && *current <= '9')
			 || ('-' == *current)
			 || ('+' == *current)
			 || ('.' == *current)) ;
	     current++);

	return  !(':' == *current);
}


/*
 * Remove "./" segments
 * Compact "../" segments inside the URI
 * Remove "." at the end of the URL 
 * Leave any ".."'s at the beginning of the URI
 
*/
/*
 * FIXME this is not the simplest or most time-efficent way
 * to do this.  Probably a far more clear way of doing this processing
 * is to split the path into segments, rather than doing the processing
 * in place.
 */
static void
remove_internal_relative_components (char *uri_current)
{
	char *segment_prev, *segment_cur;
	gsize len_prev, len_cur;

	len_prev = len_cur = 0;
	segment_prev = NULL;

	segment_cur = uri_current;

	while (*segment_cur) {
		len_cur = strcspn (segment_cur, "/");

		if (len_cur == 1 && segment_cur[0] == '.') {
			/* Remove "." 's */
			if (segment_cur[1] == '\0') {
				segment_cur[0] = '\0';
				break;
			} else {
				memmove (segment_cur, segment_cur + 2, strlen (segment_cur + 2) + 1);
				continue;
			}
		} else if (len_cur == 2 && segment_cur[0] == '.' && segment_cur[1] == '.' ) {
			/* Remove ".."'s (and the component to the left of it) that aren't at the
			 * beginning or to the right of other ..'s
			 */
			if (segment_prev) {
				if (! (len_prev == 2
				       && segment_prev[0] == '.'
				       && segment_prev[1] == '.')) {
				       	if (segment_cur[2] == '\0') {
						segment_prev[0] = '\0';
						break;
				       	} else {
						memmove (segment_prev, segment_cur + 3, strlen (segment_cur + 3) + 1);

						segment_cur = segment_prev;
						len_cur = len_prev;

						/* now we find the previous segment_prev */
						if (segment_prev == uri_current) {
							segment_prev = NULL;
						} else if (segment_prev - uri_current >= 2) {
							segment_prev -= 2;
							for ( ; segment_prev > uri_current && segment_prev[0] != '/' 
							      ; segment_prev-- );
							if (segment_prev[0] == '/') {
								segment_prev++;
							}
						}
						continue;
					}
				}
			}
		}

		/*Forward to next segment */

		if (segment_cur [len_cur] == '\0') {
			break;
		}
		 
		segment_prev = segment_cur;
		len_prev = len_cur;
		segment_cur += len_cur + 1;	
	}
	
}


/* If I had known this relative uri code would have ended up this long, I would
 * have done it a different way
 */
char *
yelp_util_resolve_relative_uri (const char *base_uri,
				const char *uri)
{
	char *result = NULL;

	g_return_val_if_fail (base_uri != NULL, g_strdup (uri));
	g_return_val_if_fail (uri != NULL, NULL);

	/* See section 5.2 in RFC 2396 */

	/* FIXME bugzilla.eazel.com 4413: This function does not take
	 * into account a BASE tag in an HTML document, so its
	 * functionality differs from what Mozilla itself would do.
	 */

	if (is_uri_relative (uri)) {
		char *mutable_base_uri;
		char *mutable_uri;

		char *uri_current;
		gsize base_uri_length;
		char *separator;

		/* We may need one extra character
		 * to append a "/" to uri's that have no "/"
		 * (such as help:)
		 */

		mutable_base_uri = g_malloc(strlen(base_uri)+2);
		strcpy (mutable_base_uri, base_uri);
		
		uri_current = mutable_uri = g_strdup (uri);

		/* Chew off Fragment and Query from the base_url */

		separator = strrchr (mutable_base_uri, '#'); 

		if (separator) {
			*separator = '\0';
		}

		separator = strrchr (mutable_base_uri, '?');

		if (separator) {
			*separator = '\0';
		}

		if ('/' == uri_current[0] && '/' == uri_current [1]) {
			/* Relative URI's beginning with the authority
			 * component inherit only the scheme from their parents
			 */

			separator = strchr (mutable_base_uri, ':');

			if (separator) {
				separator[1] = '\0';
			}			  
		} else if ('/' == uri_current[0]) {
			/* Relative URI's beginning with '/' absolute-path based
			 * at the root of the base uri
			 */

			separator = strchr (mutable_base_uri, ':');

			/* g_assert (separator), really */
			if (separator) {
				/* If we start with //, skip past the authority section */
				if ('/' == separator[1] && '/' == separator[2]) {
					separator = strchr (separator + 3, '/');
					if (separator) {
						separator[0] = '\0';
					}
				} else {
				/* If there's no //, just assume the scheme is the root */
					separator[1] = '\0';
				}
			}
		} else if ('#' != uri_current[0]) {
			/* Handle the ".." convention for relative uri's */

			/* If there's a trailing '/' on base_url, treat base_url
			 * as a directory path.
			 * Otherwise, treat it as a file path, and chop off the filename
			 */

			base_uri_length = strlen (mutable_base_uri);
			if ('/' == mutable_base_uri[base_uri_length-1]) {
				/* Trim off '/' for the operation below */
				mutable_base_uri[base_uri_length-1] = 0;
			} else {
				separator = strrchr (mutable_base_uri, '/');
				if (separator) {
					*separator = '\0';
				}
			}

			remove_internal_relative_components (uri_current);

			/* handle the "../"'s at the beginning of the relative URI */
			while (0 == strncmp ("../", uri_current, 3)) {
				uri_current += 3;
				separator = strrchr (mutable_base_uri, '/');
				if (separator) {
					*separator = '\0';
				} else {
					/* <shrug> */
					break;
				}
			}

			/* handle a ".." at the end */
			if (uri_current[0] == '.' && uri_current[1] == '.' 
			    && uri_current[2] == '\0') {

			    	uri_current += 2;
				separator = strrchr (mutable_base_uri, '/');
				if (separator) {
					*separator = '\0';
				}
			}

			/* Re-append the '/' */
			mutable_base_uri [strlen(mutable_base_uri)+1] = '\0';
			mutable_base_uri [strlen(mutable_base_uri)] = '/';
		}

		result = g_strconcat (mutable_base_uri, uri_current, NULL);
		g_free (mutable_base_uri); 
		g_free (mutable_uri); 

	} else {
		result = g_strdup (uri);
	}
	
	return result;
}

char *
yelp_util_node_to_string_path (GNode *node)
{
	char *str, *t;
	char *escaped_node_name;
	YelpSection *section;

	section = node->data;
	escaped_node_name = gnome_vfs_escape_set (section->name, " \t\n;/%");
	
	str = escaped_node_name;
	while (node->parent != NULL) {
		node = node->parent;
		
		section = node->data;
		if (section) {
			escaped_node_name = gnome_vfs_escape_set (section->name, " \t\n;/%");
			
			t = g_strconcat (escaped_node_name, "/", str, NULL);
			g_free (escaped_node_name);
			g_free (str);
			str = t;
		}
	}
	return str;
}

static GNode *
yelp_util_string_path_to_node_helper (char  **path,
				      GNode  *node)
{
	char *unescaped_pathname;
	YelpSection *section;
		
	if (*path == NULL) {
		return NULL;
	}

	if (node == NULL) {
		return NULL;
	}
	
	unescaped_pathname = gnome_vfs_unescape_string (*path, NULL);
	
	do {
		section = node->data;

		if (strcmp (section->name, unescaped_pathname) == 0) {
			g_free (unescaped_pathname);
			path += 1;
			
			if (*path == NULL) {
				return node;
			}
			
			if (node->children) {
				return yelp_util_string_path_to_node_helper (path, node->children);
			} else {
				return NULL;
			}
		}
		
	} while ((node = node->next) != NULL);

	g_free (unescaped_pathname);
	
	return NULL;
}

     
GNode *
yelp_util_string_path_to_node (const char *string_path,
			       GNode      *root)
{
	char **path;
	GNode *node;

	path = g_strsplit (string_path, "/", 0);

	node = yelp_util_string_path_to_node_helper (path, root->children);
	
	g_strfreev (path);
		
	return node;
}


GNode *
yelp_util_decompose_path_url (GNode      *root,
			      const char *path_url,
			      char      **embedded_url)
{
	const char *first_part;
	const char *second_part;
	char *path;
	GNode *res;

	*embedded_url = NULL;
	
	if (strncmp (path_url, "path:", 5) != 0) {
		return NULL;
	}

	first_part = path_url + 5;
	second_part = strchr(first_part, ';');
	if (second_part) {
		path = g_strndup (first_part, second_part - first_part);
		second_part += 1;
	} else {
		path = g_strdup (first_part);
	}

	res = yelp_util_string_path_to_node (path, root);
	g_free (path);

	if (second_part) {
		*embedded_url = g_strdup (second_part);
	}
	
	return res;
}

char *
yelp_util_compose_path_url (GNode      *node,
			    const char *embedded_url)
{
	char *path;

	path = yelp_util_node_to_string_path (node);

	if (path == NULL) {
		return NULL;
	}

	return  g_strconcat ("path:", path, ";", embedded_url, NULL);
}
 
