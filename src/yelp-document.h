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

#ifndef __YELP_DOCUMENT_H__
#define __YELP_DOCUMENT_H__

#include <glib-object.h>

#define YELP_TYPE_DOCUMENT         (yelp_document_get_type ())
#define YELP_DOCUMENT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_DOCUMENT, YelpDocument))
#define YELP_DOCUMENT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_DOCUMENT, YelpDocumentClass))
#define YELP_IS_DOCUMENT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_DOCUMENT))
#define YELP_IS_DOCUMENT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_DOCUMENT))
#define YELP_DOCUMENT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_DOCUMENT, YelpDocumentClass))

typedef struct _YelpDocument      YelpDocument;
typedef struct _YelpDocumentClass YelpDocumentClass;
typedef struct _YelpDocumentPriv  YelpDocumentPriv;

/* This needs to be after the typedefs. */
#include "yelp-page.h"

typedef enum {
    YELP_DOCUMENT_SIGNAL_PAGE,
    YELP_DOCUMENT_SIGNAL_TITLE,
    YELP_DOCUMENT_SIGNAL_ERROR
} YelpDocumentSignal;

typedef void         (*YelpDocumentFunc)     (YelpDocument       *document,
					      YelpDocumentSignal  signal,
					      gint                req_id,
					      gpointer            func_data,
					      gpointer            user_data);

struct _YelpDocument {
    GObject           parent;
    YelpDocumentPriv *priv;
};

struct _YelpDocumentClass {
    GObjectClass    parent_class;

    /* Virtual Functions */
    gint          (*get_page)                (YelpDocument     *document,
					      gchar            *page_id,
					      YelpDocumentFunc  func,
					      gpointer          user_data);
    void          (*release_page)            (YelpDocument     *document,
					      YelpPage         *page);
    void          (*cancel_page)             (YelpDocument     *document,
					      gint              req_id);
};


GType             yelp_document_get_type     (void);

gint              yelp_document_get_page     (YelpDocument       *document,
					      gchar              *page_id,
					      YelpDocumentFunc    func,
					      gpointer           *user_data);
void              yelp_document_release_page (YelpDocument       *document,
					      YelpPage           *page);
void              yelp_document_cancel_page  (YelpDocument       *document,
					      gint                req_id);

#endif /* __YELP_DOCUMENT_H__ */
