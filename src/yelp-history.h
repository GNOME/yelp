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

#ifndef __HELP_HISTORY_H__
#define __HELP_HISTORY_H__

#include <gobject/gobject.h>

#define HELP_TYPE_HISTORY         (help_history_get_type ())
#define HELP_HISTORY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), HELP_TYPE_HISTORY, HelpHistory))
#define HELP_HISTORY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), HELP_TYPE_HISTORY, HelpHistoryClass))
#define HELP_IS_HISTORY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), HELP_TYPE_HISTORY))
#define HELP_IS_HISTORY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), HELP_TYPE_HISTORY))
#define HELP_HISTORY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), HELP_TYPE_HISTORY, HelpHistoryClass))

typedef struct _HelpHistory      HelpHistory;
typedef struct _HelpHistoryClass HelpHistoryClass;
typedef struct _HelpHistoryPriv  HelpHistoryPriv;

struct _HelpHistory {
	GObject          parent;
	
	HelpHistoryPriv *priv;
};

struct _HelpHistoryClass {
	GObjectClass     parent_class;

	/* Signals */
	void   (*forward_exists_changed)     (HelpHistory    *history,
					      gboolean        exists);
	void   (*back_exists_changed)        (HelpHistory    *history,
					      gboolean        exists);
};

GType             help_history_get_type      (void);
HelpHistory *     help_history_new           (void);

void              help_history_goto          (HelpHistory         *history,
                                              const gchar         *str);

gchar *           help_history_go_forward    (HelpHistory         *history);

gchar *           help_history_go_back       (HelpHistory         *history);

gchar *           help_history_get_current   (HelpHistory         *history);

gboolean          help_history_exist_forward (HelpHistory         *history);
gboolean          help_history_exist_back    (HelpHistory         *history);

#endif /* __HELP_HISTORY_H__ */

