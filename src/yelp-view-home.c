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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnomevfs/gnome-vfs.h>
#include <libgnome/gnome-i18n.h>
#include <stdio.h>
#include "yelp-view-home.h"

#define d(x) x

static void yvh_init          (YelpViewHome              *html);
static void yvh_class_init    (YelpViewHomeClass         *klass);

struct _YelpViewHomePriv {
	HtmlDocument *doc;
	
};

GType
yelp_view_home_get_type (void)
{
        static GType view_type = 0;

        if (!view_type)
        {
                static const GTypeInfo view_info =
                        {
                                sizeof (YelpViewHomeClass),
                                NULL,
                                NULL,
                                (GClassInitFunc) yvh_class_init,
                                NULL,
                                NULL,
                                sizeof (YelpViewHome),
                                0,
                                (GInstanceInitFunc) yvh_init,
                        };
                
                view_type = g_type_register_static (HTML_TYPE_VIEW,
                                                    "YelpViewHome", 
                                                    &view_info, 0);
        }
        
        return view_type;
}

static void
yvh_init (YelpViewHome *view)
{
}

static void
yvh_class_init (YelpViewHomeClass *klass)
{
}

