/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003 Shaun McCance  <shaunm@gnome.org>
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
 * Authors: Shaun McCance  <shaunm@gnome.org>
 *          Brent Smith  <gnome@nextreality.net>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnome/gnome-program.h>
#include <libgnome/gnome-init.h>

#include "yelp-utils.h"
#include "yelp-debug.h"

#include <string.h>
#ifndef DON_UTIL
static GHashTable *doc_info_table;

typedef struct {
    gchar       *uri;
    YelpURIType  type;
} DocInfoURI;

struct _YelpDocInfo {
    gchar      *id;
    gchar      *title;
    gchar      *description;
    gchar      *category;
    gchar      *lang;
    gint        lang_priority;

    DocInfoURI *uris;
    gint        num_uris;
    gint        max_uris;

    YelpDocType type;

    YelpPager  *pager;

    gint ref_count;
};

static gchar *mandirs[] = {
    "man0p",
    "man1",
    "man1p",
    "man2",
    "man3",
    "man3p",
    "man4",
    "man5",
    "man6",
    "man7",
    "man8",
    "man9",
    "mann",
    NULL
};

static YelpDocType  get_doc_type       (gchar   *uri);
static gchar *      convert_ghelp_uri  (gchar   *uri);

static gchar *      convert_man_uri    (gchar   *uri, gboolean trust_uri);
static gchar *      convert_info_uri   (gchar   *uri);

static gchar *dot_dir = NULL;

static gchar **infopath = NULL;
static gchar *infopath_d[] = {"/usr/info", "/usr/share/info", 
			      "/usr/local/info", "/usr/local/share/info", 
			      NULL};


const char *
yelp_dot_dir (void)
{
    if (dot_dir == NULL) {
	dot_dir = g_build_filename (g_get_home_dir(), GNOME_DOT_GNOME,
	                            "yelp.d", NULL);
	
	if (!g_file_test (dot_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
	    mkdir (dot_dir, 0750);		
    }

    return dot_dir;
}

/* @uri:       the uri to interpret for the new YelpDocInfo struct 
 * @trust_uri: if the uri is absolute and is known to exist,
 *             then this should be set.  Only makes sense for local
 *             files.  This is here for performance reasons, adding
 *             40,000 man pages and accessing the disk for each one
 *             can get pretty expensive.
 */
YelpDocInfo *
yelp_doc_info_new (const gchar *uri, gboolean trust_uri)
{
    YelpDocInfo *doc;
    gchar       *doc_uri  = NULL;
    gchar       *full_uri = NULL;
    YelpDocType  doc_type = YELP_DOC_TYPE_ERROR;
    YelpURIType  uri_type;
    gchar *cur;

    g_return_val_if_fail (uri != NULL, NULL);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  uri      = \"%s\"\n", uri);

    if (trust_uri)
	full_uri = gnome_vfs_make_uri_from_input (uri);
    else
	full_uri =
	gnome_vfs_make_uri_from_input_with_dirs	(uri,
						 GNOME_VFS_MAKE_URI_DIR_CURRENT);
    
    if (g_str_has_prefix (full_uri, "file:")) {
	if ((cur = strchr (full_uri, '#')))
	    doc_uri = g_strndup (full_uri, cur - full_uri);
	else
	    doc_uri = g_strdup (full_uri);
	doc_type = get_doc_type (doc_uri);
	uri_type = YELP_URI_TYPE_FILE;
    }
    else if (g_str_has_prefix (full_uri, "ghelp:") ||
	g_str_has_prefix (full_uri, "gnome-help:")) {
	doc_uri  = convert_ghelp_uri (full_uri);
	if (doc_uri)
	    doc_type = get_doc_type (doc_uri);
	uri_type = YELP_URI_TYPE_GHELP;
    }
    else if (g_str_has_prefix (full_uri, "man:")) {
	doc_uri  = convert_man_uri (full_uri, trust_uri);
	doc_type = YELP_DOC_TYPE_MAN;
	uri_type = YELP_URI_TYPE_MAN;
    }
    else if (g_str_has_prefix (full_uri, "info:")) {
	doc_uri  = convert_info_uri (full_uri);
	if (!g_str_has_prefix (doc_uri, "man:")) {
	    doc_type = YELP_DOC_TYPE_INFO;
	    uri_type = YELP_URI_TYPE_INFO;
	} else {
	    gchar *tmp;
	    tmp = g_strdup (doc_uri);
	    g_free (doc_uri);
	    doc_uri = convert_man_uri (tmp, trust_uri);
	    g_free (tmp);
	    doc_type = YELP_DOC_TYPE_MAN;
	    uri_type = YELP_URI_TYPE_MAN;	    
	}
    }
    else if (g_str_has_prefix (full_uri, "x-yelp-toc:")) {
	doc_uri = g_strdup ("file://" DATADIR "/yelp/toc.xml");
	doc_type = YELP_DOC_TYPE_TOC;
	uri_type = YELP_URI_TYPE_TOC;
    }
    else if (g_str_has_prefix (full_uri, "x-yelp-search:")) {
	doc_uri = g_strconcat ("file://" DATADIR "/yelp/xslt/search2html.xsl?", full_uri + strlen ("x-yelp-search:"), NULL);
	doc_type = YELP_DOC_TYPE_SEARCH;
	uri_type = YELP_URI_TYPE_SEARCH;
    }
    else {
	doc_uri = g_strdup (uri);
	doc_type = YELP_DOC_TYPE_EXTERNAL;
	uri_type = YELP_URI_TYPE_EXTERNAL;
    }
    
    debug_print (DB_ARG, "  full_uri = \"%s\"\n", full_uri);
    debug_print (DB_ARG, "  doc_uri  = \"%s\"\n", doc_uri);

    g_free (full_uri);

    if (doc_uri) {
	doc = g_new0 (YelpDocInfo, 1);
	doc->uris = g_new (DocInfoURI, 8);
	doc->num_uris = 0;
	doc->max_uris = 8;

	yelp_doc_info_add_uri (doc, doc_uri, YELP_URI_TYPE_FILE);
	if (uri_type && uri_type != YELP_URI_TYPE_FILE)
	    yelp_doc_info_add_uri (doc, uri, uri_type);
	g_free (doc_uri);

	doc->type = doc_type;
	doc->ref_count = 1;
	return doc;
    } else {
	return NULL;
    }
}

YelpDocInfo *
yelp_doc_info_get (const gchar *uri, gboolean trust_uri)
{
    YelpDocInfo *doc;
    gint i;
    gchar *c, *doc_uri;

    g_return_val_if_fail (uri != NULL, NULL);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  uri     = \"%s\"\n", uri);

    if (!doc_info_table)
	doc_info_table =
	    g_hash_table_new_full (g_str_hash,
				   g_str_equal,
				   g_free,
				   (GDestroyNotify) yelp_doc_info_unref);

    c = strchr (uri, '?');
    if (c == NULL)
	c = strchr (uri, '#');

    if (c != NULL)
	doc_uri = g_strndup (uri, c - uri);
    else
	doc_uri = g_strdup (uri);

    doc = (YelpDocInfo *) g_hash_table_lookup (doc_info_table, doc_uri);

    if (!doc) {
	doc = yelp_doc_info_new (uri, trust_uri);
	if (doc && doc->type != YELP_DOC_TYPE_EXTERNAL) {
	    YelpDocInfo *old_doc = NULL;

	    for (i = 0; i < doc->num_uris; i++) {
		old_doc = g_hash_table_lookup (doc_info_table,
					       (doc->uris + i)->uri);
		if (old_doc)
		    break;
	    }

	    if (old_doc) {
		for (i = 0; i < doc->num_uris; i++) {
		    yelp_doc_info_add_uri (old_doc,
					   (doc->uris + i)->uri,
					   (doc->uris + i)->type);
		    g_hash_table_insert (doc_info_table,
					 g_strdup ((doc->uris + i)->uri),
					 yelp_doc_info_ref (old_doc));
		}
		yelp_doc_info_free (doc);
		doc = old_doc;
	    } else {
		for (i = 0; i < doc->num_uris; i++) {
		    g_hash_table_insert (doc_info_table,
					 g_strdup ((doc->uris + i)->uri),
					 yelp_doc_info_ref (doc));
		}
	    }
	}
    }

    g_free (doc_uri);
    return doc;
}

