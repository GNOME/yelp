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
#include <stdio.h>
#include "yelp-view-doc.h"

#define d(x) x

typedef struct _StreamData StreamData;

static void yvd_init                  (YelpViewDoc           *html);
static void yvd_class_init            (YelpViewDocClass      *klass);
static void yvd_async_open_cb         (GnomeVFSAsyncHandle   *handle, 
				       GnomeVFSResult         result, 
				       gpointer               callback_data);
static void yvd_async_read_cb         (GnomeVFSAsyncHandle   *handle, 
				       GnomeVFSResult         result,
				       gpointer               buffer, 
				       GnomeVFSFileSize       bytes_requested,
				       GnomeVFSFileSize       bytes_read, 
				       gpointer               callback_data);
static void yvd_async_close_cb        (GnomeVFSAsyncHandle   *handle,
				       GnomeVFSResult         result,
				       gpointer               callback_data);
static void yvd_link_clicked_cb       (HtmlDocument          *doc, 
				       const gchar           *url, 
				       gpointer               data);
static void yvd_free_stream_data      (StreamData            *sdata,
				       gboolean               remove);
static void yvd_stream_cancel         (HtmlStream            *stream, 
				       gpointer               user_data, 
				       gpointer               cancel_data);
static void yvd_url_requested_cb      (HtmlDocument          *doc,
				       const gchar           *uri,
				       HtmlStream            *stream,
				       gpointer               data);

#define BUFFER_SIZE 8192

enum {
	URI_SELECTED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

struct _YelpViewDocPriv {
        HtmlDocument *doc;
        GSList       *connections;
	gchar        *base_uri;
};

struct _StreamData {
	YelpViewDoc            *view;
	HtmlStream          *stream;
	GnomeVFSAsyncHandle *handle;
	gchar               *anchor;
};

GType
yelp_view_doc_get_type (void)
{
        static GType view_type = 0;

        if (!view_type)
        {
                static const GTypeInfo view_info =
                        {
                                sizeof (YelpViewDocClass),
                                NULL,
                                NULL,
                                (GClassInitFunc) yvd_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpViewDoc),
                                0,
                                (GInstanceInitFunc) yvd_init,
                        };
                
                view_type = g_type_register_static (HTML_TYPE_VIEW,
                                                    "YelpViewDoc", 
                                                    &view_info, 0);
        }
        
        return view_type;
}

static void
yvd_init (YelpViewDoc *view)
{
        YelpViewDocPriv *priv;
        
        priv = g_new0 (YelpViewDocPriv, 1);

        priv->doc         = html_document_new ();
        priv->connections = NULL;
        priv->base_uri    = NULL;

        html_view_set_document (HTML_VIEW (view), priv->doc);
        
        g_signal_connect (G_OBJECT (priv->doc), "link_clicked",
                          G_CALLBACK (yvd_link_clicked_cb), view);
        
        g_signal_connect (G_OBJECT (priv->doc), "request_url",
                          G_CALLBACK (yvd_url_requested_cb), view);

        view->priv = priv;
}

