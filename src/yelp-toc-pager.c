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

#include <string.h>
#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>

#include "yelp-toc-pager.h"

#define d(x)

typedef struct _OMF OMF;

struct _YelpTocPagerPriv {
    GSList           *omf_pending;
    GSList           *omf_list;

    GHashTable       *seriesid_hash;
    GHashTable       *category_hash;

    xmlParserCtxtPtr  parser;
};

struct _OMF {
    xmlChar   *title;
    xmlChar   *description;

    xmlChar   *language;
    gint     lang_priority;

    xmlChar   *omf_file;
    xmlChar   *xml_file;
};

static void          toc_pager_class_init   (YelpTocPagerClass *klass);
static void          toc_pager_init         (YelpTocPager      *pager);
static void          toc_pager_dispose      (GObject           *gobject);

gboolean             toc_pager_process      (YelpPager         *pager);
void                 toc_pager_cancel       (YelpPager         *pager);
gchar *              toc_pager_resolve_uri  (YelpPager         *pager,
					     YelpURI           *uri);
const GtkTreeModel * toc_pager_get_sections (YelpPager         *pager);

static void          toc_read_omf_dir       (YelpTocPager      *pager,
					     const gchar       *dirstr);
static gboolean      toc_process_pending    (YelpPager         *pager);

static void          omf_free               (OMF               *omf);

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

    priv->parser = xmlNewParserCtxt ();

    priv->seriesid_hash = g_hash_table_new (g_str_hash, g_str_equal);
    priv->category_hash = g_hash_table_new (g_str_hash, g_str_equal);
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
    toc_read_omf_dir (YELP_TOC_PAGER (pager), DATADIR"/omf");

    gtk_idle_add ((GtkFunction) toc_process_pending,
		  pager);
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

static void
toc_read_omf_dir (YelpTocPager *pager, const gchar *dirstr)
{
    GnomeVFSResult           result;
    GnomeVFSDirectoryHandle *dir;
    GnomeVFSFileInfo        *file_info;

    YelpTocPagerPriv *priv = pager->priv;

    result = gnome_vfs_directory_open (&dir, dirstr,
				       GNOME_VFS_FILE_INFO_DEFAULT);
    if (result != GNOME_VFS_OK)
	return;

    file_info = gnome_vfs_file_info_new ();

    while (gnome_vfs_directory_read_next (dir, file_info) == GNOME_VFS_OK) {
	switch (file_info->type) {
	case GNOME_VFS_FILE_TYPE_DIRECTORY:
	    if (strcmp (file_info->name, ".") && strcmp (file_info->name, "..")) {
		gchar *newdir = g_strconcat (dirstr, "/", file_info->name, NULL);
		toc_read_omf_dir (pager, newdir);
		g_free (newdir);
	    }
	    break;
	case GNOME_VFS_FILE_TYPE_REGULAR:
	case GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK:
	    priv->omf_pending =
		g_slist_prepend (priv->omf_pending,
				 g_strconcat (dirstr, "/", file_info->name, NULL));
	    break;
	default:
	    break;
	}
    }

    gnome_vfs_file_info_unref (file_info);
    gnome_vfs_directory_close (dir);

    while (gtk_events_pending ())
	gtk_main_iteration ();

}

static gboolean
toc_process_pending (YelpPager *pager)
{
    GSList    *first;
    gchar     *file;
    xmlDocPtr  omf_doc;
    xmlNodePtr omf_cur;
    gint       lang_priority;
    OMF       *omf = NULL;

    const GList      *langs = gnome_i18n_get_language_list ("LC_MESSAGES");
    YelpTocPagerPriv *priv  = YELP_TOC_PAGER (pager)->priv;

    first = priv->omf_pending;
    priv->omf_pending = g_slist_remove_link (priv->omf_pending, first);

    file = (gchar *) first->data;

    omf_doc = xmlCtxtReadFile (priv->parser,
			       (const char *) file,
			       NULL, 0);
    if (!omf_doc)
	goto done;

    omf_cur = xmlDocGetRootElement (omf_doc);
    if (!omf_cur)
	goto done;

    for (omf_cur = omf_cur->children; omf_cur; omf_cur = omf_cur->next) {
	if (omf_cur->type == XML_ELEMENT_NODE &&
	    !xmlStrcmp (omf_cur->name, (xmlChar *) "resource"))
	    break;
    }
    if (!omf_cur)
	goto done;

    omf = g_new0 (OMF, 1);
    omf->omf_file = (xmlChar *) g_strdup (file);

    for (omf_cur = omf_cur->children; omf_cur; omf_cur = omf_cur->next) {
	if (omf_cur->type != XML_ELEMENT_NODE)
	    continue;

	if (!xmlStrcmp (omf_cur->name, (xmlChar *) "title")) {
	    if (omf->title)
		xmlFree (omf->title);
	    omf->title = xmlNodeGetContent (omf_cur);
	    continue;
	}
	if (!xmlStrcmp (omf_cur->name, (xmlChar *) "description")) {
	    if (omf->description)
		xmlFree (omf->description);
	    omf->description = xmlNodeGetContent (omf_cur);
	    continue;
	}
	if (!xmlStrcmp (omf_cur->name, (xmlChar *) "language")) {
	    if (omf->language)
		xmlFree (omf->language);
	    omf->language = xmlGetProp (omf_cur, (const xmlChar *) "code");

	    lang_priority = 0;

	    for (; langs != NULL; langs = langs->next) {
		gchar *lang = langs->data;
		lang_priority++;

		if (lang == NULL || strchr (lang, '.') != NULL)
		    continue;

		if (!xmlStrcmp ((xmlChar *) lang, omf->language)) {
		    omf->lang_priority = lang_priority;
		    break;
		}
	    }

	    if (!omf->lang_priority) {
		// Not a language we want to display
		omf_free (omf);
		goto done;
	    }
	}
	if (!xmlStrcmp (omf_cur->name, (xmlChar *) "identifier")) {
	    if (omf->xml_file)
		xmlFree (omf->xml_file);
	    omf->xml_file = xmlGetProp (omf_cur, (const xmlChar *) "url");
	}
    }

 done:
    xmlFreeDoc (omf_doc);
    g_free (file);
    g_slist_free_1 (first);

    if (priv->omf_pending)
	return TRUE;
    else
	return FALSE;
}

static void
omf_free (OMF *omf)
{
    if (omf) {
	xmlFree (omf->title);
	xmlFree (omf->description);
	xmlFree (omf->language);
	xmlFree (omf->omf_file);
	xmlFree (omf->xml_file);
    }
    g_free (omf);
}
