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

#include <gdk/gdkkeysyms.h>
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
static void          yelp_window_get_property     (GObject            *object,
                                                   guint               prop_id,
                                                   GValue             *value,
                                                   GParamSpec         *pspec);
static void          yelp_window_set_property     (GObject            *object,
                                                   guint               prop_id,
                                                   const GValue       *value,
                                                   GParamSpec         *pspec);

static void          window_new                   (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_close                 (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_open_location         (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_font_adjustment       (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_show_text_cursor      (GtkToggleAction    *action,
                                                   YelpWindow         *window);
static void          window_go_back               (GtkAction          *action,
                                                   YelpWindow         *window);
static void          window_go_forward            (GtkAction          *action,
                                                   YelpWindow         *window);

static void          entry_location_selected      (YelpLocationEntry  *entry,
                                                   YelpWindow         *window);
static void          entry_completion_selected    (YelpLocationEntry  *entry,
                                                   GtkTreeModel       *model,
                                                   GtkTreeIter        *iter,
                                                   YelpWindow         *window);
static gboolean      entry_focus_out              (YelpLocationEntry  *entry,
                                                   GdkEventFocus      *event,
                                                   YelpWindow         *window);

static void          view_external_uri            (YelpView           *view,
                                                   YelpUri            *uri,
                                                   YelpWindow         *window);
static void          view_loaded                  (YelpView           *view,
                                                   YelpWindow         *window);
static void          view_uri_selected            (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);
static void          view_root_title              (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);
static void          view_page_title              (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);
static void          view_page_desc               (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);
static void          view_page_icon               (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   YelpWindow         *window);

static void          hidden_entry_activate        (GtkEntry           *entry,
                                                   YelpWindow         *window);
static void          hidden_entry_hide            (YelpWindow         *window);
static gboolean      hidden_key_press             (GtkWidget          *widget,
                                                   GdkEventKey        *event,
                                                   YelpWindow         *window);

enum {
    PROP_0,
    PROP_APPLICATION
};

G_DEFINE_TYPE (YelpWindow, yelp_window, GTK_TYPE_WINDOW);
#define GET_PRIV(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_WINDOW, YelpWindowPrivate))

enum {
  COL_TITLE,
  COL_DESC,
  COL_ICON,
  COL_URI,
  COL_FLAGS,
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
    GtkListStore *completion;
    GtkActionGroup *action_group;
    YelpApplication *application;

    /* no refs on these, owned by containers */
    YelpView *view;
    GtkWidget *hbox;
    GtkWidget *back_button;
    GtkWidget *forward_button;
    YelpLocationEntry *entry;
    GtkWidget *hidden_entry;

    /* refs because we dynamically add & remove */
    GtkWidget *align_location;
    GtkWidget *align_hidden;

    GList *back_list;
    GList *back_cur;
    gboolean back_load;

    gulong entry_location_selected;
};

static const GtkActionEntry entries[] = {
    { "PageMenu",      NULL, N_("_Page")      },
    { "ViewMenu",      NULL, N_("_View")      },
    { "GoMenu",        NULL, N_("_Go")        },

    { "NewWindow", GTK_STOCK_NEW,
      N_("_New Window"),
      "<Control>N",
      NULL,
      G_CALLBACK (window_new) },
    { "CloseWindow", GTK_STOCK_CLOSE,
      N_("_Close"),
      "<Control>W",
      NULL,
      G_CALLBACK (window_close) },
    { "OpenLocation", NULL,
      N_("Open Location"),
      "<Control>L",
      NULL,
      G_CALLBACK (window_open_location) },
    { "LargerText", NULL,
      N_("_Larger Text"),
      "<Control>plus",
      NULL,
      G_CALLBACK (window_font_adjustment) },
    { "SmallerText", NULL,
      N_("_Smaller Text"),
      "<Control>minus",
      NULL,
      G_CALLBACK (window_font_adjustment) },
    {"GoBack", GTK_STOCK_GO_BACK,
     N_("_Back"),
     "<Alt>Left",
     NULL,
     G_CALLBACK (window_go_back) },
    {"GoForward", GTK_STOCK_GO_FORWARD,
     N_("_Forward"),
     "<Alt>Right",
     NULL,
     G_CALLBACK (window_go_forward) }
};

static void
yelp_window_init (YelpWindow *window)
{
    GtkWidget *vbox, *scroll;
    GtkUIManager *ui_manager;
    GtkAction *action;
    GError *error = NULL;
    GtkTreeIter iter;
    YelpWindowPrivate *priv = GET_PRIV (window);

    gtk_window_set_icon_name (GTK_WINDOW (window), "help-browser");
    gtk_window_set_default_size (GTK_WINDOW (window), 520, 580);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (window), vbox);

    priv->action_group = gtk_action_group_new ("MenuActions");
    gtk_action_group_set_translation_domain (priv->action_group, GETTEXT_PACKAGE);
    gtk_action_group_add_actions (priv->action_group,
				  entries, G_N_ELEMENTS (entries),
				  window);
    action = (GtkAction *) gtk_toggle_action_new ("ShowTextCursor",
                                                  _("Show Text _Cursor"),
                                                  NULL, NULL);
    g_signal_connect (action, "activate",
                      G_CALLBACK (window_show_text_cursor), window);
    gtk_action_group_add_action_with_accel (priv->action_group,
                                            action, "F7");

    ui_manager = gtk_ui_manager_new ();
    gtk_ui_manager_insert_action_group (ui_manager, priv->action_group, 0);
    gtk_window_add_accel_group (GTK_WINDOW (window),
                                gtk_ui_manager_get_accel_group (ui_manager));
    if (!gtk_ui_manager_add_ui_from_file (ui_manager,
					  DATADIR "/yelp/ui/yelp-ui.xml",
					  &error)) {
        GtkWidget *dialog = gtk_message_dialog_new (NULL, 0,
                                                    GTK_MESSAGE_ERROR,
                                                    GTK_BUTTONS_OK,
                                                    "%s", _("Cannot create window"));
        gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog), "%s",
                                                    error->message);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
        g_error_free (error);
    }
    gtk_box_pack_start (GTK_BOX (vbox),
                        gtk_ui_manager_get_widget (ui_manager, "/ui/menubar"),
                        FALSE, FALSE, 0);

    priv->hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), priv->hbox, FALSE, FALSE, 0);

    action = gtk_action_group_get_action (priv->action_group, "GoBack");
    priv->back_button = gtk_action_create_tool_item (action);
    gtk_box_pack_start (GTK_BOX (priv->hbox),
                        priv->back_button,
                        FALSE, FALSE, 0);
    action = gtk_action_group_get_action (priv->action_group, "GoForward");
    priv->forward_button = gtk_action_create_tool_item (action);
    gtk_box_pack_start (GTK_BOX (priv->hbox),
                        priv->forward_button,
                        FALSE, FALSE, 0);
    
    priv->history = gtk_list_store_new (6,
                                        G_TYPE_STRING,  /* title */
                                        G_TYPE_STRING,  /* desc */
                                        G_TYPE_STRING,  /* icon */
                                        G_TYPE_STRING,  /* uri */
                                        G_TYPE_INT,     /* flags */
                                        G_TYPE_STRING   /* search terms */
                                        );
    gtk_list_store_append (priv->history, &iter);
    gtk_list_store_set (priv->history, &iter,
                        COL_FLAGS, YELP_LOCATION_ENTRY_IS_SEPARATOR,
                        -1);
    gtk_list_store_append (priv->history, &iter);
    gtk_list_store_set (priv->history, &iter,
                        COL_ICON, "system-search",
                        COL_TITLE, _("Search..."),
                        COL_FLAGS, YELP_LOCATION_ENTRY_IS_SEARCH,
                        -1);
    priv->completion = gtk_list_store_new (4,
                                           G_TYPE_STRING,  /* title */
                                           G_TYPE_STRING,  /* desc */
                                           G_TYPE_STRING,  /* icon */
                                           G_TYPE_STRING   /* uri */
                                           );
    priv->entry = (YelpLocationEntry *)
        yelp_location_entry_new_with_model (GTK_TREE_MODEL (priv->history),
                                            COL_TITLE,
                                            COL_DESC,
                                            COL_ICON,
                                            COL_FLAGS);
    g_signal_connect (priv->entry, "focus-out-event",
                      G_CALLBACK (entry_focus_out), window);

    yelp_location_entry_set_completion_model (YELP_LOCATION_ENTRY (priv->entry),
                                              GTK_TREE_MODEL (priv->completion),
                                              COL_TITLE,
                                              COL_DESC,
                                              COL_ICON);
    g_signal_connect (priv->entry, "completion-selected",
                      G_CALLBACK (entry_completion_selected), window);

    priv->entry_location_selected = g_signal_connect (priv->entry, "location-selected",
                                                      G_CALLBACK (entry_location_selected), window);
    priv->align_location = g_object_ref_sink (gtk_alignment_new (0.0, 0.5, 1.0, 0.0));
    gtk_box_pack_start (GTK_BOX (priv->hbox),
                        GTK_WIDGET (priv->align_location),
                        TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (priv->align_location), GTK_WIDGET (priv->entry));

    priv->hidden_entry = gtk_entry_new ();
    priv->align_hidden = g_object_ref_sink (gtk_alignment_new (0.0, 0.5, 1.0, 0.0));
    gtk_container_add (GTK_CONTAINER (priv->align_hidden), GTK_WIDGET (priv->hidden_entry));

    g_signal_connect (priv->hidden_entry, "activate",
                      G_CALLBACK (hidden_entry_activate), window);
    g_signal_connect_swapped (priv->hidden_entry, "focus-out-event",
                              G_CALLBACK (hidden_entry_hide), window);
    g_signal_connect (priv->hidden_entry, "key-press-event",
                      G_CALLBACK (hidden_key_press), window);

    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_box_pack_end (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);

    priv->view = (YelpView *) yelp_view_new ();
    g_signal_connect (priv->view, "external-uri", G_CALLBACK (view_external_uri), window);
    g_signal_connect (priv->view, "loaded", G_CALLBACK (view_loaded), window);
    g_signal_connect (priv->view, "notify::yelp-uri", G_CALLBACK (view_uri_selected), window);
    g_signal_connect (priv->view, "notify::root-title", G_CALLBACK (view_root_title), window);
    g_signal_connect (priv->view, "notify::page-title", G_CALLBACK (view_page_title), window);
    g_signal_connect (priv->view, "notify::page-desc", G_CALLBACK (view_page_desc), window);
    g_signal_connect (priv->view, "notify::page-icon", G_CALLBACK (view_page_icon), window);
    gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (priv->view));
    gtk_widget_grab_focus (GTK_WIDGET (priv->view));
}

