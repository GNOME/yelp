/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2007 Shaun McCance  <shaunm@gnome.org>
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
 * Author: Don Scorgie    <Don@Scorgie.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xinclude.h>

#include "yelp-error.h"
#include "yelp-db-print.h"
#include "yelp-settings.h"
#include "yelp-transform.h"
#include "yelp-debug.h"

#define STYLESHEET DATADIR"/yelp/xslt/db2html.xsl"

#define YELP_DBPRINT_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_DBPRINT, YelpDbprintPriv))

typedef enum {
    DBPRINT_STATE_BLANK,   /* Brand new, run transform as needed */
    DBPRINT_STATE_PARSING, /* Parsing/transforming document, please wait */
    DBPRINT_STATE_PARSED,  /* All done, if we ain't got it, it ain't here */
    DBPRINT_STATE_STOP     /* Stop everything now, object to be disposed */
} DbprintState;

struct _YelpDbprintPriv {
    gchar         *filename;
    DbprintState   state;

    GMutex        *mutex;
    GThread       *thread;

    gboolean       process_running;
    gboolean       transform_running;

    YelpTransform *transform;

    xmlDocPtr     xmldoc;
    xmlNodePtr    xmlcur;
    gint          max_depth;
    gint          cur_depth;
    gchar        *cur_page_id;
    gchar        *cur_prev_id;
};


static void           dbprint_class_init      (YelpDbprintClass    *klass);
static void           dbprint_init            (YelpDbprint         *dbprint);
static void           dbprint_try_dispose     (GObject             *object);
static void           dbprint_dispose         (GObject             *object);

/* YelpDocument */
static void           dbprint_request         (YelpDocument        *document,
					       gint                 req_id,
					       gboolean             handled,
					       gchar               *page_id,
					       YelpDocumentFunc     func,
					       gpointer             user_data);

/* YelpTransform */
static void           transform_func          (YelpTransform       *transform,
					       YelpTransformSignal  signal,
					       gpointer             func_data,
					       YelpDbprint         *dbprint);
static void           transform_page_func     (YelpTransform       *transform,
					       gchar               *page_id,
					       YelpDbprint         *dbprint);
static void           transform_final_func    (YelpTransform       *transform,
					       YelpDbprint         *dbprint);

/* Threaded */
static void           dbprint_process         (YelpDbprint         *dbprint);

#if 0
/* Walker */
static void           dbprint_walk            (YelpDbprint         *dbprint);
static gboolean       dbprint_walk_chunkQ     (YelpDbprint         *dbprint);
static gboolean       dbprint_walk_divisionQ  (YelpDbprint         *dbprint);
static gchar *        dbprint_walk_get_title  (YelpDbprint         *dbprint);
#endif

static YelpDocumentClass *parent_class;

GType
yelp_dbprint_get_type (void)
{
    static GType type = 0;
    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpDbprintClass),
	    NULL, NULL,
	    (GClassInitFunc) dbprint_class_init,
	    NULL, NULL,
	    sizeof (YelpDbprint),
	    0,
	    (GInstanceInitFunc) dbprint_init,
	};
	type = g_type_register_static (YELP_TYPE_DOCUMENT,
				       "YelpDbprint", 
				       &info, 0);
    }
    return type;
}

static void
dbprint_class_init (YelpDbprintClass *klass)
{
    GObjectClass      *object_class   = G_OBJECT_CLASS (klass);
    YelpDocumentClass *document_class = YELP_DOCUMENT_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = dbprint_try_dispose;

    document_class->request = dbprint_request;
    document_class->cancel = NULL;

    g_type_class_add_private (klass, sizeof (YelpDbprintPriv));
}

static void
dbprint_init (YelpDbprint *dbprint)
{
    YelpDbprintPriv *priv;
    priv = dbprint->priv = YELP_DBPRINT_GET_PRIVATE (dbprint);


    priv->state = DBPRINT_STATE_BLANK;

    priv->mutex = g_mutex_new ();
}

