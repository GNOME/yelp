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

#define d(x)

static void   toc_init                      (YelpViewTOC      *html);
static void   toc_class_init                (YelpViewTOCClass *klass);
static void   toc_link_clicked_cb           (HtmlDocument     *doc,
					     const gchar      *url,
					     YelpViewTOC      *view);
static void   toc_man_1                     (YelpViewTOC      *view);
static void   toc_man_2                     (YelpViewTOC      *view,
					     GNode            *root);
static void   toc_read_important_docs       (YelpViewTOC      *view);

enum {
	URI_SELECTED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

typedef struct {
	char  *title;
	GList *seriesids;
} YelpImportantDocsSection;

#define BUFFER_SIZE 4096

struct _YelpViewTOCPriv {
	GtkWidget    *html_view;
	HtmlDocument *doc;
	GNode        *doc_tree;
	GList        *important_sections;
	char          buffer[BUFFER_SIZE];
	int           buffer_pos;
};

/* HTML generation stuff */
#define BG_COLOR "#ffffff"
#define BLOCK_BG_COLOR "#c1c1c1"
#define HELP_IMAGE "file:///gnome/head/INSTALL/share/pixmaps/gnome-help.png"

#define PAGE_HEADER "<html>\n \
  <head>\n \
    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n \
    <title>\n \
      %s\n \
    </title> \n \
    <style type=\"text/css\">\n \
      A:link { color: #00008b; font-size: 10 }\n \
      A:visited { color: #00008b; font-size: 10}\n \
      A:active { color: #00008b; font-size: 10}\n \
      h1 { font-size: 25 }\n \
      h2 { font-size: 15 }\n \
      BODY { color: #000000; font-size: 10 }\n \
    </style>\n \
  </head>\n \
  <body bgcolor=\"" BG_COLOR "\">"

#define PAGE_START "<table cellpadding=\"0\" \n\
			      border=\"0\" \n \
 			      align=\"center\" \n \
			      cellspacing=\"10\" \n\
			      width=\"100%\"> \n\
			      <tr align=\"left\" \n \
			          valign=\"top\" \n \
			          bgcolor=\"" BG_COLOR "\"> \n\
			      <td height=\"69\" \n\
			          align=\"center\" \n\
			          valign=\"center\" \n\
			          bgcolor=\"" BLOCK_BG_COLOR "\" \n\
			          colspan=\"2\"> \n\
			        <h1> \n\
			          %s \n\
			        </h1> \n\
			      </td> \n\
			      </tr> \n\
			      <tr>\n"

#define PAGE_END "</tr></table>\n"

#define COLUMN_LEFT_START "<td valign=\"top\" width=\"50%\"> \n\
            <table cellpadding=\"0\" \n\
                   border=\"0\" \n\
		   align=\"left\" \n\
		   cellspacing=\"0\" \n\
		   width=\"100%\"> \n\
              <tr> \n\
	        <td>\n"
#define COLUMN_RIGHT_START "<td valign=\"top\"> \n\
			      <table cellpadding=\"0\" \n\
			             border=\"0\" \n\
			             align=\"right\" \n\
			             cellspacing=\"0\" \n\
			             width=\"100%\"><tr><td>\n"

#define COLUMN_END " \n\
		</td> \n\
              </tr> \n\
            </table> \n\
          </td>\n"

#define TOC_BLOCK_START " \
		  <table cellpadding=\"5\" \n\
		         border=\"0\" \n\
			 align=\"left\" \n\
			 cellspacing=\"0\" \n\
			 width=\"100%\" \n\
			 bgcolor=\"" BLOCK_BG_COLOR "\"> \n\
		    <tr> \n\
		      <td>\n"

#define TOC_BLOCK_END "\n \
		      </td> \n\
		    </tr> \n\
		  </table>\n "

#define TOC_BLOCK_SEPARATOR " \n\
                </td>\n \
	      </tr> \n\
              <tr> \n\
                <td>&nbsp;</td> \n\
              </tr> \n\
	      <tr>\n\
	        <td>\n"

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
                                (GClassInitFunc) toc_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpViewTOC),
                                0,
                                (GInstanceInitFunc) toc_init,
                        };
                
                view_type = g_type_register_static (HTML_TYPE_VIEW,
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

	priv->doc = html_document_new ();

	html_view_set_document (HTML_VIEW (view), priv->doc);

	g_signal_connect (G_OBJECT (priv->doc), "link_clicked",
			  G_CALLBACK (toc_link_clicked_cb), view);
}

