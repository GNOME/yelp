/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Mikael Hallendal <micke@codefactory.se>
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
#include <libgtkhtml/gtkhtml.h>

#include <string.h>
#include <stdio.h>

#include "yelp-util.h"
#include "yelp-marshal.h"
#include "yelp-db2html.h"
#include "yelp-html.h"

#define d(x)

typedef struct _StreamData StreamData;

static void yh_init                  (YelpHtml           *html);
static void yh_class_init            (YelpHtmlClass      *klass);
static void yh_link_clicked_cb       (HtmlDocument          *doc, 
				      const gchar           *url, 
				      YelpHtml              *html);
static void yh_stream_cancel         (HtmlStream            *stream, 
				      gpointer               user_data, 
				      gpointer               cancel_data);
static void yh_url_requested_cb      (HtmlDocument          *doc,
				      const gchar           *uri,
				      HtmlStream            *stream,
				      gpointer               data);

#define BUFFER_SIZE 16384

enum {
	URL_SELECTED,
	LAST_SIGNAL
};

static gint        signals[LAST_SIGNAL] = { 0 };
static GHashTable *cache_table = NULL;

struct _YelpHtmlPriv {
	HtmlView     *view;

        HtmlDocument *doc;
	HtmlDocument *load_doc;
        GSList       *connections;
	gchar        *base_uri;
};

typedef struct {
	YelpHtml       *html;
	gchar          *buffer;
	GnomeVFSHandle *handle;
	HtmlStream     *stream;
	gboolean        is_doc;
} ReadData;

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
                                (GClassInitFunc) yh_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpHtml),
                                0,
                                (GInstanceInitFunc) yh_init,
                        };
                
                view_type = g_type_register_static (G_TYPE_OBJECT,
                                                    "YelpHtml", 
                                                    &view_info, 0);
        }
        
        return view_type;
}

static void
yh_init (YelpHtml *html)
{
        YelpHtmlPriv *priv;

        priv = g_new0 (YelpHtmlPriv, 1);

	priv->view        = HTML_VIEW (html_view_new ());
        priv->doc         = html_document_new ();
        priv->connections = NULL;
        priv->base_uri    = g_strdup ("");
	priv->load_doc    = html_document_new ();
	
	html_document_open_stream (priv->load_doc, "text/html");
	
	{
		gint len;
		gchar *str = _("Loading...");
		gchar *text = g_strdup_printf ("<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"></head><body bgcolor=\"white\"><center>%s</center></body></html>", str);
		len = strlen (text);

		html_document_write_stream (priv->load_doc, text, len);
		g_free (text);
	}
	
	html_document_close_stream (priv->load_doc);

        html_view_set_document (HTML_VIEW (priv->view), priv->doc);
        
        g_signal_connect (G_OBJECT (priv->doc), "link_clicked",
                          G_CALLBACK (yh_link_clicked_cb), html);
        
        g_signal_connect (G_OBJECT (priv->doc), "request_url",
                          G_CALLBACK (yh_url_requested_cb), html);

        html->priv = priv;
}

static void
yh_class_init (YelpHtmlClass *klass)
{
	signals[URL_SELECTED] = 
		g_signal_new ("url_selected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpHtmlClass,
					       url_selected),
			      NULL, NULL,
			      yelp_marshal_VOID__STRING_STRING_BOOLEAN,
			      G_TYPE_NONE,
			      3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
	
	if (cache_table == NULL) {
		cache_table = g_hash_table_new (g_str_hash, g_str_equal);
	}
}

static int 
yelp_html_do_write (void * context, const char * buffer, int len)
{
	YelpHtml     *html;
	YelpHtmlPriv *priv;
	
	g_return_val_if_fail (YELP_IS_HTML (context), -1);
	
	html = YELP_HTML (context);
	priv = html->priv;

	if (len <= 0) {
		return 0;
	}
	
	d(g_print ("Do Write: %d\n", len));
	
	html_document_write_stream (priv->doc, buffer, len);
	
	return len;
}

static int
yelp_html_do_close (void *context)
{
	YelpHtml     *html;
	YelpHtmlPriv *priv;
	
	g_return_val_if_fail (YELP_IS_HTML (context), -1);
	
	html = YELP_HTML (context);
	priv = html->priv;

	d(g_print ("Do Close\n"));

	html_document_close_stream (priv->doc);
	gtk_adjustment_set_value (gtk_layout_get_vadjustment (GTK_LAYOUT (priv->view)),
				  0);
	return 0;
}

static void
yelp_html_do_docbook (YelpHtml *html, const gchar *uri)
{
	xmlOutputBufferPtr  buf;
	YelpHtmlPriv       *priv;

	g_return_if_fail (YELP_IS_HTML (html));
	
	priv = html->priv;

	buf = xmlAllocOutputBuffer (NULL);
		
	buf->writecallback = yelp_html_do_write;
	buf->closecallback = yelp_html_do_close;
	buf->context       = html;
		
	yelp_db2html_convert (uri, buf, NULL);
}

static gboolean
yelp_html_idle_read (gpointer data)
{
	ReadData         *read_data = (ReadData *)data;
	GnomeVFSFileSize  read_len;
	GnomeVFSResult    result;
	
	result = gnome_vfs_read (read_data->handle, 
				 read_data->buffer,
				 BUFFER_SIZE, 
				 &read_len);

	if (result == GNOME_VFS_OK) {
		g_print ("-> ");
		
		yelp_html_do_write (read_data->html, 
				    read_data->buffer, 
				    read_len);
	} else {
		return FALSE;
	}

	return TRUE;
}

