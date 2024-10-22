/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2010-2020 Shaun McCance  <shaunm@gnome.org>
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/xinclude.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "yelp-help-list.h"
#include "yelp-settings.h"
#include "yelp-transform.h"

#define STYLESHEET DATADIR"/yelp/xslt/links2html.xsl"

typedef struct _HelpListEntry HelpListEntry;

static void           yelp_help_list_dispose         (GObject               *object);
static void           yelp_help_list_finalize        (GObject               *object);

static gboolean       help_list_request_page         (YelpDocument          *document,
                                                      const gchar           *page_id,
                                                      GCancellable          *cancellable,
                                                      YelpDocumentCallback   callback,
                                                      gpointer               user_data,
                                                      GDestroyNotify         notify);
static void           help_list_think                (YelpHelpList          *list);
static void           help_list_reload               (YelpHelpList          *list);
static void           help_list_handle_page          (YelpHelpList          *list,
                                                      const gchar           *page_id);
static void           help_list_process_docbook      (YelpHelpList          *list,
                                                      HelpListEntry         *entry);
static void           help_list_process_mallard      (YelpHelpList          *list,
                                                      HelpListEntry         *entry);

static const char*const known_vendor_prefixes[] = { "gnome",
                                                    "fedora",
                                                    "mozilla",
                                                    NULL };

struct _HelpListEntry
{
    gchar *id;
    gchar *title;
    gchar *desc;
    gchar *icon;

    gchar *filename;
    YelpUriDocumentType type;
};
static void
help_list_entry_free (HelpListEntry *entry)
{
    g_free (entry->id);
    g_free (entry->title);
    g_free (entry->desc);
    g_free (entry);
}
static gint
help_list_entry_cmp (HelpListEntry *a, HelpListEntry *b)
{
    gchar *as, *bs;
    as = a->title ? a->title : strchr (a->id, ':') + 1;
    bs = b->title ? b->title : strchr (b->id, ':') + 1;
    return g_utf8_collate (as, bs);
}

typedef struct _YelpHelpListPrivate  YelpHelpListPrivate;
struct _YelpHelpListPrivate {
    GMutex         mutex;
    GThread       *thread;

    gboolean process_running;
    gboolean process_ran;

    GHashTable    *entries;
    GList         *all_entries;
    GSList        *pending;
    xmlDocPtr      entriesdoc;

    YelpTransform *transform;

    xmlXPathCompExprPtr  get_docbook_title;
    xmlXPathCompExprPtr  get_mallard_title;
    xmlXPathCompExprPtr  get_mallard_desc;
};

G_DEFINE_TYPE_WITH_PRIVATE (YelpHelpList, yelp_help_list, YELP_TYPE_DOCUMENT)

static void
yelp_help_list_class_init (YelpHelpListClass *klass)
{
    GObjectClass      *object_class   = G_OBJECT_CLASS (klass);
    YelpDocumentClass *document_class = YELP_DOCUMENT_CLASS (klass);

    object_class->dispose = yelp_help_list_dispose;
    object_class->finalize = yelp_help_list_finalize;

    document_class->request_page = help_list_request_page;
}

static void
yelp_help_list_init (YelpHelpList *list)
{
    YelpHelpListPrivate *priv = yelp_help_list_get_instance_private (list);

    g_mutex_init (&priv->mutex);
    /* don't free the key, it belongs to the value struct */
    priv->entries = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           NULL,
                                           (GDestroyNotify) help_list_entry_free);

    priv->get_docbook_title = xmlXPathCompile (BAD_CAST "normalize-space("
                                               "( /*/title | /*/db:title"
                                               "| /*/articleinfo/title"
                                               "| /*/bookinfo/title"
                                               "| /*/db:info/db:title"
                                               ")[1])");
    priv->get_mallard_title = xmlXPathCompile (BAD_CAST "normalize-space((/mal:page/mal:info/mal:title[@type='text'] |"
                                               "                 /mal:page/mal:title)[1])");
    priv->get_mallard_desc = xmlXPathCompile (BAD_CAST "normalize-space(/mal:page/mal:info/mal:desc[1])");

    yelp_document_set_page_id ((YelpDocument *) list, NULL, "index");
    yelp_document_set_page_id ((YelpDocument *) list, "index", "index");
}

static void
yelp_help_list_dispose (GObject *object)
{
    G_OBJECT_CLASS (yelp_help_list_parent_class)->dispose (object);
}

