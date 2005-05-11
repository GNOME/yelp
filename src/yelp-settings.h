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

#ifndef __YELP_SETTINGS_H__
#define __YELP_SETTINGS_H__

#include <glib.h>
#include <gtk/gtkicontheme.h>

typedef enum {
    YELP_SETTINGS_INFO_COLOR = 1 << 0,
    YELP_SETTINGS_INFO_FONTS = 1 << 1,
    YELP_SETTINGS_INFO_ICONS = 1 << 2,
    YELP_SETTINGS_INFO_A11Y  = 1 << 3,
    YELP_SETTINGS_INFO_CSS   = 1 << 4,
    YELP_SETTINGS_NUM_TYPES  = 5,

    YELP_SETTINGS_INFO_ALL =
      YELP_SETTINGS_INFO_COLOR |
      YELP_SETTINGS_INFO_FONTS |
      YELP_SETTINGS_INFO_ICONS |
      YELP_SETTINGS_INFO_A11Y  |
      YELP_SETTINGS_INFO_CSS
} YelpSettingsType;

typedef enum {
    YELP_FONT_VARIABLE = 0,
    YELP_FONT_FIXED,
    YELP_NUM_FONTS
} YelpFontType;

typedef enum {
    YELP_COLOR_FG = 0,
    YELP_COLOR_BG,
    YELP_COLOR_ANCHOR,
    YELP_COLOR_RULE,
    YELP_COLOR_GRAY_FG,
    YELP_COLOR_GRAY_BG,
    YELP_COLOR_GRAY_BG_DARK1,
    YELP_COLOR_GRAY_BG_DARK2,
    YELP_COLOR_GRAY_BG_DARK3,
    YELP_COLOR_SELECTED_FG,
    YELP_COLOR_SELECTED_BG,
    YELP_COLOR_SELECTED_BG_DARK1,
    YELP_COLOR_SELECTED_BG_DARK2,
    YELP_COLOR_SELECTED_BG_DARK3,
    YELP_COLOR_ADMON_FG,
    YELP_COLOR_ADMON_BG,
    YELP_COLOR_ADMON_BG_DARK1,
    YELP_COLOR_ADMON_BG_DARK2,
    YELP_COLOR_ADMON_BG_DARK3,
    YELP_NUM_COLORS
} YelpColorType;

typedef enum {
    YELP_ICON_BLOCKQUOTE = 0,
    YELP_ICON_CAUTION,
    YELP_ICON_IMPORTANT,
    YELP_ICON_NOTE,
    YELP_ICON_PROGRAMLISTING,
    YELP_ICON_TIP,
    YELP_ICON_WARNING,
    YELP_NUM_ICONS
} YelpIconType;

void                  yelp_settings_init                 (void);
void                  yelp_settings_open_preferences     (void);

guint                 yelp_settings_notify_add           (YelpSettingsType type,
							  GHookFunc        func,
							  gpointer         data);
void                  yelp_settings_notify_remove        (YelpSettingsType type,
							  guint            id);

const GtkIconTheme *  yelp_settings_get_icon_theme       (void);
GtkIconInfo *         yelp_settings_get_icon             (YelpIconType     icon);
gchar *               yelp_settings_get_font             (YelpFontType     font);
const gchar *         yelp_settings_get_color            (YelpColorType    color);
const gchar *         yelp_settings_get_css_file         (void);
gboolean              yelp_settings_get_caret            (void);

void                  yelp_settings_params               (gchar         ***params,
							  gint            *params_i,
							  gint            *params_max);

#endif /* __YELP_SETTINGS_H__ */
