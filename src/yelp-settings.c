/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2004 Shaun McCance <shaunm@gnome.org>
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
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <string.h>

#include "yelp-settings.h"
#include "yelp-debug.h"

#define KEY_GNOME_DIR           "/desktop/gnome/interface"
#define KEY_GNOME_VARIABLE_FONT KEY_GNOME_DIR "/document_font_name"
#define KEY_GNOME_FIXED_FONT    KEY_GNOME_DIR "/monospace_font_name"
#define KEY_GNOME_GTK_THEME     KEY_GNOME_DIR "/gtk_theme"

#define KEY_YELP_DIR            "/apps/yelp"
#define KEY_YELP_USE_CARET      KEY_YELP_DIR "/use_caret"
#define KEY_YELP_SYSTEM_FONTS   KEY_YELP_DIR "/use_system_fonts"
#define KEY_YELP_VARIABLE_FONT  KEY_YELP_DIR "/variable_font"
#define KEY_YELP_FIXED_FONT     KEY_YELP_DIR "/fixed_font"

static const gchar * const color_params[YELP_NUM_COLORS] = {
    "theme.color.text",
    "theme.color.background",
    "theme.color.text_light",
    "theme.color.link",
    "theme.color.link_visited",
    "theme.color.gray_background",
    "theme.color.gray_border",
    "theme.color.blue_background",
    "theme.color.blue_border",
    "theme.color.red_background",
    "theme.color.red_border",
    "theme.color.yellow_background",
    "theme.color.yellow_border"
};

static const gchar * const icon_params[YELP_NUM_ICONS] = {
    "theme.icon.admon.bug",
    "theme.icon.admon.caution",
    "theme.icon.admon.important",
    "theme.icon.admon.note",
    "theme.icon.admon.tip",
    "theme.icon.admon.warning"
};

static void           settings_update         (YelpSettingsType  type);
static void           prefs_system_fonts_cb   (GtkWidget        *widget);
static void           prefs_font_cb           (GtkWidget        *widget);
static void           prefs_use_caret_cb      (GtkWidget        *widget);
static void           toggle_font_table       (GtkWidget        *widget);
static void           gconf_system_fonts_cb   (GConfClient      *client,
					       guint             cnxn_id,
					       GConfEntry       *entry,
					       gpointer          data);
static void           gconf_font_cb           (GConfClient      *client,
					       guint             cnxn_id,
					       GConfEntry       *entry,
					       gpointer          data);
static void           gconf_use_caret_cb      (GConfClient      *client,
					       guint             cnxn_id,
					       GConfEntry       *entry,
					       gpointer          data);
static void           gtk_theme_changed       (GtkSettings      *settings,
					       gpointer          user_data);
static void           icon_theme_changed      (GtkIconTheme     *theme,
					       gpointer          user_data);

static GConfClient *gconf_client = NULL;

static GHookList    *hook_lists[YELP_SETTINGS_NUM_TYPES];

static GtkSettings  *gtk_settings;
static GtkIconTheme *icon_theme;
static gchar colors[YELP_NUM_COLORS][10];
static gchar *icon_names[YELP_NUM_ICONS] = {NULL,};

static GtkWidget    *prefs_dialog = NULL;
static GtkWidget    *system_fonts_widget  = NULL;
static GtkWidget    *font_table_widget    = NULL;
static GtkWidget    *variable_font_widget = NULL;
static GtkWidget    *fixed_font_widget    = NULL;
static GtkWidget    *use_caret_widget     = NULL;
static gulong system_fonts_handler  = 0;
static gulong variable_font_handler = 0;
static gulong fixed_font_handler    = 0;
static gulong use_caret_handler     = 0;

