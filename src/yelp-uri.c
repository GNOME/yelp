/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Mikael Hallendal <micke@codefactory.se>
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
 * Author: Mikael Hallendal <micke@codefactory.se>
 */

#include <string.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-program.h>

#include "yelp-error.h"
#include "yelp-util.h"
#include "yelp-uri.h"
 
#define d(x)

struct _YelpURI {
	YelpURIType  type;
	gchar       *path;
	gchar       *section;
	gint         ref_count;
};

static gchar *      uri_get_doc_path                 (const gchar  *str_uri);
static YelpURIType  uri_get_doc_type                 (const gchar  *str_uri,
						      const gchar  *doc_path);
static gchar *      uri_get_doc_section              (const gchar  *str_uri);
static gchar *      uri_get_path_from_ghelp_uri      (const gchar  *path);
static gchar *      uri_get_path_from_relative       (const gchar  *path);
static gchar *      uri_locate_help_file             (const gchar  *path,
						      const gchar  *file_name);
static gchar *      uri_locate_help_file_with_lang   (const gchar  *path,
						      const gchar  *file_name,
						      const gchar  *locate);

static gchar *
uri_get_doc_path (const gchar *str_uri)
{
	gchar       *no_anchor_uri;
	gchar       *ret_val = NULL;
	const gchar *ch = NULL;
	
	/* remove the anchor from the doc path */
	if ((ch = strchr (str_uri, '?')) || (ch = strchr (str_uri, '#'))) {
		no_anchor_uri = g_strndup (str_uri, ch - str_uri);
	} else {
		no_anchor_uri = g_strdup (str_uri);
	}

	if (no_anchor_uri[0] == '/') {
		ret_val = g_strdup (no_anchor_uri);
	}
	else if (!g_ascii_strncasecmp (no_anchor_uri, "man:", 4)) {
		ret_val = g_strdup (no_anchor_uri + 4);
	}
	else if (!g_ascii_strncasecmp (no_anchor_uri, "info:", 5)) {
		ret_val = g_strdup (no_anchor_uri + 5);
	}
	else if (!g_ascii_strncasecmp (no_anchor_uri, "toc:", 4)) {
		ret_val = g_strdup (no_anchor_uri + 4);
	}
	else if (!g_ascii_strncasecmp (no_anchor_uri, "index:", 6)) {
		ret_val = g_strdup (no_anchor_uri + 6);
	}
	else if (!g_ascii_strncasecmp (no_anchor_uri, "path:", 5)) {
		ret_val = g_strdup (no_anchor_uri + 5);
	}
	else if (!g_ascii_strncasecmp (no_anchor_uri, "ghelp:", 6)) {
		ret_val = uri_get_path_from_ghelp_uri (no_anchor_uri + 6);
	}
	else if (!g_ascii_strncasecmp (no_anchor_uri, "file:", 5)) {
		ret_val = g_strdup (no_anchor_uri + 5);
	}
	
	g_free (no_anchor_uri);

 	return ret_val;
}

