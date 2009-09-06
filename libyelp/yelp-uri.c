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

#define YELP_URI_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_URI, YelpUriPriv))

struct _YelpUriPriv {
    YelpUriDocumentType   doctype;
    GFile                *gfile;
    gchar               **search_path;
    gchar                *page_id;
    gchar                *frag_id;
};

static void           uri_class_init        (YelpUriClass   *klass);
static void           uri_init              (YelpUri        *uri);
static void           uri_dispose           (GObject        *object);

static void           resolve_file_uri      (YelpUri        *ret,
                                             gchar          *arg);
static void           resolve_file_path     (YelpUri        *ret,
                                             YelpUri        *base,
                                             gchar          *arg);
static void           resolve_ghelp_uri     (YelpUri        *ret,
                                             gchar          *arg);
static void           resolve_man_uri       (YelpUri        *ret,
                                             gchar          *arg);
static void           resolve_info_uri      (YelpUri        *ret,
                                             gchar          *arg);
static void           resolve_page_and_frag (YelpUri        *ret,
                                             gchar          *arg);
static void           resolve_common        (YelpUri        *ret);
static gboolean       is_man_path           (gchar          *uri,
                                             gchar          *encoding);

static GObjectClass *parent_class;


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

GType
yelp_uri_get_type (void)
{
    static GType type = 0;
    if (!type) {
        static const GTypeInfo info = {
            sizeof (YelpUriClass),
            NULL, NULL,
            (GClassInitFunc) uri_class_init,
            NULL, NULL,
            sizeof (YelpUri),
            0,
            (GInstanceInitFunc) uri_init,
        };
        type = g_type_register_static (G_TYPE_OBJECT,
                                       "YelpUri", 
                                       &info, 0);
    }
    return type;
}

static void
uri_class_init (YelpUriClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);
    parent_class = g_type_class_peek_parent (klass);
    object_class->dispose      = uri_dispose;
    g_type_class_add_private (klass, sizeof (YelpUriPriv));
}

static void
uri_init (YelpUri *uri)
{
    uri->priv = YELP_URI_GET_PRIVATE (uri);
}

static void
uri_dispose (GObject *object)
{
    YelpUri *uri = YELP_URI (object);

    if (uri->priv->gfile)
        g_object_unref (uri->priv->gfile);
    g_strfreev (uri->priv->search_path);
    g_free (uri->priv->page_id);
    g_free (uri->priv->frag_id);

    parent_class->dispose (object);
}

/******************************************************************************/

YelpUri *
yelp_uri_resolve (gchar *arg)
{
    return yelp_uri_resolve_relative (NULL, arg);
}

YelpUri *
yelp_uri_resolve_relative (YelpUri *base, gchar *arg)
{
    YelpUri *ret;

    ret = (YelpUri *) g_object_new (YELP_TYPE_URI, NULL);
    ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_UNKNOWN;

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
        ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_EXTERNAL;
        ret->priv->gfile = g_file_new_for_uri (arg);
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
    return uri->priv->doctype;
}

gchar *
yelp_uri_get_base_uri (YelpUri *uri)
{
    return uri->priv->gfile ? g_file_get_uri (uri->priv->gfile) : NULL;
}

gchar **
yelp_uri_get_search_path (YelpUri *uri)
{
    return g_strdupv (uri->priv->search_path);
}

gchar *
yelp_uri_get_page_id (YelpUri *uri)
{
    return g_strdup (uri->priv->page_id);
}

gchar *
yelp_uri_get_frag_id (YelpUri *uri)
{
    return g_strdup (uri->priv->frag_id);
}

/******************************************************************************/

static void
resolve_file_uri (YelpUri *ret, gchar *arg)
{
    gchar *uri;
    gchar *hash;

    hash = strchr (arg, '#');
    if (hash)
        uri = g_strndup (arg, hash - arg);
    else
        uri = arg;

    ret->priv->gfile = g_file_new_for_uri (uri);

    if (hash) {
        resolve_page_and_frag (ret, hash + 1);
        g_free (uri);
    }

    resolve_common (ret);
}

