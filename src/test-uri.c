/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *
 * Author: Mikael Hallendal <micke@imendio.com>
 */

#include <config.h>
#include <stdio.h>

#include "yelp-uri.h"

static void 
print_uri (YelpUri *uri)
{
    gchar *type, *tmp, **tmpv;

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
    case YELP_URI_DOCUMENT_TYPE_TOC:
        type = "TOC";
        break;
    case YELP_URI_DOCUMENT_TYPE_SEARCH:
        type = "SEARCH";
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
    case YELP_URI_DOCUMENT_TYPE_UNKNOWN:
        type = "UNKNOWN";
        break;
    }

    printf ("TYPE:  %s\n", type);

    tmp = yelp_uri_get_base_uri (uri);
    if (tmp) {
        printf ("URI:   %s\n", tmp);
        g_free (tmp);
    }

    tmpv = yelp_uri_get_search_path (uri);
    if (tmpv) {
        int i;
        for (i = 0; tmpv[i]; i++) {
            if (i == 0)
                printf ("PATH:  %s\n", tmpv[i]);
            else
                printf ("       %s\n", tmpv[i]);
        }
        g_strfreev (tmpv);
    }

    tmp = yelp_uri_get_page_id (uri);
    if (tmp) {
        printf ("PAGE:  %s\n", tmp);
        g_free (tmp);
    }

    tmp = yelp_uri_get_frag_id (uri);
    if (tmp) {
        printf ("FRAG:  %s\n", tmp);
        g_free (tmp);
    }
}

int
main (int argc, char **argv)
{
    YelpUri *parent = NULL;
    YelpUri *uri = NULL;

    g_type_init ();
    g_log_set_always_fatal (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
        
    if (argc < 2) {
        g_print ("Usage: test-uri uri\n");
        return 1;
    }

    if (argc > 2) {
        parent = yelp_uri_resolve (argv[1]);
        uri = yelp_uri_resolve_relative (parent, argv[2]);
    } else {
        uri = yelp_uri_resolve (argv[1]);
    }
    if (uri) {
        print_uri (uri);
        g_object_unref (uri);
    }
    if (parent) {
        g_object_unref (parent);
    }

    return 0;
}