static void
dbprint_try_dispose (GObject *object)
{
    YelpDbprintPriv *priv;

    g_assert (object != NULL && YELP_IS_DBPRINT (object));

    priv = YELP_DBPRINT (object)->priv;

    g_mutex_lock (priv->mutex);
    if (priv->process_running || priv->transform_running) {
	priv->state = DBPRINT_STATE_STOP;
	g_idle_add ((GSourceFunc) dbprint_try_dispose, object);
	g_mutex_unlock (priv->mutex);
    } else {
	g_mutex_unlock (priv->mutex);
	dbprint_dispose (object);
    }
}

static void
dbprint_dispose (GObject *object)
{
    YelpDbprint *dbprint = YELP_DBPRINT (object);

    g_free (dbprint->priv->filename);

    if (dbprint->priv->xmldoc)
	xmlFreeDoc (dbprint->priv->xmldoc);

    g_free (dbprint->priv->cur_page_id);
    g_free (dbprint->priv->cur_prev_id);

    g_mutex_free (dbprint->priv->mutex);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

YelpDocument *
yelp_dbprint_new (gchar *filename)
{
    YelpDbprint *dbprint;

    g_return_val_if_fail (filename != NULL, NULL);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  filename = \"%s\"\n", filename);

    dbprint = (YelpDbprint *) g_object_new (YELP_TYPE_DBPRINT, NULL);
    dbprint->priv->filename = g_strdup (filename);

    yelp_document_add_page_id (YELP_DOCUMENT (dbprint), "x-yelp-titlepage", "x-yelp-titlepage");

    return (YelpDocument *) dbprint;
}


/******************************************************************************/
/** YelpDocument **************************************************************/

static void
dbprint_request (YelpDocument     *document,
		 gint              req_id,
		 gboolean          handled,
		 gchar            *page_id,
		 YelpDocumentFunc  func,
		 gpointer          user_data)
{
    YelpDbprint *dbprint;
    YelpDbprintPriv *priv;
    YelpError *error;

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  req_id  = %i\n", req_id);
    debug_print (DB_ARG, "  page_id = \"%s\"\n", page_id);
    g_assert (document != NULL && YELP_IS_DBPRINT (document));

    if (handled) {
	return;
    }

    dbprint = YELP_DBPRINT (document);
    priv = dbprint->priv;

    g_mutex_lock (priv->mutex);

    switch (priv->state) {
    case DBPRINT_STATE_BLANK:
	priv->state = DBPRINT_STATE_PARSING;
	priv->process_running = TRUE;
	priv->thread = g_thread_create ((GThreadFunc) dbprint_process,
					dbprint, FALSE, NULL);
	break;
    case DBPRINT_STATE_PARSING:
	break;
    case DBPRINT_STATE_PARSED:
    case DBPRINT_STATE_STOP:
	error = yelp_error_new (_("Page not found"),
				_("The page %s was not found in the document %s."),
				page_id, priv->filename);
	yelp_document_error_request (document, req_id, error);
	break;
    }

    g_mutex_unlock (priv->mutex);
}

/******************************************************************************/
/** YelpTransform *************************************************************/

static void
transform_func (YelpTransform       *transform,
		YelpTransformSignal  signal,
		gpointer             func_data,
		YelpDbprint         *dbprint)
{
    YelpDbprintPriv *priv;
    debug_print (DB_FUNCTION, "entering\n");

    g_assert (dbprint != NULL && YELP_IS_DBPRINT (dbprint));

    priv = dbprint->priv;

    g_assert (transform == priv->transform);

    if (priv->state == DBPRINT_STATE_STOP) {
	switch (signal) {
	case YELP_TRANSFORM_CHUNK:
	    g_free (func_data);
	    break;
	case YELP_TRANSFORM_ERROR:
	    yelp_error_free ((YelpError *) func_data);
	    break;
	case YELP_TRANSFORM_FINAL:
	    break;
	}
	yelp_transform_release (transform);
	priv->transform = NULL;
	priv->transform_running = FALSE;
	return;
    }

    switch (signal) {
    case YELP_TRANSFORM_CHUNK:
	transform_page_func (transform, (gchar *) func_data, dbprint);
	break;
    case YELP_TRANSFORM_ERROR:
	yelp_document_error_pending (YELP_DOCUMENT (dbprint), (YelpError *) func_data);
	yelp_transform_release (transform);
	priv->transform = NULL;
	priv->transform_running = FALSE;
	break;
    case YELP_TRANSFORM_FINAL:
	transform_final_func (transform, dbprint);
	break;
    }
}