YelpDocInfo *
yelp_doc_info_ref (YelpDocInfo *doc)
{
    g_return_val_if_fail (doc != NULL, NULL);
    (doc->ref_count)++;
    return doc;
}

void
yelp_doc_info_unref (YelpDocInfo *doc)
{
    g_return_if_fail (doc != NULL);

    if (--(doc->ref_count) < 1)
	yelp_doc_info_free (doc);
}

void
yelp_doc_info_free (YelpDocInfo *doc)
{
    gint i;

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  uri = \"%s\"\n", doc->uris->uri);

    if (!doc)
	return;

    if (doc->pager)
	g_object_unref (doc->pager);

    g_free (doc->title);
    for (i = 0; i < doc->num_uris; i++)
	g_free ((doc->uris + i)->uri);
    g_free (doc->uris);
    g_free (doc);
}

YelpPager *
yelp_doc_info_get_pager (YelpDocInfo *doc)
{
    g_return_val_if_fail (doc != NULL, NULL);

    return doc->pager;
}

void
yelp_doc_info_set_pager (YelpDocInfo *doc, YelpPager *pager)
{
    g_return_if_fail (doc != NULL);

    if (doc->pager)
	g_object_unref (doc->pager);

    if (pager)
	g_object_ref (pager);

    doc->pager = pager;
}

const gchar *
yelp_doc_info_get_id (YelpDocInfo *doc)
{
    return (const gchar *) doc->id;
}

void
yelp_doc_info_set_id (YelpDocInfo *doc, gchar *id)
{
    if (doc->id)
	g_free (doc->id);

    doc->id = g_strdup (id);
}

const gchar *
yelp_doc_info_get_title (YelpDocInfo *doc)
{
    return (const gchar *) doc->title;
}

void
yelp_doc_info_set_title (YelpDocInfo *doc, gchar *title)
{
    if (doc->title)
	g_free (doc->title);

    doc->title = g_strdup (title);
}

const gchar *
yelp_doc_info_get_description (YelpDocInfo *doc)
{
    return (const gchar *) doc->description;
}