static YelpURIType
uri_get_doc_type (const gchar *str_uri, const gchar *doc_path)
{
	YelpURIType ret_val = YELP_URI_TYPE_UNKNOWN;

     	if (!g_ascii_strncasecmp (str_uri, "man:", 4)) {
                ret_val = YELP_URI_TYPE_MAN;
	}
	else if (!g_ascii_strncasecmp (str_uri, "info:", 5)) {
                ret_val = YELP_URI_TYPE_INFO;
	}
	else if (!g_ascii_strncasecmp (str_uri, "toc:", 4)) {
		ret_val = YELP_URI_TYPE_TOC;
	}
	else if (!g_ascii_strncasecmp (str_uri, "index:", 6)) {
		ret_val = YELP_URI_TYPE_INDEX;
	}
	else if (!g_ascii_strncasecmp (str_uri, "path:", 5)) {
		ret_val = YELP_URI_TYPE_PATH;
	}
	else if (!g_ascii_strncasecmp (str_uri, "file:", 5) ||
		 str_uri[0] == '/') {
		ret_val = YELP_URI_TYPE_FILE;
	}
        else if (!g_ascii_strncasecmp (str_uri, "ghelp:", 6)) {
		gchar *mime_type = NULL;
		gchar *docpath;

		if (doc_path) {
			docpath = (gchar *) doc_path;
		} else {
			docpath = uri_get_doc_path (str_uri);
		}

		if (!g_file_test (docpath, G_FILE_TEST_EXISTS)) {
			return YELP_URI_TYPE_NON_EXISTENT;
		} 
		
		mime_type = gnome_vfs_get_mime_type (docpath);
		
		if (mime_type) {
			if (!g_strcasecmp (mime_type, "text/xml")) {
					ret_val = YELP_URI_TYPE_DOCBOOK_XML;
			}
			else if (!g_strcasecmp (mime_type, "text/sgml")) {
					ret_val = YELP_URI_TYPE_DOCBOOK_SGML;
			}
			else if (!g_strcasecmp (mime_type, "text/html")) {
				ret_val = YELP_URI_TYPE_HTML;
			}
			
			g_free (mime_type);
		}
		
		if (docpath != doc_path) {
			g_free (docpath);
		}
	}

        return ret_val;
}

static gchar *
uri_get_doc_section (const gchar *str_uri)
{
	const gchar *ch = NULL;
	
	if ((ch = strchr (str_uri, '?')) || (ch = strchr (str_uri, '#'))) {
		return g_strdup (ch + 1);
	} 
	
	return NULL;
}

static gchar *
uri_get_path_from_ghelp_uri (const gchar *path)
{
	gchar *ret_val = NULL;
	gchar *work_path;
	
	work_path = g_strdup (path);
	
	g_strstrip (work_path);

	if (path[0] == '/') {
		/* Absolute URL */
		gint i = 1;
		gint len = strlen (work_path);
		
		while (i < len && work_path[i] == '/') {
			++i;
		}

		/* Should we always try to use xml here even if full *
		 * path to a sgml or html document is given?         */

		ret_val = g_strdup (work_path + (i - 1));
	} else {
		ret_val = uri_get_path_from_relative (path);
	}
	
	g_free (work_path);

	return ret_val;
}

static gchar *
uri_get_path_from_relative (const gchar *path)
{
	GnomeProgram *program;
	GSList       *ret_locations = NULL;
	GSList       *node;
	gchar        *doc_id;
	gchar        *file_name;
	gchar        *ret_val = NULL;
	const gchar  *ch;
	
	program = gnome_program_get ();

	if ((ch = strchr (path, '/'))) {
		/* 2:  ghelp:AisleRiot2/Klondike */
		doc_id    = g_strndup (path, ch - path);
		file_name = g_strdup  (ch + 1);
	} else {
		/* 1:  ghelp:nautilus */
		doc_id    = (gchar *)path;
		file_name = (gchar *)path;
	}

	gnome_program_locate_file (program,
				   GNOME_FILE_DOMAIN_HELP,
				   doc_id,
				   FALSE,
				   &ret_locations);
	
	if (!ret_locations) {
		return NULL;
	}
	
	for (node = ret_locations; node; node = node->next) {
		gchar *help_path = node->data;

		d(g_print ("PATH: %s\n", help_path));

		ret_val = uri_locate_help_file (help_path, file_name);
		
		if (ret_val) {
			break;
		}
	}

	g_slist_foreach (ret_locations, (GFunc) g_free, NULL);
	g_slist_free (ret_locations);

	d(g_print ("Absolute path: %s\n", ret_val));
	
	if (doc_id != path) {
		g_free (doc_id);
	}

	if (file_name != path) {
		g_free (file_name);
	}

	return ret_val;
}

