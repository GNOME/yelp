/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@imendio.com>
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
 * Author: Mikael Hallendal <micke@imendio.com>
 */

#ifndef __YELP_HTML_H__
#define __YELP_HTML_H__

#include <gtkmozembed.h>

#include "yelp-print.h"

G_BEGIN_DECLS

#define YELP_TYPE_HTML         (yelp_html_get_type ())
#define YELP_HTML(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_HTML, YelpHtml))
#define YELP_HTML_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_HTML, YelpHtmlClass))
#define YELP_IS_HTML(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_HTML))
#define YELP_IS_HTML_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_HTML))
#define YELP_HTML_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_HTML, YelpHtmlClass))

typedef struct _YelpHtml        YelpHtml;
typedef struct _YelpHtmlClass   YelpHtmlClass;
typedef struct _YelpHtmlPriv    YelpHtmlPriv;

struct _YelpHtml {
	GtkMozEmbed   parent;

	YelpHtmlPriv *priv;
};

struct _YelpHtmlClass {
        GtkMozEmbedClass parent_class;

	guint font_handler;
	guint color_handler;
	guint a11y_handler;

	/* Signals */
	void (*uri_selected)   (YelpHtml    *view,
				gchar       *uri,
				gboolean     handled);
	gboolean (*frame_selected) (YelpHtml    *view,
				gchar       *uri,
				gboolean    handled);
	void (*title_changed)  (YelpHtml    *view,
				const gchar *new_title);
	void (*popupmenu_requested) (YelpHtml *view,
				     const gchar *link);

};

GType           yelp_html_get_type       (void);
YelpHtml *      yelp_html_new            (void);

void            yelp_html_set_base_uri   (YelpHtml    *html,
					  const gchar *uri);
void            yelp_html_open_stream    (YelpHtml    *html,
					  const gchar *mime);
void            yelp_html_write          (YelpHtml    *html,
					  const gchar *data,
					  gint         len);
void            yelp_html_printf         (YelpHtml    *html, 
					  char        *format, 
					  ...) G_GNUC_PRINTF (2,3);
void            yelp_html_close          (YelpHtml    *html);

void            yelp_html_frames         (YelpHtml    *html,
					  gboolean     enable);

gboolean        yelp_html_find           (YelpHtml    *html,
					  const gchar *str);

gboolean	yelp_html_find_again	 (YelpHtml    *html,					  
					  gboolean     forward);

void		yelp_html_set_find_props (YelpHtml    *html,
					  const char  *str,
					  gboolean     match_case,
					  gboolean     wrap);

void            yelp_html_jump_to_anchor (YelpHtml    *html,
					  gchar       *anchor);

void            yelp_html_copy_selection (YelpHtml    *html);

void            yelp_html_select_all     (YelpHtml    *html);

void            yelp_html_print          (YelpHtml    *html,
					  YelpPrintInfo *info,
					  gboolean preview,
					  gint *npages);
void            yelp_html_preview_end    (YelpHtml    *html);
void            yelp_html_preview_navigate (YelpHtml *html,
					    gint page_no);
gboolean        yelp_html_initialize     (void);
void            yelp_html_shutdown       (void); 

G_END_DECLS

#endif /* __YELP_HTML_H__ */
