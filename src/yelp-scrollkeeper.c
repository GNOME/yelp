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

static gboolean   ys_trim_empty_branches  (xmlNode              *cl_node);
static gboolean   ys_tree_empty           (xmlNode              *cl_node);

static gboolean   ys_parse_books          (GNode                *tree,
					   xmlDoc               *doc);
static gboolean   ys_parse_section        (GNode                *parent,
					   xmlNode              *xml_node);
static void       ys_parse_doc            (GNode                *parent,
					   xmlNode              *xml_node);
static void       ys_parse_toc_section    (GNode                *parent,
					   xmlNode              *xml_node,
					   const gchar          *base_uri);
static gchar *    ys_get_xml_docpath      (const gchar          *command,
					   const gchar          *argument);

static gchar *    ys_strip_scheme         (gchar                *original_uri,
					   gchar               **scheme);

static gint calls = 0;

static GHashTable *seriesid_hash = NULL;

static gboolean
ys_trim_empty_branches (xmlNode *node)
{
	xmlNode  *child;
	xmlNode  *next;
	gboolean  empty;
	
	if (!node) {
		return TRUE;
	}

	for (child = node->xmlChildrenNode; child; child = next) {
		next = child->next;
		
		if (!g_strcasecmp (child->name, "sect")) {
			empty = ys_trim_empty_branches (child);
			if (empty) {
				xmlUnlinkNode (child);
				xmlFreeNode (child);
			}
		}
	}

	for (child = node->xmlChildrenNode; child; child = child->next) {
		if (!g_strcasecmp (child->name, "sect") ||
		    !g_strcasecmp (child->name, "doc")) {
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
ys_tree_empty (xmlNode *cl_node)
{
	xmlNode  *node, *next;
	gboolean  ret_val;

	if (cl_node == NULL)
		return TRUE;

	for (node = cl_node; node != NULL; node = next) {
		next = node->next;

		if (!strcmp (node->name, "sect") &&
		    node->xmlChildrenNode->next != NULL) {
			ret_val = ys_tree_empty (
				node->xmlChildrenNode->next);

			if (!ret_val) {
				return ret_val;
			}
		}

		if (!strcmp (node->name, "doc")) {
			return FALSE;
		}
	}
	
	return TRUE;
}

static gboolean
ys_parse_books (GNode *tree, xmlDoc *doc)
{
	xmlNode  *node;
	gboolean  success;
	GNode    *book_node;

	g_return_val_if_fail (tree != NULL, FALSE);

	node = doc->xmlRootNode;

	if (!node || !node->name || 
	    g_strcasecmp (node->name, "ScrollKeeperContentsList")) {
		g_warning ("Invalid ScrollKeeper XML Contents List!");
		return FALSE;
	}
	
	book_node = g_node_append_data (tree, 
					yelp_section_new (YELP_SECTION_CATEGORY,
							  "scrollkeeper", NULL,
							  NULL, NULL));
       
	for (node = node->xmlChildrenNode; node; node = node->next) {
		if (!g_strcasecmp (node->name, "sect")) {
			success = ys_parse_section (book_node, node);
		}
	}

	return TRUE;
}

static gboolean
ys_parse_section (GNode *parent, xmlNode *xml_node)
{
	xmlNode     *cur;
	xmlChar     *xml_str;
	gchar       *name;
	GNode       *node;
	
	/* Find the title */
	for (cur = xml_node->xmlChildrenNode; cur; cur = cur->next) {
		if (!g_strcasecmp (cur->name, "title")) {
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
						     name, NULL,
						     NULL, NULL));

	for (cur = xml_node->xmlChildrenNode; cur; cur = cur->next) {
		if (!g_strcasecmp (cur->name, "sect")) {
			ys_parse_section (node, cur);
		}
		else if (!g_strcasecmp (cur->name, "doc")) {
			ys_parse_doc (node, cur);
		}
	}
	
	return TRUE;
}

static void
ys_parse_doc (GNode *parent, xmlNode *xml_node) 
{
	xmlNode     *cur;
	xmlChar     *xml_str;
	gchar       *title;
	gchar       *omf;
	gchar       *link;
	gchar       *format;
	gchar       *docsource;
	gchar       *docseriesid;
	GNode       *node;

	docseriesid = NULL;
	for (cur = xml_node->xmlChildrenNode; cur; cur = cur->next) {
		if (!g_strcasecmp (cur->name, "doctitle")) {
			xml_str = xmlNodeGetContent (cur);
			title   = g_strdup (xml_str);
			xmlFree (xml_str);
		}
		else if (!g_strcasecmp (cur->name, "docomf")) {
			xml_str = xmlNodeGetContent (cur);
			omf     = g_strdup (xml_str);
			xmlFree (xml_str);
		}
		else if (!g_strcasecmp (cur->name, "docsource")) {
			xml_str = xmlNodeGetContent (cur);
			docsource = ys_strip_scheme (xml_str, NULL);
			link    = g_strconcat ("ghelp:", docsource, NULL);
			xmlFree (xml_str);
		}
		else if (!g_strcasecmp (cur->name, "docformat")) {
			xml_str = xmlNodeGetContent (cur);
			format  = g_strdup (xml_str);
			xmlFree (xml_str);
		}
		else if (!g_strcasecmp (cur->name, "docseriesid")) {
			xml_str = xmlNodeGetContent (cur);
			docseriesid  = g_strdup (xml_str);
			xmlFree (xml_str);
		}
	}

	node = g_node_append_data (parent, 
                                   yelp_section_new (YELP_SECTION_DOCUMENT,
                                                     title, link,
                                                     NULL, NULL));

	if (docseriesid) {
		g_hash_table_insert (seriesid_hash, docseriesid, node);
	}
	
#if 0
	ys_parse_toc (NULL, NULL, docsource);
#endif

/* 	index_location = ys_get_xml_docpath ("scrollkeeper-get-index-from-docpath", */
/* 					     docsource); */
	
/* 	if (index_location) { */
/* 		g_print ("Found index: %s\n", index_location); */
/* 	} */

	g_free (title);
	g_free (omf);
	g_free (link);
	g_free (format);
 	g_free (docsource);
}

static void
ys_parse_toc_section (GNode       *parent,
		      xmlNode     *xml_node, 
		      const gchar *base_uri)
{
	gchar       *name;
	gchar       *link;
	xmlNode     *next_child;
	xmlChar     *xml_str;
        GNode       *node;

	next_child = xml_node->xmlChildrenNode;
	
	name = xmlNodeGetContent (next_child);
	
	if (!name) {
		return;
	}

	g_strchomp (name);

	xml_str = xmlGetProp (xml_node, "linkid");
	
	if (xml_str) {
		link = g_strconcat ("?", xml_str, NULL);
		g_strchomp (link);
		xmlFree (xml_str);
	}
	
/* 	if (link) { */
/* 		uri = gnome_vfs_uri_resolve_relative (base_uri, link); */
/* 	} else { */
/* 	} */

        node = g_node_append_data (parent, 
                                   yelp_section_new (YELP_SECTION_DOCUMENT_SECTION,
                                                     name, 
                                                     base_uri,
                                                     link, NULL));
	
	for (; next_child != NULL; next_child = next_child->next) {
		if (!g_strncasecmp (next_child->name, "tocsect", 7)) {
			ys_parse_toc_section (node, next_child, base_uri);
		}
	}
}

static gchar *
ys_get_xml_docpath (const gchar *command, const gchar *argument)
{
	gboolean  success;
	gchar    *full_command;
	gchar    *xml_location = NULL;
	
	full_command = g_strconcat (command, " ", argument, NULL);

	success = g_spawn_command_line_sync (full_command, &xml_location,
					     NULL, NULL, NULL);

	calls++;
	
	g_free (full_command);
	
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
ys_strip_scheme(gchar *original_uri, gchar **scheme)
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

gboolean
yelp_scrollkeeper_init (GNode *tree)
{
       gchar       *docpath;
       xmlDoc      *doc;
       const GList *node;
        
       g_return_val_if_fail (tree != NULL, FALSE);

       seriesid_hash = g_hash_table_new_full (g_str_hash,
					      g_str_equal,
					      g_free, NULL);
       
       doc = NULL;
       for (node = gnome_i18n_get_language_list ("LC_MESSAGES"); node; node = node->next) {
	       docpath = ys_get_xml_docpath ("scrollkeeper-get-content-list",
					     node->data);

		if (docpath) {
			doc = xmlParseFile (docpath);
			g_free (docpath);
		}

		if (doc) {
			if (doc->xmlRootNode && !ys_tree_empty(doc->xmlRootNode->xmlChildrenNode)) {
				break;
			} else {
				xmlFreeDoc (doc);
				doc = NULL;
			}
		}
	}
		
	if (doc) {
		ys_trim_empty_branches (doc->xmlRootNode);

		ys_parse_books (tree, doc);
		
		xmlFreeDoc (doc);
	}        

	g_print ("Number of script calls: %d\n", calls);
        
        return TRUE;
}

GNode *
yelp_scrollkeeper_lookup_seriesid (const gchar *seriesid)
{
	return g_hash_table_lookup (seriesid_hash, seriesid);
}

GNode *
yelp_scrollkeeper_get_toc_tree_model (const gchar *docpath)
{
	gchar   *toc_file;
	xmlDoc  *doc = NULL;
	xmlNode *xml_node;
	GNode   *tree;
	gchar   *full_path;
        
        g_return_val_if_fail (docpath != NULL, NULL);

        g_print ("Trying to get tree for: %s\n", docpath);

        tree = g_node_new (NULL);

 	toc_file = ys_get_xml_docpath ("scrollkeeper-get-toc-from-docpath",
 				       docpath);

	if (toc_file) {
		doc = xmlParseFile (toc_file);
		g_free (toc_file);
	}

	if (!doc) {
/* 		g_warning ("Tried to parse a non-valid TOC file"); */
		return NULL;
	}
	
	if (g_strcasecmp (doc->xmlRootNode->name, "toc")) {
		g_warning ("Document with wrong root node, got: '%s'",
			   doc->xmlRootNode->name);
	}

	xml_node = doc->xmlRootNode->xmlChildrenNode;

	full_path = g_strconcat ("ghelp:", docpath, NULL);

	for (; xml_node != NULL; xml_node = xml_node->next) {
		ys_parse_toc_section (tree, xml_node, full_path);
	}

	g_free (full_path);
	
	return tree;
}


