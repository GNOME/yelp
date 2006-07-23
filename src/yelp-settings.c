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
#include <glade/glade.h>
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
    "yelp.color.fg",
    "yelp.color.bg",
    "yelp.color.anchor",
    "yelp.color.rule",
    "yelp.color.gray.fg",
    "yelp.color.gray.bg",
    "yelp.color.gray.bg.dark1",
    "yelp.color.gray.bg.dark2",
    "yelp.color.gray.bg.dark3",
    "yelp.color.selected.fg",
    "yelp.color.selected.bg",
    "yelp.color.selected.bg.dark1",
    "yelp.color.selected.bg.dark2",
    "yelp.color.selected.bg.dark3",
    "yelp.color.admon.fg",
    "yelp.color.admon.bg",
    "yelp.color.admon.bg.dark1",
    "yelp.color.admon.bg.dark2",
    "yelp.color.admon.bg.dark3"
};

static const gchar * const icon_params[YELP_NUM_ICONS] = {
    "yelp.icon.blockquote",
    "yelp.icon.caution",
    "yelp.icon.important",
    "yelp.icon.note",
    "yelp.icon.programlisting",
    "yelp.icon.tip",
    "yelp.icon.warning"
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
	       use guillemets (angle quotations) should use either 00AB or 00BB,
	       depending on whether the opening quotation is the left guillemet
	       or the right guillemet.  Languages that use inverted comma style
	       quotations should use 201C, 201D, or 201E.  Note that single
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
	use_caret_widget     = glade_xml_get_widget (glade, "use_caret");
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
settings_update (YelpSettingsType type)
{
    GtkWidget *widget;
    GtkStyle  *style;
    GdkColor  *color;
    GdkColor   blue = { 0, 0x1E1E, 0x3E3E, 0xE7E7 };
    guint8     max_text, max_base;
    guint16    rval, gval, bval;
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

	max_text = MAX(style->text[GTK_STATE_NORMAL].red,
		       MAX(style->text[GTK_STATE_NORMAL].green,
			   style->text[GTK_STATE_NORMAL].blue   )) >> 8;
	max_base = MAX(style->base[GTK_STATE_NORMAL].red,
		       MAX(style->base[GTK_STATE_NORMAL].green,
			   style->base[GTK_STATE_NORMAL].blue   )) >> 8;

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
	gtk_object_sink (GTK_OBJECT (widget));

	/* YELP_COLOR_RULE */
	g_snprintf (colors[YELP_COLOR_RULE], 8,
		    "#%02X%02X%02X",
		    ((style->base[GTK_STATE_NORMAL].red >> 8) + 
		     (style->bg[GTK_STATE_NORMAL].red >> 8) ) / 2,
		    ((style->base[GTK_STATE_NORMAL].green >> 8) + 
		     (style->bg[GTK_STATE_NORMAL].green >> 8) ) / 2,
		    ((style->base[GTK_STATE_NORMAL].blue >> 8) + 
		     (style->bg[GTK_STATE_NORMAL].blue >> 8) ) / 2);

	/* YELP_COLOR_GRAY_BG */
	for (i = 0; i < 4; i++) {
	    rval = ((4 - i) * (style->bg[GTK_STATE_NORMAL].red   >> 8) +
		    i * max_text) / 4;
	    gval = ((4 - i) * (style->bg[GTK_STATE_NORMAL].green >> 8) +
		    i * max_text) / 4;
	    bval = ((4 - i) * (style->bg[GTK_STATE_NORMAL].blue  >> 8) +
		    i * max_text) / 4;
	    g_snprintf (colors[YELP_COLOR_GRAY_BG + i], 8,
			"#%02X%02X%02X", rval, gval, bval);
	}

	/* YELP_COLOR_GRAY_FG */
	g_snprintf (colors[YELP_COLOR_GRAY_FG], 8, "%s",
		    colors[YELP_COLOR_GRAY_BG_DARK3]);

	/* YELP_COLOR_SELECTED_FG */
	g_snprintf (colors[YELP_COLOR_SELECTED_FG], 8,
		    "#%02X%02X%02X",
		    style->text[GTK_STATE_SELECTED].red >> 8,
		    style->text[GTK_STATE_SELECTED].green >> 8,
		    style->text[GTK_STATE_SELECTED].blue >> 8);

	/* YELP_COLOR_SELECTED_BG */
	for (i = 0; i < 4; i++) {
	    rval = ((4 - i) * (style->bg[GTK_STATE_SELECTED].red   >> 8) +
		    i * max_text) / 4;
	    gval = ((4 - i) * (style->bg[GTK_STATE_SELECTED].green >> 8) +
		    i * max_text) / 4;
	    bval = ((4 - i) * (style->bg[GTK_STATE_SELECTED].blue  >> 8) +
		    i * max_text) / 4;
	    g_snprintf (colors[YELP_COLOR_SELECTED_BG + i], 8,
			"#%02X%02X%02X", rval, gval, bval);
	}

	/* YELP_COLOR_ADMON_FG */
	g_snprintf (colors[YELP_COLOR_ADMON_FG], 8, "%s",
		    colors[YELP_COLOR_GRAY_BG_DARK3]);

	/* YELP_COLOR_ADMON_BG */
	for (i = 0; i < 4; i++) {
	    gint mult = max_base + ((i * (max_base - max_text)) / 3);
	    rval = ((255 * mult) / 255);
	    gval = ((245 * mult) / 255);
	    bval = ((207 * mult) / 255);

	    g_snprintf (colors[YELP_COLOR_ADMON_BG + i], 8,
			"#%02X%02X%02X", rval, gval, bval);
	}

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
    gchar *icon_file;
    gint colors_i, icons_i;

    if ((*params_i + 2 * (YELP_NUM_COLORS + YELP_NUM_ICONS)) >= *params_max) {
	*params_max += 2 * (YELP_NUM_COLORS + YELP_NUM_ICONS);
	*params = g_renew (gchar *, *params, *params_max);
    }

    for (colors_i = 0; colors_i < YELP_NUM_COLORS; colors_i++) {
	(*params)[(*params_i)++] = (gchar *) color_params[colors_i];
	(*params)[(*params_i)++] = g_strdup_printf ("\"%s\"",
						yelp_settings_get_color (colors_i));
    }

    for (icons_i = 0; icons_i < YELP_NUM_ICONS; icons_i++) {
	(*params)[(*params_i)++] = (gchar *) icon_params[icons_i];

	icon_info = yelp_settings_get_icon (icons_i);
	if (icon_info) {
	    icon_file = (gchar *) gtk_icon_info_get_filename (icon_info);
	    if (icon_file)
		(*params)[(*params_i)++] = g_strdup_printf ("\"%s\"", icon_file);
	    else 
		(*params)[(*params_i)++] = g_strdup ("\"\"");
	    gtk_icon_info_free (icon_info);
	} else {
	    (*params)[(*params_i)++] = g_strdup ("\"\"");
	}
    }
}
