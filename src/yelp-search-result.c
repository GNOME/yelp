/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2025 knuxify <knuxify@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: knuxify <knuxify@gmail.com>
 */

#include "yelp-search-result.h"
#include "glib.h"

typedef enum
{
    PROP_TITLE = 1,
    PROP_DESC,
    PROP_ICON,
    PROP_PAGE,
    PROP_FLAGS,
    PROP_KEYWORDS,
    N_PROPERTIES
} YelpSearchResultProperty;

struct _YelpSearchResult
{
    GObject parent_instance;

    gchar *title;
    gchar *desc;
    gchar *icon;
    gchar *page;
    guint flags;
    gchar *keywords;
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (YelpSearchResult, yelp_search_result, G_TYPE_OBJECT)

static void
yelp_search_result_set_property (GObject  *object,
                                  guint   property_id,
                                  const GValue *value,
                                  GParamSpec     *pspec)
{
    YelpSearchResult *self = YELP_SEARCH_RESULT (object);
    gchar *unescaped;

    switch ((YelpSearchResultProperty) property_id)
    {
    case PROP_TITLE:
            g_free (self->title);
            unescaped = g_value_dup_string (value);
            if (unescaped == NULL)
                self->title = NULL;
            else
                self->title = g_markup_escape_text(unescaped, -1);
            g_free(unescaped);
            break;

    case PROP_DESC:
            g_free (self->desc);
            unescaped = g_value_dup_string (value);
            if (unescaped == NULL)
                self->desc = NULL;
            else
                self->desc = g_markup_escape_text(unescaped, -1);
            g_free(unescaped);
            break;

    case PROP_ICON:
            g_free (self->icon);
            self->icon = g_value_dup_string (value);
            break;

    case PROP_PAGE:
            g_free (self->page);
            self->page = g_value_dup_string (value);
            break;

    case PROP_FLAGS:
            self->flags = g_value_get_uint (value);
            break;

    case PROP_KEYWORDS:
            g_free (self->keywords);
            self->keywords = g_value_dup_string (value);
            break;

    default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
yelp_search_result_get_property (GObject        *object,
                                  guint          property_id,
                                  GValue        *value,
                                  GParamSpec    *pspec)
{
    YelpSearchResult *self = YELP_SEARCH_RESULT (object);

    switch ((YelpSearchResultProperty) property_id)
    {
    case PROP_TITLE:
            g_value_set_string (value, self->title);
            break;

    case PROP_DESC:
            g_value_set_string (value, self->desc);
            break;

    case PROP_ICON:
            g_value_set_string (value, self->icon);
            break;

    case PROP_PAGE:
            g_value_set_string (value, self->page);
            break;

    case PROP_FLAGS:
            g_value_set_uint (value, self->flags);
            break;

    case PROP_KEYWORDS:
            g_value_set_string (value, self->keywords);
            break;

    default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
yelp_search_result_finalize (GObject *object)
{
    YelpSearchResult *self = YELP_SEARCH_RESULT (object);

    if (self->title)
            g_free (self->title);
    if (self->desc)
            g_free (self->desc);
    if (self->icon)
            g_free (self->icon);
    if (self->page)
            g_free (self->page);
    if (self->keywords)
            g_free (self->keywords);

    /* Always chain up to the parent class; as with dispose(), finalize()
     * is guaranteed to exist on the parent's class virtual function table
     */
    G_OBJECT_CLASS (yelp_search_result_parent_class)->finalize (object);
}

static void
yelp_search_result_init (YelpSearchResult *result)
{

}

static void
yelp_search_result_class_init (YelpSearchResultClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->set_property = yelp_search_result_set_property;
    object_class->get_property = yelp_search_result_get_property;

    obj_properties[PROP_TITLE] =
            g_param_spec_string ("title",
                                 "Title",
                                 "Title of the search result.",
                                 NULL    /* default value */,
                                 G_PARAM_READWRITE);

    obj_properties[PROP_DESC] =
            g_param_spec_string ("desc",
                                 "Description",
                                 "Description of the search result.",
                                 NULL    /* default value */,
                                 G_PARAM_READWRITE);

    obj_properties[PROP_ICON] =
            g_param_spec_string ("icon",
                                 "Icon",
                                 "The primary icon to display for the result.",
                                 NULL    /* default value */,
                                 G_PARAM_READWRITE);

    obj_properties[PROP_PAGE] =
            g_param_spec_string ("page",
                                 "Page",
                                 "Page URI corresponding to the result.",
                                 NULL    /* default value */,
                                 G_PARAM_READWRITE);

    obj_properties[PROP_FLAGS] =
            g_param_spec_uint   ("flags",
                                 "Flags",
                                 "Flags that affect the search result.",
                                 0    /* minimum value */,
                                 1024 /* maximum value */,
                                 0    /* default value */,
                                 G_PARAM_READWRITE);

    obj_properties[PROP_KEYWORDS] =
            g_param_spec_string ("keywords",
                                 "Keywords",
                                 "Keywords used when matching for the result.",
                                 NULL    /* default value */,
                                 G_PARAM_READWRITE);

    g_object_class_install_properties (object_class,
                                       N_PROPERTIES,
                                       obj_properties);

    object_class->finalize = yelp_search_result_finalize;
}
