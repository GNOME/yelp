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
#include <libgnomevfs/gnome-vfs.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xmlreader.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/HTMLtree.h>
#include <libxml/tree.h>
#include <libxslt/xslt.h>
#include <libxslt/templates.h>
#include <libxslt/transform.h>
#include <libxslt/extensions.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/xsltutils.h>

#include <beagle/beagle.h>

#include "yelp-error.h"
#include "yelp-settings.h"
#include "yelp-search-pager.h"
#include "yelp-utils.h"

#define DESKTOP_ENTRY_GROUP     "Desktop Entry"
#define KDE_DESKTOP_ENTRY_GROUP "KDE Desktop Entry"

#ifdef YELP_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define YELP_NAMESPACE "http://www.gnome.org/yelp/ns"

#define STYLESHEET_PATH DATADIR"/yelp/xslt/"
#define SEARCH_STYLESHEET  STYLESHEET_PATH"search2html.xsl"

typedef gboolean      (*ProcessFunction)        (YelpSearchPager      *pager);

typedef struct _YelpListing YelpListing;

#define YELP_SEARCH_PAGER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_SEARCH_PAGER, YelpSearchPagerPriv))

struct _YelpSearchPagerPriv {
    xmlDocPtr     search_doc;
    xmlNodePtr    root;

    xmlParserCtxtPtr  parser;

    gboolean      cancel;

    xsltStylesheetPtr       stylesheet;
    xsltTransformContextPtr transformContext;
    char *search_terms;
    GPtrArray *hits;
    int   snippet_request_count;
};

struct _YelpListing {
    gchar       *id;
    gchar       *title;

    GSList      *listings;
    GSList      *documents;

    gboolean     has_listings;
};

static void          search_pager_class_init      (YelpSearchPagerClass *klass);
static void          search_pager_init            (YelpSearchPager      *pager);
static void          search_pager_dispose         (GObject           *gobject);

static void          search_pager_error           (YelpPager        *pager);
static void          search_pager_cancel          (YelpPager        *pager);
static void          search_pager_finish          (YelpPager        *pager);

gboolean             search_pager_process         (YelpPager         *pager);
void                 search_pager_cancel          (YelpPager         *pager);
const gchar *        search_pager_resolve_frag    (YelpPager         *pager,
						const gchar       *frag_id);
GtkTreeModel *       search_pager_get_sections    (YelpPager         *pager);

static gboolean      process_xslt              (YelpSearchPager      *pager);

static void          xslt_yelp_document        (xsltTransformContextPtr ctxt,
						xmlNodePtr              node,
						xmlNodePtr              inst,
						xsltStylePreCompPtr     comp);
static gboolean      search_pager_process_idle (YelpSearchPager        *pager);

static YelpPagerClass *parent_class;

static BeagleClient   *beagle_client;

GType
yelp_search_pager_get_type (void)
{
    static GType type = 0;

    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpSearchPagerClass),
	    NULL,
	    NULL,
	    (GClassInitFunc) search_pager_class_init,
	    NULL,
	    NULL,
	    sizeof (YelpSearchPager),
	    0,
	    (GInstanceInitFunc) search_pager_init,
	};
	type = g_type_register_static (YELP_TYPE_PAGER,
				       "YelpSearchPager", 
				       &info, 0);
    }
    return type;
}

static void
search_pager_class_init (YelpSearchPagerClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);
    YelpPagerClass *pager_class  = YELP_PAGER_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    beagle_client = beagle_client_new (NULL);

    object_class->dispose = search_pager_dispose;

    pager_class->error        = search_pager_error;
    pager_class->cancel       = search_pager_cancel;
    pager_class->finish       = search_pager_finish;

    pager_class->process      = search_pager_process;
    pager_class->cancel       = search_pager_cancel;
    pager_class->resolve_frag = search_pager_resolve_frag;
    pager_class->get_sections = search_pager_get_sections;

    g_type_class_add_private (klass, sizeof (YelpSearchPagerPriv));
}

