/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009 Shaun McCance  <shaunm@gnome.org>
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

#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include "yelp-uri.h"
#include "yelp-debug.h"

static void           yelp_uri_class_init   (YelpUriClass   *klass);
static void           yelp_uri_init         (YelpUri        *uri);
static void           yelp_uri_dispose      (GObject        *object);
static void           yelp_uri_finalize     (GObject        *object);

static void           resolve_file_uri      (YelpUri        *ret,
                                             const gchar    *arg);
static void           resolve_file_path     (YelpUri        *ret,
                                             YelpUri        *base,
                                             const gchar    *arg);
static void           resolve_data_dirs     (YelpUri        *ret,
                                             const gchar   **subdirs,
                                             const gchar    *docid,
                                             const gchar    *pageid);
static void           resolve_ghelp_uri     (YelpUri        *ret,
                                             const gchar    *arg);
static void           resolve_man_uri       (YelpUri        *ret,
                                             const gchar    *arg);
static void           resolve_info_uri      (YelpUri        *ret,
                                             const gchar    *arg);
static void           resolve_page_and_frag (YelpUri        *ret,
                                             const gchar    *arg);
static void           resolve_common        (YelpUri        *ret);
static gboolean       is_man_path           (const gchar    *uri,
                                             const gchar    *encoding);

G_DEFINE_TYPE (YelpUri, yelp_uri, G_TYPE_OBJECT);
#define GET_PRIV(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_URI, YelpUriPrivate))

typedef struct _YelpUriPrivate YelpUriPrivate;
struct _YelpUriPrivate {
    YelpUriDocumentType   doctype;
    GFile                *gfile;
    gchar               **search_path;
    gchar                *page_id;
    gchar                *frag_id;
};

/******************************************************************************/

static const gchar *mancats[] = {
    "0p",
    "1", "1p", "1g", "1t", "1x", "1ssl", "1m",
    "2",
    "3", "3o", "3t", "3p", "3blt", "3nas", "3form", "3menu", "3tiff", "3ssl", "3readline",
    "3ncurses", "3curses", "3f", "3pm", "3perl", "3qt", "3x", "3X11",
    "4", "4x",
    "5", "5snmp", "5x", "5ssl",
    "6", "6x",
    "7", "7gcc", "7x", "7ssl",
    "8", "8l", "9", "0p",
    NULL
};

/******************************************************************************/

static void
yelp_uri_class_init (YelpUriClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose  = yelp_uri_dispose;
    object_class->finalize = yelp_uri_finalize;

    g_type_class_add_private (klass, sizeof (YelpUriPrivate));
}

static void
yelp_uri_init (YelpUri *uri)
{
    return;
}

static void
yelp_uri_dispose (GObject *object)
{
    YelpUriPrivate *priv = GET_PRIV (object);

    if (priv->gfile) {
        g_object_unref (priv->gfile);
        priv->gfile = NULL;
    }

    G_OBJECT_CLASS (yelp_uri_parent_class)->dispose (object);
}

static void
yelp_uri_finalize (GObject *object)
{
    YelpUriPrivate *priv = GET_PRIV (object);

    g_strfreev (priv->search_path);
    g_free (priv->page_id);
    g_free (priv->frag_id);

    G_OBJECT_CLASS (yelp_uri_parent_class)->finalize (object);
}

/******************************************************************************/

YelpUri *
yelp_uri_resolve (const gchar *arg)
{
    return yelp_uri_resolve_relative (NULL, arg);
}

YelpUri *
yelp_uri_resolve_relative (YelpUri *base, const gchar *arg)
{
    YelpUri *ret;
    YelpUriPrivate *priv;

    ret = (YelpUri *) g_object_new (YELP_TYPE_URI, NULL);
    priv = GET_PRIV (ret);
    priv->doctype = YELP_URI_DOCUMENT_TYPE_UNKNOWN;

    if (g_str_has_prefix (arg, "ghelp:") || g_str_has_prefix (arg, "gnome-help:")) {
        resolve_ghelp_uri (ret, arg);
    }
    else if (g_str_has_prefix (arg, "file:")) {
        resolve_file_uri (ret, arg);
    }
    else if (g_str_has_prefix (arg, "man:")) {
        resolve_man_uri (ret, arg);
    }
    else if (g_str_has_prefix (arg, "info:")) {
        resolve_info_uri (ret, arg);
    }
    else if (strchr (arg, ':')) {
        priv->doctype = YELP_URI_DOCUMENT_TYPE_EXTERNAL;
        priv->gfile = g_file_new_for_uri (arg);
        TRUE;
    }
    else {
        resolve_file_path (ret, base, arg);
    }

    return ret;
}

