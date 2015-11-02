/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009-2014 Shaun McCance <shaunm@gnome.org>
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
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "yelp-search-entry.h"
#include "yelp-marshal.h"
#include "yelp-settings.h"

static void     search_entry_constructed     (GObject           *object);
static void     search_entry_dispose         (GObject           *object);
static void     search_entry_finalize        (GObject           *object);
static void     search_entry_get_property    (GObject           *object,
					      guint              prop_id,
					      GValue            *value,
					      GParamSpec        *pspec);
static void     search_entry_set_property    (GObject           *object,
					      guint              prop_id,
					      const GValue      *value,
					      GParamSpec        *pspec);

/* Signals */
static void     search_entry_search_activated  (YelpSearchEntry *entry);
static void     search_entry_bookmark_clicked  (YelpSearchEntry *entry);


/* Utilities */
static void     search_entry_set_completion  (YelpSearchEntry *entry,
                                                GtkTreeModel      *model);


/* GtkEntry callbacks */
static void     entry_activate_cb                   (GtkEntry          *text_entry,
                                                     gpointer           user_data);

/* GtkEntryCompletion callbacks */
static void     cell_set_completion_bookmark_icon   (GtkCellLayout     *layout,
                                                     GtkCellRenderer   *cell,
                                                     GtkTreeModel      *model,
                                                     GtkTreeIter       *iter,
                                                     YelpSearchEntry *entry);
static void     cell_set_completion_text_cell       (GtkCellLayout     *layout,
                                                     GtkCellRenderer   *cell,
                                                     GtkTreeModel      *model,
                                                     GtkTreeIter       *iter,
                                                     YelpSearchEntry *entry);
static gboolean entry_match_func                    (GtkEntryCompletion *completion,
                                                     const gchar        *key,
                                                     GtkTreeIter        *iter,
                                                     YelpSearchEntry  *entry);
static gint     entry_completion_sort               (GtkTreeModel       *model,
                                                     GtkTreeIter        *iter1,
                                                     GtkTreeIter        *iter2,
                                                     gpointer            user_data);
static gboolean entry_match_selected                (GtkEntryCompletion *completion,
                                                     GtkTreeModel       *model,
                                                     GtkTreeIter        *iter,
                                                     YelpSearchEntry  *entry);
/* YelpView callbacks */
static void          view_loaded                    (YelpView           *view,
                                                     YelpSearchEntry    *entry);


typedef struct _YelpSearchEntryPrivate  YelpSearchEntryPrivate;
struct _YelpSearchEntryPrivate
{
    YelpView *view;
    YelpBookmarks *bookmarks;
    gchar *completion_uri;

    /* do not free below */
    GtkEntryCompletion *completion;
};

enum {
    COMPLETION_COL_TITLE,
    COMPLETION_COL_DESC,
    COMPLETION_COL_ICON,
    COMPLETION_COL_PAGE,
    COMPLETION_COL_FLAGS
};

enum {
    COMPLETION_FLAG_ACTIVATE_SEARCH = 1<<0
};

enum {
    SEARCH_ACTIVATED,
    LAST_SIGNAL
};

enum {  
    PROP_0,
    PROP_VIEW,
    PROP_BOOKMARKS
};

static GHashTable *completions;

static guint search_entry_signals[LAST_SIGNAL] = {0,};

G_DEFINE_TYPE (YelpSearchEntry, yelp_search_entry, GTK_TYPE_SEARCH_ENTRY)
#define GET_PRIV(object) (G_TYPE_INSTANCE_GET_PRIVATE((object), YELP_TYPE_SEARCH_ENTRY, YelpSearchEntryPrivate))

