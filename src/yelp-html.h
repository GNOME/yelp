/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@codefactory.se>
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
 * Author: Mikael Hallendal <micke@codefactory.se>
 */

#ifndef __YELP_HTML_H__
#define __YELP_HTML_H__

#include <gtk/gtkobject.h>
#include <gtk/gtktypeutils.h>
#include <gtk/gtkmarshal.h>
#include "yelp-section.h"

#define YELP_TYPE_HTML        (yelp_html_get_type ())
#define YELP_HTML(o)          (GTK_CHECK_CAST ((o), YELP_TYPE_HTML, YelpHtml))
#define YELP_HTML_CLASS(k)    (GTK_CHECK_FOR_CAST((k), YELP_TYPE_HTML, YelpHtmlClass))
#define YELP_IS_HTML(o)       (GTK_CHECK_TYPE ((o), YELP_TYPE_HTML))
#define YELP_IS_HTML_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), YELP_TYPE_HTML))

typedef struct _YelpHtml        YelpHtml;
typedef struct _YelpHtmlClass   YelpHtmlClass;
typedef struct _YelpHtmlPriv    YelpHtmlPriv;

struct _YelpHtml {
	GObject       parent;
	
	YelpHtmlPriv *priv;
};

struct _YelpHtmlClass {
        GObjectClass  parent_class;

	/* Signals */
	void (*uri_selected)   (YelpHtml  *view,
				YelpURI   *uri,
				gboolean   handled);
};

GType           yelp_html_get_type       (void);
YelpHtml *      yelp_html_new            (void);

void            yelp_html_set_base_uri   (YelpHtml    *html,
					  YelpURI     *uri);
void            yelp_html_clear          (YelpHtml    *html);
void            yelp_html_write          (YelpHtml    *html,
					  gint         len,
					  const gchar *data);
void            yelp_html_close          (YelpHtml    *html);

GtkWidget *     yelp_html_get_widget     (YelpHtml      *html);

#endif /* __YELP_HTML_H__ */

