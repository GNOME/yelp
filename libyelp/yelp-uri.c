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

static void           yelp_uri_class_init        (YelpUriClass   *klass);
static void           yelp_uri_init              (YelpUri        *uri);
static void           yelp_uri_dispose           (GObject        *object);
static void           yelp_uri_finalize          (GObject        *object);

static void           resolve_start              (YelpUri        *uri);
static void           resolve_async              (YelpUri        *uri);
static gboolean       resolve_final              (YelpUri        *uri);

static void           resolve_file_uri           (YelpUri        *uri);
static void           resolve_file_path          (YelpUri        *uri);
static void           resolve_data_dirs          (YelpUri        *uri,
                                                  const gchar   **subdirs,
                                                  const gchar    *docid,
                                                  const gchar    *pageid);
static void           resolve_ghelp_uri          (YelpUri        *uri);
static void           resolve_man_uri            (YelpUri        *uri);
static void           resolve_info_uri           (YelpUri        *uri);
static void           resolve_xref_uri           (YelpUri        *uri);
static void           resolve_page_and_frag      (YelpUri        *uri,
                                                  const gchar    *arg);
static void           resolve_gfile              (YelpUri        *uri,
                                                  const gchar    *hash);

static gboolean       is_man_path                (const gchar    *uri,
                                                  const gchar    *encoding);

G_DEFINE_TYPE (YelpUri, yelp_uri, G_TYPE_OBJECT);
#define GET_PRIV(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_URI, YelpUriPrivate))

typedef struct _YelpUriPrivate YelpUriPrivate;
struct _YelpUriPrivate {
    GThread              *resolver;

    YelpUriDocumentType   doctype;
    YelpUriDocumentType   tmptype;

    gchar                *docuri;
    gchar                *fulluri;
    GFile                *gfile;

    gchar               **search_path;
    gchar                *page_id;
    gchar                *frag_id;

    /* Unresolved */
    YelpUri              *res_base;
    gchar                *res_arg;
};

enum {
    RESOLVED,
    LAST_SIGNAL
};
static guint uri_signals[LAST_SIGNAL] = {0,};

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

static const gchar *infosuffix[] = {
    ".info",
    ".info.gz", ".info.bz2", ".info.lzma",
    ".gz", ".bz2", ".lzma",
    NULL
};

/******************************************************************************/

static void
yelp_uri_class_init (YelpUriClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose  = yelp_uri_dispose;
    object_class->finalize = yelp_uri_finalize;

    uri_signals[RESOLVED] =
        g_signal_new ("resolved",
                      G_OBJECT_CLASS_TYPE (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

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

    if (priv->res_base) {
        g_object_unref (priv->res_base);
        priv->res_base = NULL;
    }

    G_OBJECT_CLASS (yelp_uri_parent_class)->dispose (object);
}

static void
yelp_uri_finalize (GObject *object)
{
    YelpUriPrivate *priv = GET_PRIV (object);

    g_free (priv->docuri);
    g_free (priv->fulluri);
    g_strfreev (priv->search_path);
    g_free (priv->page_id);
    g_free (priv->frag_id);
    g_free (priv->res_arg);

    G_OBJECT_CLASS (yelp_uri_parent_class)->finalize (object);
}

/******************************************************************************/

YelpUri *
yelp_uri_new (const gchar *arg)
{
    return yelp_uri_new_relative (NULL, arg);
}

YelpUri *
yelp_uri_new_relative (YelpUri *base, const gchar *arg)
{
    YelpUri *uri;
    YelpUriPrivate *priv;

    uri = (YelpUri *) g_object_new (YELP_TYPE_URI, NULL);

    priv = GET_PRIV (uri);
    priv->doctype = YELP_URI_DOCUMENT_TYPE_UNRESOLVED;
    if (base)
        priv->res_base = g_object_ref (base);
    priv->res_arg = g_strdup (arg);

    return uri;
}

/******************************************************************************/

void
yelp_uri_resolve (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);

    if (priv->res_base && !yelp_uri_is_resolved (priv->res_base)) {
        g_signal_connect_swapped (priv->res_base, "resolved",
                                  G_CALLBACK (resolve_start),
                                  uri);
        yelp_uri_resolve (priv->res_base);
    }
    else {
        resolve_start (uri);
    }
}

/* We want code to be able to do something like this:
 *
 *   if (yelp_uri_get_document_type (uri) != YELP_URI_DOCUMENT_TYPE_UNRESOLVED) {
 *     g_signal_connect (uri, "resolve", callback, data);
 *     yelp_uri_resolve (uri);
 *   }
 *
 * Resolving happens in a separate thread, though, so if that thread can change
 * the document type, we have a race condition.  So here's the rules we play by:
 *
 * 1) None of the getters except the document type getter can return real data
 *    while the URI is unresolved.  They all do a resolved check first, and
 *    return NULL if the URI is not resolved.
 *
 * 2) The threaded resolver functions can modify anything but the document
 *    type.  They are the only things that are allowed to modify that data.
 *
 * 3) The resolver thread is not allowed to modify the document type.  When
 *    it's done, it queues an async function to set the document type and
 *    emit "resolved" in the main thread.
 *
 * 4) Once a URI is resolved, it is immutable.
 */
static void
resolve_start (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);

    if (priv->resolver == NULL) {
        g_object_ref (uri);
        priv->resolver = g_thread_create ((GThreadFunc) resolve_async,
                                          uri, FALSE, NULL);
    }
}