static void
yelp_window_class_init (YelpWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = yelp_window_dispose;
    object_class->finalize = yelp_window_finalize;
    object_class->get_property = yelp_window_get_property;
    object_class->set_property = yelp_window_set_property;

    g_object_class_install_property (object_class,
                                     PROP_APPLICATION,
                                     g_param_spec_object ("application",
							  _("Application"),
							  _("A YelpApplication instance that controls this window"),
                                                          YELP_TYPE_APPLICATION,
                                                          G_PARAM_CONSTRUCT_ONLY |
							  G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
							  G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

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

    if (priv->action_group) {
        g_object_unref (priv->action_group);
        priv->action_group = NULL;
    }

    if (priv->align_location) {
        g_object_unref (priv->align_location);
        priv->align_location = NULL;
    }

    if (priv->align_hidden) {
        g_object_unref (priv->align_hidden);
        priv->align_hidden = NULL;
    }

    while (priv->back_list) {
        back_entry_free ((YelpBackEntry *) priv->back_list->data);
        priv->back_list = g_list_delete_link (priv->back_list, priv->back_list);
    }

    G_OBJECT_CLASS (yelp_window_parent_class)->dispose (object);
}

static void
yelp_window_finalize (GObject *object)
{
    G_OBJECT_CLASS (yelp_window_parent_class)->finalize (object);
}

static void
yelp_window_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
    YelpWindowPrivate *priv = GET_PRIV (object);
    switch (prop_id) {
    case PROP_APPLICATION:
        g_value_set_object (value, priv->application);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
yelp_window_set_property (GObject     *object,
                          guint        prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
    YelpWindowPrivate *priv = GET_PRIV (object);
    switch (prop_id) {
    case PROP_APPLICATION:
        priv->application = g_value_get_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}


/******************************************************************************/

YelpWindow *
yelp_window_new (YelpApplication *app)
{
    YelpWindow *window;

    window = (YelpWindow *) g_object_new (YELP_TYPE_WINDOW, "application", app, NULL);

    return window;
}

void
yelp_window_load_uri (YelpWindow  *window,
                      YelpUri     *uri)
{
    YelpWindowPrivate *priv = GET_PRIV (window);

    yelp_view_load_uri (priv->view, uri);
}

GtkActionGroup *
yelp_window_get_action_group (YelpWindow *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);

    return priv->action_group;
}

/******************************************************************************/

static void
window_new (GtkAction *action, YelpWindow *window)
{
    gchar *uri = NULL;
    YelpWindowPrivate *priv = GET_PRIV (window);

    if (priv->back_list && priv->back_list->data)
        uri = yelp_uri_get_document_uri (((YelpBackEntry *) priv->back_list->data)->uri);

    yelp_application_new_window (priv->application, uri);

    g_free (uri);
}

static void
window_close (GtkAction *action, YelpWindow *window)
{
    gboolean ret;
    g_signal_emit_by_name (window, "delete-event", NULL, &ret);
    gtk_widget_destroy (GTK_WIDGET (window));
}

static void
window_open_location (GtkAction *action, YelpWindow *window)
{
    gchar *uri = NULL;
    const GdkColor yellow;
    gchar *color;
    YelpWindowPrivate *priv = GET_PRIV (window);

    gtk_container_remove (GTK_CONTAINER (priv->hbox),
                          priv->align_location);
    gtk_box_pack_start (GTK_BOX (priv->hbox),
                        priv->align_hidden,
                        TRUE, TRUE, 0);

    gtk_widget_show_all (priv->align_hidden);
    gtk_entry_set_text (GTK_ENTRY (priv->hidden_entry), "");
    gtk_widget_grab_focus (priv->hidden_entry);

    color = yelp_settings_get_color (yelp_settings_get_default (),
                                     YELP_SETTINGS_COLOR_YELLOW_BASE);
    if (gdk_color_parse (color, &yellow)) {
        gtk_widget_modify_base (priv->hidden_entry,
                                GTK_STATE_NORMAL,
                                &yellow);
    }
    g_free (color);

    if (priv->back_cur && priv->back_cur->data)
        uri = yelp_uri_get_canonical_uri (((YelpBackEntry *) priv->back_list->data)->uri);
    if (uri) {
        gchar *c;
        gtk_entry_set_text (GTK_ENTRY (priv->hidden_entry), uri);
        c = strchr (uri, ':');
        if (c)
            gtk_editable_select_region (GTK_EDITABLE (priv->hidden_entry), c - uri + 1, -1);
        else
            gtk_editable_select_region (GTK_EDITABLE (priv->hidden_entry), 5, -1);
        g_free (uri);
    }
}

static void
window_font_adjustment (GtkAction  *action,
                        YelpWindow *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);

    yelp_application_adjust_font (priv->application,
                                  g_str_equal (gtk_action_get_name (action), "LargerText") ? 1 : -1);
}

static void
window_show_text_cursor (GtkToggleAction *action,
                         YelpWindow *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);

    yelp_application_set_show_text_cursor (priv->application,
                                           gtk_toggle_action_get_active (action));
}

static void
window_go_back (GtkAction  *action,
                YelpWindow *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);

    if (priv->back_cur == NULL || priv->back_cur->next == NULL)
        return;

    priv->back_cur = priv->back_cur->next;

    if (priv->back_cur == NULL || priv->back_cur->data == NULL)
        return;

    priv->back_load = TRUE;
    yelp_window_load_uri (window, ((YelpBackEntry *) priv->back_cur->data)->uri);
}

