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

#include "yelp-util.h"

GtkTreeIter *
yelp_util_contents_add_section (GtkTreeStore *store,
                                GtkTreeIter  *parent,
                                YelpSection  *section)
{
	GtkTreeIter *iter;
	
	g_return_val_if_fail (GTK_IS_TREE_STORE (store), NULL);
	g_return_val_if_fail (section != NULL, NULL);

	iter = g_new0 (GtkTreeIter, 1);
	
	gtk_tree_store_append (store, iter, parent);
	
	gtk_tree_store_set (store, iter,
			    0, section->name,
			    1, section,
			    2, TRUE,
			    -1);
	return iter;
}