static void
yelp_help_list_finalize (GObject *object)
{
    YelpHelpListPrivate *priv = yelp_help_list_get_instance_private (YELP_HELP_LIST (object));

    /* entry structs belong to hash table */
    g_list_free (priv->all_entries);
    g_hash_table_destroy (priv->entries);
    g_mutex_clear (&priv->mutex);

    if (priv->entriesdoc) {
        xmlFreeDoc (priv->entriesdoc);
        priv->entriesdoc = NULL;
    }

    if (priv->get_docbook_title)
        xmlXPathFreeCompExpr (priv->get_docbook_title);
    if (priv->get_mallard_title)
        xmlXPathFreeCompExpr (priv->get_mallard_title);
    if (priv->get_mallard_desc)
        xmlXPathFreeCompExpr (priv->get_mallard_desc);

    g_clear_object (&priv->transform);

    G_OBJECT_CLASS (yelp_help_list_parent_class)->finalize (object);
}

YelpDocument *
yelp_help_list_new (YelpUri *uri)
{
    YelpHelpList *helplist = g_object_new (YELP_TYPE_HELP_LIST, NULL);

    g_signal_connect_swapped (yelp_settings_get_default (),
                              "colors-changed",
                              G_CALLBACK (help_list_reload),
                              helplist);

    return (YelpDocument *) helplist;
}

/******************************************************************************/

static gboolean
help_list_request_page (YelpDocument          *document,
                        const gchar           *page_id,
                        GCancellable          *cancellable,
                        YelpDocumentCallback   callback,
                        gpointer               user_data,
                        GDestroyNotify         notify)
{
    gboolean handled;
    YelpHelpListPrivate *priv = yelp_help_list_get_instance_private (YELP_HELP_LIST (document));

    if (page_id == NULL)
        page_id = "index";

    handled =
        YELP_DOCUMENT_CLASS (yelp_help_list_parent_class)->request_page (document,
                                                                         page_id,
                                                                         cancellable,
                                                                         callback,
                                                                         user_data,
                                                                         notify);
    if (handled) {
        return TRUE;
    }

    g_mutex_lock (&priv->mutex);
    if (priv->process_ran) {
        help_list_handle_page ((YelpHelpList *) document, page_id);
        return TRUE;
    }

    if (!priv->process_running) {
        priv->process_running = TRUE;
        g_object_ref (document);
        priv->thread = g_thread_new ("helplist-page",
                                     (GThreadFunc)(GCallback) help_list_think,
                                     document);
    }
    priv->pending = g_slist_prepend (priv->pending, g_strdup (page_id));
    g_mutex_unlock (&priv->mutex);
    return TRUE;
}

