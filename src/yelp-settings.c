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

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-href.h>
#include <string.h>
#include "yelp-settings.h"

#define KEY_GNOME_DIR           "/desktop/gnome/interface"
#define KEY_GNOME_VARIABLE_FONT KEY_GNOME_DIR "/font_name"
#define KEY_GNOME_FIXED_FONT    KEY_GNOME_DIR "/monospace_font_name"
#define KEY_GNOME_GTK_THEME     KEY_GNOME_DIR "/gtk_theme"
#define KEY_YELP_DIR            "/apps/yelp"
#define KEY_YELP_SYSTEM_FONTS   KEY_YELP_DIR "/use_system_fonts"
#define KEY_YELP_VARIABLE_FONT  KEY_YELP_DIR "/variable_font"
#define KEY_YELP_FIXED_FONT     KEY_YELP_DIR "/fixed_font"

#ifdef YELP_DEBUG
#define d(x) x
#else
#define d(x)
#endif

static void           settings_update         (YelpSettingsType  type);
static void           prefs_system_fonts_cb   (GtkWidget        *widget);
static void           prefs_font_cb           (GtkWidget        *widget);
static void           toggle_font_table       (GtkWidget        *widget);
static void           gconf_system_fonts_cb   (GConfClient      *client,
					       guint             cnxn_id,
					       GConfEntry       *entry,
					       gpointer          data);
