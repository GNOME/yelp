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
    if (!style) {
	g_warning (_("Could not obtain a GtkStyle."));
    } else {
	int state = GTK_STATE_NORMAL;
	gushort rgb[3];
	int i;

	rgb[0] = style->bg[state].red >> 8;
	rgb[1] = style->bg[state].green >> 8;
	rgb[2] = style->bg[state].blue >> 8;

	gray_background[0] = '"';
	gray_background[1] = '#';
	for (i = 0; i < 3; i++) {
	    int j = rgb[i] / 16;
	    if (j < 10)
		gray_background[2*i+2] = '0' + j;
	    else
		gray_background[2*i+2] = 'A' + (j - 10);
	    j = rgb[i] % 16;
	    if (j < 10)
		gray_background[2*i+3] = '0' + j;
	    else
		gray_background[2*i+3] = 'A' + (j - 10);
	}
	gray_background[8] = '"';
	gray_background[9] = '\0';

	rgb[0] = style->dark[state].red >> 8;
	rgb[1] = style->dark[state].green >> 8;
	rgb[2] = style->dark[state].blue >> 8;

	gray_border[0] = '"';
	gray_border[1] = '#';
	for (i = 0; i < 3; i++) {
	    int j = rgb[i] / 16;
	    if (j < 10)
		gray_border[2*i+2] = '0' + j;
	    else
		gray_border[2*i+2] = 'A' + (j - 10);
	    j = rgb[i] % 16;
	    if (j < 10)
		gray_border[2*i+3] = '0' + j;
	    else
		gray_border[2*i+3] = 'A' + (j - 10);
	}
	gray_border[8] = '"';
	gray_border[9] = '\0';
    }
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