static void
toc_class_init (YelpViewTOCClass *klass)
{
	signals[URI_SELECTED] = 
		g_signal_new ("uri_selected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpViewTOCClass,
					       uri_selected),
			      NULL, NULL,
			      yelp_marshal_VOID__POINTER_BOOLEAN,
			      G_TYPE_NONE,
			      2, G_TYPE_POINTER, G_TYPE_BOOLEAN);
}

static void
toc_link_clicked_cb (HtmlDocument *doc, const gchar *url, YelpViewTOC *view)
{
	YelpURI *uri;
	
	g_return_if_fail (HTML_IS_DOCUMENT (doc));
	g_return_if_fail (url != NULL);
	g_return_if_fail (YELP_IS_VIEW_TOC (view));

	d(g_print ("Link clicked: %s\n", url));

	uri = yelp_uri_new (url);
	g_signal_emit (view, signals[URI_SELECTED], 0, uri, FALSE);
	yelp_uri_unref (uri);
}

static void
toc_open (YelpViewTOC *view)
{
	html_document_open_stream (view->priv->doc, "text/html");
	view->priv->buffer_pos = 0;
}

static void
toc_close (YelpViewTOC *view)
{
	YelpViewTOCPriv *priv = view->priv;
	
	if (priv->buffer_pos > 0) {
		html_document_write_stream (priv->doc, priv->buffer, priv->buffer_pos);
		priv->buffer_pos = 0;
	}

		
	html_document_close_stream (view->priv->doc);
}

static void
toc_write (YelpViewTOC *view, char *data, int len)
{
	YelpViewTOCPriv *priv = view->priv;
	int chunk_size;
	
	if (len < 0) {
		len = strlen (data);
	}

	d(g_print ("%.*s", len,data));

	while (len > 0) {
		chunk_size = MIN (BUFFER_SIZE - priv->buffer_pos, len);
		
		memcpy (priv->buffer + priv->buffer_pos, data, chunk_size);
		priv->buffer_pos += chunk_size;
		len -= chunk_size;
		data += chunk_size;

		if (priv->buffer_pos == BUFFER_SIZE) {
			html_document_write_stream (priv->doc, 
						    priv->buffer, 
						    BUFFER_SIZE);
			priv->buffer_pos = 0;
		}
	}
}

static void
toc_printf (YelpViewTOC *view, char *format, ...)
{
	va_list  args;
	gchar   *string;
	
	g_return_if_fail (format != NULL);
	
	va_start (args, format);
	string = g_strdup_vprintf (format, args);
	va_end (args);
	
	toc_write (view, string, -1);
	
	g_free (string);
}

static void
toc_write_footer (YelpViewTOC *view)
{
	char *footer="\n"
		"  </body>\n"
		"</html>\n";
	toc_write (view, footer, -1);
	
}

