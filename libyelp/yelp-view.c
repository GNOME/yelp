/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009 Shaun McCance <shaunm@gnome.org>
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
#include <glib-object.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <webkit/webkit.h>

#include "yelp-debug.h"
#include "yelp-docbook-document.h"
#include "yelp-error.h"
#include "yelp-marshal.h"
#include "yelp-settings.h"
#include "yelp-types.h"
#include "yelp-view.h"

#define BOGUS_URI "file:///bogus/"
#define BOGUS_URI_LEN 14

static void        yelp_view_init                 (YelpView           *view);
static void        yelp_view_class_init           (YelpViewClass      *klass);
static void        yelp_view_dispose              (GObject            *object);
static void        yelp_view_finalize             (GObject            *object);
static void        yelp_view_get_property         (GObject            *object,
                                                   guint               prop_id,
                                                   GValue             *value,
                                                   GParamSpec         *pspec);
static void        yelp_view_set_property         (GObject            *object,
                                                   guint               prop_id,
                                                   const GValue       *value,
                                                   GParamSpec         *pspec);

static gboolean    view_external_uri              (YelpView           *view,
                                                   YelpUri            *uri);
static void        view_install_uri               (YelpView           *view,
                                                   const gchar        *uri);
static void        view_scrolled                  (GtkAdjustment      *adjustment,
                                                   YelpView           *view);
static void        view_set_hadjustment           (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   gpointer            data);
static void        view_set_vadjustment           (YelpView           *view,
                                                   GParamSpec         *pspec,
                                                   gpointer            data);
static void        popup_open_link                (GtkMenuItem        *item,
                                                   YelpView           *view);
static void        popup_open_link_new            (GtkMenuItem        *item,
                                                   YelpView           *view);
static void        popup_copy_link                (GtkMenuItem        *item,
                                                   YelpView           *view);
static void        popup_save_image               (GtkMenuItem        *item,
                                                   YelpView           *view);
static void        popup_send_image               (GtkMenuItem        *item,
                                                   YelpView           *view);
static void        popup_copy_code                (GtkMenuItem        *item,
                                                   YelpView           *view);
static void        popup_save_code                (GtkMenuItem        *item,
                                                   YelpView           *view);
static void        view_populate_popup            (YelpView           *view,
                                                   GtkMenu            *menu,
                                                   gpointer            data);
static void        view_script_alert              (YelpView           *view,
                                                   WebKitWebFrame     *frame,
                                                   gchar              *message,
                                                   gpointer            data);
static gboolean    view_navigation_requested      (WebKitWebView             *view,
                                                   WebKitWebFrame            *frame,
                                                   WebKitNetworkRequest      *request,
                                                   WebKitWebNavigationAction *action,
                                                   WebKitWebPolicyDecision   *decision,
                                                   gpointer                   user_data);
static void        view_resource_request          (WebKitWebView             *view,
                                                   WebKitWebFrame            *frame,
                                                   WebKitWebResource         *resource,
                                                   WebKitNetworkRequest      *request,
                                                   WebKitNetworkResponse     *response,
                                                   gpointer                   user_data);
static void        view_document_loaded           (WebKitWebView             *view,
                                                   WebKitWebFrame            *frame,
                                                   gpointer                   user_data);

static void        view_print                     (GtkAction          *action,
                                                   YelpView           *view);
static void        view_history_action            (GtkAction          *action,
                                                   YelpView           *view);
static void        view_navigation_action         (GtkAction          *action,
                                                   YelpView           *view);

static void        view_clear_load                (YelpView           *view);
static void        view_load_page                 (YelpView           *view);
static void        view_show_error_page           (YelpView           *view,
                                                   GError             *error);

static void        settings_set_fonts             (YelpSettings       *settings);
static void        settings_show_text_cursor      (YelpSettings       *settings);

static void        uri_resolved                   (YelpUri            *uri,
                                                   YelpView           *view);
static void        document_callback              (YelpDocument       *document,
                                                   YelpDocumentSignal  signal,
                                                   YelpView           *view,
                                                   GError             *error);

static const GtkActionEntry entries[] = {
    {"YelpViewPrint", GTK_STOCK_PRINT,
     N_("_Print..."),
     "<Control>P",
     NULL,
     G_CALLBACK (view_print) },
    {"YelpViewGoBack", GTK_STOCK_GO_BACK,
     N_("_Back"),
     "<Alt>Left",
     NULL,
     G_CALLBACK (view_history_action) },
    {"YelpViewGoForward", GTK_STOCK_GO_FORWARD,
     N_("_Forward"),
     "<Alt>Right",
     NULL,
     G_CALLBACK (view_history_action) },
    {"YelpViewGoPrevious", NULL,
     N_("_Previous Page"),
     "<Control>Page_Up",
     NULL,
     G_CALLBACK (view_navigation_action) },
    {"YelpViewGoNext", NULL,
     N_("_Next Page"),
     "<Control>Page_Down",
     NULL,
     G_CALLBACK (view_navigation_action) }
};

static gchar *nautilus_sendto = NULL;

enum {
    PROP_0,
    PROP_URI,
    PROP_STATE,
    PROP_PAGE_ID,
    PROP_ROOT_TITLE,
    PROP_PAGE_TITLE,
    PROP_PAGE_DESC,
    PROP_PAGE_ICON
};

enum {
    NEW_VIEW_REQUESTED,
    EXTERNAL_URI,
    LOADED,
    LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (YelpView, yelp_view, WEBKIT_TYPE_WEB_VIEW);
#define GET_PRIV(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_VIEW, YelpViewPrivate))

static WebKitWebSettings *websettings;

typedef struct _YelpActionEntry YelpActionEntry;
struct _YelpActionEntry {
    GtkAction               *action;
    YelpViewActionValidFunc  func;
    gpointer                 data;
};
static void
action_entry_free (YelpActionEntry *entry)
{
    if (entry == NULL)
        return;
    g_object_unref (entry->action);
    g_free (entry);
}

typedef struct _YelpBackEntry YelpBackEntry;
struct _YelpBackEntry {
    YelpUri *uri;
    gchar   *title;
    gchar   *desc;
    gdouble  hadj;
    gdouble  vadj;
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

typedef struct _YelpViewPrivate YelpViewPrivate;
struct _YelpViewPrivate {
    YelpUri       *uri;
    YelpUri       *resolve_uri;
    gulong         uri_resolved;
    gchar         *bogus_uri;
    YelpDocument  *document;
    GCancellable  *cancellable;
    GtkAdjustment *vadjustment;
    GtkAdjustment *hadjustment;
    gdouble        vadjust;
    gdouble        hadjust;
    gulong         vadjuster;
    gulong         hadjuster;

    gchar         *popup_link_uri;
    gchar         *popup_link_text;
    gchar         *popup_image_uri;
    WebKitDOMNode *popup_code_node;
    WebKitDOMNode *popup_code_title;
    gchar         *popup_code_text;

    YelpViewState  state;
    YelpViewState  prevstate;

    gchar         *page_id;
    gchar         *root_title;
    gchar         *page_title;
    gchar         *page_desc;
    gchar         *page_icon;

    GList          *back_list;
    GList          *back_cur;
    gboolean        back_load;

    GtkActionGroup *action_group;

    GSList         *link_actions;

