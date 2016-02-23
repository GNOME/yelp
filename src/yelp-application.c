/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2010 Shaun McCance <shaunm@gnome.org>
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

#define G_SETTINGS_ENABLE_BACKEND

#include <gio/gio.h>
#include <gio/gsettingsbackend.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#include <stdlib.h>

#include "yelp-bookmarks.h"
#include "yelp-settings.h"
#include "yelp-view.h"

#include "yelp-application.h"
#include "yelp-window.h"

#define DEFAULT_URI "help:gnome-help"

static gboolean editor_mode = FALSE;

G_GNUC_NORETURN static gboolean
option_version_cb (const gchar *option_name,
	           const gchar *value,
	           gpointer     data,
	           GError     **error)
{
	g_print ("%s %s\n", PACKAGE, VERSION);

	exit (0);
}

static const GOptionEntry entries[] = {
    {"editor-mode", 0, 0, G_OPTION_ARG_NONE, &editor_mode, N_("Turn on editor mode"), NULL},
    { "version", 0, G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_CALLBACK, option_version_cb, NULL, NULL },
    { NULL }
};

typedef struct _YelpApplicationLoad YelpApplicationLoad;
struct _YelpApplicationLoad {
    YelpApplication *app;
    guint32 timestamp;
    gboolean new;
    gboolean fallback_help_list;
};

static void          yelp_application_iface_init       (YelpBookmarksInterface *iface);
static void          yelp_application_dispose          (GObject                *object);
static void          yelp_application_finalize         (GObject                *object);

static gboolean      yelp_application_cmdline          (GApplication          *app,
                                                        gchar               ***arguments,
                                                        gint                  *exit_status);
static void          yelp_application_startup          (GApplication          *app);
static int           yelp_application_command_line     (GApplication          *app,
                                                        GApplicationCommandLine *cmdline);
static void          application_uri_resolved          (YelpUri               *uri,
                                                        YelpApplicationLoad   *data);
static gboolean      application_window_deleted        (YelpWindow            *window,
                                                        GdkEvent              *event,
                                                        YelpApplication       *app);
GSettings *          application_get_doc_settings      (YelpApplication       *app,
                                                        const gchar           *doc_uri);
static void          application_adjust_font           (GAction               *action,
                                                        GVariant              *parameter,
                                                        YelpApplication       *app);
static void          application_set_font_sensitivity  (YelpApplication       *app);

static void          bookmarks_changed                 (GSettings             *settings,
                                                        const gchar           *key,
                                                        YelpApplication       *app);
static gboolean      window_resized                    (YelpWindow            *window,
                                                        YelpApplication       *app);

G_DEFINE_TYPE_WITH_CODE (YelpApplication, yelp_application, GTK_TYPE_APPLICATION,
                         G_IMPLEMENT_INTERFACE (YELP_TYPE_BOOKMARKS,
                                                yelp_application_iface_init))
#define GET_PRIV(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_APPLICATION, YelpApplicationPrivate))

typedef struct _YelpApplicationPrivate YelpApplicationPrivate;
struct _YelpApplicationPrivate {
    GSList *windows;
    GHashTable *windows_by_document;

    GPropertyAction  *show_cursor_action;
    GSimpleAction    *larger_text_action;
    GSimpleAction    *smaller_text_action;

    GSettingsBackend *backend;
    GSettings *gsettings;
    GHashTable *docsettings;
};

static void
yelp_application_init (YelpApplication *app)
{
    YelpApplicationPrivate *priv = GET_PRIV (app);
    priv->docsettings = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               (GDestroyNotify) g_free,
                                               (GDestroyNotify) g_object_unref);

    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           "app.yelp-application-show-cursor",
                                           (const gchar*[]) {"F7", NULL});
    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           "app.yelp-application-larger-text",
                                           (const gchar*[]) {"<Control>plus", NULL});
    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           "app.yelp-application-smaller-text",
                                           (const gchar*[]) {"<Control>minus", NULL});

    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           "win.yelp-window-find", (const gchar*[]) {"<Control>F", NULL});
    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           "win.yelp-window-search", (const gchar*[]) {"<Control>S", NULL});
    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           "win.yelp-window-new", (const gchar*[]) {"<Control>N", NULL});
    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           "win.yelp-window-close", (const gchar*[]) {"<Control>W", NULL});
    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           "win.yelp-window-ctrll", (const gchar*[]) {"<Control>L", NULL});
    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           "win.yelp-view-print", (const gchar*[]) {"<Control>P", NULL});

    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           "win.yelp-view-go-back",
                                           (const gchar*[]) {"<Alt>Left", NULL});
    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           "win.yelp-view-go-forward",
                                           (const gchar*[]) {"<Alt>Right", NULL});
    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           "win.yelp-view-go-previous",
                                           (const gchar*[]) {"<Control>Page_Up", NULL});
    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           "win.yelp-view-go-next",
                                           (const gchar*[]) {"<Control>Page_Down", NULL});
}