static void
window_go_forward (GtkAction  *action,
                   YelpWindow *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);

    if (priv->back_cur == NULL || priv->back_cur->prev == NULL)
        return;

    priv->back_cur = priv->back_cur->prev;

    if (priv->back_cur == NULL || priv->back_cur->data == NULL)
        return;

    priv->back_load = TRUE;
    yelp_window_load_uri (window, ((YelpBackEntry *) priv->back_cur->data)->uri);
}

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
entry_completion_selected (YelpLocationEntry  *entry,
                           GtkTreeModel       *model,
                           GtkTreeIter        *iter,
                           YelpWindow         *window)
{
    YelpUri *base, *uri;
    gchar *page, *xref;
    YelpWindowPrivate *priv = GET_PRIV (window);

    base = ((YelpBackEntry *) priv->back_cur->data)->uri;
    gtk_tree_model_get (model, iter, COL_URI, &page, -1);

    xref = g_strconcat ("xref:", page, NULL);
    uri = yelp_uri_new_relative (base, xref);

    yelp_view_load_uri (priv->view, uri);

    g_free (page);
    g_free (xref);
    g_object_unref (uri);

    gtk_widget_grab_focus (GTK_WIDGET (priv->view));
}

static gboolean
entry_focus_out (YelpLocationEntry  *entry,
                 GdkEventFocus      *event,
                 YelpWindow         *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    gtk_widget_grab_focus (GTK_WIDGET (priv->view));
    return FALSE;
}