void
yelp_settings_init (void)
{
    gint i;

    gconf_client = gconf_client_get_default ();
    gconf_client_add_dir (gconf_client, KEY_GNOME_DIR,
			  GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
    gconf_client_add_dir (gconf_client, KEY_YELP_DIR,
			  GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
    gconf_client_notify_add (gconf_client,
			     KEY_GNOME_VARIABLE_FONT,
			     gconf_font_cb,
			     NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client,
			     KEY_GNOME_FIXED_FONT,
			     gconf_font_cb,
			     NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client,
			     KEY_YELP_SYSTEM_FONTS,
			     gconf_system_fonts_cb,
			     NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client,
			     KEY_YELP_VARIABLE_FONT,
			     gconf_font_cb,
			     NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client,
			     KEY_YELP_FIXED_FONT,
			     gconf_font_cb,
			     NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client,
			     KEY_YELP_USE_CARET,
			     gconf_use_caret_cb,
			     NULL, NULL, NULL);

    for (i = 0; i < YELP_SETTINGS_NUM_TYPES; i++) {
	hook_lists[i] = g_new0 (GHookList, 1);
	g_hook_list_init (hook_lists[i], sizeof (GHook));
    }

    gtk_settings = gtk_settings_get_default ();
    g_signal_connect (gtk_settings,
		      "notify::gtk-theme-name",
		      (GCallback) gtk_theme_changed,
		      NULL);

    icon_theme = gtk_icon_theme_get_default ();
    /* This should work with a newer GTK+ */
    gtk_icon_theme_append_search_path (icon_theme,
				       GDU_ICON_PATH);
    /* But if it doesn't, this will work */
    gtk_icon_theme_append_search_path (icon_theme,
				       GDU_ICON_PATH"/hicolor/48x48");
    /* And what they hey, for sentimental reasons */
    gtk_icon_theme_append_search_path (icon_theme,
				       DATADIR "/yelp/icons");
    g_signal_connect (icon_theme,
		      "changed",
		      (GCallback) icon_theme_changed,
		      NULL);

    for (i = 0; i < YELP_NUM_ICONS; i++) {
	switch (i) {
	case YELP_ICON_ADMON_BUG:
	    icon_names[i] = "admon-bug";
	    break;
	case YELP_ICON_ADMON_CAUTION:
	    icon_names[i] = "admon-caution";
	    break;
	case YELP_ICON_ADMON_IMPORTANT:
	    icon_names[i] = "admon-important";
	    break;
	case YELP_ICON_ADMON_NOTE:
	    icon_names[i] = "admon-note";
	    break;
	case YELP_ICON_ADMON_TIP:
	    icon_names[i] = "admon-tip";
	    break;
	case YELP_ICON_ADMON_WARNING:
	    icon_names[i] = "admon-warning";
	    break;
	default:
	    g_assert_not_reached ();
	}
    }

    settings_update (YELP_SETTINGS_INFO_ALL);
}

void
yelp_settings_open_preferences (void)
{
    GtkBuilder *builder;
    GError *error = NULL;
    gchar *font;
    gboolean use;

    if (!prefs_dialog) {
        builder = gtk_builder_new ();
        if (!gtk_builder_add_from_file (builder, 
                                        DATADIR "/yelp/ui/yelp-preferences.ui",
                                        &error)) {
            g_warning ("Could not load builder file: %s", error->message);
            g_error_free(error);
            return;
        }


	prefs_dialog  = GTK_WIDGET (gtk_builder_get_object (builder, "prefs_dialog"));
	use_caret_widget     = GTK_WIDGET (gtk_builder_get_object (builder, "use_caret"));
	system_fonts_widget  = GTK_WIDGET (gtk_builder_get_object (builder, "use_system_fonts"));
	font_table_widget    = GTK_WIDGET (gtk_builder_get_object (builder, "font_table"));
	variable_font_widget = GTK_WIDGET (gtk_builder_get_object (builder, "variable_font"));
	fixed_font_widget    = GTK_WIDGET (gtk_builder_get_object (builder, "fixed_font"));

	use = gconf_client_get_bool (gconf_client, KEY_YELP_SYSTEM_FONTS, NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (system_fonts_widget), use);
	gtk_widget_set_sensitive (font_table_widget, !use);

	font = gconf_client_get_string (gconf_client, KEY_YELP_VARIABLE_FONT, NULL);
	gtk_font_button_set_font_name (GTK_FONT_BUTTON (variable_font_widget), font);
	g_free (font);

	font = gconf_client_get_string (gconf_client, KEY_YELP_FIXED_FONT, NULL);
	gtk_font_button_set_font_name (GTK_FONT_BUTTON (fixed_font_widget), font);
	g_free (font);

	use = gconf_client_get_bool (gconf_client, KEY_YELP_USE_CARET, NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (use_caret_widget), use);

	use_caret_handler =
	    g_signal_connect (G_OBJECT (use_caret_widget), "toggled",
			      G_CALLBACK (prefs_use_caret_cb), NULL);
	system_fonts_handler =
	    g_signal_connect (G_OBJECT (system_fonts_widget), "toggled",
			      G_CALLBACK (prefs_system_fonts_cb), NULL);
	variable_font_handler =
	    g_signal_connect (G_OBJECT (variable_font_widget), "font_set",
			      G_CALLBACK (prefs_font_cb), NULL);
	fixed_font_handler =
	    g_signal_connect (G_OBJECT (fixed_font_widget), "font_set",
			      G_CALLBACK (prefs_font_cb), NULL);

	g_signal_connect (G_OBJECT (system_fonts_widget), "toggled",
			  G_CALLBACK (toggle_font_table), NULL);

	g_signal_connect (G_OBJECT (prefs_dialog), "response",
			  G_CALLBACK (gtk_widget_hide), NULL);
	
	g_signal_connect (G_OBJECT (prefs_dialog), "delete_event",
			  G_CALLBACK (gtk_widget_hide_on_delete), NULL);
    }

    gtk_window_present (GTK_WINDOW (prefs_dialog));
}

guint
yelp_settings_notify_add (YelpSettingsType type,
			  GHookFunc        func,
			  gpointer         data)
{
    GHook *hook;
    gint   i;

    for (i = 0; i < YELP_SETTINGS_NUM_TYPES; i++) {
	if (type & (1 << i)) {
	    hook = g_hook_alloc (hook_lists[i]);
	    hook->func = func;
	    hook->data = data;
	    g_hook_prepend (hook_lists[i], hook);
	    return hook->hook_id;
	}
    }

    return 0;
}

void
yelp_settings_notify_remove (YelpSettingsType type, guint id)
{
    gint   i;

    for (i = 0; i < YELP_SETTINGS_NUM_TYPES; i++)
	if (type & (1 << i))
	    g_hook_destroy (hook_lists[i], id);
}


void
yelp_settings_toggle_caret (void)
{
    gboolean caret;

    caret = gconf_client_get_bool (gconf_client, KEY_YELP_USE_CARET, NULL);
    gconf_client_set_bool (gconf_client,
			   KEY_YELP_USE_CARET,
			   !caret, NULL);
}

/** Getters *******************************************************************/

const GtkIconTheme *
yelp_settings_get_icon_theme (void)
{
    return icon_theme;
}

GtkIconInfo *
yelp_settings_get_icon (YelpIconType icon)
{
    GtkIconInfo *info;

    g_return_val_if_fail (icon < YELP_NUM_ICONS, NULL);

    info = gtk_icon_theme_lookup_icon (icon_theme,
				       icon_names[icon],
				       48, 0);
    return info;
}

gchar *
yelp_settings_get_font (YelpFontType font)
{
    gchar *key;
    gchar *retval = NULL;
    GError *err = NULL;
    gboolean use;

    use = gconf_client_get_bool (gconf_client,
				 KEY_YELP_SYSTEM_FONTS,
				 &err);

    /* check for some kind of gconf error here; if error exists, then
     * default to using system fonts since the yelp font settings might
     * be messed up in gconf as well */
    if (err != NULL) {
	g_warning ("An error occurred getting the gconf value %s: %s\n",
	           KEY_YELP_SYSTEM_FONTS, err->message);
	g_error_free (err);
	err = NULL;
	use = FALSE;
    }

    switch (font) {
    case YELP_FONT_VARIABLE:
	key = (use ? KEY_GNOME_VARIABLE_FONT : KEY_YELP_VARIABLE_FONT);
	break;
    case YELP_FONT_FIXED:
	key = (use ? KEY_GNOME_FIXED_FONT : KEY_YELP_FIXED_FONT);
	break;
    default:
	g_assert_not_reached ();
    }

    retval = gconf_client_get_string (gconf_client, key, &err);

    if (err != NULL || retval == NULL) {
	g_warning ("An error occurred getting the gconf value '%s'\n", key);
	if (err) {
	    g_error_free (err);
	    err = NULL;
	}

	/* ok, if we were looking at the gnome system wide font setting
	 * and it failed, then look at the yelp font setting - and vice
	 * versa */
	if (font == YELP_FONT_VARIABLE)
	    key = (use ? KEY_YELP_VARIABLE_FONT : KEY_GNOME_VARIABLE_FONT);
	else if (font == YELP_FONT_FIXED)
	    key = (use ? KEY_YELP_FIXED_FONT : KEY_GNOME_FIXED_FONT);
	else g_assert_not_reached ();

	retval = gconf_client_get_string (gconf_client, key, &err);

	/* if we still have an error, something seriously went wrong, and
	 * well just hardcode the values */
	if (err != NULL || retval == NULL) {
	    g_warning ("An error occurred getting the gconf value '%s'\n", key);
	    if (err) {
		g_error_free (err);
		err = NULL;
	    }

	    if (font == YELP_FONT_VARIABLE)
		retval = g_strdup ("Sans 10");
	    else if (font == YELP_FONT_FIXED)
		retval = g_strdup ("Monospace 10");
	    else g_assert_not_reached ();
	}
    }

    return retval;
}

const gchar *
yelp_settings_get_color (YelpColorType color)
{
    g_return_val_if_fail (color < YELP_NUM_COLORS, NULL);

    return colors[color];
}

const gchar *
yelp_settings_get_css_file (void)
{
    return "file://" DATADIR "/yelp/default.css";
}

gboolean
yelp_settings_get_caret (void)
{
    gboolean caret;
    caret = gconf_client_get_bool (gconf_client,
				   KEY_YELP_USE_CARET,
				   NULL);
    return caret;
}

/** Widget Callbacks **********************************************************/

static void
prefs_system_fonts_cb (GtkWidget *widget)
{
    gboolean use;

    g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));

    use = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

    gconf_client_set_bool (gconf_client,
			   KEY_YELP_SYSTEM_FONTS,
			   use, NULL);
}

