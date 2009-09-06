/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2004 Marco Pesenti Gritti
 * Copyright (C) 2005 Christian Persch
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
 * Author: Marco Pesenti Gritti <marco@gnome.org>
 */

#include <config.h>
#include "yelp-marshal.h"
#include "yelp-settings.h"

#include <string.h>
#include <webkit/webkitwebframe.h>
#include <webkit/webkitnetworkrequest.h>
#include <webkit/webkitwebview.h>
#include <webkit/webkitwebsettings.h>

#include "yelp-html.h"
#include "yelp-debug.h"


#define YELP_HTML_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_HTML, YelpHtmlPriv))

struct _YelpHtmlPriv {
    gchar *content;
    gchar *mime;
    gchar       *find_string;
    gboolean    initialised;
    gchar       *base_uri;
    gchar       *anchor;
    gboolean     frames_enabled;
    guint        timeout;
};

static void      html_set_fonts          (YelpHtml *html);
static void      html_set_colors         (YelpHtml *html);
static void      html_set_a11y           (YelpHtml *html);

enum {
    URI_SELECTED,
    FRAME_SELECTED,
    TITLE_CHANGED,
    POPUPMENU_REQUESTED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

static gboolean
html_open_uri (WebKitWebView* view,
	       WebKitWebFrame* web_frame,
	       WebKitNetworkRequest* req,
	       WebKitWebNavigationAction* action,
	       WebKitWebPolicyDecision* decision,
	       gpointer data)
{
    const gchar *uri = webkit_network_request_get_uri (req);
    WebKitNavigationResponse resp = WEBKIT_NAVIGATION_RESPONSE_IGNORE;
    YelpHtml *html = YELP_HTML (view);
    gboolean block_load;
    gchar *real_uri;

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  uri = \"%s\"\n", uri);

    /* Only emit our signals on clicks */
    if (webkit_web_navigation_action_get_reason (action) != WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED) {
	webkit_web_policy_decision_use (decision);
	return TRUE;
    }

    real_uri = g_strdup (uri);

    /* If we got an URI with an anchor, that means we got to a
     * reference; we want to get what comes after #, and replace what
     * is after ? in the base URI we have
     */
    if (g_str_has_prefix (uri, html->priv->base_uri)) {
	gint length = strlen (html->priv->base_uri);

	if (uri[length] == '#') {
	    gchar *question_mark = g_strrstr (real_uri, "?");
	    gchar *tmp = real_uri;

	    *question_mark = '\0';
	    real_uri = g_strdup_printf ("%s?%s", tmp, uri + length + 1);

	    g_free (tmp);
	}
    }

    if (!html->priv->frames_enabled) {
  	g_signal_emit (html, signals[URI_SELECTED], 0, real_uri, FALSE);
    } else {
  	g_signal_emit (html, signals[FRAME_SELECTED], 0, real_uri, FALSE, &block_load);
    }

    g_free (real_uri);
    webkit_web_policy_decision_ignore (decision);
    return TRUE;
}

#ifdef HAVE_GECKO_1_9
static void
html_reset_accessible_parent (GtkWidget *widget)
{
    AtkObject * html_acc = gtk_widget_get_accessible (widget);
    AtkObject * parent_acc = gtk_widget_get_accessible (widget->parent);
    if (html_acc && parent_acc) {
	atk_object_set_parent (html_acc, parent_acc);
    }
}
#endif

static void
html_realize (GtkWidget *widget)
{
#ifdef HAVE_GECKO_1_9
    /* When Gecko accessibility module init, it will overwrite 
     * atk_class->get_root.
     * But the top level accessible of yelp is not controlled by Gecko.
     * So we need to restore the callback. See Bug #545162.
     * It only need to do once.
     * We do it here because Gecko a11y module inits when it is actually used,
     * we call gtk_widget_get_accessible to pull the trigger. */

    static gboolean gail_get_root_restored = FALSE;
    static AtkObject * (*gail_get_root) (void);
    static AtkUtilClass * atk_class = NULL;
    if (!gail_get_root_restored) {
	gpointer data;
	data = g_type_class_peek (ATK_TYPE_UTIL);
	if (data) {
	    atk_class = ATK_UTIL_CLASS (data);
	    gail_get_root = atk_class->get_root;
	}
    }
#endif

    YelpHtml *html = YELP_HTML (widget);

    GTK_WIDGET_CLASS (parent_class)->realize (widget);

}