    gint            navigation_requested;
};

#define TARGET_TYPE_URI_LIST     "text/uri-list"
enum {
    TARGET_URI_LIST
};

static void
yelp_view_init (YelpView *view)
{
    GtkAction *action;
    YelpViewPrivate *priv = GET_PRIV (view);

    g_object_set (view, "settings", websettings, NULL);

    priv->cancellable = NULL;

    priv->prevstate = priv->state = YELP_VIEW_STATE_BLANK;

    priv->navigation_requested =
        g_signal_connect (view, "navigation-policy-decision-requested",
                          G_CALLBACK (view_navigation_requested), NULL);
    g_signal_connect (view, "resource-request-starting",
                      G_CALLBACK (view_resource_request), NULL);
    g_signal_connect (view, "document-load-finished",
                      G_CALLBACK (view_document_loaded), NULL);
    g_signal_connect (view, "notify::hadjustment",
                      G_CALLBACK (view_set_hadjustment), NULL);
    g_signal_connect (view, "notify::vadjustment",
                      G_CALLBACK (view_set_vadjustment), NULL);
    g_signal_connect (view, "populate-popup",
                      G_CALLBACK (view_populate_popup), NULL);
    g_signal_connect (view, "script-alert",
                      G_CALLBACK (view_script_alert), NULL);

    priv->action_group = gtk_action_group_new ("YelpView");
    gtk_action_group_set_translation_domain (priv->action_group, GETTEXT_PACKAGE);
    gtk_action_group_add_actions (priv->action_group,
				  entries, G_N_ELEMENTS (entries),
				  view);
    action = gtk_action_group_get_action (priv->action_group, "YelpViewGoBack");
    gtk_action_set_sensitive (action, FALSE);
    action = gtk_action_group_get_action (priv->action_group, "YelpViewGoForward");
    gtk_action_set_sensitive (action, FALSE);
}

static void
yelp_view_dispose (GObject *object)
{
    YelpViewPrivate *priv = GET_PRIV (object);

    view_clear_load (YELP_VIEW (object));

    if (priv->vadjuster > 0) {
        g_signal_handler_disconnect (priv->vadjustment, priv->vadjuster);
        priv->vadjuster = 0;
    }

    if (priv->hadjuster > 0) {
        g_signal_handler_disconnect (priv->hadjustment, priv->hadjuster);
        priv->hadjuster = 0;
    }

    if (priv->action_group) {
        g_object_unref (priv->action_group);
        priv->action_group = NULL;
    }

    if (priv->document) {
        g_object_unref (priv->document);
        priv->document = NULL;
    }

    while (priv->link_actions) {
        action_entry_free (priv->link_actions->data);
        priv->link_actions = g_slist_delete_link (priv->link_actions, priv->link_actions);
    }

    priv->back_cur = NULL;
    while (priv->back_list) {
        back_entry_free ((YelpBackEntry *) priv->back_list->data);
        priv->back_list = g_list_delete_link (priv->back_list, priv->back_list);
    }

    G_OBJECT_CLASS (yelp_view_parent_class)->dispose (object);
}

static void
yelp_view_finalize (GObject *object)
{
    YelpViewPrivate *priv = GET_PRIV (object);

    g_free (priv->popup_link_uri);
    g_free (priv->popup_link_text);
    g_free (priv->popup_image_uri);
    g_free (priv->popup_code_text);

    g_free (priv->page_id);
    g_free (priv->root_title);
    g_free (priv->page_title);
    g_free (priv->page_desc);
    g_free (priv->page_icon);

    g_free (priv->bogus_uri);

    G_OBJECT_CLASS (yelp_view_parent_class)->finalize (object);
}

static void
yelp_view_class_init (YelpViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    YelpSettings *settings = yelp_settings_get_default ();

    nautilus_sendto = g_find_program_in_path ("nautilus-sendto");

    websettings = webkit_web_settings_new ();
    g_object_set (websettings, "enable-universal-access-from-file-uris", TRUE, NULL);
    g_signal_connect (settings,
                      "fonts-changed",
                      G_CALLBACK (settings_set_fonts),
                      NULL);
    settings_set_fonts (settings);
    g_signal_connect (settings,
                      "notify::show-text-cursor",
                      G_CALLBACK (settings_show_text_cursor),
                      NULL);
    settings_show_text_cursor (settings);

    klass->external_uri = view_external_uri;

    object_class->dispose = yelp_view_dispose;
    object_class->finalize = yelp_view_finalize;
    object_class->get_property = yelp_view_get_property;
    object_class->set_property = yelp_view_set_property;

    signals[NEW_VIEW_REQUESTED] =
	g_signal_new ("new-view-requested",
		      G_TYPE_FROM_CLASS (klass),
		      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
		      g_cclosure_marshal_VOID__OBJECT,
		      G_TYPE_NONE, 1, YELP_TYPE_URI);

    signals[EXTERNAL_URI] =
	g_signal_new ("external-uri",
		      G_TYPE_FROM_CLASS (klass),
		      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (YelpViewClass, external_uri),
                      g_signal_accumulator_true_handled, NULL,
		      yelp_marshal_BOOLEAN__OBJECT,
		      G_TYPE_BOOLEAN, 1, YELP_TYPE_URI);

    signals[LOADED] =
        g_signal_new ("loaded",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    g_type_class_add_private (klass, sizeof (YelpViewPrivate));

    g_object_class_install_property (object_class,
                                     PROP_URI,
                                     g_param_spec_object ("yelp-uri",
							  _("Yelp URI"),
							  _("A YelpUri with the current location"),
                                                          YELP_TYPE_URI,
							  G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
							  G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property (object_class,
                                     PROP_STATE,
                                     g_param_spec_enum ("state",
                                                        N_("Loading State"),
                                                        N_("The loading state of the view"),
                                                        YELP_TYPE_VIEW_STATE,
                                                        YELP_VIEW_STATE_BLANK,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property (object_class,
                                     PROP_PAGE_ID,
                                     g_param_spec_string ("page-id",
                                                          N_("Page ID"),
                                                          N_("The ID of the root page of the page being viewed"),
                                                          NULL,
                                                          G_PARAM_READABLE |
                                                          G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property (object_class,
                                     PROP_ROOT_TITLE,
                                     g_param_spec_string ("root-title",
                                                          N_("Root Title"),
                                                          N_("The title of the root page of the page being viewed"),
                                                          NULL,
                                                          G_PARAM_READABLE |
                                                          G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property (object_class,
                                     PROP_PAGE_TITLE,
                                     g_param_spec_string ("page-title",
                                                          N_("Page Title"),
                                                          N_("The title of the page being viewed"),
                                                          NULL,
                                                          G_PARAM_READABLE |
                                                          G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property (object_class,
                                     PROP_PAGE_DESC,
                                     g_param_spec_string ("page-desc",
                                                          N_("Page Description"),
                                                          N_("The description of the page being viewed"),
                                                          NULL,
                                                          G_PARAM_READABLE |
                                                          G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property (object_class,
                                     PROP_PAGE_ICON,
                                     g_param_spec_string ("page-icon",
                                                          N_("Page Icon"),
                                                          N_("The icon of the page being viewed"),
                                                          NULL,
                                                          G_PARAM_READABLE |
                                                          G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

static void
yelp_view_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
    YelpViewPrivate *priv = GET_PRIV (object);

    switch (prop_id)
        {
        case PROP_URI:
            g_value_set_object (value, priv->uri);
            break;
        case PROP_PAGE_ID:
            g_value_set_string (value, priv->page_id);
            break;
        case PROP_ROOT_TITLE:
            g_value_set_string (value, priv->root_title);
            break;
        case PROP_PAGE_TITLE:
            g_value_set_string (value, priv->page_title);
            break;
        case PROP_PAGE_DESC:
            g_value_set_string (value, priv->page_desc);
            break;
        case PROP_PAGE_ICON:
            if (priv->page_icon)
                g_value_set_string (value, priv->page_icon);
            else
                g_value_set_string (value, "yelp-page-symbolic");
            break;
        case PROP_STATE:
            g_value_set_enum (value, priv->state);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
yelp_view_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
    YelpUri *uri;
    YelpViewPrivate *priv = GET_PRIV (object);

    switch (prop_id)
        {
        case PROP_URI:
            uri = g_value_get_object (value);
            yelp_view_load_uri (YELP_VIEW (object), uri);
            g_object_unref (uri);
            break;
        case PROP_STATE:
            priv->prevstate = priv->state;
            priv->state = g_value_get_enum (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

/******************************************************************************/

GtkWidget *
yelp_view_new (void)
{
    return (GtkWidget *) g_object_new (YELP_TYPE_VIEW, NULL);
}

void
yelp_view_load (YelpView    *view,
                const gchar *uri)
{
    YelpUri *yuri = yelp_uri_new (uri);
    yelp_view_load_uri (view, yuri);
    g_object_unref (yuri);
}

void
yelp_view_load_uri (YelpView *view,
                    YelpUri  *uri)
{
    YelpViewPrivate *priv = GET_PRIV (view);

    g_object_set (view, "state", YELP_VIEW_STATE_LOADING, NULL);

    gtk_action_set_sensitive (gtk_action_group_get_action (priv->action_group,
                                                           "YelpViewGoPrevious"),
                              FALSE);
    gtk_action_set_sensitive (gtk_action_group_get_action (priv->action_group,
                                                           "YelpViewGoNext"),
                              FALSE);

    if (!yelp_uri_is_resolved (uri)) {
        if (priv->resolve_uri != NULL) {
            if (priv->uri_resolved != 0) {
                g_signal_handler_disconnect (priv->resolve_uri, priv->uri_resolved);
                priv->uri_resolved = 0;
            }
            g_object_unref (priv->resolve_uri);
        }
        priv->resolve_uri = g_object_ref (uri);
        priv->uri_resolved = g_signal_connect (uri, "resolved",
                                               G_CALLBACK (uri_resolved),
                                               view);
        yelp_uri_resolve (uri);
    }
    else {
        uri_resolved (uri, view);
    }
}

void
yelp_view_load_document (YelpView     *view,
                         YelpUri      *uri,
                         YelpDocument *document)
{
    GParamSpec *spec;
    YelpViewPrivate *priv = GET_PRIV (view);

    g_return_if_fail (yelp_uri_is_resolved (uri));

    g_object_set (view, "state", YELP_VIEW_STATE_LOADING, NULL);

    g_object_ref (uri);
    view_clear_load (view);
    priv->uri = uri;
    spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                         "yelp-uri");
    g_signal_emit_by_name (view, "notify::yelp-uri", spec);
    g_object_ref (document);
    if (priv->document)
        g_object_unref (document);
    priv->document = document;

    view_load_page (view);
}

YelpDocument *
yelp_view_get_document (YelpView *view)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    return priv->document;
}

GtkActionGroup *
yelp_view_get_action_group (YelpView *view)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    return priv->action_group;
}

/******************************************************************************/

void
yelp_view_add_link_action (YelpView                *view,
                           GtkAction               *action,
                           YelpViewActionValidFunc  func,
                           gpointer                 data)
{
    YelpActionEntry *entry;
    YelpViewPrivate *priv = GET_PRIV (view);

    entry = g_new0 (YelpActionEntry, 1);
    entry->action = g_object_ref (action);
    entry->func = func;
    entry->data = data;

    priv->link_actions = g_slist_append (priv->link_actions, entry);
}

YelpUri *
yelp_view_get_active_link_uri (YelpView *view)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    YelpUri *uri;

    if (g_str_has_prefix (priv->popup_link_uri, BOGUS_URI))
        uri = yelp_uri_new_relative (priv->uri, priv->popup_link_uri + BOGUS_URI_LEN);
    else
        uri = yelp_uri_new_relative (priv->uri, priv->popup_link_uri);

    return uri;
}

gchar *
yelp_view_get_active_link_text (YelpView *view)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    return g_strdup (priv->popup_link_text);
}

/******************************************************************************/

static gboolean
view_external_uri (YelpView *view,
                   YelpUri  *uri)
{
    gchar *struri = yelp_uri_get_canonical_uri (uri);
    g_app_info_launch_default_for_uri (struri, NULL, NULL);
    g_free (struri);
    return TRUE;
}

typedef struct _YelpInstallInfo YelpInstallInfo;
struct _YelpInstallInfo {
    YelpView *view;
    gchar *uri;
};

static void
yelp_install_info_free (YelpInstallInfo *info)
{
    g_object_unref (info->view);
    if (info->uri)
        g_free (info->uri);
    g_free (info);
}

static void
view_install_installed (GDBusConnection *connection,
                        GAsyncResult    *res,
                        YelpInstallInfo *info)
{
    GError *error = NULL;
    g_dbus_connection_call_finish (connection, res, &error);
    if (error) {
        const gchar *err = NULL;
        if (error->domain == G_DBUS_ERROR) {
            if (error->code == G_DBUS_ERROR_SERVICE_UNKNOWN)
                err = _("You do not have PackageKit. Package install links require PackageKit.");
            else
                err = error->message;
        }
        if (err != NULL) {
            GtkWidget *dialog = gtk_message_dialog_new (NULL, 0,
                                                        GTK_MESSAGE_ERROR,
                                                        GTK_BUTTONS_CLOSE,
                                                        "%s", err);
            gtk_dialog_run ((GtkDialog *) dialog);
            gtk_widget_destroy (dialog);
        }
        g_error_free (error);
    }
    else if (info->uri) {
        gchar *struri, *docuri;
        YelpViewPrivate *priv = GET_PRIV (info->view);
        docuri = yelp_uri_get_document_uri (priv->uri);
        if (g_str_equal (docuri, info->uri)) {
            struri = yelp_uri_get_canonical_uri (priv->uri);
            yelp_view_load (info->view, struri);
            g_free (struri);
        }
        g_free (docuri);
    }

    yelp_install_info_free (info);
}

static void
view_install_uri (YelpView    *view,
                  const gchar *uri)
{
    GDBusConnection *connection;
    GError *error;
    gboolean help = FALSE, ghelp = FALSE;
    GVariantBuilder *strv;
    YelpInstallInfo *info;
    guint32 xid = 0;
    YelpViewPrivate *priv = GET_PRIV (view);
    GtkWidget *gtkwin;
    GdkWindow *gdkwin;
    /* do not free */
    gchar *pkg, *confirm_search;

    if (g_str_has_prefix (uri, "install-help:")) {
        help = TRUE;
        pkg = (gchar *) uri + 13;
    }
    else if (g_str_has_prefix (uri, "install-ghelp:")) {
        ghelp = TRUE;
        pkg = (gchar *) uri + 14;
    }
    else if (g_str_has_prefix (uri, "install:")) {
        pkg = (gchar *) uri + 8;
    }

    connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
    if (connection == NULL) {
        g_warning ("Unable to connect to dbus: %s", error->message);
        g_error_free (error);
        return;
    }

    info = g_new0 (YelpInstallInfo, 1);
    info->view = g_object_ref (view);

    gtkwin = gtk_widget_get_toplevel (GTK_WIDGET (view));
    if (gtkwin != NULL && gtk_widget_is_toplevel (gtkwin)) {
        gdkwin = gtk_widget_get_window (gtkwin);
        if (gdkwin != NULL)
            xid = gdk_x11_window_get_xid (gdkwin);
    }

    if (priv->state == YELP_VIEW_STATE_ERROR)
        confirm_search = "hide-confirm-search";
    else
        confirm_search = "";

    if (help || ghelp) {
        const gchar * const *datadirs = g_get_system_data_dirs ();
        gint datadirs_i;
        gchar *docbook, *fname;
        docbook = g_strconcat (pkg, ".xml", NULL);
        strv = g_variant_builder_new (G_VARIANT_TYPE ("as"));
        for (datadirs_i = 0; datadirs[datadirs_i] != NULL; datadirs_i++) {
            if (ghelp) {
                fname = g_build_filename (datadirs[datadirs_i], "gnome", "help",
                                          pkg, "C", "index.page", NULL);
                g_variant_builder_add (strv, "s", fname);
                g_free (fname);
                fname = g_build_filename (datadirs[datadirs_i], "gnome", "help",
                                          pkg, "C", docbook, NULL);
                g_variant_builder_add (strv, "s", fname);
                g_free (fname);
            }
            else {
                fname = g_build_filename (datadirs[datadirs_i], "help", "C",
                                          pkg, "index.page", NULL);
                g_variant_builder_add (strv, "s", fname);
                g_free (fname);
                fname = g_build_filename (datadirs[datadirs_i], "help", "C",
                                          pkg, "index.docbook", NULL);
                g_variant_builder_add (strv, "s", fname);
                g_free (fname);
            }
        }
        g_free (docbook);
        info->uri = g_strconcat (ghelp ? "ghelp:" : "help:", pkg, NULL);
        g_dbus_connection_call (connection,
                                "org.freedesktop.PackageKit",
                                "/org/freedesktop/PackageKit",
                                "org.freedesktop.PackageKit.Modify",
                                "InstallProvideFiles",
                                g_variant_new ("(uass)", xid, strv, confirm_search),
                                NULL,
                                G_DBUS_CALL_FLAGS_NONE,
                                G_MAXINT, NULL,
                                (GAsyncReadyCallback) view_install_installed,
                                info);
        g_variant_builder_unref (strv);
    }
    else {
        gchar **pkgs;
        gint i;
        strv = g_variant_builder_new (G_VARIANT_TYPE ("as"));
        pkgs = g_strsplit (pkg, ",", 0);
        for (i = 0; pkgs[i]; i++)
            g_variant_builder_add (strv, "s", pkgs[i]);
        g_strfreev (pkgs);
        g_dbus_connection_call (connection,
                                "org.freedesktop.PackageKit",
                                "/org/freedesktop/PackageKit",
                                "org.freedesktop.PackageKit.Modify",
                                "InstallPackageNames",
                                g_variant_new ("(uass)", xid, strv, confirm_search),
                                NULL,
                                G_DBUS_CALL_FLAGS_NONE,
                                G_MAXINT, NULL,
                                (GAsyncReadyCallback) view_install_installed,
                                info);
        g_variant_builder_unref (strv);
    }

    g_object_unref (connection);
}

static void
view_scrolled (GtkAdjustment *adjustment,
               YelpView      *view)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    if (priv->back_cur == NULL || priv->back_cur->data == NULL)
        return;
    if (adjustment == priv->vadjustment)
        ((YelpBackEntry *) priv->back_cur->data)->vadj = gtk_adjustment_get_value (adjustment);
    else if (adjustment = priv->hadjustment)
        ((YelpBackEntry *) priv->back_cur->data)->hadj = gtk_adjustment_get_value (adjustment);
}

static void
view_set_hadjustment (YelpView      *view,
                      GParamSpec    *pspec,
                      gpointer       data)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    priv->hadjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (view));
    if (priv->hadjuster > 0)
        g_signal_handler_disconnect (priv->hadjustment, priv->hadjuster);
    priv->hadjuster = 0;
    if (priv->hadjustment)
        priv->hadjuster = g_signal_connect (priv->hadjustment, "value-changed",
                                            G_CALLBACK (view_scrolled), view);
}

static void
view_set_vadjustment (YelpView      *view,
                      GParamSpec    *pspec,
                      gpointer       data)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    priv->vadjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (view));
    if (priv->vadjuster > 0)
        g_signal_handler_disconnect (priv->vadjustment, priv->vadjuster);
    priv->vadjuster = 0;
    if (priv->vadjustment)
        priv->vadjuster = g_signal_connect (priv->vadjustment, "value-changed",
                                            G_CALLBACK (view_scrolled), view);
}

static void
popup_open_link (GtkMenuItem *item,
                 YelpView    *view)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    YelpUri *uri;

    if (g_str_has_prefix (priv->popup_link_uri, BOGUS_URI))
        uri = yelp_uri_new_relative (priv->uri, priv->popup_link_uri + BOGUS_URI_LEN);
    else
        uri = yelp_uri_new_relative (priv->uri, priv->popup_link_uri);

    yelp_view_load_uri (view, uri);
    g_object_unref (uri);

    g_free (priv->popup_link_uri);
    priv->popup_link_uri = NULL;

    g_free (priv->popup_link_text);
    priv->popup_link_text = NULL;
}

static void
popup_open_link_new (GtkMenuItem *item,
                     YelpView    *view)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    YelpUri *uri;

    if (g_str_has_prefix (priv->popup_link_uri, BOGUS_URI))
        uri = yelp_uri_new_relative (priv->uri, priv->popup_link_uri + BOGUS_URI_LEN);
    else
        uri = yelp_uri_new_relative (priv->uri, priv->popup_link_uri);

    g_free (priv->popup_link_uri);
    priv->popup_link_uri = NULL;

    g_free (priv->popup_link_text);
    priv->popup_link_text = NULL;

    g_signal_emit (view, signals[NEW_VIEW_REQUESTED], 0, uri);
    g_object_unref (uri);
}

static void
popup_copy_link (GtkMenuItem *item,
                 YelpView    *view)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (view), GDK_SELECTION_CLIPBOARD),
                            priv->popup_link_uri,
                            -1);
}

typedef struct _YelpSaveData YelpSaveData;
struct _YelpSaveData {
    GFile     *orig;
    GFile     *dest;
    YelpView  *view;
    GtkWindow *window;
};

static void
file_copied (GFile        *file,
             GAsyncResult *res,
             YelpSaveData *data)
{
    GError *error = NULL;
    if (!g_file_copy_finish (file, res, &error)) {
        GtkWidget *dialog = gtk_message_dialog_new (gtk_widget_get_visible (GTK_WIDGET (data->window)) ? data->window : NULL,
                                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    GTK_MESSAGE_ERROR,
                                                    GTK_BUTTONS_OK,
                                                    "%s", error->message);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
    }
    g_object_unref (data->orig);
    g_object_unref (data->dest);
    g_object_unref (data->view);
    g_object_unref (data->window);
}

static void
popup_save_image (GtkMenuItem *item,
                  YelpView    *view)
{
    YelpSaveData *data;
    GtkWidget *dialog, *window;
    gchar *basename;
    gint res;
    YelpViewPrivate *priv = GET_PRIV (view);

    for (window = gtk_widget_get_parent (GTK_WIDGET (view));
         window && !GTK_IS_WINDOW (window);
         window = gtk_widget_get_parent (window));

    data = g_new0 (YelpSaveData, 1);
    data->orig = g_file_new_for_uri (priv->popup_image_uri);
    data->view = g_object_ref (view);
    data->window = g_object_ref (window);
    g_free (priv->popup_image_uri);
    priv->popup_image_uri = NULL;

    dialog = gtk_file_chooser_dialog_new (_("Save Image"),
                                          GTK_WINDOW (window),
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                                          NULL);
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
    basename = g_file_get_basename (data->orig);
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), basename);
    g_free (basename);
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
                                         g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP));

    res = gtk_dialog_run (GTK_DIALOG (dialog));

    if (res == GTK_RESPONSE_OK) {
        data->dest = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
        g_file_copy_async (data->orig, data->dest,
                           G_FILE_COPY_OVERWRITE,
                           G_PRIORITY_DEFAULT,
                           NULL, NULL, NULL,
                           (GAsyncReadyCallback) file_copied,
                           data);
    }
    else {
        g_object_unref (data->orig);
        g_object_unref (data->view);
        g_object_unref (data->window);
        g_free (data);
    }

    gtk_widget_destroy (dialog);
}

static void
popup_send_image (GtkMenuItem *item,
                  YelpView    *view)
{
    gchar *command;
    GAppInfo *app;
    GAppLaunchContext *context;
    GError *error = NULL;
    YelpViewPrivate *priv = GET_PRIV (view);

    command = g_strdup_printf ("%s %s", nautilus_sendto, priv->popup_image_uri);
    context = (GAppLaunchContext *) gdk_display_get_app_launch_context (gtk_widget_get_display (GTK_WIDGET (item)));

    app = g_app_info_create_from_commandline (command, NULL, 0, &error);
    if (app) {
        g_app_info_launch (app, NULL, context, &error);
        g_object_unref (app);
    }

    if (error) {
        g_debug ("Could not launch nautilus-sendto: %s", error->message);
        g_error_free (error);
    }

    g_object_unref (context);
    g_free (command);
    g_free (priv->popup_image_uri);
    priv->popup_image_uri = NULL;
}

static void
popup_copy_code (GtkMenuItem *item,
                 YelpView    *view)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    GtkClipboard *clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
    gchar *content = webkit_dom_node_get_text_content (priv->popup_code_node);
    gtk_clipboard_set_text (clipboard, content, -1);
    g_free (content);
}

static void
popup_save_code (GtkMenuItem *item,
                 YelpView    *view)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    GtkWidget *dialog, *window;
    gint res;

