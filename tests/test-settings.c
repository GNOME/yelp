/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009 Shaun McCance  <shaunm@gnome.org>
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
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#include <gtk/gtk.h>
#include <webkit/webkit.h>

#include "yelp-settings.h"

static YelpSettings *settings;
static WebKitWebSettings *websettings;
static GtkWidget *webview;
static GtkWidget *color_table;
static GtkWidget *color_buttons[YELP_SETTINGS_NUM_COLORS];
static GtkWidget *icon_table;
static GtkWidget *icon_choosers[YELP_SETTINGS_NUM_ICONS];
static GtkWidget *icon_images[YELP_SETTINGS_NUM_ICONS];
static GtkWidget *font_choosers[YELP_SETTINGS_NUM_FONTS];

static void
colors_changed (YelpSettings *settings, gpointer user_data)
{
    static const gchar *tmpl =
	"<html><head>"
	"<style>"
	"body { background-color: %s; color: %s; }"
	"div, pre {"
	"  margin: 0.5em; padding: 0.2em;"
	"  border: solid 3px %s;"
	"}"
	"</style>"
	"</head><body>"
	"<pre>SOME\nMONOSPACE\nTEXT</pre>"
	"<div><table>"
	"<tr><td>YELP_SETTINGS_COLOR_BASE:</td><td>%s</td></tr>"
	"<tr style='color: %s'><td>YELP_SETTINGS_COLOR_TEXT:</td><td>%s</td></tr>"
	"<tr style='color: %s'><td>YELP_SETTINGS_COLOR_TEXT_LIGHT:</td><td>%s</td></tr>"
	"<tr style='color: %s'><td>YELP_SETTINGS_COLOR_LINK:</td><td>%s</td></tr>"
	"<tr style='color: %s'><td>YELP_SETTINGS_COLOR_LINK_VISITED:</td><td>%s</td></tr>"
	"</table></div>"
	"<div style='background-color:%s;border-color:%s'>"
	"<table><tr><td>YELP_SETTINGS_COLOR_GRAY_BASE:</td><td>%s</td></tr>"
	"<tr><td>YELP_SETTINGS_COLOR_GRAY_BORDER:</td><td>%s</tr></table></div>"
	"<div style='background-color:%s;border-color:%s'>"
	"<table><tr><td>YELP_SETTINGS_COLOR_BLUE_BASE:</td><td>%s</td></tr>"
	"<tr><td>YELP_SETTINGS_COLOR_BLUE_BORDER:</td><td>%s</tr></table></div>"
	"<div style='background-color:%s;border-color:%s'>"
	"<table><tr><td>YELP_SETTINGS_COLOR_RED_BASE:</td><td>%s</td></tr>"
	"<tr><td>YELP_SETTINGS_COLOR_RED_BORDER:</td><td>%s</tr></table></div>"
	"<div style='background-color:%s;border-color:%s'>"
	"<table><tr><td>YELP_SETTINGS_COLOR_YELLOW_BASE:</td><td>%s</td></tr>"
	"<tr><td>YELP_SETTINGS_COLOR_YELLOW_BORDER:</td><td>%s</tr></table></div>"
	"</body></html>";
    gchar **colors = yelp_settings_get_colors (settings);
    gchar *page;
    page = g_strdup_printf (tmpl,
			    colors[YELP_SETTINGS_COLOR_BASE], colors[YELP_SETTINGS_COLOR_TEXT],
			    colors[YELP_SETTINGS_COLOR_BASE], colors[YELP_SETTINGS_COLOR_BASE],
			    colors[YELP_SETTINGS_COLOR_TEXT], colors[YELP_SETTINGS_COLOR_TEXT],
			    colors[YELP_SETTINGS_COLOR_TEXT_LIGHT], colors[YELP_SETTINGS_COLOR_TEXT_LIGHT],
			    colors[YELP_SETTINGS_COLOR_LINK], colors[YELP_SETTINGS_COLOR_LINK],
			    colors[YELP_SETTINGS_COLOR_LINK_VISITED], colors[YELP_SETTINGS_COLOR_LINK_VISITED],
			    colors[YELP_SETTINGS_COLOR_GRAY_BASE], colors[YELP_SETTINGS_COLOR_GRAY_BORDER],
			    colors[YELP_SETTINGS_COLOR_GRAY_BASE], colors[YELP_SETTINGS_COLOR_GRAY_BORDER],
			    colors[YELP_SETTINGS_COLOR_BLUE_BASE], colors[YELP_SETTINGS_COLOR_BLUE_BORDER],
			    colors[YELP_SETTINGS_COLOR_BLUE_BASE], colors[YELP_SETTINGS_COLOR_BLUE_BORDER],
			    colors[YELP_SETTINGS_COLOR_RED_BASE], colors[YELP_SETTINGS_COLOR_RED_BORDER],
			    colors[YELP_SETTINGS_COLOR_RED_BASE], colors[YELP_SETTINGS_COLOR_RED_BORDER],
			    colors[YELP_SETTINGS_COLOR_YELLOW_BASE], colors[YELP_SETTINGS_COLOR_YELLOW_BORDER],
			    colors[YELP_SETTINGS_COLOR_YELLOW_BASE], colors[YELP_SETTINGS_COLOR_YELLOW_BORDER]);
    webkit_web_view_load_string (WEBKIT_WEB_VIEW (webview),
                                 page,
                                 "text/html",
                                 "UTF-8",
                                 "file:///dev/null");
    g_free (page);
    g_strfreev (colors);
}

