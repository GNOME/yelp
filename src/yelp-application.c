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
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
#include <gdk/gdkx.h>

#include "yelp-settings.h"
#include "yelp-view.h"

#include "yelp-application.h"
#include "yelp-window.h"

#define DEFAULT_URI "ghelp:user-guide"

static gboolean editor_mode = FALSE;

enum {
    BOOKMARKS_CHANGED,
    READ_LATER_CHANGED,
    LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };

static const GOptionEntry entries[] = {
    {"editor-mode", 0, 0, G_OPTION_ARG_NONE, &editor_mode, N_("Turn on editor mode"), NULL},
    { NULL }
};

typedef struct _YelpApplicationLoad YelpApplicationLoad;
struct _YelpApplicationLoad {
    YelpApplication *app;
    guint32 timestamp;
    gboolean new;
};

static const gchar introspection_xml[] =
    "<node name='/org/gnome/Yelp'>"
    "  <interface name='org.gnome.Yelp'>"
    "    <annotation name='org.freedesktop.DBus.GLib.CSymbol' value='yelp_application'/>"
    "    <method name='LoadUri'>"
    "      <arg type='s' name='Uri' direction='in'/>"
    "      <arg type='u' name='Timestamp' direction='in'/>"
    "    </method>"
    "  </interface>"
    "</node>";
GDBusNodeInfo *introspection_data;

static void          yelp_application_init             (YelpApplication       *app);
static void          yelp_application_class_init       (YelpApplicationClass  *klass);
static void          yelp_application_dispose          (GObject               *object);
static void          yelp_application_finalize         (GObject               *object);

static void          yelp_application_method           (GDBusConnection       *connection,
                                                        const gchar           *sender,
                                                        const gchar           *object_path,
                                                        const gchar           *interface_name,
                                                        const gchar           *method_name,
                                                        GVariant              *parameters,
                                                        GDBusMethodInvocation *invocation,
                                                        YelpApplication       *app);

static void          application_setup                 (YelpApplication       *app);
static void          application_uri_resolved          (YelpUri               *uri,
                                                        YelpApplicationLoad   *data);
static gboolean      application_window_deleted        (YelpWindow            *window,
                                                        GdkEvent              *event,
                                                        YelpApplication       *app);
GSettings *          application_get_doc_settings      (YelpApplication       *app,
                                                        const gchar           *doc_uri);
static gboolean      application_maybe_quit            (YelpApplication       *app);
static void          application_adjust_font           (GtkAction             *action,
                                                        YelpApplication       *app);
static void          application_set_font_sensitivity  (YelpApplication       *app);

static gboolean      window_resized                    (YelpWindow            *window,
                                                        YelpApplication       *app);

G_DEFINE_TYPE (YelpApplication, yelp_application, G_TYPE_OBJECT);
#define GET_PRIV(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_APPLICATION, YelpApplicationPrivate))

GDBusInterfaceVTable yelp_dbus_vtable = {
    (GDBusInterfaceMethodCallFunc) yelp_application_method,
    NULL, NULL };

typedef struct _YelpApplicationPrivate YelpApplicationPrivate;
struct _YelpApplicationPrivate {
    GDBusConnection *connection;

    GSList *windows;
    GHashTable *windows_by_document;

    GtkActionGroup *action_group;

    GSettingsBackend *backend;
    GSettings *gsettings;
    GHashTable *docsettings;
};

static const GtkActionEntry action_entries[] = {
    { "LargerText", GTK_STOCK_ZOOM_IN,
      N_("_Larger Text"),
      "<Control>plus",
      N_("Increase the size of the text"),
      G_CALLBACK (application_adjust_font) },
    { "SmallerText", GTK_STOCK_ZOOM_OUT,
      N_("_Smaller Text"),
      "<Control>minus",
      N_("Descrease the size of the text"),
      G_CALLBACK (application_adjust_font) }
};

static void
yelp_application_init (YelpApplication *app)
{
    YelpApplicationPrivate *priv = GET_PRIV (app);
    priv->docsettings = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               (GDestroyNotify) g_free,
                                               (GDestroyNotify) g_object_unref);
}

