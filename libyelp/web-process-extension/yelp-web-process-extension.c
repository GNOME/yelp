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

#include <webkit/webkit-web-process-extension.h>
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

#define JSC_ELEMENT_MATCHES(elem, match) \
    jsc_value_to_boolean (jsc_value_object_invoke_method (elem, "matches",       \
                                                          G_TYPE_STRING, match,  \
                                                          G_TYPE_NONE))          \

#define JSC_GET_PARENT_ELEMENT(elem) \
    jsc_value_object_get_property (elem, "parentElement")

#define JSC_ELEMENT_QUERY_SELECTOR(elem, selector)           \
    jsc_value_object_invoke_method (elem, "querySelector",   \
                                    G_TYPE_STRING, selector, \
                                    G_TYPE_NONE)

#define JSC_GET_ELEMENT_TEXT_CONTENT(elem) \
    jsc_value_to_string (jsc_value_object_get_property (elem, "textContent"))


static gboolean
web_page_context_menu (WebKitWebPage          *web_page,
                       WebKitContextMenu      *context_menu,
                       WebKitWebHitTestResult *hit_test_result)
{
    JSCValue *node, *cur, *link_node = NULL, *code_node = NULL, *code_title_node = NULL;
    gchar *popup_link_text = NULL;
    GVariantDict user_data_dict;

    node = webkit_web_hit_test_result_get_js_node (hit_test_result, NULL);

    for (cur = node; !jsc_value_is_null (cur); cur = JSC_GET_PARENT_ELEMENT (cur)) {
        if (!jsc_value_is_null (cur) && JSC_ELEMENT_MATCHES (cur, "a"))
            link_node = cur;

        if (!jsc_value_is_null (cur) && JSC_ELEMENT_MATCHES (cur, "div.code")) {
            JSCValue *title;

            code_node = JSC_ELEMENT_QUERY_SELECTOR (cur, "pre.contents");
            title = JSC_GET_PARENT_ELEMENT (cur);
            if (!jsc_value_is_null (title) && JSC_ELEMENT_MATCHES (title, "div.contents")) {
                title = jsc_value_object_get_property (title, "previousElementSibling");
                if (!jsc_value_is_null (title) && JSC_ELEMENT_MATCHES (title, "div.title")) {
                    code_title_node = title;
                }
            }
        }
    }

    /*
     * TODO: The line below does not work due to what appears to be a WebKitGTK bug:
     * https://bugs.webkit.org/show_bug.cgi?id=281767
     * Remove this note once it is fixed.
     */
    if (webkit_web_hit_test_result_context_is_link (hit_test_result) && link_node) {
        JSCValue *child;
        gchar *tmp;
        gint i, tmpi;
        gboolean ws;

        child = JSC_ELEMENT_QUERY_SELECTOR (link_node, "span.title");
        if (!jsc_value_is_null (child))
            popup_link_text = JSC_GET_ELEMENT_TEXT_CONTENT(child);

        if (!popup_link_text)
            popup_link_text = JSC_GET_ELEMENT_TEXT_CONTENT(link_node);

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

    if (code_node && !jsc_value_is_null (code_node)) {
        gchar *code_code = JSC_GET_ELEMENT_TEXT_CONTENT(code_node);
        g_variant_dict_insert_value (&user_data_dict, "code-text",
            g_variant_new_take_string (code_code));
    }

    if (code_title_node && !jsc_value_is_null (code_title_node)) {
        gchar *code_title = JSC_GET_ELEMENT_TEXT_CONTENT(code_title_node);
        g_variant_dict_insert_value (&user_data_dict, "code-title",
            g_variant_new_take_string (code_title));
    }

    webkit_context_menu_set_user_data (context_menu, g_variant_dict_end (&user_data_dict));

    return FALSE;
}

static void
web_page_created_callback (WebKitWebProcessExtension *extension,
                           WebKitWebPage             *web_page,
                           gpointer                   user_data)
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
webkit_web_process_extension_initialize (WebKitWebProcessExtension *extension)
{
    g_signal_connect (extension, "page-created",
                      G_CALLBACK (web_page_created_callback),
                      NULL);
}