static void
transform_page_func (YelpTransform *transform,
		     gchar         *page_id,
		     YelpDbprint   *dbprint)
{
    YelpDbprintPriv *priv;
    gchar *content;

    debug_print (DB_FUNCTION, "entering\n");

    priv = dbprint->priv;
    g_mutex_lock (priv->mutex);

    content = yelp_transform_eat_chunk (transform, page_id);
    yelp_document_add_page (YELP_DOCUMENT (dbprint), page_id, content);

    g_free (page_id);

    g_mutex_unlock (priv->mutex);
}

static void
transform_final_func (YelpTransform *transform, YelpDbprint *dbprint)
{
    YelpError *error;
    YelpDbprintPriv *priv = dbprint->priv;

    debug_print (DB_FUNCTION, "entering\n");

    g_mutex_lock (priv->mutex);

    error = yelp_error_new (_("Page not found"),
			    _("The requested page was not found in the document %s."),
			    priv->filename);
    yelp_document_error_pending (YELP_DOCUMENT (dbprint), error);

    yelp_transform_release (transform);
    priv->transform = NULL;
    priv->transform_running = FALSE;

    if (priv->xmldoc)
	xmlFreeDoc (priv->xmldoc);
    priv->xmldoc = NULL;

    g_mutex_unlock (priv->mutex);
}


/******************************************************************************/
/** Threaded ******************************************************************/

static void
dbprint_process (YelpDbprint *dbprint)
{
    YelpDbprintPriv *priv;
    xmlDocPtr xmldoc = NULL;
    xmlChar *id = NULL;
    YelpError *error = NULL;
    xmlParserCtxtPtr parserCtxt = NULL;
    YelpDocument *document;
    gchar **params;
    gint params_i = 0;
    gint params_max = 20;


    debug_print (DB_FUNCTION, "entering\n");

    g_assert (dbprint != NULL && YELP_IS_DBPRINT (dbprint));
    g_object_ref (dbprint);
    priv = dbprint->priv;
    document = YELP_DOCUMENT (dbprint);

    if (!g_file_test (priv->filename, G_FILE_TEST_IS_REGULAR)) {
	error = yelp_error_new (_("File not found"),
				_("The file ‘%s’ does not exist."),
				priv->filename);
	yelp_document_error_pending (document, error);
	goto done;
    }

    parserCtxt = xmlNewParserCtxt ();
    xmldoc = xmlCtxtReadFile (parserCtxt,
			      (const char *) priv->filename, NULL,
			      XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
			      XML_PARSE_NOENT   | XML_PARSE_NONET   );

    if (xmldoc == NULL) {
	error = yelp_error_new (_("Could not parse file"),
				_("The file ‘%s’ could not be parsed because it is"
				  " not a well-formed XML document."),
				priv->filename);
	yelp_document_error_pending (document, error);
	goto done;
    }

    if (xmlXIncludeProcessFlags (xmldoc,
				 XML_PARSE_DTDLOAD | XML_PARSE_NOCDATA |
				 XML_PARSE_NOENT   | XML_PARSE_NONET   )
	< 0) {
	error = yelp_error_new (_("Could not parse file"),
				_("The file ‘%s’ could not be parsed because"
				  " one or more of its included files is not"
				  " a well-formed XML document."),
				priv->filename);
	yelp_document_error_pending (document, error);
	goto done;
    }

    g_mutex_lock (priv->mutex);
    if (!xmlStrcmp (xmlDocGetRootElement (xmldoc)->name, BAD_CAST "book"))
	priv->max_depth = 2;
    else
	priv->max_depth = 1;

    priv->xmldoc = xmldoc;
    priv->xmlcur = xmlDocGetRootElement (xmldoc);

    id = xmlGetProp (priv->xmlcur, BAD_CAST "id");
    if (id) {
	yelp_document_set_root_id (document, (gchar *) id);
	yelp_document_add_page_id (document, "x-yelp-index", (gchar *) id);
	yelp_document_add_prev_id (document, (gchar *) id, "x-yelp-titlepage");
	yelp_document_add_next_id (document, "x-yelp-titlepage", (gchar *) id);
    }
    else {
	yelp_document_set_root_id (document, "x-yelp-index");
	yelp_document_add_page_id (document, "x-yelp-index", "x-yelp-index");
	yelp_document_add_prev_id (document, "x-yelp-index", "x-yelp-titlepage");
	yelp_document_add_next_id (document, "x-yelp-titlepage", "x-yelp-index");
	/* add the id attribute to the root element with value "index"
	 * so when we try to load the document later, it doesn't fail */
	xmlNewProp (priv->xmlcur, BAD_CAST "id", BAD_CAST "x-yelp-index");
    }
    g_mutex_unlock (priv->mutex);

    g_mutex_lock (priv->mutex);
    if (priv->state == DBPRINT_STATE_STOP) {
	g_mutex_unlock (priv->mutex);
	goto done;
    }
    g_mutex_unlock (priv->mutex);

    g_mutex_lock (priv->mutex);
    if (priv->state == DBPRINT_STATE_STOP) {
	g_mutex_unlock (priv->mutex);
	goto done;
    }
    priv->transform = yelp_transform_new (STYLESHEET,
					  (YelpTransformFunc) transform_func,
					  dbprint);
    priv->transform_running = TRUE;
    /* FIXME: we probably need to set our own params */

    params = g_new0 (gchar *, params_max);

    yelp_settings_params (&params, &params_i, &params_max);

    if ((params_i + 10) >= params_max - 1) {
	params_max += 20;
	params = g_renew (gchar *, params, params_max);
    }
    params[params_i++] = "db.chunk.extension";
    params[params_i++] = g_strdup ("\"\"");
    params[params_i++] = "db.chunk.info_basename";
    params[params_i++] = g_strdup ("\"index\"");
    params[params_i++] = "db.chunk.max_depth";
    params[params_i++] = g_strdup ("0");
    params[params_i++] = "db2html.navbar.top";
    params[params_i++] = g_strdup ("0");
    params[params_i++] = "db2html.navbar.bottom";
    params[params_i++] = g_strdup ("0");
    params[params_i++] = "db2html.sidenav";
    params[params_i++] = g_strdup ("0");

    params[params_i] = NULL;

    yelp_transform_start (priv->transform,
			  priv->xmldoc,
			  params);
    g_strfreev (params);
    g_mutex_unlock (priv->mutex);

 done:
    if (id)
	xmlFree (id);
    if (parserCtxt)
	xmlFreeParserCtxt (parserCtxt);

    priv->process_running = FALSE;
    g_object_unref (dbprint);
}

