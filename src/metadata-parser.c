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

#include "metadata-parser.h"

static void metadata_parser_base_init (gpointer g_class);

GType
metadata_parser_get_type (void)
{
        static GType parser_type = 0;
        
        if (!parser_type) {
                static const GTypeInfo parser_info =
                        {
                                sizeof (MetaDataParserIface),
                                (GBaseInitFunc) metadata_parser_base_init,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                0,
                                0,
                                NULL
                        };
                
                parser_type = g_type_register_static (G_TYPE_INTERFACE,
                                                      "MetaDataParser",
                                                      &parser_info,
                                                      0);
                g_type_interface_add_prerequisite (parser_type,
                                                   G_TYPE_OBJECT);
        }
        
        return parser_type;
}

static void 
metadata_parser_base_init (gpointer g_class)
{
        static gboolean initialized = FALSE;

        if (!initialized) {
                g_signal_new ("new_book",
                              TYPE_META_DATA_PARSER,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (MetaDataParserIface, 
					       new_book),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE,
                              1, G_TYPE_POINTER);
        }
        
        initialized = TRUE;
}

gboolean
metadata_parser_parse (MetaDataParser *parser)
{
        g_return_val_if_fail (IS_META_DATA_PARSER (parser), FALSE);
        g_return_val_if_fail (META_DATA_PARSER_GET_IFACE (parser)->parse != NULL,
                              FALSE);
        
        return (* META_DATA_PARSER_GET_IFACE (parser)->parse) (parser);
}