static void
search_pager_init (YelpSearchPager *pager)
{
    YelpSearchPagerPriv *priv;

    pager->priv = priv = YELP_SEARCH_PAGER_GET_PRIVATE (pager);

    priv->parser = xmlNewParserCtxt ();

    priv->cancel                = 0;
    priv->search_terms          = NULL;
    priv->hits                  = NULL;
    priv->snippet_request_count = 0;
}

static void
search_pager_dispose (GObject *object)
{
    YelpSearchPager *pager = YELP_SEARCH_PAGER (object);

    g_free (pager->priv);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

static gboolean
check_hex (char check)
{
    if (check >= '0' && check <= '9')
	return TRUE;
    if (check >= 'a' && check <= 'f')
	return TRUE;
    if (check >= 'A' && check <= 'F')
	return TRUE;
    return FALSE;
}

static int
conv_hex (char conv)
{
    if (conv >= '0' && conv <= '9')
	return conv - '0';
    if (conv >= 'a' && conv <= 'f')
	return conv - 'a' + 10;
    if (conv >= 'A' && conv <= 'F')
	return conv - 'A' + 10;
    return 0;
}

static char *
decode_uri (const char *uri)
{
    char *decoded = g_strdup (uri);
    char *iterator;

    for (iterator = decoded; *iterator; iterator ++) {
	if (*iterator == '%' && check_hex (iterator[1]) && check_hex(iterator[2])) {
	    *iterator = conv_hex (iterator[1]) * 16 + conv_hex (iterator[2]);
	    memmove (iterator + 1, iterator + 3, strlen (iterator + 3));
	}
    }

    return decoded;
}

YelpSearchPager *
yelp_search_pager_get (YelpDocInfo *doc_info)
{
    static GHashTable *search_hash;
    YelpSearchPager *search_pager;
    char *uri;
    char *search_terms;

    if (search_hash == NULL) {
	search_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
					     NULL,       g_object_unref);
    }

    uri = yelp_doc_info_get_uri (doc_info, NULL, YELP_URI_TYPE_SEARCH);
    search_terms = decode_uri (uri + strlen ("x-yelp-search:"));
    g_free (uri);

    search_pager = g_hash_table_lookup (search_hash, search_terms);

    if (search_pager == NULL) {
	search_pager = (YelpSearchPager *) g_object_new (YELP_TYPE_SEARCH_PAGER,
							 "document-info", doc_info,
							 NULL);
	search_pager->priv->search_terms = search_terms;
	g_hash_table_insert (search_hash, search_terms, search_pager);
	yelp_pager_start (YELP_PAGER (search_pager));
    } else {
	g_free (search_terms);
    }

    g_object_ref (search_pager);
    return search_pager;
}

/******************************************************************************/

static void
search_pager_error (YelpPager *pager)
{
    d (g_print ("search_pager_error\n"));
    yelp_pager_set_state (pager, YELP_PAGER_STATE_ERROR);
}

static void
search_pager_cancel (YelpPager *pager)
{
    YelpSearchPagerPriv *priv = YELP_SEARCH_PAGER (pager)->priv;

    d (g_print ("search_pager_cancel\n"));
    yelp_pager_set_state (pager, YELP_PAGER_STATE_INVALID);

    priv->cancel = TRUE;
}

static void
search_pager_finish (YelpPager   *pager)
{
    d (g_print ("search_pager_finish\n"));
    yelp_pager_set_state (pager, YELP_PAGER_STATE_FINISHED);
}

