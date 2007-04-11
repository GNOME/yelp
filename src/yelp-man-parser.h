/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003 Shaun McCance  <shaunm@gnome.org>
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
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#ifndef __YELP_MAN_PARSER_H__
#define __YELP_MAN_PARSER_H__

#include <glib.h>
#include <libxml/tree.h>

typedef struct _YelpManParser YelpManParser;

YelpManParser *     yelp_man_parser_new         (void);
xmlDocPtr           yelp_man_parser_parse_file  (YelpManParser   *parser,
						 gchar           *file,
						 const gchar     *encoding);
void                yelp_man_parser_free        (YelpManParser   *parser);

#endif /* __YELP_MAN_PARSER_H__ */