static void
resolve_async (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);

    if (g_str_has_prefix (priv->res_arg, "ghelp:")
        || g_str_has_prefix (priv->res_arg, "gnome-help:")) {
        resolve_ghelp_uri (uri);
    }
    else if (g_str_has_prefix (priv->res_arg, "file:")) {
        resolve_file_uri (uri);
    }
    else if (g_str_has_prefix (priv->res_arg, "man:")) {
        resolve_man_uri (uri);
    }
    else if (g_str_has_prefix (priv->res_arg, "info:")) {
        resolve_info_uri (uri);
    }
    else if (g_str_has_prefix (priv->res_arg, "xref:")) {
        YelpUriPrivate *base_priv;
        if (priv->res_base == NULL) {
            priv->tmptype = YELP_URI_DOCUMENT_TYPE_ERROR;
            goto done;
        }
        base_priv = GET_PRIV (priv->res_base);
        switch (base_priv->doctype) {
        case YELP_URI_DOCUMENT_TYPE_UNRESOLVED:
            break;
        case YELP_URI_DOCUMENT_TYPE_DOCBOOK:
        case YELP_URI_DOCUMENT_TYPE_MALLARD:
        case YELP_URI_DOCUMENT_TYPE_INFO:
            resolve_xref_uri (uri);
            break;
        case YELP_URI_DOCUMENT_TYPE_MAN:
            /* FIXME: what do we do? */
            break;
        case YELP_URI_DOCUMENT_TYPE_TEXT:
        case YELP_URI_DOCUMENT_TYPE_HTML:
        case YELP_URI_DOCUMENT_TYPE_XHTML:
            resolve_file_path (uri);
            break;
        case YELP_URI_DOCUMENT_TYPE_TOC:
            /* FIXME: what do we do? */
            break;
        case YELP_URI_DOCUMENT_TYPE_SEARCH:
            /* FIXME: what do we do? */
            break;
        case YELP_URI_DOCUMENT_TYPE_NOT_FOUND:
        case YELP_URI_DOCUMENT_TYPE_EXTERNAL:
        case YELP_URI_DOCUMENT_TYPE_ERROR:
            break;
        }
    }
    else if (strchr (priv->res_arg, ':')) {
        priv->tmptype = YELP_URI_DOCUMENT_TYPE_EXTERNAL;
        priv->fulluri = g_strdup (priv->res_arg);
    }
    else {
        resolve_file_path (uri);
    }

 done:
    g_idle_add ((GSourceFunc) resolve_final, uri);
}

static gboolean
resolve_final (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);

    priv->resolver = NULL;

    if (priv->tmptype != YELP_URI_DOCUMENT_TYPE_UNRESOLVED)
        priv->doctype = priv->tmptype;
    else
        priv->doctype = YELP_URI_DOCUMENT_TYPE_ERROR;
    
    if (priv->res_base) {
        g_object_unref (priv->res_base);
        priv->res_base = NULL;
    }

    if (priv->res_arg) {
        g_free (priv->res_arg);
        priv->res_arg = NULL;
    }

    g_signal_emit (uri, uri_signals[RESOLVED], 0);
    g_object_unref (uri);
    return FALSE;
}