gboolean
search_pager_process (YelpPager *pager)
{
    d (g_print ("search_pager_process\n"));

    if (beagle_client == NULL) {
	GError *error = NULL;
	g_set_error (&error, YELP_ERROR, YELP_ERROR_PROC,
		     _("Your search could not be processed. There "
		       "is no connection to the beagle daemon."),
		     SEARCH_STYLESHEET);
	yelp_pager_error (YELP_PAGER (pager), error);
	return FALSE;
    }

    yelp_pager_set_state (pager, YELP_PAGER_STATE_PARSING);
    g_signal_emit_by_name (pager, "parse");

    /* Set it running */
    yelp_pager_set_state (pager, YELP_PAGER_STATE_RUNNING);
    g_signal_emit_by_name (pager, "start");

    gtk_idle_add_priority (G_PRIORITY_LOW,
			   (GtkFunction) search_pager_process_idle,
			   pager);
    return FALSE;
}

const gchar *
search_pager_resolve_frag (YelpPager *pager, const gchar *frag_id)
{
    if (!frag_id)
	return "results";
    else
	return frag_id;
}

GtkTreeModel *
search_pager_get_sections (YelpPager *pager)
{
    return NULL;
}

/******************************************************************************/

static void
check_finished (YelpSearchPager *pager)
{
    if (pager->priv->snippet_request_count == 0) {
	gtk_idle_add_priority (G_PRIORITY_LOW,
			       (GtkFunction) process_xslt,
			       pager);
    }
}

typedef struct
{
    YelpSearchPager *pager;
    xmlNode *node;
} SnippetLocation;

static void
snippet_closed (BeagleSnippetRequest *request, SnippetLocation *snippet_location)
{
    YelpSearchPager *pager = snippet_location->pager;
    
    d(g_print ("snippet_closed\n"));

    pager->priv->snippet_request_count --;
    check_finished (pager);
    g_free (snippet_location);
    g_object_unref (request);
}

static void
snippet_response (BeagleSnippetRequest *request, BeagleSnippetResponse *response, SnippetLocation *snippet_location)
{
    xmlDoc *snippet_doc;
    xmlNode *node;
    char *xmldoc;

    const char *xml = beagle_snippet_response_get_snippet (response);

    if (xml == NULL) {
	d(g_print ("snippet_response empty\n"));
	goto done;
    }
    d(g_print ("snippet_response: %s\n", xml));

    xmldoc = g_strdup_printf ("<snippet>%s</snippet>",  xml);
    snippet_doc = xmlParseDoc (xmldoc);
    g_free (xmldoc);
    if (!snippet_doc)
	goto done;
    node = xmlDocGetRootElement (snippet_doc);
    xmlUnlinkNode (node);
    xmlAddChild (snippet_location->node, node);
    xmlFreeDoc (snippet_doc);
    done:
    snippet_closed (request, snippet_location);
}

static void
snippet_error (BeagleSnippetRequest *request, GError *error, SnippetLocation *snippet_location)
{
    d(g_print ("snippet_error\n"));
    snippet_closed (request, snippet_location);
}


static void
hits_added_cb (BeagleQuery *query, BeagleHitsAddedResponse *response, YelpSearchPager *pager)
{
    YelpSearchPagerPriv *priv = YELP_SEARCH_PAGER (pager)->priv;

    GSList *hits, *l;

    d(g_print ("hits_added\n"));

    hits = beagle_hits_added_response_get_hits (response);

    for (l = hits; l; l = l->next) {
	BeagleHit *hit = l->data;
	beagle_hit_ref (hit);
	d(g_print ("%f\n", beagle_hit_get_score (hit)));
	g_ptr_array_add (priv->hits, hit);
    }
}

static gint
compare_hits (gconstpointer  a,
	      gconstpointer  b)
{
    BeagleHit **hita = (BeagleHit **) a;
    BeagleHit **hitb = (BeagleHit **) b;

    double scorea = beagle_hit_get_score (*hita);
    double scoreb = beagle_hit_get_score (*hitb);

    /* The values here are inverted so that it's a descending sort. */
    if (scorea < scoreb)
	return 1;
    if (scoreb < scorea)
	return -1;
    return 0;
}

