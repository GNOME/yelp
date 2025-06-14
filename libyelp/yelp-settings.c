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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "yelp-settings.h"

struct _YelpSettingsPrivate {
    GMutex        mutex;

    gchar         colors[YELP_SETTINGS_NUM_COLORS][8];
    gchar        *setfonts[YELP_SETTINGS_NUM_FONTS];
    gchar        *fonts[YELP_SETTINGS_NUM_FONTS];

    GtkSettings  *gtk_settings;
    GtkIconTheme *gtk_icon_theme;

    gint          font_adjustment;

    gulong        gtk_theme_changed;
    gulong        gtk_font_changed;

    gboolean      show_text_cursor;

    gboolean      editor_mode;

    GHashTable   *tokens;
};

enum {
    COLORS_CHANGED,
    FONTS_CHANGED,
    LAST_SIGNAL
};
static guint settings_signals[LAST_SIGNAL] = {0,};

enum {  
  PROP_0,
  PROP_GTK_SETTINGS,
  PROP_GTK_ICON_THEME,
  PROP_FONT_ADJUSTMENT,
  PROP_SHOW_TEXT_CURSOR,
  PROP_EDITOR_MODE
};

G_DEFINE_TYPE_WITH_PRIVATE (YelpSettings, yelp_settings, G_TYPE_OBJECT)

static void           yelp_settings_constructed  (GObject              *object);
static void           yelp_settings_finalize     (GObject              *object);
static void           yelp_settings_get_property (GObject              *object,
						  guint                 prop_id,
						  GValue               *value,
						  GParamSpec           *pspec);
static void           yelp_settings_set_property (GObject              *object,
						  guint                 prop_id,
						  const GValue         *value,
						  GParamSpec           *pspec);
static void           yelp_settings_set_if_token (YelpSettings         *settings,
                                                  const gchar          *token);

static void           gtk_theme_changed          (GtkSettings          *gtk_settings,
						  GParamSpec           *pspec,
					          YelpSettings         *settings);
static void           gtk_font_changed           (GtkSettings          *gtk_settings,
						  GParamSpec           *pspec,
					          YelpSettings         *settings);


/******************************************************************************/