/******************************************************************************/

gboolean
yelp_uri_is_resolved (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    return priv->doctype != YELP_URI_DOCUMENT_TYPE_UNRESOLVED;
}

YelpUriDocumentType
yelp_uri_get_document_type (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    return priv->doctype;
}

gchar *
yelp_uri_get_document_uri (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    if (priv->doctype == YELP_URI_DOCUMENT_TYPE_UNRESOLVED)
        return NULL;
    return g_strdup (priv->docuri);
}

gchar *
yelp_uri_get_canonical_uri (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    if (priv->doctype == YELP_URI_DOCUMENT_TYPE_UNRESOLVED)
        return NULL;
    return g_strdup (priv->fulluri);
}

GFile *
yelp_uri_get_file (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    if (priv->doctype == YELP_URI_DOCUMENT_TYPE_UNRESOLVED)
        return NULL;
    return priv->gfile ? g_object_ref (priv->gfile) : NULL;
}

gchar **
yelp_uri_get_search_path (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    if (priv->doctype == YELP_URI_DOCUMENT_TYPE_UNRESOLVED)
        return NULL;
    return g_strdupv (priv->search_path);
}

gchar *
yelp_uri_get_page_id (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    if (priv->doctype == YELP_URI_DOCUMENT_TYPE_UNRESOLVED)
        return NULL;
    return g_strdup (priv->page_id);
}

gchar *
yelp_uri_get_frag_id (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    if (priv->doctype == YELP_URI_DOCUMENT_TYPE_UNRESOLVED)
        return NULL;
    return g_strdup (priv->frag_id);
}

/******************************************************************************/

gchar *
yelp_uri_locate_file_uri (YelpUri     *uri,
                          const gchar *filename)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    GFile *gfile;
    gchar *fullpath;
    gchar *returi = NULL;
    gint i;
    for (i = 0; priv->search_path[i] != NULL; i++) {
        fullpath = g_strconcat (priv->search_path[i],
                                G_DIR_SEPARATOR_S,
                                filename,
                                NULL);
        if (g_file_test (fullpath, G_FILE_TEST_EXISTS)) {
            gfile = g_file_new_for_path (fullpath);
            returi = g_file_get_uri (gfile);
            g_object_unref (gfile);
        }
        g_free (fullpath);
        if (returi)
            break;
    }
    return returi;
}

/******************************************************************************/

static void
resolve_file_uri (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    gchar *uristr;
    const gchar *hash = strchr (priv->res_arg, '#');

    if (hash) {
        uristr = g_strndup (priv->res_arg, hash - priv->res_arg);
        hash++;
    }
    else
        uristr = priv->res_arg;

    priv->gfile = g_file_new_for_uri (uristr);

    resolve_gfile (uri, hash);
}