static void
resolve_file_path (YelpUri *ret, YelpUri *base, gchar *arg)
{
    gchar *path;
    gchar *hash;

    hash = strchr (arg, '#');
    if (hash)
        path = g_strndup (arg, hash - arg);
    else
        path = arg;

    if (arg[0] == '/') {
        ret->priv->gfile = g_file_new_for_path (path);
    }
    else if (base && base->priv->gfile) {
        ret->priv->gfile = g_file_resolve_relative_path (base->priv->gfile, path);
    }
    else {
        gchar *cur;
        GFile *curfile;
        cur = g_get_current_dir ();
        curfile = g_file_new_for_path (cur);
        ret->priv->gfile = g_file_resolve_relative_path (curfile, path);
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
resolve_ghelp_uri (YelpUri *ret, gchar *arg)
{
    /* ghelp:/path/to/file
     * ghelp:document
     */
    gchar *colon;

    colon = strchr (arg, ':');
    if (!colon) {
        ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_ERROR;
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

}

static void
resolve_man_uri (YelpUri *ret, gchar *arg)
{
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
        ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_MAN;
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
        colon = arg;

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
        ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_MAN;
        ret->priv->gfile = g_file_new_for_path (fullpath);
        resolve_common (ret);
        g_free (fullpath);
    } else {
        ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_NOT_FOUND;
    }

    if (hash)
        resolve_page_and_frag (ret, hash + 1);

    g_free (newarg);
}

static void
resolve_info_uri (YelpUri *ret, gchar *arg)
{
    /* FIXME */
}

static void
resolve_page_and_frag (YelpUri *ret, gchar *arg)
{
    gchar *hash;

    if (!arg || arg[0] == '\0')
        return;

    hash = strchr (arg, '#');
    if (hash) {
        ret->priv->page_id = g_strndup (arg, hash - arg);
        ret->priv->frag_id = g_strdup (hash + 1);
    } else {
        ret->priv->page_id = g_strdup (arg);
        ret->priv->frag_id = g_strdup (arg);
    }
    return;
}

static void
resolve_common (YelpUri *ret)
{
    GFileInfo *info;
    GError *error = NULL;

    info = g_file_query_info (ret->priv->gfile,
                              G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                              G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
                              G_FILE_QUERY_INFO_NONE,
                              NULL, &error);
    if (error) {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
            ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_NOT_FOUND;
        else
            ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_ERROR;
        g_error_free (error);
        return;
    }

    if (ret->priv->search_path == NULL) {
        if (g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE) ==
            G_FILE_TYPE_DIRECTORY) {
            ret->priv->search_path = g_new0 (gchar *, 2);
            ret->priv->search_path[0] = g_file_get_path (ret->priv->gfile);
        } else {
            GFile *parent = g_file_get_parent (ret->priv->gfile);
            ret->priv->search_path = g_new0 (gchar *, 2);
            ret->priv->search_path[0] = g_file_get_path (parent);
            g_object_unref (parent);
        }
    }

    if (ret->priv->doctype == YELP_URI_DOCUMENT_TYPE_UNKNOWN) {
        if (g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE) ==
            G_FILE_TYPE_DIRECTORY) {
            ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_MALLARD;
        }
        else {
            gchar *basename;
            const gchar *mime_type = g_file_info_get_attribute_string (info,
                                                                       G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
            basename = g_file_get_basename (ret->priv->gfile);
            if (g_str_equal (mime_type, "text/xml") ||
                g_str_equal (mime_type, "application/docbook+xml") ||
                g_str_equal (mime_type, "application/xml")) {
                ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_DOCBOOK;
            }
            else if (g_str_equal (mime_type, "text/html")) {
                ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_HTML;
            }
            else if (g_str_equal (mime_type, "application/xhtml+xml")) {
                ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_XHTML;
            }
            else if (g_str_equal (mime_type, "application/x-gzip")) {
                if (g_str_has_suffix (basename, ".info.gz"))
                    ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_INFO;
                else if (is_man_path (basename, "gz"))
                    ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_MAN;
            }
            else if (g_str_equal (mime_type, "application/x-bzip")) {
                if (g_str_has_suffix (basename, ".info.bz2"))
                    ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_INFO;
                else if (is_man_path (basename, "bz2"))
                    ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_MAN;
            }
            else if (g_str_equal (mime_type, "application/x-lzma")) {
                if (g_str_has_suffix (basename, ".info.lzma"))
                    ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_INFO;
                else if (is_man_path (basename, "lzma"))
                    ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_MAN;
            }
            else if (g_str_equal (mime_type, "application/octet-stream")) {
                if (g_str_has_suffix (basename, ".info"))
                    ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_INFO;
                else if (is_man_path (basename, NULL))
                    ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_MAN;
            }
            else if (g_str_equal (mime_type, "text/plain")) {
                if (g_str_has_suffix (basename, ".info"))
                    ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_INFO;
                else if (is_man_path (basename, NULL))
                    ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_MAN;
                else
                    ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_TEXT;
            }
            else {
                ret->priv->doctype = YELP_URI_DOCUMENT_TYPE_EXTERNAL;
            }
        }
    }
}

static gboolean
is_man_path (gchar *path, gchar *encoding)
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
