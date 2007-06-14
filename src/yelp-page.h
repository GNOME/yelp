/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2006 Brent Smith  <gnome@nextreality.net>
 * Copyright (C) 2007 Shaun McCance  <shaunm@gnome.org>
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
 * Authors: Brent Smith  <gnome@nextreality.net>
 *          Shaun McCance  <shaunm@gnome.org>
 */

#ifndef __YELP_PAGE_H__
#define __YELP_PAGE_H__

#include <glib.h>

#include "yelp-error.h"

G_BEGIN_DECLS

typedef enum {
    YELP_PAGE_MIME_HTML,
    YELP_PAGE_MIME_XHTML
} YelpPageMime;

typedef enum {
    YELP_PAGE_SOURCE_STRING,
    YELP_PAGE_SOURCE_FILE
} YelpPageSource;

typedef struct _YelpPage YelpPage;

/* This needs to be right here to compile. */
#include "yelp-document.h"

struct _YelpPage {
    YelpDocument   *document;
    YelpPageSource  source;
    YelpPageMime    mime;

    gchar  *title;

    /* Do not free content.  The string is owned by the YelpDocument,
     * and it does some internal reference counting to make sure it's
     * only reed when it's no longer referenced.  These strings are
     * just too big to strdup all over the place.
     */
    gchar  *content;
    gsize   content_len;
    gsize   content_offset;

    gchar  *id;
    gchar  *prev_id;
    gchar  *next_id;
    gchar  *up_id;
    gchar  *root_id;
};

YelpPage *    yelp_page_new_string   (YelpDocument  *document,
				      gchar         *id,
				      const gchar   *content,
				      YelpPageMime   mime);

GIOStatus     yelp_page_read         (YelpPage      *page,
				      gchar         *buffer,
				      gsize          count,
				      gsize         *bytes_read,
				      YelpError    **error);
gsize        yelp_page_get_length    (YelpPage      *page);				      

void          yelp_page_free         (YelpPage      *page);

G_END_DECLS

#endif
