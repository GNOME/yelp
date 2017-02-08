/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2010 Shaun McCance  <shaunm@gnome.org>
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

G_DEFINE_TYPE (YelpHelpList, yelp_help_list, YELP_TYPE_DOCUMENT)
#define GET_PRIV(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_HELP_LIST, YelpHelpListPrivate))

typedef struct _YelpHelpListPrivate  YelpHelpListPrivate;
struct _YelpHelpListPrivate {
    GMutex         mutex;
    GThread       *thread;

    gboolean process_running;
    gboolean process_ran;

    GHashTable    *entries;
    GList         *all_entries;
    GSList        *pending;

    xmlXPathCompExprPtr  get_docbook_title;
    xmlXPathCompExprPtr  get_mallard_title;
    xmlXPathCompExprPtr  get_mallard_desc;
};

static void
yelp_help_list_class_init (YelpHelpListClass *klass)
{
    GObjectClass      *object_class   = G_OBJECT_CLASS (klass);
    YelpDocumentClass *document_class = YELP_DOCUMENT_CLASS (klass);

    object_class->dispose = yelp_help_list_dispose;
    object_class->finalize = yelp_help_list_finalize;

    document_class->request_page = help_list_request_page;

    g_type_class_add_private (klass, sizeof (YelpHelpListPrivate));
}

static void
yelp_help_list_init (YelpHelpList *list)
{
    YelpHelpListPrivate *priv = GET_PRIV (list);

    g_mutex_init (&priv->mutex);
    priv->entries = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           g_free,
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
    YelpHelpListPrivate *priv = GET_PRIV (object);

    g_hash_table_destroy (priv->entries);
    g_mutex_clear (&priv->mutex);

    if (priv->get_docbook_title)
        xmlXPathFreeCompExpr (priv->get_docbook_title);
    if (priv->get_mallard_title)
        xmlXPathFreeCompExpr (priv->get_mallard_title);
    if (priv->get_mallard_desc)
        xmlXPathFreeCompExpr (priv->get_mallard_desc);

    G_OBJECT_CLASS (yelp_help_list_parent_class)->finalize (object);
}