    g_free (priv->popup_code_text);
    priv->popup_code_text = webkit_dom_node_get_text_content (priv->popup_code_node);
    if (!g_str_has_suffix (priv->popup_code_text, "\n")) {
        gchar *tmp = g_strconcat (priv->popup_code_text, "\n", NULL);
        g_free (priv->popup_code_text);
        priv->popup_code_text = tmp;
    }

    for (window = gtk_widget_get_parent (GTK_WIDGET (view));
         window && !GTK_IS_WINDOW (window);
         window = gtk_widget_get_parent (window));

    dialog = gtk_file_chooser_dialog_new (_("Save Code"),
                                          GTK_WINDOW (window),
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                                          NULL);
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
    if (priv->popup_code_title) {
        gchar *filename = webkit_dom_node_get_text_content (priv->popup_code_title);
        gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), filename);
        g_free (filename);
    }
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
                                         g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS));

    res = gtk_dialog_run (GTK_DIALOG (dialog));

    if (res == GTK_RESPONSE_OK) {
        GError *error = NULL;
        GFile *file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
        GFileOutputStream *stream = g_file_replace (file, NULL, FALSE,
                                                    G_FILE_CREATE_NONE,
                                                    NULL,
                                                    &error);
        if (stream == NULL) {
            GtkWidget *dialog = gtk_message_dialog_new (gtk_widget_get_visible (window) ? GTK_WINDOW (window) : NULL,
                                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                                        GTK_MESSAGE_ERROR,
                                                        GTK_BUTTONS_OK,
                                                        "%s", error->message);
            gtk_dialog_run (GTK_DIALOG (dialog));
            gtk_widget_destroy (dialog);
            g_error_free (error);
        }
        else {
            /* FIXME: we should do this async */
            GDataOutputStream *datastream = g_data_output_stream_new (G_OUTPUT_STREAM (stream));
            if (!g_data_output_stream_put_string (datastream, priv->popup_code_text, NULL, &error)) {
                GtkWidget *dialog = gtk_message_dialog_new (gtk_widget_get_visible (window) ? GTK_WINDOW (window) : NULL,
                                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                                            GTK_MESSAGE_ERROR,
                                                            GTK_BUTTONS_OK,
                                                            "%s", error->message);
                gtk_dialog_run (GTK_DIALOG (dialog));
                gtk_widget_destroy (dialog);
                g_error_free (error);
            }
            g_object_unref (datastream);
        }
        g_object_unref (file);
    }

    priv->popup_code_node = NULL;
    priv->popup_code_title = NULL;
    g_free (priv->popup_code_text);
    priv->popup_code_text = NULL;

    gtk_widget_destroy (dialog);
}

