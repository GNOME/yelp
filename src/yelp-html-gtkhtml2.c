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
#include <libgtkhtml/gtkhtml.h>
#include <libgtkhtml/dom/traversal/dom-nodeiterator.h>
#include <libgtkhtml/dom/traversal/dom-documenttraversal.h>
#include <libgtkhtml/view/htmlselection.h>
#include <libgtkhtml/layout/htmlboxtext.h>

#include <string.h>
#include <stdio.h>

#include "yelp-util.h"
#include "yelp-marshal.h"
#include "yelp-error.h"
#include "yelp-uri.h"
#include "yelp-html.h"

#define d(x)

struct _YelpHtmlPriv {
	HtmlView        *view;

        HtmlDocument    *doc;
	YelpURI         *base_uri;

	DomNodeIterator *find_iter;
	DomNode         *find_node;
	gint             find_offset;
};


static void      html_init               (YelpHtml           *html);
static void      html_class_init         (YelpHtmlClass      *klass);

static void      html_url_requested_cb   (HtmlDocument       *doc,
					  const gchar        *uri,
					  HtmlStream         *stream,
					  gpointer            data);
static void      html_cancel_stream      (HtmlStream         *stream, 
					  gpointer            user_data, 
					  gpointer            cancel_data);

static void      html_link_clicked_cb    (HtmlDocument       *doc, 
					  const gchar        *url, 
					  YelpHtml           *html);
static void      html_title_changed_cb   (HtmlDocument       *doc,
					  const gchar        *new_title,
					  YelpHtml           *html);
static void      html_clear_find_data    (YelpHtml           *html);

#define BUFFER_SIZE 16384

enum {
	URI_SELECTED,
	TITLE_CHANGED,
	LAST_SIGNAL
};

static gint        signals[LAST_SIGNAL] = { 0 };

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

	priv->view        = HTML_VIEW (html_view_new ());
        priv->doc         = html_document_new ();
        priv->base_uri    = NULL;

        html_view_set_document (HTML_VIEW (priv->view), priv->doc);
        
        g_signal_connect (G_OBJECT (priv->doc), "link_clicked",
                          G_CALLBACK (html_link_clicked_cb), html);
        
        g_signal_connect (G_OBJECT (priv->doc), "request_url",
                          G_CALLBACK (html_url_requested_cb), html);

        g_signal_connect (G_OBJECT (priv->doc), "title_changed",
                          G_CALLBACK (html_title_changed_cb), html);

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

	signals[TITLE_CHANGED] = 
		g_signal_new ("title_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpHtmlClass,
					       title_changed),
			      NULL, NULL,
			      yelp_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);
}

static void
html_url_requested_cb (HtmlDocument *doc,
		       const gchar  *url,
		       HtmlStream   *stream,
		       gpointer      data)
{
	YelpHtmlPriv     *priv;
        YelpHtml         *html;
	GnomeVFSHandle   *handle;
	GnomeVFSResult    result;
	gchar             buffer[BUFFER_SIZE];
	GnomeVFSFileSize  read_len;
	gchar            *absolute_url;
	
        html = YELP_HTML (data);
	priv = html->priv;

	/* Reset find data when we load a new page. */
	html_clear_find_data (html);
	
	html_stream_set_cancel_func (stream, html_cancel_stream, html);

	d(g_print ("URL REQUESTED: %s\n", url));

	absolute_url = yelp_util_resolve_relative_url (yelp_uri_get_path (priv->base_uri), url);
	
	result = gnome_vfs_open (&handle, absolute_url, GNOME_VFS_OPEN_READ);

	if (result != GNOME_VFS_OK) {
		g_warning ("Failed to open: %s", absolute_url);
		g_free (absolute_url);

		return;
	}

	g_free (absolute_url);

	while (gnome_vfs_read (handle, buffer, BUFFER_SIZE, &read_len) ==
	       GNOME_VFS_OK) {
		html_stream_write (stream, buffer, read_len);
	}

	gnome_vfs_close (handle);
}

static void
html_cancel_stream (HtmlStream *stream, 
		    gpointer    user_data, 
		    gpointer    cancel_data)
{
	d(g_print ("CANCEL!!\n"));

	/* Not sure what to do here */
}