#if 0
/******************************************************************************/
/** Walker ********************************************************************/

static void
dbprint_walk (YelpDbprint *dbprint)
{
    static       gint autoid = 0;
    gchar        autoidstr[20];
    xmlChar     *id = NULL;
    xmlChar     *title = NULL;
    gchar       *old_page_id = NULL;
    xmlNodePtr   cur, old_cur;
    GtkTreeIter  iter;
    GtkTreeIter *old_iter = NULL;
    YelpDbprintPriv *priv = dbprint->priv;
    YelpDocument *document = YELP_DOCUMENT (dbprint);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_DEBUG, "  priv->xmlcur->name: %s\n", priv->xmlcur->name);

    /* check for the yelp:chunk-depth or db.chunk.max_depth processing
     * instruction and set the max chunk depth accordingly.
     */
    if (priv->cur_depth == 0)
	for (cur = priv->xmlcur; cur; cur = cur->prev)
	    if (cur->type == XML_PI_NODE)
		if (!xmlStrcmp (cur->name, (const xmlChar *) "yelp:chunk-depth") ||
		    !xmlStrcmp (cur->name, (const xmlChar *) "db.chunk.max_depth")) {
		    gint max = atoi ((gchar *) cur->content);
		    if (max)
			priv->max_depth = max;
		    break;
		}

    id = xmlGetProp (priv->xmlcur, BAD_CAST "id");

    if (dbprint_walk_chunkQ (dbprint)) {
	title = BAD_CAST dbprint_walk_get_title (dbprint);

	/* if id attribute is not present, autogenerate a
	 * unique value, and insert it into the in-memory tree */
	if (!id) {
	    g_snprintf (autoidstr, 20, "_auto-gen-id-%d", ++autoid);
	    xmlNewProp (priv->xmlcur, BAD_CAST "id", BAD_CAST autoidstr);
	    id = xmlGetProp (priv->xmlcur, BAD_CAST "id"); 
	}

	debug_print (DB_DEBUG, "  id: \"%s\"\n", id);
	debug_print (DB_DEBUG, "  title: \"%s\"\n", title);

	yelp_document_add_title (document, (gchar *) id, (gchar *) title);


	if (priv->cur_prev_id) {
	    yelp_document_add_prev_id (document, (gchar *) id, priv->cur_prev_id);
	    yelp_document_add_next_id (document, priv->cur_prev_id, (gchar *) id);
	    g_free (priv->cur_prev_id);
	}
	priv->cur_prev_id = g_strdup ((gchar *) id);

	if (priv->cur_page_id)
	    yelp_document_add_up_id (document, (gchar *) id, priv->cur_page_id);
	old_page_id = priv->cur_page_id;
	priv->cur_page_id = g_strdup ((gchar *) id);

    }

    old_cur = priv->xmlcur;
    priv->cur_depth++;

    if (id)
	yelp_document_add_page_id (document, (gchar *) id, priv->cur_page_id);

    for (cur = priv->xmlcur->children; cur; cur = cur->next) {
	if (cur->type == XML_ELEMENT_NODE) {
	    priv->xmlcur = cur;
	    dbprint_walk (dbprint);
	}
    }
    priv->cur_depth--;
    priv->xmlcur = old_cur;

    if (dbprint_walk_chunkQ (dbprint)) {
	priv->sections_iter = old_iter;
	g_free (priv->cur_page_id);
	priv->cur_page_id = old_page_id;
    }

    if (priv->cur_depth == 0) {
	g_free (priv->cur_prev_id);
	priv->cur_prev_id = NULL;

	g_free (priv->cur_page_id);
	priv->cur_page_id = NULL;
    }

    if (id != NULL)
	xmlFree (id);
    if (title != NULL)
	xmlFree (title);
}

