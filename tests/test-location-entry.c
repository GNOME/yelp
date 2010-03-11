#include <gtk/gtk.h>
#include "yelp-location-entry.h"

enum {
  COL_TITLE,
  COL_DESC,
  COL_ICON,
  COL_FLAGS,
  COL_URI,
  COL_TERMS
};

YelpLocationEntry *entry;
GtkListStore *event_list;

static gboolean
loading_callback (gpointer data)
{
  GtkTreeRowReference *row = (GtkTreeRowReference *) data;
  GtkTreePath *path = gtk_tree_row_reference_get_path (row);
  GtkTreeModel *model;
  GtkTreeIter iter;

  model = gtk_tree_row_reference_get_model (row);
  gtk_tree_model_get_iter (model, &iter, path);

  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      COL_TITLE, "Desktop Help",
		      COL_DESC, "Get help with your desktop",
		      COL_ICON, "gnome-main-menu",
		      COL_FLAGS, YELP_LOCATION_ENTRY_CAN_BOOKMARK,
		      -1);
  gtk_tree_path_free (path);
  gtk_tree_row_reference_free (row);
  return FALSE;
}

static void
check_clicked (GtkToggleButton *button, gpointer user_data)
{
  YelpLocationEntry *entry = YELP_LOCATION_ENTRY (user_data);
  g_object_set (G_OBJECT (entry), "enable-search",
		gtk_toggle_button_get_active (button), NULL);
}

static void
search_clicked (GtkToggleButton *button, gpointer user_data)
{
  YelpLocationEntry *entry = YELP_LOCATION_ENTRY (user_data);
  yelp_location_entry_start_search (entry);
}

static void
button_clicked (GtkButton *button, gpointer user_data)
{
  GtkListStore *model = GTK_LIST_STORE (user_data);
  const gchar *label = gtk_button_get_label (button);
  GtkTreeIter iter;
  gchar *uri = NULL, *icon, *title, *desc;
  gboolean loading = FALSE;

  if (g_str_equal (label, "Empathy"))
    {
      uri = "help:empathy";
      icon = "empathy";
      desc = "Send and receive messages";
    }
  else if (g_str_equal (label, "Calculator"))
    {
      uri = "help:gcalctool";
      icon = "accessories-calculator";
      desc = "Perform calculations";
    }
  else if (g_str_equal (label, "Terminal"))
    {
      uri = "help:gnome-terminal";
      icon = "gnome-terminal";
      desc = "Use the command line";
    }
  else if (g_str_equal (label, "Slow-loading document"))
    {
      uri = "help:gnumeric";
      icon = NULL;
      loading = TRUE;
      desc = NULL;
    }

  if (uri)
    {
      gchar *iter_uri;
      gboolean cont;
      cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter);
      while (cont)
	{
	  gtk_tree_model_get (GTK_TREE_MODEL (model), &iter,
			      COL_URI, &iter_uri,
			      -1);
	  if (iter_uri && g_str_equal (uri, iter_uri))
	    {
	      g_free (iter_uri);
	      break;
	    }
	  else
	    {
	      g_free (iter_uri);
	      cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter);
	    }
	}
      if (cont)
	{
	  GtkTreeIter first;
	  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &first);
	  gtk_list_store_move_before (model, &iter, &first);
	}
      else
	{
	  gtk_list_store_prepend (model, &iter);
	  if (!loading)
	    {
	      title = g_strconcat (label, " Help", NULL);
	    }
	  else
	    {
	      GtkTreePath *path;
	      GtkTreeRowReference *row;
	      title = g_strdup ("Loading...");
	      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
	      row = gtk_tree_row_reference_new (GTK_TREE_MODEL (model), path);
	      gtk_tree_path_free (path);
	      g_timeout_add_seconds (5, loading_callback, row);
	    }
	  gtk_list_store_set (model, &iter,
			      COL_TITLE, title,
			      COL_DESC, desc,
			      COL_ICON, icon,
			      COL_FLAGS, YELP_LOCATION_ENTRY_CAN_BOOKMARK | (loading ? YELP_LOCATION_ENTRY_IS_LOADING : 0),
			      COL_URI, uri,
			      -1);
	  g_free (title);
	}
      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (entry), &iter);
    }
}

