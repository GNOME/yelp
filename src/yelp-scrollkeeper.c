/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@codefactory.se>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <libxml/parser.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>

#include "yelp-section.h"
#include "yelp-util.h"
#include "yelp-scrollkeeper.h"

#define d(x)

static gboolean   scrollkeeper_trim_empty_branches (xmlNode      *cl_node);
static gboolean   scrollkeeper_tree_empty          (xmlNode      *cl_node);

static gboolean   scrollkeeper_parse_books         (GNode        *tree,
						    xmlDoc       *doc);
static gboolean   scrollkeeper_parse_section       (GNode        *parent,
						    xmlNode      *xml_node);
static void       scrollkeeper_parse_doc           (GNode        *parent,
						    xmlNode      *xml_node,
						    gchar        *docid);
static void       scrollkeeper_parse_toc_section   (GNode        *parent,
						    xmlNode      *xml_node,
						    const gchar  *base_uri);
static gchar *    scrollkeeper_get_xml_docpath     (const gchar  *command,
						    const gchar  *argument);
static gchar *    scrollkeeper_strip_scheme        (gchar        *original_uri,
						    gchar       **scheme);

static gboolean   scrollkeeper_parse_index         (GList       **index);
static void       scrollkeeper_parse_index_file    (GList       **index,
						    const gchar  *index_path,
						    YelpSection  *section);
static void       scrollkeeper_parse_index_item    (GList       **index,
						    YelpSection  *section,
						    xmlNode      *node);


static GHashTable *seriesid_hash = NULL;
static GHashTable *docid_hash    = NULL;

