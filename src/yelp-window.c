/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2010 Shaun McCance <shaunm@gnome.org>
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
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "yelp-location-entry.h"
#include "yelp-settings.h"
#include "yelp-uri.h"
#include "yelp-view.h"

#include "yelp-application.h"
#include "yelp-window.h"

static void          yelp_window_init             (YelpWindow         *window);
static void          yelp_window_class_init       (YelpWindowClass    *klass);
static void          yelp_window_dispose          (GObject            *object);
static void          yelp_window_finalize         (GObject            *object);

static void          entry_location_selected      (YelpLocationEntry  *entry,
                                                   YelpWindow         *window);

static void          back_button_clicked          (GtkWidget          *button,
                                                   YelpWindow         *window);

static void          view_uri_selected            (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);
static void          view_page_title              (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);
static void          view_page_desc               (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);

G_DEFINE_TYPE (YelpWindow, yelp_window, GTK_TYPE_WINDOW);
#define GET_PRIV(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_WINDOW, YelpWindowPrivate))

enum {
  COL_TITLE,
  COL_DESC,
  COL_ICON,
  COL_FLAGS,
  COL_URI,
  COL_TERMS
};

typedef struct _YelpBackEntry YelpBackEntry;
struct _YelpBackEntry {
    YelpUri *uri;
    gchar *title;
    gchar *desc;
};
static void
back_entry_free (YelpBackEntry *back)
{
    if (back == NULL)
        return;
    g_object_unref (back->uri);
    g_free (back->title);
    g_free (back->desc);
    g_free (back);
}

typedef struct _YelpWindowPrivate YelpWindowPrivate;
struct _YelpWindowPrivate {
    GtkListStore *history;

    /* no refs on these, owned by containers */
    YelpView *view;
    YelpLocationEntry *entry;
    GtkWidget *back_button;

    GSList *back_list;
    gboolean back_load;

    gulong entry_location_selected;
};

static void
yelp_window_init (YelpWindow *window)
{
    GtkWidget *vbox, *hbox, *scroll, *align;;
    YelpWindowPrivate *priv = GET_PRIV (window);

    gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_UTILITY);
    gtk_window_set_default_size (GTK_WINDOW (window), 520, 580);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (window), vbox);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    priv->back_button = (GtkWidget *) gtk_tool_button_new_from_stock (GTK_STOCK_GO_BACK);
    g_signal_connect (priv->back_button, "clicked", back_button_clicked, window);
    gtk_box_pack_start (GTK_BOX (hbox), priv->back_button, FALSE, FALSE, 0);
    
    priv->history = gtk_list_store_new (6,
                                        G_TYPE_STRING,  /* title */
                                        G_TYPE_STRING,  /* desc */
                                        G_TYPE_STRING,  /* icon */
                                        G_TYPE_INT,     /* flags */
                                        G_TYPE_STRING,  /* uri */
                                        G_TYPE_STRING   /* search terms */
                                        );
    priv->entry = (YelpLocationEntry *)
        yelp_location_entry_new_with_model (GTK_TREE_MODEL (priv->history),
                                            COL_TITLE,
                                            COL_DESC,
                                            COL_ICON,
                                            COL_FLAGS);
    priv->entry_location_selected = g_signal_connect (priv->entry, "location-selected",
                                                      G_CALLBACK (entry_location_selected), window);
    align = gtk_alignment_new (0.0, 0.5, 1.0, 0.0);
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (align), TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (align), GTK_WIDGET (priv->entry));

    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_box_pack_end (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);

    priv->view = (YelpView *) yelp_view_new ();
    g_signal_connect (priv->view, "notify::yelp-uri", G_CALLBACK (view_uri_selected), window);
    g_signal_connect (priv->view, "notify::page-title", G_CALLBACK (view_page_title), window);
    g_signal_connect (priv->view, "notify::page-desc", G_CALLBACK (view_page_desc), window);
    gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (priv->view));
    gtk_widget_grab_focus (GTK_WIDGET (priv->view));
}

static void
yelp_window_class_init (YelpWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = yelp_window_dispose;
    object_class->finalize = yelp_window_finalize;

    g_type_class_add_private (klass, sizeof (YelpWindowPrivate));
}

static void
yelp_window_dispose (GObject *object)
{
    YelpWindowPrivate *priv = GET_PRIV (object);

    if (priv->history) {
        g_object_unref (priv->history);
        priv->history = NULL;
    }

    while (priv->back_list) {
        back_entry_free ((YelpBackEntry *) priv->back_list->data);
        priv->back_list = g_slist_delete_link (priv->back_list, priv->back_list);
    }

    G_OBJECT_CLASS (yelp_window_parent_class)->dispose (object);
}

static void
yelp_window_finalize (GObject *object)
{
    G_OBJECT_CLASS (yelp_window_parent_class)->finalize (object);
}


/******************************************************************************/

YelpWindow *
yelp_window_new (YelpApplication *app)
{
    YelpWindow *window;

    window = (YelpWindow *) g_object_new (YELP_TYPE_WINDOW, NULL);

    return window;
}

void
yelp_window_load_uri (YelpWindow  *window,
                      YelpUri     *uri)
{
    YelpWindowPrivate *priv = GET_PRIV (window);

    yelp_view_load_uri (priv->view, uri);
}

/******************************************************************************/

