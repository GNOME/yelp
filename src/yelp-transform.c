/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2006 Shaun McCance  <shaunm@gnome.org>
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
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xinclude.h>
#include <libxslt/xslt.h>
#include <libxslt/templates.h>
#include <libxslt/transform.h>
#include <libxslt/extensions.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/xsltutils.h>

#include "yelp-debug.h"
#include "yelp-error.h"
#include "yelp-transform.h"

#define YELP_NAMESPACE "http://www.gnome.org/yelp/ns"

static void      transform_run         (YelpTransform  *transform);
static void      transform_free        (YelpTransform  *transform);
static void      transform_set_error   (YelpTransform  *transform,
					YelpError      *error);

static gboolean  transform_chunk       (YelpTransform  *transform);
static gboolean  transform_error       (YelpTransform  *transform);
static gboolean  transform_final       (YelpTransform  *transform);

static void      xslt_yelp_document    (xsltTransformContextPtr ctxt,
					xmlNodePtr              node,
					xmlNodePtr              inst,
					xsltStylePreCompPtr     comp);
static void      xslt_yelp_cache       (xsltTransformContextPtr ctxt,
					xmlNodePtr              node,
					xmlNodePtr              inst,
					xsltStylePreCompPtr     comp);

/******************************************************************************/

YelpTransform
*yelp_transform_new (gchar                   *stylesheet,
		     YelpTransformChunkFunc   chunk_func,
		     YelpTransformErrorFunc   error_func,
		     YelpTransformFinalFunc   final_func,
		     gpointer                 user_data)
{
    YelpTransform *transform;
    
    transform = g_new0 (YelpTransform, 1);

    transform->stylesheet = xsltParseStylesheetFile (BAD_CAST stylesheet);
    if (!transform->stylesheet) {
	transform->error =
	    yelp_error_new (_("Invalid Stylesheet"),
			    _("The XSLT stylesheet ‘%s’ is either missing, or it is "
			      "not valid."),
			    stylesheet);
	transform_error (transform);
	return NULL;
    }

    transform->chunk_func = chunk_func;
    transform->error_func = error_func;
    transform->final_func = final_func;

    transform->queue = g_async_queue_new ();

    transform->user_data = user_data;

    return transform;
}

void
yelp_transform_start (YelpTransform *transform,
		      xmlDocPtr      document,
		      gchar        **params)
{
    transform->inputDoc = document;

    transform->context = xsltNewTransformContext (transform->stylesheet,
						  transform->inputDoc);
    if (!transform->context) {
	YelpError *error = 
	    yelp_error_new (_("Broken Transformation"),
			    _("An unknown error occurred while attempting to "
			      "transform the document."));
	transform_set_error (transform, error);
	return;
    }

    transform->params = g_strdupv (params);

    transform->context->_private = transform;
    xsltRegisterExtElement (transform->context,
			    BAD_CAST "document",
			    BAD_CAST YELP_NAMESPACE,
			    (xsltTransformFunction) xslt_yelp_document);
    xsltRegisterExtElement (transform->context,
			    BAD_CAST "cache",
			    BAD_CAST YELP_NAMESPACE,
			    (xsltTransformFunction) xslt_yelp_cache);

    transform->mutex = g_mutex_new ();
    g_mutex_lock (transform->mutex);
    transform->running = TRUE;
    transform->thread = g_thread_create ((GThreadFunc) transform_run,
					 transform, FALSE, NULL);
    g_mutex_unlock (transform->mutex);
}

void
yelp_transform_release (YelpTransform *transform)
{
    g_mutex_lock (transform->mutex);
    if (transform->running) {
	transform->freeme = TRUE;
	transform->context->state = XSLT_STATE_STOPPED;
	g_mutex_unlock (transform->mutex);
    } else {
	transform_free (transform);
    }
}

void
yelp_transform_chunk_free (YelpTransformChunk *chunk)
{
    g_free (chunk->id);
    g_free (chunk->title);
    g_free (chunk->contents);

    g_free (chunk);
}

/******************************************************************************/

