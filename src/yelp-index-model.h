/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002 Richard Hult <rhult@imendio.com>
 * Copyright (C) 2002 Mikael Hallendal <micke@imendio.com>
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
 */

#ifndef __YELP_INDEX_MODEL_H__
#define __YELP_INDEX_MODEL_H__

#include <glib-object.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreesortable.h>

#define YELP_TYPE_INDEX_MODEL	          (yelp_index_model_get_type ())
#define YELP_INDEX_MODEL(obj)	          (G_TYPE_CHECK_INSTANCE_CAST ((obj), YELP_TYPE_INDEX_MODEL, YelpIndexModel))
#define YELP_INDEX_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), YELP_TYPE_INDEX_MODEL, YelpIndexModelClass))
#define YELP_IS_INDEX_MODEL(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), YELP_TYPE_INDEX_MODEL))
#define YELP_IS_INDEX_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), YELP_TYPE_INDEX_MODEL))
#define YELP_INDEX_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), YELP_TYPE_INDEX_MODEL, YelpIndexModelClass))

typedef struct _YelpIndexModel       YelpIndexModel;
typedef struct _YelpIndexModelClass  YelpIndexModelClass;
typedef struct _YelpIndexModelPriv   YelpIndexModelPriv;

struct _YelpIndexModel
{
        GObject              parent;

        YelpIndexModelPriv *priv;

};

struct _YelpIndexModelClass
{
  GObjectClass parent_class;
};

enum {
	YELP_INDEX_MODEL_COL_NAME,
	YELP_INDEX_MODEL_COL_SECTION,
	YELP_INDEX_MODEL_NR_OF_COLS
};


GtkType          yelp_index_model_get_type     (void);

YelpIndexModel * yelp_index_model_new          (void);
void             yelp_index_model_set_words    (YelpIndexModel   *model,
						GList            *index_words);

void             yelp_index_model_filter       (YelpIndexModel   *model,
						const gchar      *string);

#endif /* __YELP_INDEX_MODEL_H__ */