static void
entry_location_selected (YelpLocationEntry  *entry,
                         YelpWindow         *window)
{
    GtkTreeIter iter;
    gchar *uri;
    YelpWindowPrivate *priv = GET_PRIV (window);

    gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->entry), &iter);
    gtk_tree_model_get (GTK_TREE_MODEL (priv->history), &iter,
                        COL_URI, &uri,
                        -1);
    yelp_view_load (priv->view, uri);
    g_free (uri);
}

static void
back_button_clicked (GtkWidget  *button,
                     YelpWindow *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);

    if (priv->back_list == NULL)
        return;

    back_entry_free ((YelpBackEntry *) priv->back_list->data);
    priv->back_list = g_slist_delete_link (priv->back_list, priv->back_list);

    if (priv->back_list == NULL || priv->back_list->data == NULL)
        return;

    priv->back_load = TRUE;
    yelp_window_load_uri (window, ((YelpBackEntry *) priv->back_list->data)->uri);
}

static void
view_uri_selected (YelpView     *view,
                   GParamSpec   *pspec,
                   YelpWindow   *window)
{
    GtkTreeIter iter;
    gchar *iter_uri;
    gboolean cont;
    YelpUri *uri;
    gchar *struri;
    YelpBackEntry *back;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (G_OBJECT (view), "yelp-uri", &uri, NULL);
    if (uri == NULL)
        return;

    back = g_new0 (YelpBackEntry, 1);
    back->uri = g_object_ref (uri);
    if (!priv->back_load)
        priv->back_list = g_slist_prepend (priv->back_list, back);
    priv->back_load = FALSE;
    gtk_widget_set_sensitive (priv->back_button, FALSE);
    gtk_widget_set_tooltip_text (priv->back_button, "");
    if (priv->back_list->next && priv->back_list->next->data) {
        gchar *tooltip = "";
        YelpBackEntry *back = priv->back_list->next->data;

        gtk_widget_set_sensitive (priv->back_button, TRUE);
        if (back->title && back->desc) {
            gchar *color;
            color = yelp_settings_get_color (yelp_settings_get_default (),
                                             YELP_SETTINGS_COLOR_TEXT_LIGHT);
            tooltip = g_markup_printf_escaped ("<span size='larger'>%s</span>\n<span color='%s'>%s</span>",
                                               back->title, color, back->desc);
            g_free (color);
        }
        else if (back->title)
            tooltip = g_markup_printf_escaped ("<span size='larger'>%s</span>",
                                               back->title);
        gtk_widget_set_tooltip_markup (priv->back_button, tooltip);
    }

    struri = yelp_uri_get_canonical_uri (uri);

    cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &iter);
    while (cont) {
        gtk_tree_model_get (GTK_TREE_MODEL (priv->history), &iter,
                            COL_URI, &iter_uri,
                            -1);
        if (iter_uri && g_str_equal (struri, iter_uri)) {
            g_free (iter_uri);
            break;
        }
        else {
            g_free (iter_uri);
            cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->history), &iter);
        }
    }
    if (cont) {
        GtkTreeIter first;
        gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &first);
        gtk_list_store_move_before (priv->history, &iter, &first);
    }
    else {
        gtk_list_store_prepend (priv->history, &iter);
        gtk_list_store_set (priv->history, &iter,
                            COL_TITLE, _("Loading"),
                            COL_ICON, "help-browser",
                            COL_URI, struri,
                            -1);
    }
    g_signal_handler_block (priv->entry,
                            priv->entry_location_selected);
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->entry), &iter);
    g_signal_handler_unblock (priv->entry,
                              priv->entry_location_selected);

    g_free (struri);
    g_object_unref (uri);
}

static void
view_page_title (YelpView    *view,
                 GParamSpec  *pspec,
                 YelpWindow  *window)
{
    GtkTreeIter first;
    gchar *title, *frag;
    YelpUri *uri;
    YelpBackEntry *back = NULL;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (view, "page-title", &title, NULL);
    if (title == NULL)
        return;

    g_object_get (view, "yelp-uri", &uri, NULL);
    frag = yelp_uri_get_frag_id (uri);

    if (priv->back_list)
        back = priv->back_list->data;

    gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &first);
    if (frag) {
        gchar *tmp = g_strdup_printf ("%s (#%s)", title, frag);
        gtk_list_store_set (priv->history, &first,
                            COL_TITLE, tmp,
                            -1);
        if (back) {
            g_free (back->title);
            back->title = tmp;
        }
        else {
            g_free (tmp);
        }
        g_free (frag);
    }
    else {
        gtk_list_store_set (priv->history, &first,
                            COL_TITLE, title,
                            -1);
        if (back) {
            g_free (back->title);
            back->title = g_strdup (title);
        }
    }

    g_object_unref (uri);
}

static void
view_page_desc (YelpView    *view,
                GParamSpec  *pspec,
                YelpWindow  *window)
{
    GtkTreeIter first;
    gchar *desc;
    YelpBackEntry *back = NULL;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (view, "page-desc", &desc, NULL);
    if (desc == NULL)
        return;

    if (priv->back_list)
        back = priv->back_list->data;

    gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &first);
    gtk_list_store_set (priv->history, &first,
                        COL_DESC, desc,
                        -1);
    if (back) {
        g_free (back->desc);
        back->desc = g_strdup (desc);
    }
}
