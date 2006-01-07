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

#include <mozilla-config.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "yelp-html.h"
#include "yelp-marshal.h"
#include "yelp-gecko-utils.h"
#include "yelp-settings.h"
#include "yelp-gecko-services.h"

#include "Yelper.h"

#include <libgnome/gnome-init.h>

#ifdef GNOME_ENABLE_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define YELP_HTML_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_HTML, YelpHtmlPriv))

struct _YelpHtmlPriv {
    Yelper      *yelper;
    gchar       *base_uri;
    gchar       *anchor;
    gboolean     frames_enabled;
};

static void      html_set_fonts          (void);
static void      html_set_colors         (void);
static void      html_set_a11y           (void);

enum {
    URI_SELECTED,
    FRAME_SELECTED,
    TITLE_CHANGED,
    POPUPMENU_REQUESTED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

static void
html_title (GtkMozEmbed *embed)
{
    YelpHtml *html = YELP_HTML (embed);
    char *new_title;

    new_title = gtk_moz_embed_get_title (embed);

    if (new_title && *new_title != '\0')
    {
        g_signal_emit (html, signals[TITLE_CHANGED], 0, new_title);
        g_free (new_title);
    }
}

static gint
html_dom_mouse_down (GtkMozEmbed *embed, gpointer dom_event)
{
    YelpHtml *html = YELP_HTML (embed);
 
    html->priv->yelper->ProcessMouseEvent (dom_event);

    return FALSE;
}

static gint
html_open_uri (GtkMozEmbed *embed, const gchar *uri)
{
    YelpHtml *html = YELP_HTML (embed);
    gboolean block_load;

    g_return_val_if_fail (uri != NULL, FALSE);

    d (g_print ("embed_open_uri_cb uri=%s\n", uri));
    d (g_print ("  uri = \"%s\"\n", uri));

    if (!html->priv->frames_enabled) {
	g_signal_emit (html, signals[URI_SELECTED], 0, uri, FALSE);
	block_load = TRUE;
    } else {
	gboolean do_load = FALSE;
	g_signal_emit (html, signals[FRAME_SELECTED], 0, uri, FALSE, &block_load);
    }
    return block_load;
}

static void
html_realize (GtkWidget *widget)
{
    YelpHtml *html = YELP_HTML (widget);

    GTK_WIDGET_CLASS (parent_class)->realize (widget);

    nsresult rv;
    rv = html->priv->yelper->Init ();
        
    if (NS_FAILED (rv)) {
        g_warning ("Yelper initialization failed for %p\n", (void*) html);
    }
}

static void
html_init (YelpHtml *html)
{
    YelpHtmlPriv  *priv;
    YelpHtmlClass *klass;

    html->priv = priv = YELP_HTML_GET_PRIVATE (html);

    priv->base_uri = NULL;
    priv->anchor = NULL;

    priv->yelper = new Yelper (GTK_MOZ_EMBED (html));
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
    yelp_register_printing ();
}

static void
html_dispose (GObject *object)
{
    YelpHtml *html = YELP_HTML (object);

    html->priv->yelper->Destroy ();

    parent_class->dispose (object);
}

static void
html_finalize (GObject *object)
{
    YelpHtml *html = YELP_HTML (object);
    YelpHtmlPriv *priv = html->priv;

    delete priv->yelper;

    g_free (priv->base_uri);
    g_free (priv->anchor);

    parent_class->finalize (object);
}

static void
html_class_init (YelpHtmlClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GtkMozEmbedClass *moz_embed_class = GTK_MOZ_EMBED_CLASS (klass);

    parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

    object_class->finalize = html_finalize;
    object_class->dispose = html_dispose;

    widget_class->realize = html_realize;

    moz_embed_class->title = html_title;
    moz_embed_class->dom_mouse_down = html_dom_mouse_down;
    moz_embed_class->open_uri = html_open_uri;

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
    
    signals[FRAME_SELECTED] =
	g_signal_new ("frame_selected",
		      G_TYPE_FROM_CLASS (klass),
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (YelpHtmlClass,
				       frame_selected),
		      NULL, NULL,
		      yelp_marshal_BOOLEAN__POINTER_BOOLEAN,
		      G_TYPE_BOOLEAN,
		      2, G_TYPE_POINTER, G_TYPE_BOOLEAN);
    
    signals[TITLE_CHANGED] = 
	g_signal_new ("title_changed",
		      G_TYPE_FROM_CLASS (klass),
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (YelpHtmlClass,
				       title_changed),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__STRING,
		      G_TYPE_NONE,
		      1, G_TYPE_STRING);

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

	type = g_type_register_static (GTK_TYPE_MOZ_EMBED,
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

    d (g_print ("yelp_html_set_base_uri\n"));
    d (g_print ("  uri = \"%s\"\n", uri));

    priv = html->priv;

    if (priv->base_uri)
	g_free (priv->base_uri);

    priv->base_uri = g_strdup (uri);
}

void
yelp_html_open_stream (YelpHtml *html, const gchar *mime)
{
    d (g_print ("yelp_html_open\n"));

    html->priv->frames_enabled = FALSE;
    gtk_moz_embed_open_stream (GTK_MOZ_EMBED (html),
			       html->priv->base_uri,
			       mime);
}

void
yelp_html_write (YelpHtml *html, const gchar *data, gint len)
{
     if (len == -1) len = strlen (data);

    d (g_print ("yelp_html_write\n"));
    d (g_print ("  data = %i bytes\n", strlen (data)));
    d (g_print ("  len  = %i\n", len));

    gtk_moz_embed_append_data (GTK_MOZ_EMBED (html),
			       data, len);
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
    d (g_print ("yelp_html_close\n"));
    gtk_moz_embed_close_stream (GTK_MOZ_EMBED (html));
}

gboolean
yelp_html_find (YelpHtml    *html,
		const gchar *find_string)
{
    return html->priv->yelper->Find (find_string);
}

gboolean
yelp_html_find_again (YelpHtml    *html,					  
		     gboolean     forward)
{
    return html->priv->yelper->FindAgain ((PRBool) forward);
}

void
yelp_html_set_find_props (YelpHtml    *html,
			  const char  *str,
			  gboolean     match_case,
			  gboolean     wrap)
{
    html->priv->yelper->SetFindProperties (str, (PRBool) match_case, (PRBool) wrap);
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
    html->priv->yelper->DoCommand ("cmd_copy");
}

void
yelp_html_select_all (YelpHtml *html)
{
    html->priv->yelper->DoCommand ("cmd_selectAll");
}

void
yelp_html_print (YelpHtml *html, YelpPrintInfo *info, gboolean preview, gint *npages)
{
    html->priv->yelper->Print (info, preview, npages);
}

void
yelp_html_preview_navigate (YelpHtml *html, gint page_no)
{
    html->priv->yelper->PrintPreviewNavigate (page_no);
}

void
yelp_html_preview_end (YelpHtml *html)
{
    html->priv->yelper->PrintPreviewEnd ();
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
    const gchar *color;

    color = yelp_settings_get_color (YELP_COLOR_FG);
    yelp_gecko_set_color (YELP_COLOR_FG, color);

    color = yelp_settings_get_color (YELP_COLOR_BG);
    yelp_gecko_set_color (YELP_COLOR_BG, color);
 
    color = yelp_settings_get_color (YELP_COLOR_ANCHOR);
    yelp_gecko_set_color (YELP_COLOR_ANCHOR, color);
}

static void
html_set_a11y (void)
{
    gboolean caret;

    caret = yelp_settings_get_caret ();
    yelp_gecko_set_caret (caret);
}

void
yelp_html_initialize (void)
{
    static gboolean initialized = FALSE;

    if (initialized)
	return;
    initialized = TRUE;

    gtk_moz_embed_set_comp_path (MOZILLA_HOME);

}
