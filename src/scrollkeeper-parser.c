/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 CodeFactory AB
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
#include "yelp-book.h"
#include "scrollkeeper-parser.h"

static void       sp_init                   (ScrollKeeperParser      *parser);
static void       sp_class_init             (ScrollKeeperParserClass *klass);
static void       sp_finalize               (GObject                 *object);
static void       sp_metadata_parser_init   (MetaDataParserIface     *iface);
static gboolean   sp_trim_empty_branches    (xmlNode                 *cl_node);
static gboolean   sp_tree_empty             (xmlNode                 *cl_node);

static void       sp_parse_books            (ScrollKeeperParser      *parser, 
					     xmlDoc                  *doc);
static YelpBook * sp_parse_book             (ScrollKeeperParser      *parser, 
					     xmlNode                 *node);
static void       sp_parse_section          (GNode                   *parent,
					     xmlNode                 *xml_node);
static void       sp_parse_doc              (GNode                   *parent,
					     xmlNode                 *xml_node);
static void       sp_parse_toc              (GNode                   *parent,
					     const gchar             *docsource);
static void       sp_parse_toc_section      (GNode                   *parent,
					     xmlNode                 *xml_node,
					     const gchar             *base_uri);
static gchar *    sp_get_xml_docpath        (const gchar             *command,
					     const gchar             *argument);
/* MetaDataParser */
static gboolean   sp_parse                  (MetaDataParser          *parser);

struct _ScrollKeeperParserPriv {
        GSList *paths;
};

GType
scrollkeeper_parser_get_type (void)
{
        static GType parser_type = 0;
        
        if (!parser_type) {
                static const GTypeInfo parser_info = {
                        sizeof (ScrollKeeperParserClass),
                        NULL,
                        NULL,
                        (GClassInitFunc) sp_class_init,
                        NULL,
                        NULL,
                        sizeof (ScrollKeeperParser),
                        0,
                        (GInstanceInitFunc) sp_init,
                };
                
                static const GInterfaceInfo metadata_parser_info = {
                        (GInterfaceInitFunc) sp_metadata_parser_init,
                        NULL,
                        NULL
                };

                parser_type = g_type_register_static (G_TYPE_OBJECT,
                                                      "ScrollKeeperParser",
                                                      &parser_info, 0);

                g_type_add_interface_static (parser_type,
                                             TYPE_META_DATA_PARSER,
                                             &metadata_parser_info);
        }

        return parser_type;
}

static void
sp_init (ScrollKeeperParser *parser)
{
        ScrollKeeperParserPriv *priv;
        
        priv         = g_new0 (ScrollKeeperParserPriv, 1);
        parser->priv = priv;
        priv->paths  = NULL;
}

static void
sp_class_init (ScrollKeeperParserClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);
        
        object_class->finalize = sp_finalize;
}

static void
sp_finalize (GObject *object)
{
        ScrollKeeperParser     *parser;
        ScrollKeeperParserPriv *priv;
        GSList                 *node;
        
        parser = SCROLLKEEPER_PARSER (object);
        priv   = parser->priv;
        
        if (priv->paths) {
                for (node = priv->paths; node; node = node->next) {
                        g_free (node->data);
                }
                
                g_slist_free (priv->paths);
                priv->paths = NULL;
        }

        g_free (priv);

        G_OBJECT_GET_CLASS (object)->finalize (object);
}

static void
sp_metadata_parser_init (MetaDataParserIface *iface)
{
        iface->parse = sp_parse;
}

static gboolean
sp_parse (MetaDataParser *parser)
{
	gchar       *docpath;
	xmlDoc      *doc;
	const GList *node;

	g_return_val_if_fail (IS_SCROLLKEEPER_PARSER (parser), FALSE);

	doc = NULL;

	for (node = gnome_i18n_get_language_list ("LC_MESSAGES"); node; node = node->next) {
		docpath = sp_get_xml_docpath ("scrollkeeper-get-content-list",
					      node->data);

		if (docpath) {
			doc = xmlParseFile (docpath);
			g_free (docpath);
		}

		if (doc) {
			if (doc->xmlRootNode && !sp_tree_empty(doc->xmlRootNode->xmlChildrenNode)) {
				break;
			} else {
				xmlFreeDoc (doc);
				doc = NULL;
			}
		}
	}
		
	if (doc) {
		sp_trim_empty_branches (doc->xmlRootNode);

		sp_parse_books (SCROLLKEEPER_PARSER (parser), doc);
		
		xmlFreeDoc (doc);
	}        

        
        return TRUE;
}

