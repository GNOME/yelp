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
#include "yelp-theme.h"

GtkIconTheme *icon_theme;

gchar gray_background[10];
gchar gray_border[10];

void
yelp_theme_init (void)
{
    GtkStyle *style;

    icon_theme = gtk_icon_theme_get_default ();

    style = gtk_rc_get_style_by_paths (gtk_settings_get_default (),
				       "GtkWidget", "GtkWidget",
				       GTK_TYPE_WIDGET);

    if (!style)
	style = gtk_style_new ();

    g_object_ref (G_OBJECT (style));

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

const GtkIconTheme*
yelp_theme_get_icon_theme (void)
{
    return (const GtkIconTheme *) icon_theme;
}

const gchar*
yelp_theme_get_gray_background (void)
{
    return gray_background;
}

const gchar*
yelp_theme_get_gray_border (void)
{
    return gray_border;
}
