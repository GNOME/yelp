/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009 Shaun McCance <shaunm@gnome.org>
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
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifndef __YELP_ERROR_H__
#define __YELP_ERROR_H__

#include <glib.h>

G_BEGIN_DECLS

#define YELP_ERROR g_quark_from_static_string ("yelp-error")

typedef enum {
    YELP_ERROR_NOT_FOUND,
    YELP_ERROR_CANT_READ,
    YELP_ERROR_PROCESSING,
    YELP_ERROR_UNKNOWN
} YelpError;

G_GNUC_INTERNAL
GError *            yelp_error_copy               (GError *error);

G_END_DECLS

#endif /* __YELP_ERROR_H__ */
