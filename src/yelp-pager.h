/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003 Shaun McCance  <shaunm@gnome.org>
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

#ifndef __YELP_PAGER_H__
#define __YELP_PAGER_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include "yelp-uri.h"

#define YELP_TYPE_PAGER         (yelp_pager_get_type ())
#define YELP_PAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_PAGER, YelpPager))
#define YELP_PAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_PAGER, YelpPagerClass))
#define YELP_IS_PAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_PAGER))
#define YELP_IS_PAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_PAGER))
#define YELP_PAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_PAGER, YelpPagerClass))

typedef struct _YelpPager      YelpPager;
typedef struct _YelpPagerClass YelpPagerClass;
typedef struct _YelpPagerPriv  YelpPagerPriv;

typedef enum {
    YELP_PAGER_STATE_NEW,
    YELP_PAGER_STATE_START,
    YELP_PAGER_STATE_ERROR,
    YELP_PAGER_STATE_CANCEL,
    YELP_PAGER_STATE_FINISH
} YelpPagerState;

struct _YelpPager {
    GObject        parent;

    YelpPagerPriv *priv;
};

struct _YelpPagerClass {
    GObjectClass    parent_class;

    /* Virtual Functions */
    void                 (*process)      (YelpPager    *pager);
    void                 (*cancel)       (YelpPager    *pager);
    const gchar *        (*resolve_uri)  (YelpPager    *pager,
					  YelpURI      *uri);
    const GtkTreeModel * (*get_sections) (YelpPager *pager);
    
};

GType                yelp_pager_get_type     (void);

gboolean             yelp_pager_start        (YelpPager      *pager);
void                 yelp_pager_cancel       (YelpPager      *pager);

const YelpURI *      yelp_pager_get_uri      (YelpPager      *pager);

YelpPagerState       yelp_pager_get_state    (YelpPager      *pager);
void                 yelp_pager_set_state    (YelpPager      *pager,
					      YelpPagerState  state);
void                 yelp_pager_lock_state   (YelpPager      *pager);
void                 yelp_pager_unlock_state (YelpPager      *pager);

GError *             yelp_pager_get_error    (YelpPager      *pager);
void                 yelp_pager_error        (YelpPager      *pager,
					      GError         *error);

const GtkTreeModel * yelp_pager_get_sections (YelpPager      *pager);

const gchar *        yelp_pager_lookup_chunk (YelpPager      *pager,
					      YelpURI        *uri);
const gchar *        yelp_pager_get_chunk    (YelpPager      *pager,
					      gchar          *id);
void                 yelp_pager_add_chunk    (YelpPager      *pager,
					      gchar          *id,
					      gchar          *chunk);

#endif /* __YELP_PAGER_H__ */
