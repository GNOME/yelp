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
#include "yelp-util.h"
#include "yelp-scrollkeeper.h"

#define d(x) x
#undef DEBUG_OUTPUT

static void   yvh_init                      (YelpViewTOC      *html);
static void   yvh_class_init                (YelpViewTOCClass *klass);
static void   yvh_link_clicked_cb           (HtmlDocument     *doc,
					     const gchar      *url,
					     YelpViewTOC      *view);
static void   yelp_view_toc_man_1           (YelpViewTOC      *view);
static void   yelp_view_toc_man_2           (YelpViewTOC      *view,
					     GNode            *root);
static void   yelp_view_read_important_docs (YelpViewTOC      *view);

enum {
	URL_SELECTED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

struct _YelpViewTOCPriv {
	GtkWidget    *html_view;
	HtmlDocument *doc;
	GNode        *doc_tree;
	GList        *important_docs;
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
	
	yelp_view_read_important_docs (view);

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
	char *header="\n"
"<html>\n"
"  <head>\n"
"    <title>\n"
"      %s\n"
"    </title> \n"
"    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n"
"    <style type=\"text/css\">\n"
"      A:link { color: #00008b }\n"
"      A:visited { color: #00008b }\n"
"      A:active { color: #00008b }\n"
"      BODY { color: #00008b }\n"
"    </style>\n"
"  </head>\n"
"  <body bgcolor=\"#ffffff\">";
	char *s;

	s = g_strdup_printf (header, title);

	yelp_view_toc_write (view, s, -1);