static void
yelp_application_class_init (YelpApplicationClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = yelp_application_dispose;
    object_class->finalize = yelp_application_finalize;

    signals[BOOKMARKS_CHANGED] =
        g_signal_new ("bookmarks-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE, 1, G_TYPE_STRING);

    signals[READ_LATER_CHANGED] =
        g_signal_new ("read-later-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE, 1, G_TYPE_STRING);

    g_type_class_add_private (klass, sizeof (YelpApplicationPrivate));
}

static void
yelp_application_dispose (GObject *object)
{
    YelpApplicationPrivate *priv = GET_PRIV (object);

    if (priv->connection) {
        g_object_unref (priv->connection);
        priv->connection = NULL;
    }

    if (priv->action_group) {
        g_object_unref (priv->action_group);
        priv->action_group = NULL;
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


static void
application_setup (YelpApplication *app)
{
    YelpApplicationPrivate *priv = GET_PRIV (app);
    YelpSettings *settings = yelp_settings_get_default ();
    GtkAction *action;

    priv->windows_by_document = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free,
                                                       NULL);
    /* FIXME: is there a better way to see if there's a real backend in use? */
    if (g_getenv ("GSETTINGS_BACKEND") == NULL) {
        gchar *keyfile = g_build_filename (g_get_user_config_dir (),
                                           "yelp", "yelp.cfg",
                                           NULL);
        priv->backend = g_keyfile_settings_backend_new (keyfile, "/apps/yelp/", "yelp");
        priv->gsettings = g_settings_new_with_backend ("org.gnome.yelp",
                                                       priv->backend);
        g_free (keyfile);
    }
    else {
        priv->gsettings = g_settings_new ("org.gnome.yelp");
    }
    g_settings_bind (priv->gsettings, "show-cursor",
                     settings, "show-text-cursor",
                     G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (priv->gsettings, "font-adjustment",
                     settings, "font-adjustment",
                     G_SETTINGS_BIND_DEFAULT);

    priv->action_group = gtk_action_group_new ("ApplicationActions");
    gtk_action_group_set_translation_domain (priv->action_group, GETTEXT_PACKAGE);
    gtk_action_group_add_actions (priv->action_group,
				  action_entries, G_N_ELEMENTS (action_entries),
				  app);
    action = (GtkAction *) gtk_toggle_action_new ("ShowTextCursor",
                                                  _("Show Text _Cursor"),
                                                  NULL, NULL);
    gtk_action_group_add_action_with_accel (priv->action_group,
                                            action, "F7");
    g_settings_bind (priv->gsettings, "show-cursor",
                     action, "active",
                     G_SETTINGS_BIND_DEFAULT);
    g_object_unref (action);
    application_set_font_sensitivity (app);
}

/******************************************************************************/

GtkActionGroup *
yelp_application_get_action_group (YelpApplication  *app)
{
    YelpApplicationPrivate *priv = GET_PRIV (app);
    return priv->action_group;
}

static void
application_adjust_font (GtkAction       *action,
                         YelpApplication *app)
{
    YelpApplicationPrivate *priv = GET_PRIV (app);
    gint adjustment = g_settings_get_int (priv->gsettings, "font-adjustment");
    gint adjust = g_str_equal (gtk_action_get_name (action), "LargerText") ? 1 : -1;

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
    gtk_action_set_sensitive (gtk_action_group_get_action (priv->action_group, "LargerText"),
                              adjustment < ((GParamSpecInt *) spec)->maximum);
    gtk_action_set_sensitive (gtk_action_group_get_action (priv->action_group, "SmallerText"),
                              adjustment > ((GParamSpecInt *) spec)->minimum);
}

/******************************************************************************/

YelpApplication *
yelp_application_new (void)
{
    YelpApplication *app;

    app = (YelpApplication *) g_object_new (YELP_TYPE_APPLICATION, NULL);

    return app;
}

gint
yelp_application_run (YelpApplication  *app,
                      gint              argc,
                      gchar           **argv)
{
    GOptionContext *context;
    GError *error = NULL;
    GVariant *ret;
    guint32 request;
    YelpApplicationPrivate *priv = GET_PRIV (app);
    gchar *uri;

    g_set_application_name (N_("Help"));

    context = g_option_context_new (NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));
    g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
    g_option_context_parse (context, &argc, &argv, NULL);

    priv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
    if (priv->connection == NULL) {
        g_warning ("Unable to connect to dbus: %s", error->message);
        g_error_free (error);
        return 1;
    }

    if (argc > 1)
        uri = argv[1];
    else
        uri = DEFAULT_URI;

    ret = g_dbus_connection_call_sync (priv->connection,
                                       "org.freedesktop.DBus",
                                       "/org/freedesktop/DBus",
                                       "org.freedesktop.DBus",
                                       "RequestName",
                                       g_variant_new ("(su)", "org.gnome.Yelp", 0),
                                       NULL,
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1, NULL, &error);
    if (error) {
        g_warning ("Unable to register service: %s", error->message);
        g_error_free (error);
        g_variant_unref (ret);
        return 1;
    }

    g_variant_get (ret, "(u)", &request);
    g_variant_unref (ret);

    if (request == 2 || request == 3) { /* IN_QUEUE | EXISTS */
        gchar *newuri;

        if (uri && (strchr (uri, ':') || (uri[0] == '/')))
            newuri = uri;
        else {
            GFile *base, *new;
            gchar *cur = g_get_current_dir ();
            base = g_file_new_for_path (cur);
            new = g_file_resolve_relative_path (base, uri);
            newuri = g_file_get_uri (new);
            g_free (cur);
        }

        ret = g_dbus_connection_call_sync (priv->connection,
                                           "org.gnome.Yelp",
                                           "/org/gnome/Yelp",
                                           "org.gnome.Yelp",
                                           "LoadUri",
                                           g_variant_new ("(su)", newuri, gtk_get_current_event_time ()),
                                           NULL,
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1, NULL, &error);
        if (error) {
            g_warning ("Unable to notify existing process: %s\n", error->message);
            g_error_free (error);
        }

        if (newuri != uri)
            g_free (newuri);
        g_variant_unref (ret);
        return 1;
    }

    introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
    g_dbus_connection_register_object (priv->connection,
                                       "/org/gnome/Yelp",
                                       introspection_data->interfaces[0],
                                       &yelp_dbus_vtable,
                                       app, NULL, NULL);

    application_setup (app);

    yelp_settings_set_editor_mode (yelp_settings_get_default (), editor_mode);

    yelp_application_load_uri (app, uri, gtk_get_current_event_time (), NULL);

    gtk_main ();

    return 0;
}

static void
yelp_application_method (GDBusConnection *connection,
                         const gchar *sender,
                         const gchar *object_path,
                         const gchar *interface_name,
                         const gchar *method_name,
                         GVariant *parameters,
                         GDBusMethodInvocation *invocation,
                         YelpApplication *app)
{
    if (g_str_equal (interface_name, "org.gnome.Yelp")) {
        if (g_str_equal (method_name, "LoadUri")) {
            GError *error = NULL;
            gchar *uri;
            guint32 timestamp;
            g_variant_get (parameters, "(&su)", &uri, &timestamp);
            yelp_application_load_uri (app, uri, timestamp, &error);
            if (error) {
                g_dbus_method_invocation_return_error (invocation,
                                                       error->domain,
                                                       error->code,
                                                       "%s", error->message);
                g_error_free (error);
            }
            else
                g_dbus_method_invocation_return_value (invocation, NULL);
            return;
        }
    }
    g_dbus_method_invocation_return_error (invocation,
                                           G_IO_ERROR,
                                           G_IO_ERROR_NOT_SUPPORTED,
                                           "Method not supported");
}

gboolean
yelp_application_load_uri (YelpApplication  *app,
                           const gchar      *uri,
                           guint32           timestamp,
                           GError          **error)
{
    YelpApplicationLoad *data;
    YelpUri *yuri;

    data = g_new (YelpApplicationLoad, 1);
    data->app = app;
    data->timestamp = timestamp;
    data->new = FALSE;
    
    yuri = yelp_uri_new (uri);
    
    g_signal_connect (yuri, "resolved",
                      G_CALLBACK (application_uri_resolved),
                      data);
    yelp_uri_resolve (yuri);

    return TRUE;
}

void
yelp_application_new_window (YelpApplication  *app,
                             const gchar      *uri)
{
    YelpUri *yuri;

    yuri = yelp_uri_new (uri ? uri : DEFAULT_URI);

    yelp_application_new_window_uri (app, yuri);
}

void
yelp_application_new_window_uri (YelpApplication  *app,
                                 YelpUri          *uri)
{
    YelpApplicationLoad *data;
    data = g_new (YelpApplicationLoad, 1);
    data->app = app;
    data->timestamp = gtk_get_current_event_time ();
    data->new = TRUE;
    g_signal_connect (uri, "resolved",
                      G_CALLBACK (application_uri_resolved),
                      data);
    yelp_uri_resolve (uri);
}

static void
application_uri_resolved (YelpUri             *uri,
                          YelpApplicationLoad *data)
{
    YelpWindow *window;
    gchar *doc_uri;
    GdkWindow *gdk_window;
    YelpApplicationPrivate *priv = GET_PRIV (data->app);

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
    }
    else {
        g_free (doc_uri);
    }

    yelp_window_load_uri (window, uri);

    gtk_widget_show_all (GTK_WIDGET (window));

    /* Metacity no longer does anything useful with gtk_window_present */
    gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
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

    g_free (data);
}

GSettings *
application_get_doc_settings (YelpApplication *app, const gchar *doc_uri)
{
    YelpApplicationPrivate *priv = GET_PRIV (app);
    GSettings *settings = g_hash_table_lookup (priv->docsettings, doc_uri);
    if (settings == NULL) {
        gchar *tmp;
        gchar *settings_path;
        tmp = g_uri_escape_string (doc_uri, "", FALSE);
        settings_path = g_strconcat ("/apps/yelp/documents/", tmp, "/", NULL);
        g_free (tmp);
        if (priv->backend)
            settings = g_settings_new_with_backend_and_path ("org.gnome.yelp.documents",
                                                             priv->backend,
                                                             settings_path);
        else
            settings = g_settings_new_with_path ("org.gnome.yelp.document",
                                                 settings_path);
        g_hash_table_insert (priv->docsettings, g_strdup (doc_uri), settings);
        g_free (settings_path);
    }
    return settings;
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

    if (priv->windows == NULL)
        g_timeout_add_seconds (5, (GSourceFunc) application_maybe_quit, app);

    return FALSE;
}

static gboolean
application_maybe_quit (YelpApplication *app)
{
    YelpApplicationPrivate *priv = GET_PRIV (app);

    if (priv->windows == NULL)
        gtk_main_quit ();

    return FALSE;
}

/******************************************************************************/

void
yelp_application_add_bookmark (YelpApplication   *app,
                               const gchar       *doc_uri,
                               const gchar       *page_id,
                               const gchar       *icon,
                               const gchar       *title)
{
    GSettings *settings;

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
            g_signal_emit (app, signals[BOOKMARKS_CHANGED], 0, doc_uri);
        }
    }
}

