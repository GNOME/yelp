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
#include <libgnome/gnome-i18n.h>

#include <string.h>
#include <stdio.h>

#include "yelp-util.h"
#include "yelp-marshal.h"
#include "yelp-html.h"

#define d(x)

typedef struct _StreamData StreamData;

static void yh_init                  (YelpHtml           *html);
static void yh_class_init            (YelpHtmlClass      *klass);
static void yh_async_open_cb         (GnomeVFSAsyncHandle   *handle, 
				      GnomeVFSResult         result, 
				      gpointer               callback_data);
static void yh_async_read_cb         (GnomeVFSAsyncHandle   *handle, 
				      GnomeVFSResult         result,
				      gpointer               buffer, 
				      GnomeVFSFileSize       bytes_requested,
				      GnomeVFSFileSize       bytes_read, 
				      gpointer               callback_data);
static void yh_async_close_cb        (GnomeVFSAsyncHandle   *handle,
				      GnomeVFSResult         result,
				      gpointer               callback_data);
static void yh_link_clicked_cb       (HtmlDocument          *doc, 
				      const gchar           *url, 
				      YelpHtml              *html);
static void yh_free_stream_data      (StreamData            *sdata,
				      gboolean               remove);
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

static gint signals[LAST_SIGNAL] = { 0 };

struct _YelpHtmlPriv {
        HtmlDocument *doc;
        GSList       *connections;
	gchar        *base_uri;
};

struct _StreamData {
	YelpHtml            *view;
	HtmlStream          *stream;
	GnomeVFSAsyncHandle *handle;
	gchar               *anchor;
};

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
                
                view_type = g_type_register_static (HTML_TYPE_VIEW,
                                                    "YelpHtml", 
                                                    &view_info, 0);
        }
        
        return view_type;
}

static void
yh_init (YelpHtml *view)
{
        YelpHtmlPriv *priv;
        
        priv = g_new0 (YelpHtmlPriv, 1);

        priv->doc         = html_document_new ();
        priv->connections = NULL;
        priv->base_uri    = g_strdup ("");

        html_view_set_document (HTML_VIEW (view), priv->doc);
        
        g_signal_connect (G_OBJECT (priv->doc), "link_clicked",
                          G_CALLBACK (yh_link_clicked_cb), view);
        
        g_signal_connect (G_OBJECT (priv->doc), "request_url",
                          G_CALLBACK (yh_url_requested_cb), view);

        view->priv = priv;
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
}


static void
yh_async_close_cb (GnomeVFSAsyncHandle *handle,
		   GnomeVFSResult       result,
		   gpointer             callback_data)
{
	StreamData *sdata;
	
	d(puts(G_GNUC_FUNCTION));

	sdata = (StreamData *) callback_data;

	if (sdata->anchor) {
		html_view_jump_to_anchor (HTML_VIEW (sdata->view),
 					  sdata->anchor);
	}

	yh_free_stream_data (sdata, TRUE);
}

static void
yh_async_read_cb (GnomeVFSAsyncHandle *handle, 
		  GnomeVFSResult       result,
		  gpointer             buffer, 
		  GnomeVFSFileSize     bytes_requested,
		  GnomeVFSFileSize     bytes_read, 
		  gpointer             callback_data)
{
	StreamData *sdata;

        d(puts(G_GNUC_FUNCTION));

        sdata = (StreamData *) callback_data;

	if (result != GNOME_VFS_OK) {
		gnome_vfs_async_close (handle, 
                                       yh_async_close_cb, 
                                       sdata);

		g_free (buffer);
	} else {
		html_stream_write (sdata->stream, buffer, bytes_read);
		
		gnome_vfs_async_read (handle, buffer, bytes_requested, 
				      yh_async_read_cb, sdata);
	}
}

static void
yh_async_open_cb  (GnomeVFSAsyncHandle *handle, 
		   GnomeVFSResult       result, 
		   gpointer             callback_data)
{
	StreamData *sdata;
        
        d(puts(G_GNUC_FUNCTION));

        sdata = (StreamData *) callback_data;

	if (result != GNOME_VFS_OK) {
		g_warning ("Open failed: %s.\n", 
                           gnome_vfs_result_to_string (result));

		yh_free_stream_data (sdata, TRUE);
	} else {
		gchar *buffer;

		buffer = g_malloc (BUFFER_SIZE);

		gnome_vfs_async_read (handle, buffer, 
                                      BUFFER_SIZE, 
                                      yh_async_read_cb, sdata);
	}
}