static void
prefs_font_cb (GtkWidget *widget)
{
    const gchar *key;
    const gchar *font;

    g_return_if_fail (GTK_IS_FONT_BUTTON (widget));

    font = gtk_font_button_get_font_name (GTK_FONT_BUTTON (widget));

    if (widget == variable_font_widget)
	key = KEY_YELP_VARIABLE_FONT;
    else if (widget == fixed_font_widget)
	key = KEY_YELP_FIXED_FONT;
    else
	g_assert_not_reached ();

    gconf_client_set_string (gconf_client,
			     key, font,
			     NULL);
}

static void
prefs_use_caret_cb (GtkWidget *widget)
{
    gboolean caret;

    g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));

    caret = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

    gconf_client_set_bool (gconf_client,
			   KEY_YELP_USE_CARET,
			   caret, NULL);
}

static void
toggle_font_table (GtkWidget *widget)
{
    gboolean use;

    g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));

    use = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

    gtk_widget_set_sensitive (font_table_widget, !use);
}

/** GConf Callbacks ***********************************************************/

static void
gconf_system_fonts_cb (GConfClient   *client,
		       guint          cnxn_id,
		       GConfEntry    *entry,
		       gpointer       data)
{
    gboolean use;

    debug_print (DB_FUNCTION, "entering\n");

    use = gconf_value_get_bool (gconf_entry_get_value (entry));

    if (prefs_dialog) {
	g_signal_handler_block (system_fonts_widget, system_fonts_handler);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (system_fonts_widget), use);
	g_signal_handler_unblock (system_fonts_widget, system_fonts_handler);
    }

    settings_update (YELP_SETTINGS_INFO_FONTS);
}