void
yelp_application_remove_bookmark (YelpApplication   *app,
                                  const gchar       *doc_uri,
                                  const gchar       *page_id)
{
    GSettings *settings;

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
        g_signal_emit (app, signals[BOOKMARKS_CHANGED], 0, doc_uri);
    }
}

void
yelp_application_update_bookmarks (YelpApplication   *app,
                                   const gchar       *doc_uri,
                                   const gchar       *page_id,
                                   const gchar       *icon,
                                   const gchar       *title)
{
    GSettings *settings;

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

        if (updated) {
            GVariant *value;
            value = g_variant_builder_end (&builder);
            g_settings_set_value (settings, "bookmarks", value);
            g_signal_emit (app, signals[BOOKMARKS_CHANGED], 0, doc_uri);
        }
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
packages_installed (GDBusConnection *connection,
                    GAsyncResult *res,
                    YelpApplication *app)
{
    GError *error = NULL;
    g_dbus_connection_call_finish (connection, res, &error);
    if (error) {
        const gchar *err = NULL;
        if (error->domain == G_DBUS_ERROR) {
            if (error->code == G_DBUS_ERROR_SERVICE_UNKNOWN)
                err = _("You do not have PackageKit installed.  Package installation links require PackageKit.");
            else
                err = error->message;
        }
        if (err != NULL) {
            GtkWidget *dialog = gtk_message_dialog_new (NULL, 0,
                                                        GTK_MESSAGE_ERROR,
                                                        GTK_BUTTONS_CLOSE,
                                                        "%s", err);
            gtk_dialog_run ((GtkDialog *) dialog);
            gtk_widget_destroy (dialog);
        }
        g_error_free (error);
    }
}

void
yelp_application_add_read_later (YelpApplication   *app,
                                 const gchar       *doc_uri,
                                 const gchar       *full_uri,
                                 const gchar       *title)
{
    GSettings *settings;

    settings = application_get_doc_settings (app, doc_uri);

    if (settings) {
        GVariantBuilder builder;
        GVariantIter *iter;
        gchar *this_uri, *this_title;
        gboolean broken = FALSE;
        g_settings_get (settings, "readlater", "a(ss)", &iter);
        g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
        while (g_variant_iter_loop (iter, "(&s&s)", &this_uri, &this_title)) {
            if (g_str_equal (full_uri, this_uri)) {
                /* Already have this link */
                broken = TRUE;
                break;
            }
            g_variant_builder_add (&builder, "(ss)", this_uri, this_title);
        }
        g_variant_iter_free (iter);

        if (!broken) {
            GVariant *value;
            g_variant_builder_add (&builder, "(ss)", full_uri, title);
            value = g_variant_builder_end (&builder);
            g_settings_set_value (settings, "readlater", value);
            g_signal_emit (app, signals[READ_LATER_CHANGED], 0, doc_uri);
        }
    }
}

void
yelp_application_remove_read_later (YelpApplication *app,
                                    const gchar     *doc_uri,
                                    const gchar     *full_uri)
{
    GSettings *settings;

    settings = application_get_doc_settings (app, doc_uri);

    if (settings) {
        GVariantBuilder builder;
        GVariantIter *iter;
        gchar *this_uri, *this_title;
        g_settings_get (settings, "readlater", "a(ss)", &iter);
        g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
        while (g_variant_iter_loop (iter, "(&s&s)", &this_uri, &this_title)) {
            if (!g_str_equal (this_uri, full_uri))
                g_variant_builder_add (&builder, "(ss)", this_uri, this_title);
        }
        g_variant_iter_free (iter);

        g_settings_set_value (settings, "readlater", g_variant_builder_end (&builder));
        g_signal_emit (app, signals[READ_LATER_CHANGED], 0, doc_uri);
    }
}

GVariant *
yelp_application_get_read_later (YelpApplication *app,
                                 const gchar     *doc_uri)
{
    GSettings *settings = application_get_doc_settings (app, doc_uri);

    return g_settings_get_value (settings, "readlater");
}

void
yelp_application_install_package (YelpApplication  *app,
                                  const gchar      *pkg,
                                  const gchar      *alt)
{
    GVariantBuilder *strv;
    YelpApplicationPrivate *priv = GET_PRIV (app);
    guint32 xid = 0;
    strv = g_variant_builder_new (G_VARIANT_TYPE ("as"));
    g_variant_builder_add (strv, "s", pkg);
    g_dbus_connection_call (priv->connection,
                            "org.freedesktop.PackageKit",
                            "/org/freedesktop/PackageKit",
                            "org.freedesktop.PackageKit.Modify",
                            "InstallPackageNames",
                            g_variant_new ("(uass)", xid, strv, ""),
                            NULL,
                            G_DBUS_CALL_FLAGS_NONE,
                            -1, NULL,
                            (GAsyncReadyCallback) packages_installed,
                            app);
    g_variant_builder_unref (strv);
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
    doc_uri = yelp_uri_get_document_uri (uri);
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
