/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Sun Microsyatems Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.,  59 Temple Place - Suite 330, Cambridge, MA 02139, USA.
 *
 */

#include <config.h>
#include <glib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libgnome/gnome-i18n.h>
#include <libxml/xmlversion.h>
#include <libxml/xmlmemory.h>
#include <libxml/debugXML.h>
#include <libxml/HTMLtree.h>
#include <libxml/DOCBparser.h>
#include <libxml/catalog.h>
#include "yelp-pregenerate.h"

static gboolean   parse_books         (GNode        *tree,
                                       xmlDoc       *doc);
static gboolean   parse_section       (GNode        *parent,
                                       xmlNode      *xml_node);
static void       parse_doc_source    (GNode        *parent,
                                       xmlNode      *xml_node,
                                       gchar        *docid);
static gchar *    strip_scheme        (gchar        *original_uri,
                                       gchar        **scheme);

static gboolean   tree_empty	      (xmlNode 	    *cl_node);
static gboolean   trim_empty_branches (xmlNode      *cl_node);
Section *	  section_new 	      (SectionType   type,
				       const gchar  *name);

static gboolean   yelp_pregenerate_write_to_html (gchar 	*xml_file,
					   	  gchar         *html_data);

static GHashTable *seriesid_hash = NULL;
static GHashTable *docid_hash    = NULL;
static GList 	  *xml_list      = NULL;

#define YELP_DB2HTML BINDIR"/yelp-db2html"
#define BUFFER_SIZE 16384


gint 
main (gint argc, gchar **argv) 
{
	/* This utility pre-generates HTML files from a given list of XML
	 * files. 
	 */

	gchar  *xml_url;
	gchar  *xml_file;
	gchar  *command_line = NULL;
 	gchar  *html_data = NULL;	
	GError *error = NULL;
	gint    exit_status;
	GList  *index;
	GNode  *tree;
	FILE   *fp = NULL;
	gchar   buffer[256];
	int    c = 0;
	
	if (argc < 2) {
		g_print ("Pre-generates HTML files from XML files. This uses yelp-db2html executable\nfor XML to HTML conversion.\n\nUsage: yelp-pregenerate [OPTIONS] \n\n"
			 "-a :                 Pregenerate HTMLs for all XML files installed in \n                     the system\n"
			 "-f <XML-LIST-FILE> : Pregenerate HTMLs for all the XML files specified in\n                     file XML-LIST-FILE. Enter full path of XML-LIST-FILE\n"
			 "<XML_FILE(S)>....  : Enter one or more XML files to be converted to \n                     HTML files on the command line\n\n");

		exit (1);
	}

	c = getopt (argc, argv, "af:");

	switch (c) {
	case 'a':
                g_print ("Pre-generating HTMLs for all XML files installed in the system.\n");
                tree = g_new0 (GNode, 1);
                
                yelp_pregenerate_xml_list_init (tree);	
                
                g_node_destroy (tree);
                break;
	case 'f':
                g_print ("Pre-generating HTMLs for the list of XML files specified in %s\n", argv[optind - 1]);

                fp = fopen (argv[optind - 1], "r");

                if (fp == NULL) {
                        g_print ("Error in opening the file %s\n", 
                                 argv[optind - 1]);
                        exit (1);
                }

                while (fgets (buffer, 256, fp) != NULL) {
                        xml_file = g_strchomp (buffer);	
                        xml_list = g_list_append (xml_list, g_strdup(xml_file));
                }

                fclose (fp);
                break;

	case -1:

                if (argc >= 2) {
                        int i = 1;
                        g_print ("Pre-generating HTMLs for the XML files entered on the command line\n");
                        while (i <= argc -1 && argv[i]) {
                                if (g_strrstr (argv[i], ".xml")) {
                                        xml_list = g_list_append (xml_list, argv[i]);
                                } else {
                                        g_print("The file %s entered on the command line is not an XML file\n", argv[i]);
                                }
                                i++;
                        }
                }
                break;

	case '?':
                g_print ("Usage: yelp-pregenerate [OPTIONS] \n\n"
                         "-a :                 Pregenerate HTMLs for all XML files installed in \n                     the system\n"
                         "-f <XML-LIST-FILE> : Pregenerate HTMLs for all the XML files specified in\n                     file XML-LIST-FILE. Enter full path of XML-LIST-FILE\n"
                         "<XML_FILE(S)>....  : Enter one or more XML files to beconverted to \n                     HTML files on the command line\n\n");
                exit (1);

	default :
                exit (1);	

	}

	for (index = xml_list; index; index = index->next) {
		html_data = NULL;
		error = NULL;

		xml_url = (gchar *)index->data;

		if (!g_file_test (xml_url, G_FILE_TEST_EXISTS)) {
			g_print ("File %s to be parsed does not exist\n", xml_url);
			continue;
		}

		command_line = g_strdup_printf ("%s/yelp-db2html %s", 
                                                SERVERDIR,
                                                xml_url);
		
		g_print ("Parsing %s\n", xml_url);

		g_spawn_command_line_sync (command_line,
					   &html_data,
					   NULL,
					   &exit_status,
					   &error);

		if (html_data) {
			yelp_pregenerate_write_to_html (xml_url, html_data);
                }
	}

	g_list_free (xml_list);
	g_free (command_line);
	g_free (html_data);

	exit (0);
}


