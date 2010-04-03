/*
 *  Copyright (C) 2003, 2004 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "config.h"

#include "gtkentryaction.h"

#include <gtk/gtk.h>

#define GTK_ENTRY_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), GTK_TYPE_ENTRY_ACTION, GtkEntryActionPrivate))

struct _GtkEntryActionPrivate
{
  char *text;
  gboolean editable;
  GtkWidget *entry_widget;
};

static void  gtk_entry_action_init        (GtkEntryAction      *action);
static void  gtk_entry_action_class_init  (GtkEntryActionClass *class);
static void  changed_cb                   (GtkEntry            *entry,
					   GtkEntryAction      *action);

enum
{
  PROP_0,
  PROP_TEXT,
  PROP_EDITABLE,
};

static GObjectClass *parent_class = NULL;

GType
gtk_entry_action_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
    {
      static const GTypeInfo type_info =
	{
	  sizeof (GtkEntryActionClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) gtk_entry_action_class_init,
	  (GClassFinalizeFunc) NULL,
	  NULL,
	  sizeof (GtkEntryAction),
	  0, /* n_preallocs */
	  (GInstanceInitFunc) gtk_entry_action_init,
	};

      type = g_type_register_static (GTK_TYPE_ACTION,
				     "GtkEntryAction",
				     &type_info, 0);
    }

  return type;
}

static GtkWidget *
gtk_entry_action_create_tool_item (GtkAction *action)
{
  GtkToolItem *tool_item;
  GtkWidget *entry;
  GtkWidget *box;
  GtkWidget *label;
  GtkEntryAction *action_cast = (GtkEntryAction *)action;

  tool_item = gtk_tool_item_new ();
  box = gtk_hbox_new (FALSE, 6);
  label = gtk_label_new ("");
  entry = gtk_entry_new ();
  action_cast->priv->entry_widget = entry;
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
  gtk_container_add (GTK_CONTAINER (tool_item), box);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), entry, FALSE, FALSE, 0);
  gtk_widget_show (entry);
  gtk_widget_show (box);

  g_object_set_data (G_OBJECT (tool_item), "label", label);
  g_object_set_data (G_OBJECT (tool_item), "entry", entry);

  return GTK_WIDGET (tool_item);
}

static gboolean
check_widget (GtkWidget *widget)
{
  return GTK_IS_TOOL_ITEM (widget) && 
    GTK_IS_LABEL (g_object_get_data (G_OBJECT (widget), "label")) &&
    GTK_IS_ENTRY (g_object_get_data (G_OBJECT (widget), "entry"));
}

static void
sync_label (GtkAction *gaction,
	    GParamSpec *pspec,
	    GtkWidget *proxy)
{
  GtkEntryAction *action = GTK_ENTRY_ACTION (gaction);
  GtkToolItem *item = GTK_TOOL_ITEM (proxy);
  GtkLabel *label = GTK_LABEL (g_object_get_data (G_OBJECT (item), "label"));
  char *text = NULL;

  g_object_get (action, "label", &text, NULL);
  gtk_label_set_text_with_mnemonic (label, text);
  if (text && *text)
    gtk_widget_show (GTK_WIDGET (label));
  else
    gtk_widget_hide (GTK_WIDGET (label));
  g_free (text);
}

static void
sync_text (GtkAction *gaction,
	   GParamSpec *pspec,
	   GtkWidget *proxy)
{
  GtkEntryAction *action = GTK_ENTRY_ACTION (gaction);
  GtkToolItem *item = GTK_TOOL_ITEM (proxy);
  GtkEntry *entry = GTK_ENTRY (g_object_get_data (G_OBJECT (item), "entry"));

  g_signal_handlers_block_by_func (entry, G_CALLBACK (changed_cb), action);
  gtk_entry_set_text (entry, action->priv->text);
  g_signal_handlers_unblock_by_func (entry, G_CALLBACK (changed_cb), action);
}

static void
sync_editable (GtkAction *gaction,
	       GParamSpec *pspec,
	       GtkWidget *proxy)
{
  GtkEntryAction *action = GTK_ENTRY_ACTION (gaction);
  GtkToolItem *item = GTK_TOOL_ITEM (proxy);
  GtkEntry *entry = GTK_ENTRY (g_object_get_data (G_OBJECT (item), "entry"));

  gtk_editable_set_editable (GTK_EDITABLE (entry), action->priv->editable);
}

static void
changed_cb (GtkEntry *entry, GtkEntryAction *action)
{
  const char *text;
  GtkWidget *proxy;

  text = gtk_entry_get_text (entry);
  proxy = gtk_widget_get_parent (GTK_WIDGET (entry));

  g_signal_handlers_block_by_func (action, G_CALLBACK (sync_text), proxy);
  gtk_entry_action_set_text (action, text);
  g_signal_handlers_unblock_by_func (action, G_CALLBACK (sync_text), proxy);
}



