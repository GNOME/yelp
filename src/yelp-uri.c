/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
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
 * Author: Shaun McCance <shaunm@gnome.org>
 *   Based on implementation by Mikael Hallendal <micke@imendio.com>
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

struct _YelpURIPriv {
    YelpURIType  type;
    gchar       *path;
    gchar       *frag;
    gboolean     exists;
};

/*
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
*/


static void         uri_class_init                   (YelpURIClass *klass);
static void         uri_init                         (YelpURI      *uri);
static void         uri_dispose                      (GObject      *gobject);

static void         uri_parse_uri                    (YelpURI      *uri,
						      const gchar  *uri_str);
static void         uri_parse_ghelp_uri              (YelpURI      *uri,
						      const gchar  *uri_str);
static void         uri_resource_type                (YelpURI      *uri);
static gchar *      uri_locate_file                  (gchar        *path,
						      gchar        *file);
static gchar *      uri_locate_file_lang             (gchar        *path,
						      gchar        *file,
						      gchar        *lang);

static GObjectClass *parent_class;

GType
yelp_uri_get_type (void)
{
    static GType type = 0;

    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpURIClass),
	    NULL,
	    NULL,
	    (GClassInitFunc) uri_class_init,
	    NULL,
	    NULL,
	    sizeof (YelpURI),
	    0,
	    (GInstanceInitFunc) uri_init,
	};
	type = g_type_register_static (G_TYPE_OBJECT,
				       "YelpURI",
				       &info, 0);
    }
    return type;
}

static void
uri_class_init (YelpURIClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = uri_dispose;
}

static void
uri_init (YelpURI *uri)
{
    YelpURIPriv *priv;

    priv = g_new0 (YelpURIPriv, 1);
    uri->priv = priv;

    uri->priv->type = YELP_URI_TYPE_UNKNOWN;
    uri->priv->path = NULL;
    uri->priv->frag = NULL;
}

