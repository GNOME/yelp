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

#include <atk/atk.h>
#include <gtk/gtkaccessible.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

#include "yelp-index-model.h"
#include "yelp-html.h"
#include "yelp-marshal.h"
#include "yelp-reader.h"
#include "yelp-view-index.h"

#define d(x)

static void     yvi_init                       (YelpViewIndex       *view);
static void     yvi_class_init                 (YelpViewIndexClass  *klass);
static void     yvi_index_selection_changed_cb (GtkTreeSelection    *selection,
						YelpViewIndex       *content);
static void     yvi_html_uri_selected_cb       (YelpHtml            *html,
						YelpURI             *uri,
						gboolean             handled,
						YelpViewIndex       *view);
static void     yvi_entry_changed_cb           (GtkEntry            *entry,
						YelpViewIndex       *view);
static void     yvi_entry_activated_cb         (GtkEntry            *entry,
						YelpViewIndex       *view);
static void     yvi_entry_text_inserted_cb     (GtkEntry            *entry,
						const gchar         *text,
						gint                 length,
						gint                *position,
						YelpViewIndex       *view);
static gboolean yvi_complete_idle              (YelpViewIndex       *view);
static gboolean yvi_filter_idle                (YelpViewIndex       *view);
static gchar *  yvi_complete_func              (YelpSection         *section);
static void     set_relation                   (GtkWidget           *widget,
						GtkLabel            *label);
static void     yvi_reader_data_cb             (YelpReader          *reader,
						const gchar         *data,
						gint                 len,
						YelpViewIndex       *view);
static void     yvi_reader_finished_cb         (YelpReader          *reader,
						YelpURI             *uri,
						YelpViewIndex       *view);


struct _YelpViewIndexPriv {
	/* List of keywords */
	GtkWidget      *index_view;
	YelpIndexModel *model;

	/* Query entry */
	GtkWidget      *entry;
	
	YelpReader     *reader;

	/* Html view */
	YelpHtml       *html_view;
	GtkWidget      *html_widget;

	GCompletion    *completion;

	guint           idle_complete;
	guint           idle_filter;

	gboolean        first;
};

enum {
	URI_SELECTED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

GType
yelp_view_index_get_type (void)
{
        static GType view_type = 0;

        if (!view_type)
        {
                static const GTypeInfo view_info =
                        {
                                sizeof (YelpViewIndexClass),
                                NULL,
                                NULL,
                                (GClassInitFunc) yvi_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpViewIndex),
                                0,
                                (GInstanceInitFunc) yvi_init,
                        };
                
                view_type = g_type_register_static (GTK_TYPE_HPANED,
                                                    "YelpViewIndex", 
                                                    &view_info, 0);
        }
        
        return view_type;
}

static void
yvi_init (YelpViewIndex *view)
{
	YelpViewIndexPriv *priv;
	
	priv = g_new0 (YelpViewIndexPriv, 1);
	view->priv = priv;
	
	priv->idle_complete = 0;
	priv->idle_filter   = 0;

	priv->completion = 
		g_completion_new ((GCompletionFunc) yvi_complete_func);

	priv->index_view = gtk_tree_view_new ();
	priv->model      = yelp_index_model_new ();

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->index_view),
				 GTK_TREE_MODEL (priv->model));

	priv->html_view = yelp_html_new ();
	priv->html_widget = yelp_html_get_widget (priv->html_view);
	
	g_signal_connect (priv->html_view, "uri_selected",
			  G_CALLBACK (yvi_html_uri_selected_cb),
			  view);

	priv->reader = yelp_reader_new ();
	
	g_signal_connect (G_OBJECT (priv->reader), "data",
			  G_CALLBACK (yvi_reader_data_cb),
			  view);
	g_signal_connect (G_OBJECT (priv->reader), "finished",
			  G_CALLBACK (yvi_reader_finished_cb),
			  view);
}

static void
yvi_class_init (YelpViewIndexClass *klass)
{
	signals[URI_SELECTED] =
		g_signal_new ("uri_selected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (YelpViewIndexClass,
					       uri_selected),
			      NULL, NULL,
			      yelp_marshal_VOID__POINTER_BOOLEAN,
			      G_TYPE_NONE,
			      2, G_TYPE_POINTER, G_TYPE_BOOLEAN);
}

static void
yvi_index_selection_changed_cb (GtkTreeSelection *selection, 
				YelpViewIndex    *view)
{
	YelpViewIndexPriv *priv;
 	GtkTreeIter        iter;
	YelpSection       *section;
	
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_return_if_fail (YELP_IS_VIEW_INDEX (view));

	priv = view->priv;

	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		YelpURI *index_uri;
		
		gtk_tree_model_get (GTK_TREE_MODEL (priv->model), &iter,
				    YELP_INDEX_MODEL_COL_SECTION, &section,
				    -1);

		d(g_print ("Index View: selection changed: %s\n", 
			   yelp_uri_to_string (section->uri)));
		
		index_uri = yelp_uri_to_index (section->uri);

  		g_signal_emit (view, signals[URI_SELECTED], 0,
 			       index_uri, FALSE);

		yelp_uri_unref (index_uri);
	}
}