static void
yelp_application_class_init (YelpApplicationClass *klass)
{
    GApplicationClass *application_class = G_APPLICATION_CLASS (klass);
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    application_class->local_command_line = yelp_application_cmdline;
    application_class->startup = yelp_application_startup;
    application_class->command_line = yelp_application_command_line;

    object_class->dispose = yelp_application_dispose;
    object_class->finalize = yelp_application_finalize;

    g_type_class_add_private (klass, sizeof (YelpApplicationPrivate));
}

static void
yelp_application_iface_init (YelpBookmarksInterface *iface)
{
    iface->add_bookmark = yelp_application_add_bookmark;
    iface->remove_bookmark = yelp_application_remove_bookmark;
    iface->is_bookmarked = yelp_application_is_bookmarked;
}

static void
yelp_application_dispose (GObject *object)
{
    YelpApplicationPrivate *priv = GET_PRIV (object);

    if (priv->show_cursor_action) {
        g_object_unref (priv->show_cursor_action);
        priv->show_cursor_action = NULL;
    }

    if (priv->larger_text_action) {
        g_object_unref (priv->larger_text_action);
        priv->larger_text_action = NULL;
    }

    if (priv->smaller_text_action) {
        g_object_unref (priv->smaller_text_action);
        priv->smaller_text_action = NULL;
    }

    if (priv->gsettings) {
        g_object_unref (priv->gsettings);
        priv->gsettings = NULL;
    }

    G_OBJECT_CLASS (yelp_application_parent_class)->dispose (object);
}

static void
yelp_application_finalize (GObject *object)
{
    YelpApplicationPrivate *priv = GET_PRIV (object);

    g_hash_table_destroy (priv->windows_by_document);
    g_hash_table_destroy (priv->docsettings);

    G_OBJECT_CLASS (yelp_application_parent_class)->finalize (object);
}


static gboolean
yelp_application_cmdline (GApplication     *app,
                          gchar          ***arguments,
                          gint             *exit_status)
{
    GOptionContext *context;
    gint argc = g_strv_length (*arguments);
    gint i;

    context = g_option_context_new (NULL);
    g_option_context_add_group (context, gtk_get_option_group (FALSE));
    g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
    g_option_context_parse (context, &argc, arguments, NULL);

    for (i = 1; i < argc; i++) {
        if (!strchr ((*arguments)[i], ':') && !((*arguments)[i][0] == '/')) {
            GFile *base, *new;
            gchar *cur, *newuri;
            cur = g_get_current_dir ();
            base = g_file_new_for_path (cur);
            new = g_file_resolve_relative_path (base, (*arguments)[i]);
            newuri = g_file_get_uri (new);
            g_free ((*arguments)[i]);
            (*arguments)[i] = newuri;
            g_free (cur);
            g_object_unref (new);
            g_object_unref (base);
        }
    }

    return G_APPLICATION_CLASS (yelp_application_parent_class)
        ->local_command_line (app, arguments, exit_status);
}