static void
html_link_clicked_cb (HtmlDocument *doc, const gchar *url, YelpHtml *html)
{
	YelpHtmlPriv *priv;
	gboolean      handled;
	YelpURI      *uri;

	g_return_if_fail (HTML_IS_DOCUMENT (doc));
	g_return_if_fail (url != NULL);
	g_return_if_fail (YELP_IS_HTML (html));

	priv    = html->priv;
	handled = FALSE;

	d(g_print ("Link clicked: %s\n", url));

	uri = yelp_uri_get_relative (priv->base_uri, url);

	d(g_print ("That would be: %s\n", yelp_uri_to_string (uri)));
	
	/* If this is a relative reference. Shortcut reload. */
	if (yelp_uri_equal_path (uri, priv->base_uri) &&
	    yelp_uri_get_section (uri)) {
		if (yelp_uri_get_type (uri) == YELP_URI_TYPE_HTML ||
		    yelp_uri_get_type (uri) == YELP_URI_TYPE_MAN) {
			html_view_jump_to_anchor (HTML_VIEW (html->priv->view),
						  yelp_uri_get_section (uri));
			handled = TRUE;
		}
	}

	/* Reset find data when we load a new page. */
	html_clear_find_data (html);

	g_signal_emit (html, signals[URI_SELECTED], 0, uri, handled);

	yelp_uri_unref (uri);
}

static void
html_title_changed_cb (HtmlDocument *doc, 
		       const gchar  *new_title,
		       YelpHtml     *html)
{
	g_return_if_fail (HTML_IS_DOCUMENT (doc));
	g_return_if_fail (new_title != NULL);
	
	g_signal_emit (html, signals[TITLE_CHANGED], 0, new_title);
}

