/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2011-2020 Shaun McCance <shaunm@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#include "config.h"

#include <glib/gi18n.h>
#include <sqlite3.h>

#include "yelp-sqlite-storage.h"

static void        yelp_sqlite_storage_iface_init   (YelpStorageInterface   *iface);
static void        yelp_sqlite_storage_finalize     (GObject                *object);
static void        yelp_sqlite_storage_get_property (GObject                *object,
                                                     guint                   prop_id,
                                                     GValue                 *value,
                                                     GParamSpec             *pspec);
static void        yelp_sqlite_storage_set_property (GObject                *object,
                                                     guint                   prop_id,
                                                     const GValue           *value,
                                                     GParamSpec             *pspec);

static void        yelp_sqlite_storage_update         (YelpStorage      *storage,
                                                       const gchar      *doc_uri,
                                                       const gchar      *full_uri,
                                                       const gchar      *title,
                                                       const gchar      *desc,
                                                       const gchar      *icon,
                                                       const gchar      *text);
static GVariant *  yelp_sqlite_storage_search         (YelpStorage      *storage,
                                                       const gchar      *doc_uri,
                                                       const gchar      *text);
static gchar *     yelp_sqlite_storage_get_root_title (YelpStorage      *storage,
                                                       const gchar      *doc_uri);
static void        yelp_sqlite_storage_set_root_title (YelpStorage      *storage,
                                                       const gchar      *doc_uri,
                                                       const gchar      *title);

typedef struct _YelpSqliteStoragePrivate YelpSqliteStoragePrivate;
struct _YelpSqliteStoragePrivate {
    gchar   *filename;
    sqlite3 *db;
    GMutex mutex;
};

enum {  
    PROP_0,
    PROP_FILENAME
};

G_DEFINE_TYPE_WITH_CODE (YelpSqliteStorage, yelp_sqlite_storage, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (YELP_TYPE_STORAGE,
                                                yelp_sqlite_storage_iface_init)
                         G_ADD_PRIVATE (YelpSqliteStorage) )

static void
yelp_sqlite_storage_finalize (GObject *object)
{
    YelpSqliteStoragePrivate *priv =
        yelp_sqlite_storage_get_instance_private (YELP_SQLITE_STORAGE (object));

    if (priv->filename)
        g_free (priv->filename);

    if (priv->db)
        sqlite3_close (priv->db);

    g_mutex_clear (&priv->mutex);

    G_OBJECT_CLASS (yelp_sqlite_storage_parent_class)->finalize (object);
}

static void
yelp_sqlite_storage_init (YelpSqliteStorage *storage)
{
    YelpSqliteStoragePrivate *priv = yelp_sqlite_storage_get_instance_private (storage);
    g_mutex_init (&priv->mutex);
}

static void
yelp_sqlite_storage_constructed (GObject *object)
{
    int status;
    sqlite3_stmt *stmt = NULL;
    YelpSqliteStoragePrivate *priv =
        yelp_sqlite_storage_get_instance_private (YELP_SQLITE_STORAGE (object));

    if (priv->filename != NULL)
        status = sqlite3_open (priv->filename, &(priv->db));
    else
        status = sqlite3_open (":memory:", &(priv->db));

    if (status != SQLITE_OK)
        return;

    status = sqlite3_prepare_v2 (priv->db,
                                 "create virtual table pages using fts4("
                                 " doc_uri, lang, full_uri,"
                                 " title, desc, icon, body"
                                 ");",
                                 -1, &stmt, NULL);
    if (status != SQLITE_OK)
        return;
    sqlite3_step (stmt);
    sqlite3_finalize (stmt);

    status = sqlite3_prepare_v2 (priv->db,
                                 "create table titles (doc_uri text, lang text, title text);",
                                 -1, &stmt, NULL);
    if (status != SQLITE_OK)
        return;
    sqlite3_step (stmt);
    sqlite3_finalize (stmt);
}

static void
yelp_sqlite_storage_class_init (YelpSqliteStorageClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->constructed = yelp_sqlite_storage_constructed;
    object_class->finalize = yelp_sqlite_storage_finalize;
    object_class->get_property = yelp_sqlite_storage_get_property;
    object_class->set_property = yelp_sqlite_storage_set_property;

    g_object_class_install_property (object_class,
                                     PROP_FILENAME,
                                     g_param_spec_string ("filename",
                                                          "Database filename",
                                                          "The filename of the sqlite database",
                                                          NULL,
                                                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS));
}

static void
yelp_sqlite_storage_iface_init (YelpStorageInterface *iface)
{
    iface->update = yelp_sqlite_storage_update;
    iface->search = yelp_sqlite_storage_search;
    iface->get_root_title = yelp_sqlite_storage_get_root_title;
    iface->set_root_title = yelp_sqlite_storage_set_root_title;
}

YelpStorage *
yelp_sqlite_storage_new (const gchar *filename)
{
    YelpStorage *storage;

    storage = g_object_new (YELP_TYPE_SQLITE_STORAGE,
                            "filename", filename,
                            NULL);

    return storage;
}

