/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@codefactory.se>
 * Copyright (C) 2002 Red Hat Inc.
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
#include <libxml/parser.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "yelp-view-toc.h"
#include "yelp-marshal.h"
#include "yelp-util.h"
#include "yelp-html.h"
#include "yelp-scrollkeeper.h"

#define d(x)

static void   toc_init                      (YelpViewTOC       *html);
static void   toc_class_init                (YelpViewTOCClass  *klass);
static void   toc_man_1                     (YelpViewTOC       *view);
static void   toc_man_2                     (YelpViewTOC       *view,
					     GNode             *root);
static void   toc_read_important_docs       (YelpViewTOC       *view);
static void   toc_uri_selected_cb           (YelpHtml          *html,
					     YelpURI           *uri,
					     gboolean           handled,
					     YelpViewTOC       *view);
static void   toc_html_title_changed_cb     (YelpHtml          *html,
					     const gchar       *title,
					     YelpViewTOC       *view);
static void   toc_page_start                (YelpViewTOC       *view,
					     const gchar       *title,
					     const gchar       *heading);
static void   toc_page_end                  (YelpViewTOC       *view);
static void   toc_show_uri                  (YelpView          *view,
					     YelpURI           *uri,
					     GError          **error);

typedef struct {
	char  *title;
	GList *seriesids;
} YelpImportantDocsSection;

#define BUFFER_SIZE 4096

struct _YelpViewTOCPriv {
	YelpHtml  *html_view;
	GtkWidget *html_widget;
	
	GNode     *doc_tree;
	GList     *important_sections;
};

GType
yelp_view_toc_get_type (void)
{
        static GType view_type = 0;

        if (!view_type) {
                static const GTypeInfo view_info =
                        {
                                sizeof (YelpViewTOCClass),
                                NULL,
                                NULL,
                                (GClassInitFunc) toc_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpViewTOC),
                                0,
                                (GInstanceInitFunc) toc_init,
                        };
                
                view_type = g_type_register_static (YELP_TYPE_VIEW,
                                                    "YelpViewTOC", 
                                                    &view_info, 0);
        }
        
        return view_type;
}

static void
toc_init (YelpViewTOC *view)
{
	YelpViewTOCPriv *priv;
	
	priv = g_new0 (YelpViewTOCPriv, 1);
	view->priv = priv;
	
	toc_read_important_docs (view);

	priv->html_view   = yelp_html_new ();
	priv->html_widget = yelp_html_get_widget (priv->html_view);
	YELP_VIEW (view)->widget = priv->html_widget;

	g_signal_connect (priv->html_view, "uri_selected",
			  G_CALLBACK (toc_uri_selected_cb),
			  view);

	g_signal_connect (priv->html_view, "title_changed",
			  G_CALLBACK (toc_html_title_changed_cb),
			  view);
}

static void
toc_class_init (YelpViewTOCClass *klass)
{
	YelpViewClass *view_class = YELP_VIEW_CLASS (klass);
       
	view_class->show_uri = toc_show_uri;
}

#if 0
static void
toc_write_footer (YelpViewTOC *view)
{
	char *footer="\n"
		"  </body>\n"
		"</html>\n";

	yelp_html_write (view->priv->html_view, footer, -1);
}
#endif 

