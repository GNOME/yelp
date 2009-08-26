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
#include <gtk/gtk.h>

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

enum {
    YELP_DOCUMENT_COLUMN_ID = 0,
    YELP_DOCUMENT_COLUMN_TITLE,
    YELP_DOCUMENT_NUM_COLUMNS
};

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
    void          (*request)                 (YelpDocument     *document,
					      gint              req_id,
					      gboolean          handled,
					      gchar            *page_id,
					      YelpDocumentFunc  func,
					      gpointer          user_data);
    void          (*cancel)                  (YelpDocument     *document,
					      gint              req_id);

    gint          (*get_page)                (YelpDocument     *document,
					      gchar            *page_id,
					      gpointer          user_data);
    void          (*release_page)            (YelpDocument     *document,
					      YelpPage         *page);
    gpointer      (*get_sections)           (YelpDocument     *document);
};


GType             yelp_document_get_type     (void);

gint              yelp_document_get_page     (YelpDocument       *document,
					      gchar              *page_id,
					      YelpDocumentFunc    func,
					      gpointer            user_data);
void              yelp_document_cancel_page  (YelpDocument       *document,
					      gint                req_id);

/* Only called by yelp_page_free */
void              yelp_document_release_page   (YelpDocument       *document,
					       YelpPage           *page);

/* Only called by subclasses */
void              yelp_document_set_root_id    (YelpDocument       *document,
						gchar              *root_id);
void              yelp_document_add_page_id    (YelpDocument       *document,
						gchar              *id,
						gchar              *page_id);
void              yelp_document_add_prev_id    (YelpDocument       *document,
						gchar              *page_id,
						gchar              *prev_id);
void              yelp_document_add_next_id    (YelpDocument       *document,
						gchar              *page_id,
						gchar              *next_id);
void              yelp_document_add_up_id      (YelpDocument       *document,
						gchar              *page_id,
						gchar              *up_id);
void              yelp_document_add_title      (YelpDocument       *document,
						gchar              *page_id,
						gchar              *title);
void              yelp_document_add_page       (YelpDocument       *document,
						gchar              *page_id,
						const gchar        *contents);
gboolean          yelp_document_has_page       (YelpDocument       *document,
						gchar              *page_id);
void              yelp_document_error_request  (YelpDocument       *document,
						gint                req_id,
						YelpError          *error);
void              yelp_document_error_page     (YelpDocument       *document,
						gchar              *page_id,
						YelpError          *error);
void              yelp_document_error_pending  (YelpDocument       *document,
						YelpError          *error);
GtkTreeModel     *yelp_document_get_sections   (YelpDocument       *document);

void              yelp_document_final_pending  (YelpDocument       *document,
						YelpError          *error);

#endif /* __YELP_DOCUMENT_H__ */
