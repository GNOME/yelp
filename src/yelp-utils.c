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

#include <string.h>
#include <rarian-info.h>
#include <rarian-man.h>
#include <rarian.h>

#include "yelp-utils.h"
#include "yelp-debug.h"


YelpRrnType    resolve_process_ghelp      (char         *uri, 
					   gchar       **result);
gchar *        resolve_get_section        (const gchar  *uri);
gboolean       resolve_is_man_path        (const gchar  *path, 
					   const gchar  *encoding);
YelpRrnType    resolve_full_file          (const gchar  *path);
YelpRrnType    resolve_man_page           (const gchar  *name, 
					   gchar       **result, 
					   gchar       **section);
gchar *        resolve_remove_section     (const gchar  *uri, 
					   const gchar  *sect);
YelpRrnType    yelp_uri_resolve           (gchar        *uri, 
					   gchar       **result, 
					   gchar       **section);

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
	
	if (!mime) {
	    type = YELP_RRN_TYPE_ERROR;
	} else if (g_str_equal (mime, "text/xml") || 
	    g_str_equal (mime, "application/docbook+xml") || 
	    g_str_equal (mime, "application/xml"))
	    type = YELP_RRN_TYPE_DOC;
	else if (g_str_equal (mime, "text/html"))
	    type = YELP_RRN_TYPE_HTML;
	else if (g_str_equal (mime, "application/xhtml+xml"))
	    type = YELP_RRN_TYPE_XHTML;

    } else {
	gint file_cut = 6;
	/* If a full file path after ghelp:, see if the file
	 * exists and return type if it does 
	 */
	while (uri[file_cut] == '/' && uri[file_cut+1] == '/')
	    file_cut++;
	type = resolve_full_file (&uri[file_cut]);
	*result = g_strdup (&uri[file_cut]);
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
    else if (g_str_equal (mime_type, "text/html"))
	type = YELP_RRN_TYPE_HTML;
    else if (g_str_equal (mime_type, "application/xhtml+xml"))
	type = YELP_RRN_TYPE_XHTML;
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
    if (g_file_test (real_name, G_FILE_TEST_EXISTS)) {
	/* Full filename */
	*result = g_strdup (real_name);
	return YELP_RRN_TYPE_MAN;
    } else if (g_file_test (name, G_FILE_TEST_EXISTS)) {
	/* Full filename */
	*result = g_strdup (name);
	return YELP_RRN_TYPE_MAN;
    }

    entry = rrn_man_find_from_name (real_name, sect);

    if (entry) {
	*result = strdup (entry->path);
	/**section = strdup (entry->section);*/
	return YELP_RRN_TYPE_MAN;
    } else if (repeat) {
	entry = rrn_man_find_from_name ((char *) name, NULL);
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
    if (sect)
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

    if (intern_section && g_str_equal (intern_section, "")) {
	intern_section = NULL;
    }

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
	    gint frag = 11;
	    while (uri[frag] == '#' || uri[frag] == '?')
		frag++;
	    *section = g_strdup (&uri[frag]);
	} else {
	    *section = g_strdup("index");
	}
	*result = g_strdup ("x-yelp-toc:");
	/* TOC page */
    } else if (!strncmp (uri, "x-yelp-search:", 14)) {
	/* Search pager request.  *result contains the search terms */
	*result = g_strdup (uri);
	*section = g_strdup ("results");
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
