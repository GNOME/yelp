/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2003 Shaun McCance <shaunm@gnome.org>
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
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#include <string.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnome/gnome-program.h>

#include "yelp-error.h"
#include "yelp-uri.h"
 
#define d(x)

static void          uri_set_resource_type         (YelpURI      *uri);
static gboolean      uri_parse_uri                 (YelpURI      *uri);
static void          uri_parse_toc_uri             (YelpURI      *uri);
static void          uri_parse_man_uri             (YelpURI      *uri);
static void          uri_parse_ghelp_uri           (YelpURI      *uri);
static gchar *       uri_locate_file               (gchar        *path,
						    gchar        *file);
static gchar *       uri_locate_file_lang          (gchar        *path,
						    gchar        *file,
						    const gchar  *lang);

YelpURI *
yelp_uri_new (const gchar *uri_str)
{
    YelpURI *uri = g_new0 (YelpURI, 1);

    uri->refcount = 1;
    uri->src_uri = g_strdup (uri_str);

    if (!uri_parse_uri (uri)) {
	uri->uri = gnome_vfs_uri_new (uri_str);
	uri_set_resource_type (uri);
    }

    return uri;
}

YelpURI *
yelp_uri_resolve_relative (YelpURI *base, const gchar *uri_str)
{
    YelpURI *uri = g_new0 (YelpURI, 1);

    uri->refcount = 1;
    uri->src_uri = g_strdup (uri_str);

    if (!uri_parse_uri (uri)) {
	uri->uri = gnome_vfs_uri_resolve_relative (base->uri, uri_str);
	uri_set_resource_type (uri);
    }

    return uri;
}

YelpURIType
yelp_uri_get_resource_type (YelpURI *uri)
{
    g_return_val_if_fail (uri != NULL, YELP_URI_TYPE_ERROR);

    return uri->resource_type;
}

static void
uri_set_resource_type (YelpURI *uri)
{
    gchar *uri_str;
    gchar *mime_type;

    g_return_if_fail (uri != NULL);

    if (!uri->uri) {
	uri->resource_type = YELP_URI_TYPE_ERROR;
	return;
    }

    if (strcmp (gnome_vfs_uri_get_scheme (uri->uri), "file")) {
	uri->resource_type = YELP_URI_TYPE_EXTERNAL;
	return;
    }

    uri_str = gnome_vfs_uri_to_string (uri->uri,
				       GNOME_VFS_URI_HIDE_FRAGMENT_IDENTIFIER);
    g_return_if_fail (uri_str != NULL);

    mime_type = gnome_vfs_get_mime_type (uri_str);
    if (!mime_type) {
	g_free (uri_str);
	return;
    }

    if (!strcmp (mime_type, "text/xml"))
	uri->resource_type = YELP_URI_TYPE_DOCBOOK_XML;
    else if (!strcmp (mime_type, "text/sgml"))
	uri->resource_type = YELP_URI_TYPE_DOCBOOK_SGML;
    else if (!strcmp (mime_type, "text/html"))
	uri->resource_type = YELP_URI_TYPE_HTML;
    else
	uri->resource_type = YELP_URI_TYPE_EXTERNAL;

    g_free (mime_type);
    g_free (uri_str);
}

YelpURI *
yelp_uri_ref (YelpURI   *uri)
{
    g_return_val_if_fail (uri != NULL, NULL);

    uri->refcount++;

    return uri;
}

void
yelp_uri_unref (YelpURI   *uri)
{
    g_return_if_fail (uri != NULL);

    if (--uri->refcount < 1) {
	if (uri->uri)
	    gnome_vfs_uri_unref (uri->uri);

	if (uri->src_uri)
	    g_free (uri->src_uri);

	g_free (uri);
    }
}

static gboolean
uri_parse_uri (YelpURI *uri)
{
    if (!strncmp (uri->src_uri, "ghelp:", 6)) {
	uri_parse_ghelp_uri (uri);
	return TRUE;
    }
    else if (!strncmp (uri->src_uri, "man:", 4)) {
	uri_parse_man_uri (uri);
	return TRUE;
    }
    else if (!strncmp (uri->src_uri, "toc:", 4)) {
	uri_parse_toc_uri (uri);
	return TRUE;
    }
    else if (!strncmp (uri->src_uri, "mailto:", 7)) {
	uri->resource_type = YELP_URI_TYPE_MAILTO;
	return TRUE;
    }
    else
	return FALSE;
}

