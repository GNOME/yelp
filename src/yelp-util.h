/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@imendio.com>
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
 * Author: Mikael Hallendal <micke@imendio.com>
 */

#ifndef __YELP_UTIL_H__
#define __YELP_UTIL_H__

#include <glib.h>

#include "yelp-uri.h"

gboolean yelp_util_is_url_relative          (const char    *url);

gchar *  yelp_util_resolve_relative_url     (const gchar   *base_uri,
					     const gchar   *url);

gchar *  yelp_util_node_to_string_path      (GNode         *node);
GNode *  yelp_util_string_path_to_node      (const gchar   *string_path,
					     GNode         *root);

GNode *  yelp_util_decompose_path_url       (GNode         *root,
					     const gchar   *path_url,
					     YelpURI      **embedded_uri);
gchar *  yelp_util_compose_path_url         (GNode         *node,
					     const gchar   *embedded_url);

GNode *  yelp_util_find_toplevel            (GNode         *doc_tree,
					     const gchar   *name);

GNode *  yelp_util_find_node_from_name      (GNode         *doc_tree,
					     const gchar   *name);

GNode *  yelp_util_find_node_from_uri       (GNode         *doc_tree,
					     YelpURI       *uri);

gchar *  yelp_util_extract_docpath_from_uri (const gchar   *uri);
gchar *  yelp_util_split_uri                (const gchar   *uri,
					     gchar        **anchor);

const gchar *  yelp_util_find_anchor_in_uri (const gchar   *str_uri);

gchar *  yelp_util_str_remove_multiple_chars (const gchar  *str,
					      const gchar  *chr);

#endif /* __YELP_UTIL_H__ */
