/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2006 Shaun McCance  <shaunm@gnome.org>
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
typedef struct _YelpTransformChunk YelpTransformChunk;

typedef void  (*YelpTransformChunkFunc)  (YelpTransform           *transform,
					  YelpTransformChunk      *chunk,
					  gpointer                 user_data);
typedef void  (*YelpTransformErrorFunc)  (YelpTransform           *transform,
					  YelpError               *error,
					  gpointer                 user_data);
typedef void  (*YelpTransformFinalFunc)  (YelpTransform           *transform,
					  gpointer                 user_data);

struct _YelpTransform {
    xmlDocPtr               inputDoc;
    xmlDocPtr               outputDoc;
    xsltStylesheetPtr       stylesheet;
    xsltTransformContextPtr context;

    YelpTransformChunkFunc  chunk_func;
    YelpTransformErrorFunc  error_func;
    YelpTransformFinalFunc  final_func;

    gchar                 **params;

    GThread                *thread;
    GMutex                 *mutex;
    GAsyncQueue            *queue;

    gboolean                running;
    gboolean                freeme;

    gpointer                user_data;

    YelpError              *error;
};

struct _YelpTransformChunk {
    gchar *id;
    gchar *title;
    gchar *contents;
};

YelpTransform  *yelp_transform_new       (gchar                   *stylesheet,
					  YelpTransformChunkFunc   chunk_func,
					  YelpTransformErrorFunc   error_func,
					  YelpTransformFinalFunc   final_func,
					  gpointer                 user_data);
void            yelp_transform_start     (YelpTransform           *transform,
					  xmlDocPtr                document,
					  gchar                  **params);
void            yelp_transform_release   (YelpTransform           *transform);

void            yelp_transform_chunk_free  (YelpTransformChunk  *chunk);

#endif /* __YELP_TRANSFORM_H__ */
