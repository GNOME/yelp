/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Mikael Hallendal <micke@imendio.com>
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

#define YELP_ERROR yelp_error_quark ()

typedef enum {
	YELP_ERROR_NO_DOC,
	YELP_ERROR_NO_PAGE,
	YELP_ERROR_NO_TOC,
	YELP_ERROR_NO_SGML
} YelpError;

GQuark   yelp_error_quark     (void) G_GNUC_CONST;

void     yelp_set_error       (GError     **error,
			       YelpError    code);

#endif /* __YELP_ERROR_H__ */