static void
html_init (YelpHtml *html)
{
    YelpHtmlPriv  *priv;
    YelpHtmlClass *klass;

    html->priv = priv = YELP_HTML_GET_PRIVATE (html);

    priv->base_uri = NULL;
    priv->anchor = NULL;
    priv->timeout = 0;
    priv->content = NULL;
    priv->mime = NULL;
    priv->initialised = FALSE;

    klass = YELP_HTML_GET_CLASS (html);
    if (!klass->font_handler) {
	klass->font_handler =
	    yelp_settings_notify_add (YELP_SETTINGS_INFO_FONTS,
				      (GHookFunc) html_set_fonts,
				      html);
	html_set_fonts (html);
    }
    if (!klass->color_handler) {
	klass->color_handler =
	    yelp_settings_notify_add (YELP_SETTINGS_INFO_COLOR,
				      (GHookFunc) html_set_colors,
				      html);
	html_set_colors (html);
    }
    if (!klass->a11y_handler) {
	klass->a11y_handler =
	    yelp_settings_notify_add (YELP_SETTINGS_INFO_A11Y,
				      (GHookFunc) html_set_a11y,
				      html);
	html_set_a11y (html);
    }

    g_signal_connect (html, "navigation-policy-decision-requested",
		      G_CALLBACK (html_open_uri), NULL);
}

static void
html_dispose (GObject *object)
{
    YelpHtml *html = YELP_HTML (object);

    parent_class->dispose (object);
}

static void
html_finalize (GObject *object)
{
    YelpHtml *html = YELP_HTML (object);
    YelpHtmlPriv *priv = html->priv;


    if (priv->timeout)
	g_source_remove (priv->timeout);
    g_free (priv->base_uri);
    g_free (priv->anchor);

    g_signal_handlers_disconnect_by_func (html, html_open_uri, NULL);

    parent_class->finalize (object);
}

static void
html_class_init (YelpHtmlClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    WebKitWebViewClass* wc_class = WEBKIT_WEB_VIEW_CLASS (klass); 

    parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

    object_class->finalize = html_finalize;
    object_class->dispose = html_dispose;

    widget_class->realize = html_realize;


    klass->font_handler = 0;
    klass->color_handler = 0;
    klass->a11y_handler = 0;

    signals[URI_SELECTED] = 
	g_signal_new ("uri_selected",
		      G_TYPE_FROM_CLASS (klass),
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (YelpHtmlClass,
				       uri_selected),
		      NULL, NULL,
		      yelp_marshal_VOID__POINTER_BOOLEAN,
		      G_TYPE_NONE,
		      2, G_TYPE_POINTER, G_TYPE_BOOLEAN);
    
    signals[FRAME_SELECTED] =
	g_signal_new ("frame_selected",
		      G_TYPE_FROM_CLASS (klass),
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (YelpHtmlClass,
				       frame_selected),
		      g_signal_accumulator_true_handled, NULL,
		      yelp_marshal_BOOLEAN__POINTER_BOOLEAN,
		      G_TYPE_BOOLEAN,
		      2, G_TYPE_POINTER, G_TYPE_BOOLEAN);
    

    signals[POPUPMENU_REQUESTED] = 
	g_signal_new ("popupmenu_requested",
		      G_TYPE_FROM_CLASS (klass),
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (YelpHtmlClass,
				       popupmenu_requested),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__STRING,
		      G_TYPE_NONE,
		      1, G_TYPE_STRING);

    g_type_class_add_private (klass, sizeof (YelpHtmlPriv));
}

GType
yelp_html_get_type (void)
{
    static GType type = 0;

    if (G_UNLIKELY (type == 0)) {
	static const GTypeInfo info = {
	    sizeof (YelpHtmlClass),
	    NULL,
	    NULL,
	    (GClassInitFunc) html_class_init,
	    NULL,
	    NULL,
	    sizeof (YelpHtml),
	    0,
	    (GInstanceInitFunc) html_init,
	};

	type = g_type_register_static (WEBKIT_TYPE_WEB_VIEW,
				       "YelpHtml", 
				       &info, (GTypeFlags) 0);
    }

    return type;
}

YelpHtml *
yelp_html_new (void)
{
    YelpHtml *html;

    html = YELP_HTML (g_object_new (YELP_TYPE_HTML, NULL));

    return html;
}

void
yelp_html_set_base_uri (YelpHtml *html, const gchar *uri)
{
    YelpHtmlPriv *priv;

    g_return_if_fail (YELP_IS_HTML (html));

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  uri = \"%s\"\n", uri);

    priv = html->priv;

    if (priv->base_uri)
	g_free (priv->base_uri);

    if (uri[0] == '/')
	priv->base_uri = g_strdup_printf ("file://%s", uri);
    else
	priv->base_uri = g_strdup (uri);
}

void
yelp_html_open_stream (YelpHtml *html, const gchar *mime)
{
    debug_print (DB_FUNCTION, "entering\n");

    html->priv->frames_enabled = FALSE;
    g_free (html->priv->content);
    html->priv->content = NULL;
    g_free (html->priv->mime);
    html->priv->mime = g_strdup(mime);
}

void
yelp_html_write (YelpHtml *html, const gchar *data, gint len)
{
    gchar *tmp = NULL;

    if (len == -1) len = strlen (data);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  data = %i bytes\n", strlen (data));
    debug_print (DB_ARG, "  len  = %i\n", len);

    if (html->priv->content) {
	tmp = g_strjoin (NULL, html->priv->content, data, NULL);
	g_free (html->priv->content);
	html->priv->content = tmp;
    } else {
	html->priv->content = g_strdup (data);
    }
}