static void
resolve_file_path (YelpUri *uri)
{
    YelpUriPrivate *base_priv;
    YelpUriPrivate *priv = GET_PRIV (uri);
    gchar *path;
    const gchar *hash = strchr (priv->res_arg, '#');

    /* Treat xref: URIs like relative file paths */
    if (g_str_has_prefix (priv->res_arg, "xref:")) {
        gchar *tmp = g_strdup (priv->res_arg + 5);
        g_free (priv->res_arg);
        priv->res_arg = tmp;
    }

    if (priv->res_base)
        base_priv = GET_PRIV (priv->res_base);

    if (hash) {
        path = g_strndup (priv->res_arg, hash - priv->res_arg);
        hash++;
    }
    else
        path = priv->res_arg;

    if (priv->res_arg[0] == '/') {
        priv->gfile = g_file_new_for_path (path);
    }
    else if (base_priv && base_priv->gfile) {
        GFileInfo *info;
        info = g_file_query_info (base_priv->gfile,
                                  G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                  G_FILE_QUERY_INFO_NONE,
                                  NULL, NULL);
        if (g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR) {
            GFile *parent = g_file_get_parent (base_priv->gfile);
            priv->gfile = g_file_resolve_relative_path (parent, path);
            g_object_unref (parent);
        }
        else {
            priv->gfile = g_file_resolve_relative_path (base_priv->gfile, path);
        }
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

    resolve_gfile (uri, hash);
}

static void
resolve_data_dirs (YelpUri      *ret,
                   const gchar **subdirs,
                   const gchar  *docid,
                   const gchar  *pageid)
{
    const gchar * const *sdatadirs = g_get_system_data_dirs ();
    const gchar * const *langs = g_get_language_names ();
    /* The strings are still owned by GLib; we just own the array. */
    gchar **datadirs;
    YelpUriPrivate *priv = GET_PRIV (ret);
    gchar *filename = NULL;
    gchar **searchpath = NULL;
    gint searchi, searchmax;
    gint datadir_i, subdir_i, lang_i;

    datadirs = g_new0 (gchar *, g_strv_length ((gchar **) sdatadirs) + 2);
    datadirs[0] = (gchar *) g_get_user_data_dir ();
    for (datadir_i = 0; sdatadirs[datadir_i]; datadir_i++)
        datadirs[datadir_i + 1] = (gchar *) sdatadirs[datadir_i];

    searchi = 0;
    searchmax = 10;
    searchpath = g_new0 (gchar *, 10);

    for (datadir_i = 0; datadirs[datadir_i]; datadir_i++) {
        for (subdir_i = 0; subdirs[subdir_i]; subdir_i++) {
            for (lang_i = 0; langs[lang_i]; lang_i++) {
                gchar *helpdir = g_strdup_printf ("%s%s%s/%s/%s",
                                                  datadirs[datadir_i],
                                                  (datadirs[datadir_i][strlen(datadirs[datadir_i]) - 1] == '/' ? "" : "/"),
                                                  subdirs[subdir_i],
                                                  docid,
                                                  langs[lang_i]);
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

                if (priv->tmptype != YELP_URI_DOCUMENT_TYPE_UNRESOLVED)
                    /* We've already found it.  We're just adding to the search path now. */
                    continue;

                filename = g_strdup_printf ("%s/index.page", helpdir);
                if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
                    priv->tmptype = YELP_URI_DOCUMENT_TYPE_MALLARD;
                    g_free (filename);
                    filename = g_strdup (helpdir);
                    continue;
                }
                g_free (filename);

                filename = g_strdup_printf ("%s/%s.xml", helpdir, pageid);
                if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
                    priv->tmptype = YELP_URI_DOCUMENT_TYPE_DOCBOOK;
                    continue;
                }
                g_free (filename);

                filename = g_strdup_printf ("%s/%s.html", helpdir, pageid);
                if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
                    priv->tmptype = YELP_URI_DOCUMENT_TYPE_HTML;
                    continue;
                }
                g_free (filename);
            } /* end for langs */
        } /* end for subdirs */
    } /* end for datadirs */

    g_free (datadirs);
    if (priv->tmptype == YELP_URI_DOCUMENT_TYPE_UNRESOLVED) {
        g_strfreev (searchpath);
        priv->tmptype = YELP_URI_DOCUMENT_TYPE_NOT_FOUND;
    }
    else {
        priv->gfile = g_file_new_for_path (filename);
        priv->search_path = searchpath;
    }
}

