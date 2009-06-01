/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009 Shaun McCance  <shaunm@gnome.org>
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
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xinclude.h>

#include "yelp-error.h"
#include "yelp-mallard.h"
#include "yelp-settings.h"
#include "yelp-transform.h"
#include "yelp-debug.h"

#define STYLESHEET DATADIR"/yelp/xslt/mal2html.xsl"

#define YELP_MALLARD_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_MALLARD, YelpMallardPriv))

struct _YelpMallardPriv {
    gchar         *directory;

    GMutex        *mutex;
    GThread       *thread;
};


static void           mallard_class_init      (YelpMallardClass    *klass);
static void           mallard_init            (YelpMallard         *mallard);
static void           mallard_try_dispose     (GObject             *object);
static void           mallard_dispose         (GObject             *object);

/* YelpDocument */
static void           mallard_request         (YelpDocument        *document,
					       gint                 req_id,
					       gboolean             handled,
					       gchar               *page_id,
					       YelpDocumentFunc     func,
					       gpointer             user_data);

/* YelpTransform */
static void           transform_func          (YelpTransform       *transform,
					       YelpTransformSignal  signal,
					       gpointer             func_data,
					       YelpMallard         *mallard);
static void           transform_page_func     (YelpTransform       *transform,
					       gchar               *page_id,
					       YelpMallard         *mallard);
static void           transform_final_func    (YelpTransform       *transform,
					       YelpMallard         *mallard);


static YelpDocumentClass *parent_class;

GType
yelp_mallard_get_type (void)
{
    static GType type = 0;
    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpMallardClass),
	    NULL, NULL,
	    (GClassInitFunc) mallard_class_init,
	    NULL, NULL,
	    sizeof (YelpMallard),
	    0,
	    (GInstanceInitFunc) mallard_init,
	};
	type = g_type_register_static (YELP_TYPE_DOCUMENT,
				       "YelpMallard", 
				       &info, 0);
    }
    return type;
}

static void
mallard_class_init (YelpMallardClass *klass)
{
    GObjectClass      *object_class   = G_OBJECT_CLASS (klass);
    YelpDocumentClass *document_class = YELP_DOCUMENT_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = mallard_try_dispose;

    document_class->request = mallard_request;
    document_class->cancel = NULL;

    g_type_class_add_private (klass, sizeof (YelpMallardPriv));
}

static void
mallard_init (YelpMallard *mallard)
{
    YelpMallardPriv *priv;
    priv = mallard->priv = YELP_MALLARD_GET_PRIVATE (mallard);

    priv->mutex = g_mutex_new ();
}

static void
mallard_try_dispose (GObject *object)
{
    YelpMallardPriv *priv;

    g_assert (object != NULL && YELP_IS_MALLARD (object));

    priv = YELP_MALLARD (object)->priv;

    /*
    g_mutex_lock (priv->mutex);
    if (priv->process_running || priv->transform_running) {
	priv->state = DOCBOOK_STATE_STOP;
	g_idle_add ((GSourceFunc) docbook_try_dispose, object);
	g_mutex_unlock (priv->mutex);
    } else {
	g_mutex_unlock (priv->mutex);
	docbook_dispose (object);
    }
    */
    mallard_dispose (object);
}

static void
mallard_dispose (GObject *object)
{
    YelpMallard *mallard = YELP_MALLARD (object);

    g_free (mallard->priv->directory);

    g_mutex_free (mallard->priv->mutex);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

YelpDocument *
yelp_mallard_new (gchar *directory)
{
    YelpMallard *mallard;

    g_return_val_if_fail (directory != NULL, NULL);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  directory = \"%s\"\n", directory);

    mallard = (YelpMallard *) g_object_new (YELP_TYPE_MALLARD, NULL);
    mallard->priv->directory = g_strdup (directory);

    /*
      FIXME: find pages, add
    yelp_document_add_page_id (YELP_DOCUMENT (docbook), "x-yelp-titlepage", "x-yelp-titlepage");
    */

    return (YelpDocument *) mallard;
}


