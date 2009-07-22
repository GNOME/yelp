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

#ifndef __YELP_BOOKMARKS_H__
#define __YELP_BOOKMARKS_H__

#include <glib.h>
#include <gtk/gtk.h>

#include "yelp-window.h"

void               yelp_bookmarks_init          (void);
void               yelp_bookmarks_register      (YelpWindow   *window);
void               yelp_bookmarks_add           (const gchar  *uri,
						 YelpWindow   *window);
void               yelp_bookmarks_edit          (void);
void               yelp_bookmarks_write         (void);

#endif /* __YELP_BOOKMARKS_H__ */