void
yelp_doc_info_set_description (YelpDocInfo *doc, gchar *desc)
{
    if (doc->description)
	g_free (doc->description);

    doc->description = g_strdup (desc);
}

const gchar *
yelp_doc_info_get_category (YelpDocInfo *doc)
{
    return (const gchar *) doc->category;
}

void
yelp_doc_info_set_category (YelpDocInfo *doc, gchar *category)
{
    if (doc->category)
	g_free (doc->category);

    doc->category = g_strdup (category);
}

const gchar *
yelp_doc_info_get_language (YelpDocInfo *doc)
{
    return (const gchar *) doc->lang;
}

void
yelp_doc_info_set_language (YelpDocInfo *doc, gchar *language)
{
    gint i;
    const gchar * const * langs = g_get_language_names ();

    if (doc->lang)
	g_free (doc->lang);

    doc->lang = g_strdup (language);

    doc->lang_priority = INT_MAX;
    for (i = 0; langs[i] != NULL; i++) {
	if (g_str_equal (language, langs[i])) {
	    doc->lang_priority = i;
	    break;
	}
    }
}

gint
yelp_doc_info_cmp_language (YelpDocInfo *doc1, YelpDocInfo *doc2)
{
    return CLAMP (doc1->lang_priority - doc2->lang_priority, -1, 1);
}

YelpDocType
yelp_doc_info_get_type (YelpDocInfo *doc)
{
    g_return_val_if_fail (doc != NULL, YELP_DOC_TYPE_ERROR);

    return doc->type;
}

gchar *
yelp_doc_info_get_uri (YelpDocInfo *doc,
		       gchar       *frag_id,
		       YelpURIType  uri_type)
{
    gchar *base = NULL;
    gint i;

    g_return_val_if_fail (doc != NULL, NULL);

    for (i = 0; i < doc->num_uris; i++)
	if ((doc->uris + i)->type & uri_type) {
	    base = (doc->uris + i)->uri;
	    break;
	}

    if (!base)
	return NULL;

    if (!frag_id || *frag_id == '\0')
	return g_strdup (base);

    return g_strconcat (base, "#", frag_id, NULL);
}

gchar *
yelp_doc_info_get_filename (YelpDocInfo *doc) {
    gchar *filename = NULL;
    gchar *base = NULL;
    gint i;

    g_return_val_if_fail (doc != NULL, NULL);

    for (i = 0; i < doc->num_uris; i++)
	if ((doc->uris + i)->type == YELP_URI_TYPE_FILE) {
	    base = (doc->uris + i)->uri;
	    break;
	}

    if (g_str_has_prefix (base, "file://"))
	filename = g_strdup (base + 7);

    return filename;
}

gboolean
yelp_doc_info_equal (YelpDocInfo *doc1, YelpDocInfo *doc2)
{
    gboolean equal = TRUE;

    g_return_val_if_fail (doc1 != NULL, FALSE);
    g_return_val_if_fail (doc2 != NULL, FALSE);

    /* FIXME: this sucks */
    if (!g_str_equal (doc1->uris->uri, doc2->uris->uri))
	equal = FALSE;

    return equal;
}

gchar *
yelp_uri_get_fragment (const gchar *uri)
{
    gchar *cur;
    gchar *frag_id = NULL;

    g_return_val_if_fail (uri != NULL, NULL);

    if (g_str_has_prefix (uri, "ghelp:"))
	if ((cur = strchr (uri, '?')))
	    if (*(++cur) != '\0')
		frag_id = g_strdup (cur);

    if (g_str_has_prefix (uri, "x-yelp-toc:"))
	if ((cur = strchr (uri, ':')))
	    if (*(++cur) != '\0')
		frag_id = g_strdup (cur);
    if (g_str_has_prefix (uri, "info:"))
	if ((cur = strchr (uri, ')')))
	    if (*(++cur) != '\0')
		frag_id = g_strdup (cur);
    if ((cur = strchr (uri, '#')))
	if (*(++cur) != '\0') {
	    if (frag_id)
		g_free (frag_id);
	    frag_id = g_strdup (cur);
	}

    return frag_id;
}

gchar *
yelp_uri_get_relative (gchar *base, gchar *ref)
{
    GnomeVFSURI *vfs_base, *vfs_uri;
    gchar *uri;

    vfs_base = gnome_vfs_uri_new (base);
    vfs_uri  = gnome_vfs_uri_resolve_relative (vfs_base, ref);

    uri = gnome_vfs_uri_to_string (vfs_uri, GNOME_VFS_URI_HIDE_NONE);

    gnome_vfs_uri_unref (vfs_base);
    gnome_vfs_uri_unref (vfs_uri);

    return uri;
}

static YelpDocType
get_doc_type (gchar *uri)
{
    gchar *mime_type;
    YelpDocType type;

    g_return_val_if_fail (uri != NULL, YELP_DOC_TYPE_ERROR);

    if (strncmp (uri, "file:", 5))
	return YELP_DOC_TYPE_EXTERNAL;

    mime_type = gnome_vfs_get_mime_type (uri);
    if (mime_type == NULL)
	return YELP_DOC_TYPE_ERROR;

    if (g_str_equal (mime_type, "text/xml") || g_str_equal (mime_type, "application/docbook+xml") || g_str_equal (mime_type, "application/xml"))
	type = YELP_DOC_TYPE_DOCBOOK_XML;
    else if (g_str_equal (mime_type, "text/sgml"))
	type = YELP_DOC_TYPE_DOCBOOK_SGML;
    else if (g_str_equal (mime_type, "text/html"))
	type = YELP_DOC_TYPE_HTML;
    else if (g_str_equal (mime_type, "application/xhtml+xml"))
	type = YELP_DOC_TYPE_XHTML;
    else
	type = YELP_DOC_TYPE_EXTERNAL;

    g_free (mime_type);
    return type;
}

