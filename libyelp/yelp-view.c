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
#include <webkit/webkit.h>
#include <webkit/webkitwebresource.h>

#include "yelp-debug.h"
#include "yelp-docbook-document.h"
#include "yelp-error.h"
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

typedef struct _YelpViewPrivate YelpViewPrivate;
struct _YelpViewPrivate {
    YelpUri       *uri;
    gulong         uri_resolved;
    gchar         *bogus_uri;
    YelpDocument  *document;
    GCancellable  *cancellable;

    YelpViewState  state;

    gchar         *page_id;
    gchar         *root_title;
    gchar         *page_title;
    gchar         *page_desc;
    gchar         *page_icon;

    GList *back_list;
    GList *back_cur;
    gboolean back_load;

    GtkActionGroup *action_group;

    gint           navigation_requested;
};

#define TARGET_TYPE_URI_LIST     "text/uri-list"
enum {
    TARGET_URI_LIST
};

static void
yelp_view_init (YelpView *view)
{
    YelpViewPrivate *priv = GET_PRIV (view);

    g_object_set (view, "settings", websettings, NULL);

    priv->cancellable = NULL;

    priv->state = YELP_VIEW_STATE_BLANK;

    priv->navigation_requested =
        g_signal_connect (view, "navigation-policy-decision-requested",
                          G_CALLBACK (view_navigation_requested), NULL);
    g_signal_connect (view, "resource-request-starting",
                      G_CALLBACK (view_resource_request), NULL);

    priv->action_group = gtk_action_group_new ("YelpView");
    gtk_action_group_set_translation_domain (priv->action_group, GETTEXT_PACKAGE);
    gtk_action_group_add_actions (priv->action_group,
				  entries, G_N_ELEMENTS (entries),
				  view);
}

