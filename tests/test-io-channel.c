/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009  Shaun McCance  <shaunm@gnome.org
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
 * Author: Shaun McCance <shaunm@gnome.org
 */

#include <config.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

#include "yelp-io-channel.h"

int
main (int argc, char **argv)
{
    GIOChannel *channel;
    gchar *str;
    gint len;

    g_log_set_always_fatal (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
        
    if (argc < 2) {
	g_printerr ("Usage: test-io-channel FILE\n");
	return 1;
    }

    channel = yelp_io_channel_new_file (argv[1], NULL);
    g_io_channel_read_to_end (channel, &str, &len, NULL);
    printf ("%s\n", str);

    return 0;
}