static void
view_populate_popup (YelpView *view,
                     GtkMenu  *menu,
                     gpointer  data)
{
    WebKitHitTestResult *result;
    WebKitHitTestResultContext context;
    GdkEvent *event;
    YelpViewPrivate *priv = GET_PRIV (view);
    GList *children;
    GtkWidget *item;
    WebKitDOMNode *node, *cur, *link_node = NULL, *code_node = NULL, *code_title_node = NULL;

    children = gtk_container_get_children (GTK_CONTAINER (menu));
    while (children) {
        gtk_container_remove (GTK_CONTAINER (menu),
                              GTK_WIDGET (children->data));
        children = children->next;
    }
    g_list_free (children);

    event = gtk_get_current_event ();

    result = webkit_web_view_get_hit_test_result (WEBKIT_WEB_VIEW (view), (GdkEventButton *) event);
    g_object_get (result,
                  "context", &context,
                  "inner-node", &node,
                  NULL);
    for (cur = node; cur != NULL; cur = webkit_dom_node_get_parent_node (cur)) {
        if (WEBKIT_DOM_IS_ELEMENT (cur) &&
            webkit_dom_element_webkit_matches_selector ((WebKitDOMElement *) cur,
                                                        "a", NULL))
            link_node = cur;

        if (WEBKIT_DOM_IS_ELEMENT (cur) &&
            webkit_dom_element_webkit_matches_selector ((WebKitDOMElement *) cur,
                                                        "div.code", NULL)) {
            WebKitDOMNode *title;
            code_node = (WebKitDOMNode *)
                webkit_dom_element_query_selector ((WebKitDOMElement *) cur,
                                                   "pre.contents", NULL);
            title = webkit_dom_node_get_parent_node (cur);
            if (title != NULL && WEBKIT_DOM_IS_ELEMENT (title) &&
                webkit_dom_element_webkit_matches_selector ((WebKitDOMElement *) title,
                                                            "div.contents", NULL)) {
                title = webkit_dom_node_get_previous_sibling (title);
                if (title != NULL && WEBKIT_DOM_IS_ELEMENT (title) &&
                    webkit_dom_element_webkit_matches_selector ((WebKitDOMElement *) title,
                                                                "div.title", NULL)) {
                    code_title_node = title;
                }
            }
        }
    }

    if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) {
        gchar *uri;
        g_object_get (result, "link-uri", &uri, NULL);
        g_free (priv->popup_link_uri);
        priv->popup_link_uri = uri;

        g_free (priv->popup_link_text);
        priv->popup_link_text = NULL;
        if (link_node != NULL) {
            WebKitDOMNode *child;
            gchar *tmp;
            gint i, tmpi;
            gboolean ws;

            child = (WebKitDOMNode *)
                webkit_dom_element_query_selector (WEBKIT_DOM_ELEMENT (link_node),
                                                   "span.title", NULL);
            if (child != NULL)
                priv->popup_link_text = webkit_dom_node_get_text_content (child);

            if (priv->popup_link_text == NULL)
                priv->popup_link_text = webkit_dom_node_get_text_content (link_node);

            tmp = g_new0 (gchar, strlen(priv->popup_link_text) + 1);
            ws = FALSE;
            for (i = 0, tmpi = 0; priv->popup_link_text[i] != '\0'; i++) {
                if (priv->popup_link_text[i] == ' ' || priv->popup_link_text[i] == '\n') {
                    if (!ws) {
                        tmp[tmpi] = ' ';
                        tmpi++;
                        ws = TRUE;
                    }
                }
                else {
                    tmp[tmpi] = priv->popup_link_text[i];
                    tmpi++;
                    ws = FALSE;
                }
            }
            tmp[tmpi] = '\0';
            g_free (priv->popup_link_text);
            priv->popup_link_text = tmp;
        }
        else {
            priv->popup_link_text = g_strdup (uri);
        }

        if (g_str_has_prefix (priv->popup_link_uri, "mailto:")) {
            gchar *label = g_strdup_printf (_("Send email to %s"),
                                            priv->popup_link_uri + 7);
            /* Not using a mnemonic because underscores are common in email
             * addresses, and we'd have to escape them. There doesn't seem
             * to be a quick GTK+ function for this. In practice, there will
             * probably only be one menu item for mailto link popups anyway,
             * so the mnemonic's not that big of a deal.
             */
            item = gtk_menu_item_new_with_label (label);
            g_signal_connect (item, "activate",
                              G_CALLBACK (popup_open_link), view);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
            g_free (label);
        }
        else if (g_str_has_prefix (priv->popup_link_uri, "install:")) {
            item = gtk_menu_item_new_with_mnemonic (_("_Install Packages"));
            g_signal_connect (item, "activate",
                              G_CALLBACK (popup_open_link), view);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        }
        else {
            GSList *cur;

            item = gtk_menu_item_new_with_mnemonic (_("_Open Link"));
            g_signal_connect (item, "activate",
                              G_CALLBACK (popup_open_link), view);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

            if (g_str_has_prefix (priv->popup_link_uri, "http://") ||
                g_str_has_prefix (priv->popup_link_uri, "https://")) {
                item = gtk_menu_item_new_with_mnemonic (_("_Copy Link Location"));
                g_signal_connect (item, "activate",
                                  G_CALLBACK (popup_copy_link), view);
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
            }
            else {
                item = gtk_menu_item_new_with_mnemonic (_("Open Link in New _Window"));
                g_signal_connect (item, "activate",
                                  G_CALLBACK (popup_open_link_new), view);
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
            }

            for (cur = priv->link_actions; cur != NULL; cur = cur->next) {
                gboolean add;
                YelpActionEntry *entry = (YelpActionEntry *) cur->data;
                if (entry->func == NULL)
                    add = TRUE;
                else
                    add = (* entry->func) (view, entry->action,
                                           priv->popup_link_uri,
                                           entry->data);
                if (add) {
                    item = gtk_action_create_menu_item (entry->action);
                    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
                }
            }
        }
    }
    else {
        item = gtk_action_create_menu_item (gtk_action_group_get_action (priv->action_group,
                                                                         "YelpViewGoBack"));
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        item = gtk_action_create_menu_item (gtk_action_group_get_action (priv->action_group,
                                                                         "YelpViewGoForward"));
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }

    if ((context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE) ||
        (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_MEDIA)) {
        /* This doesn't currently work for video with automatic controls,
         * because WebKit puts the hit test on the div with the controls.
         */
        gboolean image = context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE;
        gchar *uri;
        g_object_get (result, image ? "image-uri" : "media-uri", &uri, NULL);
        g_free (priv->popup_image_uri);
        if (g_str_has_prefix (uri, BOGUS_URI)) {
            priv->popup_image_uri = yelp_uri_locate_file_uri (priv->uri, uri + BOGUS_URI_LEN);
            g_free (uri);
        }
        else {
            priv->popup_image_uri = uri;
        }

        item = gtk_separator_menu_item_new ();
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        if (image)
            item = gtk_menu_item_new_with_mnemonic (_("_Save Image As..."));
        else
            item = gtk_menu_item_new_with_mnemonic (_("_Save Video As..."));
        g_signal_connect (item, "activate",
                          G_CALLBACK (popup_save_image), view);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        if (nautilus_sendto) {
            if (image)
                item = gtk_menu_item_new_with_mnemonic (_("S_end Image To..."));
            else
                item = gtk_menu_item_new_with_mnemonic (_("S_end Video To..."));
            g_signal_connect (item, "activate",
                              G_CALLBACK (popup_send_image), view);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        }
    }

    if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_SELECTION) {
        item = gtk_separator_menu_item_new ();
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        item = gtk_menu_item_new_with_mnemonic (_("_Copy Text"));
        g_signal_connect_swapped (item, "activate",
                                  G_CALLBACK (webkit_web_view_copy_clipboard), view);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }

    if (code_node != NULL) {
        item = gtk_separator_menu_item_new ();
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        priv->popup_code_node = code_node;
        priv->popup_code_title = code_title_node;

        item = gtk_menu_item_new_with_mnemonic (_("C_opy Code Block"));
        g_signal_connect (item, "activate",
                          G_CALLBACK (popup_copy_code), view);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        item = gtk_menu_item_new_with_mnemonic (_("Save Code _Block As..."));
        g_signal_connect (item, "activate",
                          G_CALLBACK (popup_save_code), view);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }

    g_object_unref (result);
    gdk_event_free (event);
    gtk_widget_show_all (GTK_WIDGET (menu));
}

