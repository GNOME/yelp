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

#ifdef ENABLE_BEAGLE
#include <beagle/beagle.h>
#endif /* ENABLE_BEAGLE */

#include "yelp-error.h"
#include "yelp-settings.h"
#include "yelp-search-pager.h"
#include "yelp-utils.h"
#include "yelp-debug.h"

#define DESKTOP_ENTRY_GROUP     "Desktop Entry"
#define KDE_DESKTOP_ENTRY_GROUP "KDE Desktop Entry"

#define YELP_NAMESPACE "http://www.gnome.org/yelp/ns"

#define STYLESHEET_PATH DATADIR"/yelp/xslt/"
#define SEARCH_STYLESHEET  STYLESHEET_PATH"search2html.xsl"

typedef gboolean      (*ProcessFunction)        (YelpSearchPager      *pager);

typedef struct _SearchContainer SearchContainer;

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
    GSList * pending_searches;
};

enum {
    NOT_SEARCHING = 0,
    SEARCH_1,
    SEARCH_CHILD,
    SEARCH_DOC = 99
};

struct _SearchContainer {
    gchar *      current_subsection;
    gchar *      result_subsection;
    gchar *      doc_title;
    gchar *      base_path;
    gchar *      base_filename;
    gchar *      snippet;
    GSList *     components;
    GHashTable  *entities;
    gchar **     search_term;
    gint         required_words;
    gint *       dup_of;
    gboolean *   found_terms;
    gboolean *   stop_word;
    gfloat *     score_per_word;
    gchar *      top_element;
    gint         search_status;
    gchar *      elem_type;
    GSList *     elem_stack;
    gfloat       score;
    gfloat       snippet_score;
    gboolean     html;
    gchar *      sect_name;
    gboolean     grab_text;
    gchar *      default_snippet;
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

static void          s_startElement            (void       *data,
						const xmlChar         *name,
						const xmlChar        **attrs);
static void          s_endElement              (void       *data,
						const xmlChar         *name);
static void          s_characters              (void       *data,
						const xmlChar         *ch,
						int                    len);
static void          s_declEntity              (void       *data, 
						const xmlChar         *name, 
						int                    type,
						const xmlChar         *pID, 
						const xmlChar         *sID,
						xmlChar              *content);
static xmlEntityPtr  s_getEntity               (void      *data, 
						const xmlChar        *name);
static gboolean      slow_search_setup         (YelpSearchPager       *pager);
static gboolean      slow_search_process       (YelpSearchPager       *pager);
static void          search_parse_result       (YelpSearchPager       *pager,
						SearchContainer       *c);
static gchar *       search_clean_snippet      (gchar                 *snippet,
						gchar                **terms);
static void          search_process_man        (YelpSearchPager       *pager,
						gchar                **terms);
static void          search_process_info       (YelpSearchPager        *pager,
						gchar                **terms);
static void          process_man_result        (YelpSearchPager       *pager, 
						gchar                 *result, 
						gchar                **terms);
void                 process_info_result       (YelpSearchPager       *pager, 
						gchar                 *result,
						gchar                **terms);
gchar *              string_append             (gchar                 *current,
						gchar                 *new, 
						gchar                 *suffix);


#ifdef ENABLE_BEAGLE
static BeagleClient   *beagle_client;
#endif /* ENABLE_BEAGLE */
static char const * const * langs;


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

#ifdef ENABLE_BEAGLE
    beagle_client = beagle_client_new (NULL);
    debug_print (DB_DEBUG, "client: %p\n", beagle_client);
#endif /* ENABLE_BEAGLE */

    langs = g_get_language_names ();

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
    debug_print (DB_FUNCTION, "entering\n");
    yelp_pager_set_state (pager, YELP_PAGER_STATE_ERROR);
}

static void
search_pager_cancel (YelpPager *pager)
{
    YelpSearchPagerPriv *priv = YELP_SEARCH_PAGER (pager)->priv;

    debug_print (DB_FUNCTION, "entering\n");
    yelp_pager_set_state (pager, YELP_PAGER_STATE_INVALID);

    priv->cancel = TRUE;
}

static void
search_pager_finish (YelpPager   *pager)
{
    debug_print (DB_FUNCTION, "entering\n");
    yelp_pager_set_state (pager, YELP_PAGER_STATE_FINISHED);
}

gboolean
search_pager_process (YelpPager *pager)
{
    debug_print (DB_FUNCTION, "entering\n");

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
#ifdef ENABLE_BEAGLE
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

static void snippet_closed   (BeagleSnippetRequest  *request,
			      SnippetLocation       *snippet_location);
static void snippet_response (BeagleSnippetRequest  *request,
			      BeagleSnippetResponse *response,
			      SnippetLocation       *snippet_location);
static void snippet_error    (BeagleSnippetRequest  *request,
			      GError                *error,
			      SnippetLocation       *snippet_location);

static void
snippet_closed (BeagleSnippetRequest *request, SnippetLocation *snippet_location)
{
    YelpSearchPager *pager = snippet_location->pager;
    
    debug_print (DB_FUNCTION, "entering\n");

    pager->priv->snippet_request_count --;
    check_finished (pager);

    g_signal_handlers_disconnect_by_func (request,
					  G_CALLBACK (snippet_response),
					  snippet_location);
    g_signal_handlers_disconnect_by_func (request,
					  G_CALLBACK (snippet_error),
					  snippet_location);
    g_signal_handlers_disconnect_by_func (request,
					  G_CALLBACK (snippet_closed),
					  snippet_location);

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
	debug_print (DB_DEBUG, "snippet_response empty\n");
	return;
    }
    debug_print (DB_DEBUG, "snippet_response: %s\n", xml);

    xmldoc = g_strdup_printf ("<snippet>%s</snippet>",  xml);
    snippet_doc = xmlParseDoc (BAD_CAST xmldoc);
    g_free (xmldoc);
    if (!snippet_doc)
	return;
    node = xmlDocGetRootElement (snippet_doc);
    xmlUnlinkNode (node);
    xmlAddChild (snippet_location->node, node);
    xmlFreeDoc (snippet_doc);
}