static gboolean
dbprint_walk_chunkQ (YelpDbprint *dbprint)
{
    if (dbprint->priv->cur_depth <= dbprint->priv->max_depth
	&& dbprint_walk_divisionQ (dbprint))
	return TRUE;
    else
	return FALSE;
}

static gboolean
dbprint_walk_divisionQ (YelpDbprint *dbprint)
{
    xmlNodePtr node = dbprint->priv->xmlcur;
    return (!xmlStrcmp (node->name, (const xmlChar *) "appendix")     ||
	    !xmlStrcmp (node->name, (const xmlChar *) "article")      ||
	    !xmlStrcmp (node->name, (const xmlChar *) "book")         ||
	    !xmlStrcmp (node->name, (const xmlChar *) "bibliography") ||
	    !xmlStrcmp (node->name, (const xmlChar *) "chapter")      ||
	    !xmlStrcmp (node->name, (const xmlChar *) "colophon")     ||
	    !xmlStrcmp (node->name, (const xmlChar *) "glossary")     ||
	    !xmlStrcmp (node->name, (const xmlChar *) "index")        ||
	    !xmlStrcmp (node->name, (const xmlChar *) "part")         ||
	    !xmlStrcmp (node->name, (const xmlChar *) "preface")      ||
	    !xmlStrcmp (node->name, (const xmlChar *) "reference")    ||
	    !xmlStrcmp (node->name, (const xmlChar *) "refentry")     ||
	    !xmlStrcmp (node->name, (const xmlChar *) "refsect1")     ||
	    !xmlStrcmp (node->name, (const xmlChar *) "refsect2")     ||
	    !xmlStrcmp (node->name, (const xmlChar *) "refsect3")     ||
	    !xmlStrcmp (node->name, (const xmlChar *) "refsection")   ||
	    !xmlStrcmp (node->name, (const xmlChar *) "sect1")        ||
	    !xmlStrcmp (node->name, (const xmlChar *) "sect2")        ||
	    !xmlStrcmp (node->name, (const xmlChar *) "sect3")        ||
	    !xmlStrcmp (node->name, (const xmlChar *) "sect4")        ||
	    !xmlStrcmp (node->name, (const xmlChar *) "sect5")        ||
	    !xmlStrcmp (node->name, (const xmlChar *) "section")      ||
	    !xmlStrcmp (node->name, (const xmlChar *) "set")          ||
	    !xmlStrcmp (node->name, (const xmlChar *) "setindex")     ||
	    !xmlStrcmp (node->name, (const xmlChar *) "simplesect")   );
}

