/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003 Shaun McCance <shaunm@gnome.org>
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

#include <string.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <gconf/gconf-client.h>
#include "yelp-theme.h"

#define KEY_INTERFACE_DIR    "/desktop/gnome/interface"
#define KEY_GTK_THEME        KEY_INTERFACE_DIR "/gtk_theme"

static GHookList    *hook_lists[YELP_THEME_NUM_TYPES];

static GtkIconTheme *icon_theme;

static gchar gray_background[10];
static gchar gray_border[10];

static void     yelp_theme_update      (guint         type);
static void     yelp_theme_notify      (GConfClient  *gconf,
					guint         id,
					GConfEntry   *entry,
					gpointer      user_data);
static void     icon_theme_changed     (GtkIconTheme *theme,
					gpointer      user_data);

void
yelp_theme_init (void)
{
    GConfClient *gconf;
    gint i;

    gconf = gconf_client_get_default ();
    gconf_client_notify_add (gconf,
			     KEY_INTERFACE_DIR,
			     yelp_theme_notify,
			     NULL, NULL, NULL);

    for (i = 0; i < YELP_THEME_NUM_TYPES; i++) {
	hook_lists[i] = g_new0 (GHookList, 1);
	g_hook_list_init (hook_lists[i], sizeof (GHook));
    }

    icon_theme = gtk_icon_theme_get_default ();
    g_signal_connect (icon_theme,
		      "changed",
		      (GCallback) icon_theme_changed,
		      NULL);

    yelp_theme_update (YELP_THEME_INFO_COLOR |
		       YELP_THEME_INFO_ICONS |
		       YELP_THEME_INFO_CSS   );

}

guint
yelp_theme_notify_add (guint      type,
		       GHookFunc  func,
		       gpointer   data)
{
    GHook *hook;
    gint   i;

    for (i = 0; i < YELP_THEME_NUM_TYPES; i++) {
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
yelp_theme_notify_remove (guint type, guint id)
{
    gint   i;

    for (i = 0; i < YELP_THEME_NUM_TYPES; i++)
	if (type & (1 << i))
	    g_hook_destroy (hook_lists[i], id);
}

const GtkIconTheme*
yelp_theme_get_icon_theme (void)
{
    return (const GtkIconTheme *) icon_theme;
}

const gchar *
yelp_theme_get_css_file (void)
{
    return "file://" DATADIR "/yelp/default.css";
}

const gchar *
yelp_theme_get_gray_background (void)
{
    return gray_background;
}

const gchar *
yelp_theme_get_gray_border (void)
{
    return gray_border;
}

static void
yelp_theme_update (guint type)
{
    GtkStyle *style;
    gint i;

    if (type & YELP_THEME_INFO_COLOR) {
	style = gtk_rc_get_style_by_paths (gtk_settings_get_default (),
					   "GtkWidget", "GtkWidget",
					   GTK_TYPE_WIDGET);
	if (style)
	    g_object_ref (G_OBJECT (style));
	else
	    style = gtk_style_new ();

	g_snprintf (gray_background, 10,
		    "\"#%02X%02X%02X\"",
		    style->bg[GTK_STATE_NORMAL].red >> 8,
		    style->bg[GTK_STATE_NORMAL].green >> 8,
		    style->bg[GTK_STATE_NORMAL].blue >> 8);
	g_snprintf (gray_border, 10,
		    "\"#%02X%02X%02X\"",
		    style->dark[GTK_STATE_NORMAL].red >> 8,
		    style->dark[GTK_STATE_NORMAL].green >> 8,
		    style->dark[GTK_STATE_NORMAL].blue >> 8);

	g_object_unref (G_OBJECT (style));
    }

    for (i = 0; i < YELP_THEME_NUM_TYPES; i++)
	if (type & (1 << i))
	    g_hook_list_invoke (hook_lists[i], FALSE);
}

static void
yelp_theme_notify (GConfClient *gconf,
		   guint        id,
		   GConfEntry  *entry,
		   gpointer     user_data)
{
    g_return_if_fail (entry && entry->key);

    if (g_str_equal (entry->key, KEY_GTK_THEME)) {
	yelp_theme_update (YELP_THEME_INFO_COLOR);
    }
}

static void
icon_theme_changed (GtkIconTheme *theme, gpointer user_data)
{
    yelp_theme_update (YELP_THEME_INFO_ICONS);
}
