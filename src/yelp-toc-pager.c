/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003 Shaun McCance  <shaunm@gnome.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>

#include "yelp-toc-pager.h"

#define d(x)

struct _YelpTocPagerPriv {
};

static void          toc_pager_class_init   (YelpTocPagerClass *klass);
static void          toc_pager_init         (YelpTocPager      *pager);
static void          toc_pager_dispose      (GObject           *gobject);

gboolean             toc_pager_process      (YelpPager   *pager);
void                 toc_pager_cancel       (YelpPager   *pager);
gchar *              toc_pager_resolve_uri  (YelpPager   *pager,
					     YelpURI     *uri);
const GtkTreeModel * toc_pager_get_sections (YelpPager   *pager);

static YelpPagerClass *parent_class;

static YelpTocPager   *toc_pager;

GType
yelp_toc_pager_get_type (void)
{
    static GType type = 0;

    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpTocPagerClass),
	    NULL,
	    NULL,
	    (GClassInitFunc) toc_pager_class_init,
	    NULL,
	    NULL,
	    sizeof (YelpTocPager),
	    0,
	    (GInstanceInitFunc) toc_pager_init,
	};
	type = g_type_register_static (YELP_TYPE_PAGER,
				       "YelpTocPager", 
				       &info, 0);
    }
    return type;
}

static void
toc_pager_class_init (YelpTocPagerClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);
    YelpPagerClass *pager_class  = YELP_PAGER_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = toc_pager_dispose;

    pager_class->process      = toc_pager_process;
    pager_class->cancel       = toc_pager_cancel;
    pager_class->resolve_uri  = toc_pager_resolve_uri;
    pager_class->get_sections = toc_pager_get_sections;
}

static void
toc_pager_init (YelpTocPager *pager)
{
    YelpTocPagerPriv *priv;

    priv = g_new0 (YelpTocPagerPriv, 1);
    pager->priv = priv;
}

static void
toc_pager_dispose (GObject *object)
{
    YelpTocPager *pager = YELP_TOC_PAGER (object);

    g_free (pager->priv);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

void
yelp_toc_pager_init (void)
{
    toc_pager = (YelpTocPager *) g_object_new (YELP_TYPE_TOC_PAGER, NULL);

    yelp_pager_start (YELP_PAGER (toc_pager));
}

YelpPager *
yelp_toc_pager_get (void)
{
    return (YelpPager *) toc_pager;
}

/******************************************************************************/

gboolean
toc_pager_process (YelpPager *pager)
{
    // FIXME below

    while (gtk_events_pending ())
	gtk_main_iteration ();

    return FALSE;
}

void
toc_pager_cancel (YelpPager *pager)
{
    // FIXME
}

gchar *
toc_pager_resolve_uri (YelpPager *pager, YelpURI *uri)
{
    // FIXME
    return NULL;
}

const GtkTreeModel *
toc_pager_get_sections (YelpPager *pager)
{
    return NULL;
}

/******************************************************************************/