static void
yelp_application_startup (GApplication *application)
{
    YelpApplication *app = YELP_APPLICATION (application);
    YelpApplicationPrivate *priv = GET_PRIV (app);
    GMenu *menu, *section;
    gchar *keyfile;
    YelpSettings *settings;

    g_set_application_name (N_("Help"));

    /* chain up */
    G_APPLICATION_CLASS (yelp_application_parent_class)->startup (application);

    settings = yelp_settings_get_default ();
    if (editor_mode)
        yelp_settings_set_editor_mode (settings, TRUE);
    priv->windows_by_document = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free,
                                                       NULL);
    /* Use a config file for per-document settings, because
       Ryan asked me to. */
    keyfile = g_build_filename (g_get_user_config_dir (), "yelp", "yelp.cfg", NULL);
    priv->backend = g_keyfile_settings_backend_new (keyfile, "/org/gnome/yelp/", "yelp");
    g_free (keyfile);

    /* But the main settings are in dconf */
    priv->gsettings = g_settings_new ("org.gnome.yelp");

    g_settings_bind (priv->gsettings, "show-cursor",
                     settings, "show-text-cursor",
                     G_SETTINGS_BIND_DEFAULT);
    priv->show_cursor_action = g_property_action_new ("yelp-application-show-cursor",
                                                      settings, "show-text-cursor");
    g_action_map_add_action (G_ACTION_MAP (app), G_ACTION (priv->show_cursor_action));

    g_settings_bind (priv->gsettings, "font-adjustment",
                     settings, "font-adjustment",
                     G_SETTINGS_BIND_DEFAULT);

    priv->larger_text_action = g_simple_action_new ("yelp-application-larger-text", NULL);
    g_signal_connect (priv->larger_text_action,
                      "activate",
                      G_CALLBACK (application_adjust_font),
                      app);
    g_action_map_add_action (G_ACTION_MAP (app), G_ACTION (priv->larger_text_action));

    priv->smaller_text_action = g_simple_action_new ("yelp-application-smaller-text", NULL);
    g_signal_connect (priv->smaller_text_action,
                      "activate",
                      G_CALLBACK (application_adjust_font),
                      app);
    g_action_map_add_action (G_ACTION_MAP (app), G_ACTION (priv->smaller_text_action));

    application_set_font_sensitivity (app);

    menu = g_menu_new ();
    section = g_menu_new ();
    g_menu_append (section, _("New Window"), "win.yelp-window-new");
    g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
    g_object_unref (section);
    section = g_menu_new ();
    g_menu_append (section, _("Larger Text"), "app.yelp-application-larger-text");
    g_menu_append (section, _("Smaller Text"), "app.yelp-application-smaller-text");
    g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
    g_object_unref (section);
    gtk_application_set_app_menu (GTK_APPLICATION (application), G_MENU_MODEL (menu));
}

/******************************************************************************/

static void
application_adjust_font (GAction         *action,
                         GVariant        *parameter,
                         YelpApplication *app)
{
    YelpApplicationPrivate *priv = GET_PRIV (app);
    gint adjustment = g_settings_get_int (priv->gsettings, "font-adjustment");
    gint adjust = g_str_equal (g_action_get_name (action), "yelp-application-larger-text") ? 1 : -1;

    adjustment += adjust;
    g_settings_set_int (priv->gsettings, "font-adjustment", adjustment);

    application_set_font_sensitivity (app);
}

static void
application_set_font_sensitivity (YelpApplication *app)
{
    YelpApplicationPrivate *priv = GET_PRIV (app);
    YelpSettings *settings = yelp_settings_get_default ();
    GParamSpec *spec = g_object_class_find_property ((GObjectClass *) YELP_SETTINGS_GET_CLASS (settings),
                                                     "font-adjustment");
    gint adjustment = g_settings_get_int (priv->gsettings, "font-adjustment");
    if (!G_PARAM_SPEC_INT (spec)) {
        g_warning ("Expcected integer param spec for font-adjustment");
        return;
    }
    g_simple_action_set_enabled (priv->larger_text_action, 
                                 adjustment < ((GParamSpecInt *) spec)->maximum);
    g_simple_action_set_enabled (priv->smaller_text_action, 
                                 adjustment > ((GParamSpecInt *) spec)->minimum);
}

/******************************************************************************/

YelpApplication *
yelp_application_new (void)
{
    YelpApplication *app;

    app = g_object_new (YELP_TYPE_APPLICATION,
                        "application-id", "org.gnome.Yelp",
                        "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                        "inactivity-timeout", 5000,
                        NULL);

    return app;
}

