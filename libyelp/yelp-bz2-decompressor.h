/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 
 * Copyright (C) 2009 Red Hat, Inc.
 * Copyright (C) 2009 Shaun McCance <shaunm@gnome.org>
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

#ifndef __YELP_BZ2_DECOMPRESSOR_H__
#define __YELP_BZ2_DECOMPRESSOR_H__

#include <gio/gio.h>
#include <bzlib.h>

G_BEGIN_DECLS

#define YELP_TYPE_BZ2_DECOMPRESSOR         (yelp_bz2_decompressor_get_type ())
#define YELP_BZ2_DECOMPRESSOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_BZ2_DECOMPRESSOR, YelpBz2Decompressor))
#define YELP_BZ2_DECOMPRESSOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), YELP_TYPE_BZ2_DECOMPRESSOR, YelpBz2DecompressorClass))
#define YELP_IS_BZ2_DECOMPRESSOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_BZ2_DECOMPRESSOR))
#define YELP_IS_BZ2_DECOMPRESSOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_BZ2_DECOMPRESSOR))
#define YELP_BZ2_DECOMPRESSOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_BZ2_DECOMPRESSOR, YelpBz2DecompressorClass))

typedef struct _YelpBz2Decompressor        YelpBz2Decompressor;
typedef struct _YelpBz2DecompressorClass   YelpBz2DecompressorClass;

struct _YelpBz2DecompressorClass
{
    GObjectClass parent_class;
};

G_GNUC_INTERNAL
GType               yelp_bz2_decompressor_get_type (void);

G_GNUC_INTERNAL
YelpBz2Decompressor *yelp_bz2_decompressor_new (void);

G_END_DECLS

#endif /* __YELP_BZ2_DECOMPRESSOR_H__ */
