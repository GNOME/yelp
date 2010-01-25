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
#include <config.h>

#include <string.h>

#include "yelp-gecko-services.h"
#include "yelp-gecko-utils.h"
#include "yelp-marshal.h"
#include "yelp-settings.h"

#include "Yelper.h"

#include "yelp-html.h"
#include "yelp-debug.h"


#define YELP_HTML_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_HTML, YelpHtmlPriv))

struct _YelpHtmlPriv {
    Yelper      *yelper;
    gchar       *base_uri;
    gchar       *anchor;
    gboolean     frames_enabled;
    guint        timeout;
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
    gboolean block_load = FALSE;

    g_return_val_if_fail (uri != NULL, FALSE);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  uri = \"%s\"\n", uri);

    if (g_str_equal (html->priv->base_uri, uri)) {
	/* As of xulrunner 1.6.2, open_uri is called in response
	   to the base URI we pass in.
	 */
	return FALSE;
    }

    if (!html->priv->frames_enabled) {
	g_signal_emit (html, signals[URI_SELECTED], 0, uri, FALSE);
	block_load = TRUE;
    } else {
	g_signal_emit (html, signals[FRAME_SELECTED], 0, uri, FALSE, &block_load);
    }
    return block_load;
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

    nsresult rv;
    rv = html->priv->yelper->Init ();
        
    if (NS_FAILED (rv)) {
        g_warning ("Yelper initialization failed for %p\n", (void*) html);
    }

#ifdef HAVE_GECKO_1_9
    if (!gail_get_root_restored) {
	gail_get_root_restored = TRUE;
	if (atk_class && gail_get_root) {
	    gtk_widget_get_accessible (widget);
	    atk_class->get_root = gail_get_root;
	}
    }
#endif
  
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

    if (priv->timeout)
	g_source_remove (priv->timeout);
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

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  uri = \"%s\"\n", uri);

    priv = html->priv;

    if (priv->base_uri)
	g_free (priv->base_uri);

    priv->base_uri = g_strdup (uri);
}

void
yelp_html_open_stream (YelpHtml *html, const gchar *mime)
{
    debug_print (DB_FUNCTION, "entering\n");

    html->priv->frames_enabled = FALSE;
    gtk_moz_embed_open_stream (GTK_MOZ_EMBED (html),
			       html->priv->base_uri,
			       mime);
}

void
yelp_html_write (YelpHtml *html, const gchar *data, gint len)
{
     if (len == -1) len = strlen (data);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  data = %i bytes\n", strlen (data));
    debug_print (DB_ARG, "  len  = %i\n", len);

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

/* Fire "children_changed::add" event to refresh "UI-Grab" window of GOK,
 * this event is not fired when using gtk_moz_embed_xxx_stream,
 * see Mozilla bug #293670.  Done in a timeout to allow mozilla to
 * actually draw to the screen */

static gboolean
timeout_update_gok (YelpHtml *html)
{
    g_signal_emit_by_name (gtk_widget_get_accessible (GTK_WIDGET (html)),
			   "children_changed::add", -1, NULL, NULL);
    html->priv->timeout = 0;
    return FALSE;
}

void
yelp_html_close (YelpHtml *html)
{
    debug_print (DB_FUNCTION, "entering\n");
    gtk_moz_embed_close_stream (GTK_MOZ_EMBED (html));

#ifdef HAVE_GECKO_1_9
    html_reset_accessible_parent (GTK_WIDGET (html));
#else
    html->priv->timeout = g_timeout_add_seconds (2,
						 (GSourceFunc) timeout_update_gok,
						 html);
#endif
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

gboolean
yelp_html_initialize (void)
{
    return yelp_gecko_init ();
}

void
yelp_html_shutdown (void)
{
    yelp_gecko_shutdown ();
}
