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
#include <gtk/gtktreemodel.h>
#include <libxml/parser.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include "yelp-section.h"
#include "yelp-util.h"
#include "yelp-scrollkeeper.h"

typedef struct {
	GtkTreeStore *store;
} ParseData;

static gboolean   ys_trim_empty_branches  (xmlNode              *cl_node);
static gboolean   ys_tree_empty           (xmlNode              *cl_node);

static gboolean   ys_parse_books          (ParseData            *data,
					   xmlDoc               *doc);
static gboolean   ys_parse_section        (ParseData            *data,
					   GtkTreeIter          *iter,
					   xmlNode              *xml_node);
static void       ys_parse_doc            (ParseData            *data,
					   GtkTreeIter          *parent,
					   xmlNode              *xml_node);
static void       ys_parse_toc            (ParseData            *data,
					   GtkTreeIter          *parent,
					   const gchar          *docsource);
static void       ys_parse_toc_section    (ParseData            *data,
					   GtkTreeIter          *parent,
					   xmlNode              *xml_node,
					   const gchar          *base_uri);
static gchar *    ys_get_xml_docpath      (const gchar          *command,
					   const gchar          *argument);
static void       ys_strip_scheme         (gchar               **original_uri,
					   gchar               **scheme);

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
ys_parse_books (ParseData *data, xmlDoc *doc)
{
	xmlNode  *node;
	gboolean  success;
	GtkTreeIter *root;
	
	g_return_val_if_fail (data != NULL, FALSE);

	node = doc->xmlRootNode;

	if (!node || !node->name || 
	    g_strcasecmp (node->name, "ScrollKeeperContentsList")) {
		g_warning ("Invalid ScrollKeeper XML Contents List!");
		return FALSE;
	}
	
	root = yelp_util_contents_add_section (data->store, NULL, 
 					       yelp_section_new (YELP_SECTION_CATEGORY,
								 "scrollkeeper", NULL,
								 NULL, NULL));

	for (node = node->xmlChildrenNode; node; node = node->next) {
		if (!g_strcasecmp (node->name, "sect")) {
			success = ys_parse_section (data, root, node);
		}
	}

	return TRUE;
}

static gboolean
ys_parse_section (ParseData *data, GtkTreeIter *parent, xmlNode *xml_node)
{
	xmlNode     *cur;
	xmlChar     *xml_str;
	gchar       *name;
	GtkTreeIter *iter;
	
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
	
	iter = yelp_util_contents_add_section (data->store, parent, 
					       yelp_section_new (YELP_SECTION_CATEGORY,
								 name, NULL, 
								 NULL, NULL));
	
	for (cur = xml_node->xmlChildrenNode; cur; cur = cur->next) {
		if (!g_strcasecmp (cur->name, "sect")) {
			ys_parse_section (data, iter, cur);
		}
		else if (!g_strcasecmp (cur->name, "doc")) {
			ys_parse_doc (data, iter, cur);
		}
	}
	
	return TRUE;
}

static void
ys_parse_doc (ParseData *data, GtkTreeIter *parent, xmlNode *xml_node) 
{
	xmlNode     *cur;
	xmlChar     *xml_str;
	gchar       *title;
	gchar       *omf;
	gchar       *link;
	gchar       *format;
	GtkTreeIter *iter;
	gchar       *docsource;
	gchar       *scheme;
/* 	gchar       *index_location; */
	
	for (cur = xml_node->xmlChildrenNode; cur; cur = cur->next) {
		if (!g_strcasecmp (cur->name, "doctitle")) {
			xml_str = xmlNodeGetContent (cur);
			title   = g_strdup (xml_str);
			g_free (xml_str);
		}
		else if (!g_strcasecmp (cur->name, "docomf")) {
			xml_str = xmlNodeGetContent (cur);
			omf     = g_strdup (xml_str);
			g_free (xml_str);
		}
		else if (!g_strcasecmp (cur->name, "docsource")) {
			xml_str = xmlNodeGetContent (cur);
			docsource = g_strdup (xml_str);
			ys_strip_scheme(&docsource, &scheme);
			link    = g_strconcat ("ghelp:", docsource, NULL);
			g_free (xml_str);
		}
		else if (!g_strcasecmp (cur->name, "docformat")) {
			xml_str = xmlNodeGetContent (cur);
			format  = g_strdup (xml_str);
			g_free (xml_str);
		}
	}

	iter = yelp_util_contents_add_section (data->store, parent, 
					       yelp_section_new (YELP_SECTION_DOCUMENT,
								 title, link, 
								 NULL, NULL));
	
	ys_parse_toc (data, iter, docsource);

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
ys_parse_toc (ParseData *data, GtkTreeIter *parent, const gchar *docsource)
{
	gchar       *toc_file;
	xmlDoc      *doc = NULL;
	xmlNode     *xml_node;
	YelpSection *section;
	
	toc_file = ys_get_xml_docpath ("scrollkeeper-get-toc-from-docpath",
				       docsource);

	if (toc_file) {
		doc = xmlParseFile (toc_file);
		g_free (toc_file);
	}

	if (!doc) {
		g_warning ("Tried to parse a non-valid TOC file");
		return;
	}
	
	if (g_strcasecmp (doc->xmlRootNode->name, "toc")) {
		g_warning ("Document with wrong root node, got: '%s'",
			   doc->xmlRootNode->name);
	}

	xml_node = doc->xmlRootNode->xmlChildrenNode;

	gtk_tree_model_get (GTK_TREE_MODEL (data->store), parent,
			    1, &section,
			    -1);

	for (; xml_node != NULL; xml_node = xml_node->next) {
		ys_parse_toc_section (data, parent, xml_node, 
				      section->uri);
	}
}

static void
ys_parse_toc_section (ParseData   *data, 
		      GtkTreeIter *parent, 
		      xmlNode     *xml_node, 
		      const gchar *base_uri)
{
	gchar       *name;
	gchar       *link;
	xmlNode     *next_child;
	xmlChar     *xml_str;
	GtkTreeIter *iter;

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

	iter = yelp_util_contents_add_section (data->store, parent, 
					       yelp_section_new (YELP_SECTION_DOCUMENT_SECTION,
								 name, 
								 base_uri,
								 link, NULL));
	
	for (; next_child != NULL; next_child = next_child->next) {
		if (!g_strncasecmp (next_child->name, "tocsect", 7)) {
			ys_parse_toc_section (data, iter, 
					      next_child, base_uri);
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

static void
ys_strip_scheme(gchar **original_uri, gchar **scheme)
{
	gchar *new_uri;
	gchar *point;

	point = g_strstr_len(*original_uri, strlen(*original_uri), ":");
	if (!point)
	{
		scheme = NULL;
		return;
	}
	*scheme = g_strndup(*original_uri, point - *original_uri);
	new_uri = g_strdup(point + 1);
	g_free(*original_uri);
	*original_uri = new_uri;
}

gboolean
yelp_scrollkeeper_init (GtkTreeStore *store)
{
	gchar       *docpath;
	xmlDoc      *doc;
	const GList *node;
	ParseData   *data;
	
	g_return_val_if_fail (GTK_IS_TREE_STORE (store), FALSE);

	data = g_new0 (ParseData, 1);
	data->store = store;

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

		ys_parse_books (data, doc);
		
		xmlFreeDoc (doc);
	}        

        
        return TRUE;
}