static void
view_script_alert (YelpView        *view,
                   WebKitWebFrame  *frame,
                   gchar           *message,
                   gpointer         data)
{
    printf ("\n\n===ALERT===\n%s\n\n", message);
}

static gboolean
view_navigation_requested (WebKitWebView             *view,
                           WebKitWebFrame            *frame,
                           WebKitNetworkRequest      *request,
                           WebKitWebNavigationAction *action,
                           WebKitWebPolicyDecision   *decision,
                           gpointer                   user_data)
{
    const gchar *requri = webkit_network_request_get_uri (request);
    YelpViewPrivate *priv = GET_PRIV (view);
    YelpUri *uri;

    if (priv->bogus_uri &&
        g_str_has_prefix (requri, priv->bogus_uri) &&
        requri[strlen(priv->bogus_uri)] == '#') {
        gchar *tmp = g_strconcat("xref:", requri + strlen(priv->bogus_uri), NULL);
        uri = yelp_uri_new_relative (priv->uri, tmp);
        g_free (tmp);
    }
    else if (g_str_has_prefix (requri, BOGUS_URI)) {
        uri = yelp_uri_new_relative (priv->uri, requri + BOGUS_URI_LEN);
    }
    else
        uri = yelp_uri_new_relative (priv->uri, requri);

    webkit_web_policy_decision_ignore (decision);

    yelp_view_load_uri ((YelpView *) view, uri);
    g_object_unref (uri);

    return TRUE;
}