/* consumes the uri */
static void
open_uri (YelpApplication *app,
          YelpUri         *uri,
          gboolean         new_window,
          gboolean         fallback_help_list)
{
    YelpApplicationLoad *data;
    data = g_new (YelpApplicationLoad, 1);
    data->app = app;
    data->timestamp = gtk_get_current_event_time ();
    data->new = new_window;
    data->fallback_help_list = fallback_help_list;

    g_signal_connect (uri, "resolved",
                      G_CALLBACK (application_uri_resolved),
                      data);

    /* hold the app while resolving the uri so we don't exit while
     * in the middle of the load
     */
    g_application_hold (G_APPLICATION (app));

    yelp_uri_resolve (uri);
}


static int
yelp_application_command_line (GApplication            *application,
                               GApplicationCommandLine *cmdline)
{
    YelpApplication *app = YELP_APPLICATION (application);
    gchar **argv;
    int i;

    argv = g_application_command_line_get_arguments (cmdline, NULL);

    if (argv[1] == NULL)
        open_uri (app, yelp_uri_new (DEFAULT_URI), FALSE, TRUE);

    for (i = 1; argv[i]; i++)
        open_uri (app, yelp_uri_new (argv[i]), FALSE, FALSE);

    g_strfreev (argv);

    return 0;
}

void
yelp_application_new_window (YelpApplication  *app,
                             const gchar      *uri)
{
    if (uri)
        open_uri (app, yelp_uri_new (uri), TRUE, FALSE);
    else
        open_uri (app, yelp_uri_new (DEFAULT_URI), TRUE, TRUE);
}

void
yelp_application_new_window_uri (YelpApplication  *app,
                                 YelpUri          *uri)
{
    open_uri (app, g_object_ref (uri), TRUE, FALSE);
}

static void
application_uri_resolved (YelpUri             *uri,
                          YelpApplicationLoad *data)
{
    YelpWindow *window;
    gchar *doc_uri;
    GdkWindow *gdk_window;
    YelpApplicationPrivate *priv = GET_PRIV (data->app);
    GFile *gfile;

    /* We held the application while resolving the URI, so unhold now. */
    g_application_release (G_APPLICATION (data->app));

    /* Get the GFile associated with the URI, or NULL if not available */
    gfile = yelp_uri_get_file (uri);
    if (gfile == NULL && data->fallback_help_list) {
        /* There is no file associated to the default uri, so we'll fallback
         * to help-list: if we're told to do so. */
        open_uri (data->app, yelp_uri_new ("help-list:"), data->new, FALSE);
        g_object_unref (uri);
        g_free (data);
        return;
    }
    g_clear_object (&gfile);

    doc_uri = yelp_uri_get_document_uri (uri);

    if (data->new || !doc_uri)
        window = NULL;
    else
        window = g_hash_table_lookup (priv->windows_by_document, doc_uri);

    if (window == NULL) {
        gint width, height;
        GSettings *settings = application_get_doc_settings (data->app, doc_uri);

        g_settings_get (settings, "geometry", "(ii)", &width, &height);
        window = yelp_window_new (data->app);
        gtk_window_set_default_size (GTK_WINDOW (window), width, height);
        g_signal_connect (window, "resized", G_CALLBACK (window_resized), data->app);
        priv->windows = g_slist_prepend (priv->windows, window);

        if (!data->new) {
            g_hash_table_insert (priv->windows_by_document, doc_uri, window);
            g_object_set_data (G_OBJECT (window), "doc_uri", doc_uri);
        }
        else {
            g_free (doc_uri);
        }

        g_signal_connect (window, "delete-event",
                          G_CALLBACK (application_window_deleted), data->app);
        gtk_window_set_application (GTK_WINDOW (window),
                                    GTK_APPLICATION (data->app));
    }
    else {
        g_free (doc_uri);
    }

    yelp_window_load_uri (window, uri);

    gtk_widget_show_all (GTK_WIDGET (window));

    /* Metacity no longer does anything useful with gtk_window_present */
    gdk_window = gtk_widget_get_window (GTK_WIDGET (window));

#ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_WINDOW (gdk_window)){
        if (gdk_window)
            gdk_x11_window_move_to_current_desktop (gdk_window);

        /* Ensure we actually present the window when invoked from the command
         * line. This is somewhat evil, but the minor evil of Yelp stealing
         * focus (after you requested it) is outweighed for me by the major
         * evil of no help window appearing when you click Help.
         */
        if (data->timestamp == 0)
            data->timestamp = gdk_x11_get_server_time (gtk_widget_get_window (GTK_WIDGET (window)));

        gtk_window_present_with_time (GTK_WINDOW (window), data->timestamp);
    }
    else
