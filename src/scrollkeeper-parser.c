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

#include "scrollkeeper-parser.h"

static void scrollkeeper_parser_init       (ScrollKeeperParser      *parser);
static void scrollkeeper_parser_class_init (ScrollKeeperParserClass *klass);
static void scrollkeeper_parser_finalize   (GObject                 *object);
static void 
scrollkeeper_parser_metadata_parser_init   (MetaDataParserIface     *iface);

/* MetaDataParser */
static gboolean scrollkeeper_parser_parse  (MetaDataParser         *md_parser);

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
        GSList            *node;
        
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
scrollkeeper_parser_parse (MetaDataParser *md_parser)
{
        /* Do the funky stuff */
        g_return_val_if_fail (IS_SCROLLKEEPER_PARSER (md_parser), FALSE);
        
        g_print ("ScrollKeeperParser: parsing... ");

        return TRUE;
}

MetaDataParser *
scrollkeeper_parser_new (void)
{
        ScrollKeeperParser *parser;
        
        parser = g_object_new (TYPE_SCROLLKEEPER_PARSER, NULL);
        
        return META_DATA_PARSER (parser);
}