static void
view_resource_request (WebKitWebView         *view,
                       WebKitWebFrame        *frame,
                       WebKitWebResource     *resource,
                       WebKitNetworkRequest  *request,
                       WebKitNetworkResponse *response,
                       gpointer               user_data)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    const gchar *requri = webkit_network_request_get_uri (request);
    gchar last;
    gchar *newpath;

    if (!g_str_has_prefix (requri, BOGUS_URI))
        return;

    /* We get this signal for the page itself.  Ignore. */
    if (g_str_equal (requri, priv->bogus_uri))
        return;

    newpath = yelp_uri_locate_file_uri (priv->uri, requri + BOGUS_URI_LEN);
    if (newpath != NULL) {
        webkit_network_request_set_uri (request, newpath);
        g_free (newpath);
    }
    else {
        webkit_network_request_set_uri (request, "about:blank");
    }
}

static void
view_document_loaded (WebKitWebView   *view,
                      WebKitWebFrame  *frame,
                      gpointer         user_data)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    gchar *search_terms;

    search_terms = yelp_uri_get_query (priv->uri, "terms");

    if (search_terms) {
        WebKitDOMDocument *doc;
        WebKitDOMElement *body, *link;
        doc = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
        body = webkit_dom_document_query_selector (doc, "div.body", NULL);
        if (body) {
            gchar *tmp, *uri, *txt;
            link = webkit_dom_document_create_element (doc, "a", NULL);
            webkit_dom_element_set_attribute (link, "class", "fullsearch", NULL);
            tmp = g_uri_escape_string (search_terms, NULL, FALSE);
            uri = g_strconcat ("xref:search=", tmp, NULL);
            webkit_dom_element_set_attribute (link, "href", uri, NULL);
            g_free (tmp);
            g_free (uri);
            txt = g_strdup_printf (_("See all search results for “%s”"),
                                   search_terms);
            webkit_dom_node_set_text_content (WEBKIT_DOM_NODE (link), txt, NULL);
            g_free (txt);
            webkit_dom_node_insert_before (WEBKIT_DOM_NODE (body),
                                           WEBKIT_DOM_NODE (link),
                                           webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body)),
                                           NULL);
        }
        g_free (search_terms);
    }
}

static void
view_print (GtkAction *action, YelpView  *view)
{
    webkit_web_frame_print (webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view)));
}

static void
view_history_action (GtkAction *action,
                     YelpView  *view)
{
    GList *newcur;
    YelpViewPrivate *priv = GET_PRIV (view);

    if (priv->back_cur == NULL)
        return;

    if (g_str_equal (gtk_action_get_name (action), "YelpViewGoBack"))
        newcur = priv->back_cur->next;
    else
        newcur = priv->back_cur->prev;

    if (newcur == NULL)
        return;

    priv->back_cur = newcur;

    if (priv->back_cur->data == NULL)
        return;

    priv->back_load = TRUE;
    yelp_view_load_uri (view, ((YelpBackEntry *) priv->back_cur->data)->uri);
    priv->vadjust = ((YelpBackEntry *) priv->back_cur->data)->vadj;
    priv->hadjust = ((YelpBackEntry *) priv->back_cur->data)->hadj;
}

static void
view_navigation_action (GtkAction *action,
                        YelpView  *view)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    gchar *page_id, *new_id, *xref;
    YelpUri *new_uri;

    page_id = yelp_uri_get_page_id (priv->uri);

    if (g_str_equal (gtk_action_get_name (action), "YelpViewGoPrevious"))
        new_id = yelp_document_get_prev_id (priv->document, page_id);
    else
        new_id = yelp_document_get_next_id (priv->document, page_id);

    /* Just in case we screwed up somewhere */
    if (new_id == NULL) {
        gtk_action_set_sensitive (action, FALSE);
        return;
    }

    xref = g_strconcat ("xref:", new_id, NULL);
    new_uri = yelp_uri_new_relative (priv->uri, xref);
    yelp_view_load_uri (view, new_uri);

    g_free (xref);
    g_free (new_id);
    g_object_unref (new_uri);
}

static void
view_clear_load (YelpView *view)
{
    YelpViewPrivate *priv = GET_PRIV (view);

    if (priv->resolve_uri != NULL) {
        if (priv->uri_resolved != 0) {
            g_signal_handler_disconnect (priv->resolve_uri, priv->uri_resolved);
            priv->uri_resolved = 0;
        }
        g_object_unref (priv->resolve_uri);
    }
    priv->resolve_uri = NULL;

    if (priv->uri) {
        g_object_unref (priv->uri);
        priv->uri = NULL;
    }

    if (priv->cancellable) {
        g_cancellable_cancel (priv->cancellable);
        priv->cancellable = NULL;
    }
}

static void
view_load_page (YelpView *view)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    gchar *page_id;

    debug_print (DB_FUNCTION, "entering\n");

    g_return_if_fail (priv->cancellable == NULL);

    if (priv->document == NULL) {
        GError *error;
        gchar *docuri;
        /* FIXME: and if priv->uri is NULL? */
        docuri = yelp_uri_get_document_uri (priv->uri);
        /* FIXME: CANT_READ isn't right */
        if (docuri) {
            error = g_error_new (YELP_ERROR, YELP_ERROR_CANT_READ,
                                 _("Could not load a document for ‘%s’"),
                                 docuri);
            g_free (docuri);
        }
        else {
            error = g_error_new (YELP_ERROR, YELP_ERROR_CANT_READ,
                                 _("Could not load a document"));
        }
        view_show_error_page (view, error);
        g_error_free (error);
        return;
    }

    page_id = yelp_uri_get_page_id (priv->uri);
    priv->cancellable = g_cancellable_new ();
    yelp_document_request_page (priv->document,
                                page_id,
                                priv->cancellable,
                                (YelpDocumentCallback) document_callback,
                                view);
    g_free (page_id);
}

