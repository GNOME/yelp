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

#include "yelp-uri.h"
#include "yelp-error.h"
#include "yelp-theme.h"
#include "yelp-toc-pager.h"

#define d(x)

typedef gboolean      (*ProcessFunction)        (YelpTocPager      *pager);

typedef enum   _YelpSource   YelpSource;
typedef enum   _YelpFormat   YelpFormat;
typedef struct _YelpMetafile YelpMetafile;
typedef struct _YelpMenu     YelpMenu;

struct _YelpTocPagerPriv {
    GSList       *omf_dir_pending;
    GSList       *omf_pending;

    GSList       *man_dir_pending;
    GSList       *man_pending;

    GSList       *info_dir_pending;
    GSList       *info_pending;

    GSList       *node_pending;
    GSList       *menu_pending;

    GSList       *idx_pending;

    xmlDocPtr     toc_doc;

    GHashTable   *unique_hash;
    GHashTable   *category_hash;

    xmlParserCtxtPtr  parser;

    ProcessFunction   pending_func;

    gboolean      cancel;
    gint          pause_count;
};

enum _YelpSource {
    YELP_SOURCE_XDG,
    YELP_SOURCE_OMF,
    YELP_SOURCE_KDE,
    YELP_SOURCE_MAN,
    YELP_SOURCE_INFO
};

enum _YelpFormat {
    YELP_FORMAT_XML_DOCBOOK
};

struct _YelpMetafile {
    gchar   *id;

    gchar   *title;
    gchar   *description;

    gchar   *seriesid;

    gchar   *language;
    gint     lang_priority;

    GSList  *categories;

    YelpSource  source;
    YelpFormat  format;

    gchar   *metafile;
    gchar   *file;
};

struct _YelpMenu {
    gchar       *id;
    gchar       *title;

    xmlNodePtr   node;

    GSList      *submenus;
    GSList      *metafiles;

    gboolean     has_submenus;
};

static void          toc_pager_class_init      (YelpTocPagerClass *klass);
static void          toc_pager_init            (YelpTocPager      *pager);
static void          toc_pager_dispose         (GObject           *gobject);

gboolean             toc_pager_process         (YelpPager         *pager);
void                 toc_pager_cancel          (YelpPager         *pager);
gchar *              toc_pager_resolve_uri     (YelpPager         *pager,
						YelpURI           *uri);
const GtkTreeModel * toc_pager_get_sections    (YelpPager         *pager);

static gboolean      toc_process_pending       (YelpTocPager      *pager);

static gboolean      process_omf_dir_pending   (YelpTocPager      *pager);
static gboolean      process_omf_pending       (YelpTocPager      *pager);
static gboolean      process_man_dir_pending   (YelpTocPager      *pager);
static gboolean      process_man_pending       (YelpTocPager      *pager);
static gboolean      process_info_dir_pending  (YelpTocPager      *pager);
static gboolean      process_info_pending      (YelpTocPager      *pager);

static gboolean      process_read_menu         (YelpTocPager      *pager);
static gboolean      process_menu_node_pending (YelpTocPager      *pager);
static gboolean      process_menu_pending      (YelpTocPager      *pager);

#if 0
static gboolean      toc_process_idx           (YelpTocPager      *pager);
static gboolean      toc_process_idx_pending   (YelpTocPager      *pager);
#endif

static gchar *       menu_write_page           (YelpMenu          *menu);
static void          menu_write_menu           (YelpMenu          *menu,
						GString           *gstr);

static xmlChar *     node_get_title            (xmlNodePtr         node);
static void          toc_hash_metafile         (YelpTocPager      *pager,
						YelpMetafile      *meta);
static void          toc_unhash_metafile       (YelpTocPager      *pager,
						YelpMetafile      *meta);
static void          metafile_free             (YelpMetafile      *meta);

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

    priv->unique_hash   = g_hash_table_new (g_str_hash, g_str_equal);
    priv->category_hash = g_hash_table_new (g_str_hash, g_str_equal);

    priv->cancel       = 0;
    priv->pause_count  = 0;
    priv->pending_func = NULL;
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
    YelpURI *uri;

    uri = yelp_uri_new ("toc:");
    
    toc_pager = (YelpTocPager *) g_object_new (YELP_TYPE_TOC_PAGER, 
    					       "uri", uri, NULL);

    yelp_pager_start (YELP_PAGER (toc_pager));
}

