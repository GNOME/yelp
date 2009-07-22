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

#ifndef __YELP_UTILS_H__
#define __YELP_UTILS_H__

#include <glib/gi18n.h>

typedef enum {
    YELP_RRN_TYPE_DOC = 0,
    YELP_RRN_TYPE_MAL,
    YELP_RRN_TYPE_MAN,
    YELP_RRN_TYPE_INFO,
    YELP_RRN_TYPE_TEXT,
    YELP_RRN_TYPE_HTML,
    YELP_RRN_TYPE_XHTML,
    YELP_RRN_TYPE_TOC,
    YELP_RRN_TYPE_SEARCH,
    YELP_RRN_TYPE_NOT_FOUND,
    YELP_RRN_TYPE_EXTERNAL,
    YELP_RRN_TYPE_ERROR
} YelpRrnType;


/* Generic resolver function.  Takes in the uri (which can be
 * anything) and returns the type (enum above)
 * The result is filled with a new string that the callee
 * must free, except when returning YELP_TYPE_ERROR, when it will
 * be NULL.  The result is the base filename for the document.
 * The section will be filled when the requested uri has a section
 * otherwise, it will be NULL
 * Both *result and *section must be NULL when calling (otherwise
 * we throw an error
 */
YelpRrnType             yelp_uri_resolve        (gchar *uri, 
						   gchar **result,
						   gchar **section);

#endif /* __YELP_UTILS_H__ */