	g_free (s);
}

static void
yelp_view_toc_write_footer (YelpViewTOC *view)
{
	char *footer="\n"
"  </body>\n"
"</html>\n";
	yelp_view_toc_write (view, footer, -1);
	
}

static void 
yelp_view_toc_start (YelpViewTOC *view)
{
	YelpViewTOCPriv *priv;
	GNode           *node;
	YelpSection     *section;
	gchar           *table_start = "\n"
"    <center>\n"
"      <table cellspacing=\"20\" width=\"100%\">\n"
"        <tr>\n";
	gchar           *table_end = "\n"
"      </tr>\n"
"    </table>\n"
"   </center>\n";
	char            *seriesid;
	GNode           *root;
	char            *path;
	GList           *list;
	gchar           *important_docs_string = _("Important documents");
	gchar           *other_docs_string = _("Other document systems");
	gchar           *man_string = _("Manual pages");
	gchar           *info_string = _("Info pages");
	gchar           *installed_string = _("Installed documents");
	
	priv = view->priv;

	if (!g_node_first_child (priv->doc_tree)) {
		g_warning ("No nodes in tree");
	}
	
	yelp_view_toc_open (view);
	
	yelp_view_toc_write_header (view, _("Start page"));
	
	yelp_view_toc_write (view, table_start, -1);

	yelp_view_toc_printf (view, 
			      "<td valign=\"top\">\n"
			      "<h2>%s</h2>\n"
			      "<ul>\n", important_docs_string);

	list = priv->important_docs;
	while (list != NULL) {
		seriesid = list->data;

		node = yelp_scrollkeeper_lookup_seriesid (seriesid);
		if (node) {
			section = node->data;
			yelp_view_toc_printf (view, 
					      "<li><a href=\"%s\">%s</a>\n",
					      section->uri, section->name);
		}

		list = list->next;
	}
	
			      
	yelp_view_toc_write (view, 
			     "</ul>\n"
			     "</td>\n", -1);



	yelp_view_toc_printf (view, 
			     "<td valign=\"top\">\n"
			     "<h2>%s</h2>\n", installed_string);

	root = yelp_util_find_toplevel (priv->doc_tree, "scrollkeeper");
	node = g_node_first_child (root);

	while (node) {
		section = (YelpSection *) node->data;
		path = yelp_util_node_to_string_path (node);
		yelp_view_toc_printf (view, 
				      "<a href=\"toc:scrollkeeper/%s\">%s</a><br>\n", 
				      path, section->name);
		g_free (path);
		
		node = g_node_next_sibling (node);
	}

	
	yelp_view_toc_printf (view, 
			     "<h2>%s</h2>\n"
			     "<a href=\"toc:man\">%s</a><br>\n"
			     "<a href=\"toc:info\">%s</a><br>\n",
			      other_docs_string, man_string, info_string);
			      
	yelp_view_toc_write (view, 
			     "</td>\n", -1);

	yelp_view_toc_write (view, table_end, -1);

	yelp_view_toc_write_footer (view);

	yelp_view_toc_close (view);
}

static char *
yelp_view_toc_full_path_name (YelpViewTOC *view, GNode *node) 
{
	char *str, *t;		
	YelpSection *section;

	section = node->data;

	str = g_strdup (section->name);

	while (node->parent) {
		node = node->parent;
		
		if (node->parent == NULL ||
		    node->parent->data == NULL) {
			/* Skip top node */
			break;
		}
		
		section = node->data;
		
		t = g_strconcat (section->name, "/", str, NULL);
		g_free (str);
		str = t;
	}
	return str;
}

static void
yelp_view_toc_man_emit (YelpViewTOC *view, GNode *first)
{
	YelpViewTOCPriv *priv;
	GNode *node, *child;
	YelpSection *section;
	gboolean got_a_leaf;
	char *path, *url;
	int i;

	priv = view->priv;

	got_a_leaf = FALSE;
	node = first;
	do {
		if (node->children != NULL) {
			child = node->children;
			
			section = node->data;

			path = yelp_util_node_to_string_path (node);
			yelp_view_toc_printf (view, "<h2><a href=\"toc:man/%s\">%s</a></h2>\n", path, section->name);
			g_free (path);
		} else {
			got_a_leaf = TRUE;
		}
	} while ((node = node->next) != NULL);


	if (got_a_leaf) {
		yelp_view_toc_write (view,
				     "<table cellpadding=\"2\" cellspacing=\"2\" border=\"0\" width=\"100%%\"><tbody>\n",
				     -1);

		i = 0;
		node = first;
		do {
			if (node->children == NULL) {
				YelpSection *section;
				
				if (i % 3 == 0) {
					if (i == 0) {
						yelp_view_toc_write (view, 
								     "<tr>\n",
								     -1);
					} else {
						yelp_view_toc_write (view,
								     "</tr>\n<tr>\n", 
								     -1);
					}
				}
			
				section = node->data;
				url = yelp_util_compose_path_url (node->parent, section->uri);
				yelp_view_toc_printf (view, "<td valign=\"Top\"><a href=\"%s\">%s</a></td>\n", url, section->name);
				g_free (url);
				i++;
			}
		} while ((node = node->next) != NULL);
		
		yelp_view_toc_write (view, "</tr>\n", -1);
		yelp_view_toc_write (view, "</tbody></table>\n", -1);
	}
}

static void 
yelp_view_toc_man_2 (YelpViewTOC *view,
		     GNode *root)
{
	GNode *first;
	gchar *name;
	gchar *string = _("Manual pages for section");
	
	if (root->children == NULL) {
		return;
	}
	
	first = root->children;

	yelp_view_toc_open (view);
	
	yelp_view_toc_write_header (view, "Manual pages");
		
	name = yelp_view_toc_full_path_name (view, root);
	
	yelp_view_toc_printf (view, "<h1>%s '%s'</h1>\n", string, name);
	g_free (name);

	yelp_view_toc_man_emit (view, first);
		
	yelp_view_toc_write_footer (view);
	yelp_view_toc_close (view);
}

static void 
yelp_view_toc_man_1 (YelpViewTOC *view)
{
	YelpViewTOCPriv *priv;
	GNode           *root, *node, *child;
	YelpSection     *section;
	char            *path;
	gchar           *string = _("Man page sections");
	
	priv = view->priv;

	root = yelp_util_find_toplevel (priv->doc_tree, "man");

	if (!root) {
		g_warning ("Unable to find man toplevel");
		return;
	}
	
	node = g_node_first_child (root);

	if (!node) {
		return;
	}

	yelp_view_toc_open (view);
	
	yelp_view_toc_write_header (view, _("Man page sections"));
	
	yelp_view_toc_printf (view, "<h1>%s</h1>\n", string);
	
	do {
		child = g_node_first_child (node);
		
		if (child) {
			section = (YelpSection *) node->data;
			path = yelp_util_node_to_string_path (node);
			yelp_view_toc_printf (view, 
					      "<a href=\"toc:man/%s\">%s</a><br>\n", 
					      path, section->name);
			g_free (path);
		}
	} while ((node = g_node_next_sibling (node)));
		
	yelp_view_toc_write_footer (view);
	yelp_view_toc_close (view);
}

static void 
yelp_view_toc_info (YelpViewTOC *view)
{
	YelpViewTOCPriv *priv;
	GNode           *root, *node;
	YelpSection     *section;
	char            *url;
	gchar           *string = _("Info pages");
	
	priv = view->priv;

	root = yelp_util_find_toplevel (priv->doc_tree, "info");

	if (!root) {
		g_warning ("Unable to find info toplevel");
		return;
	}
	
	node = g_node_first_child (root);

	if (!node) {
		return;
	}

	yelp_view_toc_open (view);
	
	yelp_view_toc_write_header (view, _("Info pages"));
	
	yelp_view_toc_printf (view, "<h1>%s</h1>\n", string);
	
	do {
		section = (YelpSection *) node->data;
		url = yelp_util_compose_path_url (root, section->uri);
		yelp_view_toc_printf (view, 
				      "<a href=\"%s\">%s</a><br>\n", 
				      url, section->name);
		g_free  (url);
	} while ((node = g_node_next_sibling (node)));
		
	yelp_view_toc_write_footer (view);
	yelp_view_toc_close (view);
}

static void
yelp_view_read_important_docs (YelpViewTOC *view)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	xmlNodePtr child;
	xmlChar *prop;
	