static void
view_external_uri (YelpView   *view,
                   YelpUri    *uri,
                   YelpWindow *window)
{
    gchar *struri = yelp_uri_get_canonical_uri (uri);
    if (g_str_has_prefix (struri, "install:")) {
        YelpWindowPrivate *priv = GET_PRIV (window);
        gchar *pkg = struri + 8;
        yelp_application_install_package (priv->application, pkg, "");
    }
    else
        g_app_info_launch_default_for_uri (struri, NULL, NULL);

    g_free (struri);
}

static void
view_loaded (YelpView   *view,
             YelpWindow *window)
{
    gchar **ids;
    gint i;
    YelpWindowPrivate *priv = GET_PRIV (window);
    YelpDocument *document = yelp_view_get_document (view);
    ids = yelp_document_list_page_ids (document);

    gtk_list_store_clear (priv->completion);
    for (i = 0; ids[i]; i++) {
        GtkTreeIter iter;
        gchar *title, *desc, *icon;
        gtk_list_store_insert (GTK_LIST_STORE (priv->completion), &iter, 0);
        title = yelp_document_get_page_title (document, ids[i]);
        desc = yelp_document_get_page_desc (document, ids[i]);
        icon = yelp_document_get_page_icon (document, ids[i]);
        gtk_list_store_set (priv->completion, &iter,
                            COL_TITLE, title,
                            COL_DESC, desc,
                            COL_ICON, icon,
                            COL_URI, ids[i],
                            -1);
        g_free (icon);
        g_free (desc);
        g_free (title);
    }

    g_strfreev (ids);
    g_object_unref (document);
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
    GtkAction *action;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (G_OBJECT (view), "yelp-uri", &uri, NULL);
    if (uri == NULL)
        return;

    back = g_new0 (YelpBackEntry, 1);
    back->uri = g_object_ref (uri);
    if (!priv->back_load) {
        while (priv->back_list != priv->back_cur) {
            back_entry_free ((YelpBackEntry *) priv->back_list->data);
            priv->back_list = g_list_delete_link (priv->back_list, priv->back_list);
        }
        priv->back_list = g_list_prepend (priv->back_list, back);
        priv->back_cur = priv->back_list;
    }
    priv->back_load = FALSE;

    action = gtk_action_group_get_action (priv->action_group, "GoBack");
    gtk_action_set_sensitive (action, FALSE);
    gtk_widget_set_tooltip_text (priv->back_button, "");
    if (priv->back_cur->next && priv->back_cur->next->data) {
        gchar *tooltip = "";
        YelpBackEntry *back = priv->back_cur->next->data;

        gtk_action_set_sensitive (action, TRUE);
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
        /* Can't seem to use markup on GtkAction tooltip */
        gtk_widget_set_tooltip_markup (priv->back_button, tooltip);
    }

    action = gtk_action_group_get_action (priv->action_group, "GoForward");
    gtk_action_set_sensitive (action, FALSE);
    gtk_widget_set_tooltip_text (priv->forward_button, "");
    if (priv->back_cur->prev && priv->back_cur->prev->data) {
        gchar *tooltip = "";
        YelpBackEntry *back = priv->back_cur->prev->data;

        gtk_action_set_sensitive (action, TRUE);
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
        /* Can't seem to use markup on GtkAction tooltip */
        gtk_widget_set_tooltip_markup (priv->forward_button, tooltip);
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
        gint num;
        GtkTreeIter last;
        gtk_list_store_prepend (priv->history, &iter);
        gtk_list_store_set (priv->history, &iter,
                            COL_TITLE, _("Loading"),
                            COL_ICON, "help-contents",
                            COL_URI, struri,
                            -1);
        /* Limit to 15 entries. There are two extra for the search entry and
         * the separator above it.
         */
        num = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->history), NULL);
        if (num > 17) {
            if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (priv->history),
                                                               &last, NULL,
                                                               num - 3)) {
                gtk_list_store_remove (priv->history, &last);
            }
        }
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
view_root_title (YelpView    *view,
                 GParamSpec  *pspec,
                 YelpWindow  *window)
{
    gchar *title;
    g_object_get (view, "root-title", &title, NULL);

    if (title) {
        gtk_window_set_title (GTK_WINDOW (window), title);
        g_free (title);
    } else {
        gtk_window_set_title (GTK_WINDOW (window), _("Help"));
    }
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

    if (priv->back_cur)
        back = priv->back_cur->data;

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

    g_free (title);
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

    if (priv->back_cur)
        back = priv->back_cur->data;

    gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &first);
    gtk_list_store_set (priv->history, &first,
                        COL_DESC, desc,
                        -1);
    if (back) {
        g_free (back->desc);
        back->desc = g_strdup (desc);
    }

    g_free (desc);
}