static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
  if (check_widget (proxy))
    {
      GtkToolItem *item = GTK_TOOL_ITEM (proxy);
      GtkEntry *entry = GTK_ENTRY (g_object_get_data (G_OBJECT (item), "entry"));

      sync_label (action, NULL, proxy);
      g_signal_connect_object (action, "notify::label",
			       G_CALLBACK (sync_label), proxy, 0);
      sync_text (action, NULL, proxy);
      g_signal_connect_object (action, "notify::text",
			       G_CALLBACK (sync_text), proxy, 0);
      sync_editable (action, NULL, proxy);
      g_signal_connect_object (action, "notify::editable",
			       G_CALLBACK (sync_editable), proxy, 0);

      g_signal_connect_object (entry, "activate",
			       G_CALLBACK (gtk_action_activate), action,
			       G_CONNECT_SWAPPED);
      g_signal_connect_object (entry, "changed",
			       G_CALLBACK (changed_cb), action, 0);
    }

  GTK_ACTION_CLASS (parent_class)->connect_proxy (action, proxy);
}

static void
disconnect_proxy (GtkAction *action, GtkWidget *proxy)
{
  GTK_ACTION_CLASS (parent_class)->disconnect_proxy (action, proxy);

  if (check_widget (proxy))
    {
      GtkToolItem *item = GTK_TOOL_ITEM (proxy);
      GtkEntry *entry = GTK_ENTRY (g_object_get_data (G_OBJECT (item), "entry"));

      g_signal_handlers_disconnect_matched (action, G_SIGNAL_MATCH_DATA,
					    0, 0, NULL, NULL, proxy);
      g_signal_handlers_disconnect_matched (entry, G_SIGNAL_MATCH_DATA,
					    0, 0, NULL, NULL, action);
    }
}

static void
gtk_entry_action_set_property (GObject *object,
				   guint prop_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
  GtkEntryAction *action = GTK_ENTRY_ACTION (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      gtk_entry_action_set_text (action, g_value_get_string (value));
      break;
    case PROP_EDITABLE:
      action->priv->editable = g_value_get_boolean (value);
      break;
    }
}

static void
gtk_entry_action_get_property (GObject *object,
				   guint prop_id,
				   GValue *value,
				   GParamSpec *pspec)
{
  GtkEntryAction *action = GTK_ENTRY_ACTION (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, gtk_entry_action_get_text (action));
      break;
    case PROP_EDITABLE:
      g_value_set_boolean (value, action->priv->editable);
      break;
    }
}

static void
gtk_entry_action_finalize (GObject *object)
{
  GtkEntryAction *action = GTK_ENTRY_ACTION (object);
  GtkEntryActionPrivate *priv = action->priv;

  g_free (priv->text);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_entry_action_class_init (GtkEntryActionClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkActionClass *action_class = GTK_ACTION_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  object_class->finalize = gtk_entry_action_finalize;
  object_class->get_property = gtk_entry_action_get_property;
  object_class->set_property = gtk_entry_action_set_property;

  action_class->create_tool_item = gtk_entry_action_create_tool_item;
  action_class->connect_proxy = connect_proxy;
  action_class->disconnect_proxy = disconnect_proxy;

  g_object_class_install_property (object_class,
				   PROP_TEXT,
				   g_param_spec_string ("text",
							"Text",
							"The text",
							"",
							G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_EDITABLE,
				   g_param_spec_boolean ("editable",
							 "Editable",
							 "Editable",
							 TRUE,
							 G_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkEntryActionPrivate));
}

static void
gtk_entry_action_init (GtkEntryAction *action)
{
  action->priv = GTK_ENTRY_ACTION_GET_PRIVATE (action);

  action->priv->text = g_strdup ("");
  action->priv->editable = TRUE;
}

GtkAction *
gtk_entry_action_new (const gchar *name,
		      const gchar *label,
		      const gchar *tooltip,
		      const gchar *stock_id)
{
  GtkAction *action;

  action = g_object_new (GTK_TYPE_ENTRY_ACTION,
			 "name", name,
			 "label", label,
			 "tooltip", tooltip,
			 "stock_id", stock_id,
			 NULL);

  return action;
}

const char *
gtk_entry_action_get_text (GtkEntryAction *action)
{
  g_return_val_if_fail (GTK_IS_ENTRY_ACTION (action), "");

  return action->priv->text;
}

void
gtk_entry_action_set_text (GtkEntryAction *action,
			   const char *text)
{
  char *old_text;
  g_return_if_fail (GTK_IS_ENTRY_ACTION (action));

  old_text = action->priv->text;
  action->priv->text = g_strdup (text);
  g_free (old_text);
  g_object_notify (G_OBJECT (action), "text");
}

gboolean
gtk_entry_action_has_focus (GtkEntryAction *action)
{
  g_return_val_if_fail (GTK_IS_ENTRY_ACTION (action), FALSE);

  return (gtk_widget_has_focus (action->priv->entry_widget));
}