static void
icons_changed (YelpSettings *settings, gpointer user_data)
{
    gint i;
    for (i = 0; i < YELP_SETTINGS_NUM_ICONS; i++) {
	gchar *filename = yelp_settings_get_icon (settings, i);
	if (filename != NULL) {
	    gtk_image_set_from_file (GTK_IMAGE (icon_images[i]), filename);
	    g_free (filename);
	}
    }
}

static void
fonts_changed (YelpSettings *settings, gpointer user_data)
{
    g_object_set (websettings,
		  "default-font-family", yelp_settings_get_font_family (settings, YELP_SETTINGS_FONT_VARIABLE),
		  "default-font-size", yelp_settings_get_font_size (settings, YELP_SETTINGS_FONT_VARIABLE),
		  "monospace-font-family", yelp_settings_get_font_family (settings, YELP_SETTINGS_FONT_FIXED),
		  "default-monospace-font-size", yelp_settings_get_font_size (settings, YELP_SETTINGS_FONT_FIXED),
		  NULL);
}

static void
color_set (GtkColorButton *button,
	   gpointer        user_data)
{
    GdkColor color;
    gchar str[8];
    gint i;
    for (i = 0; i < YELP_SETTINGS_NUM_COLORS; i++)
	if ((gpointer) color_buttons[i] == (gpointer) button)
	    break;
    g_return_if_fail (i < YELP_SETTINGS_NUM_COLORS);

    gtk_color_button_get_color (button, &color);
    g_snprintf (str, 8, "#%02X%02X%02X", color.red / 255, color.green / 255, color.blue / 255);
    yelp_settings_set_colors (settings, i, str, -1);
}

static void
font_set (GtkFontButton *button,
	  GParamSpec    *pspec,
	  gpointer       user_data)
{
    gint i;
    for (i = 0; i < YELP_SETTINGS_NUM_FONTS; i++)
	if ((gpointer) font_choosers[i] == (gpointer) button)
	    break;
    g_return_if_fail (i < YELP_SETTINGS_NUM_FONTS);

    yelp_settings_set_fonts (settings, i, gtk_font_button_get_font_name (button), -1);
}

static void
icon_file_set (GtkFileChooserButton *button,
	       gpointer              user_data)
{
    gchar *filename;
    gint i;
    for (i = 0; i < YELP_SETTINGS_NUM_ICONS; i++)
	if ((gpointer) icon_choosers[i] == (gpointer) button)
	    break;
    g_return_if_fail (i < YELP_SETTINGS_NUM_ICONS);

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (button));
    yelp_settings_set_icons (settings, i, filename, -1);
    g_free (filename);
}

static void
use_gtk_settings_toggled (GtkToggleButton *button,
			  gpointer         user_data)
{
    if (gtk_toggle_button_get_active (button)) {
	g_object_set (settings, "gtk-settings", gtk_settings_get_default (), NULL);
	gtk_widget_set_sensitive (color_table, FALSE);
    }
    else {
	g_object_set (settings, "gtk-settings", NULL, NULL);
	gtk_widget_set_sensitive (color_table, TRUE);
    }
}

static void
use_gtk_icon_theme_toggled (GtkToggleButton *button,
			    gpointer         user_data)
{
    if (gtk_toggle_button_get_active (button)) {
	g_object_set (settings, "gtk-icon-theme", gtk_icon_theme_get_default (), NULL);
	gtk_widget_set_sensitive (icon_table, FALSE);
    }
    else {
	g_object_set (settings, "gtk-icon-theme", NULL, NULL);
	gtk_widget_set_sensitive (icon_table, TRUE);
    }
}

static void
use_small_icons_toggled (GtkToggleButton *button,
			 gpointer         user_data)
{
    yelp_settings_set_icon_size (settings, gtk_toggle_button_get_active (button) ? 24 : 48);
}