static void 
toc_start (YelpViewTOC *view)
{
	YelpViewTOCPriv *priv;
	GNode           *node;
	YelpSection     *section;
	char            *seriesid;
	GNode           *root;
	char            *path;
	GList           *sections, *seriesids;
	gchar           *page_title = _("Help Contents");
	gchar           *section_gnome = _("GNOME - Desktop");
	gchar           *section_additional = _("Additional documents");
	gchar           *man_string = _("Manual Pages");
	gchar           *info_string = _("Info Pages");
	YelpImportantDocsSection *important_section;
	gboolean         important_doc_installed = FALSE;
	gboolean         left_column_started = FALSE;
	
	priv = view->priv;

	if (!g_node_first_child (priv->doc_tree)) {
		g_warning ("No nodes in tree");
	}
	
	toc_open (view);
	
	toc_printf (view, PAGE_HEADER, page_title);
	
	toc_printf (view, 
		    PAGE_START,
		    page_title);
	
	sections = priv->important_sections;
	
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
				toc_printf (view, 
					    COLUMN_LEFT_START);
				left_column_started = TRUE;
			}
			
			toc_printf (view, 
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
				section = node->data;
				toc_printf (view, 
					    "<a href=\"%s\">%s</a>\n",
					    yelp_uri_to_string (section->uri), section->name);
			}

			seriesids = seriesids->next;
		}

		if (important_doc_installed) {
			toc_printf (view, 
				    TOC_BLOCK_END);
			if (sections->next) {
				toc_printf (view, 
					    TOC_BLOCK_SEPARATOR);
			}
		}
		
		sections = sections->next;
		important_doc_installed = FALSE;
	}

	if (left_column_started) {
		toc_printf (view, COLUMN_END);
	}
	
	toc_printf (view, 
		    COLUMN_RIGHT_START);

	root = yelp_util_find_toplevel (priv->doc_tree, "scrollkeeper");
	node = g_node_first_child (root);

	toc_printf (view,
		    TOC_BLOCK_SEPARATOR
		    TOC_BLOCK_START
		    "<h2>%s</h2>\n",
		    section_gnome);
			      
	root = yelp_util_find_node_from_name (priv->doc_tree, "GNOME");
	node = g_node_first_child (root);

	while (node) {
		section = YELP_SECTION (node->data);
		path = yelp_util_node_to_string_path (node);
		toc_printf (view, 
			    "<a href=\"toc:scrollkeeper/%s\">%s</a><br>\n", 
			    path, section->name);
		g_free (path);
		
		node = g_node_next_sibling (node);
	}

	toc_printf (view,
		    TOC_BLOCK_END);

	toc_printf (view,
		    TOC_BLOCK_SEPARATOR
		    TOC_BLOCK_START
		    "<h2>%s</h2>\n",
		    section_additional);

	root = yelp_util_find_toplevel (priv->doc_tree, "scrollkeeper");
	node = g_node_first_child (root);

	while (node) {
		section = YELP_SECTION (node->data);
		if (strcmp (section->name, "GNOME")) {
			path = yelp_util_node_to_string_path (node);
			toc_printf (view, 
				    "<a href=\"toc:scrollkeeper/%s\">%s</a><br>\n", 
				    path, section->name);
			g_free (path);
			
		}
		node = g_node_next_sibling (node);
	}

	if (yelp_util_find_toplevel (priv->doc_tree, "man")) {
		toc_printf (view,
			    "<a href=\"toc:man\">%s</a><br>\n",
			    man_string);
	}
		
	if (yelp_util_find_toplevel (priv->doc_tree, "info")) {
		toc_printf (view,
			    "<a href=\"toc:info\">%s</a><br>\n",
			    info_string);
	}

	toc_printf (view,
		    TOC_BLOCK_END
		    COLUMN_END 
		    PAGE_END);
	
	toc_write_footer (view);

	toc_close (view);
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
				toc_printf (view, TOC_BLOCK_START);
				toc_printf (view, "<h3>%s</h3>", 
					    str_subcats);
				
				sub_started = TRUE;
			}
			
			child = node->children;
			
			section = node->data;

			path = yelp_util_node_to_string_path (node);
			toc_printf (view, "<a href=\"toc:man/%s\">%s</a><br>\n", path, section->name);
			g_free (path);
		} else {
			got_a_leaf = TRUE;
		}
	} while ((node = node->next) != NULL);

	if (sub_started) {
		toc_printf (view, TOC_BLOCK_END);
		toc_printf (view, TOC_BLOCK_SEPARATOR);
	}

	if (got_a_leaf) {
		toc_printf (view,  TOC_BLOCK_START);
		toc_printf (view, "<h3>%s</h3>",
			    str_docs);

		toc_write (view, "</td></tr><tr><td>", -1);
		
		toc_write (view,
			   "<table cellpadding=\"2\" cellspacing=\"2\" border=\"0\" width=\"100%%\">\n",
			   -1);

		i = 0;
		node = first;
		do {
			if (node->children == NULL) {
				YelpSection *section;
				
				if (i % 3 == 0) {
					if (i == 0) {
						toc_write (view, 
							   "<tr>\n",
							   -1);
					} else {
						toc_write (view,
							   "</tr>\n<tr>\n", 
							   -1);
					}
				}
			
				section = node->data;
				url = yelp_util_compose_path_url (node->parent, yelp_uri_get_path (section->uri));
				toc_printf (view, "<td valign=\"Top\"><a href=\"%s\">%s</a></td>\n", yelp_uri_to_string (section->uri), 
					    section->name);
/* 				toc_printf (view, "<td valign=\"Top\"><a href=\"%s\">%s</a></td>\n", url, section->name); */
				g_free (url);
				i++;
			}
		} while ((node = node->next) != NULL);
		
		toc_write (view, "</tr>\n", -1);
		toc_write (view, "</table>\n", -1);

		toc_printf (view, TOC_BLOCK_END);
	}
}

