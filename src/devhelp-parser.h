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

#ifndef __DEVHELP_PARSER_H__
#define __DEVHELP_PARSER_H__

#include "metadata-parser.h"
#include <glib-object.h>

#define TYPE_DEVHELP_PARSER         (devhelp_parser_get_type ())
#define DEVHELP_PARSER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_DEVHELP_PARSER, DevHelpParser))
#define DEVHELP_PARSER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TYPE_DEVHELP_PARSER, DevHelpParserClass))
#define IS_DEVHELP_PARSER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TYPE_DEVHELP_PARSER))
#define IS_DEVHELP_PARSER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TYPE_DEVHELP_PARSER))
#define DEVHELP_PARSER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_DEVHELP_PARSER, DevHelpParserClass))

typedef struct _DevHelpParser      DevHelpParser;
typedef struct _DevHelpParserClass DevHelpParserClass;
typedef struct _DevHelpParserPriv  DevHelpParserPriv;

struct _DevHelpParser {
        GObject            parent;
        
        DevHelpParserPriv *priv;
};

struct _DevHelpParserClass {
        GObjectClass       parent_class;
        
};

GType             devhelp_parser_get_type   (void);
MetaDataParser *  devhelp_parser_new        (const gchar      *path);
void              devhelp_parser_add_path   (DevHelpParser    *parser,
                                             const gchar      *path);

#endif /* __DEVHELP_PARSER_H__ */
