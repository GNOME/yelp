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

#ifndef __METADATA_PARSER_H__
#define __METADATA_PARSER_H__

#include <glib.h>
#include <glib-object.h>

#define TYPE_META_DATA_PARSER             (metadata_parser_get_type ())
#define META_DATA_PARSER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_META_DATA_PARSER, MetaDataParser))
#define IS_META_DATA_PARSER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_META_DATA_PARSER))
#define META_DATA_PARSER_GET_IFACE(obj)   ((MetaDataParserIface *)g_type_interface_peek (((GTypeInstance *)META_DATA_PARSER (obj))->g_class, TYPE_META_DATA_PARSER))

typedef struct _MetaDataParser      MetaDataParser;
typedef struct _MetaDataParserIface MetaDataParserIface;

struct _MetaDataParserIface {
        GTypeInterface g_iface;

        /* Signals */
        void       (*new_book)          (MetaDataParser    *parser,
					 GNode             *root);
        
        /* Virtual Table */
        gboolean   (*parse)             (MetaDataParser    *parser);
};

        
GType        metadata_parser_get_type   (void);

gboolean     metadata_parser_parse      (MetaDataParser     *parser);

#endif /* __META_DATA_PARSER_H__ */