YelpHtml *
yelp_html_new (void)
{
        YelpHtml *html;
	
        d(puts(G_GNUC_FUNCTION));

        html = g_object_new (YELP_TYPE_HTML, NULL);
	
 	yelp_html_clear (html);
	
 	{
 		gchar *text = "<html><body></body></html>";

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

	priv->base_uri = yelp_uri_ref (uri);
}

void
yelp_html_clear (YelpHtml *html)
{
	YelpHtmlPriv *priv;
	
	g_return_if_fail (YELP_IS_HTML (html));

	priv = html->priv;

	html_document_clear (priv->doc);
	html_document_open_stream (priv->doc, "text/html");
	html_stream_set_cancel_func (priv->doc->current_stream,
 				     html_cancel_stream,
 				     html);
}

void
yelp_html_write (YelpHtml *html, const gchar *data, gint len)
{
	YelpHtmlPriv *priv;
	
	g_return_if_fail (YELP_IS_HTML (html));
	
	priv = html->priv;
	
	if (len == -1) {
		len = strlen (data);
	}
	
	if (len <= 0) {
		return;
	}
	
	html_document_write_stream (priv->doc, data, len);
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
	
	html_document_close_stream (priv->doc);

	gtk_adjustment_set_value (gtk_layout_get_vadjustment (GTK_LAYOUT (priv->view)),
				  0);
}

GtkWidget *
yelp_html_get_widget (YelpHtml *html)
{
	g_return_val_if_fail (YELP_IS_HTML (html), NULL);
	
	return GTK_WIDGET (html->priv->view);
}

static void
html_clear_find_data (YelpHtml *html)
{
	YelpHtmlPriv *priv;

	priv = html->priv;
	
	/* Reset find data when we load a new page. */
	if (priv->find_iter) {
		g_object_unref (priv->find_iter);
		priv->find_iter = NULL;
	}
}

/* This code is really ugly, need to clean up. */
void
yelp_html_find (YelpHtml    *html,
		const gchar *find_string,
		gboolean     match_case,
		gboolean     wrap,
		gboolean     forward)
{
	YelpHtmlPriv    *priv;
	DomNode         *root;
	DomNode         *tmp_node;
	DomNode         *node;
	DomNodeIterator *iter;
	gchar           *str;
	gchar           *hit;
	gchar           *haystack;

	HtmlBox         *box;
	gchar           *box_text;
	gint             len;
 
	g_return_if_fail (YELP_IS_HTML (html));
	g_return_if_fail (find_string != NULL);

	priv = html->priv;

	if (!priv->find_iter) {
		iter = dom_DocumentTraversal_createNodeIterator (
			DOM_DOCUMENT_TRAVERSAL (priv->doc->dom_document),
			DOM_NODE (priv->doc->dom_document),
			DOM_SHOW_ALL,
			NULL,
			FALSE,
			NULL);

		root = NULL;
		while ((node = dom_NodeIterator_nextNode (iter, NULL))) {
			if (!g_ascii_strcasecmp (dom_Node__get_nodeName (node), "body")) {
				root = node;
				break;
			}
		}
		
		g_object_unref (iter);
		iter = NULL;
	
		if (!root) {
			g_warning ("html_find(): Couldn't find html body.");
			return;
		}
		
		priv->find_iter = dom_DocumentTraversal_createNodeIterator (
			DOM_DOCUMENT_TRAVERSAL (priv->doc->dom_document),
			root,
			DOM_SHOW_TEXT,
			NULL,
			FALSE,
			NULL);

		if (forward) {
			priv->find_node = dom_NodeIterator_nextNode (priv->find_iter, NULL);
			priv->find_offset = 0;
		} else {
			do {
				tmp_node = priv->find_node;
				priv->find_node = dom_NodeIterator_nextNode (priv->find_iter, NULL);
			} while (priv->find_node);

			priv->find_node = tmp_node;

			if (priv->find_node) {
				box = html_view_find_layout_box (priv->view, priv->find_node, FALSE);
				box_text = html_box_text_get_text (HTML_BOX_TEXT (box), &len);
				priv->find_offset = len;
			}
		}
	}
	
	while (priv->find_node) {
		box = html_view_find_layout_box (priv->view, priv->find_node, FALSE);

		/* We get the text from the layout box instead from the DOM node
		 * directly, since the text in the box is canonicalized
		 * (whitespace stripped in places etc).
		 */
		box_text = html_box_text_get_text (HTML_BOX_TEXT (box), &len);
		if (len) {
			str = g_new (gchar, len + 1);
			memcpy (str, box_text, sizeof (gchar) * len);
			str[len] = 0;
		} else {
			str = g_strdup ("");
		}

		if (!match_case) {
			if (forward) {
				haystack = g_utf8_casefold (str + priv->find_offset, len - priv->find_offset);
			} else {
				haystack = g_utf8_casefold (str, priv->find_offset);
			}
		} else {
			if (forward) {
				haystack = g_strdup (str + priv->find_offset);
			} else {
				haystack = g_strndup (str, priv->find_offset);
			}
		}

		if (forward) {
			hit = strstr (haystack, find_string);
		} else {
			hit = g_strrstr_len (haystack,
					     priv->find_offset,
					     find_string);
		}

		if (hit) {
			if (forward) {
				html_selection_set (priv->view,
						    priv->find_node,
						    hit - haystack + priv->find_offset,
						    strlen (find_string));
				priv->find_offset += hit - haystack + strlen (find_string);
			} else {
				html_selection_set (priv->view,
						    priv->find_node,
						    hit - haystack,
						    strlen (find_string));
				priv->find_offset = hit - haystack;
			}
				
			html_view_scroll_to_node (priv->view,
						  priv->find_node,
						  HTML_VIEW_SCROLL_TO_TOP);
		}

		g_free (str);
		g_free (haystack);
		
		if (hit) {
			break;
		}
		
		if (forward) {
			priv->find_node = dom_NodeIterator_nextNode (priv->find_iter, NULL);
			priv->find_offset = 0;
		} else {
			priv->find_node = dom_NodeIterator_previousNode (priv->find_iter, NULL);
			if (priv->find_node) {
				box = html_view_find_layout_box (priv->view, priv->find_node, FALSE);
				box_text = html_box_text_get_text (HTML_BOX_TEXT (box), &len);
				priv->find_offset = len;
			}
		}			
	}

	if (!priv->find_node) {
		g_object_unref (priv->find_iter);
		priv->find_iter = NULL;
		priv->find_offset = 0;
		html_selection_clear (priv->view);
	}
}