static void
uri_parse_toc_uri (YelpURI *uri)
{
    gchar *c, *str;

    c = strchr (uri->src_uri, ':');

    if (c && *c != '\0' && *(++c) != '\0')
	str = g_strconcat (DATADIR, "/yelp/toc.xml#", c, NULL);
    else
	str = g_strconcat (DATADIR, "/yelp/toc.xml", NULL);

    uri->uri = gnome_vfs_uri_new (str);
    uri->resource_type = YELP_URI_TYPE_TOC;

    g_free (str);
    return;
}

static void
uri_parse_man_uri (YelpURI *uri)
{
    gchar *path;
    // gchar *manpath;

    if ((path = strchr(uri->src_uri, ':')))
	path++;
    else
	return;

    if (path[0] == '/') {
	gchar *str = g_strconcat ("file:", path, NULL);
	uri->uri = gnome_vfs_uri_new (str);
	g_free (str);
	uri->resource_type = YELP_URI_TYPE_MAN;
	return;
    }

    // FIXME: handle "relative" man URI
}

static void
uri_parse_ghelp_uri (YelpURI *uri)
{
    GSList *locations = NULL;
    GSList *node;
    gchar  *path, *c;
    gchar  *doc_id    = NULL;
    gchar  *file_name = NULL;
    gchar  *link_id   = NULL;

    GnomeProgram *program = gnome_program_get ();

    if ((path = strchr(uri->src_uri, ':')))
	path++;
    else
	goto done;

    if (path[0] && path[0] == '/') {
	gchar *str = g_strconcat ("file:", path, NULL);

	if ((c = strchr (str, '?')))
	    *c = '#';

	uri->uri = gnome_vfs_uri_new (str);
	g_free (str);

	uri_set_resource_type (uri);
	return;
    }

    if ((c = strchr (path, '/'))) {
	doc_id = g_strndup (path, c - path);
	path   = c + 1;
    }

    if ((c = strchr (path, '?')) || (c = strchr (path, '#'))) {
	file_name = g_strndup (path, c - path);
	link_id   = g_strdup (c + 1);
    } else {
	file_name = g_strdup (path);
    }

    if (doc_id)
	gnome_program_locate_file (program,
				   GNOME_FILE_DOMAIN_HELP,
				   doc_id,
				   FALSE,
				   &locations);
    else
	gnome_program_locate_file (program,
				   GNOME_FILE_DOMAIN_HELP,
				   file_name,
				   FALSE,
				   &locations);

    if (!locations)
	goto done;

    for (node = locations; node; node = node->next) {
	gchar *full_path, *full_uri;

	full_path = uri_locate_file ((gchar *) node->data, file_name);

	if (full_path) {
	    if (link_id)
		full_uri = g_strconcat ("file://", full_path, "#", link_id, NULL);
	    else
		full_uri = g_strconcat ("file://", full_path, NULL);

	    uri->uri = gnome_vfs_uri_new (full_uri);

	    g_free (full_uri);
	    g_free (full_path);

	    uri_set_resource_type (uri);
	    break;
	}
    }

 done:
    g_free (doc_id);
    g_free (file_name);
    g_free (link_id);

    if (!uri)
	g_warning ("Couldn't resolve ghelp URI: %s", uri->src_uri);
}

static gchar *
uri_locate_file (gchar *path, gchar *file)
{
    gint i;
    gchar *ret;
    const gchar * const * langs;

    langs = g_get_language_names ();

    for (i = 0; langs[i] != NULL; i++) {

	/* This has to be a valid language AND a language with
	 * no encoding postfix.  The language will come up without
	 * encoding next */
	if (strchr (langs[i], '.') != NULL)
	    continue;

	ret = uri_locate_file_lang (path, file, langs[i]);

	if (!ret) {
	    /* Check for index file in wanted locale */
	    ret = uri_locate_file_lang (path, "index", langs[i]);
	}

	if (ret)
	    return ret;
    }

    /* Look in C locale since that exists for almost all documents */
    ret = uri_locate_file_lang (path, file, "C");
    if (ret)
	return ret;

    /* Last chance, look for index-file with C lang */
    return uri_locate_file_lang (path, "index", "C");
}

static gchar *
uri_locate_file_lang (gchar *path, gchar *file, const gchar *lang)
{
    gchar *exts[] = {".xml", ".docbook", ".sgml", ".html", "", NULL};
    gint   i;

    for (i = 0; exts[i] != NULL; i++) {
	gchar *full;

	full = g_strconcat (path, "/", lang, "/", file, exts[i], NULL);

	if (g_file_test (full, G_FILE_TEST_EXISTS))
	    return full;

	g_free (full);
    }

    return NULL;
}
