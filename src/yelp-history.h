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

#ifndef __YELP_HISTORY_H__
#define __YELP_HISTORY_H__

#include <glib-object.h>
#include "yelp-section.h"

#define YELP_TYPE_HISTORY         (yelp_history_get_type ())
#define YELP_HISTORY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_HISTORY, YelpHistory))
#define YELP_HISTORY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_HISTORY, YelpHistoryClass))
#define YELP_IS_HISTORY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_HISTORY))
#define YELP_IS_HISTORY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_HISTORY))
#define YELP_HISTORY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_HISTORY, YelpHistoryClass))

typedef struct _YelpHistory      YelpHistory;
typedef struct _YelpHistoryClass YelpHistoryClass;
typedef struct _YelpHistoryPriv  YelpHistoryPriv;

struct _YelpHistory {
	GObject          parent;
	
	YelpHistoryPriv *priv;
};

struct _YelpHistoryClass {
	GObjectClass     parent_class;

	/* Signals */
	void   (*forward_exists_changed)       (YelpHistory    *history,
						gboolean        exists);
	void   (*back_exists_changed)          (YelpHistory    *history,
						gboolean        exists);
};

GType               yelp_history_get_type      (void);
YelpHistory *       yelp_history_new           (void);

void                yelp_history_goto          (YelpHistory         *history,
						YelpURI             *uri);

YelpURI *           yelp_history_go_forward    (YelpHistory         *history);

YelpURI *           yelp_history_go_back       (YelpHistory         *history);

YelpURI *           yelp_history_get_current   (YelpHistory         *history);

gboolean            yelp_history_exist_forward (YelpHistory         *history);
gboolean            yelp_history_exist_back    (YelpHistory         *history);

#endif /* __YELP_HISTORY_H__ */