static void
snippet_error (BeagleSnippetRequest *request, GError *error, SnippetLocation *snippet_location)
{
    debug_print (DB_FUNCTION, "entering\n");
}


static void
hits_added_cb (BeagleQuery *query, BeagleHitsAddedResponse *response, YelpSearchPager *pager)
{
    YelpSearchPagerPriv *priv = YELP_SEARCH_PAGER (pager)->priv;

    GSList *hits, *l;

    debug_print (DB_FUNCTION, "hits_added\n");

    hits = beagle_hits_added_response_get_hits (response);

    for (l = hits; l; l = l->next) {
	BeagleHit *hit = l->data;
	beagle_hit_ref (hit);
	debug_print (DB_DEBUG, "%f\n", beagle_hit_get_score (hit));
	g_ptr_array_add (priv->hits, hit);
    }
}

static gboolean
check_lang (const char *lang) {
    int i;
    for (i = 0; langs[i]; i++) {
	if (!strncmp (lang, langs[i], 2)) {
	    debug_print (DB_DEBUG, "%s preferred\n", lang);
	    return TRUE;
	}
    }
    debug_print (DB_DEBUG, "%s not preferred\n", lang);
    return FALSE;
}

static gint
compare_hits (gconstpointer  a,
	      gconstpointer  b)
{
    BeagleHit **hita = (BeagleHit **) a;
    BeagleHit **hitb = (BeagleHit **) b;
    const char *langa, *langb;
    gboolean a_preferred = TRUE, b_preferred = TRUE;

    if (beagle_hit_get_one_property (*hita, "fixme:language", &langa))
	a_preferred = check_lang(langa);
    if (beagle_hit_get_one_property (*hitb, "fixme:language", &langb))
	b_preferred = check_lang(langb);

    if (a_preferred != b_preferred) {
	if (a_preferred)
	    return -1;
	if (b_preferred)
	    return 1;
    }

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

    debug_print (DB_FUNCTION, "entering\n");

    g_ptr_array_sort (priv->hits, compare_hits);

    for (i = 0; i < 10 && i < priv->hits->len; i++) {
	BeagleHit *hit = g_ptr_array_index (priv->hits, i);
	xmlNode *child;
	/* static float score_fake = 0; */
	char *score;
	const char *property;
	BeagleSnippetRequest *request;
	SnippetLocation *snippet_location;

	child = xmlNewTextChild (priv->root, NULL, BAD_CAST "result", NULL);
	xmlSetProp (child, BAD_CAST "uri", BAD_CAST beagle_hit_get_uri (hit));
	xmlSetProp (child, BAD_CAST "parent_uri", 
		    BAD_CAST beagle_hit_get_parent_uri (hit));
	if (beagle_hit_get_one_property (hit, "dc:title", &property))
	    xmlSetProp (child, BAD_CAST "title", BAD_CAST property);
	if (beagle_hit_get_one_property (hit, "fixme:base_title", &property))
	    xmlSetProp (child, BAD_CAST "base_title", BAD_CAST property);

	score = g_strdup_printf ("%f", beagle_hit_get_score (hit));
	debug_print (DB_DEBUG, "%f\n", beagle_hit_get_score (hit));
	/*xmlSetProp (child, BAD_CAST "score", BAD_CAST score);*/
	g_free (score);

	priv->snippet_request_count ++;

	snippet_location = g_new (SnippetLocation, 1);

	snippet_location->pager = pager;
	snippet_location->node = child;

	request = beagle_snippet_request_new ();
	beagle_snippet_request_set_hit (request, hit);
	beagle_snippet_request_set_query (request, query);

	g_signal_connect (request, "response",
			  G_CALLBACK (snippet_response), snippet_location);
	g_signal_connect (request, "error",
			  G_CALLBACK (snippet_error), snippet_location);
	g_signal_connect (request, "closed",
			  G_CALLBACK (snippet_closed), snippet_location);

	debug_print (DB_DEBUG, "Requesting snippet\n");
	beagle_client_send_request_async (beagle_client, BEAGLE_REQUEST (request),
					  NULL);
    }

    g_signal_handlers_disconnect_by_func (query,
					  G_CALLBACK (hits_added_cb),
					  pager);
    g_signal_handlers_disconnect_by_func (query,
					  G_CALLBACK (finished_cb),
					  pager);
    g_object_unref (query);

    g_ptr_array_foreach (priv->hits, (GFunc) beagle_hit_unref, NULL);
    g_ptr_array_free (priv->hits, TRUE);
    priv->hits = NULL;

    check_finished (pager);
}
#endif /* ENABLE_BEAGLE */