static void
yvi_html_uri_selected_cb (YelpHtml      *html, 
			  YelpURI       *uri, 
			  gboolean       handled,
			  YelpViewIndex *view)
{
	YelpURI *index_uri;

	g_return_if_fail (YELP_IS_VIEW_INDEX (view));

	d(g_print ("Index View: uri selected: %s\n", 
		   yelp_uri_to_string (uri)));

	index_uri = yelp_uri_to_index (uri);
	g_signal_emit (view, signals[URI_SELECTED], 0, index_uri, handled);
	yelp_uri_unref (index_uri);
}

static void
yvi_entry_changed_cb (GtkEntry *entry, YelpViewIndex *view)
{
	YelpViewIndexPriv *priv;
	
	g_return_if_fail (GTK_IS_ENTRY (entry));
	g_return_if_fail (YELP_IS_VIEW_INDEX (view));
	
	priv = view->priv;
 
	d(g_print ("Entry changed\n"));

	if (!priv->idle_filter) {
		priv->idle_filter =
			g_idle_add ((GSourceFunc) yvi_filter_idle, view);
	}
}

static void
yvi_entry_activated_cb (GtkEntry *entry, YelpViewIndex *view)
{
	YelpViewIndexPriv *priv;
	gchar             *str;
	
	g_return_if_fail (GTK_IS_ENTRY (entry));
	g_return_if_fail (YELP_IS_VIEW_INDEX (view));

	priv = view->priv;
	
	str = (gchar *) gtk_entry_get_text (GTK_ENTRY (priv->entry));
	
	yelp_index_model_filter (view->priv->model, str);
}

static void
yvi_entry_text_inserted_cb (GtkEntry      *entry,
			    const gchar   *text,
			    gint           length,
			    gint          *position,
			    YelpViewIndex *view)
{
	YelpViewIndexPriv *priv;
	
 	g_return_if_fail (YELP_IS_VIEW_INDEX (view));
	
	priv = view->priv;
	
	if (!priv->idle_complete) {
		priv->idle_complete = 
			g_idle_add ((GSourceFunc) yvi_complete_idle, view);
	}
}

static gboolean
yvi_complete_idle (YelpViewIndex *view)
{
	YelpViewIndexPriv *priv;
	const gchar       *text;
	gchar             *completed = NULL;
	GList             *list;
	gint               text_length;
	
	g_return_val_if_fail (YELP_IS_VIEW_INDEX (view), FALSE);
	
	priv = view->priv;
	
	text = gtk_entry_get_text (GTK_ENTRY (priv->entry));

	list = g_completion_complete (priv->completion, 
				      (gchar *)text,
				      &completed);

	if (completed) {
		text_length = strlen (text);
		
		gtk_entry_set_text (GTK_ENTRY (priv->entry), completed);
 		gtk_editable_set_position (GTK_EDITABLE (priv->entry),
 					   text_length);
		gtk_editable_select_region (GTK_EDITABLE (priv->entry),
					    text_length, -1);
	}
	
	priv->idle_complete = 0;

	return FALSE;
}

static gboolean
yvi_filter_idle (YelpViewIndex *view)
{
	YelpViewIndexPriv *priv;
	gchar             *str;
	
	g_return_val_if_fail (YELP_IS_VIEW_INDEX (view), FALSE);

	priv = view->priv;

	d(g_print ("Filter idle\n"));
	
	str = (gchar *) gtk_entry_get_text (GTK_ENTRY (priv->entry));
	
	yelp_index_model_filter (view->priv->model, str);

	priv->idle_filter = 0;

	return FALSE;
}

static gchar *
yvi_complete_func (YelpSection *section)
{
	return section->name;
}

/**
 * set_relation
 * @widget : The Gtk widget which is labelled by @label
 * @label : The label for the @widget.
 * Description : This function establishes atk relation
 * between a gtk widget and a label.
 */

static void
set_relation (GtkWidget *widget, GtkLabel *label)
{
	AtkObject      *aobject;
	AtkRelationSet *relation_set;
	AtkRelation    *relation;
	AtkObject      *targets[1];

	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (GTK_IS_LABEL (label));

	aobject = gtk_widget_get_accessible (widget);

	/* Return if GAIL is not loaded */
	if (! GTK_IS_ACCESSIBLE (aobject)) {
		return;
	}
	
	/* Set the ATK_RELATION_LABEL_FOR relation */
	gtk_label_set_mnemonic_widget (label, widget);

	targets[0] = gtk_widget_get_accessible (GTK_WIDGET (label));

	relation_set = atk_object_ref_relation_set (aobject);

	relation = atk_relation_new (targets, 1, ATK_RELATION_LABELLED_BY);
	atk_relation_set_add (relation_set, relation);
	g_object_unref (G_OBJECT (relation));
}

