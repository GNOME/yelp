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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <zlib.h>
#ifdef HAVE_LIBBZ2
#include <bzlib.h>
#endif
#include <string.h>

#include "yelp-error.h"
#include "yelp-io-channel.h"

typedef struct _YelpIOChannel YelpIOChannel;
struct _YelpIOChannel {
    GIOChannel  channel;
#ifdef HAVE_LIBBZ2
    BZFILE     *bzin;
#endif
    gzFile      gzin;
};

static GIOStatus    yelp_io_read          (GIOChannel    *channel,
					   gchar         *buffer,
					   gsize          count,
					   gsize         *bytes_read,
					   GError       **error);
static GIOStatus    yelp_io_close         (GIOChannel    *channel,
					   GError       **error);
static void         yelp_io_free          (GIOChannel    *channel);
static GIOStatus    yelp_io_set_flags     (GIOChannel    *channel,
					   GIOFlags       flags,
					   GError       **error);
static GIOFlags     yelp_io_get_flags     (GIOChannel    *channel);

static GIOFuncs yelp_channel_funcs = {
    yelp_io_read,
    NULL,
    NULL,
    yelp_io_close,
    NULL,
    yelp_io_free,
    yelp_io_set_flags,
    yelp_io_get_flags,
};

GIOChannel *
yelp_io_channel_new_file (gchar    *file,
			  GError  **error)
{
    YelpIOChannel *channel;
    GIOChannel    *iochannel;

    g_return_val_if_fail (file != NULL, NULL);

    channel = g_new0(YelpIOChannel, 1);
    iochannel = (GIOChannel *) channel;

#ifdef HAVE_LIBBZ2
    if (g_str_has_suffix (file, ".bz2"))
	channel->bzin = bzopen (file, "r");
    else
#endif
	channel->gzin = gzopen (file, "r");

#ifdef HAVE_LIBBZ2
    if (!channel->bzin && !channel->gzin) {
#else
    if (!channel->gzin) {
#endif
	if (error)
	    g_set_error (error, YELP_ERROR, YELP_ERROR_IO,
			 _("The file ‘%s’ could not be read and decoded. "
			   "The file may be compressed in an unsupported "
			   "format."),
			 file);
	return NULL;
    }

    iochannel->is_readable  = TRUE;
    iochannel->is_seekable  = FALSE;
    iochannel->is_writeable = FALSE;

    g_io_channel_init (iochannel);
    iochannel->close_on_unref = TRUE;
    iochannel->funcs = &yelp_channel_funcs;

    return iochannel;
}

static GIOStatus
yelp_io_read          (GIOChannel    *channel,
		       gchar         *buffer,
		       gsize          count,
		       gsize         *bytes_read,
		       GError       **error)
{
    YelpIOChannel *yelp_channel = (YelpIOChannel *) channel;

#ifdef HAVE_LIBBZ2
    if (yelp_channel->bzin)
	*bytes_read = bzread (yelp_channel->bzin, buffer, count);
    else
#endif
	*bytes_read = gzread (yelp_channel->gzin, buffer, count);

    if (*bytes_read < 0)
	return G_IO_STATUS_ERROR;
    else if (*bytes_read == 0)
	return G_IO_STATUS_EOF;
    else
	return G_IO_STATUS_NORMAL;
}

static GIOStatus
yelp_io_close         (GIOChannel    *channel,
		       GError       **error)
{
    YelpIOChannel *yelp_channel = (YelpIOChannel *) channel;

#ifdef HAVE_LIBBZ2
    if (yelp_channel->bzin)
	bzclose (yelp_channel->bzin);
#endif
    if (yelp_channel->gzin)
	gzclose (yelp_channel->gzin);

    // FIXME: return error on error
    return G_IO_STATUS_NORMAL;
}

static void
yelp_io_free          (GIOChannel    *channel)
{
    YelpIOChannel *yelp_channel = (YelpIOChannel *) channel;

#ifdef HAVE_LIBBZ2
    if (yelp_channel->bzin)
	g_free (yelp_channel->bzin);
#endif

    g_free (yelp_channel);
}

static GIOStatus
yelp_io_set_flags     (GIOChannel    *channel,
		       GIOFlags       flags,
		       GError       **error)
{
    if (flags == G_IO_FLAG_IS_READABLE)
	return G_IO_STATUS_NORMAL;
    else
	return G_IO_STATUS_ERROR;
}

static GIOFlags
yelp_io_get_flags     (GIOChannel    *channel)
{
    return G_IO_FLAG_IS_READABLE;
}