void
yelp_doc_info_add_uri (YelpDocInfo *doc_info,
		       const gchar *uri,
		       YelpURIType  type)
{
    DocInfoURI *info_uri;

    debug_print (DB_FUNCTION, "entering\n");

    g_assert (doc_info->num_uris <= doc_info->max_uris);

    if (doc_info->num_uris == doc_info->max_uris) {
	doc_info->max_uris += 8;
	doc_info->uris = g_renew (DocInfoURI,
				  doc_info->uris,
				  doc_info->max_uris);
    }

    info_uri = doc_info->uris + doc_info->num_uris;

    info_uri->uri  = g_strdup (uri);
    info_uri->type = type;

    doc_info->num_uris++;

    debug_print (DB_ARG, "  uri      = \"%s\"\n", uri);
    debug_print (DB_ARG, "  num_uris = %i\n", doc_info->num_uris);
    debug_print (DB_ARG, "  max_uris = %i\n", doc_info->max_uris);
}

/******************************************************************************/
/** Convert fancy URIs to file URIs *******************************************/

static gchar *
locate_file_lang (gchar *path, gchar *file, const gchar *lang)
{
    gchar *exts[] = {".xml", ".docbook", ".sgml", ".html", "", NULL};
    gint   i;
    gchar *full, *uri = NULL;

    for (i = 0; exts[i] != NULL; i++) {
	full = g_strconcat (path, "/", lang, "/", file, exts[i], NULL);
	if (g_file_test (full, G_FILE_TEST_IS_REGULAR))
	    uri = g_strconcat ("file://", full, NULL);
	g_free (full);
	if (uri)
	    return uri;
    }
    return NULL;
}

static gchar *
convert_ghelp_uri (gchar *uri)
{
    GSList *locations = NULL;
    GSList *node;
    gchar  *path, *cur;
    gchar  *doc_id   = NULL;
    gchar  *doc_name = NULL;
    gchar  *doc_uri  = NULL;

    GnomeProgram *program = gnome_program_get ();

    if ((path = strchr(uri, ':')))
	path++;
    else
	goto done;

    if (path && path[0] == '/') {
	if ((cur = strchr (path, '?')) || (cur = strchr (path, '#')))
	    *cur = '\0';
	doc_uri = g_strconcat ("file://", path, NULL);
	if (cur)
	    *cur = '#';

	goto done;
    }

    if ((cur = strchr (path, '/'))) {
	doc_id = g_strndup (path, cur - path);
	path   = cur + 1;
    }

    if ((cur = strchr (path, '?')) || (cur = strchr (path, '#')))
	doc_name = g_strndup (path, cur - path);
    else
	doc_name = g_strdup (path);

    if (doc_id)
	gnome_program_locate_file (program,
				   GNOME_FILE_DOMAIN_HELP,
				   doc_id,
				   FALSE,
				   &locations);
    else
	gnome_program_locate_file (program,
				   GNOME_FILE_DOMAIN_HELP,
				   doc_name,
				   FALSE,
				   &locations);

    if (!locations)
	goto done;

    for (node = locations; node; node = node->next) {
	gint i;
	const gchar * const * langs;
	gchar *location = (gchar *) node->data;

	langs = g_get_language_names ();

	for (i = 0; langs[i] != NULL; i++) {

	    /* This has to be a valid language AND a language with
	     * no encoding postfix.  The language will come up without
	     * encoding next */
	    if (strchr (langs[i], '.') != NULL)
		continue;

	    doc_uri = locate_file_lang (location, doc_name, langs[i]);

	    if (!doc_uri)
		doc_uri = locate_file_lang (location, "index", langs[i]);

	    if (doc_uri)
		goto done;
	}

	/* Look in C locale since that exists for almost all docs */
	doc_uri = locate_file_lang (location, doc_name, "C");
	if (doc_uri)
	    goto done;

	/* Last chance, look for index-file with C lang */
	doc_uri = locate_file_lang (location, "index", "C");
    }

 done:
    if (locations) {
	g_slist_foreach (locations, (GFunc) g_free, NULL);
	g_slist_free (locations);
    }
    g_free (doc_id);
    g_free (doc_name);

    if (!doc_uri)
	g_warning ("Could not resolve ghelp URI: %s", uri);

    return doc_uri;
}