static void
yelp_search_entry_class_init (YelpSearchEntryClass *klass)
{
    GObjectClass *object_class;

    klass->search_activated = search_entry_search_activated;
    klass->bookmark_clicked = search_entry_bookmark_clicked;

    object_class = G_OBJECT_CLASS (klass);
  
    object_class->constructed = search_entry_constructed;
    object_class->dispose = search_entry_dispose;
    object_class->finalize = search_entry_finalize;
    object_class->get_property = search_entry_get_property;
    object_class->set_property = search_entry_set_property;

    /**
     * YelpSearchEntry::search-activated
     * @widget: The #YelpLocationEntry for which the signal was emitted.
     * @text: The search text.
     * @user_data: User data set when the handler was connected.
     *
     * This signal will be emitted whenever the user activates a search, generally
     * by pressing <keycap>Enter</keycap> in the embedded text entry while @widget
     * is in search mode.
     **/
    search_entry_signals[SEARCH_ACTIVATED] =
        g_signal_new ("search-activated",
                      G_OBJECT_CLASS_TYPE (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (YelpSearchEntryClass, search_activated),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE, 1,
                      G_TYPE_STRING);

    /**
     * YelpLocationEntry:view
     *
     * The YelpView instance that this location entry controls.
     **/
    g_object_class_install_property (object_class,
                                     PROP_VIEW,
                                     g_param_spec_object ("view",
							  "View",
							  "A YelpView instance to control",
                                                          YELP_TYPE_VIEW,
                                                          G_PARAM_CONSTRUCT_ONLY |
							  G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS));

    /**
     * YelpLocationEntry:bookmarks
     *
     * An instance of an implementation of YelpBookmarks to provide
     * bookmark information for this location entry.
     **/
    g_object_class_install_property (object_class,
                                     PROP_BOOKMARKS,
                                     g_param_spec_object ("bookmarks",
							  "Bookmarks",
							  "A YelpBookmarks implementation instance",
                                                          YELP_TYPE_BOOKMARKS,
                                                          G_PARAM_CONSTRUCT_ONLY |
							  G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS));

    g_type_class_add_private ((GObjectClass *) klass,
                              sizeof (YelpSearchEntryPrivate));

    completions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

static void
yelp_search_entry_init (YelpSearchEntry *entry)
{
}

static void
search_entry_constructed (GObject *object)
{
    YelpSearchEntryPrivate *priv = GET_PRIV (object);

    g_signal_connect (object, "activate",
                      G_CALLBACK (entry_activate_cb), object);

    g_signal_connect (priv->view, "loaded", G_CALLBACK (view_loaded), object);
}

static void
search_entry_dispose (GObject *object)
{
    YelpSearchEntryPrivate *priv = GET_PRIV (object);

    if (priv->view) {
        g_object_unref (priv->view);
        priv->view = NULL;
    }

    if (priv->bookmarks) {
        g_object_unref (priv->bookmarks);
        priv->bookmarks = NULL;
    }

    G_OBJECT_CLASS (yelp_search_entry_parent_class)->dispose (object);
}

static void
search_entry_finalize (GObject *object)
{
    YelpSearchEntryPrivate *priv = GET_PRIV (object);

    g_free (priv->completion_uri);

    G_OBJECT_CLASS (yelp_search_entry_parent_class)->finalize (object);
}

static void
search_entry_get_property   (GObject      *object,
                               guint         prop_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
    YelpSearchEntryPrivate *priv = GET_PRIV (object);

    switch (prop_id) {
    case PROP_VIEW:
        g_value_set_object (value, priv->view);
        break;
    case PROP_BOOKMARKS:
        g_value_set_object (value, priv->bookmarks);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
search_entry_set_property   (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    YelpSearchEntryPrivate *priv = GET_PRIV (object);

    switch (prop_id) {
    case PROP_VIEW:
        priv->view = g_value_dup_object (value);
        break;
    case PROP_BOOKMARKS:
        priv->bookmarks = g_value_dup_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
search_entry_search_activated  (YelpSearchEntry *entry)
{
    YelpUri *base, *uri;
    YelpSearchEntryPrivate *priv = GET_PRIV (entry);

    g_object_get (priv->view, "yelp-uri", &base, NULL);
    if (base == NULL)
        return;
    uri = yelp_uri_new_search (base,
                               gtk_entry_get_text (GTK_ENTRY (entry)));
    g_object_unref (base);
    yelp_view_load_uri (priv->view, uri);
    gtk_widget_grab_focus (GTK_WIDGET (priv->view));
}

static void
search_entry_bookmark_clicked  (YelpSearchEntry *entry)
{
    YelpUri *uri;
    gchar *doc_uri, *page_id;
    YelpSearchEntryPrivate *priv = GET_PRIV (entry);

    g_object_get (priv->view,
                  "yelp-uri", &uri,
                  "page-id", &page_id,
                  NULL);
    doc_uri = yelp_uri_get_document_uri (uri);
    if (priv->bookmarks && doc_uri && page_id) {
        if (!yelp_bookmarks_is_bookmarked (priv->bookmarks, doc_uri, page_id)) {
            gchar *icon, *title;
            g_object_get (priv->view,
                          "page-icon", &icon,
                          "page-title", &title,
                          NULL);
            yelp_bookmarks_add_bookmark (priv->bookmarks, doc_uri, page_id, icon, title);
            g_free (icon);
            g_free (title);
        }
        else {
            yelp_bookmarks_remove_bookmark (priv->bookmarks, doc_uri, page_id);
        }
    }
    g_free (doc_uri);
    g_free (page_id);
    g_object_unref (uri);
}

static void
search_entry_set_completion (YelpSearchEntry *entry,
			     GtkTreeModel    *model)
{
    YelpSearchEntryPrivate *priv = GET_PRIV (entry);
    GList *cells;
    GtkCellRenderer *icon_cell, *bookmark_cell;

    priv->completion = gtk_entry_completion_new ();
    gtk_entry_completion_set_minimum_key_length (priv->completion, 3);
    gtk_entry_completion_set_model (priv->completion, model);
    gtk_entry_completion_set_text_column (priv->completion, COMPLETION_COL_TITLE);
    gtk_entry_completion_set_match_func (priv->completion,
                                         (GtkEntryCompletionMatchFunc) entry_match_func,
                                         entry, NULL);
    g_signal_connect (priv->completion, "match-selected",
                      G_CALLBACK (entry_match_selected), entry);

    cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (priv->completion));
    g_object_set (cells->data, "xpad", 4, NULL);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (priv->completion),
                                        GTK_CELL_RENDERER (cells->data),
                                        (GtkCellLayoutDataFunc) cell_set_completion_text_cell,
                                        entry, NULL);
    g_object_set (cells->data, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    g_list_free (cells);

    icon_cell = gtk_cell_renderer_pixbuf_new ();
    g_object_set (icon_cell, "yalign", 0.2, NULL);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->completion), icon_cell, FALSE);
    gtk_cell_layout_reorder (GTK_CELL_LAYOUT (priv->completion), icon_cell, 0);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->completion),
                                    icon_cell,
                                    "icon-name",
                                    COMPLETION_COL_ICON,
                                    NULL);
    if (priv->bookmarks) {
        bookmark_cell = gtk_cell_renderer_pixbuf_new ();
        gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (priv->completion), bookmark_cell, FALSE);
        gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (priv->completion),
                                            bookmark_cell,
                                            (GtkCellLayoutDataFunc) cell_set_completion_bookmark_icon,
                                            entry, NULL);
    }
    gtk_entry_set_completion (GTK_ENTRY (entry), priv->completion);
}