static void
finished_cb (BeagleQuery            *query,
	     BeagleFinishedResponse *response, 
	     YelpSearchPager        *pager)
{
    int i;
    YelpSearchPagerPriv *priv = YELP_SEARCH_PAGER (pager)->priv;

    d(g_print ("finished_cb\n"));
    g_object_unref (query);

    g_ptr_array_sort (priv->hits, compare_hits);

    for (i = 0; i < 10; i++) {
	BeagleHit *hit = g_ptr_array_index (priv->hits, i);
	xmlNode *child;
	static float score_fake = 0;
	char *score;
	const char *property;
	BeagleSnippetRequest *request;
	SnippetLocation *snippet_location;

	child = xmlNewTextChild (priv->root, NULL, "result", NULL);
	xmlSetProp (child, "uri", beagle_hit_get_uri (hit));
	xmlSetProp (child, "parent_uri", beagle_hit_get_parent_uri (hit));
	property = beagle_hit_get_property (hit, "dc:title");
	if (property)
	    xmlSetProp (child, "title", property);

	score = g_strdup_printf ("%f", beagle_hit_get_score (hit));
	d(g_print ("%f\n", beagle_hit_get_score (hit)));
	xmlSetProp (child, "score", score);
	g_free (score);

	priv->snippet_request_count ++;

	snippet_location = g_new (SnippetLocation, 1);

	snippet_location->pager = pager;
	snippet_location->node = child;

	request = beagle_snippet_request_new ();
	beagle_snippet_request_set_hit (request, hit);
	beagle_snippet_request_add_query_term (request, priv->search_terms);

	g_signal_connect (request, "response",
			  G_CALLBACK (snippet_response), snippet_location);
	g_signal_connect (request, "error",
			  G_CALLBACK (snippet_error), snippet_location);
	g_signal_connect (request, "closed",
			  G_CALLBACK (snippet_closed), snippet_location);

	d(g_print ("Requesting snippet\n"));
	beagle_client_send_request_async (beagle_client, BEAGLE_REQUEST (request),
					  NULL);
    }

    g_ptr_array_foreach (priv->hits, (GFunc) beagle_hit_unref, NULL);
    g_ptr_array_free (priv->hits, TRUE);
    priv->hits = NULL;
}

static gboolean
search_pager_process_idle (YelpSearchPager *pager)
{
    BeagleQuery    *query;
    YelpSearchPagerPriv *priv = YELP_SEARCH_PAGER (pager)->priv;
    GError *error = NULL;

    priv->search_doc = xmlNewDoc ("1.0");
    priv->root = xmlNewNode (NULL, "search");
    xmlSetProp (priv->root, "title", priv->search_terms);
    xmlDocSetRootElement (priv->search_doc, priv->root);

    query = beagle_query_new ();

    beagle_query_set_max_hits (query, 10000);
    beagle_query_add_text (query, priv->search_terms);
    beagle_query_add_mime_type (query, "text/html");
    beagle_query_add_mime_type (query, "application/xhtml+xml");
    beagle_query_add_mime_type (query, "text/xml");
    beagle_query_add_mime_type (query, "application/xml");
    beagle_query_add_mime_type (query, "application/docbook+xml");
    /*    beagle_query_add_hit_type (query, "DocBookEntry");*/

    priv->hits = g_ptr_array_new ();

    g_signal_connect (query, "hits-added",
		      G_CALLBACK (hits_added_cb),
		      pager);
    
    g_signal_connect (query, "finished",
		      G_CALLBACK (finished_cb),
		      pager);
	
    d(g_print ("Request: %s\n", beagle_client_send_request_async (beagle_client, BEAGLE_REQUEST (query),
								  &error) ? "true" : "false"));

    if (error) {
	g_print ("error: %s\n", error->message);
    }

    return FALSE;
}