static void
transform_run (YelpTransform *transform)
{
    transform->outputDoc = xsltApplyStylesheetUser (transform->stylesheet,
						    transform->inputDoc,
						    (const char **) transform->params,
						    NULL, NULL,
						    transform->context);
    // FIXME: do something with outputDoc?
    // FIXME: check for anything remaining on the queue
    g_idle_add ((GSourceFunc) transform_final, transform);

    g_mutex_lock (transform->mutex);
    if (transform->freeme) {
	transform_free (transform);
    } else {
	transform->running = FALSE;
	g_mutex_unlock (transform->mutex);
    }
}

static void
transform_free (YelpTransform *transform)
{
    if (transform->outputDoc)
	xmlFreeDoc (transform->outputDoc);
    if (transform->stylesheet)
	xsltFreeStylesheet (transform->stylesheet);
    if (transform->context)
	xsltFreeTransformContext (transform->context);

    g_strfreev (transform->params);

    g_async_queue_unref (transform->queue);
    g_mutex_free (transform->mutex);

    if (transform->error)
	yelp_error_free (transform->error);

    g_free (transform);
}

static void
transform_set_error (YelpTransform *transform,
		     YelpError     *error)
{
    if (transform->error)
	yelp_error_free (transform->error);
    transform->error = error;
    g_idle_add ((GSourceFunc) transform_error, transform);
}

static gboolean
transform_chunk (YelpTransform *transform)
{
    YelpTransformChunk *chunk;

    chunk = (YelpTransformChunk *) g_async_queue_try_pop (transform->queue);

    if (chunk) {
	if (transform->chunk_func)
	    transform->chunk_func (transform, chunk, transform->user_data);
	else
	    yelp_transform_chunk_free (chunk);
    }

    return FALSE;
}

static gboolean
transform_error (YelpTransform *transform)
{
    YelpError *error;

    error = transform->error;
    transform->error = NULL;

    if (transform->error_func)
	transform->error_func (transform, error, transform->user_data);
    else
	yelp_error_free (error);

    return FALSE;
}

static gboolean
transform_final (YelpTransform *transform)
{
    if (transform->final_func)
	transform->final_func (transform, transform->user_data);

    return FALSE;
}

/******************************************************************************/

static void
xslt_yelp_document (xsltTransformContextPtr ctxt,
		    xmlNodePtr              node,
		    xmlNodePtr              inst,
		    xsltStylePreCompPtr     comp)
{
    YelpTransform *transform;
    YelpTransformChunk *chunk;
    xmlChar *page_id = NULL;
    xmlChar *page_title = NULL;
    xmlChar *page_buf;
    gint     buf_size;
    xsltStylesheetPtr style = NULL;
    const char *old_outfile;
    xmlDocPtr   new_doc = NULL;
    xmlDocPtr   old_doc;
    xmlNodePtr  old_insert;
    xmlNodePtr  cur;

    if (!ctxt || !node || !inst || !comp)
	return;

    transform = (YelpTransform *) ctxt->_private;

    debug_print (DB_FUNCTION, "entering\n");

    page_id = xsltEvalAttrValueTemplate (ctxt, inst,
					 (const xmlChar *) "href",
					 NULL);
    if (page_id == NULL) {
	xsltTransformError (ctxt, NULL, inst,
			    _("No href attribute found on yelp:document"));
	/* FIXME: put a real error here */
	goto done;
    }
    debug_print (DB_FUNCTION, "  page_id = \"%s\"\n", page_id);

    old_outfile = ctxt->outputFile;
    old_doc     = ctxt->output;
    old_insert  = ctxt->insert;
    ctxt->outputFile = (const char *) page_id;

    style = xsltNewStylesheet ();
    if (style == NULL) {
	xsltTransformError (ctxt, NULL, inst,
			    _("Out of memory"));
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

    if (!page_title)
	page_title = BAD_CAST g_strdup (_("Unknown Page"));

    chunk = g_new0 (YelpTransformChunk, 1);

    chunk->id = (gchar *) page_id;
    chunk->title = (gchar *) page_title;
    chunk->contents = (gchar *) page_buf;

    g_async_queue_push (transform->queue, chunk);
    g_idle_add ((GSourceFunc) transform_chunk, transform);

 done:
    if (new_doc)
	xmlFreeDoc (new_doc);
    if (style)
	xsltFreeStylesheet (style);
}

static void
xslt_yelp_cache (xsltTransformContextPtr ctxt,
		 xmlNodePtr              node,
		 xmlNodePtr              inst,
		 xsltStylePreCompPtr     comp)
{
}
