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

#include <libxml/parser.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include "scrollkeeper-parser.h"

static void scrollkeeper_parser_init       (ScrollKeeperParser      *parser);
static void scrollkeeper_parser_class_init (ScrollKeeperParserClass *klass);
static void scrollkeeper_parser_finalize   (GObject                 *object);
static void 
scrollkeeper_parser_metadata_parser_init   (MetaDataParserIface     *iface);
static void 
scrollkeeper_parser_trim_empty_branches    (xmlNode                 *cl_node);
static xmlDoc *
scrollkeeper_parser_get_xml_tree_of_locale (gchar                   *locale);
static gboolean
scrollkeeper_parser_tree_empty             (xmlNode                 *cl_node);

static void 
scrollkeeper_parser_parse_books            (ScrollKeeperParser      *parser, 
					    xmlDoc                  *doc);
static YelpBook *
scrollkeeper_parser_parse_book             (ScrollKeeperParser      *parser, 
					    xmlNode                 *node);
static void
scrollkeeper_parser_tree_parse_section     (GNode                   *parent,
					    xmlNode                 *xml_node);
static void
scrollkeeper_parser_tree_parse_doc         (GNode                   *parent,
					    xmlNode                 *xml_node);
/* MetaDataParser */
static gboolean scrollkeeper_parser_parse  (MetaDataParser          *parser);

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
                        (GClassInitFunc) scrollkeeper_parser_class_init,
                        NULL,
                        NULL,
                        sizeof (ScrollKeeperParser),
                        0,
                        (GInstanceInitFunc) scrollkeeper_parser_init,
                };
                
                static const GInterfaceInfo metadata_parser_info = {
                        (GInterfaceInitFunc) scrollkeeper_parser_metadata_parser_init,
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
scrollkeeper_parser_init (ScrollKeeperParser *parser)
{
        ScrollKeeperParserPriv *priv;
        
        priv         = g_new0 (ScrollKeeperParserPriv, 1);
        parser->priv = priv;
        priv->paths  = NULL;
}

static void
scrollkeeper_parser_class_init (ScrollKeeperParserClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);
        
        object_class->finalize = scrollkeeper_parser_finalize;
}

static void
scrollkeeper_parser_finalize (GObject *object)
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
scrollkeeper_parser_metadata_parser_init (MetaDataParserIface *iface)
{
        iface->parse = scrollkeeper_parser_parse;
}

static gboolean
scrollkeeper_parser_parse (MetaDataParser *parser)
{
	xmlDoc      *doc;
	const GList *node;

	g_return_val_if_fail (IS_SCROLLKEEPER_PARSER (parser), FALSE);

	doc = NULL;

	for (node = gnome_i18n_get_language_list ("LC_MESSAGES"); node; node = node->next) {
		doc = scrollkeeper_parser_get_xml_tree_of_locale (node->data);

		if (doc) {
			if (doc->xmlRootNode && !scrollkeeper_parser_tree_empty(doc->xmlRootNode->xmlChildrenNode)) {
				break;
			} else {
				xmlFreeDoc (doc);
				doc = NULL;
			}
		}
	}
		
	if (doc) {
		scrollkeeper_parser_trim_empty_branches (
			doc->xmlRootNode->xmlChildrenNode);

		scrollkeeper_parser_parse_books (
			SCROLLKEEPER_PARSER (parser), doc);

		xmlFreeDoc (doc);
	}        

        
        return TRUE;
}

static void
scrollkeeper_parser_trim_empty_branches (xmlNode *cl_node)
{
	xmlNode *node, *next;

	if (cl_node == NULL) {
		return;
	}

	for (node = cl_node; node != NULL; node = next) {
		next = node->next;

		if (!strcmp (node->name, "sect") && node->xmlChildrenNode->next) {
			scrollkeeper_parser_trim_empty_branches (
				node->xmlChildrenNode->next);
		}

		if (!strcmp (node->name, "sect") && !node->xmlChildrenNode->next) {
			xmlUnlinkNode (node);
			xmlFreeNode (node);
		}
	}
}

