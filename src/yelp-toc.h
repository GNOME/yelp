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

#ifndef __HELP_TOC_H__
#define __HELP_TOC_H__

#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include "yelp-section.h"

#define YELP_TYPE_TOC		 (yelp_toc_get_type ())
#define YELP_TOC(obj)		 (GTK_CHECK_CAST ((obj), YELP_TYPE_TOC, YelpToc))
#define YELP_TOC_CLASS(klass)	 (GTK_CHECK_CLASS_CAST ((klass), YELP_TYPE_TOC, YelpTocClass))
#define YELP_IS_TOC(obj)	 (GTK_CHECK_TYPE ((obj), YELP_TYPE_TOC))
#define YELP_IS_TOC_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), YELP_TYPE_TOC))

typedef struct _YelpToc       YelpToc;
typedef struct _YelpTocClass  YelpTocClass;
typedef struct _YelpTocPriv   YelpTocPriv;

struct _YelpToc
{
        GtkTreeView    parent;
        
        YelpTocPriv   *priv;
};

struct _YelpTocClass
{
        GtkTreeViewClass   parent_class;

        /* Signals */
	void   (*section_selected)     (YelpToc             *toc,
					YelpSection         *section);
};

GType          yelp_toc_get_type        (void);
GtkWidget *    yelp_toc_new             (GtkTreeStore    *model);
gboolean       yelp_toc_open            (YelpToc         *toc,
					 GnomeVFSURI     *uri);
void           yelp_toc_add_book        (YelpToc         *toc,
					 GNode           *root);

#endif /* __YELP_TOC_H__ */