static void
yh_url_requested_cb (HtmlDocument *doc,
		     const gchar  *uri,
		     HtmlStream   *stream,
		     gpointer      data)
{
        YelpHtml     *view;
        YelpHtmlPriv *priv;
	GnomeVFSURI  *vfs_uri;
	StreamData   *sdata;

        d(puts(G_GNUC_FUNCTION));

        view = YELP_HTML (data);
        priv = view->priv;

	g_assert (HTML_IS_DOCUMENT(doc));
	g_assert (stream != NULL);

        sdata         = g_new0 (StreamData, 1);
	sdata->view   = view;
	sdata->stream = stream;
	
        priv->connections = g_slist_prepend (priv->connections, sdata);

	vfs_uri = gnome_vfs_uri_new (uri);

	gnome_vfs_async_open_uri (&sdata->handle, 
                                  vfs_uri, 
                                  GNOME_VFS_OPEN_READ,
				  GNOME_VFS_PRIORITY_DEFAULT,
                                  yh_async_open_cb, 
                                  sdata);

	gnome_vfs_uri_unref (vfs_uri);

	html_stream_set_cancel_func (stream, yh_stream_cancel, sdata);
}

/* static void */
/* kill_old_connections (HtmlDocument *doc) */
/* { */
/* 	GSList *connection_list, *tmp; */

/* 	tmp = connection_list = g_object_get_data (G_OBJECT (doc), "connection_list"); */
/* 	while(tmp) { */

/* 		StreamData *sdata = (StreamData *)tmp->data; */
/* 		gnome_vfs_async_cancel (sdata->handle); */
/* 		free_stream_data (sdata, FALSE); */

/* 		tmp = tmp->next; */
/* 	} */

/* 	g_object_set_data (G_OBJECT (doc), "connection_list", NULL); */
/* 	g_slist_free (connection_list); */
/* } */

static void
yh_stream_cancel (HtmlStream *stream, 
		  gpointer    user_data, 
		  gpointer    cancel_data)
{
	StreamData *sdata = (StreamData *)cancel_data;

        d(puts(G_GNUC_FUNCTION));

	gnome_vfs_async_cancel (sdata->handle);

	yh_free_stream_data (sdata, TRUE);
}

static void
yh_free_stream_data (StreamData *sdata, gboolean remove)
{
        YelpHtmlPriv *priv;
        
        d(puts(G_GNUC_FUNCTION));

        priv = sdata->view->priv;
        
	if (remove) {
		priv->connections = g_slist_remove (priv->connections, sdata);
	}

	if (sdata->anchor) {
		g_free (sdata->anchor);
	}

	html_stream_close(sdata->stream);
	
	g_free (sdata);

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
	if (url && url[0] == '#') {
		html_view_jump_to_anchor (HTML_VIEW (html),
 					  &url[1]);
		handled = TRUE;
	}
	
	g_signal_emit (html, signals[URL_SELECTED], 0,
		       url, html->priv->base_uri, handled);
}

GtkWidget *
yelp_html_new (void)
{
        YelpHtml *view;

        d(puts(G_GNUC_FUNCTION));

        view = g_object_new (YELP_TYPE_HTML, NULL);

	html_document_open_stream (view->priv->doc, "text/html");
	
	{
		gint len;
		gchar *text = "<html><body bgcolor=\"white\"><h1>Yelp</h1></body></html>";
		len = strlen (text);
		
		html_document_write_stream (view->priv->doc, text, len);
	}
	
	html_document_close_stream (view->priv->doc);

        return GTK_WIDGET (view);
}


void
yelp_html_open_uri (YelpHtml    *view, 
		    const gchar *str_uri,
		    const gchar *reference)
{
        YelpHtmlPriv *priv;
        StreamData   *sdata;
	GnomeVFSURI  *uri;
	const char   *fragment;
	
        d(puts(G_GNUC_FUNCTION));
	
	g_return_if_fail (YELP_IS_HTML (view));
	g_return_if_fail (str_uri != NULL);

        priv = view->priv;

	d(g_print ("Trying to open: %s\n", str_uri));
	
        html_document_clear (priv->doc);
        html_document_open_stream (priv->doc, "text/html");

	gtk_adjustment_set_value (
		gtk_layout_get_vadjustment (GTK_LAYOUT (view)), 0);

	if (strcmp (priv->base_uri, str_uri)) {
		g_free (priv->base_uri);
		priv->base_uri = g_strdup (str_uri);
	}

	sdata          = g_new0 (StreamData, 1);
	sdata->view    = view;
	sdata->stream  = priv->doc->current_stream;
	sdata->anchor  = NULL;
	
	priv->connections = g_slist_prepend (priv->connections, sdata);

	if (reference) {
 		gchar *tmp_uri = g_strconcat (str_uri, reference, NULL);
		uri = gnome_vfs_uri_new (tmp_uri);
		g_free (tmp_uri);
	} else {
		uri = gnome_vfs_uri_new (str_uri);
	}

	if (reference) {
		sdata->anchor = g_strdup (reference);
	} else if (uri) {
		fragment = gnome_vfs_uri_get_fragment_identifier (uri);
		if (fragment) {
			sdata->anchor = g_strdup (fragment);
		}
	}
	
	gnome_vfs_async_open_uri (&sdata->handle,
				  uri,
				  GNOME_VFS_OPEN_READ,
				  GNOME_VFS_PRIORITY_DEFAULT,
				  yh_async_open_cb,
				  sdata);

	gnome_vfs_uri_unref (uri);

	html_stream_set_cancel_func (sdata->stream, 
				     yh_stream_cancel, 
				     sdata);
}