YelpDocument *
yelp_help_list_new (YelpUri *uri)
{
    return g_object_new (YELP_TYPE_HELP_LIST, NULL);
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
    YelpHelpListPrivate *priv = GET_PRIV (document);

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
                                     (GThreadFunc) help_list_think,
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
    YelpHelpListPrivate *priv = GET_PRIV (list);
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
                    entry->id = g_strdup (docid);
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
                    entry->id = g_strdup (docid);
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
            else
                g_free (docid);
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

                g_free (docid);
            found:
                g_object_unref (child);
                if (entry != NULL) {
                    g_hash_table_insert (priv->entries, docid, entry);
                    priv->all_entries = g_list_prepend (priv->all_entries, entry);
                }
            }

            g_object_unref (children);
        }
    }
    g_free (datadirs);

    theme = gtk_icon_theme_get_default ();
    for (cur = priv->all_entries; cur != NULL; cur = cur->next) {
        GDesktopAppInfo *app;
        gchar *tmp;
        HelpListEntry *entry = (HelpListEntry *) cur->data;
        const gchar *entryid = strchr (entry->id, ':') + 1;

        if (entry->type == YELP_URI_DOCUMENT_TYPE_MALLARD)
            help_list_process_mallard (list, entry);
        else if (entry->type == YELP_URI_DOCUMENT_TYPE_DOCBOOK)
            help_list_process_docbook (list, entry);

        tmp = g_strconcat (entryid, ".desktop", NULL);
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

        if (app != NULL) {
            GIcon *icon = g_app_info_get_icon ((GAppInfo *) app);
            if (icon != NULL) {
                GtkIconInfo *info = gtk_icon_theme_lookup_by_gicon (theme,
                                                                    icon, 22,
                                                                    GTK_ICON_LOOKUP_NO_SVG);
                if (info != NULL) {
                    const gchar *iconfile = gtk_icon_info_get_filename (info);
                    if (iconfile)
                        entry->icon = g_filename_to_uri (iconfile, NULL, NULL);
                    g_object_unref (info);
                }
            }
            g_object_unref (app);
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

/* This function expects to be called inside a locked mutex */
static void
help_list_handle_page (YelpHelpList *list,
                       const gchar  *page_id)
{
    gchar **colors, *tmp;
    GList *cur;
    YelpHelpListPrivate *priv = GET_PRIV (list);
    GtkTextDirection direction = gtk_widget_get_default_direction ();
    GString *string = g_string_new
        ("<html xmlns=\"http://www.w3.org/1999/xhtml\"><head><style type='text/css'>\n"
         "html { height: 100%; }\n"
         "body { margin: 0; padding: 0; max-width: 100%;");
    colors = yelp_settings_get_colors (yelp_settings_get_default ());

    tmp = g_markup_printf_escaped (" background-color: %s; color: %s;"
                                   " direction: %s; }\n",
                                   colors[YELP_SETTINGS_COLOR_BASE],
                                   colors[YELP_SETTINGS_COLOR_TEXT],
                                   (direction == GTK_TEXT_DIR_RTL) ? "rtl" : "ltr");
    g_string_append (string, tmp);
    g_free (tmp);

    g_string_append (string,
                     "div.body { margin: 0 12px 0 12px; padding: 0;"
                     " max-width: 60em; min-height: 20em; }\n"
                     "div.header { max-width: 100%; width: 100%;"
                     " padding: 0; margin: 0 0 1em 0; }\n"
                     "div.footer { max-width: 60em; }\n"
                     "div.sect { margin-top: 1.72em; }\n"
                     "div.trails { margin: 0; padding: 0.2em 12px 0 12px;");

    tmp = g_markup_printf_escaped (" background-color: %s;"
                                   " border-bottom: solid 1px %s; }\n",
                                   colors[YELP_SETTINGS_COLOR_GRAY_BASE],
                                   colors[YELP_SETTINGS_COLOR_GRAY_BORDER]);
    g_string_append (string, tmp);
    g_free (tmp);

    g_string_append (string,
                     "div.trail { margin: 0 1em 0.2em 1em; padding: 0; text-indent: -1em;");

    tmp = g_markup_printf_escaped (" color: %s; }\n",
                                   colors[YELP_SETTINGS_COLOR_TEXT_LIGHT]);
    g_string_append (string, tmp);
    g_free (tmp);

    g_string_append (string,
                     "a.trail { white-space: nowrap; }\n"
                     "div.hgroup { margin: 0 0 0.5em 0;");
    
    tmp = g_markup_printf_escaped (" color: %s;"
                                   " border-bottom: solid 1px %s; }\n",
                                   colors[YELP_SETTINGS_COLOR_TEXT_LIGHT],
                                   colors[YELP_SETTINGS_COLOR_GRAY_BORDER]);
    g_string_append (string, tmp);
    g_free (tmp);

    tmp = g_markup_printf_escaped ("div.title { margin: 0 0 0.2em 0; font-weight: bold;  color: %s; }\n"
                                   "div.desc { margin: 0 0 0.2em 0; }\n"
                                   "div.linkdiv div.inner { padding-%s: 30px; min-height: 24px;"
                                   " background-position: top %s; background-repeat: no-repeat;"
                                   " -webkit-background-size: 22px 22px; }\n"
                                   "div.linkdiv div.title {font-size: 1em; color: inherit; }\n"
                                   "div.linkdiv div.desc { color: %s; }\n"
                                   "div.linkdiv { margin: 0; padding: 0.5em; }\n"
                                   "a:hover div.linkdiv {"
                                   " text-decoration: none;"
                                   " outline: solid 1px %s;"
                                   " background: -webkit-gradient(linear, left top, left 80,"
                                   " from(%s), to(%s)); }\n",
                                   colors[YELP_SETTINGS_COLOR_TEXT_LIGHT],
                                   ((direction == GTK_TEXT_DIR_RTL) ? "right" : "left"),
                                   ((direction == GTK_TEXT_DIR_RTL) ? "right" : "left"),
                                   colors[YELP_SETTINGS_COLOR_TEXT_LIGHT],
                                   colors[YELP_SETTINGS_COLOR_BLUE_BASE],
                                   colors[YELP_SETTINGS_COLOR_BLUE_BASE],
                                   colors[YELP_SETTINGS_COLOR_BASE]);
    g_string_append (string, tmp);
    g_free (tmp);

    g_string_append (string,
                     "h1, h2, h3, h4, h5, h6, h7 { margin: 0; padding: 0; font-weight: bold; }\n"
                     "h1 { font-size: 1.44em; }\n"
                     "h2 { font-size: 1.2em; }"
                     "h3.title, h4.title, h5.title, h6.title, h7.title { font-size: 1.2em; }"
                     "h3, h4, h5, h6, h7 { font-size: 1em; }"
                     "p { line-height: 1.72em; }"
                     "div, pre, p { margin: 1em 0 0 0; padding: 0; }"
                     "div:first-child, pre:first-child, p:first-child { margin-top: 0; }"
                     "div.inner, div.contents, pre.contents { margin-top: 0; }"
                     "p img { vertical-align: middle; }"
                     "a {"
                     "  text-decoration: none;");

    tmp = g_markup_printf_escaped (" color: %s; } a:visited { color: %s; }",
                                   colors[YELP_SETTINGS_COLOR_LINK],
                                   colors[YELP_SETTINGS_COLOR_LINK_VISITED]);
    g_string_append (string, tmp);
    g_free (tmp);

    g_string_append (string,
                     "a:hover { text-decoration: underline; }\n"
                     "a img { border: none; }\n"
                     "</style>\n");

    tmp = g_markup_printf_escaped ("<title>%s</title>",
                                   _("All Help Documents"));
    g_string_append (string, tmp);
    g_free (tmp);

    g_string_append (string,
                     "</head><body>"
                     "<div class='header'></div>"
                     "<div class='body'><div class='hgroup'>");
    tmp = g_markup_printf_escaped ("<h1>%s</h1></div>\n",
                                   _("All Help Documents"));
    g_string_append (string, tmp);
    g_free (tmp);

    priv->all_entries = g_list_sort (priv->all_entries,
                                     (GCompareFunc) help_list_entry_cmp);
    for (cur = priv->all_entries; cur != NULL; cur = cur->next) {
        HelpListEntry *entry = (HelpListEntry *) cur->data;
        gchar *title = entry->title ? entry->title : (strchr (entry->id, ':') + 1);
        const gchar *desc = entry->desc ? entry->desc : "";

        tmp = g_markup_printf_escaped ("<a href='%s'><div class='linkdiv'>",
                                       entry->id);
        g_string_append (string, tmp);
        g_free (tmp);

        if (entry->icon) {
            tmp = g_markup_printf_escaped ("<div class='inner' style='background-image: url(%s);'>",
                                           entry->icon);
            g_string_append (string, tmp);
            g_free (tmp);
        }
        else
            g_string_append (string, "<div class='inner'>");

        tmp = g_markup_printf_escaped ("<div class='title'>%s</div>"
                                       "<div class='desc'>%s</div>"
                                       "</div></div></a>",
                                       title, desc);
        g_string_append (string, tmp);
        g_free (tmp);
    }

    g_string_append (string,
                     "</div>"
                     "<div class='footer'></div>"
                     "</body></html>");

    yelp_document_give_contents (YELP_DOCUMENT (list), page_id,
                                 string->str,
                                 "application/xhtml+xml");
    g_strfreev (colors);
    g_string_free (string, FALSE);
    yelp_document_signal (YELP_DOCUMENT (list), page_id,
                          YELP_DOCUMENT_SIGNAL_CONTENTS, NULL);
}


static void
help_list_process_docbook (YelpHelpList  *list,
                           HelpListEntry *entry)
{
    xmlParserCtxtPtr parserCtxt;
    xmlDocPtr xmldoc;
    xmlXPathContextPtr xpath;
    xmlXPathObjectPtr obj = NULL;
    YelpHelpListPrivate *priv = GET_PRIV (list);

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
    YelpHelpListPrivate *priv = GET_PRIV (list);

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
