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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtkenums.h>
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

#include "yelp-marshal.h"
#include "yelp-error.h"
#include "yelp-uri.h"
#include "yelp-html.h"

#define d(x)

typedef struct _YelpHtmlBoxNode YelpHtmlBoxNode;

#define YELP_HTML_BOX_NODE(x)  ((YelpHtmlBoxNode *)(x))
#define ADJUSTMENT_TIMEOUT_INTERVAL 200

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

    gchar           *anchor;
};

struct _YelpHtmlBoxNode {
    HtmlBox *find_box;
    glong    find_index;
    gint     find_len;
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
static gint      adjustment_timeout_cb   (gpointer         data);

static DomNode * html_get_dom_node       (HtmlDocument       *doc,
					  const gchar        *node_name);
static void      html_clear_box_text     (gpointer            data, 
                                          gpointer            user_data);
static gint      html_cmp_box_text       (gconstpointer       a,
		                          gconstpointer       b);
static void      html_get_box_list       (YelpHtml           *html);

static void      html_clear_find_data    (YelpHtml           *html);
static gboolean  html_find_text          (YelpHtml           *html,
					  const gchar *       find_string,
					  gboolean            match_case,
					  gboolean            wrap,
					  gboolean            forward);

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

    priv->find_is_forward = TRUE;

    html_view_set_document (HTML_VIEW (priv->view), priv->doc);

    g_signal_connect (G_OBJECT (priv->doc), "link_clicked",
		      G_CALLBACK (html_link_clicked_cb), html);
    g_signal_connect (G_OBJECT (priv->doc), "request_url",
		      G_CALLBACK (html_url_requested_cb), html);
    g_signal_connect (G_OBJECT (priv->doc), "title_changed",
		      G_CALLBACK (html_title_changed_cb), html);
 
    gtk_widget_set_size_request (GTK_WIDGET (priv->view), 300, 200);

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
    YelpURI          *absolute_uri;
    gchar            *absolute_uri_str;

    html = YELP_HTML (data);
    priv = html->priv;

    /* Reset find data when we load a new page. */
    html_clear_find_data (html);

    html_stream_set_cancel_func (stream, html_cancel_stream, html);

    d(g_print ("URL REQUESTED: %s\n", url));

    absolute_uri = yelp_uri_resolve_relative (priv->base_uri, url);
    absolute_uri_str = gnome_vfs_uri_to_string (absolute_uri->uri,
						GNOME_VFS_URI_HIDE_NONE);

    result = gnome_vfs_open (&handle, absolute_uri_str, GNOME_VFS_OPEN_READ);

    if (result != GNOME_VFS_OK) {
	g_warning ("Failed to open: %s", absolute_uri_str);
	yelp_uri_unref (absolute_uri);
	g_free (absolute_uri_str);

	return;
    }

    yelp_uri_unref (absolute_uri);
    g_free (absolute_uri_str);

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
    const gchar  *fragment;
    YelpURIType   res_type;
    const gchar  *uri_path, *base_uri_path;

    g_return_if_fail (HTML_IS_DOCUMENT (doc));
    g_return_if_fail (url != NULL);
    g_return_if_fail (YELP_IS_HTML (html));

    priv    = html->priv;
    handled = FALSE;

    d(g_print ("Link clicked: %s\n", url));

    uri = yelp_uri_resolve_relative (priv->base_uri, url);

