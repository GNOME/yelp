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
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnome/gnome-program.h>

#include "yelp-utils.h"

#include <string.h>

#ifdef YELP_DEBUG
#define d(x) x
#else
#define d(x)
#endif

GHashTable *doc_info_table;

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

static YelpDocType  get_doc_type       (gchar   *uri);
static gchar *      convert_ghelp_uri  (gchar   *uri);

static gchar *      convert_man_uri    (gchar   *uri);
static gchar *      convert_info_uri   (gchar   *uri);

YelpDocInfo *
yelp_doc_info_new (const gchar *uri)
{
    YelpDocInfo *doc;
    gchar       *doc_uri  = NULL;
    gchar       *full_uri = NULL;
    YelpDocType  doc_type = YELP_DOC_TYPE_ERROR;
    YelpURIType  uri_type;
    gchar *cur;

    g_return_val_if_fail (uri != NULL, NULL);

    d (g_print ("yelp_doc_info_new\n"));
    d (g_print ("  uri      = \"%s\"\n", uri));

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
	doc_uri  = convert_man_uri (full_uri);
	doc_type = YELP_DOC_TYPE_MAN;
	uri_type = YELP_URI_TYPE_MAN;
    }
    else if (g_str_has_prefix (full_uri, "info:")) {
	doc_uri  = convert_info_uri (full_uri);
	doc_type = YELP_DOC_TYPE_INFO;
	uri_type = YELP_URI_TYPE_INFO;
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

    d (g_print ("  full_uri = \"%s\"\n", full_uri));
    d (g_print ("  doc_uri  = \"%s\"\n", doc_uri));

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
yelp_doc_info_get (const gchar *uri)
{
    YelpDocInfo *doc;
    gint i;
    gchar *c, *doc_uri;

    g_return_val_if_fail (uri != NULL, NULL);

    d (g_print ("yelp_doc_info_get\n"));
    d (g_print ("  uri     = \"%s\"\n", uri));

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
	doc = yelp_doc_info_new (doc_uri);
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

    d (g_print ("yelp_doc_info_free\n"));
    d (g_print ("  uri = \"%s\"\n", doc->uris->uri));

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

void
yelp_doc_page_free (YelpDocPage *page)
{
    if (!page)
	return;

    g_free (page->page_id);
    g_free (page->title);
    g_free (page->contents);

    g_free (page->prev_id);
    g_free (page->next_id);
    g_free (page->toc_id);

    g_free (page);
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

    if (g_str_equal (mime_type, "text/xml"))
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

    d (g_print ("yelp_doc_add_uri\n"));

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

    d (g_print ("  uri      = \"%s\"\n", uri));
    d (g_print ("  num_uris = %i\n", doc_info->num_uris));
    d (g_print ("  max_uris = %i\n", doc_info->max_uris));
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
convert_man_uri (gchar *uri)
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
	if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
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

static gchar *
convert_info_uri (gchar   *uri)
{
    gchar *path, *cur;
    gchar *doc_uri  = NULL;
    gchar *info_name = NULL;
    gchar *info_dot_info = NULL;

    static gchar **infopath = NULL;
    static gchar *infopath_d[] = {"/usr/info", "/usr/share/info", NULL};

    gint i;

    GDir  *dir;
    const gchar *filename;
    gchar *pattern;
    GPatternSpec *pspec = NULL;

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

    pattern = g_strdup_printf ("%s.info.*", info_name);
    pspec = g_pattern_spec_new (pattern);
    g_free (pattern);

    info_dot_info = g_strconcat (info_name, ".info", NULL);

    for (i = 0; infopath[i]; i++) {
	dir = g_dir_open (infopath[i], 0, NULL);
	if (dir) {
	    while ((filename = g_dir_read_name (dir))) {
		if (g_str_equal (info_dot_info, filename)  ||
		    g_pattern_match_string (pspec, filename) ) {
		    doc_uri = g_strconcat ("file://",
					   infopath[i], "/",
					   filename,
					   NULL);
		    g_dir_close (dir);
		    goto done;
		}
	    }
	    g_dir_close (dir);
	}
    }

 done:
    if (pspec)
	g_pattern_spec_free (pspec);
    g_free (info_dot_info);
    g_free (info_name);
    return doc_uri;
}