static void
resolve_ghelp_uri (YelpUri *uri)
{
    /* ghelp:/path/to/file
     * ghelp:document[/file][?page][#frag]
     */
    YelpUriPrivate *priv = GET_PRIV (uri);
    const gchar const *helpdirs[3] = {"help", "gnome/help", NULL};
    gchar *document, *slash, *query, *hash;
    gchar *colon, *c; /* do not free */

    colon = strchr (priv->res_arg, ':');
    if (!colon) {
        priv->tmptype = YELP_URI_DOCUMENT_TYPE_ERROR;
        return;
    }

    slash = query = hash = NULL;
    for (c = colon; *c != '\0'; c++) {
        if (*c == '#' && hash == NULL)
            hash = c;
        else if (*c == '?' && query == NULL && hash == NULL)
            query = c;
        else if (*c == '/' && slash == NULL && query == NULL && hash == NULL)
            slash = c;
    }

    if (slash || query || hash)
        document = g_strndup (colon + 1,
                              (slash ? slash : (query ? query : hash)) - colon - 1);
    else
        document = g_strdup (colon + 1);

    if (slash && (query || hash))
        slash = g_strndup (slash + 1,
                           (query ? query : hash) - slash - 1);
    else if (slash)
        slash = g_strdup (slash + 1);

    if (query && hash)
        query = g_strndup (query + 1,
                           hash - query - 1);
    else if (query)
        query = g_strdup (query + 1);

    if (hash)
        hash = g_strdup (hash + 1);

    if (*(colon + 1) == '/') {
        gchar *newuri;
        newuri = g_strdup_printf ("file:/%s", slash);
        g_free (priv->res_arg);
        priv->res_arg = newuri;
        resolve_file_uri (uri);
    }
    else {
        resolve_data_dirs (uri, helpdirs, document, slash ? slash : document);
    }

    if (query && hash) {
        priv->page_id = query;
        priv->frag_id = hash;
    }
    else if (query) {
        priv->page_id = query;
        if (priv->tmptype != YELP_URI_DOCUMENT_TYPE_MALLARD)
            priv->frag_id = g_strdup (query);
    }
    else if (hash) {
        priv->page_id = hash;
        priv->frag_id = g_strdup (hash);
    }

    priv->docuri = g_strconcat ("ghelp:", document,
                                slash ? "/" : NULL,
                                slash, NULL);

    priv->fulluri = g_strconcat (priv->docuri,
                                 priv->page_id ? "?" : "",
                                 priv->page_id ? priv->page_id : "",
                                 priv->frag_id ? "#" : "",
                                 priv->frag_id ? priv->frag_id : "",
                                 NULL);

    g_free (document);
    g_free (slash);
    return;
}

static void
resolve_man_uri (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
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
    gchar *realsection;
    gchar *colon, *hash;
    gchar *lbrace = NULL;
    gchar *rbrace = NULL;
    gint i, j, k;

    if (g_str_has_prefix (priv->res_arg, "man:/")) {
        gchar *newuri;
        priv->tmptype = YELP_URI_DOCUMENT_TYPE_MAN;
        newuri = g_strdup_printf ("file:%s", priv->res_arg + 4);
        g_free (priv->res_arg);
        priv->res_arg = newuri;
        resolve_file_uri (uri);
        return;
    }

    if (!manpath) {
        /* Initialize manpath only once */
        const gchar *env = g_getenv ("MANPATH");
        if (!env || env[0] == '\0')
            env = "/usr/share/man:/usr/man:/usr/local/share/man:/usr/local/man";
        manpath = g_strsplit (env, ":", 0);
    }

    colon = strchr (priv->res_arg, ':');
    if (colon)
        colon++;
    else
        colon = (gchar *) priv->res_arg;

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
                if (section)
                    realsection = section;
                else
                    realsection = (gchar *) mancats[k];

                fullpath = g_strconcat (langdir, "/man", realsection,
                                        "/", name, ".", realsection,
                                        NULL);
                if (g_file_test (fullpath, G_FILE_TEST_IS_REGULAR))
                    goto gotit;
                g_free (fullpath);

                fullpath = g_strconcat (langdir, "/man", realsection,
                                        "/", name, ".", realsection, ".gz",
                                        NULL);
                if (g_file_test (fullpath, G_FILE_TEST_IS_REGULAR))
                    goto gotit;
                g_free (fullpath);

                fullpath = g_strconcat (langdir, "/man", realsection,
                                        "/", name, ".", realsection, ".bz2",
                                        NULL);
                if (g_file_test (fullpath, G_FILE_TEST_IS_REGULAR))
                    goto gotit;
                g_free (fullpath);

                fullpath = g_strconcat (langdir, "/man", realsection,
                                        "/", name, ".", realsection, ".lzma",
                                        NULL);
                if (g_file_test (fullpath, G_FILE_TEST_IS_REGULAR))
                    goto gotit;
                g_free (fullpath);

                fullpath = NULL;
            gotit:
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
        priv->tmptype = YELP_URI_DOCUMENT_TYPE_MAN;
        priv->gfile = g_file_new_for_path (fullpath);
        priv->docuri = g_strconcat ("man:",  name, ".", realsection, NULL);
        priv->fulluri = g_strdup (priv->docuri);
        resolve_gfile (uri, NULL);
        g_free (fullpath);
    } else {
        priv->tmptype = YELP_URI_DOCUMENT_TYPE_NOT_FOUND;
    }

    if (hash)
        resolve_page_and_frag (uri, hash + 1);

    g_free (newarg);
}