static gchar *
convert_man_uri (gchar *uri, gboolean trust_uri)
{
    gchar *path, *cur;
    gchar *doc_uri  = NULL;
    gchar *man_name = NULL;
    gchar *man_num  = NULL;
    gchar *man_dir  = NULL;
    const gchar * const * langs = g_get_language_names ();
    gint langs_i;

    static gchar **manpath = NULL;
    gint i, j;

    GDir  *dir;
    gchar *dirname;
    const gchar *filename;
    gchar *pattern;
    GPatternSpec *pspec = NULL;

    if ((path = strchr(uri, ':')))
	path++;
    else
	goto done;

    /* An absolute file path after man: */
    if (path[0] == '/') {
	if (trust_uri)
	    doc_uri = g_strconcat ("file://", path, NULL);
	else if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
	    doc_uri = g_strconcat ("file://", path, NULL);
	goto done;
    }

    /* Get the manpath, either from the 'manpath' command, for from the
       MANPATH envar.  manpath is static, so this should only run once
       for each program invocation.
    */
    if (!manpath) {
	gchar *manp;

	if (!g_spawn_command_line_sync ("manpath", &manp, NULL, NULL, NULL))
	    manp = g_strdup (g_getenv ("MANPATH"));
	if (!manp) {
	    return NULL;
	}
	g_strstrip (manp);
	manpath = g_strsplit (manp, ":", -1);

	g_free (manp);
    }

    /* The URI is either man:frobnicate or man:frobnicate(1).  If the former,
       set man_name to everything after man:.  If the latter, set man_name to
       everything leading to (1), and man_num to the section number.
    */
    if ((cur = strchr (path, '('))) {
	man_name = g_strndup (path, cur - path);
	path = cur + 1;
	if ((cur = strchr (path, ')')))
	    man_num = g_strndup (path, cur - path);
	if (man_num[0]) {
	    man_dir = g_new (gchar, 5);
	    g_snprintf (man_dir, 5, "man%c", man_num[0]);
	}
    } else {
	man_name = g_strdup (path);
    }

    /* Create the glob pattern.  If we have man_num, then look for
       man_name.man_num*.  Otherwise, looks for man_name.*
    */
    if (man_num && man_num[0])
	pattern = g_strdup_printf ("%s.%s*", man_name, man_num);
    else
	pattern = g_strdup_printf ("%s.*", man_name);
    pspec = g_pattern_spec_new (pattern);
    g_free (pattern);

    for (i = 0; manpath[i]; i++) {
	for (langs_i = 0; langs[langs_i]; langs_i++) {
	    for (j = 0; man_dir ? (j < 1) : (mandirs[j] != NULL); j++) {
		if (g_str_equal (langs[langs_i], "C"))
		    dirname = g_build_filename (manpath[i],
						man_dir ? man_dir : mandirs[j],
						NULL);
		else
		    dirname = g_build_filename (manpath[i],
						langs[langs_i],
						man_dir ? man_dir : mandirs[j],
						NULL);
		dir = g_dir_open (dirname, 0, NULL);
		if (dir) {
		    while ((filename = g_dir_read_name (dir))) {
			if (g_pattern_match_string (pspec, filename)) {
			    doc_uri = g_strconcat ("file://",
						   dirname, "/",
						   filename,
						   NULL);
			    g_dir_close (dir);
			    g_free (dirname);
			    goto done;
			}
		    }
		    g_dir_close (dir);
		}
		g_free (dirname);
	    }
	}
    }

 done:
    if (pspec)
	g_pattern_spec_free (pspec);
    g_free (man_dir);
    g_free (man_num);
    g_free (man_name);

    return doc_uri;
}

gchar **
yelp_get_info_paths ( )
{
    /* Get the infopath, either from the INFOPATH envar,
       or from the default infopath_d.
    */
    if (!infopath) {
	gchar *infop;

	infop = g_strdup (g_getenv ("INFOPATH"));
	if (infop) {
	    g_strstrip (infop);
	    infopath = g_strsplit (infop, ":", -1);
	    g_free (infop);
	} else {
	    infopath = infopath_d;
	}
    }


    return infopath;
}


gchar **
yelp_get_man_paths ()
{
	return mandirs;
}