YelpTocPager *
yelp_toc_pager_get (void)
{
    return toc_pager;
}

void
yelp_toc_pager_pause (YelpTocPager *pager)
{
    g_return_if_fail (pager != NULL);
    pager->priv->pause_count = pager->priv->pause_count + 1;
}

void
yelp_toc_pager_unpause (YelpTocPager *pager)
{
    g_return_if_fail (pager != NULL);
    pager->priv->pause_count = pager->priv->pause_count - 1;

    if (pager->priv->pause_count < 0) {
	g_warning (_("YelpTocPager: Pause count is negative."));
	pager->priv->pause_count = 0;
    }

    if (pager->priv->pause_count < 1) {
	gtk_idle_add_priority (G_PRIORITY_LOW,
			       (GtkFunction) toc_process_pending,
			       pager);
    }
}

/******************************************************************************/

gboolean
toc_pager_process (YelpPager *pager)
{
    gchar  *manpath;
    YelpTocPagerPriv *priv = YELP_TOC_PAGER (pager)->priv;

    /* Set the OMF directories to be read */
    priv->omf_dir_pending = g_slist_prepend (priv->omf_dir_pending,
					     g_strdup (DATADIR "/omf"));

    /* Set the man directories to be read */
    if (!g_spawn_command_line_sync ("manpath", &manpath, NULL, NULL, NULL))
	manpath = g_strdup (g_getenv ("MANPATH"));

    if (manpath) {
	gint    i;
	gchar **paths;

	g_strstrip (manpath);
	paths = g_strsplit (manpath, G_SEARCHPATH_SEPARATOR_S, -1);
                
	for (i = 0; paths[i]; ++i)
	    priv->man_dir_pending = g_slist_prepend (priv->man_dir_pending,
						     g_strdup (paths[i]));

	g_free (paths);
	g_free (manpath);
    }

    /* Set the info directories to be read */

    /* Set it running */
    priv->pending_func = (ProcessFunction) process_omf_dir_pending;

    gtk_idle_add_priority (G_PRIORITY_LOW,
			   (GtkFunction) toc_process_pending,
			   pager);
    return FALSE;
}

void
toc_pager_cancel (YelpPager *pager)
{
    YelpTocPagerPriv *priv = YELP_TOC_PAGER (pager)->priv;

    priv->cancel = TRUE;
}

gchar *
toc_pager_resolve_uri (YelpPager *pager, YelpURI *uri)
{
    const gchar *frag = gnome_vfs_uri_get_fragment_identifier (uri->uri);

    if (!frag)
	return g_strdup ("index");
    else
	return g_strdup (frag);
}

const GtkTreeModel *
toc_pager_get_sections (YelpPager *pager)
{
    return NULL;
}

/******************************************************************************/

static gboolean
toc_process_pending (YelpTocPager *pager)
{
    gboolean readd;
    YelpTocPagerPriv *priv = pager->priv;

    if (priv->cancel || !priv->pending_func) {
	// FIXME: clean stuff up.
	return FALSE;
    }

    readd = priv->pending_func(pager);

    if (!readd) {
	if (priv->pending_func == process_omf_dir_pending)
	    priv->pending_func = process_omf_pending;
	else if (priv->pending_func == process_omf_pending)
	    priv->pending_func = process_man_dir_pending;

	else if (priv->pending_func == process_man_dir_pending)
	    priv->pending_func = process_man_pending;
	else if (priv->pending_func == process_man_pending)
	    priv->pending_func = process_info_dir_pending;

	else if (priv->pending_func == process_info_dir_pending)
	    priv->pending_func = process_info_pending;
	else if (priv->pending_func == process_info_pending)
	    priv->pending_func = process_read_menu;

	else if (priv->pending_func == process_read_menu)
	    priv->pending_func = process_menu_node_pending;
	else if (priv->pending_func == process_menu_node_pending)
	    priv->pending_func = process_menu_pending;
	else if (priv->pending_func == process_menu_pending)
	    priv->pending_func = NULL;
    }

    if (priv->pending_func == NULL)
	g_signal_emit_by_name (pager, "finish");

    if (priv->pending_func && (priv->pause_count < 1))
	return TRUE;
    else
	return FALSE;
}

