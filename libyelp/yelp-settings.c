/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2004-2009 Shaun McCance <shaunm@gnome.org>
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

#include <stdarg.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "yelp-settings.h"

struct _YelpSettingsPriv {
    GMutex       *mutex;

    gchar         colors[YELP_SETTINGS_NUM_COLORS][8];
    gchar        *fonts[YELP_SETTINGS_NUM_FONTS];
    gchar        *icons[YELP_SETTINGS_NUM_ICONS];
    gint          icon_size;

    GtkSettings  *gtk_settings;
    GtkIconTheme *gtk_icon_theme;

    gulong        gtk_theme_changed;
    gulong        icon_theme_changed;
};

enum {
    COLORS_CHANGED,
    FONTS_CHANGED,
    ICONS_CHANGED,
    LAST_SIGNAL
};
static guint settings_signals[LAST_SIGNAL] = {0,};

enum {  
  PROP_0,
  PROP_GTK_SETTINGS,
  PROP_GTK_ICON_THEME
};

gchar *icon_names[YELP_SETTINGS_NUM_ICONS];

G_DEFINE_TYPE (YelpSettings, yelp_settings, G_TYPE_OBJECT);
#define GET_PRIV(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_SETTINGS, YelpSettingsPriv))

static void           yelp_settings_class_init   (YelpSettingsClass    *klass);
static void           yelp_settings_init         (YelpSettings         *settings);
static void           yelp_settings_dispose      (GObject              *object);
static void           yelp_settings_finalize     (GObject              *object);
static void           yelp_settings_get_property (GObject              *object,
						  guint                 prop_id,
						  GValue               *value,
						  GParamSpec           *pspec);
static void           yelp_settings_set_property (GObject              *object,
						  guint                 prop_id,
						  const GValue         *value,
						  GParamSpec           *pspec);

static void           gtk_theme_changed          (GtkSettings          *gtk_settings,
						  GParamSpec           *pspec,
					          YelpSettings         *settings);
static void           icon_theme_changed         (GtkIconTheme         *theme,
						  YelpSettings         *settings);

static void           rgb_to_hls                 (gdouble  r,
						  gdouble  g,
						  gdouble  b,
						  gdouble *h,
						  gdouble *l,
						  gdouble *s);
static void           hls_to_hex                 (gdouble  h,
						  gdouble  l,
						  gdouble  s,
						  gchar    *str);

/******************************************************************************/

