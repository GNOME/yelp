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

#define d(x) x

static void yvh_init               (YelpViewTOC          *html);
static void yvh_class_init         (YelpViewTOCClass     *klass);
static void yvh_link_clicked_cb    (HtmlDocument          *doc, 
				    const gchar           *url, 
				    YelpViewTOC          *view);
static void yelp_view_toc_man      (YelpViewTOC          *view);

enum {
	PATH_SELECTED,
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
	signals[PATH_SELECTED] = 
		g_signal_new ("path_selected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpViewTOCClass,
					       path_selected),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
}

static void
yvh_link_clicked_cb (HtmlDocument *doc, const gchar *url, YelpViewTOC *view)
{
	YelpViewTOCPriv *priv;
	GtkTreePath      *path;
	
	g_return_if_fail (HTML_IS_DOCUMENT (doc));
	g_return_if_fail (url != NULL);
	g_return_if_fail (YELP_IS_VIEW_TOC (view));
	
	priv = view->priv;

	g_print ("Category %s pressed\n", url);

	path = gtk_tree_path_new_from_string (url);

	if (path) {
		g_signal_emit (view, signals[PATH_SELECTED], 0, path);
	}

	gtk_tree_path_free (path);
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
	g_print ("%.*s\n", len,data);
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
	GtkTreeIter      iter;

	view = g_object_new (YELP_TYPE_VIEW_TOC, NULL);

	priv = view->priv;

	priv->tree_model = tree_model;

	if (!gtk_tree_model_get_iter_from_string (tree_model, &iter, "0")) {
		g_warning ("No nodes in tree");
		return NULL;
	}

#if 1
	yelp_view_toc_open (view);
	
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
                Describe what man pages are...
              <li><b>Info pages</b><br>
                Describe what info pages are...
            </ul>
          </td>
      </tr>
    </table>
   </center>
";
		gchar *name;
		GtkTreePath *path;
		gchar       *path_str;

		yelp_view_toc_write_header (view, "Start page");
		
		yelp_view_toc_write (view, table_start, -1);
		
		do {
			gtk_tree_model_get (priv->tree_model, &iter,
					    0, &name,
					    -1);

			path = gtk_tree_model_get_path (tree_model, &iter);
			path_str = gtk_tree_path_to_string (path);
			
			yelp_view_toc_printf (view, "<li><a href=\"%s\">%s</a>\n", 
					      path_str, name);
			
		} while (gtk_tree_model_iter_next  (priv->tree_model, &iter));

		
		yelp_view_toc_write (view, table_end, -1);
		yelp_view_toc_write_footer (view);
	}
	
	yelp_view_toc_close (view);
#else
	yelp_view_toc_man (view);
#endif

	return GTK_WIDGET (view);
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


static void
yelp_view_toc_man_emit (YelpViewTOC *view, GtkTreeIter *first, int level)
{
	YelpViewTOCPriv *priv;
	GtkTreeIter iter, child;
	char level_c;
	char *name;
	gboolean got_a_leaf;
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
			yelp_view_toc_printf (view, "<h%c>%s</h%c>\n", level_c, name, level_c);
			g_free (name);
			yelp_view_toc_man_emit (view, &child, level + 1);
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
				gtk_tree_model_get (priv->tree_model, &iter,
						    0, &name, -1);
				yelp_view_toc_printf (view, "<td valign=\"Top\">%s</td>\n", name);
				g_free (name);
				i++;
			}
		} while (gtk_tree_model_iter_next  (priv->tree_model, &iter));
		
		yelp_view_toc_write (view, "</tr>\n", -1);
		yelp_view_toc_write (view, "</tbody></table>\n", -1);
	}
}

static void 
yelp_view_toc_man (YelpViewTOC *view)
{
	GtkTreeIter root, first;

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
	
	yelp_view_toc_write_header (view, "Man pages");
		
	yelp_view_toc_man_emit (view, &first, 0);
		
	yelp_view_toc_write_footer (view);
	yelp_view_toc_close (view);
}
