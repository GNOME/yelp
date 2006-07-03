/*
 * Copyright (C) 2006 Brent Smith  <gnome@nextreality.net>
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
 * Author: Brent Smith  <gnome@nextreality.net>
 */

#ifndef __YELP_DOCUMENT_H__
#define __YELP_DOCUMENT_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include "yelp-page.h"

G_BEGIN_DECLS

typedef struct _YelpDocument      YelpDocument;
typedef struct _YelpDocumentClass YelpDocumentClass;
typedef struct _YelpDocumentPriv  YelpDocumentPriv;

#define YELP_TYPE_DOCUMENT         (yelp_document_get_type ())
#define YELP_DOCUMENT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_DOCUMENT, YelpDocument))
#define YELP_DOCUMENT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_DOCUMENT, YelpDocumentClass))
#define YELP_IS_DOCUMENT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_DOCUMENT))
#define YELP_IS_DOCUMENT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_DOCUMENT))
#define YELP_DOCUMENT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_DOCUMENT, YelpDocumentClass))

struct _YelpDocument {
	GObject            parent;

	YelpDocumentPriv  *priv;
};

struct _YelpDocumentClass {
	GObjectClass       parent_class;

	/* virtual functions - must be implemented by derived classes */
	/* initiates a page request */
	gboolean          (*get_page)     (YelpDocument *document);

	/* cancels a page request */
	gboolean          (*cancel)       (YelpDocument *document);

	/* gets a #GtkTreeModel which represents all the sections in the
	 * document */
	GtkTreeModel *    (*get_sections) (YelpDocument *document);
};

/* callback function definitions */
typedef void (*YelpPageLoadFunc)  (YelpDocument *document,
                                   gint          req_id,
                                   const gchar  *page_id,
                                   YelpPage     *page,
                                   gpointer      user_data);

typedef void (*YelpPageTitleFunc) (YelpDocument *document,
                                   gint          req_id,
                                   const gchar  *page_id,
                                   const gchar  *title,
                                   gpointer      user_data);

typedef void (*YelpPageErrorFunc) (YelpDocument *document,
                                   gint          req_id,
                                   const gchar  *page_id,
                                   const GError *error,
                                   gpointer      user_data);

/* public methods for YelpDocument Class */
gint           yelp_document_get_page (YelpDocument     *document,
                                       const gchar      *page_id,
                                       YelpPageLoadFunc  page_func,
                                       YelpPageTitleFunc title_func,
                                       YelpPageErrorFunc error_func,
                                       gpointer          user_data);

gboolean       yelp_document_cancel_get (YelpDocument   *document,
                                         gint            source_id);

GtkTreeModel  *yelp_document_get_sections (YelpDocument *document);

void           yelp_document_add_page (YelpDocument *document,
                                       YelpPage     *page);


G_END_DECLS

#endif
