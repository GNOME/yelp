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
#include "yelp-error.h"
#include "yelp-html.h"

#define d(x)

typedef enum {
	MAN,
	INFO
} DocType;

struct _YelpHtmlPriv {
	HtmlView     *view;

        HtmlDocument *doc;
	HtmlDocument *load_doc;
        GSList       *connections;
	gchar        *base_uri;
};

typedef struct {
	YelpHtml   *html;
	HtmlStream *stream;
	gchar      *section;
} ReadData;

static void      yh_init                  (YelpHtml           *html);
static void      yh_class_init            (YelpHtmlClass      *klass);
static int       yelp_html_do_write       (void               *context,
					   const char         *buffer,
					   int                 len);
static int       yelp_html_do_close       (void               *context);

static void      yelp_html_do_docbook     (YelpHtml           *html,
					   const gchar        *docpath,
					   const gchar        *section,
					   GError            **error);
static gboolean  yelp_html_io_watch_cb    (GIOChannel         *iochannel, 
					   GIOCondition        cond, 
					   ReadData           *read_data);

static void      yelp_html_do_maninfo     (YelpHtml           *html,
					   HtmlStream         *stream,
					   DocType             type, 
					   const gchar        *uri,
					   GError            **error);
static void      yelp_html_do_html        (YelpHtml           *html,
					   HtmlStream         *stream,
					   const gchar        *uri,
					   const gchar        *section,
					   GError            **error);
static void      yh_url_requested_cb      (HtmlDocument       *doc,
					   const gchar        *uri,
					   HtmlStream         *stream,
					   gpointer            data);
static void      yh_stream_cancel         (HtmlStream         *stream, 
					   gpointer            user_data, 
					   gpointer            cancel_data);

static void      yh_link_clicked_cb       (HtmlDocument       *doc, 
					   const gchar        *url, 
					   YelpHtml           *html);

#define BUFFER_SIZE 16384

enum {
	URL_SELECTED,
	LAST_SIGNAL
};

static gint        signals[LAST_SIGNAL] = { 0 };
static GHashTable *cache_table = NULL;

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

	gdk_window_set_cursor (GTK_WIDGET (priv->view)->window, NULL);

	return 0;
}

static void
yelp_html_do_docbook (YelpHtml *html, const gchar *docpath, 
		      const gchar *section, GError **error)
{
	xmlOutputBufferPtr  buf;
	YelpHtmlPriv       *priv;
	gchar              *uri;
	
	g_return_if_fail (YELP_IS_HTML (html));
	
	priv = html->priv;

	uri = g_strconcat (docpath, "?", section, NULL);

	buf = xmlAllocOutputBuffer (NULL);
		
	buf->writecallback = yelp_html_do_write;
	buf->closecallback = yelp_html_do_close;
	buf->context       = html;
		
	yelp_db2html_convert (uri, buf, error);

	g_free (uri);
}

static gboolean
yelp_html_io_watch_cb (GIOChannel   *iochannel, 
		       GIOCondition  cond, 
		       ReadData     *read_data)
{
	YelpHtmlPriv *priv;
	static gchar  buffer[BUFFER_SIZE];
	guint         n;
	GIOError      io_error;
	gboolean      finished = FALSE;

	g_return_val_if_fail (read_data != NULL, FALSE);
	g_return_val_if_fail (YELP_IS_HTML (read_data->html), FALSE);

	priv = read_data->html->priv;

	if (cond & G_IO_IN) {
 		d(g_print ("Read available\n"));
		
		io_error = g_io_channel_read (iochannel, buffer, 
					      BUFFER_SIZE, &n);
		if (io_error != G_IO_ERROR_NONE) {
			g_warning ("Read Error: %d", io_error);
			return FALSE;
		}

		yelp_html_do_write (read_data->html, buffer, n);
		
		if (n < BUFFER_SIZE) {
			finished = TRUE;
		}
	}

	if (cond & G_IO_HUP || finished) {
		html_stream_close (read_data->stream);
		
		d(g_print ("Close\n"));

		g_io_channel_unref (iochannel);
		
		gtk_adjustment_set_value (
			gtk_layout_get_vadjustment (GTK_LAYOUT (priv->view)),
			0);

		gdk_window_set_cursor (GTK_WIDGET (priv->view)->window, NULL);

		if (read_data->section) {
			html_view_jump_to_anchor (priv->view, 
						  read_data->section);
			g_free (read_data->section);
		}

		g_free (read_data);
		return FALSE;
	}

	return TRUE;
}

static void
yelp_html_do_maninfo (YelpHtml *html, HtmlStream *stream,
		      DocType type, const gchar *uri, GError **error)
{
	gchar       *command_line = NULL;
	gchar      **argv = 0;
	gint         output_fd;
	GIOChannel  *io_channel;
	ReadData    *read_data;
	
	d(g_print ("entering from maninfo: %d\n", output_fd));

	switch (type) {
	case MAN:
		command_line = g_strdup_printf ("gnome2-man2html %s", uri);
		break;
	case INFO:
		command_line = g_strdup_printf ("gnome2-info2html %s", uri);
		break;
	default:
		g_warning ("Non-supported doctype");
		return;
	};
	
	if (!g_shell_parse_argv (command_line, NULL, &argv, error)) {
		return;
	}

	g_free (command_line);

	d(g_print ("spawning in maninfo\n"));
	
	if (!g_spawn_async_with_pipes (NULL, argv, NULL, 
				       G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_SEARCH_PATH,
				       NULL, NULL, NULL, NULL, 
				       &output_fd, NULL, error)) {
		return;
	}
	
	d(g_print ("spawned in maninfo: %d\n", output_fd));

	g_strfreev (argv);

	io_channel = g_io_channel_unix_new (output_fd);
	
	read_data = g_new (ReadData, 1);
	read_data->html    = html;
	read_data->stream  = stream;
	read_data->section = NULL;
	
	g_io_add_watch (io_channel, G_IO_IN | G_IO_HUP, 
			(GIOFunc) yelp_html_io_watch_cb,
			read_data);

	d(g_print ("returning from maninfo: %d\n", output_fd));
}

