/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
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

typedef struct _YelpHtmlBoxNode YelpHtmlBoxNode;

#define YELP_HTML_BOX_NODE(x)  ((YelpHtmlBoxNode *)(x))

struct _YelpHtmlPriv {
    HtmlView        *view;

    HtmlDocument    *doc;
    YelpURI         *base_uri;

    DomNodeIterator *find_iter;
    gint             find_offset;

    gchar           *find_str;

    GList           *find_list;
    GList           *find_elem;
    gboolean         find_is_forward;
};

struct _YelpHtmlBoxNode {
    HtmlBox *find_box;
    glong    find_index;
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

static DomNode * html_get_dom_node       (HtmlDocument       *doc,
					  const gchar        *node_name);
static void      html_clear_box_text     (gpointer            data, 
                                          gpointer            user_data);
static gint      html_cmp_box_text       (gconstpointer       a,
		                          gconstpointer       b);
static void      html_get_box_list       (YelpHtml           *html);

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

    if (!view_type) {
	static const GTypeInfo view_info = {
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

    absolute_url = yelp_util_resolve_relative_url (yelp_uri_get_path (priv->base_uri),
						   url);

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
    gchar        *fragment;

    g_return_if_fail (HTML_IS_DOCUMENT (doc));
    g_return_if_fail (url != NULL);
    g_return_if_fail (YELP_IS_HTML (html));

    priv    = html->priv;
    handled = FALSE;

    d(g_print ("Link clicked: %s\n", url));

    uri = yelp_uri_new_relative (priv->base_uri, url);

    d(g_print ("That would be: %s\n", yelp_uri_to_string (uri)));
	
    /* If this is a relative reference. Shortcut reload. */
    if (yelp_uri_equal_path (uri, priv->base_uri)) {
	fragment = yelp_uri_get_fragment (uri);
	if (fragment) {
	    if (yelp_uri_get_resource_type (uri) == YELP_URI_TYPE_HTML ||
		yelp_uri_get_resource_type (uri) == YELP_URI_TYPE_MAN) {

		html_view_jump_to_anchor (HTML_VIEW (html->priv->view),
					  fragment);
		g_free (fragment);
		handled = TRUE;
	    }
	}
    }

    /* Reset find data when we load a new page. */
    html_clear_find_data (html);

    g_signal_emit (html, signals[URI_SELECTED], 0, uri, handled);

    g_object_unref (uri);
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

    if (priv->base_uri)
	g_object_unref (priv->base_uri);

    g_object_ref (uri);
    priv->base_uri = uri;
}

void
yelp_html_clear (YelpHtml *html)
{
    YelpHtmlPriv *priv;

    g_return_if_fail (YELP_IS_HTML (html));
    html_clear_find_data (html);

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

    if (len == -1)
	len = strlen (data);

    if (len <= 0)
	return;

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
 	html_selection_clear (priv->view);

	if (priv->find_list) {
	    g_list_foreach (priv->find_list, html_clear_box_text, NULL);
	    
	    g_list_free (priv->find_list);

	    priv->find_list = NULL;
	}

	if (priv->find_str) {
	    g_free (priv->find_str);
	    priv->find_str = NULL;
	}

	priv->find_elem = NULL;

	g_object_unref (priv->find_iter);
	priv->find_iter = NULL;
    }
}

static DomNode *
html_get_dom_node (HtmlDocument *doc,
		   const gchar  *node_name)
{
    DomNodeIterator *iter;
    DomNode         *node;

    g_return_val_if_fail (HTML_IS_DOCUMENT (doc), NULL);
    g_return_val_if_fail (node_name != NULL, NULL);

    iter = dom_DocumentTraversal_createNodeIterator
	(DOM_DOCUMENT_TRAVERSAL (doc->dom_document),
	 DOM_NODE (doc->dom_document),
	 DOM_SHOW_ALL,
	 NULL,
	 FALSE,
	 NULL);
	
    while ((node = dom_NodeIterator_nextNode (iter, NULL))) {
	if (!g_ascii_strcasecmp (dom_Node__get_nodeName (node), node_name)) {
	    break;
	}
    }
	
    g_object_unref (iter);

    return node;
}

static void
html_clear_box_text (gpointer data, gpointer user_data)
{
    g_free (data);
}

static gint
html_cmp_box_text (gconstpointer a,
		   gconstpointer b)
{
    YelpHtmlBoxNode *box_node;
    HtmlBox         *html_box;

    box_node = YELP_HTML_BOX_NODE (a);
    html_box = HTML_BOX (b);

    return ! (html_box == box_node->find_box);
}

static void 
html_get_box_list (YelpHtml *html)
{
    YelpHtmlPriv    *priv;
    YelpHtmlBoxNode *box_node;
    DomNode         *node;
    HtmlBox         *html_box;
    gint             len;
    glong            total_len;
    
    g_return_if_fail (YELP_IS_HTML (html));

    priv = html->priv;

    g_return_if_fail (priv->find_iter != NULL);

    node = dom_NodeIterator_nextNode (priv->find_iter, 
				      NULL);

    priv->find_list = NULL;

    while (node) {
	html_box = html_view_find_layout_box (priv->view,
					      node, FALSE);
	total_len = 0;
	while (html_box) {
	    if (HTML_IS_BOX_TEXT (html_box) &&
		(!g_list_find_custom (priv->find_list,
				      html_box,
				      html_cmp_box_text))) {
		box_node             = g_new0 (YelpHtmlBoxNode, 1);
		
		box_node->find_box   = html_box;
		box_node->find_index = total_len;
		priv->find_list      = g_list_append (priv->find_list, box_node);

		html_box_text_get_text (HTML_BOX_TEXT (html_box), &len);

		total_len += len;
	    } else {
		total_len = 0;
	    }
				
	    if (html_box->children)
		html_box = html_box->children;
	    else if (html_box->next)
		html_box = html_box->next;
	    else
		html_box = NULL;
	}
			
	node = dom_NodeIterator_nextNode (priv->find_iter,
					  NULL);
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
    YelpHtmlBoxNode *box_node;
    DomNode         *node;
    gchar           *box_text;
    gchar           *str;
    gchar           *haystack;
    gchar           *hit;
    gint             len;
    gboolean         found;

    g_return_if_fail (YELP_IS_HTML (html));
    g_return_if_fail (find_string != NULL);

    priv = html->priv;

    if (!priv->find_iter) { 
	node = html_get_dom_node (priv->doc, "body");

	if (!node) {
	    g_warning ("html_find(): Couldn't find html body.");  
	    return;
	}

	priv->find_iter = dom_DocumentTraversal_createNodeIterator
	    (DOM_DOCUMENT_TRAVERSAL (priv->doc->dom_document),
	     node,
	     DOM_SHOW_TEXT,
	     NULL,
	     FALSE,
	     NULL);

	html_get_box_list (html);

	if (!forward) {
	    priv->find_list = g_list_reverse (priv->find_list);
	}
	
	priv->find_str        = NULL;
	priv->find_offset     = 0;
	priv->find_elem       = NULL;
	priv->find_is_forward = forward;
    }

    if (!priv->find_elem) { 
	priv->find_elem = priv->find_list; 
    }
    
    if (priv->find_is_forward != forward) {
	box_node = priv->find_elem->data;
	html_box_text_get_text (HTML_BOX_TEXT (box_node->find_box), &len);

	priv->find_offset     = len - priv->find_offset + strlen (priv->find_str);
	priv->find_list       = g_list_reverse (priv->find_list);
	priv->find_is_forward = forward;
    } 

    if (priv->find_str)
	g_free (priv->find_str);
    
    priv->find_str = g_strdup (find_string);

    found = FALSE;

    while (priv->find_elem) {
	box_node = priv->find_elem->data;
	box_text = html_box_text_get_text (HTML_BOX_TEXT (box_node->find_box), &len);
		
	if (len > 0) {
	    str = g_strndup (box_text, len);

	    if (!match_case) {
		haystack = g_utf8_casefold (str, -1);
	    } else {
		haystack = g_strdup (str);
	    }

	    if (forward) {
		hit = strstr (haystack + priv->find_offset, find_string);
	    } else {
		hit = g_strrstr_len (haystack, len - priv->find_offset,
				     find_string);
	    }

	    if (hit) {
		html_selection_set (priv->view,
				    box_node->find_box->dom_node,
				    box_node->find_index + (hit - haystack),
				    strlen (find_string));

		html_view_scroll_to_node (priv->view,
					  box_node->find_box->dom_node,
					  HTML_VIEW_SCROLL_TO_TOP);
		found = TRUE;

		if (forward) {
		    priv->find_offset = (hit - haystack + strlen (find_string));
		}else {
		    priv->find_offset = strlen (hit);
		}
	    }

	    g_free (str);
	    g_free (haystack);
	}

	if (found)
	    break;
		
	priv->find_offset = 0;
	priv->find_elem   = g_list_next (priv->find_elem);
    }

    if (!priv->find_elem) {
	html_selection_clear (priv->view);
    }	
}