#endif
        gtk_window_present (GTK_WINDOW (window));

    g_object_unref (uri);
    g_free (data);
}

static gboolean
application_window_deleted (YelpWindow      *window,
                            GdkEvent        *event,
                            YelpApplication *app)
{
    gchar *doc_uri; /* owned by windows_by_document */
    YelpApplicationPrivate *priv = GET_PRIV (app);

    priv->windows = g_slist_remove (priv->windows, window);
    doc_uri = g_object_get_data (G_OBJECT (window), "doc_uri");
    if (doc_uri)
        g_hash_table_remove (priv->windows_by_document, doc_uri);

    return FALSE;
}

GSettings *
application_get_doc_settings (YelpApplication *app, const gchar *doc_uri)
{
    YelpApplicationPrivate *priv = GET_PRIV (app);
    GSettings *settings = g_hash_table_lookup (priv->docsettings, doc_uri);
    if (settings == NULL) {
        gchar *tmp, *key, *settings_path;
        tmp = g_uri_escape_string (doc_uri, "", FALSE);
        settings_path = g_strconcat ("/org/gnome/yelp/documents/", tmp, "/", NULL);
        g_free (tmp);
        if (priv->backend)
            settings = g_settings_new_with_backend_and_path ("org.gnome.yelp.documents",
                                                             priv->backend,
                                                             settings_path);
        else
            settings = g_settings_new_with_path ("org.gnome.yelp.documents",
                                                 settings_path);
        key = g_strdup (doc_uri);
        g_hash_table_insert (priv->docsettings, key, settings);
        g_object_set_data ((GObject *) settings, "doc_uri", key);
        g_signal_connect (settings, "changed::bookmarks",
                          G_CALLBACK (bookmarks_changed), app);
        g_free (settings_path);
    }
    return settings;
}

/******************************************************************************/

void
yelp_application_add_bookmark (YelpBookmarks     *bookmarks,
                               const gchar       *doc_uri,
                               const gchar       *page_id,
                               const gchar       *icon,
                               const gchar       *title)
{
    GSettings *settings;
    YelpApplication *app = YELP_APPLICATION (bookmarks);

    g_return_if_fail (page_id);
    g_return_if_fail (doc_uri);

    settings = application_get_doc_settings (app, doc_uri);

    if (settings) {
        GVariantBuilder builder;
        GVariantIter *iter;
        gchar *this_id, *this_icon, *this_title;
        gboolean broken = FALSE;
        g_settings_get (settings, "bookmarks", "a(sss)", &iter);
        g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sss)"));
        while (g_variant_iter_loop (iter, "(&s&s&s)", &this_id, &this_icon, &this_title)) {
            if (g_str_equal (page_id, this_id)) {
                /* Already have this bookmark */
                broken = TRUE;
                break;
            }
            g_variant_builder_add (&builder, "(sss)", this_id, this_icon, this_title);
        }
        g_variant_iter_free (iter);

        if (!broken) {
            GVariant *value;
            g_variant_builder_add (&builder, "(sss)", page_id, icon, title);
            value = g_variant_builder_end (&builder);
            g_settings_set_value (settings, "bookmarks", value);
        }
    }
}

void
yelp_application_remove_bookmark (YelpBookmarks     *bookmarks,
                                  const gchar       *doc_uri,
                                  const gchar       *page_id)
{
    GSettings *settings;
    YelpApplication *app = YELP_APPLICATION (bookmarks);

    g_return_if_fail (page_id);
    g_return_if_fail (doc_uri);

    settings = application_get_doc_settings (app, doc_uri);

    if (settings) {
        GVariantBuilder builder;
        GVariantIter *iter;
        gchar *this_id, *this_icon, *this_title;
        g_settings_get (settings, "bookmarks", "a(sss)", &iter);
        g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sss)"));
        while (g_variant_iter_loop (iter, "(&s&s&s)", &this_id, &this_icon, &this_title)) {
            if (!g_str_equal (page_id, this_id))
                g_variant_builder_add (&builder, "(sss)", this_id, this_icon, this_title);
        }
        g_variant_iter_free (iter);

        g_settings_set_value (settings, "bookmarks", g_variant_builder_end (&builder));
    }
}