static gchar *
dbprint_walk_get_title (YelpDbprint *dbprint)
{
    gchar *infoname = NULL;
    xmlNodePtr child = NULL;
    xmlNodePtr title = NULL;
    xmlNodePtr title_tmp = NULL;
    YelpDbprintPriv *priv = dbprint->priv;

    if (!xmlStrcmp (priv->xmlcur->name, BAD_CAST "refentry")) {
	/* The title for a refentry element can come from the following:
	 *   refmeta/refentrytitle
	 *   refentryinfo/title[abbrev]
	 *   refnamediv/refname
	 * We take the first one we find.
	 */
	for (child = priv->xmlcur->children; child; child = child->next) {
	    if (!xmlStrcmp (child->name, BAD_CAST "refmeta")) {
		for (title = child->children; title; title = title->next) {
		    if (!xmlStrcmp (title->name, BAD_CAST "refentrytitle"))
			break;
		}
		if (title)
		    goto done;
	    }
	    else if (!xmlStrcmp (child->name, BAD_CAST "refentryinfo")) {
		for (title = child->children; title; title = title->next) {
		    if (!xmlStrcmp (title->name, BAD_CAST "titleabbrev"))
			break;
		    else if (!xmlStrcmp (title->name, BAD_CAST "title"))
			title_tmp = title;
		}
		if (title)
		    goto done;
		else if (title_tmp) {
		    title = title_tmp;
		    goto done;
		}
	    }
	    else if (!xmlStrcmp (child->name, BAD_CAST "refnamediv")) {
		for (title = child->children; title; title = title->next) {
		    if (!xmlStrcmp (title->name, BAD_CAST "refname"))
			break;
		    else if (!xmlStrcmp (title->name, BAD_CAST "refpurpose")) {
			title = NULL;
			break;
		    }
		}
		if (title)
		    goto done;
	    }
	    else if (!xmlStrncmp (child->name, BAD_CAST "refsect", 7))
		break;
	}
    }
    else {
	/* The title for other elements appears in the following:
	 *   title[abbrev]
	 *   *info/title[abbrev]
	 *   blockinfo/title[abbrev]
	 *   objectinfo/title[abbrev]
	 * We take them in that order.
	 */
	xmlNodePtr infos[3] = {NULL, NULL, NULL};
	int i;

	infoname = g_strdup_printf ("%sinfo", priv->xmlcur->name);

	for (child = priv->xmlcur->children; child; child = child->next) {
	    if (!xmlStrcmp (child->name, BAD_CAST "titleabbrev")) {
		title = child;
		goto done;
	    }
	    else if (!xmlStrcmp (child->name, BAD_CAST "title"))
		title_tmp = child;
	    else if (!xmlStrcmp (child->name, BAD_CAST infoname))
		infos[0] = child;
	    else if (!xmlStrcmp (child->name, BAD_CAST "blockinfo"))
		infos[1] = child;
	    else if (!xmlStrcmp (child->name, BAD_CAST "objectinfo"))
		infos[2] = child;
	}

	if (title_tmp) {
	    title = title_tmp;
	    goto done;
	}

	for (i = 0; i < 3; i++) {
	    child = infos[i];
	    if (child) {
		for (title = child->children; title; title = title->next) {
		    if (!xmlStrcmp (title->name, BAD_CAST "titleabbrev"))
			goto done;
		    else if (!xmlStrcmp (title->name, BAD_CAST "title"))
			title_tmp = title;
		}
		if (title_tmp) {
		    title = title_tmp;
		    goto done;
		}
	    }
	}
    }

 done:
    g_free (infoname);

    if (title)
	return (gchar *) xmlNodeGetContent (title);
    else
	return g_strdup (_("Unknown"));
}
#endif
