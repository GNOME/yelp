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
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#include <config.h>
#include <stdio.h>
#include <string.h>

#include <gio/gio.h>
#include <gio/gunixoutputstream.h>

#include "yelp-uri.h"

GMainLoop *loop;

static void 
print_uri (gchar *orig, YelpUri *uri, GOutputStream *stream)
{
    GFile *file;
    const gchar *type = NULL;
    gchar *tmp, **tmpv, *out;

    g_output_stream_write (stream, orig, strlen (orig), NULL, NULL);
    g_output_stream_write (stream, "\n", 1, NULL, NULL);

    switch (yelp_uri_get_document_type (uri)) {
    case YELP_URI_DOCUMENT_TYPE_DOCBOOK:
        type = "DOCBOOK";
        break;
    case YELP_URI_DOCUMENT_TYPE_MALLARD:
        type = "MALLARD";
        break;
    case YELP_URI_DOCUMENT_TYPE_MAN:
        type = "MAN";
        break;
    case YELP_URI_DOCUMENT_TYPE_INFO:
        type = "INFO";
        break;
    case YELP_URI_DOCUMENT_TYPE_TEXT:
        type = "TEXT";
        break;
    case YELP_URI_DOCUMENT_TYPE_HTML:
        type = "HTML";
        break;
    case YELP_URI_DOCUMENT_TYPE_XHTML:
        type = "XHTML";
        break;
    case YELP_URI_DOCUMENT_TYPE_HELP_LIST:
        type = "TOC";
        break;
    case YELP_URI_DOCUMENT_TYPE_NOT_FOUND:
        type = "NOT FOUND";
        break;
    case YELP_URI_DOCUMENT_TYPE_EXTERNAL:
        type = "EXTERNAL";
        break;
    case YELP_URI_DOCUMENT_TYPE_ERROR:
        type = "ERROR";
        break;
    case YELP_URI_DOCUMENT_TYPE_UNRESOLVED:
        type = "UNRESOLVED";
        break;
    default:
        g_assert_not_reached ();
        break;
    }

    out = g_strdup_printf ("DOCUMENT TYPE: %s\n", type);
    g_output_stream_write (stream, out, strlen (out), NULL, NULL);
    g_free (out);

    tmp = yelp_uri_get_document_uri (uri);
    if (tmp) {
        out = g_strdup_printf ("DOCUMENT URI:  %s\n", tmp);
        g_output_stream_write (stream, out, strlen (out), NULL, NULL);
        g_free (out);
        g_free (tmp);
    }

    tmp = yelp_uri_get_canonical_uri (uri);
    if (tmp) {
        out = g_strdup_printf ("CANONICAL URI: %s\n", tmp);
        g_output_stream_write (stream, out, strlen (out), NULL, NULL);
        g_free (out);
        g_free (tmp);
    }

    file = yelp_uri_get_file (uri);
    if (file) {
        tmp = g_file_get_uri (file);
        out = g_strdup_printf ("FILE URI:      %s\n", tmp);
        g_output_stream_write (stream, out, strlen (out), NULL, NULL);
        g_free (out);
        g_free (tmp);
        g_object_unref (file);
    }

    tmpv = yelp_uri_get_search_path (uri);
    if (tmpv) {
        int i;
        for (i = 0; tmpv[i]; i++) {
            if (i == 0)
                out = g_strdup_printf ("SEARCH PATH:   %s\n", tmpv[i]);
            else
                out = g_strdup_printf ("               %s\n", tmpv[i]);
            g_output_stream_write (stream, out, strlen (out), NULL, NULL);
            g_free (out);
        }
        g_strfreev (tmpv);
    }

    tmp = yelp_uri_get_page_id (uri);
    if (tmp) {
        out = g_strdup_printf ("PAGE ID:       %s\n", tmp);
        g_output_stream_write (stream, out, strlen (out), NULL, NULL);
        g_free (out);
        g_free (tmp);
    }

    tmp = yelp_uri_get_frag_id (uri);
    if (tmp) {
        out = g_strdup_printf ("FRAG ID:       %s\n", tmp);
        g_output_stream_write (stream, out, strlen (out), NULL, NULL);
        g_free (out);
        g_free (tmp);
    }

    g_output_stream_write (stream, "\0", 1, NULL, NULL);
}

