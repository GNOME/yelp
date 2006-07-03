/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
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

#include <config.h>

#include <glib/gi18n.h>

#include "yelp-error.h"

GQuark
yelp_error_quark (void)
{
    static GQuark q = 0;

    if (q == 0)
	q = g_quark_from_static_string ("yelp-error-quark");

    return q;
}

const gchar *
yelp_error_get_primary (GError  *error)
{
    if (!error || error->domain != YELP_ERROR)
	return _("An unknown error occured");

    switch (error->code) {
    case YELP_ERROR_NO_DOC:
	return _("Could not load document");
    case YELP_ERROR_NO_PAGE:
	return _("Could not load section");
    case YELP_ERROR_NO_TOC:
	return _("Could not read the table of contents");
    case YELP_ERROR_FORMAT:
	return _("Unsupported Format");
    case YELP_ERROR_IO:
	return _("Could not read document");
    case YELP_ERROR_PROC:
	return _("Could not process document");
    default:
	return _("An unknown error occured");
    }
}

const gchar *
yelp_error_get_secondary (GError  *error)
{
    if (!error || !error->message)
	return _("No information is available about the error.");
    else
	return error->message;
}

YelpError *
yelp_error_new (gchar *title, gchar *format, ...)
{
    YelpError *error;
    va_list args;

    error = g_new0 (YelpError, 1);
    error->title = g_strdup (title);

    va_start (args, format);
    error->text = g_strdup_vprintf (format, args);
    va_end (args);

    return error;
}

void
yelp_error_free (YelpError *error)
{
    g_free (error->title);
    g_free (error->text);
    g_free (error);
}