static void
yelp_settings_class_init (YelpSettingsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->constructed  = yelp_settings_constructed;
    object_class->finalize = yelp_settings_finalize;
    object_class->get_property = yelp_settings_get_property;
    object_class->set_property = yelp_settings_set_property;

    g_object_class_install_property (object_class,
                                     PROP_GTK_SETTINGS,
                                     g_param_spec_object ("gtk-settings",
							  "GtkSettings",
							  "A GtkSettings object to get settings from",
							  GTK_TYPE_SETTINGS,
							  G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
							  G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property (object_class,
                                     PROP_GTK_ICON_THEME,
                                     g_param_spec_object ("gtk-icon-theme",
							  "GtkIconTheme",
							  "A GtkIconTheme object to get icons from",
							  GTK_TYPE_ICON_THEME,
							  G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
							  G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property (object_class,
                                     PROP_FONT_ADJUSTMENT,
                                     g_param_spec_int ("font-adjustment",
                                                       "Font Adjustment",
                                                       "A size adjustment to add to font sizes",
                                                       -3, 10, 0,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property (object_class,
                                     PROP_SHOW_TEXT_CURSOR,
                                     g_param_spec_boolean ("show-text-cursor",
                                                           "Show Text Cursor",
                                                           "Show the text cursor or caret for accessible navigation",
                                                           FALSE,
                                                           G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                           G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property (object_class,
                                     PROP_EDITOR_MODE,
                                     g_param_spec_boolean ("editor-mode",
                                                           "Editor Mode",
                                                           "Enable features useful to editors",
                                                           FALSE,
                                                           G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                                           G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    settings_signals[COLORS_CHANGED] =
	g_signal_new ("colors-changed",
		      G_OBJECT_CLASS_TYPE (klass),
		      G_SIGNAL_RUN_LAST,
		      0, NULL, NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE, 0);

    settings_signals[FONTS_CHANGED] =
	g_signal_new ("fonts-changed",
		      G_OBJECT_CLASS_TYPE (klass),
		      G_SIGNAL_RUN_LAST,
		      0, NULL, NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE, 0);
}

static void
yelp_settings_init (YelpSettings *settings)
{
    gint i;

    settings->priv = yelp_settings_get_instance_private (settings);
    g_mutex_init (&settings->priv->mutex);

    for (i = 0; i < YELP_SETTINGS_NUM_FONTS; i++) {
	settings->priv->setfonts[i] = NULL;
	settings->priv->fonts[i] = NULL;
    }

    settings->priv->tokens = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                    g_free, NULL);
}

static void
yelp_settings_constructed (GObject *object)
{
    YelpSettings *settings = YELP_SETTINGS (object);
    gboolean skip_dbus_checks = FALSE;
    gchar *os_release = NULL;
    const gchar *desktop;

    yelp_settings_set_if_token (settings, "action:install");

    g_file_get_contents ("/etc/os-release", &os_release, NULL, NULL);
    if (os_release == NULL)
        g_file_get_contents ("/usr/lib/os-release", &os_release, NULL, NULL);

    if (os_release != NULL) {
        gint i;
        gchar **lines = g_strsplit(os_release, "\n", -1);
        gchar *osid = NULL, *osversion = NULL, *end;
        g_free (os_release);

        for (i = 0; lines[i] != NULL; i++) {
            if (g_str_has_prefix (lines[i], "ID=")) {
                if (lines[i][3] == '"') {
                    end = strchr (lines[i] + 4, '"');
                    if (end != NULL)
                        osid = g_strndup (lines[i] + 4, end - lines[i] - 4);
                }
                else if (lines[i][3] == '\'') {
                    end = strchr (lines[i] + 4, '\'');
                    if (end != NULL)
                        osid = g_strndup (lines[i] + 4, end - lines[i] - 4);
                }
                else {
                    osid = g_strdup (lines[i] + 3);
                }
            }
            else if (g_str_has_prefix (lines[i], "VERSION_ID=")) {
                if (lines[i][11] == '"') {
                    end = strchr (lines[i] + 12, '"');
                    if (end != NULL)
                        osversion = g_strndup (lines[i] + 12, end - lines[i] - 12);
                }
                else if (lines[i][11] == '\'') {
                    end = strchr (lines[i] + 12, '\'');
                    if (end != NULL)
                        osversion = g_strndup (lines[i] + 12, end - lines[i] - 12);
                }
                else {
                    osversion = g_strdup (lines[i] + 11);
                }
            }
        }

        if (osid) {
            gchar *token = g_strconcat("platform:", osid, NULL);
            yelp_settings_set_if_token (settings, token);
            g_free (token);
            if (osversion) {
                token = g_strconcat("platform:", osid, "-", osversion, NULL);
                yelp_settings_set_if_token (settings, token);
                g_free (token);
                g_free (osversion);
            }
            g_free (osid);
        }

        g_strfreev(lines);
    }

    desktop = g_getenv ("XDG_CURRENT_DESKTOP");
    if (desktop != NULL) {
        gchar **desktops = g_strsplit (desktop, ":", -1);
        gint i;
        gboolean xdg_gnome = FALSE, xdg_gnome_classic = FALSE;
        for (i = 0; desktops[i]; i++) {
            if (!g_ascii_strcasecmp (desktops[i], "gnome")) {
                xdg_gnome = TRUE;
            }
            else if (!g_ascii_strcasecmp (desktops[i], "gnome-classic")) {
                xdg_gnome_classic = TRUE;
            }
            else if (!g_ascii_strcasecmp (desktops[i], "kde")) {
                yelp_settings_set_if_token (settings, "platform:kde");
                skip_dbus_checks = TRUE;
                break;
            }
            else if (!g_ascii_strcasecmp (desktops[i], "mate")) {
                yelp_settings_set_if_token (settings, "platform:mate");
                yelp_settings_set_if_token (settings, "platform:gnome-panel");
                skip_dbus_checks = TRUE;
                break;
            }
            else if (!g_ascii_strcasecmp (desktops[i], "pantheon")) {
                yelp_settings_set_if_token (settings, "platform:pantheon");
                yelp_settings_set_if_token (settings, "platform:gnome-shell");
                skip_dbus_checks = TRUE;
                break;
            }
            else if (!g_ascii_strcasecmp (desktops[i], "unity")) {
                yelp_settings_set_if_token (settings, "platform:unity");
                skip_dbus_checks = TRUE;
                break;
            }
            else if (!g_ascii_strcasecmp (desktops[i], "x-cinnamon")) {
                yelp_settings_set_if_token (settings, "platform:cinnamon");
                yelp_settings_set_if_token (settings, "platform:gnome-shell");
                skip_dbus_checks = TRUE;
                break;
            }
            else if (!g_ascii_strcasecmp (desktops[i], "xfce")) {
                yelp_settings_set_if_token (settings, "platform:xfce");
                skip_dbus_checks = TRUE;
                break;
            }
        }
        if (xdg_gnome) {
            yelp_settings_set_if_token (settings, "platform:gnome-shell");
            if (!xdg_gnome_classic)
                yelp_settings_set_if_token (settings, "platform:gnome-3");
            skip_dbus_checks = TRUE;
        }
        if (xdg_gnome_classic) {
            yelp_settings_set_if_token (settings, "platform:gnome-classic");
            yelp_settings_set_if_token (settings, "platform:gnome-shell");
            skip_dbus_checks = TRUE;
        }
        g_strfreev (desktops);
    }

    if (!skip_dbus_checks) {
        GDBusConnection *connection;
        GVariant *ret, *names;
        GVariantIter iter;
        gchar *name;
        gboolean env_shell, env_classic, env_panel, env_unity, env_xfce;
        GError *error = NULL;

        connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
        if (connection == NULL) {
            g_warning ("Unable to connect to dbus: %s", error->message);
            g_error_free (error);
            return;
        }

        ret = g_dbus_connection_call_sync (connection,
                                           "org.freedesktop.DBus",
                                           "/org/freedesktop/DBus",
                                           "org.freedesktop.DBus",
                                           "ListNames",
                                           NULL,
                                           G_VARIANT_TYPE ("(as)"),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1, NULL, &error);
        if (ret == NULL) {
            g_warning ("Unable to query dbus: %s", error->message);
            g_error_free (error);
            return;
        }
        env_shell = env_classic = env_panel = env_unity = env_xfce = FALSE;
        names = g_variant_get_child_value (ret, 0);
        g_variant_iter_init (&iter, names);
        while (g_variant_iter_loop (&iter, "&s", &name)) {
            if (g_str_equal (name, "org.gnome.Panel"))
                env_panel = TRUE;
            else if (g_str_equal (name, "org.gnome.Shell"))
                env_shell = TRUE;
            else if (g_str_equal (name, "com.canonical.Unity"))
                env_unity = TRUE;
            else if (g_str_equal (name, "org.xfce.Panel"))
                env_xfce = TRUE;
        }
        g_variant_unref (names);
        g_variant_unref (ret);
        if (env_shell) {
            ret = g_dbus_connection_call_sync (connection,
                                                   "org.gnome.Shell",
                                                   "/org/gnome/Shell",
                                                   "org.freedesktop.DBus.Properties",
                                                   "Get",
                                                   g_variant_new ("(ss)",
                                                                  "org.gnome.Shell",
                                                                  "Mode"),
                                                   G_VARIANT_TYPE ("(v)"),
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1, NULL, &error);
            if (ret == NULL) {
                g_warning ("Failed to get GNOME shell mode: %s", error->message);
                g_error_free (error);
            } else {
                GVariant *v;
                g_variant_get (ret, "(v)", &v);
                if (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING) &&
                    g_str_equal (g_variant_get_string (v, NULL), "classic")) {
                    env_classic = TRUE;
                }
                g_variant_unref (v);
                g_variant_unref (ret);
            }
        }

        if (env_classic)
            yelp_settings_set_if_token (settings, "platform:gnome-classic");

        /* order is important:
           gnome-shell also provides org.gnome.Panel
           unity also provides org.gnome.Shell
         */
        if (env_unity)
            yelp_settings_set_if_token (settings, "platform:unity");
        else if (env_shell)
            yelp_settings_set_if_token (settings, "platform:gnome-shell");
        else if (env_xfce)
            yelp_settings_set_if_token (settings, "platform:xfce");
        else if (env_panel)
            yelp_settings_set_if_token (settings, "platform:gnome-panel");
    }
}

static void
yelp_settings_finalize (GObject *object)
{
    YelpSettings *settings = YELP_SETTINGS (object);

    g_mutex_clear (&settings->priv->mutex);

    g_hash_table_destroy (settings->priv->tokens);

    G_OBJECT_CLASS (yelp_settings_parent_class)->finalize (object);
}

static void
yelp_settings_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
    YelpSettings *settings = YELP_SETTINGS (object);

    switch (prop_id) {
    case PROP_GTK_SETTINGS:
	g_value_set_object (value, settings->priv->gtk_settings);
	break;
    case PROP_GTK_ICON_THEME:
	g_value_set_object (value, settings->priv->gtk_icon_theme);
	break;
    case PROP_FONT_ADJUSTMENT:
        g_value_set_int (value, settings->priv->font_adjustment);
        break;
    case PROP_SHOW_TEXT_CURSOR:
        g_value_set_boolean (value, settings->priv->show_text_cursor);
        break;
    case PROP_EDITOR_MODE:
        g_value_set_boolean (value, settings->priv->editor_mode);
        break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	break;
    }
}

static void
yelp_settings_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
    YelpSettings *settings = YELP_SETTINGS (object);

    switch (prop_id) {
    case PROP_GTK_SETTINGS:
	if (settings->priv->gtk_settings) {
	    g_signal_handler_disconnect (settings->priv->gtk_settings,
					 settings->priv->gtk_theme_changed);
	    g_signal_handler_disconnect (settings->priv->gtk_settings,
					 settings->priv->gtk_font_changed);
	    g_object_unref (settings->priv->gtk_settings);
	}
	settings->priv->gtk_settings = g_value_get_object (value);
	if (settings->priv->gtk_settings != NULL) {
	    g_object_ref (settings->priv->gtk_settings);
	    settings->priv->gtk_theme_changed =
		g_signal_connect (settings->priv->gtk_settings,
				  "notify::gtk-theme-name",
				  (GCallback) gtk_theme_changed,
				  settings);
	    settings->priv->gtk_font_changed =
		g_signal_connect (settings->priv->gtk_settings,
				  "notify::gtk-font-name",
				  (GCallback) gtk_font_changed,
				  settings);
	    gtk_theme_changed (settings->priv->gtk_settings, NULL, settings);
	    gtk_font_changed (settings->priv->gtk_settings, NULL, settings);
	}
	else {
	    settings->priv->gtk_theme_changed = 0;
	    settings->priv->gtk_font_changed = 0;
	}
	break;
    case PROP_GTK_ICON_THEME:
	settings->priv->gtk_icon_theme = g_value_get_object (value);
	if (settings->priv->gtk_icon_theme != NULL) {
	    gchar **search_path;
	    gint search_path_len, i;
	    gboolean append_search_path = TRUE;
	    gtk_icon_theme_get_search_path (settings->priv->gtk_icon_theme,
					    &search_path, &search_path_len);
	    for (i = search_path_len - 1; i >= 0; i--)
		if (g_str_equal (search_path[i], YELP_ICON_PATH)) {
		    append_search_path = FALSE;
		    break;
		}
	    if (append_search_path)
		gtk_icon_theme_append_search_path (settings->priv->gtk_icon_theme,
						   YELP_ICON_PATH);
            append_search_path = TRUE;
	    for (i = search_path_len - 1; i >= 0; i--)
		if (g_str_equal (search_path[i], DATADIR"/yelp/icons")) {
		    append_search_path = FALSE;
		    break;
		}
	    if (append_search_path)
		gtk_icon_theme_append_search_path (settings->priv->gtk_icon_theme,
                                                   DATADIR"/yelp/icons");
	    g_strfreev (search_path);
	    g_object_ref (settings->priv->gtk_icon_theme);
	}
	break;
    case PROP_FONT_ADJUSTMENT:
        settings->priv->font_adjustment = g_value_get_int (value);
        gtk_font_changed (settings->priv->gtk_settings, NULL, settings);
        break;
    case PROP_SHOW_TEXT_CURSOR:
        settings->priv->show_text_cursor = g_value_get_boolean (value);
        break;
    case PROP_EDITOR_MODE:
        settings->priv->editor_mode = g_value_get_boolean (value);
        break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	break;
    }
}

/******************************************************************************/

YelpSettings *
yelp_settings_get_default (void)
{
    static GMutex mutex;
    static YelpSettings *settings = NULL;
    g_mutex_lock (&mutex);
    if (settings == NULL)
	settings = g_object_new (YELP_TYPE_SETTINGS,
				 "gtk-settings", gtk_settings_get_default (),
				 "gtk-icon-theme", gtk_icon_theme_get_default (),
				 NULL);
    g_mutex_unlock (&mutex);
    return settings;
}

/******************************************************************************/

gchar *
yelp_settings_get_color (YelpSettings       *settings,
			 YelpSettingsColor   color)
{
    gchar *colorstr;
    g_return_val_if_fail (color < YELP_SETTINGS_NUM_COLORS, NULL);

    g_mutex_lock (&settings->priv->mutex);
    colorstr = g_strdup (settings->priv->colors[color]);
    g_mutex_unlock (&settings->priv->mutex);

    return colorstr;
}

gchar **
yelp_settings_get_colors (YelpSettings *settings)
{
    gchar **colors = g_new0 (gchar *, YELP_SETTINGS_NUM_COLORS + 1);
    gint i;
    for (i = 0; i < YELP_SETTINGS_NUM_COLORS; i++)
	colors[i] = yelp_settings_get_color (settings, i);
    return colors;
}

void
yelp_settings_set_colors (YelpSettings      *settings,
			  YelpSettingsColor  first_color,
			  ...)
{
    YelpSettingsColor color;
    va_list args;

    g_mutex_lock (&settings->priv->mutex);
    va_start (args, first_color);

    color = first_color;
    while ((gint) color >= 0) {
	gchar *colorstr = va_arg (args, gchar *);
	gint i;
	for (i = 0; i < 7; i++) {
	    settings->priv->colors[color][i] = colorstr[i];
	    if (colorstr[i] == '\0')
		break;
	}
	settings->priv->colors[color][7] = '\0';
	color = va_arg (args, YelpSettingsColor);
    }

    va_end (args);
    g_mutex_unlock (&settings->priv->mutex);

    g_signal_emit (settings, settings_signals[COLORS_CHANGED], 0);
}

const gchar*
yelp_settings_get_color_param (YelpSettingsColor color)
{
    static const gchar *params[YELP_SETTINGS_NUM_COLORS] = {
        "color.bg",
        "color.fg"
    };
    g_return_val_if_fail (color < YELP_SETTINGS_NUM_COLORS, NULL);
    return params[color];
}

/******************************************************************************/

gchar *
yelp_settings_get_font (YelpSettings     *settings,
			YelpSettingsFont  font)
{
    gchar *ret;
    g_return_val_if_fail (font < YELP_SETTINGS_NUM_FONTS, NULL);

    g_mutex_lock (&settings->priv->mutex);
    if (settings->priv->setfonts[font])
	ret = g_strdup (settings->priv->setfonts[font]);
    else
	ret = g_strdup (settings->priv->fonts[font]);
    g_mutex_unlock (&settings->priv->mutex);

    return ret;
}

gchar *
yelp_settings_get_font_family (YelpSettings     *settings,
			       YelpSettingsFont  font)
{
    const gchar *def = (font == YELP_SETTINGS_FONT_VARIABLE) ? "Sans" : "Monospace";
    gchar *desc, *ret, *c; /* do not free */
    g_return_val_if_fail (font < YELP_SETTINGS_NUM_FONTS, NULL);

    g_mutex_lock (&settings->priv->mutex);

    if (settings->priv->setfonts[font])
	desc = g_strdup (settings->priv->setfonts[font]);
    else
	desc = g_strdup (settings->priv->fonts[font]);

    if (desc == NULL) {
	ret = g_strdup (def);
	goto done;
    }

    c = strrchr (desc, ' ');
    if (c == NULL) {
	g_warning ("Cannot parse font: %s", desc);
	ret = g_strdup (def);
	goto done;
    }

    ret = g_strndup (desc, c - desc);

 done:
    g_mutex_unlock (&settings->priv->mutex);
    return ret;
}

gint
yelp_settings_get_font_size (YelpSettings     *settings,
			     YelpSettingsFont  font)
{
    gchar *desc, *c; /* do not free */
    gint ret;
    g_return_val_if_fail (font < YELP_SETTINGS_NUM_FONTS, 0);

    g_mutex_lock (&settings->priv->mutex);

    if (settings->priv->setfonts[font])
	desc = g_strdup (settings->priv->setfonts[font]);
    else
	desc = g_strdup (settings->priv->fonts[font]);

    if (desc == NULL) {
	ret = 10;
	goto done;
    }

    c = strrchr (desc, ' ');
    if (c == NULL) {
	g_warning ("Cannot parse font %s", desc);
	ret = 10;
	goto done;
    }

    ret = g_ascii_strtod (c, NULL);

 done:
    g_mutex_unlock (&settings->priv->mutex);
    ret += settings->priv->font_adjustment;
    ret = (ret < 5) ? 5 : ret;
    return ret;
}

void
yelp_settings_set_fonts (YelpSettings     *settings,
			 YelpSettingsFont  first_font,
			 ...)
{
    YelpSettingsFont font;
    va_list args;

    g_mutex_lock (&settings->priv->mutex);
    va_start (args, first_font);

    font = first_font;
    while ((gint) font >= 0) {
	gchar *fontname = va_arg (args, gchar *);
	if (settings->priv->setfonts[font] != NULL)
	    g_free (settings->priv->setfonts[font]);
	settings->priv->setfonts[font] = g_strdup (fontname);
	font = va_arg (args, YelpSettingsFont);
    }

    va_end (args);
    g_mutex_unlock (&settings->priv->mutex);

    g_signal_emit (settings, settings_signals[FONTS_CHANGED], 0);
}

gint
yelp_settings_get_font_adjustment (YelpSettings *settings)
{
    return settings->priv->font_adjustment;
}

void
yelp_settings_set_font_adjustment (YelpSettings *settings,
                                   gint          adjustment)
{
    g_object_set (settings, "font-adjustment", adjustment, NULL);
}

/******************************************************************************/

gboolean
yelp_settings_get_show_text_cursor (YelpSettings *settings)
{
    return settings->priv->show_text_cursor;
}

void
yelp_settings_set_show_text_cursor (YelpSettings *settings,
                                    gboolean      show)
{
    g_object_set (settings, "show-text-cursor", show, NULL);
}

gboolean
yelp_settings_get_editor_mode (YelpSettings *settings)
{
    return settings->priv->editor_mode;
}

void
yelp_settings_set_editor_mode (YelpSettings *settings,
                               gboolean      editor_mode)
{
    g_object_set (settings, "editor-mode", editor_mode, NULL);
}

/******************************************************************************/

static void
yelp_settings_set_if_token (YelpSettings *settings,
                            const gchar  *token)
{
    if (g_hash_table_lookup (settings->priv->tokens, token) == NULL) {
        gchar *ins = g_strdup (token);
        g_hash_table_insert (settings->priv->tokens, ins, ins);
    }
}

/******************************************************************************/

gchar **
yelp_settings_get_all_params (YelpSettings *settings,
			      gint          extra,
			      gint         *end)
{
    gchar **params;
    gint i, ix;
    GString *malstr, *dbstr;
    GList *envs, *envi;
    GRand *rand;

    params = g_new0 (gchar *,
                     (2*YELP_SETTINGS_NUM_COLORS) + extra + 9);

    for (i = 0; i < YELP_SETTINGS_NUM_COLORS; i++) {
        gchar *val;
        ix = 2 * i;
        params[ix] = g_strdup (yelp_settings_get_color_param (i));
        val = yelp_settings_get_color (settings, i);
        params[ix + 1] = g_strdup_printf ("\"%s\"", val);
        g_free (val);
    }
    ix = 2 * YELP_SETTINGS_NUM_COLORS;
    params[ix++] = g_strdup ("yelp.editor_mode");
    if (settings->priv->editor_mode)
        params[ix++] = g_strdup ("true()");
    else
        params[ix++] = g_strdup ("false()");

    malstr = g_string_new ("'");
    dbstr = g_string_new ("'");
    envs = g_hash_table_get_keys (settings->priv->tokens);
    for (envi = envs; envi != NULL; envi = envi->next) {
        g_string_append_c (malstr, ' ');
        g_string_append (malstr, (gchar *) envi->data);
        if (g_str_has_prefix ((gchar *) envi->data, "platform:")) {
            g_string_append_c (dbstr, ';');
            g_string_append (dbstr, (gchar *) (envi->data) + 9);
        }
    }
    g_string_append_c (malstr, '\'');
    g_string_append_c (dbstr, '\'');
    g_list_free (envs);
    params[ix++] = g_strdup ("mal.if.custom");
    params[ix++] = g_string_free (malstr, FALSE);
    params[ix++] = g_strdup ("db.profile.os");
    params[ix++] = g_string_free (dbstr, FALSE);

    rand = g_rand_new ();
    params[ix++] = g_strdup ("html.csp.nonce");
    params[ix++] = g_strdup_printf("'%08x%08x'", g_rand_int (rand), g_rand_int (rand));
    g_rand_free (rand);

    params[ix] = NULL;

    if (end != NULL)
	*end = ix;
    return params;
}

/******************************************************************************/

static void
gtk_theme_changed (GtkSettings  *gtk_settings,
		   GParamSpec   *pspec,
		   YelpSettings *settings)
{
    GtkStyleContext *context;
    GdkRGBA base, text;

    g_mutex_lock (&settings->priv->mutex);

    context = gtk_style_context_new ();

    if ( gtk_style_context_lookup_color(context, "theme_base_color", &base) &&
         gtk_style_context_lookup_color(context, "theme_text_color", &text) ) {
        g_snprintf (settings->priv->colors[YELP_SETTINGS_COLOR_BASE], 8, "#%02X%02X%02X",
                    (guint) (base.red * 255), (guint) (base.green * 255), (guint) (base.blue * 255));
        g_snprintf (settings->priv->colors[YELP_SETTINGS_COLOR_TEXT], 8, "#%02X%02X%02X",
                    (guint) (text.red * 255), (guint) (text.green * 255), (guint) (text.blue * 255));
    }
    else {
        g_snprintf (settings->priv->colors[YELP_SETTINGS_COLOR_BASE], 8, "#FFFFFF");
        g_snprintf (settings->priv->colors[YELP_SETTINGS_COLOR_TEXT], 8, "#000000");
    }

    g_object_unref (context);

    g_mutex_unlock (&settings->priv->mutex);

    g_signal_emit (settings, settings_signals[COLORS_CHANGED], 0);
}

static void
gtk_font_changed (GtkSettings  *gtk_settings,
		  GParamSpec   *pspec,
		  YelpSettings *settings)
{
    gchar *font, *c;

    /* This happens when font_adjustment is set during init */
    if (gtk_settings == NULL)
        return;

    g_free (settings->priv->fonts[YELP_SETTINGS_FONT_VARIABLE]);
    g_object_get (gtk_settings, "gtk-font-name", &font, NULL);
    settings->priv->fonts[YELP_SETTINGS_FONT_VARIABLE] = font;

    c = strrchr (font, ' ');
    if (c == NULL) {
	g_warning ("Cannot parse font: %s", font);
	font = g_strdup ("Monospace 10");
    }
    else {
	font = g_strconcat ("Monospace", c, NULL);
    }

    g_free (settings->priv->fonts[YELP_SETTINGS_FONT_FIXED]);
    settings->priv->fonts[YELP_SETTINGS_FONT_FIXED] = font;

    g_signal_emit (settings, settings_signals[FONTS_CHANGED], 0);
}

gint
yelp_settings_cmp_icons (const gchar *icon1,
                         const gchar *icon2)
{
    static const gchar *icons[] = {
        "yelp-page-search-symbolic",
        "yelp-page-video-symbolic",
        "yelp-page-task-symbolic",
        "yelp-page-tip-symbolic",
        "yelp-page-problem-symbolic",
        "yelp-page-ui-symbolic",
        "yelp-page-symbolic",
        NULL
    };
    gint i;
    for (i = 0; icons[i] != NULL; i++) {
        gboolean eq1 = icon1 ? g_str_has_prefix (icon1, icons[i]) : FALSE;
        gboolean eq2 = icon2 ? g_str_has_prefix (icon2, icons[i]) : FALSE;
        if (eq1 && eq2)
            return 0;
        else if (eq1)
            return -1;
        else if (eq2)
            return 1;
    }
    if (icon1 == NULL && icon2 == NULL)
        return 0;
    else if (icon2 == NULL)
        return -1;
    else if (icon1 == NULL)
        return 1;
    else
        return strcmp (icon1, icon2);
}