static void
view_show_error_page (YelpView *view,
                      GError   *error)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    static const gchar *errorpage =
        "<html><head>"
        "<style type='text/css'>"
        "body {"
        " margin: 1em;"
        " color: %s;"
        " background-color: %s;"
        " }\n"
        "p { margin: 1em 0 0 0; }\n"
        "div.note {"
        " padding: 6px;"
        " border-color: %s;"
        " border-top: solid 1px;"
        " border-bottom: solid 1px;"
        " background-color: %s;"
        " }\n"
        "div.note div.inner {"
        " margin: 0; padding: 0;"
        " background-image: url(%s);"
        " background-position: %s top;"
        " background-repeat: no-repeat;"
        " min-height: %ipx;"
        " }\n"
        "div.note div.contents {"
        " margin-%s: %ipx;"
        " }\n"
        "div.note div.title {"
        " margin-%s: %ipx;"
        " margin-bottom: 0.2em;"
        " font-weight: bold;"
        " color: %s;"
        " }\n"
        "a { color: %s; text-decoration: none; }\n"
        "</style>"
        "</head><body>"
        "<div class='note'><div class='inner'>"
        "%s<div class='contents'>%s%s</div>"
        "</div></div>"
        "</body></html>";
    YelpSettings *settings = yelp_settings_get_default ();
    gchar *page, *title = NULL, *link = NULL, *title_m, *content_beg, *content_end;
    gchar *textcolor, *bgcolor, *noteborder, *notebg, *titlecolor, *noteicon, *linkcolor;
    gint iconsize;
    GParamSpec *spec;
    gboolean doc404 = FALSE;
    const gchar *left = (gtk_widget_get_direction((GtkWidget *) view) == GTK_TEXT_DIR_RTL) ? "right" : "left";

    if (priv->uri && yelp_uri_get_document_type (priv->uri) == YELP_URI_DOCUMENT_TYPE_NOT_FOUND)
        doc404 = TRUE;
    if (error->domain == YELP_ERROR)
        switch (error->code) {
        case YELP_ERROR_NOT_FOUND:
            if (doc404)
                title = _("Document Not Found");
            else
                title = _("Page Not Found");
            break;
        case YELP_ERROR_CANT_READ:
            title = _("Cannot Read");
            break;
        default:
            break;
        }
    if (title == NULL)
        title = _("Unknown Error");
    title_m = g_markup_printf_escaped ("<div class='title'>%s</div>", title);

    content_beg = g_markup_printf_escaped ("<p>%s</p>", error->message);
    content_end = NULL;
    if (doc404) {
        gchar *struri = yelp_uri_get_document_uri (priv->uri);
        /* do not free */
        gchar *pkg = NULL, *scheme = NULL;
        if (g_str_has_prefix (struri, "help:")) {
            scheme = "help";
            pkg = struri + 5;
        }
        else if (g_str_has_prefix (struri, "ghelp:")) {
            scheme = "ghelp";
            pkg = struri + 6;
        }
        if (pkg != NULL)
            content_end = g_markup_printf_escaped ("<p><a href='install-%s:%s'>%s</a></p>",
                                                   scheme, pkg,
                                                   _("Search for packages containing this document."));
        g_free (struri);
    }

    textcolor = yelp_settings_get_color (settings, YELP_SETTINGS_COLOR_TEXT);
    bgcolor = yelp_settings_get_color (settings, YELP_SETTINGS_COLOR_BASE);
    noteborder = yelp_settings_get_color (settings, YELP_SETTINGS_COLOR_RED_BORDER);
    notebg = yelp_settings_get_color (settings, YELP_SETTINGS_COLOR_YELLOW_BASE);
    titlecolor = yelp_settings_get_color (settings, YELP_SETTINGS_COLOR_TEXT_LIGHT);
    linkcolor = yelp_settings_get_color (settings, YELP_SETTINGS_COLOR_LINK);
    noteicon = yelp_settings_get_icon (settings, YELP_SETTINGS_ICON_WARNING);
    iconsize = yelp_settings_get_icon_size (settings) + 6;

    page = g_strdup_printf (errorpage,
                            textcolor, bgcolor, noteborder, notebg, noteicon,
                            left, iconsize, left, iconsize, left, iconsize,
                            titlecolor, linkcolor, title_m, content_beg,
                            (content_end != NULL) ? content_end : "");

    g_object_set (view, "state", YELP_VIEW_STATE_ERROR, NULL);

    if (doc404) {
        g_free (priv->root_title);
        priv->root_title = g_strdup (title);
        spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                             "root-title");
        g_signal_emit_by_name (view, "notify::root-title", spec);
        g_free (priv->page_id);
        priv->page_id = g_strdup ("index");
        spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                             "page-id");
        g_signal_emit_by_name (view, "notify::page-id", spec);
    }

    g_free (priv->page_title);
    priv->page_title = g_strdup (title);
    spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                         "page-title");
    g_signal_emit_by_name (view, "notify::page-title", spec);

    g_free (priv->page_desc);
    priv->page_desc = NULL;
    if (priv->uri)
        priv->page_desc = yelp_uri_get_canonical_uri (priv->uri);
    spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                         "page-desc");
    g_signal_emit_by_name (view, "notify::page-desc", spec);

    g_free (priv->page_icon);
    priv->page_icon = g_strdup ("dialog-warning");
    spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                         "page-icon");
    g_signal_emit_by_name (view, "notify::page-icon", spec);

    g_signal_emit (view, signals[LOADED], 0);
    g_signal_handler_block (view, priv->navigation_requested);
    webkit_web_view_load_string (WEBKIT_WEB_VIEW (view),
                                 page,
                                 "text/html",
                                 "UTF-8",
                                 "file:///error/");
    g_signal_handler_unblock (view, priv->navigation_requested);
    g_free (title_m);
    g_free (content_beg);
    if (content_end != NULL)
        g_free (content_end);
    g_free (page);
}


static void
settings_set_fonts (YelpSettings *settings)
{
    gchar *family;
    gint size;

    g_object_set (websettings,
                  "default-encoding", "utf-8",
                  "enable-private-browsing", TRUE,
                  NULL);

    family = yelp_settings_get_font_family (settings,
                                            YELP_SETTINGS_FONT_VARIABLE);
    size = yelp_settings_get_font_size (settings,
                                        YELP_SETTINGS_FONT_VARIABLE);
    g_object_set (websettings,
                  "default-font-family", family,
                  "sans-serif-font-family", family,
                  "default-font-size", size,
                  NULL);
    g_free (family);

    family = yelp_settings_get_font_family (settings,
                                            YELP_SETTINGS_FONT_FIXED);
    size = yelp_settings_get_font_size (settings,
                                        YELP_SETTINGS_FONT_FIXED);
    g_object_set (websettings,
                  "monospace-font-family", family,
                  "default-monospace-font-size", size,
                  NULL);
    g_free (family);
}

static void
settings_show_text_cursor (YelpSettings *settings)
{
    g_object_set (websettings,
                  "enable-caret-browsing",
                  yelp_settings_get_show_text_cursor (settings),
                  NULL);
}

/******************************************************************************/

static void
uri_resolved (YelpUri  *uri,
              YelpView *view)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    YelpDocument *document;
    YelpBackEntry *back;
    GtkAction *action;
    GSList *proxies, *cur;
    GError *error = NULL;
    gchar *struri;
    GParamSpec *spec;

    if (yelp_uri_get_document_type (uri) != YELP_URI_DOCUMENT_TYPE_EXTERNAL) {
        g_object_ref (uri);
        view_clear_load (view);
        priv->uri = uri;
    }

    switch (yelp_uri_get_document_type (uri)) {
    case YELP_URI_DOCUMENT_TYPE_EXTERNAL:
        g_object_set (view, "state", priv->prevstate, NULL);
        struri = yelp_uri_get_canonical_uri (uri);
        if (g_str_has_prefix (struri, "install:") ||
            g_str_has_prefix (struri, "install-ghelp:") ||
            g_str_has_prefix (struri, "install-help:")) {
            view_install_uri (view, struri);
        }
        else {
            gboolean result;
            g_signal_emit (view, signals[EXTERNAL_URI], 0, uri, &result);
        }
        g_free (struri);
        return;
    case YELP_URI_DOCUMENT_TYPE_NOT_FOUND:
        struri = yelp_uri_get_canonical_uri (uri);
        if (struri != NULL) {
            error = g_error_new (YELP_ERROR, YELP_ERROR_NOT_FOUND,
                                 _("The URI ‘%s’ does not point to a valid page."),
                                 struri);
            g_free (struri);
        }
        else {
            error = g_error_new (YELP_ERROR, YELP_ERROR_NOT_FOUND,
                                 _("The URI does not point to a valid page."));
        }
        break;
    case YELP_URI_DOCUMENT_TYPE_ERROR:
        struri = yelp_uri_get_canonical_uri (uri);
        error = g_error_new (YELP_ERROR, YELP_ERROR_PROCESSING,
                             _("The URI ‘%s’ could not be parsed."),
                             struri);
        g_free (struri);
        break;
    default:
        break;
    }

    if (error == NULL) {
        document  = yelp_document_get_for_uri (uri);
        if (priv->document)
            g_object_unref (priv->document);
        priv->document = document;
    }
    else {
        if (priv->document != NULL) {
            g_object_unref (priv->document);
            priv->document = NULL;
        }
    }

    if (!priv->back_load) {
        back = g_new0 (YelpBackEntry, 1);
        back->uri = g_object_ref (uri);
        while (priv->back_list != priv->back_cur) {
            back_entry_free ((YelpBackEntry *) priv->back_list->data);
            priv->back_list = g_list_delete_link (priv->back_list, priv->back_list);
        }
        priv->back_list = g_list_prepend (priv->back_list, back);
        priv->back_cur = priv->back_list;
    }
    priv->back_load = FALSE;

    action = gtk_action_group_get_action (priv->action_group, "YelpViewGoBack");
    gtk_action_set_sensitive (action, FALSE);
    proxies = gtk_action_get_proxies (action);
    if (priv->back_cur->next && priv->back_cur->next->data) {
        gchar *tooltip = "";
        back = priv->back_cur->next->data;

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
        for (cur = proxies; cur != NULL; cur = cur->next)
            gtk_widget_set_tooltip_markup (GTK_WIDGET (cur->data), tooltip);
    }
    else {
        for (cur = proxies; cur != NULL; cur = cur->next)
            gtk_widget_set_tooltip_text (GTK_WIDGET (cur->data), "");
    }

    action = gtk_action_group_get_action (priv->action_group, "YelpViewGoForward");
    gtk_action_set_sensitive (action, FALSE);
    proxies = gtk_action_get_proxies (action);
    if (priv->back_cur->prev && priv->back_cur->prev->data) {
        gchar *tooltip = "";
        back = priv->back_cur->prev->data;

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
        for (cur = proxies; cur != NULL; cur = cur->next)
            gtk_widget_set_tooltip_markup (GTK_WIDGET (cur->data), tooltip);
    }
    else {
        for (cur = proxies; cur != NULL; cur = cur->next)
            gtk_widget_set_tooltip_text (GTK_WIDGET (cur->data), "");
    }

    spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                         "yelp-uri");
    g_signal_emit_by_name (view, "notify::yelp-uri", spec);

    g_free (priv->page_id);
    priv->page_id = NULL;
    if (priv->uri != NULL)
        priv->page_id = yelp_uri_get_page_id (priv->uri);
    spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                         "page-id");
    g_signal_emit_by_name (view, "notify::page-id", spec);

    g_free (priv->root_title);
    g_free (priv->page_title);
    g_free (priv->page_desc);
    g_free (priv->page_icon);
    priv->root_title = NULL;
    priv->page_title = NULL;
    priv->page_desc = NULL;
    priv->page_icon = NULL;

    spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                         "root-title");
    g_signal_emit_by_name (view, "notify::root-title", spec);

    spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                         "page-title");
    g_signal_emit_by_name (view, "notify::page-title", spec);

    spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                         "page-desc");
    g_signal_emit_by_name (view, "notify::page-desc", spec);

    spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                         "page-icon");
    g_signal_emit_by_name (view, "notify::page-icon", spec);

    if (error == NULL)
        view_load_page (view);
    else {
        view_show_error_page (view, error);
        g_error_free (error);
    }
}

