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
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-program.h>

#include "yelp-utils.h"
#include "yelp-uri.h"

#include <string.h>

#define d(x)

GHashTable *document_info_table;
GMutex     *document_info_mutex;

static gchar *           convert_ghelp_uri    (gchar   *uri);
static gchar *           convert_man_uri      (gchar   *uri);
static gchar *           convert_info_uri     (gchar   *uri);
static YelpDocumentType  get_document_type    (gchar   *uri);

void
yelp_utils_init (void)
{
    document_info_mutex = g_mutex_new ();
    document_info_table =
	g_hash_table_new_full (g_str_hash,
			       g_str_equal,
			       g_free,
			       (GDestroyNotify) yelp_document_info_free);
}

YelpDocumentInfo *
yelp_document_info_new (gchar *uri)
{
    YelpDocumentInfo *doc;
    gchar            *doc_uri = NULL;
    YelpDocumentType  doc_type;
    gchar *cur;

    g_return_val_if_fail (uri != NULL, NULL);

    if (!strncmp (uri, "file:", 5)) {
	if ((cur = strchr (uri, '#')))
	    doc_uri = g_strndup (uri, cur - uri);
	else
	    doc_uri = g_strdup (uri);
	doc_type = get_document_type (doc_uri);
    }
    if (!strncmp (uri, "ghelp:", 6)) {
	doc_uri  = convert_ghelp_uri (uri);
	if (doc_uri)
	    doc_type = get_document_type (doc_uri);
    }
    else if (!strncmp (uri, "man:", 4)) {
	doc_uri  = convert_man_uri (uri);
	doc_type = YELP_TYPE_MAN;
    }
    else if (!strncmp (uri, "info:", 5)) {
	doc_uri  = convert_info_uri (uri);
	doc_type = YELP_TYPE_INFO;
    }
    else if (!strncmp (uri, "x-yelp-toc:", 11)) {
    }

    if (doc_uri) {
	doc = g_new0 (YelpDocumentInfo, 1);
	doc->uri  = doc_uri;
	doc->type = doc_type;
	return doc;
    } else {
	return NULL;
    }
}

YelpDocumentInfo *
yelp_document_info_get (gchar *uri)
{
    YelpDocumentInfo *doc;

    g_mutex_lock (document_info_mutex);

    doc = (YelpDocumentInfo *) g_hash_table_lookup (document_info_table, uri);

    if (!doc) {
	doc = yelp_document_info_new (uri);
	if (doc && doc->type != YELP_TYPE_EXTERNAL) {
	    YelpDocumentInfo *old_doc;

	    if ((old_doc = g_hash_table_lookup (document_info_table, doc->uri))) {
		yelp_document_info_free (doc);
		doc = old_doc;
		g_hash_table_insert (document_info_table,
				     g_strdup (uri),
				     doc);
	    } else {
		g_hash_table_insert (document_info_table,
				     g_strdup (uri),
				     doc);
		if (!g_str_equal (uri, doc->uri))
		    g_hash_table_insert (document_info_table,
					 g_strdup (doc->uri),
					 doc);
	    }
	}
    }

    g_mutex_unlock (document_info_mutex);
    return doc;
}

void
yelp_document_info_free (YelpDocumentInfo *doc)
{
    if (!doc)
	return;

    g_free (doc->uri);
    g_free (doc);
}

void
yelp_document_page_free (YelpDocumentPage *page)
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

static YelpDocumentType
get_document_type (gchar *uri)
{
    gchar *mime_type;
    YelpDocumentType type;

    g_return_val_if_fail (uri != NULL, YELP_TYPE_ERROR);

    if (strncmp (uri, "file:", 5))
	return YELP_TYPE_EXTERNAL;

    mime_type = gnome_vfs_get_mime_type (uri);
    g_return_val_if_fail (mime_type != NULL, YELP_TYPE_ERROR);

    if (g_str_equal (mime_type, "text/xml"))
	type = YELP_URI_TYPE_DOCBOOK_XML;
    else if (g_str_equal (mime_type, "text/sgml"))
	type = YELP_URI_TYPE_DOCBOOK_SGML;
    else if (g_str_equal (mime_type, "text/html"))
	type = YELP_URI_TYPE_HTML;
    else
	type = YELP_URI_TYPE_EXTERNAL;

    g_free (mime_type);
    return type;
}

/******************************************************************************/
/** Convert fancy URIs to file URIs *******************************************/

static gchar *
locate_file_lang (gchar *path, gchar *file, gchar *lang)
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
	const GList *langs;
	gchar *location = (gchar *) node->data;

	langs = gnome_i18n_get_language_list ("LC_MESSAGES");

	for (; langs != NULL; langs = langs->next) {
	    gchar *lang = langs->data;

	    /* This has to be a valid language AND a language with
	     * no encoding postfix.  The language will come up without
	     * encoding next */
	    if (lang == NULL || strchr (lang, '.') != NULL)
		continue;

	    doc_uri = locate_file_lang (location, doc_name, lang);

	    if (!doc_uri)
		doc_uri = locate_file_lang (location, "index", lang);

	    if (doc_uri)
		goto done;
	}

	/* Look in C locale since that exists for almost all documents */
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

    static gchar **manpath = NULL;
    static gchar *mandirs[] =
	{"man1", "man2", "man3", "man4", "man5", "man6",
	 "man7", "man8", "man9", "mann", NULL};
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
	/* The man_dir/mandirs thing is probably cleverer than it should be. */
	for (j = 0; man_dir ? (j < 1) : (mandirs[j] != NULL); j++) {
	    dirname = g_build_filename (manpath[i],
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