static void
help_list_think (YelpHelpList *list)
{
    const gchar * const *sdatadirs = g_get_system_data_dirs ();
    const gchar * const *langs = g_get_language_names ();
    YelpHelpListPrivate *priv = yelp_help_list_get_instance_private (list);
    /* The strings are still owned by GLib; we just own the array. */
    gchar **datadirs;
    gint datadir_i, lang_i;
    GList *cur;
    GtkIconTheme *theme;

    datadirs = g_new0 (gchar *, g_strv_length ((gchar **) sdatadirs) + 2);
    datadirs[0] = (gchar *) g_get_user_data_dir ();
    for (datadir_i = 0; sdatadirs[datadir_i]; datadir_i++)
        datadirs[datadir_i + 1] = (gchar *) sdatadirs[datadir_i];

    for (datadir_i = 0; datadirs[datadir_i]; datadir_i++) {
        gchar *helpdirname = g_build_filename (datadirs[datadir_i], "gnome", "help", NULL);       
        GFile *helpdir = g_file_new_for_path (helpdirname);
        GFileEnumerator *children = g_file_enumerate_children (helpdir,
                                                               G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                                               G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                               G_FILE_QUERY_INFO_NONE,
                                                               NULL, NULL);
        GFileInfo *child;
        if (children == NULL) {
            g_object_unref (helpdir);
            g_free (helpdirname);
            continue;
        }
        while ((child = g_file_enumerator_next_file (children, NULL, NULL))) {
            gchar *docid;
            HelpListEntry *entry = NULL;

            if (g_file_info_get_file_type (child) != G_FILE_TYPE_DIRECTORY) {
                g_object_unref (child);
                continue;
            }

            docid = g_strconcat ("ghelp:", g_file_info_get_name (child), NULL);
            if (g_hash_table_lookup (priv->entries, docid)) {
                g_free (docid);
                g_object_unref (child);
                continue;
            }

            for (lang_i = 0; langs[lang_i]; lang_i++) {
                gchar *filename, *tmp;

                filename = g_build_filename (helpdirname,
                                            g_file_info_get_name (child),
                                            langs[lang_i],
                                            "index.page",
                                             NULL);
                if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
                    entry = g_new0 (HelpListEntry, 1);
                    entry->id = docid;
                    entry->filename = filename;
                    entry->type = YELP_URI_DOCUMENT_TYPE_MALLARD;
                    break;
                }
                g_free (filename);

                tmp = g_strdup_printf ("%s.xml", g_file_info_get_name (child));
                filename = g_build_filename (helpdirname,
                                             g_file_info_get_name (child),
                                             langs[lang_i],
                                             tmp,
                                             NULL);
                g_free (tmp);
                if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
                    entry = g_new0 (HelpListEntry, 1);
                    entry->id = docid;
                    entry->filename = filename;
                    entry->type = YELP_URI_DOCUMENT_TYPE_DOCBOOK;
                    break;
                }
                g_free (filename);
            }

            if (entry != NULL) {
                g_hash_table_insert (priv->entries, docid, entry);
                priv->all_entries = g_list_prepend (priv->all_entries, entry);
            }
            else {
                g_free (docid);
            }
            g_object_unref (child);
        }
        g_object_unref (children);
        g_object_unref (helpdir);
        g_free (helpdirname);
    }
    for (datadir_i = 0; datadirs[datadir_i]; datadir_i++) {
        for (lang_i = 0; langs[lang_i]; lang_i++) {
            gchar *langdirname = g_build_filename (datadirs[datadir_i], "help", langs[lang_i], NULL);
            GFile *langdir = g_file_new_for_path (langdirname);
            GFileEnumerator *children = g_file_enumerate_children (langdir,
                                                                   G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                                                   G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                                   G_FILE_QUERY_INFO_NONE,
                                                                   NULL, NULL);
            GFileInfo *child;
            if (children == NULL) {
                g_object_unref (langdir);
                g_free (langdirname);
                continue;
            }
            while ((child = g_file_enumerator_next_file (children, NULL, NULL))) {
                gchar *docid, *filename;
                HelpListEntry *entry = NULL;
                if (g_file_info_get_file_type (child) != G_FILE_TYPE_DIRECTORY) {
                    g_object_unref (child);
                    continue;
                }

                docid = g_strconcat ("help:", g_file_info_get_name (child), NULL);
                if (g_hash_table_lookup (priv->entries, docid) != NULL) {
                    g_free (docid);
                    continue;
                }

                filename = g_build_filename (langdirname,
                                            g_file_info_get_name (child),
                                            "index.page",
                                             NULL);
                if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
                    entry = g_new0 (HelpListEntry, 1);
                    entry->id = docid;
                    entry->filename = filename;
                    entry->type = YELP_URI_DOCUMENT_TYPE_MALLARD;
                    goto found;
                }
                g_free (filename);

                filename = g_build_filename (langdirname,
                                            g_file_info_get_name (child),
                                            "index.docbook",
                                             NULL);
                if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
                    entry = g_new0 (HelpListEntry, 1);
                    entry->id = docid;
                    entry->filename = filename;
                    entry->type = YELP_URI_DOCUMENT_TYPE_DOCBOOK;
                    goto found;
                }
                g_free (filename);

            found:
                g_object_unref (child);
                if (entry != NULL) {
                    g_hash_table_insert (priv->entries, docid, entry);
                    priv->all_entries = g_list_prepend (priv->all_entries, entry);
                }
                else {
                    g_free (docid);
                }
            }

            g_object_unref (children);
        }
    }
    g_free (datadirs);

    theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
    for (cur = priv->all_entries; cur != NULL; cur = cur->next) {
        GDesktopAppInfo *app;
        gchar *tmp;
        HelpListEntry *entry = (HelpListEntry *) cur->data;
        const gchar *entryid = strchr (entry->id, ':') + 1;

        if (entry->type == YELP_URI_DOCUMENT_TYPE_MALLARD)
            help_list_process_mallard (list, entry);
        else if (entry->type == YELP_URI_DOCUMENT_TYPE_DOCBOOK)
            help_list_process_docbook (list, entry);

        if (g_str_equal (entryid, "gnome-terminal")) {
            tmp = g_strconcat ("org.gnome.Terminal", ".desktop", NULL);
        }
        else {
            tmp = g_strconcat (entryid, ".desktop", NULL);
        }
        app = g_desktop_app_info_new (tmp);
        g_free (tmp);

        if (app == NULL) {
            char **prefix;
            for (prefix = (char **) known_vendor_prefixes; *prefix; prefix++) {
                tmp = g_strconcat (*prefix, "-", entryid, ".desktop", NULL);
                app = g_desktop_app_info_new (tmp);
                g_free (tmp);
                if (app)
                    break;
            }
        }

        if (app != NULL && theme != NULL) {
            GIcon *icon = g_app_info_get_icon ((GAppInfo *) app);
            if (icon != NULL) {
                GtkIconPaintable *paintable = gtk_icon_theme_lookup_by_gicon (theme, icon,
                                                                              48, 1,
                                                                              GTK_TEXT_DIR_NONE,
                                                                              0);
                GFile *iconfile = gtk_icon_paintable_get_file (paintable);
                if (iconfile) {
                    entry->icon = g_file_get_uri (iconfile);
                    g_object_unref (iconfile);
                }
                g_object_unref (paintable);
            }
        }
    }

    g_mutex_lock (&priv->mutex);
    priv->process_running = FALSE;
    priv->process_ran = TRUE;
    while (priv->pending) {
        gchar *page_id = (gchar *) priv->pending->data;
        help_list_handle_page (list, page_id);
        g_free (page_id);
        priv->pending = g_slist_delete_link (priv->pending, priv->pending);
    }
    g_mutex_unlock (&priv->mutex);

    g_object_unref (list);
}