static gboolean
search_pager_process_idle (YelpSearchPager *pager)
{
#ifdef ENABLE_BEAGLE
    BeagleQuery    *query;
    GError *error = NULL;
#endif /* ENABLE_BEAGLE */
    YelpSearchPagerPriv *priv = YELP_SEARCH_PAGER (pager)->priv;

    priv->search_doc = xmlNewDoc (BAD_CAST "1.0");
    priv->root = xmlNewNode (NULL, BAD_CAST "search");
    xmlSetProp (priv->root, BAD_CAST "title", BAD_CAST priv->search_terms);
    xmlDocSetRootElement (priv->search_doc, priv->root);

#ifdef ENABLE_BEAGLE
    if (beagle_client != NULL) {
	query = beagle_query_new ();

	beagle_query_set_max_hits (query, 10000);
	beagle_query_add_text (query, priv->search_terms);
	beagle_query_add_source (query, "documentation");

	priv->hits = g_ptr_array_new ();

	g_signal_connect (query, "hits-added",
			  G_CALLBACK (hits_added_cb),
			  pager);
    
	g_signal_connect (query, "finished",
			  G_CALLBACK (finished_cb),
			  pager);
	
	beagle_client_send_request_async (beagle_client, BEAGLE_REQUEST (query), &error);

	if (error) {
	    debug_print (DB_DEBUG, "error: %s\n", error->message);
	}

	g_clear_error (&error);
    } else {
	g_warning ("beagled not running, using basic search support.");
    }
#endif /* ENABLE_BEAGLE */

#ifdef ENABLE_BEAGLE
    if (beagle_client == NULL) {
#endif
	gtk_idle_add ((GtkFunction) slow_search_setup,
		      pager);
#ifdef ENABLE_BEAGLE
    }
#endif

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
    xmlXPathContextPtr results_xpath_ctx = NULL;
    xmlXPathObjectPtr results_xpath = NULL;
    int number_of_results;
    gchar *title = NULL, *text = NULL;

    d (xmlDocFormatDump(stdout, priv->search_doc, 1));

    priv->stylesheet = xsltParseStylesheetFile (BAD_CAST SEARCH_STYLESHEET);
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
			    BAD_CAST "document",
			    BAD_CAST YELP_NAMESPACE,
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

    results_xpath_ctx = xmlXPathNewContext(priv->search_doc);
    results_xpath = xmlXPathEvalExpression(BAD_CAST "/search/result", results_xpath_ctx);
    if (results_xpath && results_xpath->nodesetval && results_xpath->nodesetval->nodeNr) {
    	number_of_results = results_xpath->nodesetval->nodeNr;
    } else {
    	number_of_results = 0;
    }
    xmlXPathFreeObject(results_xpath);
    xmlXPathFreeContext(results_xpath_ctx);
    if (number_of_results == 0) {
    	title = g_strdup_printf( _("No results for \"%s\""), priv->search_terms);
	text = g_strdup(_("Try using different words to describe the problem "
			  "you're having or the topic you want help with."));
    } else {
    	title = g_strdup_printf( _("Search results for \"%s\""), priv->search_terms);
    }
    xmlNewTextChild (priv->root, NULL, BAD_CAST "title", BAD_CAST title);
    g_free(title);

    if (text) {
      xmlNewTextChild (priv->root, NULL, BAD_CAST "text", BAD_CAST text);
      g_free(text);
    }

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

    new_doc = xmlNewDoc (BAD_CAST "1.0");
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
	page->page_id = g_strdup ((gchar *) page_id);
	xmlFree (page_id);
    }
    if (page_title) {
	page->title = g_strdup ((gchar *) page_title);
	xmlFree (page_title);
    } else {
	page->title = g_strdup (_("Help Contents"));
    }
    page->contents = (gchar *) page_buf;

    cur = xmlDocGetRootElement (new_doc);
    for (cur = cur->children; cur; cur = cur->next) {
	if (!xmlStrcmp (cur->name, (xmlChar *) "head")) {
	    for (cur = cur->children; cur; cur = cur->next) {
		if (!xmlStrcmp (cur->name, (xmlChar *) "link")) {
		    xmlChar *rel = xmlGetProp (cur, BAD_CAST "rel");

		    if (!xmlStrcmp (rel, (xmlChar *) "Previous"))
			page->prev_id = (gchar *) xmlGetProp (cur, BAD_CAST "href");
		    else if (!xmlStrcmp (rel, (xmlChar *) "Next"))
			page->next_id = (gchar *) xmlGetProp (cur, BAD_CAST "href");
		    else if (!xmlStrcmp (rel, (xmlChar *) "Top"))
			page->toc_id = (gchar *) xmlGetProp (cur, BAD_CAST "href");

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

static gboolean sk_docomf = FALSE;
static GSList *omf_pending = NULL;

static void
sk_startElement (void *empty, const xmlChar  *name,
		 const xmlChar **attrs)
{
    if (xmlStrEqual((const xmlChar*) name, BAD_CAST "docomf"))
	sk_docomf = TRUE;
}

static void
sk_endElement (void *empty, const xmlChar *name)
{
    if (xmlStrEqual((const xmlChar*) name, BAD_CAST "docomf"))
	sk_docomf = FALSE;
}

static void
sk_characters (void *empty, const xmlChar *ch,
	       int            len)
{
    gchar *omf;
    
    if (sk_docomf) {
	omf = g_strndup ((gchar *) ch, len);
	omf_pending = g_slist_prepend (omf_pending, omf);
    }
}

void s_startElement(void *data,
		  const xmlChar * name,
		  const xmlChar ** attrs)
{
    SearchContainer *c = (SearchContainer *) data;
    
    if (g_str_equal (name, "xi:include") || g_str_equal (name, "include")) {
	gint i=0;
	while (attrs[i]) {
	    if (g_str_equal (attrs[i], "href")) {

		c->components = g_slist_append (c->components,
						g_strconcat (c->base_path,
							     "/",
							     attrs[i+1], 
							     NULL));
		break;
	    }
	    i+=2;
	}
    }
	
    if (attrs) {
	gint i=0;
	while (attrs[i]) {
	    if (g_str_equal (attrs[i], "id")) {
		g_free (c->current_subsection);
		c->current_subsection = g_strdup ((gchar *) attrs[i+1]);
	    }
	    i+=2;
	}
    }
    /* Do we need to grab the title of the document?
     * used in snippets when displaying results from an indexterm etc.
     */
    if (c->search_status != NOT_SEARCHING && g_str_equal (name, "title")) {
	c->grab_text = TRUE;
    }

    /* Are we allowed to search this element? */
    if (c->search_status == NOT_SEARCHING) {
	if (c->html && g_str_equal (name, "html")) {
	    c->search_status = SEARCH_DOC;
	    return;
	}

	if (g_str_equal (name, "title")) {
	    c->search_status = SEARCH_1;
	}
	else if (g_str_equal (name, "indexterm"))
	    c->search_status = SEARCH_1;
	else if (g_str_equal (name, "sect1") ||
		 g_str_equal (name, "section") ||
		 g_str_equal (name, "chapter") ||
		 g_str_equal (name, "body"))
	    c->search_status = SEARCH_DOC;
    } else if (c->search_status == SEARCH_1) {
	c->search_status = SEARCH_CHILD;
    }

    if (c->elem_type) {
	c->elem_stack = g_slist_prepend (c->elem_stack, 
					 g_strdup (c->elem_type));
	g_free (c->elem_type);	
    }

    c->elem_type = g_strdup ((gchar *) name);

    return;
}

void s_endElement(void * data,
		const xmlChar * name)
{
    SearchContainer *c = (SearchContainer *) data;
    
    if (c->search_status == SEARCH_CHILD) {
	c->search_status = SEARCH_1;
    } else if (c->search_status == SEARCH_1) {
	c->search_status = NOT_SEARCHING;
    }

    g_free (c->elem_type);    
    c->elem_type = NULL;

    if (c->elem_stack) {
	GSList *top = c->elem_stack;
	c->elem_type = g_strdup ((gchar *) top->data);
	c->elem_stack = g_slist_delete_link (c->elem_stack, top);
    }
    c->grab_text = FALSE;
    return;
}

void s_characters(void * data,
		  const xmlChar * ch,
		  int len)
{
    SearchContainer *c = (SearchContainer *) data;
    if (c->grab_text) {
	g_free (c->sect_name);
	c->sect_name = g_strndup ((gchar *) ch, len);
    }

    /* Sometimes html docs don't trigger the "startElement" method
     * I don't know why.  Instead, we just search the entire
     * html file, hoping to find something.
     */
    if (c->html && c->search_status != SEARCH_DOC)
	c->search_status = SEARCH_DOC;
    if (c->search_status != NOT_SEARCHING) {
	gchar *tmp = g_utf8_casefold ((gchar *) ch, len);
	gint i = 0;
	gchar *s_term = c->search_term[i];
	while (s_term && c->score_per_word[i] < 1.0) {
	    if (c->stop_word[i] || c->score_per_word[c->dup_of[i]] == 1.0) {
		i++;
		s_term = c->search_term[i];
		continue;
	    }

	    gchar *location = strstr (tmp, s_term);	    
	    if (location) {
		gchar before = *(location-1);
		gchar after = *(location+strlen(s_term));
		gfloat local_score = 0.0;
		gboolean use_text = TRUE;
		if (location == tmp)
		    before = ' ';
		if (strlen(location) == strlen(s_term))
		    after = ' ';

		if ((g_ascii_ispunct (before) || g_ascii_isspace (before)) 
		    && (g_ascii_ispunct (after) || g_ascii_isspace (after))) {
		    if (!c->elem_type) {
			/* Stupid HTML. Treat like its a normal tag */
			local_score = 0.1;
		    } else if (g_str_equal(c->elem_type, "primary")) {
			local_score = 1.0;
			use_text = FALSE;
		    } else if (g_str_equal (c->elem_type, "secondary")) {
			local_score = 0.9;
			use_text = FALSE;
		    } else if (g_str_equal (c->elem_type, "title") || 
			       g_str_equal (c->elem_type, "titleabbrev")) {
			local_score = 0.8;
		    } else {
			local_score = 0.1;
		    }
		    c->score += local_score;
		    c->found_terms[c->dup_of[i]] = TRUE;
		    if (local_score > c->snippet_score) {
			g_free (c->snippet);
			if (use_text) {
			    c->snippet = g_strndup (g_utf8_casefold ((gchar *) ch,
								     len), 
						    len);
			} else {
			    c->snippet = g_strdup (c->sect_name);
			}
			c->result_subsection = g_strdup (c->current_subsection);
			c->snippet_score = local_score;
			c->score_per_word[c->dup_of[i]] = local_score;
		    }
		}
	    }
	    i++;
	    s_term = c->search_term[i];
	}
	g_free (tmp);
    }
    return;
}

void s_declEntity (void *data, const xmlChar *name, int type,
		  const xmlChar *pID, const xmlChar *sID,
		  xmlChar *content)
{
    SearchContainer *c = (SearchContainer *) data;
    if (type == 2) {
	g_hash_table_insert (c->entities, 
			     g_strdup ((gchar *) name), 
			     g_strdup ((gchar *) sID));

    }
    return;
}

xmlEntityPtr s_getEntity (void *data, const xmlChar *name)
{
    SearchContainer *c = (SearchContainer *) data;
    xmlEntityPtr t = xmlGetPredefinedEntity(name);

    if (!t) {
	gchar * lookup = g_hash_table_lookup (c->entities, name);
	if (lookup) {
	    c->components = g_slist_append (c->components,
					    g_strconcat (c->base_path,
							     "/",
							     lookup, NULL));
	}
    }

    return t;

}





static xmlSAXHandler handlers = {
    NULL, NULL, NULL, NULL, NULL,
    s_getEntity,
    s_declEntity, NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL,
    s_startElement, s_endElement, NULL, s_characters,
    NULL, NULL, NULL, NULL, NULL, NULL
};


/* Parse the omfs and build the list of files to be searched */

/* A common bit of code used below.  Chucked in a function for easy */
gchar *
string_append (gchar *current, gchar *new, gchar *suffix)
{
    gchar *ret;
    
    if (suffix) {
	ret = g_strconcat (current, ":", new, suffix, NULL);
    } else {
	ret = g_strconcat (current, ":", new, NULL);
    }
    g_free (current);
    return ret;
}

static gint
build_lists (gchar *search_terms, gchar ***terms, gint **dups, 
	     gboolean ** stops, gint *req)
{
    gchar *ignore_words, *common_prefixes, *common_suffixes;
    gchar **prefixes, **suffixes, **ignore;
    gchar **list_copy;
    gchar **iter, **iter1 = NULL;
    gchar *term_str = NULL;
    gchar *dup_str = NULL;
    gint n_terms = 0, i=-1;
    gint orig_term = 0;
    gint non_stop = 0;


    /* Translators: Do not translate this list exactly.  These are 
     * colon-separated words that aren't useful for choosing search 
     * results; they will be different for each language. Include 
     * pronouns, articles, very common verbs and prepositions, 
     * words from question structures like "tell me about" and 
     * "how do I", and words for functional states like "not", 
     * "work", and "broken".
    */
    ignore_words = g_strdup (_("a:about:an:are:as:at:be:broke:broken:by"
			       ":can:can't:dialog:dialogue:do:doesn't"
			       ":doesnt:don't:dont:explain:for:from:get"
			       ":gets:got:make:makes:not:when:has"
			       ":have:help:how:i:in:is:it:item:me:my:of"
			       ":on:or:tell:that:the:thing:this:to:what"
			       ":where:who:will:with:won't:wont:why:work"
			       ":working:works"));
    /* Translators: This is a list of common prefixes for words.
     * Do not translate this directly.  Instead, use a colon
     * seperated list of word-starts.  In English, an example
     * is re-.  If there is none, please use the term NULL
     * If there is only one, please put a colon after.
     * E.g. if the common prefix is re then the string would be
     * "re:"
     */
    common_prefixes = g_strdup (_("re"));

    /* Translators: This is a list of (guess what?) common suffixes 
     * to words.  Things that may be put at ends of words to slightly 
     * alter their meaning (like -ing and -s in English).  This is a
     * colon seperated list (I like colons).  If there are none,
     * please use the strig NULL.  If there is only 1, please
     * add a colon at the end of the list
     */
    common_suffixes = g_strdup (_("ers:er:ing:es:s:'s"));

    ignore = g_strsplit (ignore_words, ":", -1);
    if (strchr (common_prefixes, ':')) {
	prefixes = g_strsplit (common_prefixes, ":", -1);
    } else {
	prefixes = NULL;
    }
    if (strchr (common_suffixes, ':')) {
	suffixes = g_strsplit (common_suffixes, ":", -1);
    } else {
	suffixes = NULL;
    }

    list_copy = g_strsplit (g_utf8_casefold (g_strstrip (
					  search_terms), -1),
			    " ", -1);
    
    for (iter = list_copy; *iter != NULL; iter++) {
	gboolean ignoring = FALSE;
	
	if (g_str_has_suffix (*iter, "?")) {
	    gchar *tmp;
	    tmp = g_strndup (*iter, strlen (*iter) - 1);
	    g_free (*iter);
	    *iter = g_strdup (tmp);
	    g_free (tmp);
	}
	if (!term_str) {
	    term_str = g_strdup (*iter);
	} else {
	    term_str = string_append (term_str, *iter, NULL);
	}
	
	for (iter1 = ignore; *iter1; iter1++) {
	    if (g_str_equal (*iter, *iter1)) {
		ignoring = TRUE;
		break;
	    }
	}
	if (ignoring) {
	    if (!dup_str) {
		dup_str = g_strdup ("I");
	    } else {
		dup_str = string_append (dup_str, "I", NULL);
	    }
	    continue;
	}
	non_stop++;

	if (!dup_str) {
	    dup_str = g_strdup ("O");
	} else {
	    dup_str = string_append (dup_str, "O", NULL);
	}
	(*req)++;
	if (prefixes) {
	    for (iter1 = prefixes; *iter1; iter1++) {
		if (g_str_has_prefix (*iter, *iter1)) {
		    term_str = string_append (term_str, 
					      (*iter+strlen(*iter1)), NULL);
		} else {
		    term_str = string_append (term_str, *iter, *iter1);
		}
		dup_str = string_append (dup_str, "D", NULL);
	    }
	}
	if (suffixes) {
	    for (iter1 = suffixes; *iter1; iter1++) {
		if (g_str_has_suffix (*iter, *iter1)) {
		    gchar *tmp;
		    tmp = g_strndup (*iter, (strlen(*iter)-strlen(*iter1)));
		    term_str = string_append (term_str, tmp, NULL);
		    g_free (tmp);
		} else {
		    term_str = string_append (term_str, *iter, *iter1);   
		}
		dup_str = string_append (dup_str, "D", NULL);
	    }
	}
    }
    g_strfreev (list_copy);
    *terms = g_strsplit (term_str, ":", -1);
    n_terms = g_strv_length (*terms);
    (*dups) = g_new0 (gint, n_terms);
    (*stops) = g_new0 (gboolean, n_terms);
    list_copy = g_strsplit (dup_str, ":", -1);
    
    for (iter = *terms; *iter; iter++) {
	i++;
	if (g_str_equal (list_copy[i], "O")) {
	    orig_term = i;
	}
	(*dups)[i] = orig_term;
	
	for (iter1 = ignore; *iter1; iter1++) {
	    if (non_stop > 0 && g_str_equal (*iter, *iter1)) {
		(*stops)[i] = TRUE;
		(*dups)[i] = -2;
		break;
	    }
	}
    }

    /* Clean up all those pesky strings */
    g_free (ignore_words);
    g_free (common_prefixes);
    g_free (common_suffixes);
    g_free (term_str);
    g_free (dup_str);
    g_strfreev (prefixes);
    g_strfreev (suffixes);
    g_strfreev (ignore);
    g_strfreev (list_copy);

    return n_terms;
}


static gboolean
slow_search_setup (YelpSearchPager *pager)
{
    gchar  *content_list;
    gchar  *stderr_str;
    gchar  *lang;
    gchar  *command;

    gchar **terms_list = NULL;
    gint   *dup_list = NULL;
    gboolean *stop_list = NULL;
    gint      terms_number = 0;
    gint required_no = 0;

    static xmlSAXHandler sk_sax_handler = { 0, };
    xmlParserCtxtPtr parser;
    if (langs && langs[0])
	lang = (gchar *) langs[0];
    else
	lang = "C";
    
    command = g_strconcat("scrollkeeper-get-content-list ", lang, NULL);
    
    if (g_spawn_command_line_sync (command, &content_list, &stderr_str, NULL, NULL)) {
	if (!sk_sax_handler.startElement) {
	    sk_sax_handler.startElement = sk_startElement;
	    sk_sax_handler.endElement   = sk_endElement;
	    sk_sax_handler.characters   = sk_characters;
	    sk_sax_handler.initialized  = TRUE;
	}
	content_list = g_strstrip (content_list);
	xmlSAXUserParseFile (&sk_sax_handler, NULL, content_list);
    }
    
    parser = xmlNewParserCtxt ();

    g_free (content_list);
    g_free (stderr_str);
    g_free (command);


    terms_number = build_lists (pager->priv->search_terms,&terms_list, 
				&dup_list, &stop_list, 
				&required_no);


    while (omf_pending) {
	GSList  *first = NULL;
	gchar   *file  = NULL;
	xmlDocPtr          omf_doc    = NULL;
	xmlXPathContextPtr omf_xpath  = NULL;
	xmlXPathObjectPtr  omf_url    = NULL;
	xmlXPathObjectPtr  omf_title  = NULL;
	xmlXPathObjectPtr  omf_mime   = NULL;
	xmlXPathObjectPtr  omf_desc   = NULL;

	SearchContainer *container;
	gchar *ptr;
	gchar *path;
	gchar *fname;
	gchar *realfname;
	gchar *mime_type;
	int i = 0;

	first = omf_pending;
	omf_pending = g_slist_remove_link (omf_pending, first);
	file = (gchar *) first->data;


	omf_doc = xmlCtxtReadFile (parser, (const char *) file, NULL,
			       XML_PARSE_NOBLANKS | XML_PARSE_NOCDATA  |
			       XML_PARSE_NOENT    | XML_PARSE_NOERROR  |
			       XML_PARSE_NONET    );

	if (!omf_doc) {
	    g_warning (_("Could not load the OMF file '%s'."), file);
	    continue;
	}

	omf_xpath = xmlXPathNewContext (omf_doc);
	omf_url =
	    xmlXPathEvalExpression (BAD_CAST 
				    "string(/omf/resource/identifier/@url)", 
				    omf_xpath);
	omf_title =
	    xmlXPathEvalExpression (BAD_CAST 
				    "string(/omf/resource/title)", 
				    omf_xpath);
	omf_mime = 
	    xmlXPathEvalExpression (BAD_CAST
				    "string(/omf/resource/format/@mime)",
				    omf_xpath);
	omf_desc = 
	    xmlXPathEvalExpression (BAD_CAST
				    "string(/omf/resource/description)",
				    omf_xpath);

	mime_type = g_strdup ((gchar *) omf_mime->stringval);

	fname = g_strdup ((gchar *) omf_url->stringval);
	if (g_str_has_prefix (fname, "file:")) {
	    realfname = &fname[5];
	} else {
	    realfname = fname;
	}    

	if (!g_file_test (realfname, G_FILE_TEST_EXISTS)) {
	    continue;
	}

	container = g_new0 (SearchContainer, 1);
	
	container->base_filename = g_strdup (realfname);
	container->entities = g_hash_table_new (g_str_hash, g_str_equal);
	container->doc_title = g_strdup ((gchar *) omf_title->stringval);
	container->score=0;
	container->html = FALSE;
	container->default_snippet = g_strdup ((gchar *) omf_desc->stringval);

	ptr = g_strrstr (container->base_filename, "/");

	path = g_strndup (container->base_filename, 
			    ptr - container->base_filename);

	/* BEGIN HTML special block */
	if (g_str_equal (mime_type, "text/html")) {
	    GDir *dir;
	    gchar *filename;
	    container->html = TRUE;
	    ptr++;
	    
	    dir = g_dir_open (path, 0, NULL);
       
	    while ((filename = (gchar *) g_dir_read_name (dir))) {
		if ((g_str_has_suffix (filename, ".html") ||
		     g_str_has_suffix (filename, ".htm")) &&
		     !g_str_equal (filename, ptr)) {
		    container->components = 
			g_slist_append (container->components,
					g_strconcat (path, "/", filename,
						     NULL));

		}
	    }
	    /* END HTML special blcok */
	}

	container->base_path = g_strdup (path);

	container->required_words = required_no;
	container->grab_text = FALSE;
	container->sect_name = NULL;

	container->search_term = g_strdupv (terms_list);
	container->stop_word = g_new0 (gboolean, terms_number);
	container->dup_of = g_new0 (gint, terms_number);
	container->found_terms = g_new0 (gboolean, terms_number);
	container->score_per_word = g_new0 (gfloat, terms_number);
	container->found_terms = g_new0 (gboolean, terms_number);

	container->search_status = NOT_SEARCHING;
	container->snippet_score = 0;

	for (i=0; i< terms_number; i++) {
	    container->stop_word[i] = stop_list[i];
	    container->dup_of[i] = dup_list[i];
	}

	pager->priv->pending_searches = 
	    g_slist_prepend (pager->priv->pending_searches, container);
	
	g_free (fname);
	g_free (path);
	if (omf_url)
	    xmlXPathFreeObject (omf_url);
	if (omf_title)
	    xmlXPathFreeObject (omf_title);
	if (omf_xpath)
	    xmlXPathFreeContext (omf_xpath);
	if (omf_doc)
	    xmlFreeDoc (omf_doc);

    }

    gtk_idle_add ((GtkFunction) slow_search_process,
		  pager);
    if (parser)
	xmlFreeParserCtxt (parser);

    return FALSE;

}

static void
search_free_container (SearchContainer *c)
{
    g_strfreev (c->search_term);
    g_free (c->dup_of);
    g_free (c->found_terms);
    g_free (c->stop_word);
    g_free (c->score_per_word);
    g_free (c->top_element);
    g_free (c->elem_type);
    g_free (c->sect_name);
    g_free (c->default_snippet);
    g_free (c->current_subsection);
    g_free (c->result_subsection);
    g_free (c->doc_title);
    g_free (c->base_path);
    g_free (c->base_filename);
    g_free (c->snippet);
    g_hash_table_destroy (c->entities);
    g_free (c);
}


static gboolean 
slow_search_process (YelpSearchPager *pager)
{
    SearchContainer *c;
    GSList *first = pager->priv->pending_searches;
    gint i, j=0;

    pager->priv->pending_searches = 
	g_slist_remove_link (pager->priv->pending_searches, first);

    if (first == NULL) {
	gtk_idle_add_priority (G_PRIORITY_LOW,
			       (GtkFunction) process_xslt,
			       pager);
	return FALSE;
    }

    c = (SearchContainer *) first->data;

    xmlSAXUserParseFile (&handlers, c, c->base_filename);
    for (i=0; i< g_strv_length (c->search_term); ++i) {
	if (c->found_terms[i]) {
	    j++;
	}
    }
    if (j >= c->required_words) {
	search_parse_result (pager, c);
    } else while (c->components) {
	GSList *next = c->components;
	c->components = g_slist_remove_link (c->components, next);
	c->search_status = NOT_SEARCHING;
	xmlSAXUserParseFile (&handlers, c, (gchar *) next->data);
	j = 0;
	for (i=0; i< g_strv_length (c->search_term); ++i) {
	    if (c->found_terms[i])
		j++;
	}
	if (j >= c->required_words) {
	    search_parse_result (pager, c);
	    break;
	}
    }

    if (pager->priv->pending_searches) {
	search_free_container (c);
	return TRUE;
    }
    else {
#ifdef ENABLE_MAN
	search_process_man (pager, c->search_term);
#endif
#ifdef ENABLE_INFO
	search_process_info (pager, c->search_term);
#endif
	search_free_container (c);

	gtk_idle_add_priority (G_PRIORITY_LOW,
			       (GtkFunction) process_xslt,
			       pager);
	return FALSE;
    }
}

gchar *
search_clean_snippet (gchar *snippet, gchar **terms)
{    
    /* This is probably what you want to change */
    gint len_before_term = 47;
    gint len_after_term = 47;
    gchar **iteration;
    gboolean am_cutting = FALSE;
    gchar *result = NULL;
    gboolean found_terms = FALSE;


    if (!snippet)
	return NULL;

    if (strlen(snippet) > (len_before_term+len_after_term)) {
	am_cutting = TRUE;
    }
    result = g_strdup (snippet);

    for (iteration = terms; *iteration; iteration++) {
	gchar *before, *after, *tmp;
	gchar *str;
	gchar before_c, after_c;
	gint count = 0;

	while ((str = strstr (result, (*iteration)))) {
	    gboolean breaking = FALSE;
	    gint i;
	    for (i=0; i< count; i++) {
		str++;
		str = strstr (str, (*iteration));
		if (!str) {
		    breaking = TRUE;
		    break;
		}
	    }
	    count++;
	    if (breaking)
		break;

	    before_c = *(str-1);
	    after_c = *(str+strlen(*iteration));

	    if (g_ascii_isalpha (before_c) || g_ascii_isalpha (after_c)) {
		continue;
	    }
	    
	    tmp = g_strndup (result, (str-result));
	    /* If we have to chop the snippet down to size, here is the
	     * place to do it.  Only the first time through though
	     */
	    if (am_cutting && !found_terms && strlen (tmp) > len_before_term) {
		gchar *tmp1;
		gchar *tmp2;
		gint cut_by;

		tmp1 = tmp;
		cut_by = strlen(tmp) - len_before_term;

		tmp1 += cut_by;
		tmp2 = g_strdup (tmp1);
		g_free (tmp);
		tmp = g_strconcat ("...",tmp2, NULL);
		g_free (tmp2);		
	    }

	    before = g_strconcat (tmp, "<em>", NULL);
	    g_free (tmp);
	    
	    str += strlen (*iteration);

	    if (am_cutting && !found_terms && strlen (str) > len_after_term) {
		gchar *tmp1;

		tmp1 = g_strndup (str, len_after_term);
		tmp = g_strconcat (tmp1, "...", NULL);
		g_free (tmp1);
	    } else {
		tmp = g_strdup (str);
	    }
	    
	    after = g_strconcat ((*iteration), "</em>", tmp, NULL);


	    
	    g_free (result);
	    result = g_strconcat (before, after, NULL);
	    found_terms = TRUE;
	}
    }    
    return result;
}

void
search_parse_result (YelpSearchPager *pager, SearchContainer *c)
{    
    xmlNode *child;
    gchar *new_uri;
    xmlDoc *snippet_doc;
    xmlNode *node;
    char *xmldoc;

    new_uri = g_strconcat (c->base_filename, "#", c->result_subsection, 
			   NULL);
    child = xmlNewTextChild (pager->priv->root, NULL, 
			     BAD_CAST "result", NULL);
    xmlSetProp (child, BAD_CAST "uri", BAD_CAST new_uri);
    xmlSetProp (child, BAD_CAST "title", BAD_CAST g_strstrip (c->doc_title));
    xmlSetProp (child, BAD_CAST "score", 
		BAD_CAST g_strdup_printf ("%f", c->score));
    /* Fix up the snippet to show the break_term in bold */
    if (!c->snippet)
	c->snippet = g_strdup (c->default_snippet);
    xmldoc = g_strdup_printf ("<snippet>%s</snippet>",  
			      search_clean_snippet (c->snippet, c->search_term));
    snippet_doc = xmlParseDoc (BAD_CAST xmldoc);
    g_free (xmldoc);
    
    if (!snippet_doc)
	return;
    
    node = xmlDocGetRootElement (snippet_doc);
    xmlUnlinkNode (node);
    xmlAddChild (child, node);
    xmlFreeDoc (snippet_doc);
}

void
process_man_result (YelpSearchPager *pager, gchar *result, gchar **terms)
{
    gchar ** split = g_strsplit (result, "\n", -1);
    gint i;

    for (i=0;split[i];i++) {
	gchar ** line = g_strsplit (split[i], "(", 2);
	gchar *filename = NULL;
	gchar *desc = NULL;
	xmlNode *child;
	gchar *tmp = NULL;
	gchar *after = NULL;
	/*gchar *before = NULL;*/
	gchar *title = NULL;
	/*gint i;*/

	if (line == NULL || line[0] == NULL)
	    continue;

	title = g_strdup (g_strstrip (line[0]));
	after = strstr (line[1], ")");

	tmp = g_strndup (line[1], after-line[1]);
	
	filename = g_strconcat ("man:", title, "(", tmp,")", NULL);

	after++;
	g_free (tmp);
	
	tmp = g_strdup (g_strchug (after));
	after = tmp; after++;
	desc = g_strdup (g_strchug (after));

	child = xmlNewTextChild (pager->priv->root, NULL, 
				 BAD_CAST "result", NULL);
	xmlSetProp (child, BAD_CAST "uri", BAD_CAST filename);
	xmlSetProp (child, BAD_CAST "title", 
		    BAD_CAST g_strconcat (title, 
					  " manual page", NULL));
	
	xmlNewChild (child, NULL, BAD_CAST "snippet",
		     BAD_CAST desc);
	xmlNewChild (child, NULL, BAD_CAST "score",
		     BAD_CAST "0.1");
	g_free (tmp);
	g_strfreev (line);
    }

}

void
process_info_result (YelpSearchPager *pager, gchar *result, gchar **terms)
{
    gchar ** split = NULL;
    gint i;

    split = g_strsplit (result, "\n", -1);
    if (split == NULL)
	return;

    for (i=0;split[i];i++) {
	gchar ** line = NULL;
	gchar *filename = NULL;
	gchar *desc = NULL;
	gchar *title = NULL;
	xmlNode *child;
	gchar *tmp;
	gchar *tmp1;
	gchar *file_name;
	
	line = g_strsplit (split[i], "--", 3); 
	if (g_strv_length (line) != 2) {
	    g_strfreev (line);
	    continue;
	}

	/* First is the filename
	 * We gotta do some fiddling to get the actual filename
	 * we can use
	*/
	tmp = g_strdup (g_strchomp (line[0]));
	tmp++;
	tmp1 = strstr (tmp, "\"");
	if (!tmp1) {
	    g_strfreev (line);
	    g_free (tmp);
	    continue;
	}	    
	file_name = g_strndup (tmp, tmp1-tmp);
	tmp++;
	tmp1 = strstr (tmp, ")");
	if (tmp1)
	    title = g_strndup (tmp, tmp1-tmp);
	else {
	    title = g_strdup (++file_name);
	    --file_name;
	}
	tmp--;
	tmp--;
	filename = g_strconcat ("info:", file_name, NULL);
	g_free (tmp);
	g_free (file_name);

	/* Then the description */
	desc = g_strdup (g_strchug (line[1]));

	/* Now we add the result to the page */
	child = xmlNewTextChild (pager->priv->root, NULL, 
				 BAD_CAST "result", NULL);
	xmlSetProp (child, BAD_CAST "uri", BAD_CAST filename);
	xmlSetProp (child, BAD_CAST "title", 
		    BAD_CAST g_strconcat (title, 
					  " info page", NULL));
	
	xmlNewChild (child, NULL, BAD_CAST "snippet",
		     BAD_CAST desc);
	xmlNewChild (child, NULL, BAD_CAST "score",
		     BAD_CAST "0.05");
	g_strfreev (line);
	g_free (title);
    }

}

void
search_process_man (YelpSearchPager *pager, gchar **terms)
{
    gchar *command;
    gchar *stdout_str = NULL;
    gint exit_code;
    gchar *tmp = NULL;
    gchar *search = NULL;

    tmp = g_strescape (pager->priv->search_terms, NULL);
    tmp = g_strdelimit (tmp, "\'", '\'');
    search = g_strconcat ("\"",tmp,"\"", NULL);

    command = g_strconcat("apropos ", search, NULL);
    
    if (g_spawn_command_line_sync (command, &stdout_str, NULL, 
				   &exit_code, NULL) && exit_code == 0) {
	process_man_result (pager, stdout_str, terms);
	
    }
    g_free (tmp);
    g_free (search);
    g_free (stdout_str);
    g_free (command);

    return;
}

void
search_process_info (YelpSearchPager *pager, gchar **terms)
{
    gchar *command;
    gchar *stdout_str = NULL;
    gchar *stderr_str = NULL;
    gchar *tmp;
    gint exit_code;
    
    gchar *search = NULL;

    tmp = g_strescape (pager->priv->search_terms, NULL);
    tmp = g_strdelimit (tmp, "\'", '\'');
    search = g_strconcat ("\"",tmp,"\"", NULL);
    command = g_strconcat("info --apropos ", search, NULL);
    
    if (g_spawn_command_line_sync (command, &stdout_str, &stderr_str, 
				   &exit_code, NULL) && 
	stdout_str != NULL) {
	process_info_result (pager, stdout_str, terms);	    
    }
    g_free (tmp);
    g_free (stdout_str);
    g_free (stderr_str);
    g_free (command);

    return;
}
