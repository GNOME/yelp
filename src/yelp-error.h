/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2002 Mikael Hallendal <micke@imendio.com>
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
 */

#ifndef __YELP_ERROR_H__
#define __YELP_ERROR_H__

#include <glib.h>

typedef struct _YelpError YelpError;

YelpError *       yelp_error_new          (gchar       *title,
					   gchar       *format,
					   ...);
void              yelp_error_set          (YelpError  **error,
					   gchar       *title,
					   gchar       *format,
					   ...);
YelpError *       yelp_error_copy         (YelpError   *error);

const gchar *     yelp_error_get_title    (YelpError   *error);
const gchar *     yelp_error_get_message  (YelpError   *error);

void              yelp_error_free         (YelpError   *error);


#define YELP_GERROR yelp_gerror_quark ()

enum {
    YELP_GERROR_IO
};

GQuark            yelp_gerror_quark       (void) G_GNUC_CONST;
const gchar *     yelp_gerror_get_title   (GError      *error);
const gchar *     yelp_gerror_get_message (GError      *error);

#endif /* __YELP_ERROR_H__ */
