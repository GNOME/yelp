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
#include <libgnomevfs/gnome-vfs.h>
#include "yelp-book.h"
#include "yelp-keyword-db.h"
#include "devhelp-parser.h"

static void devhelp_parser_init                 (DevHelpParser        *parser);
static void devhelp_parser_class_init           (DevHelpParserClass   *klass);
static void devhelp_parser_finalize             (GObject              *object);
static void devhelp_parser_metadata_parser_init (MetaDataParserIface  *iface);

/* MetaDataParser */
static gboolean devhelp_parser_parse    (MetaDataParser       *md_parser);

static void devhelp_parser_parse_book           (DevHelpParser        *parser,
						 const GnomeVFSURI    *uri);
static void devhelp_parser_parse_section        (GNode                *parent,
						 xmlNode              *node);

static void devhelp_parser_parse_function       (YelpBook             *book,
						 xmlNode              *node);


struct _DevHelpParserPriv {
        GSList *paths;
};

GType
devhelp_parser_get_type (void)
{
        static GType parser_type = 0;
        
        if (!parser_type) {
                static const GTypeInfo parser_info = {
                        sizeof (DevHelpParserClass),
                        NULL,
                        NULL,
                        (GClassInitFunc) devhelp_parser_class_init,
                        NULL,
                        NULL,
                        sizeof (DevHelpParser),
                        0,
                        (GInstanceInitFunc) devhelp_parser_init,
                };
                
                static const GInterfaceInfo metadata_parser_info = {
                        (GInterfaceInitFunc) devhelp_parser_metadata_parser_init,
                        NULL,
                        NULL
                };

                parser_type = g_type_register_static (G_TYPE_OBJECT,
                                                      "DevHelpParser",
                                                      &parser_info, 0);

                g_type_add_interface_static (parser_type,
                                             TYPE_META_DATA_PARSER,
                                             &metadata_parser_info);
        }

        return parser_type;
}

static void
devhelp_parser_init (DevHelpParser *parser)
{
        DevHelpParserPriv *priv;
        
        priv         = g_new0 (DevHelpParserPriv, 1);
        parser->priv = priv;
        priv->paths  = NULL;
}

static void
devhelp_parser_class_init (DevHelpParserClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);
        
        object_class->finalize = devhelp_parser_finalize;
}

