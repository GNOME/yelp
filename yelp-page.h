/*
 * Copyright (C) 2006 Brent Smith  <gnome@nextreality.net>
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
 * Author: Brent Smith  <gnome@nextreality.net>
 */

#ifndef __YELP_PAGE_H__
#define __YELP_PAGE_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _YelpPage YelpPage;

YelpPage     *yelp_page_new          (void);
void          yelp_page_set_id       (YelpPage *page, const gchar *page_id);
const gchar  *yelp_page_get_id       (YelpPage *page);
void          yelp_page_set_title    (YelpPage *page, const gchar *title);
const gchar  *yelp_page_get_title    (YelpPage *page);
void          yelp_page_set_contents (YelpPage *page, const gchar *contents);
const gchar  *yelp_page_get_contents (YelpPage *page);
void          yelp_page_set_prev_id  (YelpPage *page, const gchar *prev_id);
const gchar  *yelp_page_get_prev_id  (YelpPage *page);
void          yelp_page_set_next_id  (YelpPage *page, const gchar *next_id);
const gchar  *yelp_page_get_next_id  (YelpPage *page);
void          yelp_page_set_toc_id   (YelpPage *page, const gchar *toc_id);
const gchar  *yelp_page_get_toc_id   (YelpPage *page);
void          yelp_page_free         (YelpPage *page);

G_END_DECLS

#endif