static gchar *
convert_info_uri (gchar   *uri)
{
    gchar *path, *cur;
    gchar *doc_uri  = NULL;
    gchar *info_name = NULL;
    gchar *info_dot_info = NULL;
    gchar **infopaths = NULL;
    gboolean need_subdir = FALSE;
    gchar *test_filename = NULL;

    gint i;

    GDir  *dir;
    const gchar *filename;
    gchar *pattern;
    GPatternSpec *pspec = NULL;
    GPatternSpec *pspec1 = NULL;
    gchar *subdir = NULL;

    if ((path = strchr(uri, ':')))
	path++;
    else
	goto done;

    /* An absolute path after info: */
    if (path[0] == '/') {
	if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
	    doc_uri = g_strconcat ("file://", path, NULL);
	goto done;
    }


    /* The URI is one of the following:
       info:info_name
       info:info_name#node
       info:(info_name)
       info:(info_name)node
       In the first two, spaces are replaced with underscores.  In the other
       two, they're preserved.  That should really only matter for the node
       identifier, which we're not concerned with here.  All we need is to
       extract info_name.
    */
    if (path[0] == '(') {
	path++;
	cur = strchr (path, ')');
	if (!cur)
	    goto done;
	info_name = g_strndup (path, cur - path);
    }
    else if ((cur = strchr (path, '#')))
	info_name = g_strndup (path, cur - path);
    else
	info_name = g_strdup (path);

    if (strstr (info_name, "/")) {
	gchar *tmp = NULL;
	gchar *real_name = NULL;
	tmp = strstr (info_name, "/");
	tmp++;
	real_name = g_strdup (tmp);
	subdir = g_strndup (info_name, (strstr (info_name, "/") - info_name));
	g_free (info_name);
	info_name = g_strdup (real_name);
	g_free (real_name);
	need_subdir = TRUE;
    }


    pattern = g_strdup_printf ("%s.info.*", info_name);
    pspec = g_pattern_spec_new (pattern);
    g_free (pattern);
    pattern = g_strdup_printf ("%s.?z*", info_name);
    pspec1 = g_pattern_spec_new (pattern);
    g_free (pattern);

    info_dot_info = g_strconcat (info_name, ".info", NULL);

    infopaths = yelp_get_info_paths ();

    for (i = 0; infopaths[i]; i++) {
	dir = g_dir_open (infopath[i], 0, NULL);
	if (dir) {
	    while ((filename = g_dir_read_name (dir))) {
		g_free (test_filename);
		test_filename =  g_strconcat (infopath[i], "/", filename, NULL);
		if (need_subdir && g_str_equal (filename, subdir)) {
		    gchar *dirname = NULL;
		    g_dir_close (dir);
		    dirname = g_strconcat (infopath[i], "/", subdir, NULL);
		    dir = g_dir_open (dirname, 0, NULL);
		    g_free (dirname);
		    filename = g_dir_read_name (dir);
		    need_subdir = FALSE;
		}
		else if (g_str_equal (filename,info_name) && 
			 g_file_test (test_filename, G_FILE_TEST_IS_DIR)) {
		    /* In dir, they've specified the subdir but not the
		     * info file name.  Here, do some work to get the name
		     *  ...*/
		    gchar *real_filename;

		    g_dir_close (dir);
		    dir = g_dir_open (test_filename, 0, NULL);

		     while ((real_filename = g_dir_read_name (dir))) {
			 if ((g_str_equal (info_dot_info, real_filename)  ||
			  g_pattern_match_string (pspec, real_filename) ||
			  g_pattern_match_string (pspec1, real_filename) ||
			      g_str_equal (info_name, real_filename))) {
			     doc_uri = g_strconcat ("file://",
						    test_filename, "/",
						    real_filename,
						    NULL);
			     g_dir_close (dir);
			     goto done;
			 }
		     }

		}
		else if (!need_subdir && 
			 (g_str_equal (info_dot_info, filename)  ||
			  g_pattern_match_string (pspec, filename) ||
			  g_pattern_match_string (pspec1, filename) ||
			  g_str_equal (info_name, filename))) {
		    if (subdir) {
			doc_uri = g_strconcat ("file://",
					       infopath[i], "/", subdir, "/",
					       filename,
					       NULL);
		    } else {
			doc_uri = g_strconcat ("file://",
					       infopath[i], "/",
					       filename,
					       NULL);

		    }
		    g_dir_close (dir);
		    goto done;
		}
	    }
	    g_dir_close (dir);
	}
    }

    /* If we got this far and doc_uri is still NULL, we resort to looking
     * for a man page.  Let above handle that.  We just let it know that 
     * somethings gotta be done */
    if (doc_uri == NULL) {
	gchar *tmp;
	tmp = strchr (uri, ':');
	doc_uri = g_strconcat ("man",tmp,NULL);
    }

 done:
    if (pspec)
	g_pattern_spec_free (pspec);
    if (pspec1)
	g_pattern_spec_free (pspec1);
    g_free (test_filename);
    g_free (subdir);
    g_free (info_dot_info);
    g_free (info_name);
    
    return doc_uri;
}
#else /*DON_UTIL*/

#include <rarian-info.h>
#include <rarian-man.h>
#include <rarian.h>


YelpRrnType
resolve_process_ghelp (char *uri, gchar **result)
{
    RrnReg *reg = rrn_find_from_ghelp (&uri[6]);
    YelpRrnType type = YELP_RRN_TYPE_ERROR;

    if (reg) {
	gchar *mime = NULL;
	if (g_str_has_prefix (reg->uri, "file:"))
	    *result = g_strdup (&reg->uri[5]);
	else
	    *result = g_strdup (reg->uri);

	/* mime types are horrible in omf-translated files */
	if (reg->type && *(reg->type))
	    mime = g_strdup (reg->type);
	else 
	    mime = gnome_vfs_get_mime_type (*result);
	
	if (g_str_equal (mime, "text/xml") || 
	    g_str_equal (mime, "application/docbook+xml") || 
	    g_str_equal (mime, "application/xml"))
	    type = YELP_RRN_TYPE_DOC;
	else if (g_str_equal (mime, "text/html") ||
		 g_str_equal (mime, "application/xhtml+xml"))
	    type = YELP_RRN_TYPE_HTML;

    }

    return type;
}

gchar *
resolve_get_section (const gchar *uri)
{
    gchar *sect_delimit;
    gchar *sect;

    sect_delimit = strrchr (uri, '#');
    if (!sect_delimit) {
	sect_delimit = strrchr (uri, '?');
    }
    if (!sect_delimit) {
	return NULL;
    }
    sect = g_strdup (sect_delimit+1);

    return sect;
}

