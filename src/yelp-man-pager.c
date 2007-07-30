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
#include <glib/gi18n.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxslt/xslt.h>
#include <libxslt/templates.h>
#include <libxslt/transform.h>
#include <libxslt/extensions.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/xsltutils.h>

#include "yelp-error.h"
#include "yelp-man-pager.h"
#include "yelp-man-parser.h"
#include "yelp-settings.h"
#include "yelp-debug.h"

#define STYLESHEET_PATH DATADIR"/yelp/xslt/"
#define MAN_STYLESHEET  STYLESHEET_PATH"man2html.xsl"

#define YELP_MAN_PAGER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_MAN_PAGER, YelpManPagerPriv))

struct _YelpManPagerPriv {
    gpointer unused;
};

typedef struct _YelpManSect YelpManSect;
struct _YelpManSect {
    gchar  *title;
    gchar  *id;
    gchar  **sects;

    YelpManSect *parent;
    YelpManSect *child;
    YelpManSect *prev;
    YelpManSect *next;
};

static YelpManSect *man_section  = NULL;
static GHashTable  *man_secthash = NULL;

static void           man_pager_class_init   (YelpManPagerClass *klass);
static void           man_pager_init         (YelpManPager      *pager);
static void           man_pager_dispose      (GObject           *gobject);

static xmlDocPtr      man_pager_parse        (YelpPager        *pager);
static gchar **       man_pager_params       (YelpPager        *pager);

static const gchar *  man_pager_resolve_frag (YelpPager        *pager,
					      const gchar      *frag_id);
static GtkTreeModel * man_pager_get_sections (YelpPager        *pager);
static void           man_sections_init      (void);
static YelpManSect *  man_section_process    (xmlNodePtr        node,
					      YelpManSect      *parent,
					      YelpManSect      *previous);

static YelpPagerClass *parent_class;

typedef struct _YelpLangEncodings YelpLangEncodings;

struct _YelpLangEncodings {
    gchar *language;
    gchar *encoding;
};

/* http://www.w3.org/International/O-charset-lang.html */
static const YelpLangEncodings langmap[] = {
    { "C",     "ISO-8859-1" },
    { "af",    "ISO-8859-1" },
    { "ar",    "ISO-8859-6" },
    { "bg",    "ISO-8859-5" },
    { "be",    "ISO-8859-5" },
    { "ca",    "ISO-8859-1" },
    { "cs",    "ISO-8859-2" },
    { "da",    "ISO-8859-1" },
    { "de",    "ISO-8859-1" },
    { "el",    "ISO-8859-7" },
    { "en",    "ISO-8859-1" },
    { "eo",    "ISO-8859-3" },
    { "es",    "ISO-8859-1" },
    { "et",    "ISO-8859-15" },
    { "eu",    "ISO-8859-1" },
    { "fi",    "ISO-8859-1" },
    { "fo",    "ISO-8859-1" },
    { "fr",    "ISO-8859-1" },
    { "ga",    "ISO-8859-1" },
    { "gd",    "ISO-8859-1" },
    { "gl",    "ISO-8859-1" },
    { "hu",    "ISO-8859-2" },
    { "id",    "ISO-8859-1" }, /* is this right */
    { "mt",    "ISO-8859-3" },
    { "is",    "ISO-8859-1" },
    { "it",    "ISO-8859-1" },
    { "iw",    "ISO-8859-8" },
    { "ja",    "EUC-JP" },
    { "ko",    "EUC-KR" },
    { "lt",    "ISO-8859-13" },
    { "lv",    "ISO-8859-13" },
    { "mk",    "ISO-8859-5" },
    { "mt",    "ISO-8859-3" },
    { "no",    "ISO-8859-1" },
    { "pl",    "ISO-8859-2" },
    { "pt_BR", "ISO-8859-1" },
    { "ro",    "ISO-8859-2" },
    { "ru",    "KOI8-R" },
    { "sl",    "ISO-8859-2" },
    { "sr",    "ISO-8859-2" }, /* Latin, not cyrillic */
    { "sk",    "ISO-8859-2" },
    { "sv",    "ISO-8859-1" },
    { "tr",    "ISO-8859-9" },
    { "uk",    "ISO-8859-5" },
    { "zh_CN", "BIG5" },
    { "zh_TW", "BIG5" },
    { NULL,    NULL },
};
	

GType
yelp_man_pager_get_type (void)
{
    static GType type = 0;

    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpManPagerClass),
	    NULL,
	    NULL,
	    (GClassInitFunc) man_pager_class_init,
	    NULL,
	    NULL,
	    sizeof (YelpManPager),
	    0,
	    (GInstanceInitFunc) man_pager_init,
	};
	type = g_type_register_static (YELP_TYPE_XSLT_PAGER,
				       "YelpManPager", 
				       &info, 0);
    }
    return type;
}

