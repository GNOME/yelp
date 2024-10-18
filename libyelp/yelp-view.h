/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009 Shaun McCance <shaunm@gnome.org>
 * Copyright (C) 2014 Igalia S.L.
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
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifndef __YELP_VIEW_H__
#define __YELP_VIEW_H__

#include <gtk/gtk.h>
#include <webkit/webkit.h>

#include "yelp-document.h"
#include "yelp-uri.h"

G_BEGIN_DECLS

#define YELP_TYPE_VIEW            (yelp_view_get_type ())
#define YELP_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), YELP_TYPE_VIEW, YelpView))
#define YELP_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), YELP_TYPE_VIEW, YelpViewClass))
#define YELP_IS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), YELP_TYPE_VIEW))
#define YELP_IS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), YELP_TYPE_VIEW))
#define YELP_VIEW_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_VIEW, YelpViewClass))

typedef struct _YelpView         YelpView;
typedef struct _YelpViewClass    YelpViewClass;

struct _YelpView
{
    WebKitWebView       parent;
};

struct _YelpViewClass
{
    WebKitWebViewClass  parent_class;

    gboolean    (* external_uri)            (YelpView    *view,
                                             YelpUri     *uri);
};

typedef enum {
    YELP_VIEW_STATE_BLANK,
    YELP_VIEW_STATE_LOADING,
    YELP_VIEW_STATE_LOADED,
    YELP_VIEW_STATE_ERROR
} YelpViewState;

GType              yelp_view_get_type             (void);

GtkWidget *        yelp_view_new                  (void);
void               yelp_view_load                 (YelpView                *view,
                                                   const gchar             *uri);
void               yelp_view_load_uri             (YelpView                *view,
                                                   YelpUri                 *uri);
void               yelp_view_load_document        (YelpView                *view,
                                                   YelpUri                 *uri,
                                                   YelpDocument            *document);
YelpDocument *     yelp_view_get_document         (YelpView                *view);

void               yelp_view_register_actions     (YelpView                *view,
                                                   GActionMap              *map);

YelpUri *          yelp_view_get_active_link_uri  (YelpView                *view);
gchar *            yelp_view_get_active_link_text (YelpView                *view);

G_END_DECLS

#endif /* __YELP_VIEW_H__ */