static gboolean
process_xslt (YelpSearchPager *pager)
{
    GError *error = NULL;
    xmlDocPtr outdoc = NULL;
    YelpSearchPagerPriv *priv = pager->priv;
    gchar **params;
    gint  params_i = 0;
    gint  params_max = 12;
    GtkIconInfo *info;
    GtkIconTheme *theme = (GtkIconTheme *) yelp_settings_get_icon_theme ();

    d(xmlDocFormatDump(stdout, priv->search_doc, 1));

    priv->stylesheet = xsltParseStylesheetFile (SEARCH_STYLESHEET);
    if (!priv->stylesheet) {
	g_set_error (&error, YELP_ERROR, YELP_ERROR_PROC,
		     _("Your search could not be processed. The "
		       "file ‘%s’ is either missing or is not a valid XSLT "
		       "stylesheet."),
		     SEARCH_STYLESHEET);
	yelp_pager_error (YELP_PAGER (pager), error);
	goto done;
    }

    priv->transformContext = xsltNewTransformContext (priv->stylesheet,
						      priv->search_doc);
    priv->transformContext->_private = pager;
    xsltRegisterExtElement (priv->transformContext,
			    "document",
			    YELP_NAMESPACE,
			    (xsltTransformFunction) xslt_yelp_document);

    params = g_new0 (gchar *, params_max);
    yelp_settings_params (&params, &params_i, &params_max);

    if ((params_i + 14) >= params_max - 1) {
	params_max += 14;
	params = g_renew (gchar *, params, params_max);
    }

    info = gtk_icon_theme_lookup_icon (theme, "yelp-icon-big", 192, 0);
    if (info) {
	params[params_i++] = "help_icon";
	params[params_i++] = g_strdup_printf ("\"%s\"",
					      gtk_icon_info_get_filename (info));
	params[params_i++] = "help_icon_size";
	params[params_i++] = g_strdup_printf ("%i",
					      gtk_icon_info_get_base_size (info));
	gtk_icon_info_free (info);
    }

    params[params_i++] = "yelp.javascript";
    params[params_i++] = g_strdup_printf ("\"%s\"", DATADIR "/yelp/yelp.js");
    params[params_i++] = "yelp.topimage";
    params[params_i++] = g_strdup_printf ("\"%s\"", DATADIR "/yelp/icons/help-title.png");

    params[params_i++] = NULL;

    outdoc = xsltApplyStylesheetUser (priv->stylesheet,
				      priv->search_doc,
				      (const gchar **)params, NULL, NULL,
				      priv->transformContext);
    /* Don't do this */
    /*    g_signal_emit_by_name (pager, "finish");*/

 done:
    for (params_i = 0; params[params_i] != NULL; params_i++)
	if (params_i % 2 == 1)
	    g_free ((gchar *) params[params_i]);
    if (outdoc)
	xmlFreeDoc (outdoc);
    if (priv->search_doc) {
	xmlFreeDoc (priv->search_doc);
	priv->search_doc = NULL;
    }
    if (priv->stylesheet) {
	xsltFreeStylesheet (priv->stylesheet);
	priv->stylesheet = NULL;
    }
    if (priv->transformContext) {
	xsltFreeTransformContext (priv->transformContext);
	priv->transformContext = NULL;
    }

    g_signal_emit_by_name (pager, "finish");
    return FALSE;
}