static void 
toc_start (YelpViewTOC *view)
{
	YelpViewTOCPriv *priv;
	GNode           *node;
	YelpSection     *section;
/* 	char            *seriesid; */
	GNode           *root;
	char            *path;
	GList           *sections;
/* 	GList           *seriesids; */
	gchar           *page_title = _("Help Contents");
	gchar           *section_gnome = _("GNOME - Desktop");
	gchar           *section_additional = _("Additional documents");
	gchar           *man_string = _("Manual Pages");
	gchar           *info_string = _("Info Pages");
/* 	YelpImportantDocsSection *important_section; */
/* 	gboolean         important_doc_installed = FALSE; */
	
	priv = view->priv;

	if (!g_node_first_child (priv->doc_tree)) {
		g_warning ("No nodes in tree");
	}
	
 	yelp_html_clear (priv->html_view);

	toc_page_start (view, page_title, page_title);

	sections = priv->important_sections;
	
#if 0
	while (sections != NULL) {
		important_section = sections->data;

		seriesids = important_section->seriesids;

		/* Check if any of the important documents are installed  *
		 * before trying to write the section topic               */

		for (seriesids = important_section->seriesids; 
		     seriesids; 
		     seriesids = seriesids->next) {
			seriesid = seriesids->data;

			if (yelp_scrollkeeper_lookup_seriesid (seriesid)){
				important_doc_installed = TRUE;
			}
		}
		
 		if (important_doc_installed) {
			if (!left_column_started) {
				yelp_html_printf (priv->html_view, 
						  COLUMN_LEFT_START);
				left_column_started = TRUE;
			}
			
			yelp_html_printf (priv->html_view, 
					  TOC_BLOCK_SEPARATOR
					  TOC_BLOCK_START 
					  "<h2>%s</h2>",
					  important_section->title);
		}
		
		seriesids = important_section->seriesids;
		
		while (seriesids != NULL) {
			seriesid = seriesids->data;

			node = yelp_scrollkeeper_lookup_seriesid (seriesid);

			if (node) {
				gchar *str_uri;
				
				section = node->data;

				str_uri = yelp_uri_to_string (section->uri);
			      
				yelp_html_printf (priv->html_view, 
						  "<a href=\"%s\">%s</a>\n",
						  str_uri,
						  section->name);
				g_free (str_uri);
			}

			seriesids = seriesids->next;
		}

		if (important_doc_installed) {
			yelp_html_printf (priv->html_view, 
					  TOC_BLOCK_END);
			if (sections->next) {
				yelp_html_printf (priv->html_view, 
						  TOC_BLOCK_SEPARATOR);
			}
		}
		
		sections = sections->next;
		important_doc_installed = FALSE;
	}

	if (left_column_started) {
		yelp_html_printf (priv->html_view, COLUMN_END);
	}

	
	yelp_html_printf (priv->html_view, 
			  COLUMN_RIGHT_START);
#endif

	root = yelp_util_find_toplevel (priv->doc_tree, "scrollkeeper");
	node = g_node_first_child (root);

	yelp_html_printf (priv->html_view,
			  "<td colspan=\"2\"><h2>%s</h2><ul>",
			  section_gnome);
			      
	root = yelp_util_find_node_from_name (priv->doc_tree, "GNOME");
	node = g_node_first_child (root);

	while (node) {
		section = YELP_SECTION (node->data);
		path = yelp_util_node_to_string_path (node);
		yelp_html_printf (priv->html_view, 
				  "<li><a href=\"toc:scrollkeeper/%s\">%s</a><br>\n",
				  path, section->name);
		g_free (path);
		
		node = g_node_next_sibling (node);
	}

	yelp_html_printf (priv->html_view,"</ul>");

	yelp_html_printf (priv->html_view,
			  "<br><h2>%s</h2><ul>",
			  section_additional);

	root = yelp_util_find_toplevel (priv->doc_tree, "scrollkeeper");
	node = g_node_first_child (root);

	while (node) {
		section = YELP_SECTION (node->data);
		if (strcmp (section->name, "GNOME")) {
			path = yelp_util_node_to_string_path (node);
			yelp_html_printf (priv->html_view, 
					  "<li><a href=\"toc:scrollkeeper/%s\">%s</a><br>\n", 
					  path, section->name);
			g_free (path);
			
		}
		node = g_node_next_sibling (node);
	}

	if (yelp_util_find_toplevel (priv->doc_tree, "man")) {
		yelp_html_printf (priv->html_view,
				  "<li><a href=\"toc:man\">%s</a><br>\n",
				  man_string);
	}
		
	if (yelp_util_find_toplevel (priv->doc_tree, "info")) {
		yelp_html_printf (priv->html_view,
				  "<li><a href=\"toc:info\">%s</a><br>\n",
				  info_string);
	}

	yelp_html_printf (priv->html_view,
			  "</ul></td><td></td></tr>");
	
	toc_page_end (view);

	yelp_html_close (priv->html_view);
}

