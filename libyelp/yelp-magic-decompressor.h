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

#ifndef __YELP_MAGIC_DECOMPRESSOR_H__
#define __YELP_MAGIC_DECOMPRESSOR_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define YELP_TYPE_MAGIC_DECOMPRESSOR         (yelp_magic_decompressor_get_type ())
#define YELP_MAGIC_DECOMPRESSOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_MAGIC_DECOMPRESSOR, YelpMagicDecompressor))
#define YELP_MAGIC_DECOMPRESSOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), YELP_TYPE_MAGIC_DECOMPRESSOR, YelpMagicDecompressorClass))
#define YELP_IS_MAGIC_DECOMPRESSOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_MAGIC_DECOMPRESSOR))
#define YELP_IS_MAGIC_DECOMPRESSOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_MAGIC_DECOMPRESSOR))
#define YELP_MAGIC_DECOMPRESSOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_MAGIC_DECOMPRESSOR, YelpMagicDecompressorClass))

typedef struct _YelpMagicDecompressor        YelpMagicDecompressor;
typedef struct _YelpMagicDecompressorClass   YelpMagicDecompressorClass;

struct _YelpMagicDecompressorClass
{
    GObjectClass parent_class;
};

G_GNUC_INTERNAL
GType                 yelp_magic_decompressor_get_type (void);

G_GNUC_INTERNAL
YelpMagicDecompressor *yelp_magic_decompressor_new (void);

G_END_DECLS

#endif /* __YELP_MAGIC_DECOMPRESSOR_H__ */
