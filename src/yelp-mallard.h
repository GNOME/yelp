/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009 Shaun McCance  <shaunm@gnome.org>
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
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#ifndef __YELP_MALLARD_H__
#define __YELP_MALLARD_H__

#include <glib-object.h>

#include "yelp-document.h"

#define YELP_TYPE_MALLARD         (yelp_mallard_get_type ())
#define YELP_MALLARD(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_MALLARD, YelpMallard))
#define YELP_MALLARD_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_MALLARD, YelpMallardClass))
#define YELP_IS_MALLARD(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_MALLARD))
#define YELP_IS_MALLARD_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_MALLARD))
#define YELP_MALLARD_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_MALLARD, YelpMallardClass))

typedef struct _YelpMallard      YelpMallard;
typedef struct _YelpMallardClass YelpMallardClass;
typedef struct _YelpMallardPriv  YelpMallardPriv;

struct _YelpMallard {
    YelpDocument      parent;
    YelpMallardPriv  *priv;
};

struct _YelpMallardClass {
    YelpDocumentClass parent_class;
};

GType           yelp_mallard_get_type     (void);
YelpDocument *  yelp_mallard_new          (gchar  *uri);

#endif /* __YELP_MALLARD_H__ */