static void
yelp_settings_class_init (YelpSettingsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    gint i;

    object_class->dispose  = yelp_settings_dispose;
    object_class->finalize = yelp_settings_finalize;
    object_class->get_property = yelp_settings_get_property;
    object_class->set_property = yelp_settings_set_property;

    for (i = 0; i < YELP_SETTINGS_NUM_ICONS; i++) {
	switch (i) {
	case YELP_SETTINGS_ICON_BUG:
	    icon_names[i] = "admon-bug";
	    break;
	case YELP_SETTINGS_ICON_CAUTION:
	    icon_names[i] = "admon-caution";
	    break;
	case YELP_SETTINGS_ICON_IMPORTANT:
	    icon_names[i] = "admon-important";
	    break;
	case YELP_SETTINGS_ICON_NOTE:
	    icon_names[i] = "admon-note";
	    break;
	case YELP_SETTINGS_ICON_TIP:
	    icon_names[i] = "admon-tip";
	    break;
	case YELP_SETTINGS_ICON_WARNING:
	    icon_names[i] = "admon-warning";
	    break;
	default:
	    g_assert_not_reached ();
	}
    }

    g_object_class_install_property (object_class,
                                     PROP_GTK_SETTINGS,
                                     g_param_spec_object ("gtk-settings",
							  _("GtkSettings"),
							  _("A GtkSettings object to get settings from"),
							  GTK_TYPE_SETTINGS,
							  G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
							  G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property (object_class,
                                     PROP_GTK_ICON_THEME,
                                     g_param_spec_object ("gtk-icon-theme",
							  _("GtkIconTheme"),
							  _("A GtkIconTheme object to get icons from"),
							  GTK_TYPE_ICON_THEME,
							  G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
							  G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    settings_signals[COLORS_CHANGED] =
	g_signal_new ("colors-changed",
		      G_OBJECT_CLASS_TYPE (klass),
		      G_SIGNAL_RUN_LAST,
		      0, NULL, NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE, 0);

    settings_signals[FONTS_CHANGED] =
	g_signal_new ("fonts-changed",
		      G_OBJECT_CLASS_TYPE (klass),
		      G_SIGNAL_RUN_LAST,
		      0, NULL, NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE, 0);

    settings_signals[ICONS_CHANGED] =
	g_signal_new ("icons-changed",
		      G_OBJECT_CLASS_TYPE (klass),
		      G_SIGNAL_RUN_LAST,
		      0, NULL, NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE, 0);

    g_type_class_add_private (klass, sizeof (YelpSettingsPriv));
}

static void
yelp_settings_init (YelpSettings *settings)
{
    gint i;

    settings->priv = GET_PRIV (settings);
    settings->priv->mutex = g_mutex_new ();
    settings->priv->icon_size = 48;

    for (i = 0; i < YELP_SETTINGS_NUM_ICONS; i++)
	settings->priv->icons[i] = NULL;
}

static void
yelp_settings_dispose (GObject *object)
{
    YelpSettings *settings = YELP_SETTINGS (object);

    G_OBJECT_CLASS (yelp_settings_parent_class)->dispose (object);
}

static void
yelp_settings_finalize (GObject *object)
{
    YelpSettings *settings = YELP_SETTINGS (object);

    g_mutex_free (settings->priv->mutex);

    G_OBJECT_CLASS (yelp_settings_parent_class)->finalize (object);
}

static void
yelp_settings_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
    YelpSettings *settings = YELP_SETTINGS (object);

    switch (prop_id) {
    case PROP_GTK_SETTINGS:
	g_value_set_object (value, settings->priv->gtk_settings);
	break;
    case PROP_GTK_ICON_THEME:
	g_value_set_object (value, settings->priv->gtk_icon_theme);
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	break;
    }
}

static void
yelp_settings_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
    YelpSettings *settings = YELP_SETTINGS (object);

    switch (prop_id) {
    case PROP_GTK_SETTINGS:
	if (settings->priv->gtk_settings) {
	    g_signal_handler_disconnect (settings->priv->gtk_settings,
					 settings->priv->gtk_theme_changed);
	    g_object_unref (settings->priv->gtk_settings);
	}
	settings->priv->gtk_settings = g_value_get_object (value);
	if (settings->priv->gtk_settings != NULL) {
	    g_object_ref (settings->priv->gtk_settings);
	    settings->priv->gtk_theme_changed =
		g_signal_connect (settings->priv->gtk_settings,
				  "notify::gtk-theme-name",
				  (GCallback) gtk_theme_changed,
				  settings);
	    gtk_theme_changed (settings->priv->gtk_settings, NULL, settings);
	}
	else {
	    settings->priv->gtk_theme_changed = 0;
	}
	break;
    case PROP_GTK_ICON_THEME:
	if (settings->priv->gtk_icon_theme) {
	    g_signal_handler_disconnect (settings->priv->gtk_icon_theme,
					 settings->priv->icon_theme_changed);
	    g_object_unref (settings->priv->gtk_icon_theme);
	}
	settings->priv->gtk_icon_theme = g_value_get_object (value);
	if (settings->priv->gtk_icon_theme != NULL) {
	    gchar **search_path;
	    gint search_path_len, i;
	    gboolean append_search_path = TRUE;
	    gtk_icon_theme_get_search_path (settings->priv->gtk_icon_theme,
					    &search_path, &search_path_len);
	    for (i = search_path_len - 1; i >= 0; i--)
		if (g_str_equal (search_path[i], GDU_ICON_PATH)) {
		    append_search_path = FALSE;
		    break;
		}
	    g_strfreev (search_path);
	    if (append_search_path)
		gtk_icon_theme_append_search_path (settings->priv->gtk_icon_theme,
						   GDU_ICON_PATH);
	    g_object_ref (settings->priv->gtk_icon_theme);
	    settings->priv->icon_theme_changed =
		g_signal_connect (settings->priv->gtk_icon_theme,
				  "changed",
				  (GCallback) icon_theme_changed,
				  settings);
	    icon_theme_changed (settings->priv->gtk_icon_theme, settings);
	}
	else {
	    settings->priv->icon_theme_changed = 0;
	}
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	break;
    }
}

/******************************************************************************/

YelpSettings *
yelp_settings_get_default (void)
{
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
    static YelpSettings *settings = NULL;
    g_static_mutex_lock (&mutex);
    if (settings == NULL)
	settings = g_object_new (YELP_TYPE_SETTINGS,
				 "gtk-settings", gtk_settings_get_default (),
				 "gtk-icon-theme", gtk_icon_theme_get_default (),
				 NULL);
    g_static_mutex_unlock (&mutex);
    return settings;
}

/******************************************************************************/

gchar *
yelp_settings_get_color (YelpSettings       *settings,
			 YelpSettingsColor   color)
{
    gchar *colorstr;
    g_return_val_if_fail (color < YELP_SETTINGS_NUM_COLORS, NULL);

    g_mutex_lock (settings->priv->mutex);
    colorstr = g_strdup (settings->priv->colors[color]);
    g_mutex_unlock (settings->priv->mutex);

    return colorstr;
}

gchar **
yelp_settings_get_colors (YelpSettings *settings)
{
    gchar **colors = g_new0 (gchar *, YELP_SETTINGS_NUM_COLORS + 1);
    gint i;
    for (i = 0; i < YELP_SETTINGS_NUM_COLORS; i++)
	colors[i] = yelp_settings_get_color (settings, i);
    return colors;
}

void
yelp_settings_set_colors (YelpSettings      *settings,
			  YelpSettingsColor  first_color,
			  ...)
{
    YelpSettingsColor color;
    va_list args;

    g_mutex_lock (settings->priv->mutex);
    va_start (args, first_color);

    color = first_color;
    while ((gint) color >= 0) {
	gchar *colorstr = va_arg (args, gchar *);
	gint i;
	for (i = 0; i < 7; i++) {
	    settings->priv->colors[color][i] = colorstr[i];
	    if (colorstr[i] == '\0')
		break;
	}
	settings->priv->colors[color][7] = '\0';
	color = va_arg (args, YelpSettingsColor);
    }

    va_end (args);
    g_mutex_unlock (settings->priv->mutex);

    g_signal_emit (settings, settings_signals[COLORS_CHANGED], 0);
}

const gchar*
yelp_settings_get_color_param (YelpSettingsColor color)
{
    static const gchar *params[YELP_SETTINGS_NUM_COLORS] = {
	"theme.color.background",
	"theme.color.text",
	"theme.color.text_light",
	"theme.color.link",
	"theme.color.link_visted",
	"theme.color.gray_background",
	"theme.color.gray_border",
	"theme.color.blue_background",
	"theme.color.blue_border",
	"theme.color.red_background",
	"theme.color.red_border",
	"theme.color.yellow_background",
	"theme.color.yellow_border"
    };
    g_return_val_if_fail (color < YELP_SETTINGS_NUM_COLORS, NULL);
    return params[color];
}

/******************************************************************************/

gchar *
yelp_settings_get_font (YelpSettings     *settings,
			YelpSettingsFont  font)
{
    gchar *ret;
    g_return_val_if_fail (font < YELP_SETTINGS_NUM_FONTS, NULL);

    g_mutex_lock (settings->priv->mutex);
    ret = g_strdup (settings->priv->fonts[font]);
    g_mutex_unlock (settings->priv->mutex);

    return ret;
}

gchar *
yelp_settings_get_font_family (YelpSettings     *settings,
			       YelpSettingsFont  font)
{
    const gchar *def = (font == YELP_SETTINGS_FONT_VARIABLE) ? "Sans" : "Monospace";
    gchar *ret, *c;
    g_return_val_if_fail (font < YELP_SETTINGS_NUM_FONTS, NULL);

    g_mutex_lock (settings->priv->mutex);

    if (settings->priv->fonts[font] == NULL) {
	ret = g_strdup (def);
	goto done;
    }

    c = strrchr (settings->priv->fonts[font], ' ');
    if (c == NULL) {
	g_warning ("Cannot parse font %s", settings->priv->fonts[font]);
	ret = g_strdup (def);
	goto done;
    }

    ret = g_strndup (settings->priv->fonts[font], c - settings->priv->fonts[font]);

 done:
    g_mutex_unlock (settings->priv->mutex);
    return ret;
}

gint
yelp_settings_get_font_size (YelpSettings     *settings,
			     YelpSettingsFont  font)
{
    gchar *c;
    gint ret;
    g_return_val_if_fail (font < YELP_SETTINGS_NUM_FONTS, 0);

    g_mutex_lock (settings->priv->mutex);

    if (settings->priv->fonts[font] == NULL) {
	ret = 10;
	goto done;
    }

    c = strrchr (settings->priv->fonts[font], ' ');
    if (c == NULL) {
	g_warning ("Cannot parse font %s", settings->priv->fonts[font]);
	ret = 10;
	goto done;
    }

    ret = g_ascii_strtod (c, NULL);

 done:
    g_mutex_unlock (settings->priv->mutex);
    return ret;
}

void
yelp_settings_set_fonts (YelpSettings     *settings,
			 YelpSettingsFont  first_font,
			 ...)
{
    YelpSettingsFont font;
    va_list args;

    g_mutex_lock (settings->priv->mutex);
    va_start (args, first_font);

    font = first_font;
    while ((gint) font >= 0) {
	gchar *fontname = va_arg (args, gchar *);
	if (settings->priv->fonts[font] != NULL)
	    g_free (settings->priv->fonts[font]);
	settings->priv->fonts[font] = g_strdup (fontname);
	font = va_arg (args, YelpSettingsFont);
    }

    va_end (args);
    g_mutex_unlock (settings->priv->mutex);

    g_signal_emit (settings, settings_signals[FONTS_CHANGED], 0);
}

/******************************************************************************/

gint
yelp_settings_get_icon_size (YelpSettings *settings)
{
    return settings->priv->icon_size;
}

void
yelp_settings_set_icon_size (YelpSettings *settings,
			     gint          size)
{
    settings->priv->icon_size = size;
    if (settings->priv->gtk_icon_theme != NULL)
	icon_theme_changed (settings->priv->gtk_icon_theme, settings);
}

gchar *
yelp_settings_get_icon (YelpSettings     *settings,
			YelpSettingsIcon  icon)
{
    gchar *ret;
    g_return_val_if_fail (icon < YELP_SETTINGS_NUM_ICONS, NULL);

    g_mutex_lock (settings->priv->mutex);
    ret = g_strdup (settings->priv->icons[icon]);
    g_mutex_unlock (settings->priv->mutex);

    return ret;
}

void
yelp_settings_set_icons (YelpSettings     *settings,
			 YelpSettingsIcon  first_icon,
			 ...)
{
    YelpSettingsIcon icon;
    va_list args;

    g_mutex_lock (settings->priv->mutex);
    va_start (args, first_icon);

    icon = first_icon;
    while ((gint) icon >= 0) {
	gchar *filename = va_arg (args, gchar *);
	if (settings->priv->icons[icon] != NULL)
	    g_free (settings->priv->icons[icon]);
	settings->priv->icons[icon] = g_strdup (filename);
	icon = va_arg (args, YelpSettingsIcon);
    }

    va_end (args);
    g_mutex_unlock (settings->priv->mutex);

    g_signal_emit (settings, settings_signals[ICONS_CHANGED], 0);
}

const gchar *
yelp_settings_get_icon_param (YelpSettingsIcon icon)
{
    static const gchar *params[YELP_SETTINGS_NUM_ICONS] = {
	"theme.icon.admon.bug",
	"theme.icon.admon.caution",
	"theme.icon.admon.important",
	"theme.icon.admon.note",
	"theme.icon.admon.tip",
	"theme.icon.admon.warning"
    };
    g_return_val_if_fail (icon < YELP_SETTINGS_NUM_ICONS, NULL);
    return params[icon];
}

/******************************************************************************/

static void
gtk_theme_changed (GtkSettings  *gtk_settings,
		   GParamSpec   *pspec,
		   YelpSettings *settings)
{
    GtkWidget *widget;
    GtkStyle  *style;
    GdkColor  *color;
    GdkColor   blue = { 0, 0x1E1E, 0x3E3E, 0xE7E7 };
    gdouble    base_h, base_l, base_s;
    gdouble    text_h, text_l, text_s;
    gint i;

    g_mutex_lock (settings->priv->mutex);

    style = gtk_rc_get_style_by_paths (gtk_settings,
				       "GtkTextView", "GtkTextView",
				       GTK_TYPE_TEXT_VIEW);
    if (style)
	g_object_ref (G_OBJECT (style));
    else
	style = gtk_style_new ();


    rgb_to_hls (style->text[GTK_STATE_NORMAL].red / 65535.0,
		style->text[GTK_STATE_NORMAL].green / 65535.0,
		style->text[GTK_STATE_NORMAL].blue / 65535.0,
		&text_h, &text_l, &text_s);
    rgb_to_hls (style->base[GTK_STATE_NORMAL].red / 65535.0,
		style->base[GTK_STATE_NORMAL].green / 65535.0,
		style->base[GTK_STATE_NORMAL].blue / 65535.0,
		&base_h, &base_l, &base_s);

    /* YELP_SETTINGS_COLOR_BASE */
    g_snprintf (settings->priv->colors[YELP_SETTINGS_COLOR_BASE], 8,
		"#%02X%02X%02X",
		style->base[GTK_STATE_NORMAL].red >> 8,
		style->base[GTK_STATE_NORMAL].green >> 8,
		style->base[GTK_STATE_NORMAL].blue >> 8);

    /* YELP_SETTINGS_COLOR_TEXT */
    g_snprintf (settings->priv->colors[YELP_SETTINGS_COLOR_TEXT], 8,
		"#%02X%02X%02X",
		style->text[GTK_STATE_NORMAL].red >> 8,
		style->text[GTK_STATE_NORMAL].green >> 8,
		style->text[GTK_STATE_NORMAL].blue >> 8);

    /* YELP_SETTINGS_COLOR_LINK */
    widget = gtk_link_button_new ("http://www.gnome.org");
    gtk_widget_style_get (widget, "link-color", &color, NULL);
    if (!color)
	color = &blue;
    g_snprintf (settings->priv->colors[YELP_SETTINGS_COLOR_LINK], 8,
		"#%02X%02X%02X",
		color->red >> 8,
		color->green >> 8,
		color->blue >> 8);
    if (color != &blue)
	gdk_color_free (color);
	
    /* YELP_SETTINGS_COLOR_LINK_VISITED */
    color = NULL;
    gtk_widget_style_get (widget, "visited-link-color", &color, NULL);
    if (color) {
	g_snprintf (settings->priv->colors[YELP_SETTINGS_COLOR_LINK_VISITED], 8,
		    "#%02X%02X%02X",
		    color->red >> 8,
		    color->green >> 8,
		    color->blue >> 8);
	gdk_color_free (color);
    }

    g_object_ref_sink (widget);
    g_object_unref (widget);

    /* YELP_SETTINGS_COLOR_TEXT_LIGHT */
    hls_to_hex (text_h, 
		text_l - ((text_l - base_l) * 0.25),
		text_s,
		settings->priv->colors[YELP_SETTINGS_COLOR_TEXT_LIGHT]);

    /* YELP_SETTINGS_COLOR_GRAY */
    hls_to_hex (base_h, 
		base_l - ((base_l - text_l) * 0.03),
		base_s,
		settings->priv->colors[YELP_SETTINGS_COLOR_GRAY_BASE]);
    hls_to_hex (base_h, 
		base_l - ((base_l - text_l) * 0.25),
		base_s,
		settings->priv->colors[YELP_SETTINGS_COLOR_GRAY_BORDER]);

    /* YELP_SETTINGS_COLOR_BLUE */
    hls_to_hex (204,
		base_l - ((base_l - text_l) * 0.03),
		0.75,
		settings->priv->colors[YELP_SETTINGS_COLOR_BLUE_BASE]);
    hls_to_hex (204,
		base_l - ((base_l - text_l) * 0.25),
		0.75,
		settings->priv->colors[YELP_SETTINGS_COLOR_BLUE_BORDER]);

    /* YELP_SETTINGS_COLOR_RED */
    hls_to_hex (0,
		base_l - ((base_l - text_l) * 0.03),
		0.75,
		settings->priv->colors[YELP_SETTINGS_COLOR_RED_BASE]);
    hls_to_hex (0,
		base_l - ((base_l - text_l) * 0.25),
		0.75,
		settings->priv->colors[YELP_SETTINGS_COLOR_RED_BORDER]);

    /* YELP_SETTINGS_COLOR_YELLOW */
    hls_to_hex (60,
		base_l - ((base_l - text_l) * 0.03),
		0.75,
		settings->priv->colors[YELP_SETTINGS_COLOR_YELLOW_BASE]);
    hls_to_hex (60,
		base_l - ((base_l - text_l) * 0.25),
		0.75,
		settings->priv->colors[YELP_SETTINGS_COLOR_YELLOW_BORDER]);

    g_object_unref (G_OBJECT (style));

    g_mutex_unlock (settings->priv->mutex);

    g_signal_emit (settings, settings_signals[COLORS_CHANGED], 0);
}

static void
icon_theme_changed (GtkIconTheme *theme,
		    YelpSettings *settings)
{
    GtkIconInfo *info;
    gint i;

    g_mutex_lock (settings->priv->mutex);

    for (i = 0; i < YELP_SETTINGS_NUM_ICONS; i++) {
	if (settings->priv->icons[i] != NULL)
	    g_free (settings->priv->icons[i]);
	info = gtk_icon_theme_lookup_icon (theme,
					   icon_names[i],
					   settings->priv->icon_size,
					   GTK_ICON_LOOKUP_NO_SVG);
	if (info != NULL) {
	    settings->priv->icons[i] = g_strdup (gtk_icon_info_get_filename (info));
	    gtk_icon_info_free (info);
	}
	else {
	    settings->priv->icons[i] = NULL;
	}
    }

    g_mutex_unlock (settings->priv->mutex);

    g_signal_emit (settings, settings_signals[ICONS_CHANGED], 0);
}

/******************************************************************************/

static void
rgb_to_hls (gdouble r, gdouble g, gdouble b, gdouble *h, gdouble *l, gdouble *s)
{
    gdouble min, max, delta;

    if (r > g) {
	if (r > b)
	    max = r;
	else
	    max = b;
	if (g < b)
	    min = g;
	else
	    min = b;
    }
    else {
	if (g > b)
	    max = g;
	else
	    max = b;
	if (r < b)
	    min = r;
	else
	    min = b;
    }

    *l = (max + min) / 2;
    *s = 0;
    *h = 0;

    if (max != min) {
	if (*l <= 0.5)
	    *s = (max - min) / (max + min);
	else
	    *s = (max - min) / (2 - max - min);

	delta = max - min;
	if (r == max)
	    *h = (g - b) / delta;
	else if (g == max)
	    *h = 2 + (b - r) / delta;
	else if (b == max)
	    *h = 4 + (r - g) / delta;

	*h *= 60;
	if (*h < 0.0)
	    *h += 360;
    }
}

static void
hls_to_hex (gdouble h, gdouble l, gdouble s, gchar *str)
{
    gdouble hue;
    gdouble m1, m2;
    gdouble r, g, b;
    guint8 red, green, blue;

    if (l <= 0.5)
	m2 = l * (1 + s);
    else
	m2 = l + s - l * s;
    m1 = 2 * l - m2;

    if (s == 0) {
	r = g = b = l;
    }
    else {
	hue = h + 120;
	while (hue > 360)
	    hue -= 360;
	while (hue < 0)
	    hue += 360;

	if (hue < 60)
	    r = m1 + (m2 - m1) * hue / 60;
	else if (hue < 180)
	    r = m2;
	else if (hue < 240)
	    r = m1 + (m2 - m1) * (240 - hue) / 60;
	else
	    r = m1;

	hue = h;
	while (hue > 360)
	    hue -= 360;
	while (hue < 0)
	    hue += 360;

	if (hue < 60)
	    g = m1 + (m2 - m1) * hue / 60;
	else if (hue < 180)
	    g = m2;
	else if (hue < 240)
	    g = m1 + (m2 - m1) * (240 - hue) / 60;
	else
	    g = m1;

	hue = h - 120;
	while (hue > 360)
	    hue -= 360;
	while (hue < 0)
	    hue += 360;

	if (hue < 60)
	    b = m1 + (m2 - m1) * hue / 60;
	else if (hue < 180)
	    b = m2;
	else if (hue < 240)
	    b = m1 + (m2 - m1) * (240 - hue) / 60;
	else
	    b = m1;
    }

    red = r * 255;
    green = g * 255;
    blue = b * 255;
    g_snprintf (str, 8, "#%02X%02X%02X", red, green, blue);
}