static void
yelp_html_do_html (YelpHtml     *html,
		   HtmlStream   *stream,
		   const gchar  *docpath,
		   const gchar  *section,
		   GError      **error)
{
	GIOChannel *io_channel = NULL;
	ReadData   *read_data;

	d(g_print ("Opening html-file: %s\n", docpath));

	io_channel = g_io_channel_new_file (docpath, "r", error);

	if (!io_channel) {
		return;
	}

	read_data = g_new (ReadData, 1);
	read_data->html = html;
	read_data->stream = stream;
	read_data->section = g_strdup (section);
	
	g_io_add_watch (io_channel, G_IO_IN | G_IO_HUP,
			(GIOFunc) yelp_html_io_watch_cb,
			read_data);
}

static void
yh_url_requested_cb (HtmlDocument *doc,
		     const gchar  *uri,
		     HtmlStream   *stream,
		     gpointer      data)
{
	YelpHtmlPriv     *priv;
        YelpHtml         *html;
	GnomeVFSHandle   *handle;
	GnomeVFSResult    result;
	gchar             buffer[BUFFER_SIZE];
	GnomeVFSFileSize  read_len;
	gchar            *abs_url;
	gchar            *docpath;

        html = YELP_HTML (data);
	priv = html->priv;
	
	html_stream_set_cancel_func (stream, yh_stream_cancel, html);

	abs_url = yelp_util_resolve_relative_uri (priv->base_uri, uri);
	docpath = yelp_util_extract_docpath_from_uri (abs_url);
	
	d(g_print ("URL REQUESTED: %s\n", abs_url));
	g_free (abs_url);

	result = gnome_vfs_open (&handle, docpath, GNOME_VFS_OPEN_READ);

	if (result != GNOME_VFS_OK) {
		g_warning ("Failed to open: %s", docpath);
		g_free (docpath);
		return;
	}
	g_free (docpath);

	while (gnome_vfs_read (handle, buffer, BUFFER_SIZE, &read_len) ==
	       GNOME_VFS_OK) {
		html_stream_write (stream, buffer, read_len);
	}

	gnome_vfs_close (handle);
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
	YelpHtmlPriv *priv;
	gboolean      handled;
	gchar        *abs_url;
	gchar        *section;
	gchar        *docurl;

	g_return_if_fail (HTML_IS_DOCUMENT (doc));
	g_return_if_fail (url != NULL);
	g_return_if_fail (YELP_IS_HTML (html));

	priv    = html->priv;
	handled = FALSE;

	abs_url = yelp_util_resolve_relative_uri (priv->base_uri, url);
	docurl  = yelp_util_split_uri (abs_url, &section);

	d(g_print ("link clicked: ABS Url: %s\n", docurl));
	
	/* If this is a relative reference. Shortcut reload. */
	if (!g_ascii_strcasecmp (docurl, priv->base_uri) &&
	    !strchr (abs_url, '?')) {
		if (section) {
			html_view_jump_to_anchor (HTML_VIEW (html->priv->view),
						  section);
			handled = TRUE;
		}
	}

	g_free (abs_url);
	
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
yelp_html_open_uri (YelpHtml     *html, 
		    const gchar  *str_uri,
		    const gchar  *section,
		    GError      **error)
{
        YelpHtmlPriv *priv;
	GdkCursor    *cursor;
	
	d(g_print ("Open URI: %s with section: %s\n", str_uri, section));

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

	cursor = gdk_cursor_new (GDK_WATCH);
	
	gdk_window_set_cursor (GTK_WIDGET (priv->view)->window, cursor);
	gdk_cursor_unref (cursor);
	
	g_free (priv->base_uri);
	priv->base_uri = g_strdup (str_uri);

	if (!strncmp (str_uri, "man:", 4)) {
		yelp_html_do_maninfo (html, priv->doc->current_stream, MAN,
				      str_uri + 4, error);
	}
	else if (!strncmp (str_uri, "info:", 5)) {
		yelp_html_do_maninfo (html, priv->doc->current_stream, INFO,
				      str_uri + 5, error);
	} else {
		gchar *mime_type = NULL;
		gchar *docpath;

		docpath   = yelp_util_extract_docpath_from_uri (str_uri);
		mime_type = gnome_vfs_get_mime_type (docpath);

		if (mime_type) {
			d(g_print ("mime_type: %s\n", mime_type));
			
			if (!g_strcasecmp (mime_type, "text/sgml") ||
			    !g_strcasecmp (mime_type, "text/xml")) {
				yelp_html_do_docbook (html,
						      docpath, section, error);
			}
			else if (!g_strcasecmp (mime_type, "text/html")) {
				yelp_html_do_html (html,
						   priv->doc->current_stream,
						   docpath, section, error);
			}

			g_free (mime_type);
		}

		d(g_print ("Docpath: %s\n", docpath));
		g_free (docpath);
	}
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