static void
location_selected_cb (GtkEntry *entry, gpointer user_data)
{
  GtkTreeModel *model;
  GtkTreeIter iter, first;
  gchar *title, *icon;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (entry));
  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (entry), &iter);
  gtk_tree_model_get_iter_first (model, &first);

  gtk_list_store_move_before (GTK_LIST_STORE (model), &iter, &first);

  gtk_tree_model_get (model, &iter,
		      COL_TITLE, &title,
		      COL_ICON, &icon,
		      -1);

  gtk_list_store_prepend (event_list, &iter);
  gtk_list_store_set (event_list, &iter,
		      0, icon,
		      1, title,
		      -1);
  g_free (icon);
  g_free (title);
}

static void
search_activated_cb (GtkEntry *entry, gchar *text, gpointer user_data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *curtitle = NULL;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (entry));

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter)) {
    gchar *uri;
    gtk_tree_model_get (model, &iter,
			COL_URI, &uri,
			-1);
    if (uri) {
      if (g_str_equal (uri, "search:"))
	gtk_tree_model_get (model, &iter,
			    COL_DESC, &curtitle,
			    -1);
      else
	gtk_tree_model_get (model, &iter,
			    COL_TITLE, &curtitle,
			    -1);
      g_free (uri);
    }
  }

  if (curtitle == NULL)
    curtitle = g_strdup ("in all documents");

  gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      COL_ICON, "folder-saved-search",
		      COL_TITLE, text,
		      COL_DESC, curtitle,
		      COL_FLAGS, 0,
		      COL_URI, "search:",
		      -1);
  gtk_combo_box_set_active_iter (GTK_COMBO_BOX (entry), &iter);
  g_free (curtitle);
}

int
main (int argc, char **argv)
{
  GtkWidget *window, *vbox, *scroll, *list, *hbox, *button;
  GtkTreeViewColumn *col;
  GtkCellRenderer *cell;
  GtkListStore *model;
  GtkTreeIter iter;

  g_thread_init (NULL);
  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 450, 600);
  gtk_container_set_border_width (GTK_CONTAINER (window), 6);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  model = gtk_list_store_new (6,
			      G_TYPE_STRING,  /* title */
			      G_TYPE_STRING,  /* desc */
			      G_TYPE_STRING,  /* icon */
			      G_TYPE_INT,     /* flags */
			      G_TYPE_STRING,  /* uri */
			      G_TYPE_STRING   /* search terms */
			      );
  gtk_list_store_append (model, &iter);
  gtk_list_store_set (model, &iter,
		      COL_FLAGS, YELP_LOCATION_ENTRY_IS_SEPARATOR,
		      -1);
  gtk_list_store_append (model, &iter);
  gtk_list_store_set (model, &iter,
		      COL_ICON, "system-search",
		      COL_TITLE, "Search...",
		      COL_FLAGS, YELP_LOCATION_ENTRY_IS_SEARCH,
		      -1);

  entry = (YelpLocationEntry *) yelp_location_entry_new_with_model (GTK_TREE_MODEL (model),
								    COL_TITLE,
								    COL_DESC,
								    COL_ICON,
								    COL_FLAGS);
  g_signal_connect (entry, "location-selected",
		    G_CALLBACK (location_selected_cb), NULL);
  g_signal_connect (entry, "search-activated",
		    G_CALLBACK (search_activated_cb), NULL);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (entry), FALSE, FALSE, 0);

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);
  event_list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (event_list));
  col = gtk_tree_view_column_new ();
  gtk_tree_view_append_column (GTK_TREE_VIEW (list), col);
  cell = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (col), cell, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (col), cell,
				  "icon-name", 0,
				  NULL);
  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (col), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (col), cell,
				  "text", 1,
				  NULL);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (list), FALSE);
  gtk_container_add (GTK_CONTAINER (scroll), list);

  hbox = gtk_hbox_new (TRUE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  button = gtk_check_button_new_with_label ("Enable search");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (check_clicked), entry);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Search");
  g_signal_connect (button, "clicked",
		    G_CALLBACK (search_clicked), entry);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);

  hbox = gtk_hbox_new (TRUE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Empathy");
  g_signal_connect (button, "clicked",
		    G_CALLBACK (button_clicked), model);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Calculator");
  g_signal_connect (button, "clicked",
		    G_CALLBACK (button_clicked), model);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Terminal");
  g_signal_connect (button, "clicked",
		    G_CALLBACK (button_clicked), model);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Slow-loading document");
  g_signal_connect (button, "clicked",
		    G_CALLBACK (button_clicked), model);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  gtk_widget_show_all (GTK_WIDGET (window));
  gtk_widget_grab_focus (list);

  gtk_main ();
}