gboolean
resolve_is_man_path (const gchar *path, const gchar *encoding)
{
    gchar **cats;
    gchar **iter;

    cats = rrn_man_get_categories ();

    iter = cats;

    if (encoding && *encoding) {
	while (iter) {
	    gchar *ending = g_strdup_printf ("%s.%s", *iter, encoding);
	    if (g_str_has_suffix (path, ending)) {
		g_free (ending);
		return TRUE;
	    }
	    g_free (ending);
	    iter++;
	}
    } else {
	while (iter) {
	    if (g_str_has_suffix (path, *iter)) {
		return TRUE;
	    }
	    iter++;
	}
    }
    return FALSE;
}

YelpRrnType
resolve_full_file (const gchar *path)
{
    gchar *mime_type;
    YelpRrnType type = YELP_RRN_TYPE_ERROR;

    if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
	return YELP_RRN_TYPE_ERROR;
    }
    mime_type = gnome_vfs_get_mime_type (path);
    if (mime_type == NULL) {
	return YELP_RRN_TYPE_ERROR;
    }

    if (g_str_equal (mime_type, "text/xml") || g_str_equal (mime_type, "application/docbook+xml") || g_str_equal (mime_type, "application/xml"))
	type = YELP_RRN_TYPE_DOC;
    else if (g_str_equal (mime_type, "text/html") ||
	     g_str_equal (mime_type, "application/xhtml+xml"))
	type = YELP_RRN_TYPE_HTML;
    /* No distinction between HTML and XHTML now.  They're handled the same way */
    else if (g_str_equal (mime_type, "application/x-gzip")) {
	if (g_str_has_suffix (path, ".info.gz")) {
	    type = YELP_RRN_TYPE_INFO;
	} else if (resolve_is_man_path (path, "gz")) {
	    type = YELP_RRN_TYPE_MAN;
	}

    } else if (g_str_equal (mime_type, "application/x-bzip")) {
	if (g_str_has_suffix (path, ".info.bz2")) {
	    type = YELP_RRN_TYPE_INFO;
	} else if (resolve_is_man_path (path, "bz2")) {
	    type = YELP_RRN_TYPE_MAN;
	}
    } else if (g_str_equal (mime_type, "text/plain")) {
	if (g_str_has_suffix (path, ".info")) {
	    type = YELP_RRN_TYPE_INFO;
	} else if (resolve_is_man_path (path, NULL)) {
	    type = YELP_RRN_TYPE_MAN;
	}
    } else {
	type = YELP_RRN_TYPE_EXTERNAL;
    }

    g_free (mime_type);
    return type;

}
YelpRrnType
resolve_man_page (const gchar *name, gchar **result, gchar **section)
{
    /* Various ways the path could be presented:
     * filename - full filename after man:
     * name(section) - resolve to a particular section
     * name.section - ditto.  This must be tested twice.  Once for full filename and
     *                once for section
     * name#section - ditto, though this is never used, just added for completeness
     * name - resolve to the first one found
     */
    gchar *lbrace = NULL;
    gchar *rbrace = NULL;
    gchar *sect = NULL;
    gchar *real_name = NULL;
    gboolean repeat = FALSE;
    RrnManEntry *entry = NULL;

    lbrace = strrchr (name, '(');
    if (lbrace) {
	rbrace = strrchr (name, ')');
	if (rbrace) {
	    /*sect = g_strndup (lbrace+1, rbrace - lbrace - 1);*/
	    real_name = g_strndup (name, lbrace - name);
	} else {
	    sect = NULL;
	    real_name = strdup (name);
	}
    } else {
	lbrace = strrchr (name, '.');
	if (lbrace) {
	    repeat = TRUE;
	    /*sect = strdup (lbrace+1);*/
	    real_name = g_strndup (name, lbrace - name);
	} else {
	    lbrace = strrchr (name, '#');
	    if (lbrace) {
		/*sect = strdup (lbrace+1);*/
		real_name = g_strndup (name, lbrace - name);
	    } else {
		real_name = strdup (name);
		sect = NULL;
	    }
	}
    }
    printf ("Checking %s\n", real_name);
    if (g_file_test (real_name, G_FILE_TEST_EXISTS)) {
	/* Full filename */
	printf ("Exists\n");
	*result = g_strdup (real_name);
	return YELP_RRN_TYPE_MAN;
    } else if (g_file_test (name, G_FILE_TEST_EXISTS)) {
	/* Full filename */
	printf ("Exists\n");
	*result = g_strdup (name);
	return YELP_RRN_TYPE_MAN;
    }

    entry = rrn_man_find_from_name (real_name, sect);

    if (entry) {
	*result = strdup (entry->path);
	/**section = strdup (entry->section);*/
	return YELP_RRN_TYPE_MAN;
    } else if (repeat) {
	entry = rrn_man_find_from_name (name, NULL);
	if (entry) {
	    *result = strdup (entry->path);
	    /**section = strdup (entry->section);*/
	    return YELP_RRN_TYPE_MAN;
	}
    }
    return YELP_RRN_TYPE_ERROR;
}


gchar *
resolve_remove_section (const gchar *uri, const gchar *sect)
{
    if (sect && *sect)
	return (g_strndup (uri, (strlen(uri) - strlen(sect) - 1 /*for the delimiter char */)));
    else
	return (g_strdup (uri));
}

