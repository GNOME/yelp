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

#include <libgnome/gnome-i18n.h>

#include "yelp-error.h"

GQuark
yelp_error_quark (void)
{
    static GQuark q = 0;

    if (q == 0)
	q = g_quark_from_static_string ("yelp-error-quark");

    return q;
}

void
yelp_set_error (GError **error, YelpError code)
{
    switch (code) {
    case YELP_ERROR_NO_DOC:
	g_set_error (error,
		     YELP_ERROR,
		     YELP_ERROR_NO_DOC,
		     _("The selected document could not be opened"));
	break;
    case YELP_ERROR_NO_PAGE:
	g_set_error (error,
		     YELP_ERROR,
		     YELP_ERROR_NO_PAGE,
		     _("The selected page could not be found in this document."));
	break;
    case YELP_ERROR_NO_TOC:
	g_set_error (error,
		     YELP_ERROR,
		     YELP_ERROR_NO_TOC,
		     _("The table of contents could not be read."));
	break;
    case YELP_ERROR_NO_SGML:
	g_set_error (error,
		     YELP_ERROR,
		     YELP_ERROR_NO_SGML,
		     _("DocBook SGML documents are no longer supported."));
	break;
    case YELP_ERROR_IO:
	g_set_error (error,
		     YELP_ERROR,
		     YELP_ERROR_IO,
		     _("The selected file could not be read."));
	break;
    default:
	g_assert_not_reached ();
    }
}

