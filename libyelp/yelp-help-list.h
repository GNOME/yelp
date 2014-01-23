/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2010 Shaun McCance  <shaunm@gnome.org>
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#ifndef __YELP_HELP_LIST_H__
#define __YELP_HELP_LIST_H__

#include <glib-object.h>

#include "yelp-document.h"

#define YELP_TYPE_HELP_LIST         (yelp_help_list_get_type ())
#define YELP_HELP_LIST(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_HELP_LIST, YelpHelpList))
#define YELP_HELP_LIST_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_HELP_LIST, YelpHelpListClass))
#define YELP_IS_HELP_LIST(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_HELP_LIST))
#define YELP_IS_HELP_LIST_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_HELP_LIST))
#define YELP_HELP_LIST_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_HELP_LIST, YelpHelpListClass))

typedef struct _YelpHelpList      YelpHelpList;
typedef struct _YelpHelpListClass YelpHelpListClass;

struct _YelpHelpList {
    YelpDocument      parent;
};

struct _YelpHelpListClass {
    YelpDocumentClass parent_class;
};

GType           yelp_help_list_get_type     (void);
YelpDocument *  yelp_help_list_new          (YelpUri *uri);

#endif /* __YELP_HELP_LIST_H__ */