static void
entry_activate_cb (GtkEntry  *text_entry,
                   gpointer   user_data)
{
    gchar *text = g_strdup (gtk_entry_get_text (text_entry));

    if (text == NULL || strlen(text) == 0)
        return;

    g_signal_emit (user_data, search_entry_signals[SEARCH_ACTIVATED], 0, text);

    g_free (text);
}

static void
cell_set_completion_bookmark_icon (GtkCellLayout     *layout,
                                   GtkCellRenderer   *cell,
                                   GtkTreeModel      *model,
                                   GtkTreeIter       *iter,
                                   YelpSearchEntry *entry)
{
    YelpSearchEntryPrivate *priv = GET_PRIV (entry);

    if (priv->completion_uri) {
        gchar *page_id = NULL;
        gtk_tree_model_get (model, iter,
                            COMPLETION_COL_PAGE, &page_id,
                            -1);

        if (page_id && yelp_bookmarks_is_bookmarked (priv->bookmarks,
                                                     priv->completion_uri, page_id))
            g_object_set (cell, "icon-name", "user-bookmarks-symbolic", NULL);
        else
            g_object_set (cell, "icon-name", NULL, NULL);

        g_free (page_id);
    }
}

static void
cell_set_completion_text_cell (GtkCellLayout     *layout,
                               GtkCellRenderer   *cell,
                               GtkTreeModel      *model,
                               GtkTreeIter       *iter,
                               YelpSearchEntry *entry)
{
    gchar *title;
    gint flags;

    gtk_tree_model_get (model, iter, COMPLETION_COL_FLAGS, &flags, -1);
    if (flags & COMPLETION_FLAG_ACTIVATE_SEARCH) {
        title = g_strdup_printf (_("Search for “%s”"),
                                 gtk_entry_get_text (GTK_ENTRY (entry)));
        g_object_set (cell, "text", title, NULL);
        g_free (title);
        return;
    }

    gtk_tree_model_get (model, iter,
                        COMPLETION_COL_TITLE, &title,
                        -1);
    g_object_set (cell, "text", title, NULL);
    g_free (title);
}

