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
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-program.h>

#include "yelp-error.h"
#include "yelp-toc-pager.h"
#include "yelp-uri.h"
 
#define d(x)

static GnomeVFSURI * uri_parse_ghelp_uri           (const gchar  *uri_str);
static gchar *       uri_locate_file               (gchar        *path,
						    gchar        *file);
static gchar *       uri_locate_file_lang          (gchar        *path,
						    gchar        *file,
						    gchar        *lang);

GnomeVFSURI *
yelp_uri_new (const gchar *uri_str)
{
    GnomeVFSURI *uri;

    if (!strncmp (uri_str, "ghelp:", 6)) {
	uri = uri_parse_ghelp_uri (uri_str);
    }
    else {
	uri = gnome_vfs_uri_new (uri_str);
    }

    return uri;
}

GnomeVFSURI *
yelp_uri_resolve_relative (GnomeVFSURI *base, const gchar *uri_str)
{
    GnomeVFSURI *uri;

    if (!strncmp (uri_str, "ghelp:", 6)) {
	uri = uri_parse_ghelp_uri (uri_str);
    }
    else {
	uri = gnome_vfs_uri_resolve_relative (base, uri_str);
    }

    return uri;
}

YelpURIType
yelp_uri_get_resource_type (GnomeVFSURI *uri)
{
    gchar *uri_str;
    gchar *mime_type;
    GnomeVFSURI *toc_uri;
    gchar       *toc_uri_str;
    YelpURIType type = YELP_URI_TYPE_EXTERNAL;

    g_return_val_if_fail (uri != NULL, YELP_URI_TYPE_ERROR);

    if (strcmp (gnome_vfs_uri_get_scheme (uri), "file"))
	return YELP_URI_TYPE_EXTERNAL;

    uri_str = gnome_vfs_uri_to_string (uri,
				       GNOME_VFS_URI_HIDE_FRAGMENT_IDENTIFIER);
    if (!uri_str) {
	return YELP_URI_TYPE_ERROR;
    }

    toc_uri = yelp_pager_get_uri (YELP_PAGER (yelp_toc_pager_get ()));
    toc_uri_str = gnome_vfs_uri_to_string (toc_uri,
					   GNOME_VFS_URI_HIDE_FRAGMENT_IDENTIFIER);
    if (toc_uri_str && !strcmp (uri_str, toc_uri_str)) {
	g_free (uri_str);
	g_free (toc_uri_str);
	return YELP_URI_TYPE_TOC;
    }

    mime_type = gnome_vfs_get_mime_type (uri_str);
    if (!mime_type) {
	g_free (uri_str);
	return YELP_URI_TYPE_ERROR;
    }

    if (!strcmp (mime_type, "text/xml"))
	type = YELP_URI_TYPE_DOCBOOK_XML;
    else if (!strcmp (mime_type, "text/sgml"))
	type = YELP_URI_TYPE_DOCBOOK_SGML;
    else if (!strcmp (mime_type, "text/html"))
	type = YELP_URI_TYPE_HTML;

    g_free (mime_type);
    g_free (uri_str);

    return type;
}


static GnomeVFSURI *
uri_parse_ghelp_uri (const gchar *uri_str)
{
    GSList *locations = NULL;
    GSList *node;
    gchar  *path, *c;
    gchar  *doc_id    = NULL;
    gchar  *file_name = NULL;
    gchar  *link_id   = NULL;
    GnomeVFSURI *uri  = NULL;

    GnomeProgram *program = gnome_program_get ();

    if ((path = strchr(uri_str, ':')))
	path++;
    else
	goto done;

    if (path[0] && path[0] == '/') {
	gchar *str = g_strconcat ("file:", path, NULL);

	if ((c = strchr (str, '?')))
	    *c = '#';

	uri = gnome_vfs_uri_new (str);

	g_free (str);
	return uri;
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

	    uri = gnome_vfs_uri_new (full_uri);

	    g_free (full_uri);
	    g_free (full_path);
	    break;
	}
    }

 done:
    g_free (doc_id);
    g_free (file_name);
    g_free (link_id);

    if (!uri)
	g_warning ("Couldn't resolve ghelp URI: %s", uri_str);

    return uri;
}

static gchar *
uri_locate_file (gchar *path, gchar *file)
{
    gchar *ret;
    const GList *langs;

    langs = gnome_i18n_get_language_list ("LC_MESSAGES");

    for (; langs != NULL; langs = langs->next) {
	gchar *lang = langs->data;

	/* This has to be a valid language AND a language with
	 * no encoding postfix.  The language will come up without
	 * encoding next */
	if (lang == NULL || strchr (lang, '.') != NULL)
	    continue;

	ret = uri_locate_file_lang (path, file, lang);

	if (!ret) {
	    /* Check for index file in wanted locale */
	    ret = uri_locate_file_lang (path, "index", lang);
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
uri_locate_file_lang (gchar *path, gchar *file, gchar *lang)
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