static void
document_callback (YelpDocument       *document,
                   YelpDocumentSignal  signal,
                   YelpView           *view,
                   GError             *error)
{
    YelpViewPrivate *priv = GET_PRIV (view);

    debug_print (DB_FUNCTION, "entering\n");

    if (signal == YELP_DOCUMENT_SIGNAL_INFO) {
        gchar *prev_id, *next_id, *real_id;
        GtkAction *action;
        YelpBackEntry *back = NULL;
        GParamSpec *spec;

        real_id = yelp_document_get_page_id (document, priv->page_id);
        if (priv->page_id && real_id && g_str_equal (real_id, priv->page_id)) {
            g_free (real_id);
        }
        else {
            GParamSpec *spec;
            g_free (priv->page_id);
            priv->page_id = real_id;
            spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                                 "page-id");
            g_signal_emit_by_name (view, "notify::page-id", spec);
        }

        g_free (priv->root_title);
        g_free (priv->page_title);
        g_free (priv->page_desc);
        g_free (priv->page_icon);

        priv->root_title = yelp_document_get_root_title (document, priv->page_id);
        priv->page_title = yelp_document_get_page_title (document, priv->page_id);
        priv->page_desc = yelp_document_get_page_desc (document, priv->page_id);
        priv->page_icon = yelp_document_get_page_icon (document, priv->page_id);

        if (priv->back_cur)
            back = priv->back_cur->data;
        if (back) {
            g_free (back->title);
            back->title = g_strdup (priv->page_title);
            g_free (back->desc);
            back->desc = g_strdup (priv->page_desc);
        }

        prev_id = yelp_document_get_prev_id (document, priv->page_id);
        action = gtk_action_group_get_action (priv->action_group, "YelpViewGoPrevious");
        gtk_action_set_sensitive (action, prev_id != NULL);
        g_free (prev_id);

        next_id = yelp_document_get_next_id (document, priv->page_id);
        action = gtk_action_group_get_action (priv->action_group, "YelpViewGoNext");
        gtk_action_set_sensitive (action, next_id != NULL);
        g_free (next_id);

        spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                             "root-title");
        g_signal_emit_by_name (view, "notify::root-title", spec);

        spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                             "page-title");
        g_signal_emit_by_name (view, "notify::page-title", spec);

        spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                             "page-desc");
        g_signal_emit_by_name (view, "notify::page-desc", spec);

        spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                             "page-icon");
        g_signal_emit_by_name (view, "notify::page-icon", spec);
    }
    else if (signal == YELP_DOCUMENT_SIGNAL_CONTENTS) {
        YelpUriDocumentType doctype;
	const gchar *contents;
        gchar *mime_type, *page_id, *frag_id, *full_uri;
        page_id = yelp_uri_get_page_id (priv->uri);
        debug_print (DB_ARG, "    document.uri.page_id=\"%s\"\n", page_id);
        mime_type = yelp_document_get_mime_type (document, page_id);
        contents = yelp_document_read_contents (document, page_id);
        frag_id = yelp_uri_get_frag_id (priv->uri);
        g_free (priv->bogus_uri);
        /* We don't have actual page and frag IDs for DocBook. We just map IDs
           of block elements.  The result is that we get xref:someid#someid.
           If someid is really the page ID, we just drop the frag reference.
           Otherwise, normal page views scroll past the link trail.
         */
        if (frag_id != NULL) {
            if (YELP_IS_DOCBOOK_DOCUMENT (document)) {
                gchar *real_id = yelp_document_get_page_id (document, page_id);
                if (g_str_equal (real_id, frag_id)) {
                    g_free (frag_id);
                    frag_id = NULL;
                }
                g_free (real_id);
            }
        }
        /* We have to give WebKit a URI in a scheme it understands, otherwise we
           won't get the resource-request-starting signal.  So we can't use the
           canonical URI, because it might be something like ghelp.  We also have
           to give it something unique, because WebKit ignores our load_string
           call if the URI isn't different.  We could try to construct something
           based on actual file locations, but in fact it doesn't matter.  So
           we just make a bogus URI that's easy to process later.
         */
        doctype = yelp_uri_get_document_type (priv->uri);
        full_uri = yelp_uri_get_canonical_uri (priv->uri);
        if (g_str_has_prefix (full_uri, "file:/") &&
            (doctype == YELP_URI_DOCUMENT_TYPE_TEXT ||
             doctype == YELP_URI_DOCUMENT_TYPE_HTML ||
             doctype == YELP_URI_DOCUMENT_TYPE_XHTML )) {
            priv->bogus_uri = full_uri;
        }
        else {
            g_free (full_uri);
            if (frag_id != NULL)
                priv->bogus_uri = g_strdup_printf ("%s%p#%s", BOGUS_URI, priv->uri, frag_id);
            else
                priv->bogus_uri = g_strdup_printf ("%s%p", BOGUS_URI, priv->uri);
        }
        g_signal_handler_block (view, priv->navigation_requested);
        webkit_web_view_load_string (WEBKIT_WEB_VIEW (view),
                                     contents,
                                     mime_type,
                                     "UTF-8",
                                     priv->bogus_uri);
        g_signal_handler_unblock (view, priv->navigation_requested);
        g_object_set (view, "state", YELP_VIEW_STATE_LOADED, NULL);

        /* If we need to set the GtkAdjustment or trigger the page title
         * from what WebKit thinks it is (see comment below), we need to
         * let the main loop run through.
         */
        if (priv->vadjust > 0 || priv->hadjust > 0 ||  priv->page_title == NULL)
            while (g_main_context_pending (NULL)) {
                WebKitLoadStatus status;
                status = webkit_web_view_get_load_status (WEBKIT_WEB_VIEW (view));
                g_main_context_iteration (NULL, FALSE);
                /* Sometimes some runaway JavaScript causes there to always
                 * be pending sources. Break out if the document is loaded.
                 */
                if (status == WEBKIT_LOAD_FINISHED ||
                    status == WEBKIT_LOAD_FAILED)
                    break;
            }

        /* Setting adjustments only work after the page is loaded. These
         * are set by view_history_action, and they're reset to 0 after
         * each load here.
         */
        if (priv->vadjust > 0) {
            if (priv->vadjustment)
                gtk_adjustment_set_value (priv->vadjustment, priv->vadjust);
            priv->vadjust = 0;
        }
        if (priv->hadjust > 0) {
            if (priv->hadjustment)
                gtk_adjustment_set_value (priv->hadjustment, priv->hadjust);
            priv->hadjust = 0;
        }

        /* If the document didn't give us a page title, get it from WebKit.
         * We let the main loop run through so that WebKit gets the title
         * set so that we can send notify::page-title before loaded. It
         * simplifies things if YelpView consumers can assume the title
         * is set before loaded is triggered.
         */
        if (priv->page_title == NULL) {
            GParamSpec *spec;
            priv->page_title = g_strdup (webkit_web_view_get_title (WEBKIT_WEB_VIEW (view)));
            spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                                 "page-title");
            g_signal_emit_by_name (view, "notify::page-title", spec);
        }

        g_free (frag_id);
        g_free (page_id);
        g_free (mime_type);
	yelp_document_finish_read (document, contents);
        g_signal_emit (view, signals[LOADED], 0);
    }
    else if (signal == YELP_DOCUMENT_SIGNAL_ERROR) {
        view_show_error_page (view, error);
    }
}
