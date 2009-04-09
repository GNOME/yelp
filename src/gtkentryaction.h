/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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

#ifndef GTK_ENTRY_ACTION_H
#define GTK_ENTRY_ACTION_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_ENTRY_ACTION            (gtk_entry_action_get_type ())
#define GTK_ENTRY_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ENTRY_ACTION, GtkEntryAction))
#define GTK_ENTRY_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_ENTRY_ACTION, GtkEntryActionClass))
#define GTK_IS_ENTRY_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ENTRY_ACTION))
#define GTK_IS_ENTRY_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), GTK_TYPE_ENTRY_ACTION))
#define GTK_ENTRY_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_ENTRY_ACTION, GtkEntryActionClass))

typedef struct _GtkEntryAction		GtkEntryAction;
typedef struct _GtkEntryActionPrivate	GtkEntryActionPrivate;
typedef struct _GtkEntryActionClass		GtkEntryActionClass;

struct _GtkEntryAction
{
	GtkAction parent;

	/*< private >*/
	GtkEntryActionPrivate *priv;
};

struct _GtkEntryActionClass
{
	GtkActionClass parent_class;
};

GType       gtk_entry_action_get_type  (void);

GtkAction  *gtk_entry_action_new       (const gchar    *name,
					const gchar    *label,
					const gchar    *tooltip,
					const gchar    *stock_id);

const char *gtk_entry_action_get_text  (GtkEntryAction *action);

void        gtk_entry_action_set_text  (GtkEntryAction *action,
					const char     *text);

gboolean    gtk_entry_action_has_focus (GtkEntryAction *action);
G_END_DECLS

#endif