static void
xslt_yelp_document (xsltTransformContextPtr ctxt,
		    xmlNodePtr              node,
		    xmlNodePtr              inst,
		    xsltStylePreCompPtr     comp)
{
    GError  *error = NULL;
    YelpPage *page;
    xmlChar *page_id = NULL;
    xmlChar *page_title = NULL;
    xmlChar *page_buf;
    gint     buf_size;
    YelpPager *pager;
    xsltStylesheetPtr style = NULL;
    const char *old_outfile;
    xmlDocPtr   new_doc = NULL;
    xmlDocPtr   old_doc;
    xmlNodePtr  old_insert;
    xmlNodePtr  cur;

    if (!ctxt || !node || !inst || !comp)
	return;

    pager = (YelpPager *) ctxt->_private;

    page_id = xsltEvalAttrValueTemplate (ctxt, inst,
					 (const xmlChar *) "href",
					 NULL);
    if (page_id == NULL) {
	xsltTransformError (ctxt, NULL, inst,
			    _("No href attribute found on yelp:document"));
	error = NULL;
	yelp_pager_error (pager, error);
	goto done;
    }

    old_outfile = ctxt->outputFile;
    old_doc     = ctxt->output;
    old_insert  = ctxt->insert;
    ctxt->outputFile = (const char *) page_id;

    style = xsltNewStylesheet ();
    if (style == NULL) {
	xsltTransformError (ctxt, NULL, inst,
			    _("Out of memory"));
	error = NULL;
	yelp_pager_error (pager, error);
	goto done;
    }

    style->omitXmlDeclaration = TRUE;

    new_doc = xmlNewDoc ("1.0");
    new_doc->charset = XML_CHAR_ENCODING_UTF8;
    new_doc->dict = ctxt->dict;
    xmlDictReference (new_doc->dict);

    ctxt->output = new_doc;
    ctxt->insert = (xmlNodePtr) new_doc;

    xsltApplyOneTemplate (ctxt, node, inst->children, NULL, NULL);

    xsltSaveResultToString (&page_buf, &buf_size, new_doc, style);

    ctxt->outputFile = old_outfile;
    ctxt->output     = old_doc;
    ctxt->insert     = old_insert;

    for (cur = node->children; cur; cur = cur->next) {
	if (!xmlStrcmp (cur->name, BAD_CAST "title")) {
	    page_title = xmlNodeGetContent (cur);
	    break;
	}
    }

    page = g_new0 (YelpPage, 1);

    if (page_id) {
	page->page_id = g_strdup (page_id);
	xmlFree (page_id);
    }
    if (page_title) {
	page->title = g_strdup (page_title);
	xmlFree (page_title);
    } else {
	page->title = g_strdup (_("Help Contents"));
    }
    page->contents = page_buf;

    cur = xmlDocGetRootElement (new_doc);
    for (cur = cur->children; cur; cur = cur->next) {
	if (!xmlStrcmp (cur->name, (xmlChar *) "head")) {
	    for (cur = cur->children; cur; cur = cur->next) {
		if (!xmlStrcmp (cur->name, (xmlChar *) "link")) {
		    xmlChar *rel = xmlGetProp (cur, "rel");

		    if (!xmlStrcmp (rel, (xmlChar *) "Previous"))
			page->prev_id = xmlGetProp (cur, "href");
		    else if (!xmlStrcmp (rel, (xmlChar *) "Next"))
			page->next_id = xmlGetProp (cur, "href");
		    else if (!xmlStrcmp (rel, (xmlChar *) "Top"))
			page->toc_id = xmlGetProp (cur, "href");

		    xmlFree (rel);
		}
	    }
	    break;
	}
    }

    yelp_pager_add_page (pager, page);
    g_signal_emit_by_name (pager, "page", page->page_id);

 done:
    if (new_doc)
	xmlFreeDoc (new_doc);
    if (style)
	xsltFreeStylesheet (style);
}

#ifdef ENABLE_SCROLLKEEPER
static void
xml_trim_titles (xmlNodePtr node)
{
    xmlNodePtr cur, keep = NULL;
    xmlChar *keep_lang = NULL;
    int j, keep_pri = INT_MAX;

    const gchar * const * langs = g_get_language_names ();

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
	    if (cur_pri <= keep_pri) {
		if (keep_lang)
		    xmlFree (keep_lang);
		keep_lang = cur_lang;
		keep_pri = cur_pri;
		keep = cur;
	    } else {
		if (cur_lang)
		    xmlFree (cur_lang);
	    }
	}
    }
    cur = node->children;
    while (cur) {
	xmlNodePtr this = cur;
	cur = cur->next;
	if (!xmlStrcmp (this->name, BAD_CAST "title")) {
	    if (this != keep) {
		xmlUnlinkNode (this);
		xmlFreeNode (this);
	    }
	}
    }
    xmlFree (keep_lang);
}
#endif