static void 
toc_man_2 (YelpViewTOC *view,
	   GNode       *root)
{
	GNode *first;
	gchar *name;
	gchar *string = _("Manual pages");

	if (root->children == NULL) {
		return;
	}
	
	first = root->children;

	toc_open (view);
	
	toc_printf (view, PAGE_HEADER, string);
		
	name = toc_full_path_name (view, root);

	toc_printf (view, 
		    "<table cellpadding=\"0\"\n"
		    "border=\"0\"\n"
		    "align=\"center\"\n"
		    "cellspacing=\"10\"\n"
		    "width=\"100%\">\n"
		    "<tr align=\"left\"\n"
		    "    valign=\"top\"\n"
		    "    bgcolor=\"" BG_COLOR "\">\n"
		    "<td height=\"69\"\n"
		    "    align=\"center\"\n"
		    "    valign=\"center\"\n"
		    "    bgcolor=\"" BLOCK_BG_COLOR "\"\n"
		    "    colspan=\"2\">\n"
		    "  <h2>\n"
		    "    %s/%s\n"
		    "  </h2>\n"
		    "</td>\n"
		    "</tr>\n"
		    "<tr>\n",
		    string, name);

	g_free (name);

	toc_printf (view, COLUMN_RIGHT_START);

	toc_man_emit (view, first);
		
	toc_printf (view, 
		    COLUMN_END
		    PAGE_END);

	toc_write_footer (view);
	toc_close (view);
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
		return;
	}

	toc_open (view);
	
	toc_printf (view, PAGE_HEADER, string);

	toc_printf (view, 
		    PAGE_START, 
		    string);

	toc_printf (view,
		    COLUMN_RIGHT_START
		    TOC_BLOCK_START 
		    "<h2>%s</h2>", 
		    str_subcats);

	do {
		child = g_node_first_child (node);
		
		if (child) {
			section = YELP_SECTION (node->data);
			path = yelp_util_node_to_string_path (node);

 			toc_printf (view,
				    "<a href=\"toc:man/%s\">%s</a><br>\n",
				    path, section->name);
			g_free (path);
		}
	} while ((node = g_node_next_sibling (node)));

	toc_printf (view, 
		    TOC_BLOCK_END
		    COLUMN_END
		    PAGE_END);

	toc_write_footer (view);
	toc_close (view);
}