	doc = xmlParseFile (DATADIR "/yelp/important_docs.xml");
	if (doc == NULL)
		return;

	node = xmlDocGetRootElement (doc);
	if (node == NULL) {
		xmlFreeDoc(doc);
		return;
	}

	if (strcmp (node->name, "docs") != 0) {
		xmlFreeDoc(doc);
		return;
	}

	child = node->children;
	while (child) {
		if (strcmp (child->name, "document") == 0) {
  
			prop = xmlGetProp (child, "seriesid");
			if (prop) {
				view->priv->important_docs = g_list_append (view->priv->important_docs, g_strdup (prop));
				xmlFree (prop);
			}
		}
		child = child->next;
	}
	
	xmlFreeDoc(doc);
}

GtkWidget *
yelp_view_toc_new (GNode *doc_tree)
{
	YelpViewTOC     *view;
	YelpViewTOCPriv *priv;

	view = g_object_new (YELP_TYPE_VIEW_TOC, NULL);

	priv = view->priv;

	priv->doc_tree = doc_tree;

	yelp_view_toc_open_url (view, "toc:");
	
	return GTK_WIDGET (view);
}

static void 
yelp_view_toc_scrollkeeper (YelpViewTOC *view,
			    GNode *root)
{
	YelpViewTOCPriv *priv;
	GNode           *node, *child;
	YelpSection     *section;
	gboolean         got_a_leaf;
	char            *path;
	gchar           *name;
	gchar           *string = _("Scrollkeeper docs for category");

	priv = view->priv;
	
	if (root->children == NULL) {
		return;
	}
	
	yelp_view_toc_open (view);
	
	yelp_view_toc_write_header (view, "Scrollkeeper");
		
	name = yelp_view_toc_full_path_name (view, root);
	yelp_view_toc_printf (view, "<h1>%s '%s'</h1>\n", string, name);
	g_free (name);

	got_a_leaf = FALSE;
	node = root->children;
	while (node != NULL) {
		if (node->children != NULL) {
			child = node->children;
			
			section = node->data;

			path = yelp_util_node_to_string_path (node);
			yelp_view_toc_printf (view, "<h2><a href=\"toc:scrollkeeper/%s\">%s</a></h2>\n", path, section->name);
			g_free (path);
		} else {
			got_a_leaf = TRUE;
		}
		node = node->next;
	}


	if (got_a_leaf) {
		yelp_view_toc_write (view, "<ul>\n", -1);

		node = root->children;
		while (node != NULL) {
			if (node->children == NULL) {
				YelpSection *section;
			
				section = node->data;
				yelp_view_toc_printf (view, "<li><a href=\"%s\">%s</a>\n", section->uri, section->name);
			}
			node = node->next;
		}
		
		yelp_view_toc_write (view, "</ul>\n", -1);
	}
		
	yelp_view_toc_write_footer (view);
	yelp_view_toc_close (view);
}


void
yelp_view_toc_open_url (YelpViewTOC *view, const char *url)
{
	const char *toc_type;
	const char *path_string;
	GNode *node;
	
	g_assert (strncmp (url, "toc:", 4) == 0);
	
	if (strncmp (url, "toc:", 4) != 0) {
		return;
	}

	toc_type = url + 4;

	if (*toc_type == 0) {
		yelp_view_toc_start (view);
	} else if (strncmp (toc_type, "man", 3) == 0) {
		path_string = toc_type + 3;

		if (path_string[0] == 0) {
 			yelp_view_toc_man_1 (view);
		} else if (path_string[0] == '/') {
			/* Calculate where it should go */
			path_string++;

			node = yelp_util_string_path_to_node  (path_string,
							       view->priv->doc_tree);
			if (node) {
				yelp_view_toc_man_2 (view, node);
			} else {
				g_warning ("Bad path in toc url %s\n", url);
			}
		}
	} else if (strcmp (toc_type, "info") == 0) {
		yelp_view_toc_info (view);
	} else if (strncmp (toc_type, "scrollkeeper", strlen ("scrollkeeper")) == 0) {
		path_string = toc_type + strlen ("scrollkeeper");
		if (path_string[0] == '/') {
			/* Calculate where it should go */
			path_string++;

			node = yelp_util_string_path_to_node  (path_string,
							       view->priv->doc_tree);
			if (node) {
				yelp_view_toc_scrollkeeper (view, node);
			} else {
				g_warning ("Bad path in toc url %s\n", url);
			}
		}
	} else {
		g_warning ("Unknown toc type %s\n", url);
	}
}
