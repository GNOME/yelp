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
#include <libgtkhtml/gtkhtml.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "yelp-view-toc.h"
#include "yelp-marshal.h"

#define d(x) x

static void yvh_init               (YelpViewTOC          *html);
static void yvh_class_init         (YelpViewTOCClass     *klass);
static void yvh_link_clicked_cb    (HtmlDocument          *doc, 
				    const gchar           *url, 
				    YelpViewTOC          *view);
static void yelp_view_toc_man_1    (YelpViewTOC          *view);
static void yelp_view_toc_man_2    (YelpViewTOC          *view,
				    GtkTreeIter          *root);

enum {
	URL_SELECTED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

struct _YelpViewTOCPriv {
	GtkWidget    *html_view;
	HtmlDocument *doc;
	GtkTreeModel *tree_model;
};

GType
yelp_view_toc_get_type (void)
{
        static GType view_type = 0;

        if (!view_type)
        {
                static const GTypeInfo view_info =
                        {
                                sizeof (YelpViewTOCClass),
                                NULL,
                                NULL,
                                (GClassInitFunc) yvh_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpViewTOC),
                                0,
                                (GInstanceInitFunc) yvh_init,
                        };
                
                view_type = g_type_register_static (HTML_TYPE_VIEW,
                                                    "YelpViewTOC", 
                                                    &view_info, 0);
        }
        
        return view_type;
}

static void
yvh_init (YelpViewTOC *view)
{
	YelpViewTOCPriv *priv;
	
	priv = g_new0 (YelpViewTOCPriv, 1);
	view->priv = priv;
	
	priv->doc = html_document_new ();

	html_view_set_document (HTML_VIEW (view), priv->doc);

	g_signal_connect (G_OBJECT (priv->doc), "link_clicked",
			  G_CALLBACK (yvh_link_clicked_cb), view);
}

static void
yvh_class_init (YelpViewTOCClass *klass)
{
	signals[URL_SELECTED] = 
		g_signal_new ("url_selected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpViewTOCClass,
					       url_selected),
			      NULL, NULL,
			      yelp_marshal_VOID__STRING_STRING_BOOLEAN,
			      G_TYPE_NONE,
			      3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
}

static void
yvh_link_clicked_cb (HtmlDocument *doc, const gchar *url, YelpViewTOC *view)
{
	g_return_if_fail (HTML_IS_DOCUMENT (doc));
	g_return_if_fail (url != NULL);
	g_return_if_fail (YELP_IS_VIEW_TOC (view));
	
	g_signal_emit (view, signals[URL_SELECTED], 0,
		       url, NULL, FALSE);
}

static void
yelp_view_toc_open (YelpViewTOC *view)
{
	html_document_open_stream (view->priv->doc, "text/html");
}

static void
yelp_view_toc_close (YelpViewTOC *view)
{
	/* TODO: If buffering, flush buffers */
	html_document_close_stream (view->priv->doc);
}

static void
yelp_view_toc_write (YelpViewTOC *view, char *data, int len)
{
	if (len < 0) {
		len = strlen (data);
	}

#define DEBUG_OUTPUT
#ifdef DEBUG_OUTPUT
	g_print ("%.*s", len,data);
#endif

	/* TODO: Maybe we should be buffering writes */
	
	html_document_write_stream (view->priv->doc, data, len);
}

static void
yelp_view_toc_printf (YelpViewTOC *view, char *format, ...)
{
  va_list args;
  gchar *string;
  
  g_return_if_fail (format != NULL);
  
  va_start (args, format);
  string = g_strdup_vprintf (format, args);
  va_end (args);

  yelp_view_toc_write (view, string, -1);
  
  g_free (string);
}