static void           gconf_font_cb           (GConfClient      *client,
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
gulong system_fonts_handler  = 0;
gulong variable_font_handler = 0;
gulong fixed_font_handler    = 0;

void
yelp_settings_init (void)
{
    gint i;

    for (i = 0; i < YELP_NUM_ICONS; i++) {
	switch (i) {
	case YELP_ICON_BLOCKQUOTE:
	    /* TRANSLATORS:
	       This is an image of the opening quote character used to watermark
	       blockquote elements.  Different languages use different opening
	       quote characters, so the icon name is translatable.  The name of
	       the icon should be "yelp-watermark-blockquote-XXXX", where XXXX
	       is the Unicode code point of the opening quote character.   For
	       example, some languages use the double angle quotation mark, so
	       those would use "yelp-watermark-blockquote-00AB".  However, the
	       image is not automagically created.  Do not translate this to a
	       value if there isn't a corresponding icon in yelp/data/icons.
	       If you need an image created, contact the maintainers.

	       Phew, now some notes on which character to use.  Languages that
	       use guillemets (angle quotations) should use either 00AB or AABB,
	       depending on whether the opening quotation is the left guillemet
	       or the right guillemet.  Languages that use inverted comma style
	       quotations should use either 201C or 201E, depending on whether
	       the opening quote is raised or at the baseline.  Note that single
	       quotation marks don't make very nice watermarks.  So if you use
	       single quotes as your primary (outer) quotation marks, you should
	       just use the corresponding double quote watermark.
	    */
	    icon_names[i] = _("yelp-watermark-blockquote-201C");
	    break;
	case YELP_ICON_CAUTION:
	    icon_names[i] = "yelp-icon-caution";
	    break;
	case YELP_ICON_IMPORTANT:
	    icon_names[i] = "yelp-icon-important";
	    break;
	case YELP_ICON_NOTE:
	    icon_names[i] = "yelp-icon-note";
	    break;
	case YELP_ICON_PROGRAMLISTING:
	    icon_names[i] = "yelp-watermark-programlisting";
	    break;
	case YELP_ICON_TIP:
	    icon_names[i] = "yelp-icon-tip";
	    break;
	case YELP_ICON_WARNING:
	    icon_names[i] = "yelp-icon-warning";
	    break;
	default:
	    g_assert_not_reached ();
	}
    }

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
    gtk_icon_theme_append_search_path (icon_theme,
				       DATADIR "/yelp/icons");
    g_signal_connect (icon_theme,
		      "changed",
		      (GCallback) icon_theme_changed,
		      NULL);

    settings_update (YELP_SETTINGS_INFO_ALL);
}

void
yelp_settings_open_preferences (void)
{
    gchar *font;
    gboolean use;

    if (!prefs_dialog) {
	GladeXML *glade;
	glade = glade_xml_new (DATADIR "/yelp/ui/yelp.glade",
			       "prefs_dialog",
			       NULL);
	if (!glade) {
	    g_warning ("Could not find necessary glade file "
		       DATADIR "/yelp/ui/yelp.glade");
	    return;
	}

	prefs_dialog  = glade_xml_get_widget (glade, "prefs_dialog");
	system_fonts_widget  = glade_xml_get_widget (glade, "use_system_fonts");
	font_table_widget    = glade_xml_get_widget (glade, "font_table");
	variable_font_widget = glade_xml_get_widget (glade, "variable_font");
	fixed_font_widget    = glade_xml_get_widget (glade, "fixed_font");

	use = gconf_client_get_bool (gconf_client, KEY_YELP_SYSTEM_FONTS, NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (system_fonts_widget), use);
	gtk_widget_set_sensitive (font_table_widget, !use);

	font = gconf_client_get_string (gconf_client, KEY_YELP_VARIABLE_FONT, NULL);
	gtk_font_button_set_font_name (GTK_FONT_BUTTON (variable_font_widget), font);
	g_free (font);

	font = gconf_client_get_string (gconf_client, KEY_YELP_FIXED_FONT, NULL);
	gtk_font_button_set_font_name (GTK_FONT_BUTTON (fixed_font_widget), font);
	g_free (font);

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

	g_object_unref (glade);
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
				       36, 0);
    return info;
}

gchar *
yelp_settings_get_font (YelpFontType font)
{
    gchar *key;
    gboolean use;

    use = gconf_client_get_bool (gconf_client,
				 KEY_YELP_SYSTEM_FONTS,
				 NULL);

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

    return gconf_client_get_string (gconf_client, key, NULL);
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

    d (g_print ("gconf_system_fonts_cb\n"));

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

    d (g_print ("gconf_font_cb\n"));

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

/******************************************************************************/

static void
gtk_theme_changed (GtkSettings *settings, gpointer user_data)
{
    d (g_print ("gtk_theme_changed\n"));

    settings_update (YELP_SETTINGS_INFO_COLOR);
}

static void
icon_theme_changed (GtkIconTheme *theme, gpointer user_data)
{
    settings_update (YELP_SETTINGS_INFO_ICONS);
}

static void
settings_update (YelpSettingsType type)
{
    gint i;
    GtkStyle *style;

    d (g_print ("settings_update\n"));

    if (type & YELP_SETTINGS_INFO_COLOR) {
	style = gtk_rc_get_style_by_paths (gtk_settings,
					   "GtkWidget", "GtkWidget",
					   GTK_TYPE_WIDGET);
	if (style)
	    g_object_ref (G_OBJECT (style));
	else
	    style = gtk_style_new ();

	g_snprintf (colors[YELP_COLOR_TEXT], 8,
		    "#%02X%02X%02X",
		    style->fg[GTK_STATE_NORMAL].red >> 8,
		    style->fg[GTK_STATE_NORMAL].green >> 8,
		    style->fg[GTK_STATE_NORMAL].blue >> 8);
	g_snprintf (colors[YELP_COLOR_ANCHOR], 8,
		    "#%02X%02X%02X",
		    style->fg[GTK_STATE_NORMAL].red >> 8,
		    style->fg[GTK_STATE_NORMAL].green >> 8,
		    style->fg[GTK_STATE_NORMAL].blue >> 8);
	g_snprintf (colors[YELP_COLOR_BACKGROUND], 8,
		    "#%02X%02X%02X",
		    style->white.red >> 8,
		    style->white.green >> 8,
		    style->white.blue >> 8);
	g_snprintf (colors[YELP_COLOR_GRAY_BACKGROUND], 8,
		    "#%02X%02X%02X",
		    style->bg[GTK_STATE_NORMAL].red >> 8,
		    style->bg[GTK_STATE_NORMAL].green >> 8,
		    style->bg[GTK_STATE_NORMAL].blue >> 8);
	g_snprintf (colors[YELP_COLOR_GRAY_BACKGROUND], 8,
		    "#%02X%02X%02X",
		    style->dark[GTK_STATE_NORMAL].red >> 8,
		    style->dark[GTK_STATE_NORMAL].green >> 8,
		    style->dark[GTK_STATE_NORMAL].blue >> 8);

	g_object_unref (G_OBJECT (style));
    }

    for (i = 0; i < YELP_SETTINGS_NUM_TYPES; i++)
	if (type & (1 << i))
	    g_hook_list_invoke (hook_lists[i], FALSE);
}