static void
yelp_view_dispose (GObject *object)
{
    YelpViewPrivate *priv = GET_PRIV (object);

    if (priv->uri) {
        g_object_unref (priv->uri);
        priv->uri = NULL;
    }

    if (priv->cancellable) {
        g_cancellable_cancel (priv->cancellable);
        g_object_unref (priv->cancellable);
        priv->cancellable = NULL;
    }

    if (priv->action_group) {
        g_object_unref (priv->action_group);
        priv->action_group = NULL;
    }

    if (priv->document) {
        g_object_unref (priv->document);
        priv->document = NULL;
    }

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

    websettings = webkit_web_settings_new ();
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

    object_class->dispose = yelp_view_dispose;
    object_class->finalize = yelp_view_finalize;
    object_class->get_property = yelp_view_get_property;
    object_class->set_property = yelp_view_set_property;

    signals[NEW_VIEW_REQUESTED] =
	g_signal_new ("new-view-requested",
		      G_TYPE_FROM_CLASS (klass),
		      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
		      g_cclosure_marshal_VOID__STRING,
		      G_TYPE_NONE, 1, G_TYPE_STRING);

    signals[EXTERNAL_URI] =
	g_signal_new ("external-uri",
		      G_TYPE_FROM_CLASS (klass),
		      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
		      g_cclosure_marshal_VOID__OBJECT,
		      G_TYPE_NONE, 1, YELP_TYPE_URI);

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
            g_value_set_string (value, priv->page_icon);
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
    GParamSpec *spec;

    view_clear_load (view);
    g_object_set (view, "state", YELP_VIEW_STATE_LOADING, NULL);

    g_free (priv->page_id);
    g_free (priv->root_title);
    g_free (priv->page_title);
    g_free (priv->page_desc);
    g_free (priv->page_icon);
    priv->page_id = NULL;
    priv->root_title = NULL;
    priv->page_title = NULL;
    priv->page_desc = NULL;
    priv->page_icon = NULL;

    spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                         "page-id");
    g_signal_emit_by_name (view, "notify::page-id", spec);

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

    gtk_action_set_sensitive (gtk_action_group_get_action (priv->action_group,
                                                           "YelpViewGoPrevious"),
                              FALSE);
    gtk_action_set_sensitive (gtk_action_group_get_action (priv->action_group,
                                                           "YelpViewGoNext"),
                              FALSE);

    priv->uri = g_object_ref (uri);
    if (!yelp_uri_is_resolved (uri)) {
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
    YelpViewPrivate *priv = GET_PRIV (view);

    g_return_if_fail (yelp_uri_is_resolved (uri));

    view_clear_load (view);
    g_object_set (view, "state", YELP_VIEW_STATE_LOADING, NULL);

    priv->uri = g_object_ref (uri);
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

    if (g_str_has_prefix (requri, BOGUS_URI))
        uri = yelp_uri_new_relative (priv->uri, requri + BOGUS_URI_LEN);
    else
        uri = yelp_uri_new_relative (priv->uri, requri);

    webkit_web_policy_decision_ignore (decision);

    yelp_view_load_uri ((YelpView *) view, uri);

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

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "    uri=\"%s\"\n", requri);

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

    if (priv->uri) {
        if (priv->uri_resolved != 0) {
            g_signal_handler_disconnect (priv->uri, priv->uri_resolved);
            priv->uri_resolved = 0;
        }
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
        "</style>"
        "</head><body>"
        "<div class='note'><div class='inner'>"
        "<div class='title'>%s</div>"
        "<div class='contents'>%s</div>"
        "</div></div>"
        "</body></html>";
    YelpSettings *settings = yelp_settings_get_default ();
    gchar *page, *title = NULL;
    gchar *textcolor, *bgcolor, *noteborder, *notebg, *titlecolor, *noteicon;
    gint iconsize;
    const gchar *left = (gtk_widget_get_direction((GtkWidget *) view) == GTK_TEXT_DIR_RTL) ? "right" : "left";
    if (error->domain == YELP_ERROR)
        switch (error->code) {
        case YELP_ERROR_NOT_FOUND:
            title = _("Not Found");
            break;
        case YELP_ERROR_CANT_READ:
            title = _("Cannot Read");
            break;
        default:
            break;
        }
    if (title == NULL)
        title = _("Unknown Error");
    textcolor = yelp_settings_get_color (settings, YELP_SETTINGS_COLOR_TEXT);
    bgcolor = yelp_settings_get_color (settings, YELP_SETTINGS_COLOR_BASE);
    noteborder = yelp_settings_get_color (settings, YELP_SETTINGS_COLOR_RED_BORDER);
    notebg = yelp_settings_get_color (settings, YELP_SETTINGS_COLOR_YELLOW_BASE);
    titlecolor = yelp_settings_get_color (settings, YELP_SETTINGS_COLOR_TEXT_LIGHT);
    noteicon = yelp_settings_get_icon (settings, YELP_SETTINGS_ICON_WARNING);
    iconsize = yelp_settings_get_icon_size (settings) + 6;
    page = g_strdup_printf (errorpage,
                            textcolor, bgcolor, noteborder, notebg, noteicon,
                            left, iconsize, left, iconsize, left, iconsize,
                            titlecolor, title, error->message);
    g_object_set (view, "state", YELP_VIEW_STATE_ERROR, NULL);
    g_signal_handler_block (view, priv->navigation_requested);
    webkit_web_view_load_string (WEBKIT_WEB_VIEW (view),
                                 page,
                                 "text/html",
                                 "UTF-8",
                                 "file:///error/");
    g_signal_handler_unblock (view, priv->navigation_requested);
    g_error_free (error);
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
    GError *error;
    gchar *struri;
    GParamSpec *spec;

    debug_print (DB_FUNCTION, "entering\n");

    switch (yelp_uri_get_document_type (uri)) {
    case YELP_URI_DOCUMENT_TYPE_EXTERNAL:
        g_signal_emit (view, signals[EXTERNAL_URI], 0, uri);
        return;
    case YELP_URI_DOCUMENT_TYPE_NOT_FOUND:
        struri = yelp_uri_get_canonical_uri (uri);
        error = g_error_new (YELP_ERROR, YELP_ERROR_NOT_FOUND,
                             _("The URI ‘%s’ does point to a valid page."),
                             struri);
        g_free (struri);
        view_show_error_page (view, error);
        return;
    case YELP_URI_DOCUMENT_TYPE_ERROR:
        struri = yelp_uri_get_canonical_uri (uri);
        error = g_error_new (YELP_ERROR, YELP_ERROR_PROCESSING,
                             _("The URI ‘%s’ could not be parsed."),
                             struri);
        g_free (struri);
        view_show_error_page (view, error);
        return;
    default:
        break;
    }

    document  = yelp_document_get_for_uri (uri);
    if (priv->document)
        g_object_unref (priv->document);
    priv->document = document;

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
    priv->page_id = yelp_uri_get_page_id (priv->uri);
    spec = g_object_class_find_property ((GObjectClass *) YELP_VIEW_GET_CLASS (view),
                                         "page-id");
    g_signal_emit_by_name (view, "notify::page-id", spec);

    view_load_page (view);
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
        if (priv->page_id && g_str_equal (real_id, priv->page_id)) {
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
	const gchar *contents;
        gchar *mime_type, *page_id, *frag_id;
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
        if (frag_id != NULL)
            priv->bogus_uri = g_strdup_printf ("%s%p#%s", BOGUS_URI, priv->uri, frag_id);
        else
            priv->bogus_uri = g_strdup_printf ("%s%p", BOGUS_URI, priv->uri);
        g_signal_handler_block (view, priv->navigation_requested);
        webkit_web_view_load_string (WEBKIT_WEB_VIEW (view),
                                     contents,
                                     mime_type,
                                     "UTF-8",
                                     priv->bogus_uri);
        g_signal_handler_unblock (view, priv->navigation_requested);
        g_object_set (view, "state", YELP_VIEW_STATE_LOADED, NULL);
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
