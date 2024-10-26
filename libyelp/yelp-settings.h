/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2004-2020 Shaun McCance <shaunm@gnome.org>
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifndef __YELP_SETTINGS_H__
#define __YELP_SETTINGS_H__

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define YELP_ZOOM_LEVEL_MIN 0.5
#define YELP_ZOOM_LEVEL_MAX 4.0

#define YELP_TYPE_SETTINGS         (yelp_settings_get_type ())
#define YELP_SETTINGS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_SETTINGS, YelpSettings))
#define YELP_SETTINGS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_SETTINGS, YelpSettingsClass))
#define YELP_IS_SETTINGS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_SETTINGS))
#define YELP_IS_SETTINGS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_SETTINGS))
#define YELP_SETTINGS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_SETTINGS, YelpSettingsClass))

typedef struct _YelpSettings        YelpSettings;
typedef struct _YelpSettingsClass   YelpSettingsClass;
typedef struct _YelpSettingsPrivate YelpSettingsPrivate;

struct _YelpSettings {
    GObject              parent;
    YelpSettingsPrivate *priv;
};

struct _YelpSettingsClass {
    GObjectClass      parent_class;
};

typedef enum {
    YELP_SETTINGS_COLOR_BASE,
    YELP_SETTINGS_COLOR_TEXT,
    YELP_SETTINGS_NUM_COLORS
} YelpSettingsColor;

typedef enum {
    YELP_SETTINGS_FONT_VARIABLE,
    YELP_SETTINGS_FONT_FIXED,
    YELP_SETTINGS_NUM_FONTS
} YelpSettingsFont;

GType               yelp_settings_get_type             (void);
YelpSettings *      yelp_settings_get_default          (void);

gchar *             yelp_settings_get_color            (YelpSettings       *settings,
                                                        YelpSettingsColor   color);
gchar **            yelp_settings_get_colors           (YelpSettings       *settings);
void                yelp_settings_set_colors           (YelpSettings       *settings,
                                                        YelpSettingsColor   first_color,
                                                        ...);
const gchar*        yelp_settings_get_color_param      (YelpSettingsColor   color);


gchar *             yelp_settings_get_font             (YelpSettings       *settings,
                                                        YelpSettingsFont    font);
gchar *             yelp_settings_get_font_family      (YelpSettings       *settings,
                                                        YelpSettingsFont    font);
gint                yelp_settings_get_font_size        (YelpSettings       *settings,
                                                        YelpSettingsFont    font);
void                yelp_settings_set_fonts            (YelpSettings       *settings,
                                                        YelpSettingsFont    first_font,
                                                        ...);
double              yelp_settings_get_zoom_level       (YelpSettings       *settings);
void                yelp_settings_set_zoom_level       (YelpSettings       *settings,
                                                        double              scale);
gchar **            yelp_settings_get_all_params       (YelpSettings       *settings,
                                                        gint                extra,
                                                        gint               *end);
gboolean            yelp_settings_get_show_text_cursor (YelpSettings       *settings);
void                yelp_settings_set_show_text_cursor (YelpSettings       *settings,
                                                        gboolean            show);

gboolean            yelp_settings_get_editor_mode      (YelpSettings       *settings);
void                yelp_settings_set_editor_mode      (YelpSettings       *settings,
                                                        gboolean            editor_mode);

void                yelp_settings_set_gtk_settings     (YelpSettings       *settings,
                                                        GtkSettings        *gtk_settings);

gint                yelp_settings_cmp_icons            (const gchar        *icon1,
                                                        const gchar        *icon2);

G_END_DECLS

#endif /* __YELP_SETTINGS_H__ */