static void
resolve_info_uri (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    /* info:/path/to/file
     * info:name#node
     * info:name
     * info:(name)node
     * info:(name)
     */
    static gchar **infopath = NULL;
    const gchar * const * langs = g_get_language_names ();
    gchar *name = NULL;
    gchar *sect = NULL;
    gchar *fullpath = NULL;
    /* do not free */
    gchar *colon;
    gint infopath_i, suffix_i;

    if (g_str_has_prefix (priv->res_arg, "info:/")) {
        gchar *newuri;
        priv->tmptype = YELP_URI_DOCUMENT_TYPE_INFO;
        newuri = g_strdup_printf ("file:%s", priv->res_arg + 4);
        g_free (priv->res_arg);
        priv->res_arg = newuri;
        resolve_file_uri (uri);
        return;
    }

    if (!infopath) {
        /* Initialize infopath only once */
        const gchar *env = g_getenv ("INFOPATH");
        if (!env || env[0] == '\0')
            env = "/usr/info:/usr/share/info:/usr/local/info:/usr/local/share/info";
        infopath = g_strsplit (env, ":", 0);
    }

    colon = strchr (priv->res_arg, ':');
    if (colon)
        colon++;
    else
        colon = (gchar *) priv->res_arg;

    if (colon[0] == '(') {
        const gchar *rbrace = strchr (colon, ')');
        if (rbrace) {
            name = g_strndup (colon + 1, rbrace - colon - 1);
            sect = g_strdup (rbrace + 1);
        }
    }
    else {
        const gchar *hash = strchr (colon, '#');
        if (hash) {
            name = g_strndup (colon, hash - colon);
            sect = g_strdup (hash + 1);
        }
        else {
            name = g_strdup (colon);
            sect = NULL;
        }
    }

    for (infopath_i = 0; infopath[infopath_i]; infopath_i++) {
        if (!g_file_test (infopath[infopath_i], G_FILE_TEST_IS_DIR))
            continue;
        for (suffix_i = 0; infosuffix[suffix_i]; suffix_i++) {
            fullpath = g_strconcat (infopath[infopath_i], "/",
                                    name, infosuffix[suffix_i], NULL);
            if (g_file_test (fullpath, G_FILE_TEST_IS_REGULAR))
                break;
            g_free (fullpath);
            fullpath = NULL;
        }
        if (fullpath != NULL)
            break;
    }

    if (fullpath) {
        priv->tmptype = YELP_URI_DOCUMENT_TYPE_INFO;
        priv->gfile = g_file_new_for_path (fullpath);
        priv->docuri = g_strconcat ("info:", name, NULL);
        if (sect) {
            priv->fulluri = g_strconcat ("info:", name, "#", sect, NULL);
            priv->page_id = g_strdup (sect);
            priv->frag_id = sect;
            sect = NULL; /* steal memory */
        }
        else
            priv->fulluri = g_strdup (priv->docuri);
        resolve_gfile (uri, NULL);
    } else {
        gchar *res_arg = priv->res_arg;
        priv->res_arg = g_strconcat ("man:", name, NULL);
        resolve_man_uri (uri);
        if (priv->tmptype == YELP_URI_DOCUMENT_TYPE_MAN) {
            g_free (priv->res_arg);
            priv->res_arg = res_arg;
        }
        else {
            g_free (res_arg);
            priv->tmptype = YELP_URI_DOCUMENT_TYPE_NOT_FOUND;
        }
    }
    g_free (fullpath);
    g_free (name);
    g_free (sect);
}

