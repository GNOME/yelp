/*
 * Copyright (C) 2002 Sun Microsystems Inc.
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
 */

#ifndef __YELP_PREGENERATE_H__
#define __YELP_PREGENERATE_H__ 

typedef struct _Section   Section;

#define SECTION(x) ((Section *) x)

typedef enum {
	YELP_SECTION_DOCUMENT_SECTION,
	YELP_SECTION_DOCUMENT,
	YELP_SECTION_CATEGORY,
	YELP_SECTION_INDEX
} SectionType;

struct _Section {
	gchar           *name;
	SectionType     type;
};

gboolean  yelp_pregenerate_xml_list_init (GNode       	 *tree);

#endif /* __YELP_PREGENERATE_H__*/