static void
yelp_view_toc_write_header (YelpViewTOC *view, char *title)
{
	char *header="
<html>
  <head>
    <title>
      %s
    </title> 
    <style type=\"text/css\">
      A:link { color: #00008b }
      A:visited { color: #00008b }
      A:active { color: #00008b }
      BODY { color: #00008b }
    </style>
  </head>
  <body bgcolor=\"#ffffff\">";
	char *s;

	s = g_strdup_printf (header, title);

	yelp_view_toc_write (view, s, -1);

	g_free (s);
}

static void
yelp_view_toc_write_footer (YelpViewTOC *view)
{
	char *footer="
  </body>
</html>
";
	yelp_view_toc_write (view, footer, -1);
	
}

GtkWidget *
yelp_view_toc_new (GtkTreeModel *tree_model)
{
	YelpViewTOC     *view;
	YelpViewTOCPriv *priv;

	view = g_object_new (YELP_TYPE_VIEW_TOC, NULL);

	priv = view->priv;

	priv->tree_model = tree_model;

	yelp_view_toc_open_url (view, "toc:");

	return GTK_WIDGET (view);
}

static void 
yelp_view_toc_start (YelpViewTOC *view)
{
	gchar *table_start = "
    <center>
      <table cellspacing=\"20\" width=\"100%\">
        <tr>
          <td valign=\"top\">
	    <h2>GNOME</h2>
             <ul>
               <li><b>Introduction to GNOME</b>
               <li><b>GNOME User Guide</b>
             </ul>
             <ul>
";
	gchar *table_end = "
             </ul>
	  </td>
          <td valign=\"top\">
            <h2>System</h2>
            <ul>
              <li><b>Man pages</b><br>
                Describe what <a href=\"toc:man\">man pages</a> are...
              <li><b>Info pages</b><br>
                Describe what <a href=\"toc:info\">info pages</a> are...
            </ul>
          </td>
      </tr>
    </table>
   </center>
";
	YelpViewTOCPriv *priv;
	GtkTreeIter      iter;
	gchar *name;
	GtkTreePath *path;
	gchar       *path_str;

	priv = view->priv;

	if (!gtk_tree_model_get_iter_from_string (priv->tree_model, &iter, "0")) {
		g_warning ("No nodes in tree");
	}
	
	yelp_view_toc_open (view);
	
	yelp_view_toc_write_header (view, "Start page");
	
	yelp_view_toc_write (view, table_start, -1);
	
	do {
		gtk_tree_model_get (priv->tree_model, &iter,
				    0, &name,
				    -1);
		
		path = gtk_tree_model_get_path (priv->tree_model, &iter);
		path_str = gtk_tree_path_to_string (path);
		
		yelp_view_toc_printf (view, "<li><a href=\"path:%s\">%s</a>\n", 
				      path_str, name);
		
	} while (gtk_tree_model_iter_next  (priv->tree_model, &iter));
	
	
	yelp_view_toc_write (view, table_end, -1);
	yelp_view_toc_write_footer (view);

	yelp_view_toc_close (view);
}


static gboolean
yelp_view_toc_find_toplevel (YelpViewTOC *view, char *name, GtkTreeIter *iter_res)
{
	YelpViewTOCPriv *priv;
	GtkTreeIter iter;
	char *node_name;

	priv = view->priv;
	
	if (!gtk_tree_model_get_iter_from_string (priv->tree_model, &iter, "0")) {
		g_warning ("No nodes in tree");
		return FALSE;
	}

	do {
		gtk_tree_model_get (priv->tree_model, &iter,
				    0, &node_name,
				    -1);
		if (strcmp (name, node_name) == 0) {
			*iter_res = iter;
			g_free (node_name);
			return TRUE;
		}
		g_free (node_name);
	} while (gtk_tree_model_iter_next  (priv->tree_model, &iter));

	return FALSE;
}

static char *
yelp_view_toc_full_path_name (YelpViewTOC *view, GtkTreeIter *top) 
{
	char *str, *t;
	GtkTreeIter iter;
	GtkTreeIter next_iter;
	char *node_name;

	gtk_tree_model_get (view->priv->tree_model, top,
			    0, &str,
			    -1);

	iter = *top;
	while (gtk_tree_model_iter_parent (view->priv->tree_model,
					   &iter, &iter)) {
		if (!gtk_tree_model_iter_parent (view->priv->tree_model,
						 &next_iter, &iter))
			break;
		gtk_tree_model_get (view->priv->tree_model, &iter,
				    0, &node_name,
				    -1);
		t = g_strconcat (node_name, "/", str, NULL);
		g_free (str);
		g_free (node_name);
		str = t;
	}
	return str;
}

static void
yelp_view_toc_man_emit (YelpViewTOC *view, GtkTreeIter *first, int level)
{
	YelpViewTOCPriv *priv;
	GtkTreeIter iter, child;
	char level_c;
	char *name;
	char *path_string;
	gboolean got_a_leaf;
	GtkTreePath *path;
	int i;

	priv = view->priv;

	level_c = '1' + level;
	got_a_leaf = FALSE;
	iter = *first;
	do {
		if (gtk_tree_model_iter_children  (view->priv->tree_model,
						   &child, &iter)) {
			
			gtk_tree_model_get (priv->tree_model, &iter,
					    0, &name,
					    -1);
			
			path = gtk_tree_model_get_path (priv->tree_model, &iter);
			path_string = gtk_tree_path_to_string (path);
			yelp_view_toc_printf (view, "<h2><a href=\"toc:man/%s\">%s</a></h2>\n", path_string, name);
			gtk_tree_path_free (path);
			g_free (path_string);
			g_free (name);
		} else {
			got_a_leaf = TRUE;
		}
	} while (gtk_tree_model_iter_next  (priv->tree_model, &iter));


	if (got_a_leaf) {
		yelp_view_toc_write (view,
				     "<table cellpadding=\"2\" cellspacing=\"2\" border=\"0\" width=\"100\%\"><tbody>\n",
				     -1);

		i = 0;
		iter = *first;
		do {
			if (i % 3 == 0) {
				if (i == 0) {
					yelp_view_toc_write (view, "<tr>\n", -1);
				} else {
					yelp_view_toc_write (view, "</tr>\n<tr>\n", -1);
				}
			}
			
			if (!gtk_tree_model_iter_has_child (view->priv->tree_model,
							    &iter)) {
				YelpSection *section;
				gtk_tree_model_get (priv->tree_model, &iter,
						    0, &name,
						    1, &section, -1);
				yelp_view_toc_printf (view, "<td valign=\"Top\"><a href=\"%s\">%s</a></td>\n", section->uri, name);
				g_free (name);
				i++;
			}
		} while (gtk_tree_model_iter_next  (priv->tree_model, &iter));
		
		yelp_view_toc_write (view, "</tr>\n", -1);
		yelp_view_toc_write (view, "</tbody></table>\n", -1);
	}
}

static void 
yelp_view_toc_man_2 (YelpViewTOC *view,
		     GtkTreeIter *root)
{
	GtkTreeIter first;
	gchar *name;

	if (!gtk_tree_model_iter_children (view->priv->tree_model,
					   &first,
					   root)) {
		return;
	}

	yelp_view_toc_open (view);
	
	yelp_view_toc_write_header (view, "Manual pages");
		
	/*	gtk_tree_model_get (view->priv->tree_model, root,
			    0, &name,
			    -1);*/
	name = yelp_view_toc_full_path_name (view, root);
	yelp_view_toc_printf (view, "<h1>Manual pages for section '%s'</h1>\n", name);
	
	g_free (name);
	
	yelp_view_toc_man_emit (view, &first, 0);
		
	yelp_view_toc_write_footer (view);
	yelp_view_toc_close (view);
}


static void 
yelp_view_toc_man_1 (YelpViewTOC *view)
{
	GtkTreeIter root, first;
	YelpViewTOCPriv *priv;
	GtkTreeIter iter, child;
	char *name, *path_string;
	GtkTreePath *path;

	if (!yelp_view_toc_find_toplevel (view, "man", &root)) {
		g_warning ("Unable to find man toplevel");
		return;
	}
	
	if (!gtk_tree_model_iter_children (view->priv->tree_model,
					   &first,
					   &root)) {
		return;
	}

	yelp_view_toc_open (view);
	
	yelp_view_toc_write_header (view, "Man page sections");
	

	priv = view->priv;

	yelp_view_toc_write (view, "<h1>Man page sections</h1>\n", -1);
	
	iter = first;
	do {
		if (gtk_tree_model_iter_children  (priv->tree_model,
						   &child, &iter)) {
			gtk_tree_model_get (priv->tree_model, &iter,
					    0, &name,
					    -1);
			path = gtk_tree_model_get_path (priv->tree_model, &iter);
			path_string = gtk_tree_path_to_string (path);
			yelp_view_toc_printf (view, "<a href=\"toc:man/%s\">%s</a><br>\n", path_string, name);
			gtk_tree_path_free (path);
			g_free (path_string);
			g_free (name);
		}
	} while (gtk_tree_model_iter_next  (priv->tree_model, &iter));
		
	yelp_view_toc_write_footer (view);
	yelp_view_toc_close (view);
}

void
yelp_view_toc_open_url (YelpViewTOC *view, const char *url)
{
	const char *toc_type;
	const char *path_string;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	g_assert (strncmp (url, "toc:", 4) == 0);
	
	if (strncmp (url, "toc:", 4) != 0)
		return;

	toc_type = url + 4;
	
	if (*toc_type == 0) {
		yelp_view_toc_start (view);
	} else if (strncmp (toc_type, "man", 3) == 0) {
		path_string = toc_type + 3;

		if (path_string[0] == 0) {
			yelp_view_toc_man_1 (view);
		} else if (path_string[0] == '/') {
			path_string++;
			path = gtk_tree_path_new_from_string  (path_string);
			if (gtk_tree_model_get_iter (view->priv->tree_model,
						     &iter, path)) {
				yelp_view_toc_man_2 (view, &iter);
			} else {
				g_warning ("Bad path in toc url %s\n", url);
			}
			
			gtk_tree_path_free (path);
		}
	} else {
		g_warning ("Unknown toc type %s\n", url);
	}
}
