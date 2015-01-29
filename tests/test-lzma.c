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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Shaun McCance <shaunm@gnome.org
 */

#include <config.h>
#include <stdio.h>
#include <string.h>

#include <gio/gio.h>

#include "yelp-lzma-decompressor.h"

int
main (int argc, char **argv)
{
    GConverter *converter;
    GFile *file;
    GFileInputStream *file_stream;
    GInputStream *stream;
    gchar buf[1024];
    gssize bytes;
        
    if (argc < 2) {
        g_printerr ("Usage: test-lzma FILE\n");
        return 1;
    }

    file = g_file_new_for_path (argv[1]);
    file_stream = g_file_read (file, NULL, NULL);
    converter = G_CONVERTER (yelp_lzma_decompressor_new ());
    stream = g_converter_input_stream_new (G_INPUT_STREAM (file_stream),
                                           converter);

    while ((bytes = g_input_stream_read (stream, buf, 1024, NULL, NULL)) > 0) {
        gchar *out = g_strndup (buf, bytes);
        puts (out);
        g_free (out);
    }

    return 0;
}