static void 
toc_info (YelpViewTOC *view)
{
	YelpViewTOCPriv *priv;
	GNode           *root, *node;
	YelpSection     *section;
	char            *url;
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

	toc_open (view);
	
	toc_printf (view, PAGE_HEADER, string);

	toc_printf (view, PAGE_START, string);
	toc_printf (view, COLUMN_RIGHT_START);
	toc_printf (view, TOC_BLOCK_START);

	toc_printf (view, "<h2>%s</h2>", str_docs);

	do {
		section = YELP_SECTION (node->data);
		url = yelp_util_compose_path_url (root,
						  yelp_uri_get_path (section->uri));
		
		toc_printf (view, 
			    "<a href=\"%s\">%s</a><br>\n", 
			    yelp_uri_to_string (section->uri),
			    section->name);
/* 		toc_printf (view,  */
/* 				      "<a href=\"%s\">%s</a><br>\n",  */
/* 				      url, section->name); */
		g_free  (url);
	} while ((node = g_node_next_sibling (node)));
		
	toc_printf (view, 
		    TOC_BLOCK_END
		    COLUMN_END
		    PAGE_END);

	toc_write_footer (view);
	toc_close (view);
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

GtkWidget *
yelp_view_toc_new (GNode *doc_tree)
{
	YelpViewTOC     *view;
	YelpViewTOCPriv *priv;
	YelpURI         *uri;

	view = g_object_new (YELP_TYPE_VIEW_TOC, NULL);

	priv = view->priv;

	priv->doc_tree = doc_tree;

	uri = yelp_uri_new ("toc:");
	
	yelp_view_toc_open_uri (view, uri);
	yelp_uri_unref (uri);
	
	return GTK_WIDGET (view);
}

static void 
toc_scrollkeeper (YelpViewTOC *view,
		  GNode *root)
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
	
	toc_open (view);
	
	name = toc_full_path_name (view, root);

	toc_printf (view, PAGE_HEADER, name);

	toc_printf (view, PAGE_START, name);
	
	g_free (name);

	toc_printf (view, COLUMN_RIGHT_START);

	got_a_leaf = FALSE;
	node = root->children;

/* 	toc_printf (view, TOC_BLOCK_START); */

	for (node = root->children; node; node = node->next) {
		
		if (node->children != NULL) {
			if (!sub_started) {
				toc_printf (view, TOC_BLOCK_START);
				toc_printf (view, "<h2>%s</h2>", 
					    str_subcats);
						      
				sub_started = TRUE;
			}
			
			child = node->children;
			
			section = node->data;

			path = yelp_util_node_to_string_path (node);
			toc_printf (view, "<a href=\"toc:scrollkeeper/%s\">%s</a><br>\n", path, section->name);
			g_free (path);
		} else {
			got_a_leaf = TRUE;
		}
	}

	if (sub_started) {
		toc_printf (view, TOC_BLOCK_END);
	}

/* 	toc_printf (view, TOC_BLOCK_END); */

	if (got_a_leaf) {
		toc_printf (view, TOC_BLOCK_START);
		toc_printf (view, "<h2>%s</h2>", str_docs);
		
/* 		toc_write (view, "<ul>\n", -1); */

		for (node = root->children; node; node = node->next) {
			if (node->children == NULL) {
				YelpSection *section;
			
				section = node->data;
				toc_printf (view, "<a href=\"%s\">%s</a><br>\n", yelp_uri_to_string (section->uri), section->name);
			}
		}

		toc_printf (view, TOC_BLOCK_END);
	}

	toc_printf (view, 
		    COLUMN_END
		    PAGE_END);
	toc_write_footer (view);
	toc_close (view);
}

void
yelp_view_toc_open_uri (YelpViewTOC *view, YelpURI *uri)
{
	GNode       *node;
	const gchar *path;
	
	g_assert (yelp_uri_get_type (uri) == YELP_URI_TYPE_TOC);

	path = yelp_uri_get_path (uri);

	d(g_print ("PATH:[%s]\n", path));
	
	if (!strcmp (path, "")) {
		toc_start (view);
	} 
	else if (strncmp (path, "man", 3) == 0) {
		path += 3;
		if (path[0] == 0) {
 			toc_man_1 (view);
		} else if (path[0] == '/') {
			/* Calculate where it should go */
			path++;

			node = yelp_util_string_path_to_node  (path,
							       view->priv->doc_tree);
			if (node) {
				toc_man_2 (view, node);
			} else {
				g_warning ("Bad path in toc url %s\n", 
					   yelp_uri_to_string (uri));
			}
		}
	} else if (strcmp (path, "info") == 0) {
		toc_info (view);
	} else if (strncmp (path, "scrollkeeper", strlen ("scrollkeeper")) == 0) {
		path = path + strlen ("scrollkeeper");
		if (path[0] == '/') {
			/* Calculate where it should go */
			path++;
			
			node = yelp_util_string_path_to_node  (path,
							       view->priv->doc_tree);
			if (node) {
				toc_scrollkeeper (view, node);
			} else {
				g_warning ("Bad path in toc url %s\n", 
					   yelp_uri_to_string (uri));
			}
		}
	} else {
		g_warning ("Unknown toc type %s\n", 
			   yelp_uri_to_string (uri));
	}

 	gtk_adjustment_set_value (
 		gtk_layout_get_vadjustment (GTK_LAYOUT (view)), 0);
}
