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
#include "yelp-view-home.h"

#define d(x) x

static void yvh_init               (YelpViewHome          *html);
static void yvh_class_init         (YelpViewHomeClass     *klass);
static void yvh_link_clicked_cb    (HtmlDocument          *doc, 
				    const gchar           *url, 
				    YelpViewHome          *view);

enum {
	PATH_SELECTED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

struct _YelpViewHomePriv {
	GtkWidget    *html_view;
	HtmlDocument *doc;
	GtkTreeModel *tree_model;
};

GType
yelp_view_home_get_type (void)
{
        static GType view_type = 0;

        if (!view_type)
        {
                static const GTypeInfo view_info =
                        {
                                sizeof (YelpViewHomeClass),
                                NULL,
                                NULL,
                                (GClassInitFunc) yvh_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpViewHome),
                                0,
                                (GInstanceInitFunc) yvh_init,
                        };
                
                view_type = g_type_register_static (HTML_TYPE_VIEW,
                                                    "YelpViewHome", 
                                                    &view_info, 0);
        }
        
        return view_type;
}

static void
yvh_init (YelpViewHome *view)
{
	YelpViewHomePriv *priv;
	
	priv = g_new0 (YelpViewHomePriv, 1);
	view->priv = priv;
	
	priv->doc = html_document_new ();

	html_view_set_document (HTML_VIEW (view), priv->doc);

	g_signal_connect (G_OBJECT (priv->doc), "link_clicked",
			  G_CALLBACK (yvh_link_clicked_cb), view);
}

static void
yvh_class_init (YelpViewHomeClass *klass)
{
	signals[PATH_SELECTED] = 
		g_signal_new ("path_selected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpViewHomeClass,
					       path_selected),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
}

static void
yvh_link_clicked_cb (HtmlDocument *doc, const gchar *url, YelpViewHome *view)
{
	YelpViewHomePriv *priv;
	GtkTreePath      *path;
	
	g_return_if_fail (HTML_IS_DOCUMENT (doc));
	g_return_if_fail (url != NULL);
	g_return_if_fail (YELP_IS_VIEW_HOME (view));
	
	priv = view->priv;

	g_print ("Category %s pressed\n", url);

	path = gtk_tree_path_new_from_string (url);

	if (path) {
		g_signal_emit (view, signals[PATH_SELECTED], 0, path);
	}

	gtk_tree_path_free (path);
}

GtkWidget *
yelp_view_home_new (GtkTreeModel *tree_model)
{
	YelpViewHome     *view;
	YelpViewHomePriv *priv;
	GtkTreeIter       iter;

	view = g_object_new (YELP_TYPE_VIEW_HOME, NULL);

	priv = view->priv;

	priv->tree_model = tree_model;

	if (!gtk_tree_model_get_iter_from_string (tree_model, &iter, "0")) {
		g_warning ("No nodes in tree");
		return NULL;
	}

	/* FIXME: Write this in a sane way, just for testing right now  */
	/* AND Yes: this will be marked for translation when it's fixed */
	
	html_document_open_stream (priv->doc, "text/html");
	
	{
		int len;
		gchar *name;
		gchar *text;
		gchar *header = "
<html>
  <head>
    <title>
      Help Home View
    </title> 
    <style type=\"text/css\">
      A:link { color: white }          /* unvisited link */
      A:visited { color: white }        /* visited links */
      A:active { color: white }        /* active links */
      BODY { color: white }
    </style>
  </head>

  <body bgcolor=\"#104e8b\" link=\"white\">
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
		gchar *footer = "
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
  </body>
</html>
";
		gchar       *tmp_str;
		gchar       *other_stuff;
		GtkTreePath *path;
		gchar       *path_str;

		gtk_tree_model_get (priv->tree_model, &iter,
				    0, &name,
				    -1);

		tmp_str = g_strdup ("");
		
		do {
			gtk_tree_model_get (priv->tree_model, &iter,
					    0, &name,
					    -1);

			path = gtk_tree_model_get_path (tree_model, &iter);
			path_str = gtk_tree_path_to_string (path);
			
			other_stuff = g_strdup_printf ("%s\n<li><a href=\"%s\">%s</a>", 
						       tmp_str, 
						       path_str, name);
			gtk_tree_path_free (path);
			g_free (path_str);
			g_free (tmp_str);
			tmp_str = other_stuff;
			
		} while (gtk_tree_model_iter_next  (priv->tree_model, &iter));

		text = g_strconcat (header, other_stuff, footer, NULL);
		
		len = strlen (text);
                
		html_document_write_stream (priv->doc, text, len);
	}
	
	html_document_close_stream (priv->doc);

	return GTK_WIDGET (view);
}