    /* If this is a relative reference, shortcut reload.  This only happens for
     * HTML and man, because these are single-page formats where the fragment
     * can't possibly jump to a new chunk.
     */
    res_type = yelp_uri_get_resource_type (uri);
    if (res_type == YELP_URI_TYPE_HTML || res_type == YELP_URI_TYPE_MAN) {
	uri_path      = gnome_vfs_uri_get_path (uri->uri);
	base_uri_path = gnome_vfs_uri_get_path (priv->base_uri->uri);

	if (!strcmp (uri_path, base_uri_path)) {
	    fragment = gnome_vfs_uri_get_fragment_identifier (uri->uri);
	    if (fragment) {
		html_view_jump_to_anchor (HTML_VIEW (html->priv->view),
					  fragment);
		handled = TRUE;
	    }
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

static gint
adjustment_timeout_cb (gpointer data)
{
    YelpHtml *html = YELP_HTML (data);
    YelpHtmlPriv *priv = html->priv;
    GtkAdjustment *adjustment;

    /* gtkhtml registers a relayout callback on a one second timeout which,
     * among other things, focuses the first link on the page, which causes
     * it to scroll to said link.  This causes anchor scrolling not to work,
     * and causes pages without a link at the top to start off scrolled down.
     * As I'm sure you can imagine, this is really annoying.  Here we have my
     * ugly hack wherein I put in my own timeout callback to scroll to where
     * I actually want the page.  In order to make it visually quick but still
     * avoid happening before gtkhtml's timeout, I check relayout_timeout_id
     * on the HtmlView, which is non-zero if there's a timeout still waiting
     * to happen.  If the gtkhtml timeout is still there, this function is
     * readded, and we try again later.
     *
     * There also seems to be an idle function in GtkLayout that triggers the
     * gtkhtml2 relayout function.  So technically, we have a race condition.
     * However, grabbing focus before everything else, regardless of whether
     * or not we readd the timeout function, seems to be fairly reliable.
     */

    gtk_widget_grab_focus (GTK_WIDGET (priv->view));

    if (priv->view->relayout_timeout_id != 0)
	return TRUE;

    adjustment = gtk_layout_get_vadjustment (GTK_LAYOUT (priv->view));
    gtk_adjustment_set_value (adjustment, adjustment->lower);

    adjustment = gtk_layout_get_hadjustment (GTK_LAYOUT (priv->view));
    gtk_adjustment_set_value (adjustment, adjustment->lower);

    if (priv->anchor)
	html_view_jump_to_anchor (HTML_VIEW (priv->view), priv->anchor);

    return FALSE;
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
	yelp_uri_unref (priv->base_uri);

    yelp_uri_ref (uri);
    priv->base_uri = uri;
}

void
yelp_html_clear (YelpHtml *html)
{
    YelpHtmlPriv *priv;

    g_return_if_fail (YELP_IS_HTML (html));
    html_clear_find_data (html);

    priv = html->priv;

    if (priv->anchor) {
	g_free (priv->anchor);
	priv->anchor = NULL;
    }

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

    g_timeout_add (ADJUSTMENT_TIMEOUT_INTERVAL, adjustment_timeout_cb, html);
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

	g_free (priv->find_str);
	priv->find_str = NULL;

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
    g_assert (priv->find_list == NULL);

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
		html_box_text_get_text (HTML_BOX_TEXT (html_box), &len);

		if (len > 0) {
		    box_node             = g_new0 (YelpHtmlBoxNode, 1);
		    
		    box_node->find_box   = html_box;
		    box_node->find_index = total_len;
		    box_node->find_len   = len;
		    
		    priv->find_list      = g_list_append (priv->find_list, box_node);

		    total_len += len;
		}
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

static void
html_find_selection_set (HtmlView *view,
			 DomNode  *node,
			 gint      offset,
			 gint      len)
{
    g_return_if_fail (HTML_IS_VIEW (view));
    g_return_if_fail (DOM_IS_NODE (node));
    g_return_if_fail (offset >= 0);
    g_return_if_fail (len >= 0);

    html_selection_set (view,
			node,
			offset,
			len);

    html_view_scroll_to_node (view,
			      node,
			      HTML_VIEW_SCROLL_TO_TOP);    
}

static void
html_find_reset (YelpHtml *html)
{
    
    YelpHtmlPriv *priv;

    g_return_if_fail (YELP_IS_HTML (html));

    priv = html->priv;

    g_return_if_fail (priv->find_list != NULL);

    priv->find_elem       = priv->find_list;
    priv->find_offset     = 0;
    priv->find_is_forward = TRUE;    

    g_free (priv->find_str);
    priv->find_str = NULL;
}
			 
static gboolean
html_find_text (YelpHtml    *html,
		const gchar *find_string,
		gboolean     match_case,
		gboolean     wrap,
		gboolean     forward)		
{
    YelpHtmlPriv    *priv;
    YelpHtmlBoxNode *box_node;
    gchar           *box_text;
    gchar           *str;
    gchar           *haystack;
    gchar           *hit;
    gchar           *found_str;
    GList           *next_elem;
    GList           *next_list;
    GList           *first_elem;
    gint             len;
    gint             next_offset;
    gboolean         found;

    g_return_val_if_fail (YELP_IS_HTML (html), FALSE);
    g_return_val_if_fail (find_string != NULL, FALSE);

    priv  = html->priv;

    next_list   = priv->find_list;
    next_elem   = priv->find_elem;
    next_offset = priv->find_offset;
    
    if (priv->find_is_forward != forward) {
	next_list = g_list_reverse (next_list);

	if (next_elem) {
	    box_node = next_elem->data;

	    if (priv->find_str) {
		next_offset = box_node->find_len - next_offset + strlen (priv->find_str);
	    } else {
		if (!forward) {
		    next_offset = box_node->find_len;
		} else {
		    g_assert (next_offset == 0);
		}
	    }
	}
    } 

    first_elem = next_elem;
    found      = FALSE;
    found_str  = NULL;

    while (next_elem) {
	box_node = next_elem->data;
	box_text = html_box_text_get_text (HTML_BOX_TEXT (box_node->find_box), &len);

	g_assert (box_node->find_len == len);

	if (len > 0) {
	    str = g_strndup (box_text, len);

	    if (!match_case) {
		haystack = g_utf8_casefold (str, -1);
	    } else {
		haystack = g_strdup (str);
	    }

	    if (forward) {
		hit = strstr (haystack + next_offset, find_string);
	    } else {
		hit = g_strrstr_len (haystack, len - next_offset,
				     find_string);
	    }

	    if (hit) {
		html_find_selection_set (priv->view,
					 box_node->find_box->dom_node,
					 box_node->find_index + (hit - haystack),
					 strlen (find_string));
		found = TRUE;

		if (forward) {
		    next_offset = (hit - haystack + strlen (find_string));
		}else {
		    next_offset = strlen (hit);
		}
		
		found_str = g_strndup (box_text + (hit - haystack), strlen (find_string));
	    }
	    
	    g_free (str);
	    g_free (haystack);
	}

	if (found)
	    break;
		
	next_offset = 0;
	next_elem   = g_list_next (next_elem);

	if (first_elem == next_elem) {
	    next_elem = NULL;
	} else if (!next_elem) {
	    if (wrap) {
		next_elem  = next_list;
 		first_elem = g_list_next (first_elem); 
	    }
	}
    }

    if (!next_elem) {
	if (priv->find_is_forward != forward) {
	   g_list_reverse (next_list);
	}

	if (priv->find_str) {
	    str = g_strdup (priv->find_str);

	    if (!match_case) {
		haystack = g_utf8_casefold (str, -1);
	    } else {
		haystack = g_strdup (str);
	    }

	    if (!strcmp (find_string, haystack)) {
		box_node = priv->find_elem->data;

		if (priv->find_is_forward) {
		    html_find_selection_set (priv->view,
					     box_node->find_box->dom_node,
					     box_node->find_index + (priv->find_offset - 
								     strlen (priv->find_str)),
					     strlen (priv->find_str));
		} else {
		    html_find_selection_set (priv->view,
					     box_node->find_box->dom_node,
					     box_node->find_index + (box_node->find_len - 
								     priv->find_offset),
					     strlen (priv->find_str));		
		}
	    }
	    
	    g_free (str);
	    g_free (haystack);
	}
    } else {
	priv->find_list   = next_list;
	priv->find_elem   = next_elem;	
	priv->find_offset = next_offset;

	g_free (priv->find_str);
	priv->find_str = found_str;

	priv->find_is_forward = forward;
    }

    return next_elem != NULL;
}

gboolean
yelp_html_find (YelpHtml    *html,
		const gchar *find_string,
		gboolean     match_case,
		gboolean     wrap,
		gboolean     forward)
{
    YelpHtmlPriv    *priv;
    DomNode         *node;

    g_return_val_if_fail (YELP_IS_HTML (html), FALSE);
    g_return_val_if_fail (find_string != NULL, FALSE);

    priv = html->priv;

    if (!priv->find_iter) { 
	node = html_get_dom_node (priv->doc, "body");

	if (!node) {
	    g_warning ("yelp_html_find(): Couldn't find html body.");  
	    return FALSE;
	}

	priv->find_iter = dom_DocumentTraversal_createNodeIterator
	    (DOM_DOCUMENT_TRAVERSAL (priv->doc->dom_document),
	     node,
	     DOM_SHOW_TEXT,
	     NULL,
	     FALSE,
	     NULL);

	html_get_box_list (html);

	html_find_reset (html);
    }

    return html_find_text (html, find_string, match_case, wrap, forward);
}

void
yelp_html_jump_to_anchor (YelpHtml    *html,
			  gchar       *anchor)
{
    YelpHtmlPriv *priv;

    g_return_if_fail (html != NULL);

    priv = html->priv;

    if (priv->anchor)
	g_free (priv->anchor);

    priv->anchor = g_strdup (anchor);
}
