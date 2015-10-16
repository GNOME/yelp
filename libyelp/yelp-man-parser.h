/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2010 Shaun McCance <shaunm@gnome.org>
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifndef __YELP_MAN_PARSER_H__
#define __YELP_MAN_PARSER_H__

#include <glib.h>
#include <libxml/tree.h>

typedef struct _YelpManParser YelpManParser;

G_GNUC_INTERNAL
YelpManParser *     yelp_man_parser_new         (void);
G_GNUC_INTERNAL
xmlDocPtr           yelp_man_parser_parse_file  (YelpManParser   *parser,
                                                 gchar           *path,
                                                 GError         **error);
G_GNUC_INTERNAL
void                yelp_man_parser_free        (YelpManParser   *parser);

#endif /* __YELP_MAN_PARSER_H__ */