static void
uri_dispose (GObject *object)
{
    YelpURI *uri = YELP_URI (object);

    g_free (uri->priv->path);
    g_free (uri->priv->frag);

    g_free (uri->priv);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

YelpURI *
yelp_uri_new (const gchar *uri_str)
{
    YelpURI     *uri;

    uri = g_object_new (YELP_TYPE_URI, NULL);

    uri_parse_uri (uri, uri_str);

    return uri;
}

YelpURI *
yelp_uri_new_relative (YelpURI *base, const gchar *uri_str)
{
    YelpURI    *uri;

    uri = g_object_new (YELP_TYPE_URI, NULL);

    uri_parse_uri (uri, uri_str);

    if (yelp_uri_get_resource_type (uri) == YELP_URI_TYPE_RELATIVE) {
	switch (yelp_uri_get_resource_type (base)) {
	case YELP_URI_TYPE_FILE:
	case YELP_URI_TYPE_DOCBOOK_XML:
	case YELP_URI_TYPE_DOCBOOK_SGML:
	case YELP_URI_TYPE_HTML:
	    if (!uri->priv->path || uri->priv->path[0] == '\0') {
		// This is just a fragment reference.
		if (uri->priv->path)
		    g_free (uri->priv->path);

		uri->priv->path = g_strdup (base->priv->path);
		uri->priv->type = base->priv->type;
	    }
	    else if (uri->priv->path[0] == '/') {
		uri_resource_type (uri);
	    }
	    else {
		gchar *base_path;
		gchar *new_path;
		gchar *slash;

		slash = strrchr (base->priv->path, '/');
		base_path = g_strndup (base->priv->path,
				       slash - base->priv->path + 1);
		new_path = g_strconcat (base_path, uri->priv->path, NULL);

		g_free (base_path);
		g_free (uri->priv->path);

		uri->priv->path = new_path;
		uri_resource_type (uri);
	    }
	case YELP_URI_TYPE_MAN:
	case YELP_URI_TYPE_INFO:
	case YELP_URI_TYPE_GHELP:
	case YELP_URI_TYPE_GHELP_OTHER:
	case YELP_URI_TYPE_UNKNOWN:
	case YELP_URI_TYPE_RELATIVE:
	case YELP_URI_TYPE_TOC:
	case YELP_URI_TYPE_INDEX:
	case YELP_URI_TYPE_PATH:
	    // FIXME;
	    break;
	default:
	    g_assert_not_reached ();
	    break;
	}
    }

    return uri;
}


gboolean
yelp_uri_exists (YelpURI *uri)
{
    gboolean  exists;
	
    g_return_val_if_fail (uri != NULL, FALSE);
	
    if (!uri->priv->path)
	return FALSE;

    switch (uri->priv->type) {
    case YELP_URI_TYPE_TOC:
    case YELP_URI_TYPE_MAN:
    case YELP_URI_TYPE_INFO:
    case YELP_URI_TYPE_PATH:
    case YELP_URI_TYPE_UNKNOWN:
	exists = TRUE;
	break;
    case YELP_URI_TYPE_DOCBOOK_XML:
    case YELP_URI_TYPE_DOCBOOK_SGML:
    case YELP_URI_TYPE_HTML:
    case YELP_URI_TYPE_FILE:
	exists = g_file_test (uri->priv->path, G_FILE_TEST_EXISTS);
	break;
    default:
	exists = FALSE;
    }

    return exists;
}

YelpURIType
yelp_uri_get_resource_type (YelpURI *uri) {
    g_return_val_if_fail (uri != NULL, YELP_URI_TYPE_UNKNOWN);
    g_return_val_if_fail (YELP_IS_URI (uri), YELP_URI_TYPE_UNKNOWN);

    return uri->priv->type;
}

gchar *
yelp_uri_get_path (YelpURI *uri)
{
    g_return_val_if_fail (uri != NULL, NULL);
    g_return_val_if_fail (YELP_IS_URI (uri), NULL);

    return g_strdup (uri->priv->path);
}

gchar * yelp_uri_get_fragment (YelpURI *uri)
{
    g_return_val_if_fail (uri != NULL, NULL);
    g_return_val_if_fail (YELP_IS_URI (uri), NULL);

    return g_strdup (uri->priv->frag);
}

gchar *
yelp_uri_to_string (YelpURI *uri)
{
    gchar *type;
    gchar *uri_str = NULL;
	
    g_return_val_if_fail (uri != NULL, NULL);
    g_return_val_if_fail (YELP_IS_URI (uri), NULL);

    if (uri->priv->type == YELP_URI_TYPE_UNKNOWN) {
	if (uri->priv->path)
	    return g_strdup (uri->priv->path);

	return NULL;
    }

    switch (uri->priv->type) {
    case YELP_URI_TYPE_MAN:
	type = g_strdup ("man:");
	break;
    case YELP_URI_TYPE_INFO:
	type = g_strdup ("info:");
	break;
    case YELP_URI_TYPE_DOCBOOK_XML:
    case YELP_URI_TYPE_DOCBOOK_SGML:
    case YELP_URI_TYPE_FILE:
    case YELP_URI_TYPE_HTML:
	type = g_strdup ("file:");
    case YELP_URI_TYPE_GHELP:
	type = g_strdup ("ghelp:");
	break;
    case YELP_URI_TYPE_TOC:
	type = g_strdup ("toc:");
	break;
    case YELP_URI_TYPE_PATH:
	type = g_strdup ("path:");
	break;
    case YELP_URI_TYPE_RELATIVE:
	type = g_strdup ("");
	break;
    default:
	g_assert_not_reached ();
	break;
    }

    if (uri->priv->frag)
	uri_str = g_strconcat (type, uri->priv->path,
			       "#",  uri->priv->frag,
			       NULL);
    else
	uri_str = g_strconcat (type, uri->priv->path, NULL);
	
    g_free (type);

    return uri_str;
}

/******************************************************************************/

static void
uri_parse_uri (YelpURI *uri, const gchar *uri_str)
{
    gchar   *c;
    gchar   *scheme    = NULL;
    gchar   *path      = NULL;

    YelpURIPriv *priv = uri->priv;

    if ((c = strchr (uri_str, '?')) || (c = strchr (uri_str, '#'))) {
	path = g_strndup (uri_str, c - uri_str);
	priv->frag = g_strdup (c + 1);
    } else {
	path = g_strdup (uri_str);
	priv->frag = NULL;
    }

    if ((c = strchr (path, ':'))) {
	priv->path = g_strdup (c + 1);
	scheme     = g_strndup (path, c - path);
    } else {
	priv->type = YELP_URI_TYPE_RELATIVE;
	priv->path = path;
	return;
    }

    g_free (path);

    if (!g_ascii_strcasecmp (scheme, "ghelp") && priv->path[0] != '/') {
	path       = priv->path;
	priv->path = NULL;

	uri_parse_ghelp_uri (uri, path);

	g_free (path);
	return;
    }

    if (!g_ascii_strcasecmp (scheme, "ghelp") || 
	!g_ascii_strcasecmp (scheme, "file")) {

	uri_resource_type (uri);
    }
    else if (!g_ascii_strcasecmp (scheme, "man")) {
	priv->type = YELP_URI_TYPE_MAN;
    }
    else if (!g_ascii_strcasecmp (scheme, "info")) {
	// FIXME: This just throws away info sections.
	if (!g_ascii_strncasecmp (priv->path, "dir", 3)) {
	    g_free (priv->path);
	    priv->path = g_strdup ("info");
	    priv->type = YELP_URI_TYPE_TOC;
	} else {
	    priv->type = YELP_URI_TYPE_INFO;
	}
    }
    else if (!g_ascii_strcasecmp (scheme, "path")) {
	priv->type = YELP_URI_TYPE_PATH;
    }
    else if (!g_ascii_strcasecmp (scheme, "toc")) {
	priv->type = YELP_URI_TYPE_TOC;
    }
}

static void
uri_parse_ghelp_uri (YelpURI *uri, const gchar *uri_str)
{
    GSList *locations = NULL;
    GSList *node;
    gchar  *c;
    gchar  *doc;
    gchar  *file;

    YelpURIPriv  *priv    = uri->priv;
    GnomeProgram *program = gnome_program_get ();

    if ((c = strchr (uri_str, '/'))) {
	/* 2:  ghelp:AisleRiot2/Klondike */
	doc  = g_strndup (uri_str, c - uri_str);
	file = g_strdup (c + 1);
    } else {
	/* 1:  ghelp:nautilus */
	doc  = g_strdup (uri_str);
	file = g_strdup (uri_str);
    }

    gnome_program_locate_file (program,
			       GNOME_FILE_DOMAIN_HELP,
			       doc,
			       FALSE,
			       &locations);

    if (!locations) {
	priv->type = YELP_URI_TYPE_GHELP;
	priv->path = g_strdup (uri_str);
	return;
    }

    for (node = locations; node; node = node->next) {
	gchar *path;

	path = uri_locate_file ((gchar *) node->data, file);

	if (path) {
	    priv->path = path;
	    uri_resource_type (uri);
	    break;
	}
    }

    g_free (doc);
    g_free (file);
}

static void
uri_resource_type (YelpURI *uri)
{
    gchar   *mime_type = gnome_vfs_get_mime_type (uri->priv->path);

    if (mime_type) {
	if (!g_strcasecmp (mime_type, "text/xml"))
	    uri->priv->type = YELP_URI_TYPE_DOCBOOK_XML;
	else if (!g_strcasecmp (mime_type, "text/sgml"))
	    uri->priv->type = YELP_URI_TYPE_DOCBOOK_SGML;
	else if (!g_strcasecmp (mime_type, "text/html"))
	    uri->priv->type = YELP_URI_TYPE_HTML;
    }

    g_free (mime_type);
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

gboolean
yelp_uri_equal (YelpURI *uri1, YelpURI *uri2)
{
    g_return_val_if_fail (uri1 != NULL, FALSE);
    g_return_val_if_fail (uri2 != NULL, FALSE);

    if (uri1->priv->type == uri2->priv->type &&
	yelp_uri_equal_path (uri1, uri2) &&
	yelp_uri_equal_fragment (uri1, uri2))

	return TRUE;
    else
	return FALSE;
}

gboolean
yelp_uri_equal_path (YelpURI *uri1, YelpURI *uri2)
{
    g_return_val_if_fail (uri1 != NULL, FALSE);
    g_return_val_if_fail (uri2 != NULL, FALSE);

    if (uri1->priv->path == NULL) {
	if (uri2->priv->path == NULL)
	    return TRUE;
	else
	    return FALSE;
    }
	
    if (uri2->priv->path == NULL)
	return FALSE;

    if (!strcmp (uri1->priv->path, uri2->priv->path))
	return TRUE;

    return FALSE;
}

gboolean
yelp_uri_equal_fragment (YelpURI *uri1, YelpURI *uri2)
{
    g_return_val_if_fail (uri1 != NULL, FALSE);
    g_return_val_if_fail (uri2 != NULL, FALSE);

    if (uri1->priv->frag == NULL) {
	if (uri2->priv->frag == NULL)
	    return TRUE;
	else
	    return FALSE;
    }

    if (uri2->priv->frag == NULL)
	return FALSE;

    if (!strcmp (uri1->priv->frag, uri2->priv->frag))
	return TRUE;

    return FALSE;
}




/******************************************************************************
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
		doc_id    = g_strndup (path, ch - path);
		file_name = g_strdup  (ch + 1);
	} else {
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

	if (!ret_val) {
		ret_val = g_strdup (path);
	}

	d(g_print ("Absolute path: %s\n", ret_val));
	
	if (doc_id != path) {
		g_free (doc_id);
	}

	if (file_name != path) {
		g_free (file_name);
	}

	return ret_val;
}

gboolean
yelp_uri_exists (YelpURI *uri)
{
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



YelpURI *
yelp_uri_to_index (YelpURI *uri)
{
	YelpURI *ret_val;

	g_return_val_if_fail (uri != NULL, NULL);
	
	ret_val = g_new0 (YelpURI, 1);
	
	ret_val->path      = yelp_uri_to_string (uri);
	ret_val->type      = YELP_URI_TYPE_INDEX;
	ret_val->section   = NULL;
	ret_val->ref_count = 1;
	
	return ret_val;
}

YelpURI *
yelp_uri_from_index (YelpURI *uri)
{ 
	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (uri->type == YELP_URI_TYPE_INDEX, NULL);

	return yelp_uri_new (uri->path);
}

gboolean
yelp_uri_no_path (YelpURI *uri)
{
	g_return_val_if_fail (uri != NULL, TRUE);
	
	if (!uri->path || strcmp (uri->path, "") == 0) {
		return TRUE;
	}

	return FALSE;
}

*/