static void
resolve_xref_uri (YelpUri *uri)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    const gchar *arg = priv->res_arg + 5;
    YelpUriPrivate *base_priv = GET_PRIV (priv->res_base);

    priv->tmptype = base_priv->doctype;
    priv->gfile = g_object_ref (base_priv->gfile);
    priv->search_path = g_strdupv (base_priv->search_path);
    priv->docuri = g_strdup (base_priv->docuri);

    if (arg[0] == '#') {
        priv->page_id = g_strdup (base_priv->page_id);
        priv->frag_id = g_strdup (arg + 1);
    }
    else {
        gchar *hash = strchr (arg, '#');
        if (hash) {
            priv->page_id = g_strndup (arg, hash - arg);
            priv->frag_id = g_strdup (hash + 1);
        }
        else {
            priv->page_id = g_strdup (arg);
            priv->frag_id = NULL;
        }
    }

    if (g_str_has_prefix (priv->docuri, "ghelp:"))
        priv->fulluri = g_strconcat (priv->docuri,
                                     priv->page_id ? "?" : "",
                                     priv->page_id ? priv->page_id : "",
                                     priv->frag_id ? "#" : "",
                                     priv->frag_id ? priv->frag_id : "",
                                     NULL);
    else if (g_str_has_prefix (priv->docuri, "file:") ||
             g_str_has_prefix (priv->docuri, "info:") )
        priv->fulluri = g_strconcat (priv->docuri,
                                     (priv->page_id || priv->frag_id) ? "#" : "",
                                     priv->page_id ? priv->page_id : "",
                                     priv->frag_id ? "#" : "",
                                     priv->frag_id,
                                     NULL);
    else
        /* FIXME: other URI schemes */
        priv->fulluri = g_strconcat (priv->docuri,
                                     priv->page_id ? "#" : "",
                                     priv->page_id,
                                     NULL);
}

