/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2004 Marco Pesenti Gritti
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtkmozembed.h>
#include <string.h>

#include "yelp-html.h"
#include "yelp-marshal.h"
#include "yelp-gecko-utils.h"
#include "yelp-settings.h"

#ifdef YELP_DEBUG
#define d(x) x
#else
#define d(x)
#endif

struct _YelpHtmlPriv {
    GtkWidget	*embed;
    gchar       *base_uri;
    gchar       *anchor;
};

static void      html_init               (YelpHtml           *html);
static void      html_class_init         (YelpHtmlClass      *klass);
static void      html_set_fonts          (void);
static void      html_set_colors         (void);
static void      html_set_a11y           (void);

enum {
    URI_SELECTED,
    TITLE_CHANGED,
    POPUPMENU_REQUESTED,
    LAST_SIGNAL
};

static gint        signals[LAST_SIGNAL] = { 0 };

GType
yelp_html_get_type (void)
{
    static GType view_type = 0;

    if (!view_type) {
	static const GTypeInfo view_info = {
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

	view_type = g_type_register_static (G_TYPE_OBJECT,
					    "YelpHtml", 
					    &view_info, 0);
    }

    return view_type;
}

static void
embed_title_cb (GtkMozEmbed *embed, YelpHtml *html)
{
    char *new_title;

    new_title = gtk_moz_embed_get_title (embed);

    if (new_title && *new_title != '\0')
    {
        g_signal_emit (html, signals[TITLE_CHANGED], 0, new_title);
        g_free (new_title);
    }
}

static gint
embed_menu_cb (GtkMozEmbed *embed, gpointer dom_event, YelpHtml *html)
{
    gchar *uri = yelp_gecko_mouse_event (embed, dom_event);

    if (uri) {
	g_signal_emit (html, signals[POPUPMENU_REQUESTED], 0, uri);
    }

    return FALSE;
}

static gint
embed_open_uri_cb (GtkMozEmbed *embed, const gchar *uri, YelpHtml *html)
{
    g_return_val_if_fail (uri != NULL, FALSE);
    g_return_val_if_fail (YELP_IS_HTML (html), FALSE);

    d (g_print ("embed_open_uri_cb\n"));
    d (g_print ("  uri = \"%s\"\n", uri));

    g_signal_emit (html, signals[URI_SELECTED], 0, uri, FALSE);

    return TRUE;
}

static void
html_init (YelpHtml *html)
{
    YelpHtmlPriv  *priv;
    YelpHtmlClass *klass;

    priv = g_new0 (YelpHtmlPriv, 1);
    html->priv = priv;

    html->priv->base_uri = NULL;
    html->priv->embed = gtk_moz_embed_new ();

    klass = YELP_HTML_GET_CLASS (html);
    if (!klass->font_handler) {
	klass->font_handler =
	    yelp_settings_notify_add (YELP_SETTINGS_INFO_FONTS,
				      (GHookFunc) html_set_fonts,
				      NULL);
	html_set_fonts ();
    }
    if (!klass->color_handler) {
	klass->color_handler =
	    yelp_settings_notify_add (YELP_SETTINGS_INFO_COLOR,
				      (GHookFunc) html_set_colors,
				      NULL);
	html_set_colors ();
    }
    if (!klass->a11y_handler) {
	klass->a11y_handler =
	    yelp_settings_notify_add (YELP_SETTINGS_INFO_A11Y,
				      (GHookFunc) html_set_a11y,
				      NULL);
	html_set_a11y ();
    }

    g_signal_connect (html->priv->embed, "title",
		      G_CALLBACK (embed_title_cb),
		      html);
    g_signal_connect (html->priv->embed, "open_uri",
		      G_CALLBACK (embed_open_uri_cb),
		      html);
    g_signal_connect (html->priv->embed, "dom_mouse_down",
		      G_CALLBACK (embed_menu_cb),
		      html);
}

static void
html_class_init (YelpHtmlClass *klass)
{
    klass->font_handler = 0;

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

    signals[TITLE_CHANGED] = 
	g_signal_new ("title_changed",
		      G_TYPE_FROM_CLASS (klass),
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (YelpHtmlClass,
				       title_changed),
		      NULL, NULL,
		      yelp_marshal_VOID__STRING,
		      G_TYPE_NONE,
		      1, G_TYPE_STRING);

    signals[POPUPMENU_REQUESTED] = 
	g_signal_new ("popupmenu_requested",
		      G_TYPE_FROM_CLASS (klass),
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (YelpHtmlClass,
				       popupmenu_requested),
		      NULL, NULL,
		      yelp_marshal_VOID__STRING,
		      G_TYPE_NONE,
		      1, G_TYPE_STRING);
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

    d (g_print ("yelp_html_set_base_uri\n"));
    d (g_print ("  uri = \"%s\"\n", uri));

    priv = html->priv;

    if (priv->base_uri)
	g_free (priv->base_uri);

    priv->base_uri = g_strdup (uri);
}

void
yelp_html_clear (YelpHtml *html)
{
    d (g_print ("yelp_html_clear\n"));
    gtk_moz_embed_open_stream (GTK_MOZ_EMBED (html->priv->embed),
			       html->priv->base_uri,
			       "application/xhtml+xml");
}

void
yelp_html_write (YelpHtml *html, const gchar *data, gint len)
{
     if (len == -1) len = strlen (data);

    d (g_print ("yelp_html_write\n"));
    d (g_print ("  data = %i bytes\n", strlen (data)));
    d (g_print ("  len  = %i\n", len));

    gtk_moz_embed_append_data (GTK_MOZ_EMBED (html->priv->embed),
			       data, len);
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
    d (g_print ("yelp_html_close\n"));
    gtk_moz_embed_close_stream (GTK_MOZ_EMBED (html->priv->embed));
}

GtkWidget *
yelp_html_get_widget (YelpHtml *html)
{
    g_return_val_if_fail (YELP_IS_HTML (html), NULL);

    return GTK_WIDGET (html->priv->embed);
}

gboolean
yelp_html_find (YelpHtml    *html,
		const gchar *find_string,
		gboolean     match_case,
		gboolean     wrap,
		gboolean     forward)
{
    return yelp_gecko_find (GTK_MOZ_EMBED (html->priv->embed),
			    find_string, match_case, wrap, forward);
}

void
yelp_html_jump_to_anchor (YelpHtml    *html,
			  gchar       *anchor)
{
    YelpHtmlPriv *priv;

    g_return_if_fail (html != NULL);

    priv = html->priv;

    if (priv->anchor)
	g_free (priv->anchor);

    priv->anchor = g_strdup (anchor);
}

static void
html_set_fonts (void)
{
    gchar *font;

    font = yelp_settings_get_font (YELP_FONT_VARIABLE);
    yelp_gecko_set_font (YELP_FONT_VARIABLE, font);
    g_free (font);

    font = yelp_settings_get_font (YELP_FONT_FIXED);
    yelp_gecko_set_font (YELP_FONT_FIXED, font);
    g_free (font);
}

static void
html_set_colors (void)
{
    gchar *color;

    color = (gchar *) yelp_settings_get_color (YELP_COLOR_TEXT);
    yelp_gecko_set_color (YELP_COLOR_TEXT, color);

    color = (gchar *) yelp_settings_get_color (YELP_COLOR_ANCHOR);
    yelp_gecko_set_color (YELP_COLOR_ANCHOR, color);

    color = (gchar *) yelp_settings_get_color (YELP_COLOR_BACKGROUND);
    yelp_gecko_set_color (YELP_COLOR_BACKGROUND, color);
}

static void
html_set_a11y (void)
{
    gboolean caret;

    caret = yelp_settings_get_caret ();
    yelp_gecko_set_caret (caret);
}

void
yelp_html_copy_selection (YelpHtml    *html)
{
    YelpHtmlPriv *priv;

    g_return_if_fail (html != NULL);

    priv = html->priv;

    yelp_gecko_copy_selection (GTK_MOZ_EMBED (priv->embed));
}

void
yelp_html_select_all (YelpHtml *html)
{
    YelpHtmlPriv *priv;

    g_return_if_fail (html != NULL);

    priv = html->priv;

    yelp_gecko_select_all (GTK_MOZ_EMBED (priv->embed));
}