static char *
toc_full_path_name (YelpViewTOC *view, GNode *node) 
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
toc_man_emit (YelpViewTOC *view, GNode *first)
{
	YelpViewTOCPriv *priv;
	GNode           *node, *child;
	YelpSection     *section;
	gboolean         got_a_leaf;
	char            *path, *url;
	char            *tmp;
	int              i;
	gboolean         sub_started = FALSE;
	gchar           *str_docs = _("Documents");
	gchar           *str_subcats = _("Categories");

	priv = view->priv;

	got_a_leaf = FALSE;
	node = first;
	do {
		if (node->children != NULL) {
			if (!sub_started) {
				yelp_html_printf (priv->html_view, "<h3>%s</h3>", 
						  str_subcats);
				
				sub_started = TRUE;
			}
			
			child = node->children;
			
			section = node->data;

			path = yelp_util_node_to_string_path (node);
			yelp_html_printf (priv->html_view, "<a href=\"toc:man/%s\">%s</a><br>\n", path, section->name);
			g_free (path);
		} else {
			got_a_leaf = TRUE;
		}
	} while ((node = node->next) != NULL);

	if (sub_started) {
		yelp_html_printf (priv->html_view,
				  "<img alt=\"\" src=\"file:" IMAGEDIR "/empty.png\" height=\"10\">\n");
	}

	if (got_a_leaf) {
		yelp_html_printf (priv->html_view, "<h3>%s</h3>", str_docs);

		yelp_html_write (priv->html_view,
				 "<table cellpadding=\"2\" cellspacing=\"2\" border=\"0\" width=\"100%%\">\n",
				 -1);

		i = 0;
		node = first;
		do {
			if (node->children == NULL) {
				YelpSection *section;
				
				if (i % 3 == 0) {
					if (i == 0) {
						yelp_html_write (priv->html_view, 
								 "<tr>\n",
								 -1);
					} else {
						yelp_html_write (priv->html_view,
								 "</tr>\n<tr>\n", 
								 -1);
					}
				}
			
				section = node->data;
				url = yelp_util_compose_path_url (node->parent, yelp_uri_get_path (section->uri));
				tmp = yelp_uri_to_string (section->uri);
				yelp_html_printf (priv->html_view, "<td valign=\"Top\"><a href=\"%s\">%s</a></td>\n", tmp, 
						  section->name);
				g_free (url);
				g_free (tmp);
				i++;
			}
		} while ((node = node->next) != NULL);
		
		yelp_html_write (priv->html_view, "</tr>\n", -1);
		yelp_html_write (priv->html_view, "</table>\n", -1);

	}
}

static void 
toc_man_2 (YelpViewTOC *view,
	   GNode       *root)
{
	YelpViewTOCPriv *priv;
	GNode           *first;
	gchar           *name;
	gchar           *string = _("Manual pages");
	gchar           *title;

	g_return_if_fail (YELP_IS_VIEW_TOC (view));
	
	priv = view->priv;

	if (root->children == NULL) {
		return;
	}
	
	first = root->children;

	yelp_html_clear (priv->html_view);
 
	name = toc_full_path_name (view, root);
	
	title = g_strdup_printf ("%s/%s", string, name);

	toc_page_start (view, title, string);

 	yelp_html_printf (priv->html_view,
 			  "<td colspan=\"2\"><h2>%s</h2><ul>",
 			  title);

	g_free (title);
	g_free (name);

	toc_man_emit (view, first);

	yelp_html_printf (priv->html_view,
			  "</td></tr>\n");
		
	toc_page_end (view);

	yelp_html_close (priv->html_view);
}