static gchar *
uri_locate_help_file (const gchar *path, const gchar *file_name)
{
	gchar       *ret_val;
	const GList *lang_list;

	lang_list = gnome_i18n_get_language_list ("LC_MESSAGES");

	for (;lang_list != NULL; lang_list = lang_list->next) {
		const gchar *lang = lang_list->data;
		
		d(g_print ("lang: %s\n", lang));
				
		/* This has to be a valid language AND a language with
		 * no encoding postfix.  The language will come up without
		 * encoding next */
		if (lang == NULL || strchr (lang, '.') != NULL) {
			continue;
		}

		ret_val = uri_locate_help_file_with_lang (path, file_name, 
							  lang);

		if (!ret_val) {
			/* Check for index file in wanted locale */
			ret_val = uri_locate_help_file_with_lang (path, 
								  "index", 
								  lang);
		}
		
		if (ret_val) {
			return ret_val;
		}
	}

	/* Look in C locale since that exists for almost all documents */
	ret_val = uri_locate_help_file_with_lang (path, file_name, "C");
	
	if (ret_val) {
		return ret_val;
	}
	
	/* Last chance, look for index-file with C lang */
	return uri_locate_help_file_with_lang (path, "index", "C");
}

static gchar *
uri_locate_help_file_with_lang (const gchar *path,
				const gchar *file_name,
				const gchar *lang)
{
	gchar *exts[] = {".xml", ".docbook", ".sgml", ".html", "", NULL};
	gint   i;

	for (i = 0; exts[i] != NULL; i++) {
		gchar *full;
		
		full = g_strconcat (path, "/", lang, "/", 
				       file_name, exts[i], NULL);
		
		if (g_file_test (full, G_FILE_TEST_EXISTS)) {
			return full;
		}
		
		g_free (full);
	}

 	return NULL;
}


YelpURI *
yelp_uri_new (const gchar *str_uri)
{
	YelpURI *uri;

	uri = g_new0 (YelpURI, 1);

	d(g_print ("New YelpURI: %s\n", str_uri));

	uri->path      = uri_get_doc_path (str_uri);
	uri->type      = uri_get_doc_type (str_uri, uri->path);
	uri->section   = uri_get_doc_section (str_uri);
	uri->ref_count = 1;

	d(g_print ("Resulting path: %s section: %s\n", 
		   uri->path, 
		   uri->section));

	return uri;
}

gboolean
yelp_uri_exists (YelpURI *uri)
{
	g_return_val_if_fail (uri != NULL, FALSE);
	
	if (!uri->path) {
		return FALSE;
	}

	if (uri->type == YELP_URI_TYPE_NON_EXISTENT) {
		return FALSE;
	}
	
	return TRUE;
}

YelpURIType
yelp_uri_get_type (YelpURI *uri)
{
	g_return_val_if_fail (uri != NULL, YELP_URI_TYPE_UNKNOWN);
	
	return uri->type;
}

const gchar *
yelp_uri_get_path (YelpURI *uri)
{
	g_return_val_if_fail (uri != NULL, NULL);
	
	return uri->path;
}

const gchar *
yelp_uri_get_section (YelpURI *uri)
{
	g_return_val_if_fail (uri != NULL, NULL);
	
	return uri->section;
}

YelpURI *
yelp_uri_ref (YelpURI *uri)
{
	g_return_val_if_fail (uri != NULL, NULL);
	
	uri->ref_count++;
	
	return uri;
}

void
yelp_uri_unref (YelpURI *uri)
{
	g_return_if_fail (uri != NULL);
	
	uri->ref_count--;

	if (uri->ref_count == 0) {
		d(g_print ("Freeing up URI\n"));
		
		g_free (uri->path);
		g_free (uri->section);
		g_free (uri);
	}
}

YelpURI *
yelp_uri_copy (YelpURI *uri)
{
	YelpURI *new_uri;
	
	new_uri = g_new0 (YelpURI, 1);
	
	new_uri->type      = uri->type;
	new_uri->path      = g_strdup (uri->path);
	new_uri->section   = g_strdup (uri->section);
	new_uri->ref_count = 1;

	return new_uri;
}