static gboolean
scrollkeeper_trim_empty_branches (xmlNode *node)
{
	xmlNode  *child;
	xmlNode  *next;
	gboolean  empty;
	
	if (!node) {
		return TRUE;
	}

	for (child = node->xmlChildrenNode; child; child = next) {
		next = child->next;
		
		if (!g_ascii_strcasecmp (child->name, "sect")) {
			empty = scrollkeeper_trim_empty_branches (child);
			if (empty) {
				xmlUnlinkNode (child);
				xmlFreeNode (child);
			}
		}
	}

	for (child = node->xmlChildrenNode; child; child = child->next) {
		if (!g_ascii_strcasecmp (child->name, "sect") ||
		    !g_ascii_strcasecmp (child->name, "doc")) {
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
scrollkeeper_tree_empty (xmlNode *cl_node)
{
	xmlNode  *node, *next;
	gboolean  ret_val;

	if (cl_node == NULL)
		return TRUE;

	for (node = cl_node; node != NULL; node = next) {
		next = node->next;

		if (!g_ascii_strcasecmp (node->name, "sect") &&
		    node->xmlChildrenNode->next != NULL) {
			ret_val = scrollkeeper_tree_empty (
				node->xmlChildrenNode->next);

			if (!ret_val) {
				return ret_val;
			}
		}

		if (!g_ascii_strcasecmp (node->name, "doc")) {
			return FALSE;
		}
	}
	
	return TRUE;
}

static gboolean
scrollkeeper_parse_books (GNode *tree, xmlDoc *doc)
{
	xmlNode  *node;
	gboolean  success;
	GNode    *book_node;

	g_return_val_if_fail (tree != NULL, FALSE);

	node = doc->xmlRootNode;

	if (!node || !node->name || 
	    g_ascii_strcasecmp (node->name, "ScrollKeeperContentsList")) {
		g_warning ("Invalid ScrollKeeper XML Contents List!");
		return FALSE;
	}

	book_node = g_node_append_data (tree, 
					yelp_section_new (YELP_SECTION_CATEGORY,
							  "scrollkeeper", 
							  NULL));

	for (node = node->xmlChildrenNode; node; node = node->next) {
		if (!g_ascii_strcasecmp (node->name, "sect")) {
			success = scrollkeeper_parse_section (book_node, node);
		}
	}

	return TRUE;
}

static gboolean
scrollkeeper_parse_section (GNode *parent, xmlNode *xml_node)
{
	xmlNode *cur;
	xmlChar *xml_str;
	gchar   *name;
	GNode   *node;
	gchar   *docid;
	
	/* Find the title */
	for (cur = xml_node->xmlChildrenNode; cur; cur = cur->next) {
		if (!g_ascii_strcasecmp (cur->name, "title")) {
			xml_str = xmlNodeGetContent (cur);
			
			if (xml_str) {
				name = g_strdup (xml_str);
				xmlFree (xml_str);
			}
		}
	}

	if (!name) {
		g_warning ("Couldn't find name of the section");
		return FALSE;
	}
	
	node = g_node_append_data (parent, 
				   yelp_section_new (YELP_SECTION_CATEGORY, 
						     name, NULL));

	for (cur = xml_node->xmlChildrenNode; cur; cur = cur->next) {
		if (!g_ascii_strcasecmp (cur->name, "sect")) {
			scrollkeeper_parse_section (node, cur);
		}
		else if (!g_ascii_strcasecmp (cur->name, "doc")) {

			xml_str = xmlGetProp (cur, "docid");
			if (xml_str) {
				docid = g_strdup (xml_str);
				xmlFree (xml_str);
			}
			
			scrollkeeper_parse_doc (node, cur, docid);
		}
	}
	
	return TRUE;
}

static void
scrollkeeper_parse_doc (GNode *parent, xmlNode *xml_node, gchar *docid)
{
	xmlNode *cur;
	xmlChar *xml_str;
	gchar   *title;
	gchar   *omf;
	gchar   *link;
	gchar   *format;
	gchar   *docsource;
	gchar   *docseriesid;
	GNode   *node;
	YelpURI *uri;

	docseriesid = NULL;
	for (cur = xml_node->xmlChildrenNode; cur; cur = cur->next) {
		if (!g_ascii_strcasecmp (cur->name, "doctitle")) {
			xml_str = xmlNodeGetContent (cur);
			title   = g_strdup (xml_str);
			xmlFree (xml_str);
		}
		else if (!g_ascii_strcasecmp (cur->name, "docomf")) {
			xml_str = xmlNodeGetContent (cur);
			omf     = g_strdup (xml_str);
			xmlFree (xml_str);
		}
		else if (!g_ascii_strcasecmp (cur->name, "docsource")) {
			xml_str = xmlNodeGetContent (cur);
			docsource = scrollkeeper_strip_scheme (xml_str, NULL);
			link    = g_strconcat ("ghelp:", docsource, NULL);
			xmlFree (xml_str);
		}
		else if (!g_ascii_strcasecmp (cur->name, "docformat")) {
			xml_str = xmlNodeGetContent (cur);
			format  = g_strdup (xml_str);
			xmlFree (xml_str);
		}
		else if (!g_ascii_strcasecmp (cur->name, "docseriesid")) {
			xml_str = xmlNodeGetContent (cur);
			docseriesid  = g_strdup (xml_str);
			xmlFree (xml_str);
		}
	}

	uri = yelp_uri_new (link);

	node = g_node_append_data (parent, 
                                   yelp_section_new (YELP_SECTION_DOCUMENT,
                                                     title, uri));
	yelp_uri_unref (uri);

	if (docseriesid) {
		g_hash_table_insert (seriesid_hash, docseriesid, node);
	}
	
	g_hash_table_insert (docid_hash, docid, node);
	
	g_free (title);
	g_free (omf);
	g_free (link);
	g_free (format);
 	g_free (docsource);
}

static void
scrollkeeper_parse_toc_section (GNode       *parent,
				xmlNode     *xml_node, 
				const gchar *base_uri)
{
	gchar   *name;
	gchar   *link;
	xmlNode *next_child;
	xmlChar *xml_str;
        GNode   *node;
	gchar   *str_uri;
	YelpURI *uri;

	next_child = xml_node->xmlChildrenNode;
	
	name = xmlNodeGetContent (next_child);
	
	if (!name) {
		return;
	}

	g_strstrip (name);

	xml_str = xmlGetProp (xml_node, "linkid");
	
	if (xml_str) {
		link = g_strconcat ("?", xml_str, NULL);
		g_strchomp (link);
		xmlFree (xml_str);
	}

	str_uri = g_strconcat (base_uri, link, NULL);
	uri     = yelp_uri_new (str_uri);
	g_free (str_uri);
	
        node = g_node_append_data (parent, 
                                   yelp_section_new (YELP_SECTION_DOCUMENT_SECTION,
                                                     name, uri));
	yelp_uri_unref (uri);
	
	for (; next_child != NULL; next_child = next_child->next) {
		if (!g_ascii_strncasecmp (next_child->name, "tocsect", 7)) {
			scrollkeeper_parse_toc_section (node, next_child, base_uri);
		}
	}
}

static gchar *
scrollkeeper_get_xml_docpath (const gchar *command, const gchar *argument)
{
	gboolean  success;
	gchar    *full_command;
	gchar    *xml_location = NULL;
	gchar    *std_err;
	
	full_command = g_strconcat (command, " ", argument, NULL);

	success = g_spawn_command_line_sync (full_command, &xml_location,
					     &std_err, NULL, NULL);

	g_free (full_command);
	g_free (std_err);
	
	if (!success) {
		g_warning ("Didn't successfully run command: '%s %s'", 
			   command, argument);

		if (xml_location) {
			g_free (xml_location);
		}
     
		return NULL;
	}
	
	g_strchomp (xml_location);
	
	return xml_location;
}

static gchar *
scrollkeeper_strip_scheme(gchar *original_uri, gchar **scheme)
{
	gchar *new_uri;
	gchar *point;

	point = strstr (original_uri, ":");

	if (!point) {
		if (scheme) {
			*scheme = NULL;
		}

		return g_strdup (original_uri);
	}

	if (scheme) {
		*scheme = g_strndup(original_uri, point - original_uri);
	}
	
	new_uri = g_strdup (point + 1);

	return new_uri;
}

static gboolean
scrollkeeper_parse_index (GList **index)
{
	gchar                   *sk_data_dir = NULL;
	gchar                   *index_dir;
	GnomeVFSDirectoryHandle *dir;
	GnomeVFSResult           result;
	GnomeVFSFileInfo        *file_info;
	GNode                   *node;
	YelpSection             *section;
	
	sk_data_dir = scrollkeeper_get_xml_docpath ("scrollkeeper-config",
						    "--pkglocalstatedir");

	if (!sk_data_dir) {
		return FALSE;
	}
	
	index_dir = g_strdup_printf ("%s/index", sk_data_dir);

	g_free (sk_data_dir);
	
	result = gnome_vfs_directory_open (&dir, index_dir, 
					   GNOME_VFS_FILE_INFO_DEFAULT);

	if (result != GNOME_VFS_OK) {
		return FALSE;
	}

	file_info = gnome_vfs_file_info_new ();

	while (gnome_vfs_directory_read_next (dir, file_info) == GNOME_VFS_OK) {
		node = g_hash_table_lookup (docid_hash, file_info->name);
		
		if (node) {
			gchar *index_path = g_strdup_printf ("%s/%s",
							     index_dir,
							     file_info->name);
			
			section = YELP_SECTION (node->data);
			
			scrollkeeper_parse_index_file (index, index_path, section);

			g_free (index_path);
		}
	}
	
	g_free (index_dir);
	gnome_vfs_file_info_unref (file_info);
	gnome_vfs_directory_close (dir);
	
	return TRUE;
}

static void
scrollkeeper_parse_index_file (GList       **index, 
			       const gchar  *index_path,
			       YelpSection  *section)
{
	xmlDoc  *doc;
	xmlNode *node;
	
	doc = xmlParseFile (index_path);
	
	if (doc) {
		node = doc->xmlRootNode;
		
		if (!node || !node->name || 
		    g_ascii_strcasecmp (node->name, "indexdoc")) {
			g_warning ("Invalid Index file, root node is '%s', it should be 'indexdoc'!", node->name);
			return;
		}

		for (node = node->xmlChildrenNode; node; node = node->next) {
			if (!g_ascii_strcasecmp (node->name, "indexitem")) {
				
				scrollkeeper_parse_index_item (index, section, node);
			}
		}
	}
}

static void
scrollkeeper_parse_index_item (GList **index, YelpSection *section, xmlNode *node)
{
	xmlNode     *cur;
	xmlChar     *title = NULL;
	xmlChar     *link  = NULL;
	xmlChar     *xml_str;
	
	for (cur = node->xmlChildrenNode; cur; cur = cur->next) {
		if (!g_ascii_strcasecmp (cur->name, "title")) {
			xml_str = xmlNodeGetContent (cur);
			
			if (!g_utf8_validate (xml_str, -1, NULL)) {
				g_warning ("Index title is not valid utf8");
				xmlFree (xml_str);
				return;
			}
			
			title = g_utf8_strdown (xml_str, -1);
			xmlFree (xml_str);
		}
		else if (!g_ascii_strcasecmp (cur->name, "link")) {
			xml_str = xmlGetProp (cur, "linkid");
			link = g_strconcat ("?", xml_str, NULL);
			xmlFree (xml_str);
		}
		else if (!g_ascii_strcasecmp (cur->name, "indexitem")) {
			scrollkeeper_parse_index_item (index, section, cur);
		}
	}

	if (title && link) {
		YelpURI     *uri;
		YelpSection *index_section;
		
		uri = yelp_uri_get_relative (section->uri, link);
		
		d(g_print ("%s\n", yelp_uri_to_string (uri)));
		
		index_section = yelp_section_new (YELP_SECTION_INDEX,
						  title, uri);
		
		yelp_uri_unref (uri);
		
		*index = g_list_prepend (*index, index_section);
		
		g_free (title);
		g_free (link);
		
		title = link = NULL;
	}
}

gboolean
yelp_scrollkeeper_init (GNode *tree, GList **index)
{
	gchar       *docpath;
	xmlDoc      *doc;
	const GList *node;
        
	g_return_val_if_fail (tree != NULL, FALSE);

	seriesid_hash = g_hash_table_new_full (g_str_hash,
					       g_str_equal,
					       g_free, NULL);

	docid_hash = g_hash_table_new_full (g_str_hash, g_str_equal, 
					    g_free, NULL);
       
	doc = NULL;

	for (node = gnome_i18n_get_language_list ("LC_MESSAGES"); node; node = node->next) {
		docpath = scrollkeeper_get_xml_docpath ("scrollkeeper-get-content-list",
							node->data);

		if (docpath) {
			doc = xmlParseFile (docpath);
			g_free (docpath);
		}

		if (doc) {
			if (doc->xmlRootNode && !scrollkeeper_tree_empty(doc->xmlRootNode->xmlChildrenNode)) {
				break;
			} else {
				xmlFreeDoc (doc);
				doc = NULL;
			}
		}
	}
		
	if (doc) {
		scrollkeeper_trim_empty_branches (doc->xmlRootNode);

		scrollkeeper_parse_books (tree, doc);
		
		xmlFreeDoc (doc);
	}        

	scrollkeeper_parse_index (index);

        return TRUE;
}

GNode *
yelp_scrollkeeper_lookup_seriesid (const gchar *seriesid)
{
	return g_hash_table_lookup (seriesid_hash, seriesid);
}

GNode *
yelp_scrollkeeper_get_toc_tree (const gchar *docpath)
{
	gchar   *toc_file;
	xmlDoc  *doc = NULL;
	xmlNode *xml_node;
	GNode   *tree;
	gchar   *full_path;
        
        g_return_val_if_fail (docpath != NULL, NULL);

        tree = g_node_new (NULL);
	
 	toc_file = scrollkeeper_get_xml_docpath ("scrollkeeper-get-toc-from-docpath",
						 docpath);

	if (toc_file) {
		doc = xmlParseFile (toc_file);
		g_free (toc_file);
	}

	if (!doc) {
		/* g_warning ("Tried to parse a non-valid TOC file"); */
		return NULL;
	}
	
	if (g_ascii_strcasecmp (doc->xmlRootNode->name, "toc")) {
		g_warning ("Document with wrong root node, got: '%s'",
			   doc->xmlRootNode->name);
	}

	xml_node = doc->xmlRootNode->xmlChildrenNode;

	full_path = g_strconcat ("ghelp:", docpath, NULL);

	for (; xml_node != NULL; xml_node = xml_node->next) {
		scrollkeeper_parse_toc_section (tree, xml_node, full_path);
	}

	g_free (full_path);
	
	return tree;
}


