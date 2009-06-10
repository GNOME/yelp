/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2007 Shaun McCance  <shaunm@gnome.org>
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

#ifndef __YELP_TRANSFORM_H__
#define __YELP_TRANSFORM_H__

#include <glib.h>
#include <libxml/tree.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>

#include "yelp-error.h"

typedef struct _YelpTransform YelpTransform;

typedef enum {
    YELP_TRANSFORM_CHUNK,
    YELP_TRANSFORM_ERROR,
    YELP_TRANSFORM_FINAL
} YelpTransformSignal;

typedef void  (*YelpTransformFunc)  (YelpTransform       *transform,
				     YelpTransformSignal  signal,
				     gpointer             func_data,
				     gpointer             user_data);

struct _YelpTransform {
    xmlDocPtr               inputDoc;
    xmlDocPtr               outputDoc;
    xsltStylesheetPtr       stylesheet;
    xsltTransformContextPtr context;

    xmlDocPtr               input;
    xsltDocumentPtr         input_xslt;

    YelpTransformFunc       func;

    gchar                 **params;

    GThread                *thread;
    GMutex                 *mutex;
    GAsyncQueue            *queue;
    GHashTable             *chunks;

    gboolean                running;
    gboolean                released;
    gint                    idle_funcs;
    gint                    free_attempts;

    gpointer                user_data;

    YelpError              *error;
};

YelpTransform  *yelp_transform_new         (gchar               *stylesheet,
					    YelpTransformFunc    func,
					    gpointer             user_data);
void            yelp_transform_start       (YelpTransform       *transform,
					    xmlDocPtr            document,
					    gchar              **params);
void            yelp_transform_set_input   (YelpTransform       *transform,
					    xmlDocPtr            input);
gchar *         yelp_transform_eat_chunk   (YelpTransform       *transform,
					    gchar               *chunk_id);
void            yelp_transform_release     (YelpTransform       *transform);

#endif /* __YELP_TRANSFORM_H__ */
