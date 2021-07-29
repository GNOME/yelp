/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2009 Shaun McCance  <shaunm@gnome.org>
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
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#ifndef __YELP_TRANSFORM_H__
#define __YELP_TRANSFORM_H__

#include <glib.h>
#include <glib-object.h>
#include <libxml/tree.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>

#define YELP_TYPE_TRANSFORM            (yelp_transform_get_type ())
#define YELP_TRANSFORM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), YELP_TYPE_TRANSFORM, YelpTransform))
#define YELP_TRANSFORM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), YELP_TYPE_TRANSFORM, YelpTransformClass))
#define YELP_IS_TRANSFORM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), YELP_TYPE_TRANSFORM))
#define YELP_IS_TRANSFORM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), YELP_TYPE_TRANSFORM))

typedef struct _YelpTransform YelpTransform;
typedef struct _YelpTransformClass YelpTransformClass;
struct _YelpTransform {
    GObject       parent;
};
struct _YelpTransformClass {
    GObjectClass  parent_class;
};

GType            yelp_transform_get_type       (void);
YelpTransform  * yelp_transform_new            (const gchar         *stylesheet);
gboolean         yelp_transform_start          (YelpTransform       *transform,
                                                xmlDocPtr            document,
                                                xmlDocPtr            auxiliary,
                                                const gchar * const *params);
gchar *          yelp_transform_take_chunk     (YelpTransform       *transform,
                                                const gchar         *chunk_id);
void             yelp_transform_cancel         (YelpTransform       *transform);
GError *         yelp_transform_get_error      (YelpTransform       *transform);

xmlDocPtr        yelp_transform_get_xmldoc     (YelpTransform       *transform);

#endif /* __YELP_TRANSFORM_H__ */
