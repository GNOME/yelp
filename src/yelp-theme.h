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

#ifndef __YELP_THEME_H__
#define __YELP_THEME_H__

#include <glib.h>
#include <gtk/gtkicontheme.h>

typedef enum {
    YELP_THEME_INFO_COLOR = 1 << 0,
    YELP_THEME_INFO_ICONS = 1 << 1,
    YELP_THEME_INFO_CSS   = 1 << 2,
    YELP_THEME_NUM_TYPES  = 3
} YelpThemeInfoType;

void          yelp_theme_init             (void);
guint         yelp_theme_notify_add       (guint      type,
					   GHookFunc  func,
					   gpointer   data);
void          yelp_theme_notify_remove    (guint      type,
					   guint      id);

const GtkIconTheme *  yelp_theme_get_icon_theme       (void);
const gchar *         yelp_theme_get_css_file         (void);
const gchar *         yelp_theme_get_gray_background  (void);
const gchar *         yelp_theme_get_gray_border      (void);

#endif /* __YELP_THEME_H__ */
