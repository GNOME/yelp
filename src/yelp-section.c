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

#include "yelp-section.h"

YelpSection *
yelp_section_new (const gchar *name, const gchar *uri, const gchar *reference)
{
	YelpSection *section;

	g_return_val_if_fail (name != NULL, NULL);
	
	section = g_new0 (YelpSection, 1);
	
	section->name      = g_strdup (name);
	section->uri       = g_strdup (uri);
	section->reference = g_strdup (reference);
	
	return section;
}

GNode *
yelp_section_add_sub (GNode       *parent,
		      YelpSection *section)
{
	GNode *node;
	
	g_return_val_if_fail (section != NULL, NULL);

	node = g_node_new (section);

	if (!parent) {
		return node;
	}

	return g_node_insert (parent, -1, node);
}

YelpSection *
yelp_section_copy (const YelpSection *section)
{
	return yelp_section_new (section->name, 
				 section->uri, 
				 section->reference);
}

void 
yelp_section_free (YelpSection *section)
{
	if (section->name) {
		g_free (section->name);
	}
	
	if (section->uri) {
		g_free (section->uri);
	}
	
	if (section->reference) {
		g_free (section->reference);
	}
}