int
main (int argc, char **argv)
{
    GtkWidget *window, *hbox, *vbox, *widget, *scroll, *table;
    gint i;

    gtk_init (&argc, &argv);

    settings = yelp_settings_get_default ();

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size (GTK_WINDOW (window), 1024, 600);
    g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

    hbox = gtk_hbox_new (FALSE, 12);
    g_object_set (hbox, "border-width", 6, NULL);
    gtk_container_add (GTK_CONTAINER (window), hbox);

    vbox = gtk_vbox_new (FALSE, 6);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

    widget = gtk_check_button_new_with_label ("Use GtkSettings");
    g_object_set (widget, "active", TRUE, NULL);
    g_signal_connect (widget, "toggled", G_CALLBACK (use_gtk_settings_toggled), NULL);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

    color_table = gtk_table_new (2, 7, TRUE);
    gtk_widget_set_sensitive (color_table, FALSE);
    gtk_box_pack_start (GTK_BOX (vbox), color_table, FALSE, FALSE, 0);

    for (i = 0; i < YELP_SETTINGS_NUM_COLORS; i++) {
	color_buttons[i] = gtk_color_button_new ();
	g_signal_connect (color_buttons[i], "color-set", G_CALLBACK (color_set), NULL);
	if (i == 0) {
	    gtk_table_attach (GTK_TABLE (color_table), color_buttons[i],
			      0, 1, 0, 2, 0, GTK_FILL, 0, 0);
	}
	else {
	    gtk_table_attach (GTK_TABLE (color_table), color_buttons[i],
			      (i + 1) / 2,
			      (i + 1) / 2 + 1,
			      (i + 1) % 2, 
			      (i + 1) % 2 + 1,
			      0, 0, 0, 0);
	}
    }

    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);

    webview = webkit_web_view_new ();
    websettings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (webview));
    gtk_container_add (GTK_CONTAINER (scroll), webview);

    vbox = gtk_vbox_new (FALSE, 6);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

    table = gtk_table_new (2, 2, FALSE);
    gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);

    widget = gtk_label_new ("Variable");
    g_object_set (widget, "xalign", 0.0, NULL);
    gtk_table_attach (GTK_TABLE (table), widget, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
    font_choosers[YELP_SETTINGS_FONT_VARIABLE] = gtk_font_button_new ();
    g_signal_connect (font_choosers[YELP_SETTINGS_FONT_VARIABLE], "notify::font-name",
		      G_CALLBACK (font_set), NULL);
    gtk_font_button_set_font_name (GTK_FONT_BUTTON (font_choosers[YELP_SETTINGS_FONT_VARIABLE]),
				   "Sans 8");
    gtk_table_attach (GTK_TABLE (table), font_choosers[YELP_SETTINGS_FONT_VARIABLE],
		      1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 6, 0);

    widget = gtk_label_new ("Fixed");
    g_object_set (widget, "xalign", 0.0, NULL);
    gtk_table_attach (GTK_TABLE (table), widget, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
    font_choosers[YELP_SETTINGS_FONT_FIXED] = gtk_font_button_new ();
    g_signal_connect (font_choosers[YELP_SETTINGS_FONT_FIXED], "notify::font-name",
		      G_CALLBACK (font_set), NULL);
    gtk_font_button_set_font_name (GTK_FONT_BUTTON (font_choosers[YELP_SETTINGS_FONT_FIXED]),
				   "Monospace 8");
    gtk_table_attach (GTK_TABLE (table), font_choosers[YELP_SETTINGS_FONT_FIXED],
		      1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 6, 0);

    widget = gtk_check_button_new_with_label ("Use GtkIconTheme");
    g_object_set (widget, "active", TRUE, NULL);
    g_signal_connect (widget, "toggled", G_CALLBACK (use_gtk_icon_theme_toggled), NULL);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

    widget = gtk_check_button_new_with_label ("Use small icons");
    g_signal_connect (widget, "toggled", G_CALLBACK (use_small_icons_toggled), NULL);
    gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

    icon_table = gtk_table_new (6, 2, FALSE);
    gtk_widget_set_sensitive (icon_table, FALSE);
    gtk_box_pack_start (GTK_BOX (vbox), icon_table, FALSE, FALSE, 0);

    for (i = 0; i < YELP_SETTINGS_NUM_ICONS; i++) {
	const gchar *labels[YELP_SETTINGS_NUM_ICONS] =
	    {"BUG", "CAUTION", "IMPORTANT", "NOTE", "TIP", "WARNING"};
	widget = gtk_label_new (labels[i]);
	g_object_set (widget, "xalign", 0.0, NULL);
	gtk_table_attach (GTK_TABLE (icon_table), widget,
			  0, 1, i, i + 1, GTK_FILL, 0, 0, 0);
	icon_choosers[i] = gtk_file_chooser_button_new (labels[i], GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_table_attach (GTK_TABLE (icon_table), icon_choosers[i],
			  1, 2, i, i + 1, GTK_FILL | GTK_EXPAND, GTK_FILL, 6, 0);
	g_signal_connect (icon_choosers[i], "file-set", G_CALLBACK (icon_file_set), NULL);
    }

    hbox = gtk_hbox_new (TRUE, 6);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    for (i = 0; i < YELP_SETTINGS_NUM_ICONS; i++) {
	icon_images[i] = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (hbox), icon_images[i], FALSE, FALSE, 0);
    }

    g_signal_connect (settings, "fonts-changed", G_CALLBACK (fonts_changed), NULL);
    fonts_changed (settings, NULL);
    g_signal_connect (settings, "colors-changed", G_CALLBACK (colors_changed), NULL);
    colors_changed (settings, NULL);
    g_signal_connect (settings, "icons-changed", G_CALLBACK (icons_changed), NULL);
    icons_changed (settings, NULL);

    gtk_widget_show_all (GTK_WIDGET (window));

    gtk_main ();
}
