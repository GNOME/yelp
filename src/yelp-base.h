/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#ifndef __YELP_BASE_H__
#define __YELP_BASE_H__

#include <bonobo/bonobo-object.h>
#include "GNOME_Yelp.h"

typedef struct _YelpBase      YelpBase;
typedef struct _YelpBaseClass YelpBaseClass;
typedef struct _YelpBasePriv  YelpBasePriv;

#define YELP_TYPE_BASE         (yelp_base_get_type ())
#define YELP_BASE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), YELP_TYPE_BASE, YelpBase))
#define YELP_BASE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), YELP_TYPE_BASE, YelpBaseClass))
#define YELP_IS_BASE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), YELP_TYPE_BASE))
#define YELP_IS_BASE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), YELP_TYPE_BASE))
#define YELP_BASE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), YELP_TYPE_BASE, YelpBaseClass))


struct _YelpBase {
        BonoboObject  parent;
        
        YelpBasePriv  *priv;
};

struct _YelpBaseClass {
        BonoboObjectClass       parent_class;

	POA_GNOME_Yelp__epv epv;
};

GType            yelp_base_get_type       (void);
YelpBase *       yelp_base_new            (void);

GtkWidget *      yelp_base_new_window     (YelpBase *base);

#endif /* __YELP_BASE_H__ */