YelpUriDocumentType
yelp_uri_get_document_type (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    return priv->doctype;
}

gchar *
yelp_uri_get_base_uri (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    return priv->gfile ? g_file_get_uri (priv->gfile) : NULL;
}

gchar **
yelp_uri_get_search_path (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    return g_strdupv (priv->search_path);
}

gchar *
yelp_uri_get_page_id (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    return g_strdup (priv->page_id);
}

gchar *
yelp_uri_get_frag_id (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    return g_strdup (priv->frag_id);
}

/******************************************************************************/

static void
resolve_file_uri (YelpUri *ret, const gchar *arg)
{
    YelpUriPrivate *priv = GET_PRIV (ret);
    gchar *uri;
    const gchar *hash = strchr (arg, '#');

    if (hash)
        uri = g_strndup (arg, hash - arg);
    else
        uri = (gchar *) arg;

    priv->gfile = g_file_new_for_uri (uri);

    if (hash) {
        resolve_page_and_frag (ret, hash + 1);
        g_free (uri);
    }

    resolve_common (ret);
}

static void
resolve_file_path (YelpUri *ret, YelpUri *base, const gchar *arg)
{
    YelpUriPrivate *base_priv = GET_PRIV (base);
    YelpUriPrivate *priv = GET_PRIV (ret);
    gchar *path;
    const gchar *hash = strchr (arg, '#');

    if (hash)
        path = g_strndup (arg, hash - arg);
    else
        path = (gchar *) arg;

    if (arg[0] == '/') {
        priv->gfile = g_file_new_for_path (path);
    }
    else if (base && base_priv->gfile) {
        priv->gfile = g_file_resolve_relative_path (base_priv->gfile, path);
    }
    else {
        gchar *cur;
        GFile *curfile;
        cur = g_get_current_dir ();
        curfile = g_file_new_for_path (cur);
        priv->gfile = g_file_resolve_relative_path (curfile, path);
        g_object_unref (curfile);
        g_free (cur);
    }

    if (hash) {
        resolve_page_and_frag (ret, hash + 1);
        g_free (path);
    }

    resolve_common (ret);
}

static void
resolve_data_dirs (YelpUri      *ret,
                   const gchar **subdirs,
                   const gchar  *docid,
                   const gchar  *pageid)
{
    YelpUriPrivate *priv = GET_PRIV (ret);
    const gchar * const *datadirs = g_get_system_data_dirs ();
    const gchar * const *langs = g_get_language_names ();
    gchar *filename = NULL;
    gchar **searchpath = NULL;
    gint searchi, searchmax;
    gint datadir_i, subdir_i, lang_i;
    YelpUriDocumentType type = YELP_URI_DOCUMENT_TYPE_UNKNOWN;

    searchi = 0;
    searchmax = 10;
    searchpath = g_new0 (gchar *, 10);

    for (datadir_i = 0; datadirs[datadir_i]; datadir_i++) {
        for (subdir_i = 0; subdirs[subdir_i]; subdir_i++) {
            for (lang_i = 0; langs[lang_i]; lang_i++) {
                gchar *helpdir = g_strdup_printf ("%s%s/%s/%s",
                                                  datadirs[datadir_i], subdirs[subdir_i], docid, langs[lang_i]);
                if (!g_file_test (helpdir, G_FILE_TEST_IS_DIR)) {
                    g_free (helpdir);
                    continue;
                }

                if (searchi + 1 >= searchmax) {
                    searchmax += 5;
                    searchpath = g_renew (gchar *, searchpath, searchmax);
                }
                searchpath[searchi] = helpdir;
                searchpath[++searchi] = NULL;

                if (type != YELP_URI_DOCUMENT_TYPE_UNKNOWN)
                    /* We've already found it.  We're just adding to the search path now. */
                    continue;

                filename = g_strdup_printf ("%s/index.page", helpdir);
                if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
                    type = YELP_URI_DOCUMENT_TYPE_MALLARD;
                    g_free (filename);
                    filename = g_strdup (helpdir);
                    continue;
                }
                g_free (filename);

                filename = g_strdup_printf ("%s/%s.xml", helpdir, pageid);
                if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
                    type = YELP_URI_DOCUMENT_TYPE_DOCBOOK;
                    continue;
                }
                g_free (filename);

                filename = g_strdup_printf ("%s/%s.html", helpdir, pageid);
                if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
                    type = YELP_URI_DOCUMENT_TYPE_HTML;
                    continue;
                }
                g_free (filename);
            } /* end for langs */
        } /* end for subdirs */
    } /* end for datadirs */

    if (type == YELP_URI_DOCUMENT_TYPE_UNKNOWN) {
        g_strfreev (searchpath);
        priv->doctype = YELP_URI_DOCUMENT_TYPE_NOT_FOUND;
    }
    else {
        priv->doctype = type;
        priv->gfile = g_file_new_for_path (filename);
        priv->search_path = searchpath;
    }
}

