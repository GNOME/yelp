/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 CodeFactory AB
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

#include "yelp-history.h"

static void    help_history_init              (HelpHistory      *history);
static void    help_history_class_init        (HelpHistoryClass *klass);
static void    help_history_finalize          (GObject          *object);
static void    help_history_free_history_list (GList            *history_list);

static void    help_history_maybe_emit        (HelpHistory      *history);
					
enum { 
	FORWARD_EXISTS_CHANGED,
	BACK_EXISTS_CHANGED, 
	LAST_SIGNAL 
}; 

static gint signals[LAST_SIGNAL] = { 0 };

struct _HelpHistoryPriv {
	GList    *help_history_list;
	GList    *current;

	gboolean  last_emit_forward;
	gboolean  last_emit_back;
};

GType
help_history_get_type (void)
{
        static GType history_type = 0;

        if (!history_type) {
                static const GTypeInfo history_info = {
                        sizeof (HelpHistoryClass),
                        NULL,
                        NULL,
                        (GClassInitFunc) help_history_class_init,
                        NULL,
                        NULL,
                        sizeof (HelpHistory),
                        0,
                        (GInstanceInitFunc) help_history_init,
                };
                
                history_type = g_type_register_static (G_TYPE_OBJECT,
                                                      "HelpHistory", 
                                                      &history_info, 0);
        }
        
        return history_type;
}

static void
help_history_init (HelpHistory *history)
{
	HelpHistoryPriv *priv;

	priv = g_new0 (HelpHistoryPriv, 1);
        
	priv->help_history_list      = NULL;
	priv->current           = NULL;
	priv->last_emit_forward = FALSE;
	priv->last_emit_back    = FALSE;
	history->priv           = priv;
}

static void
help_history_class_init (HelpHistoryClass *klass)
{
        GObjectClass *object_class;
        
        object_class = (GObjectClass *) klass;

        object_class->finalize = help_history_finalize;

	signals[FORWARD_EXISTS_CHANGED] =
                g_signal_new ("forward_exists_changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (HelpHistoryClass, 
                                               forward_exists_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__BOOLEAN,
                              G_TYPE_NONE,
                              1, G_TYPE_BOOLEAN);
        
	signals[BACK_EXISTS_CHANGED] =
		g_signal_new ("back_exists_changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (HelpHistoryClass,
                                               back_exists_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__BOOLEAN,
                              G_TYPE_NONE,
                              1, G_TYPE_BOOLEAN);
}

static void
help_history_finalize (GObject *object)
{
	HelpHistory     *history;
	HelpHistoryPriv *priv;
	GList           *node;
        
	g_return_if_fail (object != NULL);
	g_return_if_fail (HELP_IS_HISTORY (object));
        
	history = HELP_HISTORY (object);
	priv    = history->priv;
        
	for (node = priv->help_history_list; node; node = node->next) {
		g_free (node->data);
	}

	g_list_free (priv->help_history_list);

	g_free (priv);

	history->priv = NULL;
}

static void
help_history_free_history_list (GList *help_history_list)
{
	GList *node;
        
	for (node = help_history_list; node; node = node->next) {
		g_free (node->data);
	}

	g_list_free (help_history_list);
}

static void
help_history_maybe_emit (HelpHistory *history)
{
	HelpHistoryPriv *priv;
		
	g_return_if_fail (history != NULL);
	g_return_if_fail (HELP_IS_HISTORY (history));
	
	priv = history->priv;
	
	if (priv->last_emit_forward != help_history_exist_forward (history)) {
		priv->last_emit_forward = help_history_exist_forward (history);
		
		g_signal_emit (history,
                               signals[FORWARD_EXISTS_CHANGED],
                               priv->last_emit_forward);
	}

	if (priv->last_emit_back != help_history_exist_back (history)) {
		priv->last_emit_back = help_history_exist_back (history);
		
		g_signal_emit (history,
                               signals[BACK_EXISTS_CHANGED],
                               priv->last_emit_back);
	}
}

void
help_history_goto (HelpHistory *history, const gchar *str)
{
	HelpHistoryPriv *priv;
	GList           *forward_list;
	
	g_return_if_fail (history != NULL);
	g_return_if_fail (HELP_IS_HISTORY (history));

	priv = history->priv;
	
	if (help_history_exist_forward (history)) {
		forward_list = priv->current->next;
		priv->current->next = NULL;
			
		help_history_free_history_list (forward_list);
	}

 	priv->help_history_list = g_list_append (priv->help_history_list, 
					    g_strdup (str));
	
	priv->current      = g_list_last (priv->help_history_list);
	
	help_history_maybe_emit (history);
}

gchar *
help_history_go_forward (HelpHistory *history)
{
	HelpHistoryPriv *priv;
        
	g_return_val_if_fail (history != NULL, NULL);
	g_return_val_if_fail (HELP_IS_HISTORY (history), NULL);

	priv = history->priv;
        
	if (priv->current->next) {
		priv->current = priv->current->next;

		help_history_maybe_emit (history);
		
		return g_strdup ((gchar *)priv->current->data);
	}

	return NULL;
}

gchar *
help_history_go_back (HelpHistory *history)
{
	HelpHistoryPriv *priv;
	
	g_return_val_if_fail (history != NULL, NULL);
	g_return_val_if_fail (HELP_IS_HISTORY (history), NULL);

	priv = history->priv;
        
	if (priv->current->prev) {
		priv->current = priv->current->prev;

		help_history_maybe_emit (history);

		return g_strdup ((gchar *) priv->current->data);
	}
        
	return NULL;
}

gchar *
help_history_get_current (HelpHistory *history)
{
	HelpHistoryPriv *priv;
	
	g_return_val_if_fail (history != NULL, NULL);
	g_return_val_if_fail (HELP_IS_HISTORY (history), NULL);

	priv = history->priv;
	
	if (!priv->current) {
		return NULL;
	}

	return g_strdup ((gchar *) priv->current->data);
}

gboolean
help_history_exist_forward (HelpHistory *history)
{
	HelpHistoryPriv *priv;
        
	g_return_val_if_fail (history != NULL, FALSE);
	g_return_val_if_fail (HELP_IS_HISTORY (history), FALSE);
        
	priv = history->priv;
        
	if (!priv->current) {
		return FALSE;
	}

	if (priv->current->next) {
		return TRUE;
	}

	return FALSE;
}

gboolean
help_history_exist_back (HelpHistory *history)
{
	HelpHistoryPriv *priv;
        
	g_return_val_if_fail (history != NULL, FALSE);
	g_return_val_if_fail (HELP_IS_HISTORY (history), FALSE);

	priv = history->priv;
        
	if (!priv->current) {
		return FALSE;
	}
        
	if (priv->current->prev) {
		return TRUE;
	}
        
	return FALSE;
}

HelpHistory *
help_history_new ()
{
	return g_object_new (HELP_TYPE_HISTORY, NULL);
}