static void
yelp_html_idle_read_end (gpointer data)
{
	ReadData     *read_data = (ReadData *)data;
	YelpHtml     *html;
	YelpHtmlPriv *priv;
	
	html = read_data->html;
	priv = html->priv;
	
	gnome_vfs_close (read_data->handle);
 	gnome_vfs_handle_destroy (read_data->handle);
	
	html_stream_close (read_data->stream);

	d(g_print ("Close\n"));

	if (read_data->is_doc) {
		gtk_adjustment_set_value (
			gtk_layout_get_vadjustment (GTK_LAYOUT (priv->view)),
			0);
	}

	g_free (read_data);
}

static void
yelp_html_do_non_docbook (YelpHtml *html, HtmlStream *stream, 
			  const gchar *uri, gboolean is_doc)
{
	GnomeVFSHandle *handle;
	GnomeVFSResult  result;
	gchar           buffer[BUFFER_SIZE];
	GTimer         *timer;
	ReadData       *read_data;
	
	g_return_if_fail (YELP_IS_HTML (html));
	
	d(g_print ("Non docbook: %s\n", uri));

	timer = g_timer_new ();

	result = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);

	d(g_print ("Opening took: %f\n", g_timer_elapsed (timer, 0)));
	
	if (result != GNOME_VFS_OK) {
		g_warning ("Failed to open: %s", uri);
		return;
	}

	g_timer_start (timer);

	read_data = g_new0 (ReadData, 1);
	
	read_data->html      = html;
	read_data->buffer    = buffer;
	read_data->handle    = handle;
	read_data->stream    = stream;
	read_data->is_doc    = is_doc;

	g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, 
			 yelp_html_idle_read,
			 read_data,
			 yelp_html_idle_read_end);
}

static void
yh_url_requested_cb (HtmlDocument *doc,
		     const gchar  *uri,
		     HtmlStream   *stream,
		     gpointer      data)
{
        YelpHtml         *html;
	GnomeVFSHandle   *handle;
	GnomeVFSResult    result;
	gchar             buffer[BUFFER_SIZE];
	GnomeVFSFileSize  read_len;

	d(g_print ("URL REQUESTED: %s\n", uri));

        html = YELP_HTML (data);

	html_stream_set_cancel_func (stream, yh_stream_cancel, html);

	result = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);
	
	if (result != GNOME_VFS_OK) {
		g_warning ("Failed to open: %s", uri);
		return;
	}

	while (gnome_vfs_read (handle, buffer, BUFFER_SIZE, &read_len) ==
	       GNOME_VFS_OK) {
		html_stream_write (stream, buffer, read_len);
	}

	gnome_vfs_close (handle);

/*  	yelp_html_do_non_docbook (html, stream, uri, FALSE); */
}

static void
yh_stream_cancel (HtmlStream *stream, 
		  gpointer    user_data, 
		  gpointer    cancel_data)
{
	d(g_print ("CANCEL!!\n"));

	/* Not sure what to do here */
}

static void
yh_link_clicked_cb (HtmlDocument *doc, const gchar *url, YelpHtml *html)
{
	gboolean handled;

	g_return_if_fail (HTML_IS_DOCUMENT (doc));
	g_return_if_fail (url != NULL);
	g_return_if_fail (YELP_IS_HTML (html));

	handled = FALSE;

	/* If this is a relative reference. Shortcut reload. */
	if (url && (url[0] == '#' || url[0] == '?')) {
		html_view_jump_to_anchor (HTML_VIEW (html->priv->view),
 					  &url[1]);
		handled = TRUE;
	}
	
	d(g_print ("link clicked: URL=%s baseUri=%s\n", url,
		   html->priv->base_uri));

	g_signal_emit (html, signals[URL_SELECTED], 0,
		       url, html->priv->base_uri, handled);
}

YelpHtml *
yelp_html_new (void)
{
        YelpHtml *html;

        d(puts(G_GNUC_FUNCTION));

        html = g_object_new (YELP_TYPE_HTML, NULL);

	html_document_open_stream (html->priv->doc, "text/html");
	
	{
		gint len;
		gchar *text = "<html><body bgcolor=\"white\"><h1>Yelp</h1></body></html>";
		len = strlen (text);

		html_document_write_stream (html->priv->doc, text, len);
	}
	
	html_document_close_stream (html->priv->doc);

        return html;
}

void
yelp_html_open_uri (YelpHtml    *html, 
		    const gchar *str_uri,
		    const gchar *reference)
{
        YelpHtmlPriv *priv;
	
	d(g_print ("Open URI: %s\n", str_uri));

	g_return_if_fail (YELP_IS_HTML (html));
	g_return_if_fail (str_uri != NULL);

        priv = html->priv;

        html_document_clear (priv->doc);
        html_document_open_stream (priv->doc, "text/html");
	html_stream_set_cancel_func (priv->doc->current_stream,
 				     yh_stream_cancel,
 				     html);
	gtk_adjustment_set_value (
		gtk_layout_get_vadjustment (
			GTK_LAYOUT (priv->view)), 0);
	gtk_adjustment_set_value (
		gtk_layout_get_hadjustment (
			GTK_LAYOUT (priv->view)), 0);

	if (!strncmp (str_uri, "ghelp:", 6)) {
		yelp_html_do_docbook (html, str_uri + 6);
		/* Docbook or HTML */
		return;
	}

	yelp_html_do_non_docbook (html, priv->doc->current_stream, 
				  str_uri, TRUE);
}

void
yelp_html_cancel_loading (YelpHtml *html)
{
}

GtkWidget *
yelp_html_get_widget (YelpHtml *html)
{
	g_return_val_if_fail (YELP_IS_HTML (html), NULL);
	
	return GTK_WIDGET (html->priv->view);
}

