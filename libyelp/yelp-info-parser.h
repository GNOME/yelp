/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2004, Davyd Madeley
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
 * Author: Davyd Madeley <davyd@madeley.id.au>
 */

#ifndef __YELP_INFO_PARSER_H__
#define __YELP_INFO_PARSER_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <libxml/tree.h>

enum {
    INFO_PARSER_COLUMN_PAGE_NO,
    INFO_PARSER_COLUMN_PAGE_NAME,
    INFO_PARSER_COLUMN_PAGE_CONTENT,
    INFO_PARSER_N_COLUMNS
};


G_GNUC_INTERNAL
GtkTreeStore          *yelp_info_parser_parse_file  (char           *file);
G_GNUC_INTERNAL
xmlDocPtr	       yelp_info_parser_parse_tree  (GtkTreeStore   *tree);

#endif /* __YELP_INFO_PARSER_H__ */
