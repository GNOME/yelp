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

#ifndef __SCROLLKEEPER_PARSER_H__
#define __SCROLLKEEPER_PARSER_H__

#include "metadata-parser.h"
#include <glib-object.h>

#define TYPE_SCROLLKEEPER_PARSER         (scrollkeeper_parser_get_type ())
#define SCROLLKEEPER_PARSER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_SCROLLKEEPER_PARSER, ScrollKeeperParser))
#define SCROLLKEEPER_PARSER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TYPE_SCROLLKEEPER_PARSER, ScrollKeeperParserClass))
#define IS_SCROLLKEEPER_PARSER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TYPE_SCROLLKEEPER_PARSER))
#define IS_SCROLLKEEPER_PARSER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TYPE_SCROLLKEEPER_PARSER))
#define SCROLLKEEPER_PARSER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_SCROLLKEEPER_PARSER, ScrollKeeperParserClass))

typedef struct _ScrollKeeperParser      ScrollKeeperParser;
typedef struct _ScrollKeeperParserClass ScrollKeeperParserClass;
typedef struct _ScrollKeeperParserPriv  ScrollKeeperParserPriv;

struct _ScrollKeeperParser {
        GObject                 parent;
        
        ScrollKeeperParserPriv *priv;
};

struct _ScrollKeeperParserClass {
        GObjectClass            parent_class;
        
};

GType              scrollkeeper_parser_get_type (void);
MetaDataParser *   scrollkeeper_parser_new      (void);

#endif /* __SCROLLKEEPER_PARSER_H__ */
