/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2021, Paul Hebble
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Paul Hebble <pjhebble@gmail.com>
 */

#ifndef __YELP_MAN_SEARCH_H__
#define __YELP_MAN_SEARCH_H__

#include <glib-object.h>
#include <gio/gio.h>

#include "yelp-error.h"

G_GNUC_INTERNAL
GVariant *      yelp_man_search                (gchar *text);

#endif /* __YELP_MAN_SEARCH_H__ */