/* retrieve the XML tree of a certain locale */
static xmlDoc *
scrollkeeper_parser_get_xml_tree_of_locale (gchar *locale)
{
	xmlDoc    *doc;
	FILE      *pipe;
	gchar     *xml_location;
	gint       bytes_read;
	
	if (locale == NULL)
	    return NULL;

	xml_location = g_new0 (char, 1024);

	/* Use g_snprintf here because we don't know how long the */
	/* location will be                                       */
	g_snprintf (xml_location, 1024, "scrollkeeper-get-content-list %s",
		    locale);

	pipe = popen (xml_location, "r");
	bytes_read = fread ((void *) xml_location, sizeof (char), 1024, pipe);

	/* Make sure that we don't end up out-of-bunds */
	if (bytes_read < 1) {
		pclose (pipe);
		g_free (xml_location);
		return NULL;
	}

	/* Make sure the string is properly terminated */
	xml_location[bytes_read - 1] = '\0';
	
	doc = NULL;

	/* Exit code of 0 means we got a path back from ScrollKeeper */
	if (!pclose (pipe)) {
		doc = xmlParseFile (xml_location);
	}

	g_free (xml_location);

	return doc;
}

/* checks if there is any doc in the tree */
static gboolean
scrollkeeper_parser_tree_empty (xmlNode *cl_node)
{
	xmlNode  *node, *next;
	gboolean  ret_val;

	if (cl_node == NULL)
		return TRUE;

	for (node = cl_node; node != NULL; node = next) {
		next = node->next;

		if (!strcmp (node->name, "sect") &&
		    node->xmlChildrenNode->next != NULL) {
			ret_val = scrollkeeper_parser_tree_empty (
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
scrollkeeper_parser_parse_books (ScrollKeeperParser  *parser, 
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
			book = scrollkeeper_parser_parse_book (parser, node);
			
			if (book) {
				g_signal_emit_by_name (parser, 
						       "new_book",
						       book);
			}
		}
	}
}

static YelpBook *
scrollkeeper_parser_parse_book (ScrollKeeperParser *parser, 
				xmlNode            *node)
{
	xmlNode     *cur;
	YelpBook    *book;
	xmlChar     *xml_str;
	gchar       *name = NULL;
	GnomeVFSURI *index_uri;
	
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
		if (scrollkeeper_parser_tree_empty (cur)) {
			continue;
		}

		if (!g_strcasecmp (cur->name, "sect")) {
			scrollkeeper_parser_tree_parse_section (book->root, 
								cur);
		}
		else if (!g_strcasecmp (cur->name, "doc")) {
			scrollkeeper_parser_tree_parse_doc (book->root, cur);
		}
	}
	
	return book;
}

static void
scrollkeeper_parser_tree_parse_section (GNode *parent, xmlNode *xml_node)
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
		if (scrollkeeper_parser_tree_empty (cur)) {
			continue;
		}

		if (!g_strcasecmp (cur->name, "sect")) {
			scrollkeeper_parser_tree_parse_section (node,
								cur);
		}
		else if (!g_strcasecmp (cur->name, "doc")) {
			scrollkeeper_parser_tree_parse_doc (node, cur);
		}
	}
}

static void
scrollkeeper_parser_tree_parse_doc (GNode *parent, xmlNode *xml_node) 
{
	xmlNode     *cur;
	xmlChar     *xml_str;
	gchar       *title;
	gchar       *omf;
	gchar       *link;
	gchar       *format;
	GnomeVFSURI *uri;
	
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
			link    = g_strconcat ("ghelp:", xml_str, NULL);
			g_free (xml_str);
		}
		else if (!g_strcasecmp (cur->name, "docformat")) {
			xml_str = xmlNodeGetContent (cur);
			format  = g_strdup (xml_str);
			g_free (xml_str);
		}
	}

	uri = gnome_vfs_uri_new (link);

	yelp_book_add_section (parent, title, uri, NULL);
	
	gnome_vfs_uri_unref (uri);
	
	g_free (title);
	g_free (omf);
	g_free (link);
	g_free (format);
}

MetaDataParser *
scrollkeeper_parser_new (void)
{
        ScrollKeeperParser *parser;
        
        parser = g_object_new (TYPE_SCROLLKEEPER_PARSER, NULL);
        
        return META_DATA_PARSER (parser);
}