static void
devhelp_parser_finalize (GObject *object)
{
        DevHelpParser     *parser;
        DevHelpParserPriv *priv;
        GSList            *node;
        
        parser = DEVHELP_PARSER (object);
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
devhelp_parser_metadata_parser_init (MetaDataParserIface *iface)
{
        iface->parse = devhelp_parser_parse;
}

static void
devhelp_parse_books (DevHelpParser *parser, 
		     GnomeVFSURI   *base_uri, 
		     GList         *dirs) 
{
	GList            *node;
	GnomeVFSURI      *book_uri;
	GnomeVFSFileInfo *file_info;
	
	for (node = dirs; node; node = node->next) {
		file_info = (GnomeVFSFileInfo *) node->data;
		
		if (file_info->type != GNOME_VFS_FILE_TYPE_REGULAR) {
			g_print ("Wrong type: %s\n", file_info->name);
			
			continue;
		}

		
		g_print ("New book: %s ", file_info->name);
		
		book_uri = gnome_vfs_uri_append_path (base_uri, 
						      file_info->name);

		g_print ("at %s\n", 
			 gnome_vfs_uri_to_string (book_uri,
						  GNOME_VFS_URI_HIDE_NONE));
		
		devhelp_parser_parse_book (parser, book_uri);
		gnome_vfs_uri_unref (book_uri);
	}
}

static gboolean
devhelp_parser_parse (MetaDataParser *md_parser)
{
	DevHelpParserPriv              *priv;
	GnomeVFSResult                  result;
	GList                          *dir_list;
	GSList                         *node;
	GnomeVFSURI                    *base_uri;
	gchar                          *text_uri;
	
        /* Do the funky stuff */
        g_return_val_if_fail (IS_DEVHELP_PARSER (md_parser), FALSE);
        
	priv = DEVHELP_PARSER (md_parser)->priv;
	
        g_print ("DevHelpParser: parsing... \n");

	/* Read the directory and call devhelp_parser_parse_book on every
	   book. Should also read the darn books.xml */

	for (node = priv->paths; node; node = node->next) {
		base_uri = gnome_vfs_uri_new ((gchar *) node->data);

		text_uri = gnome_vfs_uri_to_string (base_uri,
						    GNOME_VFS_URI_HIDE_NONE);

		if (!gnome_vfs_uri_exists (base_uri)) {
			continue;
		}

		result = gnome_vfs_directory_list_load (&dir_list,
							text_uri,
							GNOME_VFS_FILE_INFO_DEFAULT);
		/* Read the directory and add all .devhelps */
/* 		dir_list = g_list_prepend (NULL, "glib-2.0.devhelp"); */

		if (result == GNOME_VFS_OK) {
			devhelp_parse_books (DEVHELP_PARSER (md_parser), 
					     base_uri, 
					     dir_list);
		} else {
			g_print ("Error while listing directory: %s\n",
				 text_uri);
		}
		
		/* Free the elements */
		g_list_free (dir_list);
		g_free (text_uri);
		gnome_vfs_uri_unref (base_uri);
	}

	return TRUE;
}

static GnomeVFSURI *base_uri;

static void
devhelp_parser_parse_book (DevHelpParser *parser, const GnomeVFSURI *uri)
{
	xmlDoc      *doc;
 	xmlNode     *root_node, *cur;
	const gchar *file_name;
	gchar       *name;
	GnomeVFSURI *index_uri;
	xmlChar     *xml_str;
	YelpBook    *book;
	
	file_name = gnome_vfs_uri_get_path (uri);
	
	g_print ("New book: %s\n", file_name);
	
	doc       = xmlParseFile (file_name);
	root_node = xmlDocGetRootElement (doc);

	if (!root_node) {
		g_warning ("Empty document: %s", file_name);
		xmlFreeDoc (doc);
		return;
	}
	
	if (xmlStrcmp (root_node->name, (const xmlChar *) "book")) {
		g_warning ("Document wrong type, got %s, expected 'book': %s", 
			   root_node->name, file_name);
		xmlFreeDoc (doc);
		return;
	}

	xml_str = xmlGetProp (root_node, "name");
	
	if (xml_str) {
		name = g_strdup (xml_str);
		xmlFree (xml_str);
	}


	xml_str = xmlGetProp (root_node, "base");
	
	if (xml_str) {
		g_print ("BASE=%s\n", xml_str);
		base_uri = gnome_vfs_uri_new (xml_str);
		xmlFree (xml_str);
	}

	xml_str = xmlGetProp (root_node, "link");
	
	if (xml_str) {
		index_uri = gnome_vfs_uri_append_path (base_uri, xml_str);
		xmlFree (xml_str);
	} else {
		index_uri = gnome_vfs_uri_dup (base_uri);
	}

	book = yelp_book_new (name, index_uri);

	root_node = root_node->xmlChildrenNode;
	
	while (root_node && xmlIsBlankNode (root_node)) {
		root_node = root_node->next;
	}

	if (!root_node) {
		g_warning ("expected children");
		xmlFreeDoc (doc);
		return;
	}

	if (xmlStrcmp (root_node->name, (const xmlChar *) "chapters")) {
		g_warning ("Document wrong type, got %s, expected 'chapters': %s", 
			   root_node->name, file_name);
		xmlFreeDoc (doc);
		return;
	}
	
	cur = root_node->xmlChildrenNode;

/* 	root = help_book_add_section (NULL, name, index_uri, NULL); */
	
 	while (cur) {
		if (!xmlStrcmp (cur->name, (const xmlChar *) "sub") ||
		    !xmlStrcmp (cur->name, (const xmlChar *) "chapter")) {
			devhelp_parser_parse_section (book->root, cur);
		}
		
		cur = cur->next;
	}

	root_node = root_node->next;

	if (!root_node) {
		xmlFreeDoc (doc);
		return;
	}
	
	while (root_node && xmlIsBlankNode (root_node)) {
		root_node = root_node->next;
	}

	if (root_node) {
		if (xmlStrcmp (root_node->name, (const xmlChar *) "functions")) {
			g_warning ("Document wrong type, got '%s', expected 'functions': %s", 
				   root_node->name, file_name);
			xmlFreeDoc (doc);
			return;
		}

		cur = root_node->xmlChildrenNode;
		
		while (cur) {
			if (!xmlStrcmp (cur->name, (const xmlChar *) "function")) {
				devhelp_parser_parse_function (book, cur); 
			}
			
			cur = cur->next;
		}
	}
	
	xmlFreeDoc (doc);
	
	if (base_uri) {
		gnome_vfs_uri_unref (base_uri);
	}
	
	gnome_vfs_uri_unref (index_uri);
	g_signal_emit_by_name (parser, "new_book", book);
}

static void
devhelp_parser_parse_section (GNode *parent, xmlNode *xml_node)
{
	GNode       *node;
	gchar       *xml_str;
	xmlNode     *cur;
	gchar       *name;
	GnomeVFSURI *uri;

	xml_str = xmlGetProp (xml_node, "name");
	
	if (xml_str) {
		name = g_strdup (xml_str);
		xmlFree (xml_str);
	}

	xml_str = xmlGetProp (xml_node, "link");
	
	if (xml_str) {
		uri = gnome_vfs_uri_append_path (base_uri, xml_str);
	}
	
	node = yelp_book_add_section (parent, name, uri, NULL);
	
	gnome_vfs_uri_unref (uri);

	cur = xml_node->xmlChildrenNode;
	
	while (cur != NULL) {
		if (!xmlStrcmp (cur->name, (const xmlChar *) "sub") ||
		    !xmlStrcmp (cur->name, (const xmlChar *) "chapter")) {
			devhelp_parser_parse_section (node, cur); 
		}
		cur = cur->next;
	}
}

static void
devhelp_parser_parse_function (YelpBook      *book,
			       xmlNode       *xml_node)
{
	gchar       *xml_str;
	gchar       *name;
	gchar       *link;

	YelpKeyword *keyword;
	
	xml_str = xmlGetProp (xml_node, "name");
	
	if (xml_str) {
		name = g_strdup (xml_str);
		xmlFree (xml_str);
	}

	xml_str = xmlGetProp (xml_node, "link");
	
	if (xml_str) {
		link = g_strdup (xml_str);
		xmlFree (xml_str);
	}

	keyword = yelp_keyword_db_new_keyword (name, base_uri, link);

	book->keywords = g_slist_prepend (book->keywords, keyword);

/* 	g_print ("Parsed function: %s - %s\n", name, link); */

	g_free (name);
	g_free (link);
}

MetaDataParser *
devhelp_parser_new (const gchar *path)
{
        DevHelpParser *parser;
        
        parser = g_object_new (TYPE_DEVHELP_PARSER, NULL);
        
        if (path) {
                devhelp_parser_add_path (parser, path);
        }
        
        return META_DATA_PARSER (parser);
}

void
devhelp_parser_add_path (DevHelpParser *parser, const gchar *path)
{
        DevHelpParserPriv *priv;
        
        g_return_if_fail (IS_DEVHELP_PARSER (parser));
        g_return_if_fail (path != NULL);
        
        priv = parser->priv;
        
        priv->paths = g_slist_prepend (priv->paths, g_strdup (path));
}
