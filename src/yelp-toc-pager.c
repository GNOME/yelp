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

#include "yelp-error.h"
#include "yelp-toc-pager.h"

#define d(x)

typedef struct _OMF OMF;

struct _YelpTocPagerPriv {
    GSList           *omf_pending;

    xmlDocPtr         toc_doc;
    GSList           *toc_pending;

    GSList           *idx_pending;

    GHashTable       *unique_hash_omf;
    GHashTable       *category_hash_omf;

    xmlParserCtxtPtr  parser;

    gint              pause_count;
    GtkFunction       unpause_func;
};

struct _OMF {
    xmlChar   *title;
    xmlChar   *description;
    xmlChar   *seriesid;

    gchar     *uniqueid;

    xmlChar   *language;
    gint       lang_priority;

    GSList    *categories;

    xmlChar   *omf_file;
    xmlChar   *xml_file;
};

static void          toc_pager_class_init      (YelpTocPagerClass *klass);
static void          toc_pager_init            (YelpTocPager      *pager);
static void          toc_pager_dispose         (GObject           *gobject);

gboolean             toc_pager_process         (YelpPager         *pager);
void                 toc_pager_cancel          (YelpPager         *pager);
gchar *              toc_pager_resolve_uri     (YelpPager         *pager,
						YelpURI           *uri);
const GtkTreeModel * toc_pager_get_sections    (YelpPager         *pager);

static void          toc_read_omf_dir          (YelpTocPager      *pager,
						const gchar       *dirstr);
static gboolean      toc_process_omf_pending   (YelpPager         *pager);
static void          toc_hash_omf              (YelpTocPager      *pager,
						OMF               *omf);
static void          toc_unhash_omf            (YelpTocPager      *pager,
						OMF               *omf);

static gboolean      toc_process_toc           (YelpTocPager      *pager);
static gboolean      toc_process_toc_pending   (YelpTocPager      *pager);

static gboolean      toc_process_idx           (YelpTocPager      *pager);
static gboolean      toc_process_idx_pending   (YelpTocPager      *pager);

static gchar *       toc_write_page            (gchar             *id,
						gchar             *title,
						GSList            *cats,
						GSList            *omfs);

static xmlChar *     node_get_title            (xmlNodePtr         node);

static void          omf_free                  (OMF               *omf);

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

    priv->unique_hash_omf = g_hash_table_new (g_str_hash, g_str_equal);
    priv->category_hash_omf = g_hash_table_new (g_str_hash, g_str_equal);

    priv->pause_count = 0;
    priv->unpause_func = NULL;
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

YelpTocPager *
yelp_toc_pager_get (void)
{
    return toc_pager;
}

void
yelp_toc_pager_pause (YelpTocPager *pager)
{
    pager->priv->pause_count = pager->priv->pause_count + 1;
}

void
yelp_toc_pager_unpause (YelpTocPager *pager)
{
    pager->priv->pause_count = pager->priv->pause_count - 1;

    if (pager->priv->pause_count < 0) {
	g_warning (_("YelpTocPager: Pause count is negative."));
	pager->priv->pause_count = 0;
    }

    if (pager->priv->pause_count < 1 && pager->priv->unpause_func) {
	gtk_idle_add_priority (G_PRIORITY_LOW,
			       pager->priv->unpause_func,
			       pager);
	pager->priv->unpause_func = NULL;
    }
}

/******************************************************************************/