static void
help_list_reload (YelpHelpList *list)
{
    YelpHelpListPrivate *priv = yelp_help_list_get_instance_private (list);
    gchar **ids;
    gint i;

    if (priv->process_running)
        return;

    g_mutex_lock (&priv->mutex);

    ids = yelp_document_get_requests (YELP_DOCUMENT (list));
    for (i = 0; ids[i]; i++) {
        priv->pending = g_slist_prepend (priv->pending, ids[i]);
    }
    g_free (ids);

    /* entry structs belong to hash table */
    g_list_free (priv->all_entries);
    priv->all_entries = NULL;
    g_hash_table_remove_all (priv->entries);

    yelp_document_clear_contents (YELP_DOCUMENT (list));

    priv->process_ran = FALSE;
    priv->process_running = TRUE;

    g_object_ref (list);
    priv->thread = g_thread_new ("helplist-page",
                                 (GThreadFunc)(GCallback) help_list_think,
                                 list);

    g_mutex_unlock (&priv->mutex);
}


static void
transform_chunk_ready (YelpTransform   *transform,
                       gchar           *chunk_id,
                       YelpHelpList    *list)
{
    gchar *content;
    content = yelp_transform_take_chunk (transform, chunk_id);
    yelp_document_give_contents (YELP_DOCUMENT (list),
                                 chunk_id,
                                 content,
                                 "application/xhtml+xml");
    yelp_document_signal (YELP_DOCUMENT (list),
                          chunk_id,
                          YELP_DOCUMENT_SIGNAL_CONTENTS,
                          NULL);
}


