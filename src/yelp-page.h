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

typedef struct _YelpPage YelpPage;

/* This needs to be after the typedefs. */
#include "yelp-document.h"

YelpPage *    yelp_page_new_string   (YelpDocument  *document,
				      gchar         *id,
				      const gchar   *content);

GIOStatus     yelp_page_read         (YelpPage      *page,
				      gchar         *buffer,
				      gsize          count,
				      gsize         *bytes_read,
				      YelpError    **error);

const gchar  *yelp_page_get_id       (YelpPage      *page);
const gchar  *yelp_page_get_title    (YelpPage      *page);
const gchar  *yelp_page_get_prev_id  (YelpPage      *page);
const gchar  *yelp_page_get_next_id  (YelpPage      *page);
const gchar  *yelp_page_get_up_id    (YelpPage      *page);
const gchar  *yelp_page_get_toc_id   (YelpPage      *page);

void          yelp_page_set_title    (YelpPage      *page,
				      gchar         *title);
void          yelp_page_set_prev_id  (YelpPage      *page,
				      gchar         *prev_id);
void          yelp_page_set_next_id  (YelpPage      *page,
				      gchar         *next_id);
void          yelp_page_set_up_id    (YelpPage      *page,
				      gchar         *up_id);
void          yelp_page_set_toc_id   (YelpPage      *page,
				      gchar         *toc_id);

void          yelp_page_free         (YelpPage      *page);

G_END_DECLS

#endif