static void
gconf_font_cb (GConfClient   *client,
	       guint          cnxn_id,
	       GConfEntry    *entry,
	       gpointer       data)
{
    GtkFontButton *button;
    gulong handler;
    const gchar *font;

    debug_print (DB_FUNCTION, "entering\n");

    font = gconf_value_get_string (gconf_entry_get_value (entry));

    if (prefs_dialog) {
	if (g_str_equal (gconf_entry_get_key (entry), KEY_YELP_VARIABLE_FONT)) {
	    button = GTK_FONT_BUTTON (variable_font_widget);
	    handler = variable_font_handler;
	}
	else if (g_str_equal (gconf_entry_get_key (entry), KEY_YELP_FIXED_FONT)) {
	    button = GTK_FONT_BUTTON (fixed_font_widget);
	    handler = fixed_font_handler;
	}
	else
	    goto done;

	g_signal_handler_block (button, handler);
	gtk_font_button_set_font_name (button, font);
	g_signal_handler_unblock (button, handler);
    }

 done:
    settings_update (YELP_SETTINGS_INFO_FONTS);
}

static void
gconf_use_caret_cb (GConfClient  *client,
		    guint         cnxn_id,
		    GConfEntry   *entry,
		    gpointer      data)
{
    gboolean caret;

    caret = gconf_value_get_bool (gconf_entry_get_value (entry));

    if (prefs_dialog) {
	g_signal_handler_block (use_caret_widget, use_caret_handler);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (use_caret_widget), caret);
	g_signal_handler_unblock (use_caret_widget, use_caret_handler);
    }

    settings_update (YELP_SETTINGS_INFO_A11Y);
}