static void run_test (gconstpointer data)
{
    GFileInputStream *stream;
    gchar contents[1024];
    gsize bytes;
    gchar *curi, *newline;
    gchar **uriv;
    GFile *file = G_FILE (data);
    YelpUri *uri;
    GOutputStream *outstream;
    gchar *out;

    stream = g_file_read (file, NULL, NULL);
    g_assert (g_input_stream_read_all (G_INPUT_STREAM (stream),
                                       contents, 1024, &bytes,
                                       NULL, NULL));
    newline = strchr (contents, '\n');
    curi = g_strndup (contents, newline - contents);
    uriv = g_strsplit (curi, " ", 2);
    uri = yelp_uri_new (uriv[0]);
    if (uriv[1] != NULL)
        uri = yelp_uri_new_relative (uri, uriv[1]);
    g_strfreev (uriv);

    yelp_uri_resolve (uri);

    while (!yelp_uri_is_resolved (uri))
        while (g_main_context_pending (NULL))
             g_main_context_iteration (NULL, FALSE);

    outstream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
    print_uri (curi, uri, outstream);
    out = (gchar *) g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (outstream));
    g_free (curi);
    g_assert (!strncmp (out, contents, bytes));
}

static int
run_all_tests (int argc, char **argv)
{
    GFile *dir, *file;
    GFileInfo *info;
    GFileEnumerator *children;
    GList *list = NULL;

    dir = g_file_new_for_path ("uri");
    children = g_file_enumerate_children (dir,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          0, NULL, NULL);
    while ((info = g_file_enumerator_next_file (children, NULL, NULL))) {
        const gchar *name = g_file_info_get_attribute_byte_string (info,
                                                                   G_FILE_ATTRIBUTE_STANDARD_NAME);
        if (!g_str_has_suffix (name, ".test"))
            continue;
        list = g_list_insert_sorted (list, (gchar *) name, (GCompareFunc) strcmp);
    }

    while (list) {
        gchar *test_id = g_strconcat ("/", list->data, NULL);
        file = g_file_get_child (dir, list->data);
        g_test_add_data_func (test_id, file, run_test);
        g_free (test_id);
        list = g_list_delete_link (list, list);
    }

    return g_test_run ();
}

static void
uri_resolved (YelpUri *uri, gchar *orig)
{
    GOutputStream *stream = g_unix_output_stream_new (1, FALSE);
    print_uri (orig, uri, stream);
    g_object_unref (uri);
    g_main_loop_quit (loop);
}

int
main (int argc, char **argv)
{
    YelpUri *parent = NULL;
    YelpUri *uri = NULL;

    g_log_set_always_fatal (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
        
    if (argc < 2) {
        g_test_init (&argc, &argv, NULL);
        return run_all_tests (argc, argv);
    }
    else {
        if (argc > 2) {
            parent = yelp_uri_new (argv[1]);
            uri = yelp_uri_new_relative (parent, argv[2]);
        } else {
            uri = yelp_uri_new (argv[1]);
        }
        if (uri) {
            gchar *orig;
            /* We leak orig if argc > 2.  I don't care. */
            if (argc > 2)
                orig = g_strconcat (argv[1], " ", argv[2], NULL);
            else
                orig = argv[1];
            g_signal_connect (uri, "resolved", G_CALLBACK (uri_resolved), orig);
            yelp_uri_resolve (uri);
        }
        if (parent) {
            g_object_unref (parent);
        }
    }

    loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);

    return 0;
}