static gboolean
process_omf_dir_pending (YelpTocPager *pager)
{
    GSList  *first  = NULL;
    gchar   *dirstr = NULL;
    GnomeVFSResult           result;
    GnomeVFSDirectoryHandle *dir       = NULL;
    GnomeVFSFileInfo        *file_info = NULL;

    YelpTocPagerPriv *priv = pager->priv;

    first = priv->omf_dir_pending;
    priv->omf_dir_pending = g_slist_remove_link (priv->omf_dir_pending, first);
    if (!first)
	goto done;

    dirstr = (gchar *) first->data;
    if (!dirstr)
	goto done;

    result = gnome_vfs_directory_open (&dir, dirstr,
				       GNOME_VFS_FILE_INFO_DEFAULT);
    if (result != GNOME_VFS_OK)
	goto done;

    file_info = gnome_vfs_file_info_new ();

    while (gnome_vfs_directory_read_next (dir, file_info) == GNOME_VFS_OK) {
	switch (file_info->type) {
	case GNOME_VFS_FILE_TYPE_DIRECTORY:
	    if (strcmp (file_info->name, ".") && strcmp (file_info->name, "..")) {
		gchar *newdir = g_strconcat (dirstr, "/", file_info->name, NULL);
		priv->omf_dir_pending = g_slist_prepend (priv->omf_dir_pending, newdir);
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

 done:
    if (file_info)
    	gnome_vfs_file_info_unref (file_info);
    if (dir && !result)
	gnome_vfs_directory_close (dir);
    g_slist_free_1 (first);
    g_free (dirstr);

    if (priv->omf_dir_pending)
	return TRUE;
    else
	return FALSE;
}

static gboolean
process_omf_pending (YelpTocPager *pager)
{
    GSList    *first = NULL;
    gchar     *file  = NULL;
    xmlDocPtr  omf_doc = NULL;
    xmlNodePtr omf_cur;
    gint       lang_priority;
    GList     *langs;
    YelpMetafile  *omf     = NULL;
    YelpMetafile  *omf_old = NULL;

    YelpTocPagerPriv *priv  = YELP_TOC_PAGER (pager)->priv;

    first = priv->omf_pending;
    if (!first)
	goto done;

    priv->omf_pending = g_slist_remove_link (priv->omf_pending, first);

    file = (gchar *) first->data;

    if (!file || !g_str_has_suffix (file, ".omf"))
	goto done;

    omf_doc = xmlCtxtReadFile (priv->parser,
			       (const char *) file,
			       NULL,
			       XML_PARSE_NOBLANKS |
			       XML_PARSE_NOCDATA  |
			       XML_PARSE_NOENT    |
			       XML_PARSE_NOERROR  |
			       XML_PARSE_NONET    );
    if (!omf_doc) {
	g_warning (_("Could not load the OMF file '%s'."), file);
	/* FIXME:
	 * There appears to be a bug in libxml2 that prevents files from being
	 * parsed with a parser context after that context has failed on any
	 * single file.  So we'll just free the parser context and make a new
	 * one if there is an error.
	 */
	xmlFreeParserCtxt (priv->parser);
	priv->parser = xmlNewParserCtxt ();
	goto done;
    }

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

    omf = g_new0 (YelpMetafile, 1);
    omf->metafile = g_strdup (file);

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
	if (!xmlStrcmp (omf_cur->name, (xmlChar *) "relation")) {
	    xmlChar *seriesid =
		xmlGetProp (omf_cur, (const xmlChar *) "seriesid");

	    if (seriesid) {
		if (omf->seriesid)
		    xmlFree (omf->seriesid);
		omf->seriesid = seriesid;
	    }
	}
	if (!xmlStrcmp (omf_cur->name, (xmlChar *) "subject")) {
	    xmlChar *category =
		xmlGetProp (omf_cur, (const xmlChar *) "category");

	    if (category)
		omf->categories = g_slist_prepend (omf->categories,
						   category);
	}
	if (!xmlStrcmp (omf_cur->name, (xmlChar *) "language")) {
	    if (omf->language)
		xmlFree (omf->language);
	    omf->language = xmlGetProp (omf_cur, (const xmlChar *) "code");

	    /* The lang_priority of an OMF file is how early it appears in the
	     * list of preferred languages for the user.  Low numbers are best.
	     */
	    lang_priority = 0;
	    langs = (GList *) gnome_i18n_get_language_list ("LC_MESSAGES");
	    for ( ; langs != NULL; langs = langs->next) {
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
		/* If the language didn't match a user-preferred language,
		 * omf->lang_priority doesn't get set, so is 0.
		 */
		metafile_free (omf);
		goto done;
	    }
	}
	if (!xmlStrcmp (omf_cur->name, (xmlChar *) "identifier")) {
	    if (omf->file)
		xmlFree (omf->file);
	    omf->file = xmlGetProp (omf_cur, (const xmlChar *) "url");
	}
    }

    if (!omf->seriesid) {
	metafile_free (omf);
	goto done;
    }

    omf->id = g_strconcat ("scrollkeeper/", (gchar *) omf->seriesid, NULL);

    // If we have one with the same id, use the one with the lowest lang_priority.

    omf_old = g_hash_table_lookup (priv->unique_hash, omf->id);
    if (omf_old) {
	if (omf_old->lang_priority < omf->lang_priority) {
	    metafile_free (omf);
	    goto done;
	} else {
	    toc_unhash_metafile (YELP_TOC_PAGER (pager), omf_old);
	    metafile_free (omf_old);

	    toc_hash_metafile (YELP_TOC_PAGER (pager), omf);
	}
    } else {
	toc_hash_metafile (YELP_TOC_PAGER (pager), omf);
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

static gboolean
process_man_dir_pending (YelpTocPager *pager)
{
    return FALSE;
}

static gboolean
process_man_pending (YelpTocPager *pager)
{
    return FALSE;
}

static gboolean
process_info_dir_pending (YelpTocPager *pager)
{
    return FALSE;
}

static gboolean
process_info_pending (YelpTocPager *pager)
{
    return FALSE;
}

static gboolean
process_read_menu (YelpTocPager *pager)
{
    xmlNodePtr   toc_node;
    GError      *error = NULL;

    YelpTocPagerPriv *priv = pager->priv;

    priv->toc_doc = xmlCtxtReadFile (priv->parser,
				     DATADIR "/yelp/toc.xml",
				     NULL,
				     XML_PARSE_NOBLANKS |
				     XML_PARSE_NOCDATA  |
				     XML_PARSE_NOENT    |
				     XML_PARSE_NOERROR  |
				     XML_PARSE_NONET    );

    if (!priv->toc_doc) {
	yelp_set_error (&error, YELP_ERROR_NO_TOC);
	yelp_pager_error (YELP_PAGER (pager), error);
	priv->cancel = TRUE;
	return FALSE;
    }

    toc_node = xmlDocGetRootElement (priv->toc_doc);

    if (xmlStrcmp (toc_node->name, (const xmlChar *) "toc")) {
	yelp_set_error (&error, YELP_ERROR_NO_TOC);
	yelp_pager_error (YELP_PAGER (pager), error);
	priv->cancel = TRUE;
	return FALSE;
    }

    priv->node_pending = g_slist_append (priv->node_pending, toc_node);

    return FALSE;
}

static gboolean
process_menu_node_pending (YelpTocPager *pager)
{
    GSList      *first;
    YelpMenu    *menu;
    xmlNodePtr   node;
    xmlNodePtr   cur;

    YelpTocPagerPriv *priv = pager->priv;

    first = priv->node_pending;
    priv->node_pending = g_slist_remove_link (priv->node_pending, first);

    node = (xmlNodePtr) first->data;
    if (!node)
	goto done;

    menu = g_new0 (YelpMenu, 1);

    menu->node      = node;
    node->_private  = menu;

    menu->submenus = NULL;

    menu->id    = (gchar *) xmlGetProp (node, (const xmlChar *) "id");
    menu->title = (gchar *) node_get_title (node);

    /* Important:
     * We MUST prepend self to menu_pending and prepend children to
     * node_pending to get a depth-first, post-order traversal, which
     * is needed to prune empty TOC pages.
     */
    priv->menu_pending = g_slist_prepend (priv->menu_pending, menu);

    for (cur = node->children; cur; cur = cur->next) {
	if (cur->type == XML_ELEMENT_NODE) {
	    if (!xmlStrcmp (cur->name, "toc")) {
		priv->node_pending = g_slist_prepend (priv->node_pending, cur);
		menu->submenus = g_slist_append (menu->submenus, cur);
	    }
	}
    }

 done:
    g_slist_free_1 (first);

    if (priv->node_pending)
	return TRUE;
    else
	return FALSE;
}

static gboolean
process_menu_pending (YelpTocPager *pager)
{
    GSList      *first;
    xmlNodePtr   node;
    xmlNodePtr   cur;
    YelpMenu    *cur_menu;
    YelpMenu    *menu;
    GSList      *c;

    YelpTocPagerPriv *priv = pager->priv;

    first = priv->menu_pending;
    priv->menu_pending = g_slist_remove_link (priv->menu_pending, first);

    menu = (YelpMenu *) first->data;
    node = menu->node;

    for (cur = node->children; cur; cur = cur->next) {
	if (cur->type == XML_ELEMENT_NODE)
	    if (!xmlStrcmp (cur->name, "category")) {
		GSList  *meta;
		xmlChar *cat;

		cat = xmlNodeGetContent (cur);
		meta = g_hash_table_lookup (priv->category_hash, cat);

		// FIXME: insert_sorted
		for ( ; meta; meta = meta->next)
		    menu->metafiles = g_slist_prepend (menu->metafiles, meta->data);

		xmlFree (cat);
	    }
    }

    menu->has_submenus = FALSE;
    for (c = menu->submenus; c; c = c->next) {
	cur      = (xmlNodePtr) c->data;
	cur_menu = (YelpMenu *) cur->_private;

	if (cur_menu && (cur_menu->has_submenus || cur_menu->metafiles)) {
	    menu->has_submenus = TRUE;
	    break;
	}
    }

    if (menu->has_submenus || menu->metafiles) {
	YelpPage *page = g_new0 (YelpPage, 1);
	page->chunk    = menu_write_page (menu);
	page->id    = menu->id;
	page->title = menu->title;

	yelp_pager_add_page (YELP_PAGER (pager), page);
	g_signal_emit_by_name (pager, "page", menu->id);
    }

    g_slist_free_1 (first);

    if (priv->menu_pending)
	return TRUE;
    else
	return FALSE;
}

#if 0
static gboolean
toc_process_idx (YelpTocPager *pager)
{
    gtk_idle_add_priority (G_PRIORITY_LOW,
			   (GtkFunction) toc_process_idx_pending,
			   pager);
    return FALSE;
}

static gboolean
toc_process_idx_pending (YelpTocPager *pager)
{
    GSList        *first;
    YelpMetafile  *meta;
    YelpURI       *uri;
    gchar         *path;
    xmlDocPtr      doc;

    YelpTocPagerPriv *priv = pager->priv;

    first = priv->idx_pending;
    priv->idx_pending = g_slist_remove_link (priv->idx_pending, first);

    meta = first->data;

    uri  = yelp_uri_new (omf->file);
    path = gnome_vfs_uri_to_string (uri->uri,
				    GNOME_VFS_URI_HIDE_USER_NAME           |
				    GNOME_VFS_URI_HIDE_PASSWORD            |
				    GNOME_VFS_URI_HIDE_HOST_NAME           |
				    GNOME_VFS_URI_HIDE_HOST_PORT           |
				    GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD     |
				    GNOME_VFS_URI_HIDE_FRAGMENT_IDENTIFIER );

    /*
    doc = xmlCtxtReadFile (priv->parser,
			   path,
			   NULL, 0);

    xmlFreeDoc (doc);
    */

    g_free (path);
    yelp_uri_unref (uri);
    g_slist_free_1 (first);

    if (!priv->idx_pending) {
	return FALSE;
    }
    else if (priv->pause_count > 0) {
	priv->unpause_func = (GtkFunction) toc_process_idx_pending;
	return FALSE;
    }
    else {
	return TRUE;
    }
}
#endif

static gchar *
menu_write_page (YelpMenu  *menu)
{
    GString  *gstr;
    gchar    *page;

    gstr = g_string_new ("<html>");

    g_string_append_printf
	(gstr,
	 "<head><meta http-equiv='Content-Type'"
	 " content='text/html=; charset=utf-8'>"
	 "<link rel='stylesheet' type='text/css' href='%s'></head>\n",
	 yelp_theme_get_css_file ());

    g_string_append_printf (gstr, "<body><h1>%s</h1>\n", menu->title);

    menu_write_menu (menu, gstr);

    g_string_append (gstr, "</body></html>\n");

    page = gstr->str;
    g_string_free (gstr, FALSE);

    return page;
}

static void
menu_write_menu (YelpMenu  *menu,
		 GString   *gstr)
{
    GSList   *c;
    if (menu->has_submenus) {
	g_string_append_printf (gstr, "<h2>%s</h2><ul>\n", _("Categories"));
	for (c = menu->submenus; c; c = c->next) {
	    xmlNodePtr  cur      = (xmlNodePtr) c->data;
	    YelpMenu   *cur_menu = (YelpMenu *) cur->_private;
	    if (cur_menu && (cur_menu->has_submenus || cur_menu->metafiles))
		g_string_append_printf (gstr,
					"<li><a href='toc:%s'>%s</a></li>\n",
					cur_menu->id,
					cur_menu->title);
	}
	g_string_append (gstr, "</ul>\n");
    }

    if (menu->metafiles) {
	g_string_append_printf (gstr, "<h2>%s</h2><ul>\n", _("Documents"));
	for (c = menu->metafiles; c; c = c->next) {
	    YelpMetafile *meta = (YelpMetafile *) c->data;
	    g_string_append_printf (gstr,
				    "<li><a href='%s'>%s</a></li>\n",
				    meta->file,
				    meta->title);
	}
	g_string_append (gstr, "</ul>\n");
    }
}

static xmlChar *
node_get_title (xmlNodePtr node)
{
    xmlNodePtr  cur;
    xmlChar    *title = NULL;
    xmlChar    *language = NULL;
    gint        priority = 0;

    for (cur = node->children; cur; cur = cur->next) {
	if (!xmlStrcmp (cur->name, (const xmlChar *) "title")) {
	    gint pri = 0;
	    xmlChar *tlang = xmlNodeGetLang (cur);
	    GList *langs = (GList *) gnome_i18n_get_language_list ("LC_MESSAGES");

	    for ( ; langs != NULL; langs = langs->next) {
		gchar *lang = langs->data;
		pri++;

		if (lang == NULL || strchr (lang, '.') != NULL)
		    continue;

		if (!xmlStrcmp ((xmlChar *) lang, tlang))
		    break;
	    }
	    if (priority == 0 || pri < priority) {
		if (title)
		    xmlFree (title);
		if (language)
		    xmlFree (language);
		title = xmlNodeGetContent (cur);
		language = tlang;
		priority = pri;
	    } else {
		xmlFree (tlang);
	    }
	}
    }
    return title;
}

static void
toc_hash_metafile (YelpTocPager *pager, YelpMetafile *meta)
{
    GSList *category;
    YelpTocPagerPriv *priv = pager->priv;

    g_hash_table_insert (priv->unique_hash, meta->id, meta);

    for (category = meta->categories; category; category = category->next) {
	gchar  *catstr = (gchar *) category->data;
	GSList *metas =
	    (GSList *) g_hash_table_lookup (priv->category_hash, catstr);
	metas = g_slist_prepend (metas, meta);

	g_hash_table_insert (priv->category_hash, catstr, metas);
    }

    priv->idx_pending = g_slist_prepend (priv->idx_pending, meta);
}

static void
toc_unhash_metafile (YelpTocPager *pager, YelpMetafile *meta)
{
    GSList *category;
    YelpTocPagerPriv *priv = pager->priv;

    g_hash_table_remove (pager->priv->unique_hash, meta->id);

    for (category = meta->categories; category; category = category->next) {
	gchar  *catstr = (gchar *) category->data;
	GSList *metas =
	    (GSList *) g_hash_table_lookup (pager->priv->category_hash, catstr);

	if (metas) {
	    metas = g_slist_remove (metas, meta);
	    g_hash_table_insert (pager->priv->category_hash,
				 catstr, metas);
	}
    }

    priv->idx_pending = g_slist_remove (priv->idx_pending, meta);
}

static void
metafile_free (YelpMetafile *meta)
{
    GSList *category;

    if (meta) {
	g_free (meta->id);
	g_free (meta->title);
	g_free (meta->description);
	g_free (meta->seriesid);
	g_free (meta->language);

	for (category = meta->categories; category; category = category->next)
	    g_free (category->data);
	g_slist_free (meta->categories);

	g_free (meta->metafile);
	g_free (meta->file);
    }
    g_free (meta);
}