/******************************************************************************/

static void
gtk_theme_changed (GtkSettings *settings, gpointer user_data)
{
    debug_print (DB_FUNCTION, "entering\n");

    settings_update (YELP_SETTINGS_INFO_COLOR);
}

static void
icon_theme_changed (GtkIconTheme *theme, gpointer user_data)
{
    settings_update (YELP_SETTINGS_INFO_ICONS);
}

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
hls_to_hex (gdouble h, gdouble l, gdouble s, YelpColorType color)
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
    g_snprintf (colors[color], 8, "#%02X%02X%02X", red, green, blue);
}

static void
settings_update (YelpSettingsType type)
{
    GtkWidget *widget;
    GtkStyle  *style;
    GdkColor  *color;
    GdkColor   blue = { 0, 0x1E1E, 0x3E3E, 0xE7E7 };
    gdouble    base_h, base_l, base_s;
    gdouble    text_h, text_l, text_s;
    /* guint16    rval, gval, bval; */
    gint i;

    debug_print (DB_FUNCTION, "entering\n");

    if (type & YELP_SETTINGS_INFO_COLOR) {
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

	/* YELP_COLOR_FG */
	g_snprintf (colors[YELP_COLOR_FG], 8,
		    "#%02X%02X%02X",
		    style->text[GTK_STATE_NORMAL].red >> 8,
		    style->text[GTK_STATE_NORMAL].green >> 8,
		    style->text[GTK_STATE_NORMAL].blue >> 8);
	/* YELP_COLOR_BG */
	g_snprintf (colors[YELP_COLOR_BG], 8,
		    "#%02X%02X%02X",
		    style->base[GTK_STATE_NORMAL].red >> 8,
		    style->base[GTK_STATE_NORMAL].green >> 8,
		    style->base[GTK_STATE_NORMAL].blue >> 8);

	/* YELP_COLOR_ANCHOR */
	widget = gtk_link_button_new ("http://www.gnome.org");
	gtk_widget_style_get (widget, "link-color", &color, NULL);
	if (!color)
	    color = &blue;
	g_snprintf (colors[YELP_COLOR_ANCHOR], 8,
		    "#%02X%02X%02X",
		    color->red >> 8,
		    color->green >> 8,
		    color->blue >> 8);
	if (color != &blue)
	    gdk_color_free (color);
	
	color = NULL;
	gtk_widget_style_get (widget, "visited-link-color", &color, NULL);
	if (color) {
	    g_snprintf (colors[YELP_COLOR_ANCHOR], 8,
			"#%02X%02X%02X",
			color->red >> 8,
			color->green >> 8,
			color->blue >> 8);
	    gdk_color_free (color);
	}

	g_object_ref_sink (widget);
	g_object_unref (widget);

	hls_to_hex (text_h, 
		    text_l - ((text_l - base_l) * 0.25),
		    text_s,
		    YELP_COLOR_FG_LIGHT);

	hls_to_hex (base_h, 
		    base_l - ((base_l - text_l) * 0.03),
		    base_s,
		    YELP_COLOR_GRAY_BG);
	hls_to_hex (base_h, 
		    base_l - ((base_l - text_l) * 0.25),
		    base_s,
		    YELP_COLOR_GRAY_BORDER);

	hls_to_hex (204,
		    base_l - ((base_l - text_l) * 0.03),
		    0.75,
		    YELP_COLOR_BLUE_BG);
	hls_to_hex (204,
		    base_l - ((base_l - text_l) * 0.25),
		    0.75,
		    YELP_COLOR_BLUE_BORDER);

	hls_to_hex (0,
		    base_l - ((base_l - text_l) * 0.03),
		    0.75,
		    YELP_COLOR_RED_BG);
	hls_to_hex (0,
		    base_l - ((base_l - text_l) * 0.25),
		    0.75,
		    YELP_COLOR_RED_BORDER);

	hls_to_hex (60,
		    base_l - ((base_l - text_l) * 0.03),
		    0.75,
		    YELP_COLOR_YELLOW_BG);
	hls_to_hex (60,
		    base_l - ((base_l - text_l) * 0.25),
		    0.75,
		    YELP_COLOR_YELLOW_BORDER);

	g_object_unref (G_OBJECT (style));
    }

    for (i = 0; i < YELP_SETTINGS_NUM_TYPES; i++)
	if (type & (1 << i))
	    g_hook_list_invoke (hook_lists[i], FALSE);
}