static void 
toc_man_1 (YelpViewTOC *view)
{
	YelpViewTOCPriv *priv;
	GNode           *root, *node, *child;
	YelpSection     *section;
	char            *path;
	gchar           *string = _("Manual Pages");
	gchar           *str_subcats = _("Categories");

	priv = view->priv;

	root = yelp_util_find_toplevel (priv->doc_tree, "man");

	if (!root) {
		g_warning ("Unable to find man toplevel");
		return;
	}
	
	node = g_node_first_child (root);

	if (!node) {
		g_warning ("Unable to find man categories");
		return;
	}

	yelp_html_clear (priv->html_view);

	toc_page_start (view, string, string);

	yelp_html_printf (priv->html_view,
			  "<td colspan=\"2\"><h2>%s</h2><ul>",
			  str_subcats);
	
	do {
		child = g_node_first_child (node);
		
		if (child) {
			section = YELP_SECTION (node->data);
			path = yelp_util_node_to_string_path (node);

  			yelp_html_printf (priv->html_view,
 					  "<li><a href=\"toc:man/%s\">%s</a><br>\n",
 					  path, section->name);
			
			g_free (path);
		}
	} while ((node = g_node_next_sibling (node)));

	yelp_html_printf (priv->html_view,
			  "</ul></td></tr>");

	toc_page_end (view);

	yelp_html_close (priv->html_view);
}

static void 
toc_info (YelpViewTOC *view)
{
	YelpViewTOCPriv *priv;
	GNode           *root, *node;
	YelpSection     *section;
	gchar           *string = _("Info Pages");
	gchar           *str_docs = _("Documents");

	priv = view->priv;

	root = yelp_util_find_toplevel (priv->doc_tree, "info");

	if (!root) {
		g_warning ("Unable to find info toplevel");
		return;
	}
	
	node = g_node_first_child (root);

	if (!node) {
		d(g_print ("No first node\n"));
		return;
	}

	yelp_html_clear (priv->html_view);

	toc_page_start (view, string, string);

	yelp_html_printf (priv->html_view, 
			  "<td colspan=\"2\"><h2>%s</h2><ul>", str_docs);

	do {
		gchar *str_uri;
		
		section = YELP_SECTION (node->data);

		str_uri = yelp_uri_to_string (section->uri);
		
		yelp_html_printf (priv->html_view, 
				  "<li><a href=\"%s\">%s</a><br>\n", 
				  str_uri,
				  section->name);
		g_free (str_uri);
	} while ((node = g_node_next_sibling (node)));
	
	yelp_html_printf (priv->html_view,
			  "</ul></td></tr>");

	toc_page_end (view);

	yelp_html_close (priv->html_view);
}

static void
toc_read_important_docs (YelpViewTOC *view)
{
	xmlDocPtr   doc;
	xmlNodePtr  node;
	xmlNodePtr  child;
	xmlNodePtr  section;
	xmlNodePtr  title;
	xmlChar    *prop;
	YelpImportantDocsSection *important_section;
	
	doc = xmlParseFile (DATADIR "/yelp/important_docs.xml");
	if (doc == NULL) {
		return;
	}

	node = xmlDocGetRootElement (doc);
	if (node == NULL) {
		xmlFreeDoc(doc);
		return;
	}

	if (strcmp (node->name, "docs") != 0) {
		xmlFreeDoc(doc);
		return;
	}

	section = node->children;
	while (section) {
		if (strcmp (section->name, "section") == 0) {
			important_section = g_new0 (YelpImportantDocsSection, 1);
			child = section->children;
			while (child) {
				if (strcmp (child->name, "title") == 0) {
					title = child->children;
					if (title && title->type == XML_TEXT_NODE)
						important_section->title = g_strdup (title->content);
				} else if (strcmp (child->name, "document") == 0) {
					prop = xmlGetProp (child, "seriesid");
					if (prop) {
						important_section->seriesids =
							g_list_append (important_section->seriesids, g_strdup (prop));
						xmlFree (prop);
					}
					
				}
				child = child->next;

			}
			view->priv->important_sections =
				g_list_append (view->priv->important_sections,
					       important_section);
		}
		section = section->next;
	}
	
	xmlFreeDoc(doc);
}

