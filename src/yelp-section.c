/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@codefactory.se>
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

#include "string.h"
#include "yelp-section.h"

YelpSection *
yelp_section_new (YelpSectionType  type,
		  const gchar     *name,
		  YelpURI         *uri)
{
	YelpSection *section;

	section = g_new0 (YelpSection, 1);

	section->type      = type;

	if (name) {
		section->name = g_strdup (name);
	} else {
		section->name = g_strdup ("");
	}
	
	if (uri) {
		section->uri = yelp_uri_ref (uri);
	} else {
		section->uri = NULL;
	}
	
	
	return section;
}

YelpSection *
yelp_section_copy (const YelpSection *section)
{
	return yelp_section_new (section->type, 
				 section->name, 
				 section->uri);
}

void 
yelp_section_free (YelpSection *section)
{
	if (section->name) {
		g_free (section->name);
	}
	
	if (section->uri) {
		yelp_uri_unref (section->uri);
	}
}

gint
yelp_section_compare  (gconstpointer a,
		       gconstpointer b)
{
	return strcmp (((YelpSection *)a)->name, 
		       ((YelpSection *)b)->name);
}