gboolean
yelp_application_is_bookmarked (YelpBookmarks     *bookmarks,
                                const gchar       *doc_uri,
                                const gchar       *page_id)
{
    GVariant *stored_bookmarks;
    GVariantIter *iter;
    gboolean ret = FALSE;
    gchar *this_id = NULL;
    GSettings *settings;
    YelpApplication *app = YELP_APPLICATION (bookmarks);

    g_return_val_if_fail (page_id, FALSE);
    g_return_val_if_fail (doc_uri, FALSE);

    settings = application_get_doc_settings (app, doc_uri);
    if (settings == NULL)
        return FALSE;

    stored_bookmarks = g_settings_get_value (settings, "bookmarks");
    g_settings_get (settings, "bookmarks", "a(sss)", &iter);
    while (g_variant_iter_loop (iter, "(&s&s&s)", &this_id, NULL, NULL)) {
        if (g_str_equal (page_id, this_id)) {
            ret = TRUE;
            break;
        }
    }

    g_variant_iter_free (iter);
    g_variant_unref (stored_bookmarks);
    return ret;
}

void
yelp_application_update_bookmarks (YelpApplication   *app,
                                   const gchar       *doc_uri,
                                   const gchar       *page_id,
                                   const gchar       *icon,
                                   const gchar       *title)
{
    GSettings *settings;

    g_return_if_fail (page_id);
    g_return_if_fail (doc_uri);

    settings = application_get_doc_settings (app, doc_uri);

    if (settings) {
        GVariantBuilder builder;
        GVariantIter *iter;
        gchar *this_id, *this_icon, *this_title;
        gboolean updated = FALSE;
        g_settings_get (settings, "bookmarks", "a(sss)", &iter);
        g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sss)"));
        while (g_variant_iter_loop (iter, "(&s&s&s)", &this_id, &this_icon, &this_title)) {
            if (g_str_equal (page_id, this_id)) {
                if (icon && !g_str_equal (icon, this_icon)) {
                    this_icon = (gchar *) icon;
                    updated = TRUE;
                }
                if (title && !g_str_equal (title, this_title)) {
                    this_title = (gchar *) title;
                    updated = TRUE;
                }
                if (!updated)
                    break;
            }
            g_variant_builder_add (&builder, "(sss)", this_id, this_icon, this_title);
        }
        g_variant_iter_free (iter);

        if (updated)
            g_settings_set_value (settings, "bookmarks",
                                  g_variant_builder_end (&builder));
        else
            g_variant_builder_clear (&builder);
    }
}

GVariant *
yelp_application_get_bookmarks (YelpApplication *app,
                                const gchar     *doc_uri)
{
    GSettings *settings = application_get_doc_settings (app, doc_uri);

    return g_settings_get_value (settings, "bookmarks");
}

static void
bookmarks_changed (GSettings       *settings,
                   const gchar     *key,
                   YelpApplication *app)
{
    const gchar *doc_uri = g_object_get_data ((GObject *) settings, "doc_uri");
    if (doc_uri)
        g_signal_emit_by_name (app, "bookmarks-changed", doc_uri);
}

static gboolean
window_resized (YelpWindow        *window,
                YelpApplication   *app)
{
    YelpApplicationPrivate *priv = GET_PRIV (app);
    YelpUri *uri;
    gchar *doc_uri;
    GSettings *settings;

    uri = yelp_window_get_uri (window);
    if (uri == NULL)
        return FALSE;
    doc_uri = yelp_uri_get_document_uri (uri);
    if (doc_uri == NULL) {
        g_object_unref (uri);
        return FALSE;
    }
    settings = g_hash_table_lookup (priv->docsettings, doc_uri);

    if (settings) {
        gint width, height;
        yelp_window_get_geometry (window, &width, &height);
        g_settings_set (settings, "geometry", "(ii)", width, height);
    }

    g_free (doc_uri);
    g_object_unref (uri);

    return FALSE;
}