static gboolean
entry_match_func (GtkEntryCompletion *completion,
                  const gchar        *key,
                  GtkTreeIter        *iter,
                  YelpSearchEntry  *entry)
{
    gint stri;
    gchar *title, *desc, *titlecase = NULL, *desccase = NULL;
    gboolean ret = FALSE;
    gchar **strs;
    gint flags;
    GtkTreeModel *model = gtk_entry_completion_get_model (completion);
    static GRegex *nonword = NULL;

    if (nonword == NULL)
        nonword = g_regex_new ("\\W", 0, 0, NULL);
    if (nonword == NULL)
        return FALSE;

    gtk_tree_model_get (model, iter, COMPLETION_COL_FLAGS, &flags, -1);
    if (flags & COMPLETION_FLAG_ACTIVATE_SEARCH)
        return TRUE;

    gtk_tree_model_get (model, iter,
                        COMPLETION_COL_TITLE, &title,
                        COMPLETION_COL_DESC, &desc,
                        -1);
    if (title) {
        titlecase = g_utf8_casefold (title, -1);
        g_free (title);
    }
    if (desc) {
        desccase = g_utf8_casefold (desc, -1);
        g_free (desc);
    }

    strs = g_regex_split (nonword, key, 0);
    ret = TRUE;
    for (stri = 0; strs[stri]; stri++) {
        if (!titlecase || !strstr (titlecase, strs[stri])) {
            if (!desccase || !strstr (desccase, strs[stri])) {
                ret = FALSE;
                break;
            }
        }
    }

    g_free (titlecase);
    g_free (desccase);
    g_strfreev (strs);

    return ret;
}

static gint
entry_completion_sort (GtkTreeModel *model,
                       GtkTreeIter  *iter1,
                       GtkTreeIter  *iter2,
                       gpointer      user_data)
{
    gint ret = 0;
    gint flags1, flags2;
    gchar *key1, *key2;

    gtk_tree_model_get (model, iter1, COMPLETION_COL_FLAGS, &flags1, -1);
    gtk_tree_model_get (model, iter2, COMPLETION_COL_FLAGS, &flags2, -1);
    if (flags1 & COMPLETION_FLAG_ACTIVATE_SEARCH)
        return 1;
    else if (flags2 & COMPLETION_FLAG_ACTIVATE_SEARCH)
        return -1;

    gtk_tree_model_get (model, iter1, COMPLETION_COL_ICON, &key1, -1);
    gtk_tree_model_get (model, iter2, COMPLETION_COL_ICON, &key2, -1);
    ret = yelp_settings_cmp_icons (key1, key2);
    g_free (key1);
    g_free (key2);

    if (ret)
        return ret;

    gtk_tree_model_get (model, iter1, COMPLETION_COL_TITLE, &key1, -1);
    gtk_tree_model_get (model, iter2, COMPLETION_COL_TITLE, &key2, -1);
    if (key1 && key2)
        ret = g_utf8_collate (key1, key2);
    else if (key2 == NULL)
        return -1;
    else if (key1 == NULL)
        return 1;
    g_free (key1);
    g_free (key2);

    return ret;
}