static void
yvi_reader_data_cb (YelpReader    *reader,
		    const gchar   *data,
		    gint           len,
		    YelpViewIndex *view)
{
	YelpViewIndexPriv *priv;
	
	g_return_if_fail (YELP_IS_READER (reader));
	g_return_if_fail (YELP_IS_VIEW_INDEX (view));
	
	priv = view->priv;

	if (priv->first) {
		yelp_html_clear (priv->html_view);
		priv->first = FALSE;
	}

	if (len == -1) {
		len = strlen (data);
	}

	if (len <= 0) {
		return;
	}

	yelp_html_write (priv->html_view, data, len);
}

static void
yvi_reader_finished_cb (YelpReader    *reader,
			YelpURI       *uri,
			YelpViewIndex *view)
{
	YelpViewIndexPriv *priv;
	
	g_return_if_fail (YELP_IS_READER (reader));
	g_return_if_fail (YELP_IS_VIEW_INDEX (view));

	priv = view->priv;
 
	if (!priv->first) {
		yelp_html_close (priv->html_view);
	}
	
	gdk_window_set_cursor (priv->html_widget->window, NULL);
	gtk_widget_grab_focus (priv->html_widget);
}

GtkWidget *
yelp_view_index_new (GList *index)
{
	YelpViewIndex     *view;
	YelpViewIndexPriv *priv;
	GtkTreeSelection  *selection;
        GtkWidget         *html_sw;
        GtkWidget         *list_sw;
	GtkWidget         *frame;
	GtkWidget         *box;
	GtkWidget         *hbox;
	GtkWidget         *label;
		
	view = g_object_new (YELP_TYPE_VIEW_INDEX, NULL);
	priv = view->priv;

	/* Setup the index box */
	box = gtk_vbox_new (FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 0);
	
	label = gtk_label_new (_("Search for:"));

	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

	priv->entry = gtk_entry_new ();

	set_relation (priv->entry, GTK_LABEL (label));

	g_signal_connect (priv->entry, "changed", 
			  G_CALLBACK (yvi_entry_changed_cb),
			  view);

	gtk_box_pack_end (GTK_BOX (hbox), priv->entry, FALSE, FALSE, 0);
	
	g_signal_connect (priv->entry, "activate",
			  G_CALLBACK (yvi_entry_activated_cb),
			  view);
	
	g_signal_connect (priv->entry, "insert-text",
			  G_CALLBACK (yvi_entry_text_inserted_cb),
			  view);

	gtk_box_pack_start (GTK_BOX (box), hbox, 
			    FALSE, FALSE, 0);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	
        list_sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_sw),
                                        GTK_POLICY_AUTOMATIC, 
                                        GTK_POLICY_AUTOMATIC);

	gtk_container_add (GTK_CONTAINER (frame), list_sw);
	
	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (priv->index_view), -1,
		_("Section"), gtk_cell_renderer_text_new (),
		"text", 0,
		NULL);

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->index_view),
					   FALSE);

	selection = gtk_tree_view_get_selection (
		GTK_TREE_VIEW (priv->index_view));

	g_signal_connect (selection, "changed",
			  G_CALLBACK (yvi_index_selection_changed_cb),
			  view);
	
	gtk_container_add (GTK_CONTAINER (list_sw), priv->index_view);

	gtk_box_pack_end_defaults (GTK_BOX (box), frame);

        /* Setup the Html view */
 	html_sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (html_sw),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
	frame = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (frame), html_sw);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	
        gtk_container_add (GTK_CONTAINER (html_sw), priv->html_widget);

	/* Add the tree and html view to the paned */
	gtk_paned_add1 (GTK_PANED (view), box);
        gtk_paned_add2 (GTK_PANED (view), frame);
        gtk_paned_set_position (GTK_PANED (view), 250);
 
	d(g_print ("List length: %d\n", g_list_length (index)));
	
	g_completion_add_items (priv->completion, index);
	yelp_index_model_set_words (priv->model, index);

	return GTK_WIDGET (view);
}

void
yelp_view_index_show_uri (YelpViewIndex  *view,
			  YelpURI        *index_uri,
			  GError        **error)
{
	YelpViewIndexPriv *priv;
	YelpURI           *uri;

	g_return_if_fail (YELP_IS_VIEW_INDEX (view));
	g_return_if_fail (index_uri != NULL);
	
	priv = view->priv;

	d(g_print ("Index show Uri: %s\n", yelp_uri_to_string (index_uri)));

	uri = yelp_uri_from_index (index_uri);

	priv->first = TRUE;

	yelp_html_set_base_uri (priv->html_view, uri);

	if (!yelp_reader_start (priv->reader, uri)) {
		gchar     *loading = _("Loading...");

		yelp_html_clear (priv->html_view);

		yelp_html_printf (priv->html_view, 
				  "<html><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><title>..</title><body><center>%s</center></body></html>", 
				  loading);
		yelp_html_close (priv->html_view);
	}

	/* FIXME: Handle the GError */
/* 	yelp_html_open_uri (priv->html_view, uri, error); */
}