#if 0
static void
docbook_request (YelpDocument     *document,
		 gint              req_id,
		 gboolean          handled,
		 gchar            *page_id,
		 YelpDocumentFunc  func,
		 gpointer          user_data)
{
    YelpDocbook *docbook;
    YelpDocbookPriv *priv;
    YelpError *error;

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  req_id  = %i\n", req_id);
    debug_print (DB_ARG, "  page_id = \"%s\"\n", page_id);
    g_assert (document != NULL && YELP_IS_DOCBOOK (document));

    if (handled) {
	return;
    }

    docbook = YELP_DOCBOOK (document);
    priv = docbook->priv;

    g_mutex_lock (priv->mutex);

    switch (priv->state) {
    case DOCBOOK_STATE_BLANK:
	priv->state = DOCBOOK_STATE_PARSING;
	priv->process_running = TRUE;
	priv->thread = g_thread_create ((GThreadFunc) docbook_process,
					docbook, FALSE, NULL);
	break;
    case DOCBOOK_STATE_PARSING:
	break;
    case DOCBOOK_STATE_PARSED:
    case DOCBOOK_STATE_STOP:
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
		YelpDocbook         *docbook)
{
    YelpDocbookPriv *priv;
    debug_print (DB_FUNCTION, "entering\n");

    g_assert (docbook != NULL && YELP_IS_DOCBOOK (docbook));

    priv = docbook->priv;

    g_assert (transform == priv->transform);

    if (priv->state == DOCBOOK_STATE_STOP) {
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
	transform_page_func (transform, (gchar *) func_data, docbook);
	break;
    case YELP_TRANSFORM_ERROR:
	yelp_document_error_pending (YELP_DOCUMENT (docbook), (YelpError *) func_data);
	yelp_transform_release (transform);
	priv->transform = NULL;
	priv->transform_running = FALSE;
	break;
    case YELP_TRANSFORM_FINAL:
	transform_final_func (transform, docbook);
	break;
    }
}

static void
transform_page_func (YelpTransform *transform,
		     gchar         *page_id,
		     YelpDocbook   *docbook)
{
    YelpDocbookPriv *priv;
    gchar *content;

    debug_print (DB_FUNCTION, "entering\n");

    priv = docbook->priv;
    g_mutex_lock (priv->mutex);

    content = yelp_transform_eat_chunk (transform, page_id);
    yelp_document_add_page (YELP_DOCUMENT (docbook), page_id, content);

    g_free (page_id);

    g_mutex_unlock (priv->mutex);
}

static void
transform_final_func (YelpTransform *transform, YelpDocbook *docbook)
{
    YelpError *error;
    YelpDocbookPriv *priv = docbook->priv;

    debug_print (DB_FUNCTION, "entering\n");

    g_mutex_lock (priv->mutex);

    error = yelp_error_new (_("Page not found"),
			    _("The requested page was not found in the document %s."),
			    priv->filename);
    yelp_document_final_pending (YELP_DOCUMENT (docbook), error);

    yelp_transform_release (transform);
    priv->transform = NULL;
    priv->transform_running = FALSE;
    priv->state = DOCBOOK_STATE_PARSED;

    if (priv->xmldoc)
	xmlFreeDoc (priv->xmldoc);
    priv->xmldoc = NULL;

    g_mutex_unlock (priv->mutex);
}


/******************************************************************************/
/** Threaded ******************************************************************/

static void
docbook_process (YelpDocbook *docbook)
{
    YelpDocbookPriv *priv;
    xmlDocPtr xmldoc = NULL;
    xmlChar *id = NULL;
    YelpError *error = NULL;
    xmlParserCtxtPtr parserCtxt = NULL;
    YelpDocument *document;

    gint  params_i = 0;
    gint  params_max = 10;
    gchar **params = NULL;

    debug_print (DB_FUNCTION, "entering\n");

    g_assert (docbook != NULL && YELP_IS_DOCBOOK (docbook));
    g_object_ref (docbook);
    priv = docbook->priv;
    document = YELP_DOCUMENT (docbook);

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
    if (priv->state == DOCBOOK_STATE_STOP) {
	g_mutex_unlock (priv->mutex);
	goto done;
    }
    g_mutex_unlock (priv->mutex);

    docbook_walk (docbook);

    g_mutex_lock (priv->mutex);
    if (priv->state == DOCBOOK_STATE_STOP) {
	g_mutex_unlock (priv->mutex);
	goto done;
    }
    priv->transform = yelp_transform_new (STYLESHEET,
					  (YelpTransformFunc) transform_func,
					  docbook);
    priv->transform_running = TRUE;

    params = g_new0 (gchar *, params_max);
    yelp_settings_params (&params, &params_i, &params_max);


    if ((params_i + 10) >= params_max - 1) {
	params_max += 20;
	params = g_renew (gchar *, params, params_max);
    }
    params[params_i++] = "db.chunk.max_depth";
    params[params_i++] = g_strdup_printf ("%i", docbook->priv->max_depth);

    params[params_i] = NULL;

    yelp_transform_start (priv->transform,
			  priv->xmldoc,
			  params);
    g_mutex_unlock (priv->mutex);

 done:
    if (id)
	xmlFree (id);
    if (parserCtxt)
	xmlFreeParserCtxt (parserCtxt);

    priv->process_running = FALSE;
    g_object_unref (docbook);
}


/******************************************************************************/
/** Walker ********************************************************************/

static void
docbook_walk (YelpDocbook *docbook)
{
    static       gint autoid = 0;
    gchar        autoidstr[20];
    xmlChar     *id = NULL;
    xmlChar     *title = NULL;
    gchar       *old_page_id = NULL;
    xmlNodePtr   cur, old_cur;
    GtkTreeIter  iter;
    GtkTreeIter *old_iter = NULL;
    YelpDocbookPriv *priv = docbook->priv;
    YelpDocument *document = YELP_DOCUMENT (docbook);

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

    if (docbook_walk_divisionQ (docbook) && !id) {
	/* if id attribute is not present, autogenerate a
	 * unique value, and insert it into the in-memory tree */
	g_snprintf (autoidstr, 20, "_auto-gen-id-%d", ++autoid);
	xmlNewProp (priv->xmlcur, BAD_CAST "id", BAD_CAST autoidstr);
	id = xmlGetProp (priv->xmlcur, BAD_CAST "id"); 
    }

    if (docbook_walk_chunkQ (docbook)) {
	title = BAD_CAST docbook_walk_get_title (docbook);

	debug_print (DB_DEBUG, "  id: \"%s\"\n", id);
	debug_print (DB_DEBUG, "  title: \"%s\"\n", title);

	yelp_document_add_title (document, (gchar *) id, (gchar *) title);

	gdk_threads_enter ();
	gtk_tree_store_append (GTK_TREE_STORE (priv->sections),
			       &iter,
			       priv->sections_iter);
	gtk_tree_store_set (GTK_TREE_STORE (priv->sections),
			    &iter,
			    YELP_DOCUMENT_COLUMN_ID, id,
			    YELP_DOCUMENT_COLUMN_TITLE, title,
			    -1);
	gdk_threads_leave ();

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

	old_iter = priv->sections_iter;
	if (priv->xmlcur->parent->type != XML_DOCUMENT_NODE)
	    priv->sections_iter = &iter;
    }

    old_cur = priv->xmlcur;
    priv->cur_depth++;
    if (id && !g_str_equal (id, "x-yelp-index"))
	yelp_document_add_page_id (document, (gchar *) id, priv->cur_page_id);

    for (cur = priv->xmlcur->children; cur; cur = cur->next) {
	if (cur->type == XML_ELEMENT_NODE) {
	    priv->xmlcur = cur;
	    docbook_walk (docbook);
	}
    }
    priv->cur_depth--;
    priv->xmlcur = old_cur;

    if (docbook_walk_chunkQ (docbook)) {
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
docbook_walk_chunkQ (YelpDocbook *docbook)
{
    if (docbook->priv->cur_depth <= docbook->priv->max_depth
	&& docbook_walk_divisionQ (docbook))
	return TRUE;
    else
	return FALSE;
}

static gboolean
docbook_walk_divisionQ (YelpDocbook *docbook)
{
    xmlNodePtr node = docbook->priv->xmlcur;
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
docbook_walk_get_title (YelpDocbook *docbook)
{
    gchar *infoname = NULL;
    xmlNodePtr child = NULL;
    xmlNodePtr title = NULL;
    xmlNodePtr title_tmp = NULL;
    YelpDocbookPriv *priv = docbook->priv;

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
