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
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Shaun McCance  <shaunm@gnome.org>
 */

#ifndef __YELP_APPLICATION_H__
#define __YELP_APPLICATION_H__

#include <unique/unique.h>

#define YELP_TYPE_APPLICATION            (yelp_application_get_type ())
#define YELP_APPLICATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), YELP_TYPE_APPLICATION, YelpApplication))
#define YELP_APPLICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), YELP_TYPE_APPLICATION, YelpApplicationClass))
#define YELP_IS_APPLICATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), YELP_TYPE_APPLICATION))
#define YELP_IS_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), YELP_TYPE_APPLICATION))

typedef struct _YelpApplication       YelpApplication;
typedef struct _YelpApplicationClass  YelpApplicationClass;

struct _YelpApplication
{
    GObject       parent;
};

struct _YelpApplicationClass
{
    GObjectClass  parent_class;
};

GType             yelp_application_get_type    (void);
YelpApplication*  yelp_application_new         (void);
gint              yelp_application_run         (YelpApplication  *app,
                                                GOptionContext   *context,
                                                gint              argc,
                                                gchar           **argv);
gboolean          yelp_application_load_uri    (YelpApplication  *app,
                                                const gchar      *uri,
                                                guint             timestamp,
                                                GError          **error);

#endif /* __YELP_APPLICATION_H__ */
