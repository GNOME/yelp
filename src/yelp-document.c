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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "yelp-document.h"
#include "yelp-debug.h"

#define YELP_DOCUMENT_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_DOCUMENT, YelpDocumentPriv))

struct _YelpDocumentPriv {
    gpointer FIXME;
};

static void    document_class_init        (YelpDocumentClass  *klass);
static void    document_init              (YelpDocument       *document);
static void    document_dispose           (GObject            *object);

static GObjectClass *parent_class;

GType
yelp_document_get_type (void)
{
    static GType type = 0;
    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpDocumentClass),
	    NULL, NULL,
	    (GClassInitFunc) document_class_init,
	    NULL, NULL,
	    sizeof (YelpDocument),
	    0,
	    (GInstanceInitFunc) document_init,
	};
	type = g_type_register_static (G_TYPE_OBJECT,
				       "YelpDocument", 
				       &info, 0);
    }
    return type;
}

static void
document_class_init (YelpDocumentClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose      = document_dispose;

    g_type_class_add_private (klass, sizeof (YelpDocumentPriv));
}

static void
document_init (YelpDocument *document)
{
    YelpDocumentPriv *priv;

    document->priv = priv = YELP_DOCUMENT_GET_PRIVATE (document);
}

static void
document_dispose (GObject *object)
{
    YelpDocument *document = YELP_DOCUMENT (object);

    parent_class->dispose (object);
}

/******************************************************************************/

gint
yelp_document_get_page (YelpDocument     *document,
			gchar            *page_id,
			YelpDocumentFunc  func,
			gpointer         *user_data)
{
    YELP_DOCUMENT_GET_CLASS (document)->get_page (document,
						  page_id,
						  func,
						  user_data);
}

void
yelp_document_release_page (YelpDocument *document, YelpPage *page)
{
    // FIXME
}

void
yelp_document_cancel (YelpDocument *document,
		      gint          req_id)
{
    YELP_DOCUMENT_GET_CLASS (document)->cancel (document, req_id);
}