static void
toc_uri_selected_cb (YelpHtml    *html,
		     YelpURI     *uri,
		     gboolean     handled,
		     YelpViewTOC *view)
{
	g_signal_emit_by_name (view, "uri_selected", uri, FALSE);
}

static void
toc_html_title_changed_cb (YelpHtml    *html, 
			   const gchar *title,
			   YelpViewTOC *view)
{
	g_signal_emit_by_name (view, "title_changed", title);
}

static void
toc_page_start (YelpViewTOC *view, const gchar *title, const gchar *heading)
{
	YelpViewTOCPriv *priv;
	
	g_return_if_fail (YELP_IS_VIEW_TOC (view));
	g_return_if_fail (title != NULL);
	g_return_if_fail (heading != NULL);
	
	priv = view->priv;
	
	yelp_html_printf (priv->html_view,
			  "<html>\n"
			  "<head>\n"
			  "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n"
			  "<title>%s</title>\n"
			  "</head>\n"
			  "<body marginwidth=\"0\"\n"
			  "background=\"file:" IMAGEDIR "/bcg.png\" marginheight=\"0\"\n"
			  "border=\"10\" topmargin=\"0\" leftmargin=\"0\">\n<br><br>"
			  "<table width=\"100%%\" border=\"0\" cellpadding=\"4\" cellspacing=\"0\">\n"
			  "<tr valign=\"top\">\n"
			  "<td width=\"75\">\n"
			  "<img alt=\"\" src=\"file:" IMAGEDIR "/empty.png\" width=\"75\" height=\"1\">\n"
			  "</td>\n"
			  "<td colspan=\"2\"><h1>%s</h1></td>\n"
			  "</tr>\n"
			  "<tr valign=\"top\">\n"
			  "<td width=\"75\">\n"
			  "<img alt=\"\" src=\"file:" IMAGEDIR "/empty.png\" width=\"75\" height=\"1\">\n"
			  "</td>\n",
			  title,
			  heading);
}

static void
toc_page_end (YelpViewTOC *view)
{
	YelpViewTOCPriv *priv;
	gchar           *str_gnome_is  = _("GNOME is");
	gchar           *str_fs        = _("Free Software");
	gchar           *str_copyright = _("(C)");
	
	g_return_if_fail (YELP_IS_VIEW_TOC (view));
	
	priv = view->priv;
	
	yelp_html_printf (priv->html_view,
			  "<tr>\n"
			  "<td width=\"75\">\n"
			  "<img alt=\"\" src=\"file:" IMAGEDIR "/empty.png\" width=\"75\" height=\"1\">\n"
			  "</td>\n"
			  "<td colspan=\"2\">\n"
			  "<img src=\"file:" IMAGEDIR "/empty.png\" height=\"200\">\n"
			  "</td>\n"
			  "<td></td></tr>\n"
			  "<tr valign=\"top\">\n"
			  "<td width=\"75\"><img alt=\"\" src=\"file:" IMAGEDIR "/empty.png\" width=\"75\" height=\"1\"></td>\n"
			  "</tr>\n"
			  "</table></body></html>\n",
			  str_gnome_is,
			  str_fs,
			  str_copyright);
}