/* This function expects to be called inside a locked mutex */
static void
help_list_handle_page (YelpHelpList *list,
                       const gchar  *page_id)
{
    YelpHelpListPrivate *priv = yelp_help_list_get_instance_private (list);
    xmlNodePtr rootnode;
    GList *entrycur;
    gchar **params = NULL;

    priv->entriesdoc = xmlNewDoc (BAD_CAST "1.0");
    rootnode = xmlNewDocNode (priv->entriesdoc, NULL, BAD_CAST "links", NULL);
    xmlDocSetRootElement (priv->entriesdoc, rootnode);
    xmlNewTextChild (rootnode, NULL, BAD_CAST "title", BAD_CAST _("All Help Documents"));

    priv->all_entries = g_list_sort (priv->all_entries,
                                     (GCompareFunc) help_list_entry_cmp);
    for (entrycur = priv->all_entries; entrycur != NULL; entrycur = entrycur->next) {
        xmlNodePtr linknode, curnode;
        HelpListEntry *entry = (HelpListEntry *) entrycur->data;
        linknode = xmlNewChild (rootnode, NULL, BAD_CAST "link", NULL);
        xmlSetProp (linknode, BAD_CAST "href", BAD_CAST entry->id);
        xmlNewTextChild (linknode, NULL, BAD_CAST "title", BAD_CAST entry->title);
        xmlNewTextChild (linknode, NULL, BAD_CAST "desc", BAD_CAST entry->desc);
        curnode = xmlNewChild (linknode, NULL, BAD_CAST "thumb", NULL);
        xmlSetProp (curnode, BAD_CAST "src", BAD_CAST entry->icon);
    }

    params = yelp_settings_get_all_params (yelp_settings_get_default (), 0, NULL);
    priv->transform = yelp_transform_new (STYLESHEET);

    g_signal_connect (priv->transform, "chunk-ready",
                      (GCallback) transform_chunk_ready,
                      list);

    yelp_transform_start (priv->transform, priv->entriesdoc, NULL,
			  (const gchar * const *) params);

    g_strfreev (params);
}


static void
help_list_process_docbook (YelpHelpList  *list,
                           HelpListEntry *entry)
{
    xmlParserCtxtPtr parserCtxt;
    xmlDocPtr xmldoc;
    xmlXPathContextPtr xpath;
    xmlXPathObjectPtr obj = NULL;
    YelpHelpListPrivate *priv = yelp_help_list_get_instance_private (list);

    parserCtxt = xmlNewParserCtxt ();
    xmldoc = xmlCtxtReadFile (parserCtxt,
                              (const char *) entry->filename, NULL,
                              XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
                              XML_PARSE_NOENT   | XML_PARSE_NONET   );
    xmlFreeParserCtxt (parserCtxt);
    if (xmldoc == NULL)
        return;

    xmlXIncludeProcessFlags (xmldoc,
                             XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
                             XML_PARSE_NOENT   | XML_PARSE_NONET   );

    xpath = xmlXPathNewContext (xmldoc);
    xmlXPathRegisterNs (xpath, BAD_CAST "db",
                        BAD_CAST "http://docbook.org/ns/docbook");
    obj = xmlXPathCompiledEval (priv->get_docbook_title, xpath);
    if (obj) {
        if (obj->stringval)
            entry->title = g_strdup ((const gchar *) obj->stringval);
        xmlXPathFreeObject (obj);
    }

    if (xmldoc)
        xmlFreeDoc (xmldoc);
    if (xpath)
        xmlXPathFreeContext (xpath);
}

static void
help_list_process_mallard (YelpHelpList  *list,
                           HelpListEntry *entry)
{
    xmlParserCtxtPtr parserCtxt;
    xmlDocPtr xmldoc;
    xmlXPathContextPtr xpath;
    xmlXPathObjectPtr obj = NULL;
    YelpHelpListPrivate *priv = yelp_help_list_get_instance_private (list);

    parserCtxt = xmlNewParserCtxt ();
    xmldoc = xmlCtxtReadFile (parserCtxt,
                              (const char *) entry->filename, NULL,
                              XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
                              XML_PARSE_NOENT   | XML_PARSE_NONET   );
    xmlFreeParserCtxt (parserCtxt);
    if (xmldoc == NULL)
        return;

    xmlXIncludeProcessFlags (xmldoc,
                             XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
                             XML_PARSE_NOENT   | XML_PARSE_NONET   );

    xpath = xmlXPathNewContext (xmldoc);
    xmlXPathRegisterNs (xpath, BAD_CAST "mal",
                        BAD_CAST "http://projectmallard.org/1.0/");

    obj = xmlXPathCompiledEval (priv->get_mallard_title, xpath);
    if (obj) {
        if (obj->stringval)
            entry->title = g_strdup ((const gchar *) obj->stringval);
        xmlXPathFreeObject (obj);
    }

    obj = xmlXPathCompiledEval (priv->get_mallard_desc, xpath);
    if (obj) {
        if (obj->stringval)
            entry->desc = g_strdup ((const gchar *) obj->stringval);
        xmlXPathFreeObject (obj);
    }

    if (xmldoc)
        xmlFreeDoc (xmldoc);
    if (xpath)
        xmlXPathFreeContext (xpath);
}
