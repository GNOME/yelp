/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2014 Igalia S.L.
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
 */

#include <webkit2/webkit-web-extension.h>
#include <webkitdom/webkitdom.h>
#include <string.h>
#include <stdlib.h>
#include "yelp-uri.h"
#include "yelp-uri-builder.h"

static YelpUri *current_uri;

static gchar *
get_resource_path (gchar *uri, YelpUri *current_uri)
{
    gchar *doc_uri;
    gchar *resource = NULL;

    if (!g_str_has_prefix (uri, "ghelp") &&
        !g_str_has_prefix (uri, "gnome-help") &&
        !g_str_has_prefix (uri, "help")) {
        return NULL;
    }

    doc_uri = yelp_uri_get_document_uri (current_uri);
    if (g_str_has_prefix (uri, doc_uri)) {
        /* If the uri starts with the document uri,
         * simply remove the document uri to get the
         * resource path.
         */
        uri[strlen (doc_uri)] = '\0';
        resource = uri + strlen (doc_uri) + 1;
    } else {
        /* If the uri doesn't contain the document uri,
         * the full path is the resource path.
         */
        resource = strstr (uri, ":");
        if (resource) {
            resource[0] = '\0';
            resource++;
        }
    }
    g_free (doc_uri);

    if (resource && resource[0] != '\0')
        return yelp_uri_locate_file_uri (current_uri, resource);

    return NULL;
}

static gboolean
web_page_send_request (WebKitWebPage     *web_page,
                       WebKitURIRequest  *request,
                       WebKitURIResponse *redirected_response,
                       gpointer           user_data)
{
    const gchar *resource_uri = webkit_uri_request_get_uri (request);
    gchar *yelp_uri, *file_path;

    if (!current_uri)
        return FALSE;

    /* Main resource */
    if (strcmp (resource_uri, webkit_web_page_get_uri (web_page)) == 0)
        return FALSE;

    yelp_uri = build_yelp_uri (resource_uri);
    file_path = get_resource_path (yelp_uri, current_uri);
    if (file_path) {
        webkit_uri_request_set_uri (request, file_path);
        g_free (file_path);
    }
    g_free (yelp_uri);

    return FALSE;
}

static void
web_page_notify_uri (WebKitWebPage *web_page,
                     GParamSpec    *pspec,
                     gpointer       data)
{
    const gchar *uri = webkit_web_page_get_uri (web_page);
    gchar *yelp_uri;

    yelp_uri = build_yelp_uri (uri);

    if (current_uri)
        g_object_unref (current_uri);
    current_uri = yelp_uri_new (yelp_uri);

    if (!yelp_uri_is_resolved (current_uri))
        yelp_uri_resolve_sync (current_uri);

    g_free (yelp_uri);
}

static gboolean
web_page_context_menu (WebKitWebPage          *web_page,
                       WebKitContextMenu      *context_menu,
                       WebKitWebHitTestResult *hit_test_result)
{
    WebKitDOMNode *node, *cur, *link_node = NULL, *code_node = NULL, *code_title_node = NULL;
    gchar *popup_link_text = NULL;
    GVariantDict user_data_dict;

    node = webkit_web_hit_test_result_get_node (hit_test_result);

    for (cur = node; cur != NULL; cur = webkit_dom_node_get_parent_node (cur)) {
        if (WEBKIT_DOM_IS_ELEMENT (cur) &&
            webkit_dom_element_webkit_matches_selector (WEBKIT_DOM_ELEMENT (cur),
                                                        "a", NULL))
            link_node = cur;

        if (WEBKIT_DOM_IS_ELEMENT (cur) &&
            webkit_dom_element_webkit_matches_selector (WEBKIT_DOM_ELEMENT (cur),
                                                        "div.code", NULL)) {
            WebKitDOMNode *title;
            code_node = WEBKIT_DOM_NODE (
                webkit_dom_element_query_selector (WEBKIT_DOM_ELEMENT (cur),
                                                   "pre.contents", NULL));
            title = webkit_dom_node_get_parent_node (cur);
            if (WEBKIT_DOM_IS_ELEMENT (title) &&
                webkit_dom_element_webkit_matches_selector (WEBKIT_DOM_ELEMENT (title),
                                                            "div.contents", NULL)) {
                title = webkit_dom_node_get_previous_sibling (title);
                if (WEBKIT_DOM_IS_ELEMENT (title) &&
                    webkit_dom_element_webkit_matches_selector (WEBKIT_DOM_ELEMENT (title),
                                                                "div.title", NULL)) {
                    code_title_node = title;
                }
            }
        }
    }

    if (webkit_hit_test_result_context_is_link (WEBKIT_HIT_TEST_RESULT (hit_test_result)) && link_node) {
        WebKitDOMNode *child;
        gchar *tmp;
        gint i, tmpi;
        gboolean ws;

        child = WEBKIT_DOM_NODE (
            webkit_dom_element_query_selector (WEBKIT_DOM_ELEMENT (link_node),
                                               "span.title", NULL));
        if (child)
            popup_link_text = webkit_dom_node_get_text_content (child);

        if (!popup_link_text)
            popup_link_text = webkit_dom_node_get_text_content (link_node);

        tmp = g_new0 (gchar, strlen (popup_link_text) + 1);
        ws = FALSE;
        for (i = 0, tmpi = 0; popup_link_text[i] != '\0'; i++) {
            if (popup_link_text[i] == ' ' || popup_link_text[i] == '\n') {
                if (!ws) {
                    tmp[tmpi] = ' ';
                    tmpi++;
                    ws = TRUE;
                }
            }
            else {
                tmp[tmpi] = popup_link_text[i];
                tmpi++;
                ws = FALSE;
            }
        }
        tmp[tmpi] = '\0';
        g_free (popup_link_text);
        popup_link_text = tmp;
    }

    if (!(popup_link_text || code_node || code_title_node))
        return FALSE;

    g_variant_dict_init (&user_data_dict, NULL);

    if (popup_link_text) {
        g_variant_dict_insert_value (&user_data_dict, "link-title",
            g_variant_new_take_string (popup_link_text));
    }

    if (code_node) {
        gchar *code_code = webkit_dom_node_get_text_content (code_node);
        g_variant_dict_insert_value (&user_data_dict, "code-text",
            g_variant_new_take_string (code_code));
    }

    if (code_title_node) {
        gchar *code_title = webkit_dom_node_get_text_content (code_title_node);
        g_variant_dict_insert_value (&user_data_dict, "code-title",
            g_variant_new_take_string (code_title));
    }

    webkit_context_menu_set_user_data (context_menu, g_variant_dict_end (&user_data_dict));

    return FALSE;
}

static void
web_page_created_callback (WebKitWebExtension *extension,
                           WebKitWebPage      *web_page,
                           gpointer            user_data)
{
    g_signal_connect (web_page, "context-menu",
                      G_CALLBACK (web_page_context_menu),
                      NULL);
    g_signal_connect (web_page, "send-request",
                      G_CALLBACK (web_page_send_request),
                      NULL);
    g_signal_connect (web_page, "notify::uri",
                      G_CALLBACK (web_page_notify_uri),
                      NULL);
}

G_MODULE_EXPORT void
webkit_web_extension_initialize (WebKitWebExtension *extension)
{
    g_signal_connect (extension, "page-created",
                      G_CALLBACK (web_page_created_callback),
                      NULL);
}