/* This function gets the list of XML help files installed in the system 
 * by querying the Scrollkeeper database.
 */

gboolean
yelp_pregenerate_xml_list_init (GNode *tree)
{
	gchar       *docpath;
	gchar       *full_command;
	xmlDoc      *doc;
	const GList *node;
	gboolean     success = FALSE;
	gchar 	    *std_err;
	gchar 	    *command, *argument;

	g_return_val_if_fail (tree != NULL, FALSE);

	seriesid_hash = g_hash_table_new_full (g_str_hash,
					       g_str_equal,
					       g_free, NULL);

	docid_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
					    g_free, NULL);

	doc = NULL;

	for (node = gnome_i18n_get_language_list ("LC_MESSAGES"); node; node = node->next) {
		command = "scrollkeeper-get-content-list";
		argument = node->data;
		full_command = g_strconcat (command, 
					    " ", argument, NULL);

		success = g_spawn_command_line_sync (full_command, 
						     &docpath,
						     &std_err, NULL, NULL);
		g_free (full_command);
		g_free (std_err);

		if (!success) {
			g_warning ("Didn't successfully run command: '%s %s'",
			   	   command, argument);
			return FALSE;
		}

		if (docpath) {
			docpath = g_strchomp (docpath);
			doc = xmlParseFile (docpath);
			g_free (docpath);
		}

		if (doc) {
			if (doc->xmlRootNode && 
			    !tree_empty(doc->xmlRootNode->xmlChildrenNode)) {
                                break;
			} else {
				xmlFreeDoc (doc);
				doc = NULL;
			}
		}
	}

	if (doc) {

                trim_empty_branches (doc->xmlRootNode);

                parse_books (tree, doc);

                xmlFreeDoc (doc);
	}

	return TRUE;
}


gboolean
parse_books (GNode *tree, xmlDoc *doc)
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
					section_new (YELP_SECTION_CATEGORY,
					 	     "scrollkeeper"));

	for (node = node->xmlChildrenNode; node; node = node->next) {
		if (!g_ascii_strcasecmp (node->name, "sect")) {
			success = parse_section (book_node, node);
		}
	}

	return TRUE;
}

static gboolean
parse_section (GNode *parent, xmlNode *xml_node)
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
				   section_new (YELP_SECTION_CATEGORY, name));

	for (cur = xml_node->xmlChildrenNode; cur; cur = cur->next) {
		if (!g_ascii_strcasecmp (cur->name, "sect")) {
			parse_section (node, cur);
		}
		else if (!g_ascii_strcasecmp (cur->name, "doc")) {

                        xml_str = xmlGetProp (cur, "docid");
			if (xml_str) {
				docid = g_strdup (xml_str);
				xmlFree (xml_str);
			}

			parse_doc_source (node, cur, docid);
		}
	}

	return TRUE;
}


static void
parse_doc_source (GNode *parent, xmlNode *xml_node, gchar *docid)
{
	xmlNode *cur;
	xmlChar *xml_str;
	gchar   *docsource;

	for (cur = xml_node->xmlChildrenNode; cur; cur = cur->next) {

		if (!g_ascii_strcasecmp (cur->name, "docsource")) {
			xml_str = xmlNodeGetContent (cur);
			docsource = strip_scheme (xml_str, NULL);
		
			xml_list = g_list_append (xml_list, docsource);
			xmlFree (xml_str);
			break;
		}
	}
}


static gchar *
strip_scheme(gchar *original_uri, gchar **scheme)
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


Section *
section_new (SectionType  type,
             const gchar  *name)
{ 
	Section *section;

	section = g_new0 (Section, 1);

	section->type = type;

	if (name) {
		section->name = g_strdup (name);
	} else {
		section->name = g_strdup ("");
	}

	return section;
}

static gboolean
trim_empty_branches (xmlNode *node)
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
			empty = trim_empty_branches (child);
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
tree_empty (xmlNode *cl_node)
{
	xmlNode  *node, *next;
	gboolean  ret_val;

	if (cl_node == NULL)
		return TRUE;

	for (node = cl_node; node != NULL; node = next) {
		next = node->next;

		if (!g_ascii_strcasecmp (node->name, "sect") &&
                    node->xmlChildrenNode->next != NULL) {
			ret_val = tree_empty (node->xmlChildrenNode->next);

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

/* Stores the result of XML to HTML conversion in the same directory where 
 * the XML file resides.
 */

static gboolean
yelp_pregenerate_write_to_html (gchar  *xml_url,
				gchar  *html_data)
{
	gchar 	*pos;
	GString *path;
	gchar   *html_url;
	gint 	 len = 0;
	FILE    *fp;
	
	pos = g_strrstr (xml_url, ".xml");

	len = pos - xml_url;

	path = g_string_new (xml_url);

	path = g_string_truncate (path, len);

	path = g_string_append (path, ".html");

	html_url = g_string_free (path, FALSE);

	fp = fopen (html_url, "w+");

	if (fp == NULL) {
		g_print ("Error in opening file %s\n", html_url);

		return FALSE;
	}

        g_print ("Writing output to %s\n", html_url);

	fwrite (html_data, strlen (html_data), 1, fp);

	fclose (fp);

	return TRUE;
}
