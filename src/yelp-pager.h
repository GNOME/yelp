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
typedef struct _YelpPage       YelpPage;

typedef gulong YelpPagerState;
enum {
    YELP_PAGER_STATE_STARTED  = 1 << 0,
    YELP_PAGER_STATE_STOPPED  = 1 << 1,
    YELP_PAGER_STATE_FINISHED = 1 << 3,

    YELP_PAGER_STATE_CONTENTS = 1 << 4,

    YELP_PAGER_LAST_STATE     = 1 << 4
};

struct _YelpPager {
    GObject        parent;

    YelpPagerPriv *priv;
};

struct _YelpPagerClass {
    GObjectClass    parent_class;

    void                 (*error)        (YelpPager    *pager);
    void                 (*cancel)       (YelpPager    *pager);
    void                 (*finish)       (YelpPager    *pager);

    /* Virtual Functions */
    gboolean             (*process)      (YelpPager    *pager);
    const gchar *        (*resolve_frag) (YelpPager    *pager,
					  const gchar  *frag_id);
    const GtkTreeModel * (*get_sections) (YelpPager *pager);
};

struct _YelpPage {
    gchar *page_id;
    gchar *title;
    gchar *contents;

    gchar *prev_id;
    gchar *next_id;
    gchar *toc_id;
};

GType                yelp_pager_get_type     (void);

gboolean             yelp_pager_start        (YelpPager      *pager);
void                 yelp_pager_cancel       (YelpPager      *pager);

YelpURI *            yelp_pager_get_uri      (YelpPager      *pager);

YelpPagerState       yelp_pager_get_state    (YelpPager      *pager);
void                 yelp_pager_clear_state  (YelpPager      *pager);
void                 yelp_pager_set_state    (YelpPager      *pager,
					      YelpPagerState  state);

GError *             yelp_pager_get_error    (YelpPager      *pager);
void                 yelp_pager_error        (YelpPager      *pager,
					      GError         *error);

const GtkTreeModel * yelp_pager_get_sections       (YelpPager      *pager);

const gchar *        yelp_pager_resolve_frag       (YelpPager      *pager,
						    const gchar    *frag_id);
gboolean             yelp_pager_page_contains_frag (YelpPager      *pager,
						    const gchar    *page_id,
						    const gchar    *frag_id);
const YelpPage *     yelp_pager_get_page           (YelpPager      *pager,
						    const gchar    *frag_id);
void                 yelp_pager_add_page           (YelpPager      *pager,
						    YelpPage       *page);
void                 yelp_page_free                (YelpPage       *page);

#endif /* __YELP_PAGER_H__ */