gboolean
toc_pager_process (YelpPager *pager)
{
    toc_read_omf_dir (YELP_TOC_PAGER (pager), DATADIR"/omf");

    gtk_idle_add_priority (G_PRIORITY_LOW,
			   (GtkFunction) toc_process_omf_pending,
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
    gchar *path = yelp_uri_get_path (uri);

    if (!strcmp (path, "")) {
	g_free (path);
	return g_strdup ("index");
    }
    else if (!path)
	return g_strdup ("index");
    else
	return path;
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
toc_process_omf_pending (YelpPager *pager)
{
    GSList    *first;
    gchar     *file;
    xmlDocPtr  omf_doc = NULL;
    xmlNodePtr omf_cur;
    gint       lang_priority;
    OMF       *omf     = NULL;
    OMF       *omf_old = NULL;
    GList     *langs;

    YelpTocPagerPriv *priv  = YELP_TOC_PAGER (pager)->priv;

    first = priv->omf_pending;
    priv->omf_pending = g_slist_remove_link (priv->omf_pending, first);

    file = (gchar *) first->data;

    if (!g_str_has_suffix (file, ".omf"))
	goto done;

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

    if (!omf->seriesid) {
	omf_free (omf);
	goto done;
    }

    /* Do something better for uniqueid, since people can't seem to manage
     * to create a proper seriesid.  Ugh.
     */
    omf->uniqueid = g_strconcat ("seriesid:", (gchar *) omf->seriesid, NULL);

    /* If we have one with the same uniqueid, use the one with
     * the lowest lang_priority.
     */
    omf_old = g_hash_table_lookup (priv->unique_hash_omf, omf->uniqueid);
    if (omf_old) {
	if (omf_old->lang_priority < omf->lang_priority) {
	    omf_free (omf);
	    goto done;
	} else {
	    toc_unhash_omf (YELP_TOC_PAGER (pager), omf_old);
	    omf_free (omf_old);

	    toc_hash_omf (YELP_TOC_PAGER (pager), omf);
	}
    } else {
	toc_hash_omf (YELP_TOC_PAGER (pager), omf);
    }
    
 done:
    xmlFreeDoc (omf_doc);
    g_free (file);
    g_slist_free_1 (first);

    if (!priv->omf_pending) {
	if (priv->pause_count > 0)
	    priv->unpause_func = (GtkFunction) toc_process_toc;
	else
	    gtk_idle_add_priority (G_PRIORITY_LOW,
				   (GtkFunction) toc_process_toc,
				   pager);
	return FALSE;
    }
    if (priv->pause_count > 0) {
	priv->unpause_func = (GtkFunction) toc_process_omf_pending;
	return FALSE;
    }
    else
	return TRUE;
}

static gboolean
toc_process_toc (YelpTocPager *pager)
{
    xmlNodePtr   toc_node;
    GError      *error = NULL;

    YelpTocPagerPriv *priv = pager->priv;

    priv->toc_doc = xmlCtxtReadFile (priv->parser,
				     DATADIR "/yelp/toc.xml",
				     NULL, 0);

    if (!priv->toc_doc) {
	g_set_error (&error,
		     YELP_ERROR,
		     YELP_ERROR_FAILED_TOC,
		     _("The table of contents could not be read."));
	yelp_pager_error (YELP_PAGER (pager), error);
	return FALSE;
    }

    toc_node = xmlDocGetRootElement (priv->toc_doc);

    if (xmlStrcmp (toc_node->name, (const xmlChar *) "toc")) {
	g_set_error (&error,
		     YELP_ERROR,
		     YELP_ERROR_FAILED_TOC,
		     _("The table of contents could not be read."));
	yelp_pager_error (YELP_PAGER (pager), error);
	return FALSE;
    }

    priv->toc_pending = g_slist_append (priv->toc_pending, toc_node);

    gtk_idle_add_priority (G_PRIORITY_LOW,
			   (GtkFunction) toc_process_toc_pending,
			   pager);

    return FALSE;
}

static gboolean
toc_process_toc_pending (YelpTocPager *pager)
{
    GSList      *first;
    xmlNodePtr   node;
    xmlNodePtr   cur;
    xmlChar     *id = NULL;
    xmlChar     *title;
    GSList      *subcats = NULL;
    GSList      *subomfs = NULL;

    YelpTocPagerPriv *priv = pager->priv;

    first = priv->toc_pending;
    priv->toc_pending = g_slist_remove_link (priv->toc_pending, first);

    node = first->data;

    id = xmlGetProp (node, (const xmlChar *) "id");

    title = node_get_title (node);

    for (cur = node->children; cur; cur = cur->next) {
	if (cur->type == XML_ELEMENT_NODE) {
	    if (!xmlStrcmp (cur->name, "toc")) {
		priv->toc_pending = g_slist_append (priv->toc_pending, cur);
		subcats = g_slist_append (subcats, cur);
	    }
	    else if (!xmlStrcmp (cur->name, "category")) {
		GSList  *omf;
		xmlChar *cat;

		cat = xmlNodeGetContent (cur);
		omf = g_hash_table_lookup (priv->category_hash_omf, cat);

		for ( ; omf; omf = omf->next)
		    subomfs = g_slist_prepend (subomfs, omf->data);

		xmlFree (cat);
	    }
	}
    }

    if (id) {
	gchar *page = toc_write_page (id, title, subcats, subomfs);
	yelp_pager_add_page (YELP_PAGER (pager), id, title, page);
	g_signal_emit_by_name (pager, "page", id);
    } else {
	g_warning (_("YelpTocPager: TOC entry has no id."));
	g_free (title);
    }

    g_slist_free (subcats);
    g_slist_free (subomfs);
    g_slist_free_1 (first);

    if (!priv->toc_pending) {
	xmlFreeDoc (priv->toc_doc);
	g_signal_emit_by_name (pager, "finish");

	if (priv->pause_count > 0)
	    priv->unpause_func = (GtkFunction) toc_process_idx;
	else
	    gtk_idle_add_priority (G_PRIORITY_LOW,
				   (GtkFunction) toc_process_idx,
				   pager);
	return FALSE;
    }
    else if (priv->pause_count > 0) {
	priv->unpause_func = (GtkFunction) toc_process_toc_pending;
	return FALSE;
    }
    else {
	return TRUE;
    }
}

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
    GSList      *first;
    OMF         *omf;
    YelpURI     *uri;
    gchar       *path;
    xmlDocPtr    doc;

    YelpTocPagerPriv *priv = pager->priv;

    first = priv->idx_pending;
    priv->idx_pending = g_slist_remove_link (priv->idx_pending, first);

    omf = first->data;

    //printf ("OMF: %s\n", omf->xml_file);

    uri  = yelp_uri_new (omf->xml_file);
    path = yelp_uri_get_path (uri);

    /*
    doc = xmlCtxtReadFile (priv->parser,
			   path,
			   NULL, 0);

    xmlFreeDoc (doc);
    */

    g_free (path);
    g_object_unref (uri);
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

static gchar *
toc_write_page (gchar     *id,
		gchar     *title,
		GSList    *cats,
		GSList    *omfs)
{
    gint      i = 0;
    GSList   *c;
    gchar   **strs;
    gint      strs_len;
    gchar    *page;
    gint      page_len = 0;
    gchar    *page_end;

    strs_len = g_slist_length (cats) + g_slist_length (omfs) + 20;

    strs = g_new0 (gchar *, strs_len);

    strs[i] = g_strconcat ("<html><body><h1>", title, "</h1>\n", NULL);
    page_len += strlen (strs[i]);

    if (cats) {
	strs[++i] = g_strconcat ("<h2>", _("Categories"), "</h2><ul>\n", NULL);
	page_len += strlen (strs[i]);

	for (c = cats; c; c = c->next) {
	    xmlNodePtr cur = (xmlNodePtr) c->data;
	    xmlChar *id, *title;

	    id = xmlGetProp (cur, (const xmlChar *) "id");
	    title = node_get_title (cur);

	    strs[++i] = g_strconcat ("<li><a href='toc:", id, "'>",
				     title,
				     "</a></li>\n",
				     NULL);
	    page_len += strlen (strs[i]);

	    xmlFree (id);
	    xmlFree (title);
	}
	strs[++i] = g_strdup ("</ul>\n");
	page_len += strlen (strs[i]);
    }

    if (omfs) {
	strs[++i] = g_strconcat ("<h2>", _("Documents"), "</h2><ul>\n", NULL);
	page_len += strlen (strs[i]);

	for (c = omfs; c; c = c->next) {
	    OMF *omf = (OMF *) c->data;
	    strs[++i] = g_strconcat ("<li><a href='", omf->xml_file, "'>",
				     omf->title,
				     "</a></li>\n",
				     NULL);
	    page_len += strlen (strs[i]);
	}
	strs[++i] = g_strdup ("</ul>\n");
	page_len += strlen (strs[i]);
    }

    strs[++i] = g_strdup ("</body></html>\n");
    page_len += strlen (strs[i]);

    // Not necessary, but I'm paranoid.
    strs[++i] = NULL;

    page = g_new0 (gchar, page_len + 1);
    page_end = page;

    for (i = 0; i < strs_len; i++)
	if (strs[i]) {
	    page_end = g_stpcpy (page_end, strs[i]);
	    g_free (strs[i]);
	}

    g_free (strs);

    return page;
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
toc_hash_omf (YelpTocPager *pager, OMF *omf)
{
    GSList *category;
    YelpTocPagerPriv *priv = pager->priv;

    g_hash_table_insert (priv->unique_hash_omf,
			 omf->uniqueid,
			 omf);

    for (category = omf->categories; category; category = category->next) {
	gchar  *catstr = (gchar *) category->data;
	GSList *omfs =
	    (GSList *) g_hash_table_lookup (priv->category_hash_omf,
					    catstr);
	omfs = g_slist_prepend (omfs, omf);

	g_hash_table_insert (priv->category_hash_omf,
			     catstr, omfs);
    }

    priv->idx_pending = g_slist_prepend (priv->idx_pending, omf);
}

static void
toc_unhash_omf (YelpTocPager *pager, OMF *omf)
{
    GSList *category;
    YelpTocPagerPriv *priv = pager->priv;

    g_hash_table_remove (pager->priv->unique_hash_omf,
			 omf->uniqueid);

    for (category = omf->categories; category; category = category->next) {
	gchar  *catstr = (gchar *) category->data;
	GSList *omfs =
	    (GSList *) g_hash_table_lookup (pager->priv->category_hash_omf,
					    catstr);
	if (omfs) {
	    omfs = g_slist_remove (omfs, omf);
	    g_hash_table_insert (pager->priv->category_hash_omf,
				 catstr, omfs);
	}
    }

    priv->idx_pending = g_slist_remove (priv->idx_pending, omf);
}

static void
omf_free (OMF *omf)
{
    GSList *category;

    if (omf) {
	xmlFree (omf->title);
	xmlFree (omf->description);
	xmlFree (omf->seriesid);
	xmlFree (omf->uniqueid);
	xmlFree (omf->language);

	for (category = omf->categories; category; category = category->next)
	    xmlFree ((xmlChar *) category->data);
	g_slist_free (omf->categories);

	xmlFree (omf->omf_file);
	xmlFree (omf->xml_file);
    }
    g_free (omf);
}

