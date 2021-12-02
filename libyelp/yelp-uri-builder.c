/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2014 Igalia S.L.
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

#include "yelp-uri-builder.h"

#define BOGUS_PREFIX "bogus-"
#define BOGUS_PREFIX_LEN 6

gchar *
build_network_uri (const gchar *uri)
{
    GUri *guri, *network_uri;
    gchar *bogus_scheme;
    gchar *path = NULL;
    gchar *retval;
    const gchar *scheme;
    const char *fragment;

    scheme = g_uri_peek_scheme (uri);

    /* Don't mangle URIs for local files */
    if (g_str_equal (scheme, "file"))
        return g_strdup (uri);

    /* We need to use a different scheme from help or ghelp to be able to deal
       with absolute uris in the HTML. Help uri schemes are help:gnome-help/...
       they dont have a slash after the colon so WebKit resolves them as a relative
       url when they are not. This doesn't happen if the current page URI has a different
       scheme from absolute uri scheme.
    */
    bogus_scheme = build_network_scheme (scheme);

    guri = g_uri_parse (uri, G_URI_FLAGS_ENCODED, NULL);
    fragment = g_uri_get_fragment (guri);

    /* Build the URI that will be passed to WebKit. Relative URIs will be
     * automatically resolved by WebKit, so we need to add a leading slash to
     * help: and ghelp: URIs to be considered as absolute by WebKit.
     */
    if (g_str_equal (scheme, "ghelp") || g_str_equal (scheme, "gnome-help") ||
        g_str_equal (scheme, "help") || g_str_equal (scheme, "help-list") ||
        g_str_equal (scheme, "info") || g_str_equal (scheme, "man")) {
        const gchar *upath = g_uri_get_path (guri);
        if (g_str_equal (scheme, "info") && fragment) {
            if (upath[0] == '/')
                path = g_strdup_printf ("%s/%s", upath, fragment);
            else
                path = g_strdup_printf ("/%s/%s", upath, fragment);
            fragment = NULL;
        }
        else if (upath[0] != '/') {
            path = g_strdup_printf ("/%s", upath);
        }
    }

    network_uri = g_uri_build (g_uri_get_flags (guri),
                               bogus_scheme,
                               g_uri_get_userinfo (guri),
                               g_uri_get_host (guri),
                               g_uri_get_port (guri),
                               path ? path : g_uri_get_path (guri),
                               g_uri_get_query (guri),
                               fragment);
    g_free (bogus_scheme);
    g_free (path);
    g_uri_unref (guri);

    retval = g_uri_to_string (network_uri);
    g_uri_unref (network_uri);

    return retval;
}

gchar *
build_yelp_uri (const gchar *uri_str)
{
  gchar *resource;
  int path_len;
  gchar *uri = g_strdup (uri_str);

  if (!g_str_has_prefix (uri, BOGUS_PREFIX))
      return uri;

  memmove (uri, uri + BOGUS_PREFIX_LEN, strlen (uri) - BOGUS_PREFIX_LEN + 1);

  /* Remove some leading slashes */
  if ((resource = strstr (uri, ":"))) {
    resource++;

    if (g_str_has_prefix (uri, "help:")) {
        if (resource[0] == '/')
            memmove (resource, resource + 1, strlen (resource));
    }
    else if (g_str_has_prefix (uri, "ghelp:")) {
        /* This is an ugly heuristic that probably always works. The URI
           mangling ends up not distinguishing between absolute file paths
           and ordinary old ghelp URIs. But absolute file paths should
           have more than one slash, right? This will fail:
               cp foo.xml / && yelp /foo.xml
         */
        if (resource[0] == '/' && !strchr (resource + 1, '/'))
            memmove (resource, resource + 1, strlen (resource));
    }
  }

  /* Remove the trailing slash if any */
  path_len = strlen (uri);
  if (uri[path_len - 1] == '/')
    uri[path_len - 1] = '\0';

  if (g_str_has_prefix (uri, "info:")) {
      gchar *frag;

      frag = g_strrstr (uri, "/");
      if (frag)
          frag[0] = '#';
  }

  return uri;
}

gchar *
build_network_scheme (const gchar *scheme)
{
	return g_strdup_printf (BOGUS_PREFIX "%s", scheme);
}