YelpRrnType
yelp_uri_resolve (gchar *uri, gchar **result, gchar **section)
{
    YelpRrnType ret = YELP_RRN_TYPE_ERROR;
    gchar *intern_section = NULL;
    gchar *intern_uri = NULL;
    g_assert (result != NULL);
    if (*result != NULL) {
	g_warning ("result is not empty: %s.", *result);
	return ret;
    }
    g_assert (section != NULL);
    if (*section != NULL) {
	g_warning ("section is not empty: %s.", *section);
	return ret;
    }
    if (uri == NULL) {
	g_warning ("URI is NULL");
	return ret;
    }
    intern_section = resolve_get_section(uri);
    intern_uri = resolve_remove_section (uri, intern_section);

    if (!strncmp (uri, "ghelp:", 6) || !strncmp (uri, "gnome-help:", 11)) {
	ret = resolve_process_ghelp (intern_uri, result);
	if (*result) {
	    *section = intern_section;
	}
    } else if (!strncmp (uri, "man:", 4)) {
	ret = resolve_man_page (&uri[4], result, section);
	if (ret == YELP_RRN_TYPE_ERROR) {
	    *result = NULL;
	    *section = NULL;
	}
	/* Man page */
    } else if (!strncmp (uri, "info:", 5)) {
	/* info page */

	gchar *info_name = intern_uri;
	gchar *info_sect = intern_section;
	gboolean free_stuff = FALSE;
	RrnInfoEntry *entry = NULL;

	if (g_str_equal (&uri[5], "dir")) {
	    *section = g_strdup ("info");
	    *result = NULL;
	    ret = YELP_RRN_TYPE_TOC;
	    return ret;
	}
	
	if (!intern_section) {
	    gchar *lbrace = NULL;
	    gchar *rbrace = NULL;
	    lbrace = strchr (info_name, '(');
	    rbrace = strchr (info_name, ')');
	    if (lbrace && rbrace) {
		info_name = g_strndup (lbrace+1, (rbrace-lbrace-1));
		info_sect = g_strdup (rbrace+1);
		free_stuff = TRUE;
	    } else {
		info_name += 5;
	    }
	} else {
	    info_name += 5;
	}

	entry = rrn_info_find_from_uri (info_name, info_sect);
	if (entry) {
	    ret = YELP_RRN_TYPE_INFO;
	    if (entry->section)
		*section = g_strdup (entry->section);
	    else
		*section = g_strdup (intern_section);
	    *result = g_strdup (entry->base_filename);
	} else {
	    ret = resolve_man_page (&uri[5], result, section);
	    if (!ret) {
		ret = YELP_RRN_TYPE_ERROR;
		*section = NULL;
		*result = NULL;
	    }
	}
	if (free_stuff) {
	    g_free (info_name);
	    g_free (info_sect);
	}
    } else if (!strncmp (uri, "file:", 5)) {
	int file_cut = 5;
	while (uri[file_cut+1] == '/')
	    file_cut++;
	ret = resolve_full_file (&intern_uri[file_cut]);
	if (ret == YELP_RRN_TYPE_EXTERNAL) {
	    *section = NULL;
	    *result = g_strdup (uri);
	}
	else if (ret == YELP_RRN_TYPE_ERROR) {
	    *section = NULL;
	    *result = NULL;
	} else {
	    *result = g_strdup (&intern_uri[file_cut]);
	    *section = intern_section;
	}
	/* full file path.  Ensure file exists and determine type */
    } else if (!strncmp (uri, "x-yelp-toc:", 11)) {
	ret = YELP_RRN_TYPE_TOC;
	if (strlen (uri) > 11) {
	    *section = g_strdup (&uri[11]);
	} else {
	    *section = g_strdup("index");
	}
	*result = g_strdup ("x-yelp-toc:");
	/* TOC page */
    } else if (!strncmp (uri, "x-yelp-search:", 14)) {
	/* Search pager request.  *result contains the search terms */
	*result = g_strdup (uri);
	*section = g_strdup (uri+14);
	ret = YELP_RRN_TYPE_SEARCH;
    } else if (g_file_test (intern_uri, G_FILE_TEST_EXISTS)) {
	/* Full path */
	ret = resolve_full_file (intern_uri);
	if (ret == YELP_RRN_TYPE_EXTERNAL) {
	    *section = NULL;
	    *result = g_strdup (uri);
	}
	else if (ret == YELP_RRN_TYPE_ERROR) {
	    *section = NULL;
	    *result = NULL;
	} else {
	    *result = g_strdup (intern_uri);
	    *section = g_strdup (intern_section);
	}
    } else if (*uri == '/' || g_str_has_suffix (uri, ".xml")) {
	/* Quite probable it was supposed to be ours, but
	 * the file doesn't exist.  Hence, we should bin it
	 */
	ret = YELP_RRN_TYPE_ERROR;
	*result = NULL;
	*section = NULL;
    } else {
	/* We really don't care what it is.  It's not ours.  Let
	 * someone else handle it 
	 */
	ret = YELP_RRN_TYPE_EXTERNAL;
	*result = g_strdup (uri);
	*section = NULL;
    }

    return ret;
}
#endif /*DON_UTIL*/
