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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnome/gnome-i18n.h>
#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-stream.h>

#include <string.h>
#include <stdio.h>

#include "yelp-util.h"
#include "yelp-marshal.h"
#include "yelp-error.h"
#include "yelp-uri.h"
#include "yelp-reader.h"
#include "yelp-html.h"

#define d(x)

struct _YelpHtmlPriv {
	GtkHTML       *html_view;
	GtkHTMLStream *current_stream;
	
	YelpURI       *base_uri;
	YelpReader    *reader;
};


static void      html_init               (YelpHtml           *html);
static void      html_class_init         (YelpHtmlClass      *klass);

static void      html_url_requested_cb   (GtkHTML            *html_view,
					  const gchar        *url,
					  GtkHTMLStream      *stream,
					  YelpHtml           *html);

static void      html_link_clicked_cb    (GtkHTML            *html_view,
					  const gchar        *url,
					  YelpHtml           *html);

#define BUFFER_SIZE 16384

enum {
	URI_SELECTED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

GType
yelp_html_get_type (void)
{
        static GType view_type = 0;

        if (!view_type)
        {
                static const GTypeInfo view_info =
                        {
                                sizeof (YelpHtmlClass),
                                NULL,
                                NULL,
                                (GClassInitFunc) html_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpHtml),
                                0,
                                (GInstanceInitFunc) html_init,
                        };
                
                view_type = g_type_register_static (G_TYPE_OBJECT,
                                                    "YelpHtml", 
                                                    &view_info, 0);
        }
        
        return view_type;
}

static void
html_init (YelpHtml *html)
{
        YelpHtmlPriv *priv;

        priv = g_new0 (YelpHtmlPriv, 1);

	priv->html_view   = GTK_HTML (gtk_html_new ());
        priv->base_uri    = NULL;

        g_signal_connect (G_OBJECT (priv->html_view), "link_clicked",
                          G_CALLBACK (html_link_clicked_cb), 
			  html);
        
        g_signal_connect (G_OBJECT (priv->html_view), "url_requested",
                          G_CALLBACK (html_url_requested_cb), 
			  html);

        html->priv = priv;
}

static void
html_class_init (YelpHtmlClass *klass)
{
	signals[URI_SELECTED] = 
		g_signal_new ("uri_selected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpHtmlClass,
					       uri_selected),
			      NULL, NULL,
			      yelp_marshal_VOID__POINTER_BOOLEAN,
			      G_TYPE_NONE,
			      2, G_TYPE_POINTER, G_TYPE_BOOLEAN);
}

static void
html_url_requested_cb (GtkHTML       *html_view,
		       const gchar   *url,
		       GtkHTMLStream *stream,
		       YelpHtml      *html)
{
	YelpHtmlPriv     *priv;
	GnomeVFSHandle   *handle;
	GnomeVFSResult    result;
	gchar             buffer[BUFFER_SIZE];
	GnomeVFSFileSize  read_len;
/* 	gchar            *absolute_url; */
/* 	gchar            *str_uri; */

	g_return_if_fail (GTK_IS_HTML (html_view));
	g_return_if_fail (YELP_IS_HTML (html));
	
	priv = html->priv;
	
	d(g_print ("URL REQUESTED: %s\n", url));

/* 	str_uri = yelp_uri_to_string (priv->base_uri); */
/* 	absolute_url = yelp_util_resolve_relative_url (str_uri, url); */
/* 	g_free (str_uri); */
	
	result = gnome_vfs_open (&handle, url, GNOME_VFS_OPEN_READ);

	if (result != GNOME_VFS_OK) {
		g_warning ("Failed to open: %s", url);

		return;
	}

	while (gnome_vfs_read (handle, buffer, BUFFER_SIZE, &read_len) ==
	       GNOME_VFS_OK) {
		gtk_html_write (html_view, stream, buffer, read_len);
	}

	gtk_html_stream_close (stream, GTK_HTML_STREAM_OK);
	gnome_vfs_close (handle);
}