static void
yvd_class_init (YelpViewDocClass *klass)
{
	signals[URI_SELECTED] = 
		g_signal_new ("uri_selected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpViewDocClass, uri_selected),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
}


static void
yvd_async_close_cb (GnomeVFSAsyncHandle *handle,
                          GnomeVFSResult       result,
                          gpointer             callback_data)
{
	StreamData *sdata;
	
	d(puts(__FUNCTION__));

	sdata = (StreamData *) callback_data;
	
	if (sdata->anchor) {
		html_view_jump_to_anchor (HTML_VIEW (sdata->view),
 					  sdata->anchor);
	}

	yvd_free_stream_data (sdata, TRUE);
}

static void
yvd_async_read_cb (GnomeVFSAsyncHandle *handle, 
                         GnomeVFSResult       result,
                         gpointer             buffer, 
                         GnomeVFSFileSize     bytes_requested,
                         GnomeVFSFileSize     bytes_read, 
                         gpointer             callback_data)
{
	StreamData *sdata;

        d(puts(__FUNCTION__));

        sdata = (StreamData *) callback_data;

	if (result != GNOME_VFS_OK) {
		gnome_vfs_async_close (handle, 
                                       yvd_async_close_cb, 
                                       sdata);

/* 		yvd_free_stream_data (sdata, TRUE); */
		g_free (buffer);
	} else {
                g_print ("Writing to html stream... ");
		html_stream_write (sdata->stream, buffer, bytes_read);
                g_print ("done\n");
		
		gnome_vfs_async_read (handle, buffer, bytes_requested, 
				      yvd_async_read_cb, sdata);
	}
}

static void
yvd_async_open_cb  (GnomeVFSAsyncHandle *handle, 
                          GnomeVFSResult       result, 
                          gpointer             callback_data)
{
	StreamData *sdata;
        
        d(puts(__FUNCTION__));

        sdata = (StreamData *) callback_data;

	if (result != GNOME_VFS_OK) {
		g_warning ("Open failed: %s.\n", 
                           gnome_vfs_result_to_string (result));

		yvd_free_stream_data (sdata, TRUE);
	} else {
		gchar *buffer;

		buffer = g_malloc (BUFFER_SIZE);

		gnome_vfs_async_read (handle, buffer, 
                                      BUFFER_SIZE, 
                                      yvd_async_read_cb, sdata);
	}
}

static void
yvd_url_requested_cb (HtmlDocument *doc,
                            const gchar  *uri,
                            HtmlStream   *stream,
                            gpointer      data)
{
        YelpViewDoc     *view;
        YelpViewDocPriv *priv;
	GnomeVFSURI  *vfs_uri;
	StreamData   *sdata;

        d(puts(__FUNCTION__));

        view = YELP_VIEW_DOC (data);
        priv = view->priv;

/* 	if (priv->base_uri) { */
/* 		vfs_uri = gnome_vfs_uri_resolve_relative (priv->base_uri, uri); */
/*         } else { */
/* 		vfs_uri = gnome_vfs_uri_new (uri); */
/*         } */

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
                                  yvd_async_open_cb, 
                                  sdata);

	gnome_vfs_uri_unref (vfs_uri);

	html_stream_set_cancel_func (stream, yvd_stream_cancel, sdata);
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
yvd_stream_cancel (HtmlStream *stream, 
                         gpointer    user_data, 
                         gpointer    cancel_data)
{
	StreamData *sdata = (StreamData *)cancel_data;

        d(puts(__FUNCTION__));

	gnome_vfs_async_cancel (sdata->handle);

	yvd_free_stream_data (sdata, TRUE);
}

static void
yvd_free_stream_data (StreamData *sdata, gboolean remove)
{
        YelpViewDocPriv *priv;
        
        d(puts(__FUNCTION__));

        priv = sdata->view->priv;
        
	if (remove) {
		priv->connections = g_slist_remove (priv->connections, sdata);
	}

	g_print ("Out of stream_data\n");

	if (sdata->anchor) {
		g_free (sdata->anchor);
	}

	html_stream_close(sdata->stream);
	
	g_free (sdata);

}

static void
yvd_link_clicked_cb (HtmlDocument *doc, const gchar *url, gpointer data)
{
	YelpViewDoc     *view;
        YelpViewDocPriv *priv;
	
        view = YELP_VIEW_DOC (data);
        priv = view->priv;

	if (priv->base_uri) {
		g_print ("Link '%s' pressed relative to: %s\n", 
			 url,
			 priv->base_uri);
        } else {
        }

	g_signal_emit (view, signals[URI_SELECTED], 0, url);
}

GtkWidget *
yelp_view_doc_new (void)
{
        YelpViewDoc     *view;
        YelpViewDocPriv *priv;
        
        d(puts(__FUNCTION__));

        view = g_object_new (YELP_TYPE_VIEW_DOC, NULL);
        priv = view->priv;
        
	html_document_open_stream (priv->doc, "text/html");
	{
		int len; 
		gchar *text = N_("<html><head><title>This is Yelp, a help browser for GNOME 2.0</title></head><body bgcolor=\"white\"><h3>Welcome to Yelp!</h3>This is to be a help browser for GNOME 2.0 written by Mikael Hallendal.<br><br>Please read the README and TODO<br></body></html>");
		len = strlen (_(text));
                
		html_document_write_stream (priv->doc, _(text), len);
	}
        
	html_document_close_stream (priv->doc);

        return GTK_WIDGET (view);
}

void
yelp_view_doc_open_section (YelpViewDoc *view, const YelpSection *section)
{
        YelpViewDocPriv *priv;
        StreamData   *sdata;
	GnomeVFSURI  *uri;
	
        d(puts(__FUNCTION__));
	
	g_return_if_fail (YELP_IS_VIEW_DOC (view));
	g_return_if_fail (section != NULL);

        priv = view->priv;

        html_document_clear (priv->doc);
        html_document_open_stream (priv->doc, "text/html");

	gtk_adjustment_set_value (
		gtk_layout_get_vadjustment (GTK_LAYOUT (view)), 0);

	if (priv->base_uri) {
		g_free (priv->base_uri);
	}
	
        priv->base_uri = g_strdup (section->uri);

	sdata          = g_new0 (StreamData, 1);
	sdata->view    = view;
	sdata->stream  = priv->doc->current_stream;
	sdata->anchor  = NULL;
	
	if (section->reference) {
		sdata->anchor = g_strdup (section->reference);
	}
	
	priv->connections = g_slist_prepend (priv->connections, sdata);

	if (section->reference) {
 		gchar *tmp_uri = g_strconcat (section->uri, 
					      section->reference);
		uri = gnome_vfs_uri_new (tmp_uri);
		g_free (tmp_uri);
	} else {
		uri = gnome_vfs_uri_new (section->uri);
	}


	gnome_vfs_async_open_uri (&sdata->handle,
				  uri,
				  GNOME_VFS_OPEN_READ,
				  GNOME_VFS_PRIORITY_DEFAULT,
				  yvd_async_open_cb,
				  sdata);

	gnome_vfs_uri_unref (uri);

	html_stream_set_cancel_func (sdata->stream, 
				     yvd_stream_cancel, 
				     sdata);
}