YelpURI *
yelp_uri_get_relative (YelpURI *uri, const gchar *link)
{
	YelpURI *new_uri = NULL;
	
	if (!yelp_util_is_url_relative (link)) {
		new_uri = yelp_uri_new (link);
	} else {
		if (uri->type == YELP_URI_TYPE_MAN ||
		    uri->type == YELP_URI_TYPE_INFO) {
			if (link[0] == '#' || link[0] == '?') {
				new_uri = yelp_uri_copy (uri);
				g_free (new_uri->section);
				new_uri->section = g_strdup (link + 1);
			} else {
				g_assert_not_reached ();
			}
		} else {
			gchar *str_uri;
			gchar *new_url;
			
			str_uri = yelp_uri_to_string (uri);
			new_url = yelp_util_resolve_relative_url (str_uri,
								  link);
			g_free (str_uri);

			new_uri = yelp_uri_new (new_url);
			
			g_free (new_url);
		}
	}
	
	return new_uri;
}

gboolean
yelp_uri_equal (YelpURI *uri1, YelpURI *uri2)
{
	if (uri1->type == uri2->type &&
	    yelp_uri_equal_path (uri1, uri2) &&
	    yelp_uri_equal_section (uri1, uri2)) {
		return TRUE;
	}
	
	return FALSE;
}

gboolean
yelp_uri_equal_path (YelpURI *uri1, YelpURI *uri2)
{
	g_return_val_if_fail (uri1 != NULL, FALSE);
	g_return_val_if_fail (uri2 != NULL, FALSE);

	if (uri1->path == NULL) {
		if (uri2->path == NULL) {
			return TRUE;
		} else {
			return FALSE;
		}
	}
	
	if (uri2->path == NULL) {
		return FALSE;
	}

	if (!strcmp (uri1->path, uri2->path)) {
		return TRUE;
	}

	return FALSE;
}

gboolean
yelp_uri_equal_section (YelpURI *uri1, YelpURI *uri2)
{
	g_return_val_if_fail (uri1 != NULL, FALSE);
	g_return_val_if_fail (uri2 != NULL, FALSE);

	if (uri1->section == NULL) {
		if (uri2->section == NULL) {
			return TRUE;
		} else {
			return FALSE;
		}
	}
	
	if (uri2->section == NULL) {
		return FALSE;
	}

	if (!strcmp (uri1->section, uri2->section)) {
		return TRUE;
	}

	return FALSE;
}

gchar *
yelp_uri_to_string (YelpURI *uri)
{
	gchar *type;
	gchar *ret_val = NULL;
	
	g_return_val_if_fail (uri != NULL, NULL);

	if (uri->type == YELP_URI_TYPE_NON_EXISTENT ||
	    uri->type == YELP_URI_TYPE_UNKNOWN) {
		return NULL;
	}

	switch (uri->type) {
	case YELP_URI_TYPE_MAN:
		type = g_strdup ("man:");
		break;
	case YELP_URI_TYPE_INFO:
		type = g_strdup ("info:");
		break;
	case YELP_URI_TYPE_DOCBOOK_XML:
	case YELP_URI_TYPE_DOCBOOK_SGML:
	case YELP_URI_TYPE_HTML:
		type = g_strdup ("ghelp:");
		break;
	case YELP_URI_TYPE_TOC:
		type = g_strdup ("toc:");
		break;
	case YELP_URI_TYPE_PATH:
		type = g_strdup ("path:");
		break;
	case YELP_URI_TYPE_FILE:
		type = g_strdup ("file:");
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	if (uri->section) {
		ret_val = g_strconcat (type, uri->path, "?", uri->section, 
				       NULL);
	} else {
		ret_val = g_strconcat (type, uri->path, NULL);
	}
	
	g_free (type);
	
	d(g_print ("URI_TO_STRING: %s\n", ret_val));
	
	return ret_val;
}