static void 
toc_scrollkeeper (YelpViewTOC *view, GNode *root)
{
	YelpViewTOCPriv *priv;
	GNode           *node, *child;
	YelpSection     *section;
	gboolean         got_a_leaf;
	char            *path;
	gchar           *name;
	gboolean         sub_started = FALSE;
	gchar           *str_docs = _("Documents");
	gchar           *str_subcats = _("Categories");
		
	priv = view->priv;
	
	if (root->children == NULL) {
		return;
	}
	
	name = toc_full_path_name (view, root);

	yelp_html_clear (priv->html_view);

	toc_page_start (view, name, name);

	yelp_html_printf (priv->html_view, "<td colspan=\"2\">");

	g_free (name);

	got_a_leaf = FALSE;
	node = root->children;

	for (node = root->children; node; node = node->next) {
		
		if (node->children != NULL) {
			if (!sub_started) {
				yelp_html_printf (priv->html_view, "<h2>%s</h2><ul>", 
						  str_subcats);
						      
				sub_started = TRUE;
			}
			
			child = node->children;
			
			section = node->data;

			path = yelp_util_node_to_string_path (node);

			yelp_html_printf (priv->html_view, 
					  "<li><a href=\"toc:scrollkeeper/%s\">%s</a>\n", 
					  path, section->name);
			g_free (path);
		} else {
			got_a_leaf = TRUE;
		}
	}

	if (sub_started) {
		yelp_html_printf (priv->html_view,
				  "</ul><br><img alt=\"\" src=\"file:" IMAGEDIR "/empty.png\" height=\"10\"><br>\n");
	}

	if (got_a_leaf) {
		yelp_html_printf (priv->html_view,
				  "<h2>%s</h2><ul>", 
				  str_docs);
		
		for (node = root->children; node; node = node->next) {
			if (node->children == NULL) {
				gchar *str_uri;
				
				YelpSection *section;
			
				section = node->data;
				str_uri = yelp_uri_to_string (section->uri);
				
				yelp_html_printf (priv->html_view, 
						  "<li><a href=\"%s\">%s</a>\n", 
						  str_uri,
						  section->name);
				g_free (str_uri);
			}
		}
	}

	yelp_html_printf (priv->html_view,
			  "</ul></td></tr>");
	
	toc_page_end (view);

	yelp_html_close (priv->html_view);
}

static void
toc_show_uri (YelpView *view, YelpURI *uri, GError **error)
{
	YelpViewTOCPriv *priv;
	GNode           *node;
	const gchar     *path;
	
	g_return_if_fail (YELP_IS_VIEW_TOC (view));

	g_assert (yelp_uri_get_type (uri) == YELP_URI_TYPE_TOC);

	priv = YELP_VIEW_TOC(view)->priv;

	yelp_html_set_base_uri (priv->html_view, uri);

	path = yelp_uri_get_path (uri);

	d(g_print ("PATH:[%s]\n", path));
	
	if (!strcmp (path, "")) {
		toc_start (YELP_VIEW_TOC (view));
	} 
	else if (strncmp (path, "man", 3) == 0) {
		path += 3;
		if (path[0] == 0) {
 			toc_man_1 (YELP_VIEW_TOC (view));
		} else if (path[0] == '/') {
			/* Calculate where it should go */
			path++;

			node = yelp_util_string_path_to_node  (path,
							       priv->doc_tree);
			if (node) {
				toc_man_2 (YELP_VIEW_TOC (view), node);
			} else {
				g_warning ("Bad path in toc url %s\n", 
					   yelp_uri_to_string (uri));
			}
		}
	} else if (strcmp (path, "info") == 0) {
		toc_info (YELP_VIEW_TOC (view));
	} else if (strncmp (path, "scrollkeeper", strlen ("scrollkeeper")) == 0) {
		path = path + strlen ("scrollkeeper");
		if (path[0] == '/') {
			/* Calculate where it should go */
			path++;
			
			node = yelp_util_string_path_to_node  (path,
							       priv->doc_tree);
			if (node) {
				toc_scrollkeeper (YELP_VIEW_TOC (view), node);
			} else {
				g_warning ("Bad path in toc url %s\n", 
					   yelp_uri_to_string (uri));
			}
		}
	} else {
		g_warning ("Unknown toc type %s\n", 
			   yelp_uri_to_string (uri));
	}
}

YelpView *
yelp_view_toc_new (GNode *doc_tree)
{
	YelpViewTOC     *view;
	YelpViewTOCPriv *priv;

	view = g_object_new (YELP_TYPE_VIEW_TOC, NULL);

	priv = view->priv;

	priv->doc_tree = doc_tree;

	return YELP_VIEW (view);
}