static void
resolve_page_and_frag (YelpUri *uri, const gchar *arg)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
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
resolve_gfile (YelpUri *uri, const gchar *hash)
{
    YelpUriPrivate *priv = GET_PRIV (uri);
    GFileInfo *info;
    GError *error = NULL;

    info = g_file_query_info (priv->gfile,
                              G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                              G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
                              G_FILE_QUERY_INFO_NONE,
                              NULL, &error);
    if (error) {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
            priv->tmptype = YELP_URI_DOCUMENT_TYPE_NOT_FOUND;
        }
        else
            priv->tmptype = YELP_URI_DOCUMENT_TYPE_ERROR;
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

    if (priv->tmptype == YELP_URI_DOCUMENT_TYPE_UNRESOLVED) {
        gchar **splithash = NULL;
        if (hash)
            splithash = g_strsplit (hash, "#", 2);
        priv->tmptype = YELP_URI_DOCUMENT_TYPE_EXTERNAL;

        if (g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE) ==
            G_FILE_TYPE_DIRECTORY) {
            GFile *child = g_file_get_child (priv->gfile, "index.page");
            if (g_file_query_exists (child, NULL)) {
                priv->tmptype = YELP_URI_DOCUMENT_TYPE_MALLARD;
                if (splithash) {
                    if (priv->page_id == NULL)
                        priv->page_id = g_strdup (splithash[0]);
                    if (priv->frag_id == NULL && splithash[0])
                        priv->frag_id = g_strdup (splithash[1]);
                }
            }
            g_object_unref (child);
        }
        else {
            gchar *basename;
            const gchar *mime_type = g_file_info_get_attribute_string (info,
                                                                       G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
            basename = g_file_get_basename (priv->gfile);
            if (g_str_has_suffix (basename, ".page")) {
                GFile *old;
                priv->tmptype = YELP_URI_DOCUMENT_TYPE_MALLARD;
                old = priv->gfile;
                priv->gfile = g_file_get_parent (old);
                if (priv->page_id == NULL) {
                    /* File names aren't really page IDs, so we stick an illegal character
                       on the beginning so it can't possibly be confused with a real page
                       ID.  Then we let YelpMallardDocument map file names to pages IDs.
                     */
                    gchar *tmp = g_file_get_basename (old);
                    priv->page_id = g_strconcat (G_DIR_SEPARATOR_S, tmp, NULL);
                    g_free (tmp);
                }
                if (priv->frag_id == NULL)
                    priv->frag_id = g_strdup (hash);
                g_object_unref (old);
            }
            else if (g_str_equal (mime_type, "text/xml") ||
                g_str_equal (mime_type, "application/docbook+xml") ||
                g_str_equal (mime_type, "application/xml")) {
                priv->tmptype = YELP_URI_DOCUMENT_TYPE_DOCBOOK;
                if (splithash) {
                    if (priv->page_id == NULL)
                        priv->page_id = g_strdup (splithash[0]);
                    if (priv->frag_id == NULL && splithash[0])
                        priv->frag_id = g_strdup (splithash[1]);
                }
            }
            else if (g_str_equal (mime_type, "text/html") || 
                     g_str_equal (mime_type, "application/xhtml+xml")) {
                GFile *parent = g_file_get_parent (priv->gfile);
                priv->docuri = g_file_get_uri (parent);
                g_object_unref (parent);
                priv->tmptype = mime_type[0] == 't' ? YELP_URI_DOCUMENT_TYPE_HTML : YELP_URI_DOCUMENT_TYPE_XHTML;
                if (priv->page_id == NULL)
                    priv->page_id = g_file_get_basename (priv->gfile);
                if (priv->frag_id == NULL)
                    priv->frag_id = g_strdup (hash);
                if (priv->fulluri == NULL) {
                    gchar *fulluri;
                    fulluri = g_file_get_uri (priv->gfile);
                    priv->fulluri = g_strconcat (fulluri,
                                                 priv->frag_id ? "#" : NULL,
                                                 priv->frag_id,
                                                 NULL);
                    g_free (fulluri);
                }
            }
            else if (g_str_equal (mime_type, "application/x-gzip")) {
                if (g_str_has_suffix (basename, ".info.gz"))
                    priv->tmptype = YELP_URI_DOCUMENT_TYPE_INFO;
                else if (is_man_path (basename, "gz"))
                    priv->tmptype = YELP_URI_DOCUMENT_TYPE_MAN;
            }
            else if (g_str_equal (mime_type, "application/x-bzip")) {
                if (g_str_has_suffix (basename, ".info.bz2"))
                    priv->tmptype = YELP_URI_DOCUMENT_TYPE_INFO;
                else if (is_man_path (basename, "bz2"))
                    priv->tmptype = YELP_URI_DOCUMENT_TYPE_MAN;
            }
            else if (g_str_equal (mime_type, "application/x-lzma")) {
                if (g_str_has_suffix (basename, ".info.lzma"))
                    priv->tmptype = YELP_URI_DOCUMENT_TYPE_INFO;
                else if (is_man_path (basename, "lzma"))
                    priv->tmptype = YELP_URI_DOCUMENT_TYPE_MAN;
            }
            else if (g_str_equal (mime_type, "application/octet-stream")) {
                if (g_str_has_suffix (basename, ".info"))
                    priv->tmptype = YELP_URI_DOCUMENT_TYPE_INFO;
                else if (is_man_path (basename, NULL))
                    priv->tmptype = YELP_URI_DOCUMENT_TYPE_MAN;
            }
            else if (g_str_equal (mime_type, "text/plain")) {
                if (g_str_has_suffix (basename, ".info"))
                    priv->tmptype = YELP_URI_DOCUMENT_TYPE_INFO;
                else if (is_man_path (basename, NULL))
                    priv->tmptype = YELP_URI_DOCUMENT_TYPE_MAN;
                else
                    priv->tmptype = YELP_URI_DOCUMENT_TYPE_TEXT;
                if (priv->frag_id == NULL)
                    priv->frag_id = g_strdup (hash);
            }
            else if (g_str_equal (mime_type, "text/x-readme")) {
                priv->tmptype = YELP_URI_DOCUMENT_TYPE_TEXT;
            }
            else {
                priv->tmptype = YELP_URI_DOCUMENT_TYPE_EXTERNAL;
            }
        }
        if (splithash)
            g_strfreev (splithash);
    }

    if (priv->docuri == NULL)
        priv->docuri = g_file_get_uri (priv->gfile);

    if (priv->fulluri == NULL)
        priv->fulluri = g_strconcat (priv->docuri,
                                     (priv->page_id || priv->frag_id) ? "#" : NULL,
                                     priv->page_id ? priv->page_id : priv->frag_id,
                                     NULL);

    g_object_unref (info);
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