static void
html_link_clicked_cb (GtkHTML *html_view, const gchar *url, YelpHtml *html)
{
	YelpHtmlPriv *priv;
	gboolean      handled;
	YelpURI      *uri;

        g_return_if_fail (GTK_IS_HTML (html_view));
	g_return_if_fail (url != NULL);
	g_return_if_fail (YELP_IS_HTML (html));

	priv    = html->priv;
	handled = FALSE;

	d(g_print ("Link clicked: %s\n", url));

	uri = yelp_uri_new (url);

	d(g_print ("That would be: %s\n", yelp_uri_to_string (uri)));
	
	/* If this is a relative reference. Shortcut reload. */
	if (yelp_uri_equal_path (uri, priv->base_uri) &&
	    yelp_uri_get_section (uri)) {
		if (yelp_uri_get_type (uri) == YELP_URI_TYPE_HTML ||
		    yelp_uri_get_type (uri) == YELP_URI_TYPE_MAN) {
			gtk_html_jump_to_anchor (priv->html_view,
                                                 yelp_uri_get_section (uri));
			handled = TRUE;
		}
	}

	g_signal_emit (html, signals[URI_SELECTED], 0, uri, handled);
	
	yelp_uri_unref (uri);
}

YelpHtml *
yelp_html_new (void)
{
        YelpHtml *html;
	
        d(puts(G_GNUC_FUNCTION));

        html = g_object_new (YELP_TYPE_HTML, NULL);
	
 	yelp_html_clear (html);
	
 	{
 		gchar *text = "<html><body bgcolor=\"white\"></body></html>";

 		yelp_html_write (html, text, -1);
 	}
	
 	yelp_html_close (html);

        return html;
}

void
yelp_html_set_base_uri (YelpHtml *html, YelpURI *uri)
{
	YelpHtmlPriv *priv;
	
	g_return_if_fail (YELP_IS_HTML (html));

	priv = html->priv;

	if (priv->base_uri) {
		yelp_uri_unref (priv->base_uri);
	}

	/* FIXME: Is this correct? */
	gtk_html_set_base (priv->html_view, yelp_uri_get_path (uri));

	priv->base_uri = yelp_uri_ref (uri);
}

void
yelp_html_clear (YelpHtml *html)
{
	YelpHtmlPriv *priv;
	
	g_return_if_fail (YELP_IS_HTML (html));

	priv = html->priv;
	
	priv->current_stream = gtk_html_begin (priv->html_view);
}

void
yelp_html_write (YelpHtml *html, const gchar *data, gint len)
{
	YelpHtmlPriv *priv;
	
	g_return_if_fail (YELP_IS_HTML (html));

	priv = html->priv;

	if (!priv->current_stream) {
		g_warning ("You need to call yelp_html_clear before trying to write");
		return;
	}

 	if (len == -1) {
		len = strlen (data);
	}
	
	if (len <= 0) {
		return;
	}
	
	gtk_html_write (priv->html_view, priv->current_stream, data, len);
}

void
yelp_html_printf (YelpHtml *html, char *format, ...)
{
	va_list  args;
	gchar   *string;
	
	g_return_if_fail (format != NULL);
	
	va_start (args, format);
	string = g_strdup_vprintf (format, args);
	va_end (args);
	
	yelp_html_write (html, string, -1);
	
	g_free (string);
}

void
yelp_html_close (YelpHtml *html)
{
	YelpHtmlPriv *priv;
	
	g_return_if_fail (YELP_IS_HTML (html));
	
	priv = html->priv;

	if (!priv->current_stream) {
		g_warning ("You need to call yelp_html_clear before trying to close");
		return;
	}
	
	gtk_html_end (priv->html_view, priv->current_stream,
		      GTK_HTML_STREAM_OK);

	gtk_adjustment_set_value (gtk_layout_get_vadjustment (GTK_LAYOUT (priv->html_view)),
				  0);
}

GtkWidget *
yelp_html_get_widget (YelpHtml *html)
{
	g_return_val_if_fail (YELP_IS_HTML (html), NULL);
	
	return GTK_WIDGET (html->priv->html_view);
}
