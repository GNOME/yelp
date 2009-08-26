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

#include <config.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "yelp-error.h"

struct _YelpError {
    gchar *title;
    gchar *message;
};

static YelpError *
yelp_error_new_valist (gchar *title, gchar *format, va_list args)
{
    YelpError *error;

    error = g_slice_new (YelpError);
    error->title = g_strdup (title);
    error->message = g_strdup_vprintf (format, args);

    return error;
}

YelpError *
yelp_error_new (gchar *title, gchar *format, ...)
{
    YelpError *error;
    va_list args;

    va_start (args, format);
    error = yelp_error_new_valist (title, format, args);
    va_end (args);

    return error;
}

void
yelp_error_set (YelpError **error, gchar *title, gchar *format, ...)
{
    YelpError *new;
    va_list args;

    va_start (args, format);
    new = yelp_error_new_valist (title, format, args);
    va_end (args);

    if (*error == NULL)
	*error = new;
    else
	g_warning
	    ("YelpError set over the top of a previous YelpError or uninitialized\n"
	     "memory. This indicates a bug in someone's code. You must ensure an\n"
	     "error is NULL before it's set. The overwriting error message was:\n"
	     "%s",
	     new->message);
}

YelpError *
yelp_error_copy (YelpError *error)
{
    YelpError *new;

    new = g_slice_new (YelpError);
    new->title = g_strdup (error->title);
    new->message = g_strdup (error->message);

    return new;
}

const gchar *
yelp_error_get_title (YelpError *error)
{
    g_return_val_if_fail (error != NULL, NULL);
    return error->title;
}

const gchar *
yelp_error_get_message (YelpError *error)
{
    g_return_val_if_fail (error != NULL, NULL);
    return error->message;
}

void
yelp_error_free (YelpError *error)
{
    g_return_if_fail (error != NULL);
    g_free (error->title);
    g_free (error->message);
    g_slice_free (YelpError, error);
}

/******************************************************************************/

GQuark
yelp_gerror_quark (void)
{
    static GQuark q = 0;

    if (q == 0)
	q = g_quark_from_static_string ("yelp-error-quark");

    return q;
}

const gchar *
yelp_gerror_get_title (GError *error)
{
    if (!error || error->domain != YELP_GERROR)
	return _("Unknown Error");

    switch (error->code) {
    case YELP_GERROR_IO:
	return _("Could Not Read File");
    }

    return _("Unknown Error");
}

const gchar *
yelp_gerror_get_message (GError *error)
{
    if (!error || !error->message)
	return _("No information is available about this error.");
    else
	return error->message;
}
