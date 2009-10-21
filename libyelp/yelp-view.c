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
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <webkit/webkitwebresource.h>

#include "yelp-debug.h"
#include "yelp-error.h"
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

static void        view_clear_load                (YelpView           *view);
static void        view_load_page                 (YelpView           *view);
static void        view_show_error_page           (YelpView           *view,
                                                   GError             *error);

static void        uri_resolved                   (YelpUri            *uri,
                                                   YelpView           *view);
static void        document_callback              (YelpDocument       *document,
                                                   YelpDocumentSignal  signal,
                                                   YelpView           *view,
                                                   GError             *error);

enum {
    PROP_0,
    PROP_STATE
};

enum {
    NEW_VIEW_REQUESTED,
    LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (YelpView, yelp_view, WEBKIT_TYPE_WEB_VIEW);
#define GET_PRIV(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_VIEW, YelpViewPrivate))

typedef struct _YelpViewPrivate YelpViewPrivate;
struct _YelpViewPrivate {
    YelpUri       *uri;
    gulong         uri_resolved;
    gchar         *bogus_uri;
    YelpDocument  *document;
    GCancellable  *cancellable;

    YelpViewState  state;

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

    priv->cancellable = NULL;

    priv->state = YELP_VIEW_STATE_BLANK;

    priv->navigation_requested =
        g_signal_connect (view, "navigation-policy-decision-requested",
                          G_CALLBACK (view_navigation_requested), NULL);
    g_signal_connect (view, "resource-request-starting",
                      G_CALLBACK (view_resource_request), NULL);
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

    if (priv->document) {
        g_object_unref (priv->document);
        priv->document = NULL;
    }

    G_OBJECT_CLASS (yelp_view_parent_class)->dispose (object);
}

static void
yelp_view_finalize (GObject *object)
{
    YelpViewPrivate *priv = GET_PRIV (object);

    g_free (priv->bogus_uri);

    G_OBJECT_CLASS (yelp_view_parent_class)->finalize (object);
}

static void
yelp_view_class_init (YelpViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = yelp_view_dispose;
    object_class->finalize = yelp_view_finalize;
    object_class->get_property = yelp_view_get_property;
    object_class->set_property = yelp_view_set_property;

    signals[NEW_VIEW_REQUESTED] =
	g_signal_new ("new_view_requested",
		      G_TYPE_FROM_CLASS (klass),
		      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
		      g_cclosure_marshal_VOID__STRING,
		      G_TYPE_NONE, 1, G_TYPE_STRING);

    g_type_class_add_private (klass, sizeof (YelpViewPrivate));

    g_object_class_install_property (object_class,
                                     PROP_STATE,
                                     g_param_spec_enum ("state",
                                                        N_("Loading State"),
                                                        N_("The loading state of the view"),
                                                        YELP_TYPE_VIEW_STATE,
                                                        YELP_VIEW_STATE_BLANK,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
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
    YelpViewPrivate *priv = GET_PRIV (object);

    switch (prop_id)
        {
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

    view_clear_load (view);
    g_object_set (view, "state", YELP_VIEW_STATE_LOADING, NULL);

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

/******************************************************************************/

static gboolean
view_navigation_requested (WebKitWebView             *view,
                           WebKitWebFrame            *frame,
                           WebKitNetworkRequest      *request,
                           WebKitWebNavigationAction *action,
                           WebKitWebPolicyDecision   *decision,
                           gpointer                   user_data)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    YelpUri *uri;

    debug_print (DB_FUNCTION, "entering\n");

    uri = yelp_uri_new_relative (priv->uri,
                                 webkit_network_request_get_uri (request));

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
        "body { margin: 1em; }"
        ".outer {"
        "  border: solid 2px #cc0000;"
        "  -webkit-border-radius: 6px;"
        "}"
        ".inner {"
        "  padding: 1em;"
        "  border: solid 2px white;"
        "  -webkit-border-radius: 6px;"
        "  background: #fce94f;"
        "}"
        "</style>"
        "</head><body>"
        "<div class='outer'><div class='inner'>"
        "<div class='title'>%s</div>"
        "<div class='contents'>%s</div>"
        "</div></div>"
        "</body></html>";
    gchar *page, *title = NULL;
    if (error->domain == YELP_ERROR)
        switch (error->code) {
        case YELP_ERROR_NOT_FOUND:
            title = _("Document or Page Not Found");
            break;
        case YELP_ERROR_CANT_READ:
            title = _("Cannot Read the Document or Page");
            break;
        default:
            break;
        }
    if (title == NULL)
        title = _("Unknown Error");
    page = g_strdup_printf (errorpage, title, error->message);
    g_object_set (view, "state", YELP_VIEW_STATE_ERROR, NULL);
    g_signal_handler_block (view, priv->navigation_requested);
    webkit_web_view_load_string (WEBKIT_WEB_VIEW (view),
                                 page,
                                 "text/html",
                                 "UTF-8",
                                 "about:error");
    g_signal_handler_unblock (view, priv->navigation_requested);
    g_error_free (error);
    g_free (page);
}

/******************************************************************************/

static void
uri_resolved (YelpUri  *uri,
              YelpView *view)
{
    YelpViewPrivate *priv = GET_PRIV (view);
    YelpDocument *document;

    debug_print (DB_FUNCTION, "entering\n");

    document  = yelp_document_get_for_uri (uri);
    if (priv->document)
        g_object_unref (priv->document);
    priv->document = document;

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
        /* FIXME */
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
    }
    else if (signal == YELP_DOCUMENT_SIGNAL_ERROR) {
        view_show_error_page (view, error);
    }
}