static gboolean
sp_trim_empty_branches (xmlNode *node)
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
			empty = sp_trim_empty_branches (child);
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
sp_tree_empty (xmlNode *cl_node)
{
	xmlNode  *node, *next;
	gboolean  ret_val;

	if (cl_node == NULL)
		return TRUE;

	for (node = cl_node; node != NULL; node = next) {
		next = node->next;

		if (!strcmp (node->name, "sect") &&
		    node->xmlChildrenNode->next != NULL) {
			ret_val = sp_tree_empty (
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

static void
sp_parse_books (ScrollKeeperParser  *parser, 
				 xmlDoc              *doc)
{
	xmlNode  *node;
	YelpBook *book;
	
	g_return_if_fail (IS_SCROLLKEEPER_PARSER (parser));

	node = doc->xmlRootNode;

	if (!node || !node->name || 
	    g_strcasecmp (node->name, "ScrollKeeperContentsList")) {
		g_warning ("Invalid ScrollKeeper XML Contents List!");
		return;
	}

	for (node = node->xmlChildrenNode; node; node = node->next) {
		if (!g_strcasecmp (node->name, "sect")) {
			book = sp_parse_book (parser, node);
			
			if (book) {
				g_signal_emit_by_name (parser, 
						       "new_book",
						       book);
			}
		}
	}
}

static YelpBook *
sp_parse_book (ScrollKeeperParser *parser, 
				xmlNode            *node)
{
	xmlNode     *cur;
	YelpBook    *book;
	xmlChar     *xml_str;
	gchar       *name = NULL;
	
	/* Find the title */
	for (cur = node->xmlChildrenNode; cur; cur = cur->next) {
		if (!g_strcasecmp (cur->name, "title")) {
			xml_str = xmlNodeGetContent (cur);
			
			if (xml_str) {
				name = g_strdup (xml_str);
				xmlFree (xml_str);
			}
		}
	}
	
	if (!name) {
		g_warning ("Couldn't find the name of the book");
		return NULL;
	} 

	g_print ("Parse book: %s\n", name);

	book = yelp_book_new (name, NULL);
	
	for (cur = node->xmlChildrenNode; cur; cur = cur->next) {
		if (!g_strcasecmp (cur->name, "sect")) {
			sp_parse_section (book->root, cur);
		}
		else if (!g_strcasecmp (cur->name, "doc")) {
			sp_parse_doc (book->root, cur);
		}
	}
	
	return book;
}

static void
sp_parse_section (GNode *parent, xmlNode *xml_node)
{
	xmlNode *cur;
	xmlChar *xml_str;
	gchar   *name;
	GNode   *node;
	
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
		return;
	}
	
	node = yelp_book_add_section (parent, name, NULL, NULL);
	
	for (cur = xml_node->xmlChildrenNode; cur; cur = cur->next) {
		if (!g_strcasecmp (cur->name, "sect")) {
			sp_parse_section (node,
								cur);
		}
		else if (!g_strcasecmp (cur->name, "doc")) {
			sp_parse_doc (node, cur);
		}
	}
}

static void
sp_parse_doc (GNode *parent, xmlNode *xml_node) 
{
	xmlNode     *cur;
	xmlChar     *xml_str;
	gchar       *title;
	gchar       *omf;
	gchar       *link;
	gchar       *format;
	GNode       *node;
	gchar       *docsource;
	gchar       *index_location;
	
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
			link    = g_strconcat ("ghelp:", docsource, NULL);
			g_free (xml_str);
		}
		else if (!g_strcasecmp (cur->name, "docformat")) {
			xml_str = xmlNodeGetContent (cur);
			format  = g_strdup (xml_str);
			g_free (xml_str);
		}
	}

	node = yelp_book_add_section (parent, title, link, NULL);
	
	sp_parse_toc (node, docsource);

/* 	index_location = sp_get_xml_docpath ("scrollkeeper-get-index-from-docpath", */
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
sp_parse_toc (GNode *parent, const gchar *docsource)
{
	gchar       *toc_file;
	xmlDoc      *doc = NULL;
	xmlNode     *xml_node;
	
	toc_file = sp_get_xml_docpath ("scrollkeeper-get-toc-from-docpath",
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

	for (; xml_node != NULL; xml_node = xml_node->next) {
		sp_parse_toc_section (parent, xml_node, 
				      ((YelpSection *) parent->data)->uri);
	}
}

static void
sp_parse_toc_section (GNode *parent, xmlNode *xml_node, const gchar *base_uri)
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
		link = g_strconcat ("#", xml_str, NULL);
		xmlFree (xml_str);
	}
	
/* 	if (link) { */
/* 		uri = gnome_vfs_uri_resolve_relative (base_uri, link); */
/* 	} else { */
/* 	} */

	node = yelp_book_add_section (parent, name, base_uri, link);
	
	for (; next_child != NULL; next_child = next_child->next) {
		if (!g_strncasecmp (next_child->name, "tocsect", 7)) {
			sp_parse_toc_section (node, next_child, base_uri);
		}
	}
}

static gchar *
sp_get_xml_docpath (const gchar *command, const gchar *argument)
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

MetaDataParser *
scrollkeeper_parser_new (void)
{
        ScrollKeeperParser *parser;
        
        parser = g_object_new (TYPE_SCROLLKEEPER_PARSER, NULL);
        
        return META_DATA_PARSER (parser);
}