static void
view_page_icon (YelpView   *view,
                GParamSpec *pspec,
                YelpWindow *window)
{
    GtkTreeIter first;
    gchar *icon;
    YelpWindowPrivate *priv = GET_PRIV (window);

    g_object_get (view, "page-icon", &icon, NULL);
    if (icon == NULL)
        return;

    gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->history), &first);
    gtk_list_store_set (priv->history, &first,
                        COL_ICON, icon,
                        -1);
    g_free (icon);
}

static void
hidden_entry_activate (GtkEntry    *entry,
                       YelpWindow  *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    YelpUri *uri = yelp_uri_new (gtk_entry_get_text (entry));

    yelp_window_load_uri (window, uri);
    g_object_unref (uri);

    gtk_widget_grab_focus (GTK_WIDGET (priv->view));
}

static void
hidden_entry_hide (YelpWindow  *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);

    gtk_container_remove (GTK_CONTAINER (priv->hbox),
                          priv->align_hidden);
    gtk_box_pack_start (GTK_BOX (priv->hbox),
                        priv->align_location,
                        TRUE, TRUE, 0);
}

static gboolean
hidden_key_press (GtkWidget    *widget,
                  GdkEventKey  *event,
                  YelpWindow   *window)
{
    YelpWindowPrivate *priv = GET_PRIV (window);
    if (event->keyval == GDK_Escape) {
        gtk_widget_grab_focus (GTK_WIDGET (priv->view));
        return TRUE;
    }
    return FALSE;
}