void
yelp_settings_params (gchar ***params,
		      gint    *params_i,
		      gint    *params_max)
{
    GtkIconInfo *icon_info;
    gchar *icon_file, *icon_uri;
    gint colors_i , icons_i;

    if ((*params_i + 2 * (YELP_NUM_COLORS + YELP_NUM_ICONS)) >= *params_max) {
	*params_max += 2 * (YELP_NUM_COLORS + YELP_NUM_ICONS);
	*params = g_renew (gchar *, *params, *params_max);
    }

    for (colors_i = 0; colors_i < YELP_NUM_COLORS; colors_i++) {
	(*params)[(*params_i)++] = g_strdup ((gchar *) color_params[colors_i]);
	(*params)[(*params_i)++] = g_strdup_printf ("\"%s\"",
						yelp_settings_get_color (colors_i));
    }

    for (icons_i = 0; icons_i < YELP_NUM_ICONS; icons_i++) {
	(*params)[(*params_i)++] = g_strdup ((gchar *) icon_params[icons_i]);

	icon_info = yelp_settings_get_icon (icons_i);
	if (icon_info) {
	    icon_file = (gchar *) gtk_icon_info_get_filename (icon_info);
	    if (icon_file) {
		icon_uri = g_filename_to_uri (icon_file, NULL, NULL);
		(*params)[(*params_i)++] = g_strdup_printf ("\"%s\"", icon_uri);
		g_free (icon_uri);
	    }
	    else
		(*params)[(*params_i)++] = g_strdup ("\"\"");
	    gtk_icon_info_free (icon_info);
	} else {
	    (*params)[(*params_i)++] = g_strdup ("\"\"");
	}
    }
}