static void
resolve_ghelp_uri (YelpUri *ret, const gchar *arg)
{
    /* ghelp:/path/to/file
     * ghelp:document
     */
    YelpUriPrivate *priv = GET_PRIV (ret);
    const gchar const *helpdirs[3] = {"help", "gnome/help", NULL};
    gchar *docid = NULL;
    gchar *colon, *hash, *slash, *pageid; /* do not free */

    colon = strchr (arg, ':');
    if (!colon) {
        priv->doctype = YELP_URI_DOCUMENT_TYPE_ERROR;
        return;
    }

    colon++;
    if (*colon == '/') {
        gchar *newuri;
        newuri = g_strdup_printf ("file:%s", colon);
        resolve_file_uri (ret, newuri);
        g_free (newuri);
        return;
    }

    hash = strchr (colon, '?');
    if (!hash)
        hash = strchr (colon, '#');
    if (hash) {
        resolve_page_and_frag (ret, hash + 1);
        docid = g_strndup (colon, hash - colon);
    }
    else {
        docid = g_strdup (colon);
    }

    slash = strchr (docid, '/');
    if (slash) {
        *slash = '\0';
        pageid = slash + 1;
    }
    else {
        pageid = docid;
    }

    resolve_data_dirs (ret, helpdirs, docid, pageid);

    /* Specifying pages and anchors for Mallard documents with ghelp URIs is sort
       hacked on.  This is a touch inconsistent, but it maintains compatibility
       with the way things worked in 2.28, and ghelp URIs should be entering
       compatibility-only mode.
     */
    if (priv->doctype == YELP_URI_DOCUMENT_TYPE_MALLARD && pageid != docid) {
        if (priv->page_id)
            g_free (priv->page_id);
        priv->page_id = g_strdup (pageid);
    }

    g_free (docid);
}

