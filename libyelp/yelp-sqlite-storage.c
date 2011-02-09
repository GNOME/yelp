/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2011 Shaun McCance <shaunm@gnome.org>
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
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#include "config.h"

#include <glib/gi18n.h>
#include <sqlite3.h>

#include "yelp-sqlite-storage.h"

static void        yelp_sqlite_storage_init         (YelpSqliteStorage      *storage);
static void        yelp_sqlite_storage_class_init   (YelpSqliteStorageClass *klass);
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

static void        yelp_sqlite_storage_update      (YelpStorage      *storage,
                                                    const gchar      *doc_uri,
                                                    const gchar      *full_uri,
                                                    const gchar      *title,
                                                    const gchar      *desc,
                                                    const gchar      *icon,
                                                    const gchar      *text);
static GVariant *  yelp_sqlite_storage_search      (YelpStorage      *storage,
                                                    const gchar      *doc_uri,
                                                    const gchar      *text);

typedef struct _YelpSqliteStoragePrivate YelpSqliteStoragePrivate;
struct _YelpSqliteStoragePrivate {
    gchar   *filename;
    sqlite3 *db;
};

enum {  
    PROP_0,
    PROP_FILENAME
};

G_DEFINE_TYPE_WITH_CODE (YelpSqliteStorage, yelp_sqlite_storage, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (YELP_TYPE_STORAGE,
                                                yelp_sqlite_storage_iface_init))
#define GET_PRIV(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_SQLITE_STORAGE, YelpSqliteStoragePrivate))

static void
yelp_sqlite_storage_finalize (GObject *object)
{
    YelpSqliteStoragePrivate *priv = GET_PRIV (object);

    if (priv->filename)
        g_free (priv->filename);

    if (priv->db)
        sqlite3_close (priv->db);

    G_OBJECT_CLASS (yelp_sqlite_storage_parent_class)->finalize (object);
}

static void
yelp_sqlite_storage_init (YelpSqliteStorage *storage)
{
}

static void
yelp_sqlite_storage_constructed (GObject *object)
{
    YelpSqliteStoragePrivate *priv = GET_PRIV (object);

    if (priv->filename != NULL)
        sqlite3_open (priv->filename, &(priv->db));
    else
        sqlite3_open (":memory:", &(priv->db));

    /* FIXME: create tables if necessary */
}

static void
yelp_sqlite_storage_class_init (YelpSqliteStorageClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->constructed = yelp_sqlite_storage_constructed;
    object_class->finalize = yelp_sqlite_storage_finalize;
    object_class->get_property = yelp_sqlite_storage_get_property;
    object_class->set_property = yelp_sqlite_storage_set_property;

    g_type_class_add_private (klass, sizeof (YelpSqliteStoragePrivate));

    g_object_class_install_property (object_class,
                                     PROP_FILENAME,
                                     g_param_spec_string ("filename",
                                                          N_("Database filename"),
                                                          N_("The filename of the sqlite database"),
                                                          NULL,
                                                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS));
}

static void
yelp_sqlite_storage_iface_init (YelpStorageInterface *iface)
{
    iface->update = yelp_sqlite_storage_update;
    iface->search = yelp_sqlite_storage_search;
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
    YelpSqliteStoragePrivate *priv = GET_PRIV (object);

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
    YelpSqliteStoragePrivate *priv = GET_PRIV (object);

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
}

static GVariant *
yelp_sqlite_storage_search (YelpStorage   *storage,
                            const gchar   *doc_uri,
                            const gchar   *text)
{
    GVariantBuilder builder;
    GVariant *ret;
    int i;

    /* FIXME */
    g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ssss)"));
    for (i = 0; i < 1; i++) {
        g_variant_builder_add (&builder, "(ssss)",
                               doc_uri,
                               doc_uri,
                               "Description",
                               "help-contents");
    }
    ret = g_variant_new ("a(ssss)", &builder);
    return ret;
}