static void
man_pager_class_init (YelpManPagerClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);
    YelpPagerClass *pager_class  = YELP_PAGER_CLASS (klass);
    YelpXsltPagerClass *xslt_class = YELP_XSLT_PAGER_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = man_pager_dispose;

    pager_class->resolve_frag = man_pager_resolve_frag;
    pager_class->get_sections = man_pager_get_sections;

    xslt_class->parse  = man_pager_parse;
    xslt_class->params = man_pager_params;

    xslt_class->stylesheet = MAN_STYLESHEET;

    g_type_class_add_private (klass, sizeof (YelpManPagerPriv));
}

static void
man_pager_init (YelpManPager *pager)
{
    pager->priv = YELP_MAN_PAGER_GET_PRIVATE (pager);
}

static void
man_pager_dispose (GObject *object)
{
    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

YelpPager *
yelp_man_pager_new (YelpDocInfo *doc_info)
{
    YelpManPager *pager;

    g_return_val_if_fail (doc_info != NULL, NULL);

    pager = (YelpManPager *) g_object_new (YELP_TYPE_MAN_PAGER,
					   "document-info", doc_info,
					   NULL);

    return (YelpPager *) pager;
}

static xmlDocPtr
man_pager_parse (YelpPager *pager)
{
    YelpDocInfo   *doc_info;
    gchar         *filename;
    const gchar   *language;
    const gchar   *encoding;
    YelpManParser *parser;
    xmlDocPtr      doc;
    GError        *error = NULL;
    gint           i;

    g_return_val_if_fail (YELP_IS_MAN_PAGER (pager), FALSE);

    doc_info = yelp_pager_get_doc_info (pager);
    filename = yelp_doc_info_get_filename (doc_info);

    g_object_ref (pager);

    /* We use the language to determine which encoding the manual
     * page is in */ 
    language = yelp_doc_info_get_language (doc_info);
    debug_print (DB_INFO, "The language of the man page is %s\n", language);

    /* default encoding if the language doesn't match below */
    encoding = g_getenv("MAN_ENCODING");
    if (encoding == NULL)
	encoding = "ISO-8859-1";

    if (language != NULL) {
	for (i=0; langmap[i].language != NULL; i++) {
	    if (g_str_equal (language, langmap[i].language)) {
		encoding = langmap[i].encoding;
		break;
	    }
	}
    }
    
    parser = yelp_man_parser_new ();
    doc = yelp_man_parser_parse_file (parser, filename, encoding);
    yelp_man_parser_free (parser);

    if (doc == NULL) {
	g_set_error (&error, YELP_ERROR, YELP_ERROR_NO_DOC,
		     _("The file ‘%s’ could not be parsed.  Either the file "
		       "does not exist, or it is formatted incorrectly."),
		     filename);
	yelp_pager_error (pager, error);
    }

    g_object_unref (pager);

    return doc;
}

static gchar **
man_pager_params (YelpPager *pager)
{
    YelpDocInfo *doc_info;
    gchar **params;
    gint params_i = 0;
    gint params_max = 20;

    gchar *uri, *file;
    gchar *c1 = NULL, *c2 = NULL, *mansect = NULL;

    gchar *linktrail = NULL;

    if (man_section == NULL)
	man_sections_init ();

    doc_info = yelp_pager_get_doc_info (pager);
    uri = yelp_doc_info_get_uri (doc_info, NULL, YELP_URI_TYPE_MAN);
    file = strrchr (uri, '/');
    if (file)
	file++;
    else
	file = uri;
    c1 = g_strrstr (file, ".bz2");
    if (c1 && strlen (c1) != 4)
	c1 = NULL;

    if (!c1) {
	c1 = g_strrstr (file, ".gz");
	if (c1 && strlen (c1) != 3)
	    c1 = NULL;
    }

    if (c1)
	c2 = g_strrstr_len (file, c1 - file, ".");
    else
	c2 = g_strrstr (file, ".");

    if (c2) {
	if (c1)
	    mansect = g_strndup (c2 + 1, c1 - c2 - 1);
	else
	    mansect = g_strdup (c2 + 1);
    }

    if (mansect != NULL) {
	YelpManSect *sectdata = g_hash_table_lookup (man_secthash, mansect);
	while (sectdata != NULL) {
	    if (linktrail) {
		gchar *new = g_strdup_printf ("%s|%s|%s",
					      sectdata->id,
					      sectdata->title,
					      linktrail);
		g_free (linktrail);
		linktrail = new;
	    } else {
		linktrail = g_strdup_printf ("%s|%s",
					     sectdata->id,
					     sectdata->title);
	    }
	    sectdata = sectdata->parent;
	}
    }
    g_free (mansect);
    g_free (uri);

    params = g_new0 (gchar *, params_max);

    yelp_settings_params (&params, &params_i, &params_max);

    if ((params_i + 10) >= params_max - 1) {
	params_max += 10;
	params = g_renew (gchar *, params, params_max);
    }

    if (linktrail) {
	params[params_i++] = "linktrail";
	params[params_i++] = g_strdup_printf ("\"%s\"", linktrail);
	g_free (linktrail);
    }

    params[params_i++] = "yelp.javascript";
    params[params_i++] = g_strdup_printf ("\"%s\"", DATADIR "/yelp/yelp.js");
    params[params_i++] = "stylesheet_path";
    params[params_i++] = g_strdup_printf ("\"file://%s\"", STYLESHEET_PATH);

    params[params_i] = NULL;

    return params;
}

static const gchar *
man_pager_resolve_frag (YelpPager *pager, const gchar *frag_id)
{
    return "index";
}

static GtkTreeModel *
man_pager_get_sections (YelpPager *pager)
{
    return NULL;
}

static void
man_sections_init (void)
{
    xmlParserCtxtPtr parser;
    xmlDocPtr doc;

    man_secthash = g_hash_table_new (g_str_hash, g_str_equal);

    parser = xmlNewParserCtxt ();
    doc = xmlCtxtReadFile (parser, DATADIR "/yelp/man.xml", NULL,
			   XML_PARSE_NOBLANKS | XML_PARSE_NOCDATA  |
			   XML_PARSE_NOENT    | XML_PARSE_NOERROR  |
			   XML_PARSE_NONET    );

    man_section = man_section_process (xmlDocGetRootElement (doc), NULL, NULL);

    xmlFreeParserCtxt (parser);
    xmlFreeDoc (doc);
}

static YelpManSect *
man_section_process (xmlNodePtr    node,
		     YelpManSect  *parent,
		     YelpManSect  *previous)
{
    YelpManSect *this_sect, *psect;
    xmlNodePtr cur, title_node;
    xmlChar *title_lang, *title;
    int i, j, title_pri;
    xmlChar *sect;

    const gchar * const * langs = g_get_language_names ();

    title_node = NULL;
    title_lang = NULL;
    title_pri  = INT_MAX;
    for (cur = node->children; cur; cur = cur->next) {
	if (!xmlStrcmp (cur->name, BAD_CAST "title")) {
	    xmlChar *cur_lang = NULL;
	    int cur_pri = INT_MAX;
	    cur_lang = xmlNodeGetLang (cur);
	    if (cur_lang) {
		for (j = 0; langs[j]; j++) {
		    if (g_str_equal (cur_lang, langs[j])) {
			cur_pri = j;
			break;
		    }
		}
	    } else {
		cur_pri = INT_MAX - 1;
	    }
	    if (cur_pri <= title_pri) {
		if (title_lang)
		    xmlFree (title_lang);
		title_lang = cur_lang;
		title_pri  = cur_pri;
		title_node = cur;
	    } else {
		if (cur_lang)
		    xmlFree (cur_lang);
	    }
	}
    }
    title = xmlNodeGetContent (title_node);

    this_sect = g_new0 (YelpManSect, 1);
    this_sect->title = g_strdup ((gchar *) title);

    this_sect->parent = parent;
    if (previous != NULL) {
	this_sect->prev = previous;
	previous->next = this_sect;
    } else if (parent != NULL) {
	parent->child = this_sect;
    }

    this_sect->id = g_strdup ((gchar *) xmlGetProp (node, BAD_CAST "id"));

    sect = xmlGetProp (node, BAD_CAST "sect");
    if (sect) {
	this_sect->sects = g_strsplit ((gchar *) sect, " ", 0);
	if (this_sect->sects) {
	    for (i=0; this_sect->sects[i] != NULL; i++)
		g_hash_table_insert (man_secthash, this_sect->sects[i], this_sect);
	}
    }

    psect = NULL;
    for (cur = node->children; cur; cur = cur->next) {
	if (!xmlStrcmp (cur->name, BAD_CAST "toc")) {
	    psect = man_section_process (cur, this_sect, psect);
	}
    }

    if (title_lang)
	xmlFree (title_lang);
    xmlFree (title);

    return this_sect;
}