static void
yelp_sqlite_storage_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
    YelpSqliteStoragePrivate *priv =
        yelp_sqlite_storage_get_instance_private (YELP_SQLITE_STORAGE (object));

    switch (prop_id) {
    case PROP_FILENAME:
        g_value_set_string (value, priv->filename);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
yelp_sqlite_storage_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
    YelpSqliteStoragePrivate *priv =
        yelp_sqlite_storage_get_instance_private (YELP_SQLITE_STORAGE (object));

    switch (prop_id) {
    case PROP_FILENAME:
        priv->filename = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/******************************************************************************/

static void
yelp_sqlite_storage_update (YelpStorage   *storage,
                            const gchar   *doc_uri,
                            const gchar   *full_uri,
                            const gchar   *title,
                            const gchar   *desc,
                            const gchar   *icon,
                            const gchar   *text)
{
    sqlite3_stmt *stmt = NULL;
    YelpSqliteStoragePrivate *priv =
        yelp_sqlite_storage_get_instance_private (YELP_SQLITE_STORAGE (storage));

    g_mutex_lock (&priv->mutex);

    sqlite3_prepare_v2 (priv->db,
                        "delete from pages where doc_uri = ? and lang = ? and full_uri = ?;",
                        -1, &stmt, NULL);
    sqlite3_bind_text (stmt, 1, doc_uri, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, g_get_language_names()[0], -1, SQLITE_STATIC);
    sqlite3_bind_text (stmt, 3, full_uri, -1, SQLITE_TRANSIENT);
    sqlite3_step (stmt);
    sqlite3_finalize (stmt);

    sqlite3_prepare_v2 (priv->db,
                        "insert into pages (doc_uri, lang, full_uri, title, desc, icon, body)"
                        " values (?, ?, ?, ?, ?, ?, ?);",
                        -1, &stmt, NULL);
    sqlite3_bind_text (stmt, 1, doc_uri, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, g_get_language_names()[0], -1, SQLITE_STATIC);
    sqlite3_bind_text (stmt, 3, full_uri, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 4, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 5, desc, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 6, icon, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 7, text, -1, SQLITE_TRANSIENT);
    sqlite3_step (stmt);
    sqlite3_finalize (stmt);

    g_mutex_unlock (&priv->mutex);
}

static GVariant *
yelp_sqlite_storage_search (YelpStorage   *storage,
                            const gchar   *doc_uri,
                            const gchar   *text)
{
    sqlite3_stmt *stmt = NULL;
    GVariantBuilder builder;
    GVariant *ret;
    YelpSqliteStoragePrivate *priv =
        yelp_sqlite_storage_get_instance_private (YELP_SQLITE_STORAGE (storage));


    g_mutex_lock (&priv->mutex);

    sqlite3_prepare_v2 (priv->db,
                        "select full_uri, title, desc, icon from pages where"
                        " doc_uri = ? and lang = ? and body match ?;",
                        -1, &stmt, NULL);
    sqlite3_bind_text (stmt, 1, doc_uri, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, g_get_language_names()[0], -1, SQLITE_STATIC);
    sqlite3_bind_text (stmt, 3, text, -1, SQLITE_TRANSIENT);

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ssss)"));
    while (sqlite3_step (stmt) == SQLITE_ROW) {
        g_variant_builder_add (&builder, "(ssss)",
                               sqlite3_column_text (stmt, 0),
                               sqlite3_column_text (stmt, 1),
                               sqlite3_column_text (stmt, 2),
                               sqlite3_column_text (stmt, 3));
    }
    sqlite3_finalize (stmt);
    ret = g_variant_new ("a(ssss)", &builder);

    g_mutex_unlock (&priv->mutex);

    return ret;
}

static gchar *
yelp_sqlite_storage_get_root_title (YelpStorage *storage,
                                    const gchar *doc_uri)
{
    gchar *ret = NULL;
    sqlite3_stmt *stmt = NULL;
    YelpSqliteStoragePrivate *priv =
        yelp_sqlite_storage_get_instance_private (YELP_SQLITE_STORAGE (storage));

    g_mutex_lock (&priv->mutex);

    sqlite3_prepare_v2 (priv->db,
                        "select title from titles where doc_uri = ? and lang = ?;",
                        -1, &stmt, NULL);
    sqlite3_bind_text (stmt, 1, doc_uri, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, g_get_language_names()[0], -1, SQLITE_STATIC);
    if (sqlite3_step (stmt) == SQLITE_ROW)
        ret = g_strdup ((const gchar *) sqlite3_column_text (stmt, 0));
    sqlite3_finalize (stmt);

    g_mutex_unlock (&priv->mutex);
    return ret;
}

static void
yelp_sqlite_storage_set_root_title (YelpStorage *storage,
                                    const gchar *doc_uri,
                                    const gchar *title)
{
    sqlite3_stmt *stmt = NULL;
    YelpSqliteStoragePrivate *priv =
        yelp_sqlite_storage_get_instance_private (YELP_SQLITE_STORAGE (storage));

    g_mutex_lock (&priv->mutex);

    sqlite3_prepare_v2 (priv->db,
                        "delete from titles where doc_uri = ? and lang = ?;",
                        -1, &stmt, NULL);
    sqlite3_bind_text (stmt, 1, doc_uri, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, g_get_language_names()[0], -1, SQLITE_STATIC);
    sqlite3_step (stmt);
    sqlite3_finalize (stmt);

    sqlite3_prepare_v2 (priv->db,
                        "insert into titles (doc_uri, lang, title)"
                        " values (?, ?, ?);",
                        -1, &stmt, NULL);
    sqlite3_bind_text (stmt, 1, doc_uri, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, g_get_language_names()[0], -1, SQLITE_STATIC);
    sqlite3_bind_text (stmt, 3, title, -1, SQLITE_TRANSIENT);
    sqlite3_step (stmt);
    sqlite3_finalize (stmt);

    g_mutex_unlock (&priv->mutex);
}
