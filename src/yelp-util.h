/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#ifndef __YELP_UTIL_H__
#define __YELP_UTIL_H__

#include <gtk/gtktreestore.h>
#include <gtk/gtktreemodel.h>
#include "yelp-section.h"

GtkTreeIter *yelp_util_contents_add_section (GtkTreeStore  *store,
					     GtkTreeIter   *parent,
					     YelpSection   *section);
char *       yelp_util_resolve_relative_uri (const char    *base_uri,
					     const char    *uri);

char *       yelp_util_node_to_string_path  (GNode         *node);
GNode *      yelp_util_string_path_to_node  (const char   *string_path,
					     GNode         *root);

GNode *      yelp_util_decompose_path_url   (GNode         *root,
					     const char    *path_url,
					     char         **embedded_url);
char *       yelp_util_compose_path_url     (GNode         *node,
					     const char    *embedded_url);



#endif /* __YELP_UTIL_H__ */