static gboolean
entry_match_selected (GtkEntryCompletion *completion,
                      GtkTreeModel       *model,
                      GtkTreeIter        *iter,
                      YelpSearchEntry    *entry)
{
    YelpUri *base, *uri;
    gchar *page, *xref;
    gint flags;
    YelpSearchEntryPrivate *priv = GET_PRIV (entry);

    gtk_tree_model_get (model, iter, COMPLETION_COL_FLAGS, &flags, -1);
    if (flags & COMPLETION_FLAG_ACTIVATE_SEARCH) {
        entry_activate_cb (GTK_ENTRY (entry), entry);
        return TRUE;
    }

    g_object_get (priv->view, "yelp-uri", &base, NULL);
    gtk_tree_model_get (model, iter, COMPLETION_COL_PAGE, &page, -1);

    xref = g_strconcat ("xref:", page, NULL);
    uri = yelp_uri_new_relative (base, xref);

    yelp_view_load_uri (priv->view, uri);

    g_free (page);
    g_free (xref);
    g_object_unref (uri);
    g_object_unref (base);

    gtk_widget_grab_focus (GTK_WIDGET (priv->view));
    return TRUE;
}

static void
view_loaded (YelpView          *view,
             YelpSearchEntry *entry)
{
    gchar **ids;
    gint i;
    GtkTreeIter iter;
    YelpUri *uri;
    gchar *doc_uri;
    GtkTreeModel *completion;
    YelpSearchEntryPrivate *priv = GET_PRIV (entry);
    YelpDocument *document = yelp_view_get_document (view);

    g_object_get (view, "yelp-uri", &uri, NULL);
    doc_uri = yelp_uri_get_document_uri (uri);

    if ((priv->completion_uri == NULL) || 
        !g_str_equal (doc_uri, priv->completion_uri)) {
        completion = (GtkTreeModel *) g_hash_table_lookup (completions, doc_uri);
        if (completion == NULL) {
            GtkListStore *base = gtk_list_store_new (5,
                                                     G_TYPE_STRING,  /* title */
                                                     G_TYPE_STRING,  /* desc */
                                                     G_TYPE_STRING,  /* icon */
                                                     G_TYPE_STRING,  /* uri */
                                                     G_TYPE_INT      /* flags */
                                                     );
            completion = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (base));
            gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (completion),
                                                     entry_completion_sort,
                                                     NULL, NULL);
            g_hash_table_insert (completions, g_strdup (doc_uri), completion);
            if (document != NULL) {
                ids = yelp_document_list_page_ids (document);
                for (i = 0; ids[i]; i++) {
                    gchar *title, *desc, *icon;
                    gtk_list_store_insert (GTK_LIST_STORE (base), &iter, 0);
                    title = yelp_document_get_page_title (document, ids[i]);
                    desc = yelp_document_get_page_desc (document, ids[i]);
                    icon = yelp_document_get_page_icon (document, ids[i]);
                    gtk_list_store_set (base, &iter,
                                        COMPLETION_COL_TITLE, title,
                                        COMPLETION_COL_DESC, desc,
                                        COMPLETION_COL_ICON, icon,
                                        COMPLETION_COL_PAGE, ids[i],
                                        -1);
                    g_free (icon);
                    g_free (desc);
                    g_free (title);
                }
                g_strfreev (ids);
                gtk_list_store_insert (GTK_LIST_STORE (base), &iter, 0);
                gtk_list_store_set (base, &iter,
                                    COMPLETION_COL_ICON, "edit-find-symbolic",
                                    COMPLETION_COL_FLAGS, COMPLETION_FLAG_ACTIVATE_SEARCH,
                                    -1);
            }
            g_object_unref (base);
        }
        g_free (priv->completion_uri);
        priv->completion_uri = doc_uri;
        search_entry_set_completion (entry, completion);
    }

    g_object_unref (uri);
}

/**
 * yelp_search_entry_new:
 * @view: A #YelpView.
 *
 * Creates a new #YelpSearchEntry widget to control @view.
 *
 * Returns: A new #YelpSearchEntry.
 **/
GtkWidget*
yelp_search_entry_new (YelpView      *view,
		       YelpBookmarks *bookmarks)
{
    GtkWidget *ret;
    g_return_val_if_fail (YELP_IS_VIEW (view), NULL);

    ret = GTK_WIDGET (g_object_new (YELP_TYPE_SEARCH_ENTRY,
                                    "view", view,
                                    "bookmarks", bookmarks,
                                    NULL));

    return ret;
}