static void
resolve_man_uri (YelpUri *ret, const gchar *arg)
{
    YelpUriPrivate *priv = GET_PRIV (ret);
    /* man:/path/to/file
     * man:name(section)
     * man:name.section
     * man:name
     */
    static gchar **manpath = NULL;
    const gchar * const * langs = g_get_language_names ();
    gchar *newarg = NULL;
    gchar *section = NULL;
    gchar *name = NULL;
    gchar *fullpath = NULL;

    /* not to be freed */
    gchar *colon, *hash;
    gchar *lbrace = NULL;
    gchar *rbrace = NULL;
    gint i, j, k;

    if (g_str_has_prefix (arg, "man:/")) {
        gchar *newuri;
        priv->doctype = YELP_URI_DOCUMENT_TYPE_MAN;
        newuri = g_strdup_printf ("file:%s", arg + 4);
        resolve_file_uri (ret, newuri);
        g_free (newuri);
        return;
    }

    if (!manpath) {
        /* Initialize manpath only once */
        const gchar *env = g_getenv ("MANPATH");
        if (!env || env[0] == '\0')
            env = "/usr/share/man:/usr/man:/usr/local/share/man:/usr/local/man";
        manpath = g_strsplit (env, ":", 0);
    }

    colon = strchr (arg, ':');
    if (colon)
        colon++;
    else
        colon = (gchar *) arg;

    hash = strchr (colon, '#');
    if (hash)
        newarg = g_strndup (colon, hash - colon);
    else
        newarg = g_strdup (colon);

    lbrace = strrchr (newarg, '(');
    if (lbrace) {
        rbrace = strrchr (lbrace, ')');
        if (rbrace) {
            section = g_strndup (lbrace + 1, rbrace - lbrace - 1);
            name = g_strndup (newarg, lbrace - newarg);
        } else {
            section = NULL;
            name = strdup (newarg);
        }
    } else {
        lbrace = strrchr (newarg, '.');
        if (lbrace) {
            section = strdup (lbrace + 1);
            name = g_strndup (newarg, lbrace - newarg);
        } else {
            section = NULL;
            name = strdup (newarg);
        }
    }

    for (i = 0; langs[i]; i++) {
        for (j = 0; manpath[j]; j++) {
            gchar *langdir;
            if (g_str_equal (langs[i], "C"))
                langdir = g_strdup (manpath[j]);
            else
                langdir = g_strconcat (manpath[j], "/", langs[i], NULL);
            if (!g_file_test (langdir, G_FILE_TEST_IS_DIR)) {
                g_free (langdir);
                continue;
            }

            for (k = 0; section ? k == 0 : mancats[k] != NULL; k++) {
                gchar *sectiondir;
                if (section)
                    sectiondir = g_strconcat ("man", section, NULL);
                else
                    sectiondir = g_strconcat ("man", mancats[k], NULL);

                fullpath = g_strconcat (langdir, "/", sectiondir,
                                        "/", name, ".", sectiondir + 3,
                                        NULL);
                if (g_file_test (fullpath, G_FILE_TEST_IS_REGULAR))
                    goto gotit;
                g_free (fullpath);

                fullpath = g_strconcat (langdir, "/", sectiondir,
                                        "/", name, ".", sectiondir + 3, ".gz",
                                        NULL);
                if (g_file_test (fullpath, G_FILE_TEST_IS_REGULAR))
                    goto gotit;
                g_free (fullpath);

                fullpath = g_strconcat (langdir, "/", sectiondir,
                                        "/", name, ".", sectiondir + 3, ".bz2",
                                        NULL);
                if (g_file_test (fullpath, G_FILE_TEST_IS_REGULAR))
                    goto gotit;
                g_free (fullpath);

                fullpath = g_strconcat (langdir, "/", sectiondir,
                                        "/", name, ".", sectiondir + 3, ".lzma",
                                        NULL);
                if (g_file_test (fullpath, G_FILE_TEST_IS_REGULAR))
                    goto gotit;
                g_free (fullpath);

                fullpath = NULL;
            gotit:
                g_free (sectiondir);
                if (fullpath)
                    break;
            }
            g_free (langdir);
            if (fullpath)
                break;
        }
        if (fullpath)
            break;
    }

    if (fullpath) {
        priv->doctype = YELP_URI_DOCUMENT_TYPE_MAN;
        priv->gfile = g_file_new_for_path (fullpath);
        resolve_common (ret);
        g_free (fullpath);
    } else {
        priv->doctype = YELP_URI_DOCUMENT_TYPE_NOT_FOUND;
    }

    if (hash)
        resolve_page_and_frag (ret, hash + 1);

    g_free (newarg);
}

static void
resolve_info_uri (YelpUri *ret, const gchar *arg)
{
    /* FIXME */
}

static void
resolve_page_and_frag (YelpUri *ret, const gchar *arg)
{
    YelpUriPrivate *priv = GET_PRIV (ret);
    gchar *hash;

    if (!arg || arg[0] == '\0')
        return;

    hash = strchr (arg, '#');
    if (hash) {
        priv->page_id = g_strndup (arg, hash - arg);
        priv->frag_id = g_strdup (hash + 1);
    } else {
        priv->page_id = g_strdup (arg);
        priv->frag_id = g_strdup (arg);
    }
    return;
}

