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
#include <string.h>

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMElementUnstable.h>

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
}

G_MODULE_EXPORT void
webkit_web_extension_initialize (WebKitWebExtension *extension)
{
    g_signal_connect (extension, "page-created",
                      G_CALLBACK (web_page_created_callback),
                      NULL);
}