void
yelp_html_frames (YelpHtml *html, gboolean enable)
{
    html->priv->frames_enabled = enable;


}

void
yelp_html_printf (YelpHtml *html, char *format, ...)
{
    va_list  args;
    gchar   *string;

    g_return_if_fail (format != NULL);

    va_start (args, format);
    string = g_strdup_vprintf (format, args);
    va_end (args);

    yelp_html_write (html, string, -1);

    g_free (string);
}


void
yelp_html_close (YelpHtml *html)
{
    debug_print (DB_FUNCTION, "entering\n");

    if (!html->priv->initialised) {
	html->priv->initialised = TRUE;
	html_set_fonts (html);
	html_set_colors (html);
	html_set_a11y (html);
	webkit_web_view_set_maintains_back_forward_list (WEBKIT_WEB_VIEW (html), FALSE);
    }

    webkit_web_view_load_string (WEBKIT_WEB_VIEW (html),
				 html->priv->content,
				 html->priv->mime,
				 NULL,
				 html->priv->base_uri);
    g_free (html->priv->content);
    html->priv->content = NULL;
    g_free (html->priv->mime);
    html->priv->mime = NULL;
}

gboolean
yelp_html_find (YelpHtml    *html,
		const gchar *find_string)
{
    if (html->priv->find_string)
	g_free(html->priv->find_string);
    html->priv->find_string = g_strdup (find_string);
    return webkit_web_view_search_text (WEBKIT_WEB_VIEW (html),
					find_string, FALSE,
					TRUE, TRUE);
}

gboolean
yelp_html_find_again (YelpHtml    *html,					  
		     gboolean     forward)
{
    return webkit_web_view_search_text (WEBKIT_WEB_VIEW (html),
					html->priv->find_string, 
					FALSE,
					forward, TRUE);
}

void
yelp_html_set_find_props (YelpHtml    *html,
			  const char  *str,
			  gboolean     match_case,
			  gboolean     wrap)
{
    /* Empty */
}

void
yelp_html_jump_to_anchor (YelpHtml    *html,
			  gchar       *anchor)
{
    YelpHtmlPriv *priv;

    g_return_if_fail (html != NULL);

    priv = html->priv;

    g_free (priv->anchor);
    priv->anchor = g_strdup (anchor);
}

void
yelp_html_copy_selection (YelpHtml *html)
{
    webkit_web_view_copy_clipboard (WEBKIT_WEB_VIEW (html));
}

void
yelp_html_select_all (YelpHtml *html)
{
    webkit_web_view_select_all (WEBKIT_WEB_VIEW (html));
}

void
yelp_html_print (YelpHtml *html)
{
    webkit_web_view_execute_script (WEBKIT_WEB_VIEW (html), "print();");
}

static void
html_set_fonts (YelpHtml *html)
{
    gchar *font;
    WebKitWebSettings *settings;
    GValue *name, *size;
    gchar *str_name;
    gint i_size;
    gchar *tmp;

    settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (html));

    font = yelp_settings_get_font (YELP_FONT_VARIABLE);

    /* We have to separate the string into name and size and then
     * assign to the 2 gvalues */
    tmp = g_strrstr (font, " ");
    if (!tmp) {
	g_warning ("Cannot decode font pattern %s", font);
	g_free (font);
	return;
    }

    name = g_new0 (GValue, 1);
    size = g_new0 (GValue, 1);

    name = g_value_init (name, G_TYPE_STRING);
    size = g_value_init (size, G_TYPE_INT);

    str_name = g_strndup (font, tmp - font);

    i_size = g_strtod (tmp, NULL);

    g_value_set_string (name, str_name);
    g_value_set_int (size, i_size);

    g_object_set_property (G_OBJECT (settings), "default-font-family",
			   name);
    g_object_set_property (G_OBJECT (settings), "default-font-size",
			   size);

    g_free (font);

    font = yelp_settings_get_font (YELP_FONT_FIXED);

    tmp = g_strrstr (font, " ");
    if (!tmp) {
	g_warning ("Cannot decode monospace font pattern %s", font);
	g_free (font);
	return; 
   }

    name = g_value_reset (name);
    size = g_value_reset (size);

    str_name = g_strndup (font, tmp - font);

    i_size = g_strtod (tmp, NULL);

    g_value_set_string (name, str_name);
    g_value_set_int (size, i_size);
    

    g_object_set_property (G_OBJECT (settings), "monospace-font-family",
			   name);
    g_object_set_property (G_OBJECT (settings), "default-monospace-font-size",
			   size);

    g_free (font);
}

static void
html_set_colors (YelpHtml *html)
{
    /* TODO: No Webkit equivalent ... */
    /* See https://bugs.webkit.org/show_bug.cgi?id=19486 */

}

static void
html_set_a11y (YelpHtml *html)
{
    gboolean caret;

    caret = yelp_settings_get_caret ();
    /* TODO Webkit version */
}

gboolean
yelp_html_initialize (void)
{
    return TRUE;
}

void
yelp_html_shutdown (void)
{
    /* Empty */
}