static void
resolve_common (YelpUri *ret)
{
    YelpUriPrivate *priv = GET_PRIV (ret);
    GFileInfo *info;
    GError *error = NULL;

    info = g_file_query_info (priv->gfile,
                              G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                              G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
                              G_FILE_QUERY_INFO_NONE,
                              NULL, &error);
    if (error) {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
            priv->doctype = YELP_URI_DOCUMENT_TYPE_NOT_FOUND;
        else
            priv->doctype = YELP_URI_DOCUMENT_TYPE_ERROR;
        g_error_free (error);
        return;
    }

    if (priv->search_path == NULL) {
        if (g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE) ==
            G_FILE_TYPE_DIRECTORY) {
            priv->search_path = g_new0 (gchar *, 2);
            priv->search_path[0] = g_file_get_path (priv->gfile);
        } else {
            GFile *parent = g_file_get_parent (priv->gfile);
            priv->search_path = g_new0 (gchar *, 2);
            priv->search_path[0] = g_file_get_path (parent);
            g_object_unref (parent);
        }
    }

    if (priv->doctype == YELP_URI_DOCUMENT_TYPE_UNKNOWN) {
        if (g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE) ==
            G_FILE_TYPE_DIRECTORY) {
            priv->doctype = YELP_URI_DOCUMENT_TYPE_MALLARD;
        }
        else {
            gchar *basename;
            const gchar *mime_type = g_file_info_get_attribute_string (info,
                                                                       G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
            basename = g_file_get_basename (priv->gfile);
            if (g_str_equal (mime_type, "text/xml") ||
                g_str_equal (mime_type, "application/docbook+xml") ||
                g_str_equal (mime_type, "application/xml")) {
                priv->doctype = YELP_URI_DOCUMENT_TYPE_DOCBOOK;
            }
            else if (g_str_equal (mime_type, "text/html")) {
                priv->doctype = YELP_URI_DOCUMENT_TYPE_HTML;
            }
            else if (g_str_equal (mime_type, "application/xhtml+xml")) {
                priv->doctype = YELP_URI_DOCUMENT_TYPE_XHTML;
            }
            else if (g_str_equal (mime_type, "application/x-gzip")) {
                if (g_str_has_suffix (basename, ".info.gz"))
                    priv->doctype = YELP_URI_DOCUMENT_TYPE_INFO;
                else if (is_man_path (basename, "gz"))
                    priv->doctype = YELP_URI_DOCUMENT_TYPE_MAN;
            }
            else if (g_str_equal (mime_type, "application/x-bzip")) {
                if (g_str_has_suffix (basename, ".info.bz2"))
                    priv->doctype = YELP_URI_DOCUMENT_TYPE_INFO;
                else if (is_man_path (basename, "bz2"))
                    priv->doctype = YELP_URI_DOCUMENT_TYPE_MAN;
            }
            else if (g_str_equal (mime_type, "application/x-lzma")) {
                if (g_str_has_suffix (basename, ".info.lzma"))
                    priv->doctype = YELP_URI_DOCUMENT_TYPE_INFO;
                else if (is_man_path (basename, "lzma"))
                    priv->doctype = YELP_URI_DOCUMENT_TYPE_MAN;
            }
            else if (g_str_equal (mime_type, "application/octet-stream")) {
                if (g_str_has_suffix (basename, ".info"))
                    priv->doctype = YELP_URI_DOCUMENT_TYPE_INFO;
                else if (is_man_path (basename, NULL))
                    priv->doctype = YELP_URI_DOCUMENT_TYPE_MAN;
            }
            else if (g_str_equal (mime_type, "text/plain")) {
                if (g_str_has_suffix (basename, ".info"))
                    priv->doctype = YELP_URI_DOCUMENT_TYPE_INFO;
                else if (is_man_path (basename, NULL))
                    priv->doctype = YELP_URI_DOCUMENT_TYPE_MAN;
                else
                    priv->doctype = YELP_URI_DOCUMENT_TYPE_TEXT;
            }
            else {
                priv->doctype = YELP_URI_DOCUMENT_TYPE_EXTERNAL;
            }
        }
    }
}

static gboolean
is_man_path (const gchar *path, const gchar *encoding)
{
    gchar **iter = (gchar **) mancats;

    if (encoding && *encoding) {
        while (iter && *iter) {
            gchar *ending = g_strdup_printf ("%s.%s", *iter, encoding);
            if (g_str_has_suffix (path, ending)) {
                g_free (ending);
                return TRUE;
            }
            g_free (ending);
            iter++;
        }
    } else {
        while (iter && *iter) {
            if (g_str_has_suffix (path, *iter)) {
                return TRUE;
            }
            iter++;
        }
    }
    return FALSE;
}
