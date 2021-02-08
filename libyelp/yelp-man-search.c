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

#include <stdio.h>
#include <gio/gio.h>
#include <gio/gunixinputstream.h>

#include "yelp-man-search.h"
#include "yelp-error.h"

static GInputStream*   yelp_man_search_get_apropos    (const gchar *text,
                                                       GError **error);
static void            yelp_man_search_add_to_builder (GVariantBuilder *builder,
                                                       const gchar *text);

static GInputStream*
yelp_man_search_get_apropos (const gchar *text, GError **error)
{
    gint ystdout;
    GError *err = NULL;
    /* text can be a regex, so "." matches everything */
    const gchar *argv[] = { "apropos", "-l", text == NULL ? "." : text, NULL };
    gchar **my_argv;
    my_argv = g_strdupv ((gchar **) argv);
    if (!g_spawn_async_with_pipes (NULL, my_argv, NULL,
                                   /* apropos can print "<term>: nothing appropriate."
                                      to stderr, discard it */
                                   G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
                                   NULL, NULL, NULL, NULL, &ystdout, NULL, &err)) {
        /* We failed to run the apropos program. Return a "Huh?" error. */
        *error = g_error_new (YELP_ERROR, YELP_ERROR_UNKNOWN,
                              "%s", err->message);
        g_error_free (err);
        g_strfreev (my_argv);
        return NULL;
    }
    g_strfreev (my_argv);
    return (GInputStream*) g_unix_input_stream_new (ystdout, TRUE);
}

static void
yelp_man_search_add_to_builder (GVariantBuilder *builder, const gchar *text)
{
    GError *error = NULL;
    GInputStream *apropos_stream = yelp_man_search_get_apropos (text, &error);
    if (apropos_stream == NULL) {
        g_critical ("Can't run apropos: %s", error->message);
        g_error_free (error);
    } else {
        gchar *line;
        gsize line_len;
        GDataInputStream *stream = g_data_input_stream_new (apropos_stream);
        while ((line = g_data_input_stream_read_line (stream,
                                                      &line_len,
                                                      NULL, NULL))) {
            int section;
            char *title, *description;
            /* the 'm' modifier mallocs the strings for us */
            switch (sscanf (line, "%ms (%d) - %m[^\n]",
                            &title, &section, &description)) {
                case 3: {
                    g_variant_builder_add(builder, "(ssss)",
                                          g_strconcat ("man:", title, NULL),
                                          title,
                                          description,
                                          "");
                } break;
                case 2:
                case 1:
                    // Parse failed, don't leak
                    g_free (title);
                    break;
                default:
                    break;
            }
            g_free (line);
        }
        g_object_unref (stream);
    }
}

GVariant *
yelp_man_search (gchar *text)
{
    GVariantBuilder builder;
    g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ssss)"));
    yelp_man_search_add_to_builder (&builder, text);
    return g_variant_new ("a(ssss)", &builder);
}
