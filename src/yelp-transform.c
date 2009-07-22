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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xinclude.h>
#include <libxml/xpathInternals.h>
#include <libxslt/documents.h>
#include <libxslt/xslt.h>
#include <libexslt/exslt.h>
#include <libxslt/templates.h>
#include <libxslt/transform.h>
#include <libxslt/extensions.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/xsltutils.h>

#include "yelp-debug.h"
#include "yelp-error.h"
#include "yelp-transform.h"

#define YELP_NAMESPACE "http://www.gnome.org/yelp/ns"

static gboolean exslt_registered = FALSE;

static void      transform_run         (YelpTransform  *transform);
static gboolean  transform_free        (YelpTransform  *transform);
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
static void      xslt_yelp_input       (xmlXPathParserContextPtr ctxt,
					int                      nargs);

/******************************************************************************/

YelpTransform
*yelp_transform_new (gchar             *stylesheet,
		     YelpTransformFunc  func,
		     gpointer           user_data)
{
    YelpTransform *transform;
    
    transform = g_new0 (YelpTransform, 1);
    transform->func = func;
    transform->user_data = user_data;

    transform->stylesheet = xsltParseStylesheetFile (BAD_CAST stylesheet);
    if (!transform->stylesheet) {
	transform->error =
	    yelp_error_new (_("Invalid Stylesheet"),
			    _("The XSLT stylesheet ‘%s’ is either missing, or it is "
			      "not valid."),
			    stylesheet);
	transform_error (transform);
	g_free (transform);
	return NULL;
    }

    transform->queue = g_async_queue_new ();
    transform->chunks = g_hash_table_new_full (g_str_hash,
					       g_str_equal,
					       g_free,
					       NULL);

    return transform;
}

void
yelp_transform_set_input (YelpTransform *transform,
			  xmlDocPtr      input)
{
    transform->input = input;
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
    if (!exslt_registered) {
	exsltRegisterAll ();
	exslt_registered = TRUE;
    }
    xsltRegisterExtElement (transform->context,
			    BAD_CAST "document",
			    BAD_CAST YELP_NAMESPACE,
			    (xsltTransformFunction) xslt_yelp_document);
    xsltRegisterExtElement (transform->context,
			    BAD_CAST "cache",
			    BAD_CAST YELP_NAMESPACE,
			    (xsltTransformFunction) xslt_yelp_cache);
    xsltRegisterExtFunction (transform->context,
			     BAD_CAST "input",
			     BAD_CAST YELP_NAMESPACE,
			     (xmlXPathFunction) xslt_yelp_input);

    transform->mutex = g_mutex_new ();
    g_mutex_lock (transform->mutex);
    transform->running = TRUE;
    transform->thread = g_thread_create ((GThreadFunc) transform_run,
					 transform, FALSE, NULL);
    g_mutex_unlock (transform->mutex);
}

gchar *
yelp_transform_eat_chunk (YelpTransform *transform, gchar *chunk_id)
{
    gchar *buf;

    g_mutex_lock (transform->mutex);

    buf = g_hash_table_lookup (transform->chunks, chunk_id);
    if (buf)
	g_hash_table_remove (transform->chunks, chunk_id);

    g_mutex_unlock (transform->mutex);

    /* The caller assumes ownership of this memory. */
    return buf;
}

void
yelp_transform_release (YelpTransform *transform)
{
    g_mutex_lock (transform->mutex);
    if (transform->running) {
	/* We can't free it just now, because the thread is running.
	 * Instead, we'll tell libxslt to stop and mark the transform
	 * as released.
	 */
	transform->released = TRUE;
	transform->context->state = XSLT_STATE_STOPPED;
    } else {
	/* We might still have pending pops from the queue, so just
	 * schedule transform_free.
	 */
	g_idle_add ((GSourceFunc) transform_free, transform);
    }
    g_mutex_unlock (transform->mutex);
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

    /* FIXME: do something with outputDoc? */
    transform->idle_funcs++;
    g_idle_add ((GSourceFunc) transform_final, transform);

    g_mutex_lock (transform->mutex);
    transform->running = FALSE;
    if (transform->released) {
	/* The transform was released by its owner, but it couldn't
	 * be freed because this thread was running.  But we're in
	 * a thread, and the main thread might still be popping stuff
	 * off the asynchronous queue.  Schedule this for freeing.
	 */
	g_idle_add ((GSourceFunc) transform_free, transform);
    }
    g_mutex_unlock (transform->mutex);
}

static gboolean
transform_free (YelpTransform *transform)
{
    gchar *chunk_id;

    /* If the queue isn't empty yet, try again later.  But because
     * threads scare me and I don't want runaway code, stop trying
     * after an insane number of attempts and just leak.
     */
    if (transform->idle_funcs > 0) {
	transform->free_attempts++;
	if (transform->free_attempts < 1000) {
	    return TRUE;
	} else {
	    g_warning ("Runaway free attempt detected.  Memory is about to leak.\n");
	    return FALSE;
	}
    }

    g_mutex_lock (transform->mutex);
    if (transform->input_xslt)
	transform->input_xslt->doc = NULL;
    if (transform->outputDoc)
	xmlFreeDoc (transform->outputDoc);
    if (transform->stylesheet)
	xsltFreeStylesheet (transform->stylesheet);
    if (transform->context)
	xsltFreeTransformContext (transform->context);

    g_strfreev (transform->params);

    /* FIXME: destroy data */
    while ((chunk_id = (gchar *) g_async_queue_try_pop (transform->queue)))
	g_free (chunk_id);
    g_async_queue_unref (transform->queue);
    g_mutex_unlock (transform->mutex);
    g_mutex_free (transform->mutex);

    if (transform->error)
	yelp_error_free (transform->error);

    g_free (transform);
    return FALSE;
}

static void
transform_set_error (YelpTransform *transform,
		     YelpError     *error)
{
    g_mutex_lock (transform->mutex);
    if (transform->released) {
	yelp_error_free (error);
	g_mutex_unlock (transform->mutex);
	return;
    }
    if (transform->error)
	yelp_error_free (transform->error);
    transform->error = error;
    transform->idle_funcs++;
    g_idle_add ((GSourceFunc) transform_error, transform);
    g_mutex_unlock (transform->mutex);
}

static gboolean
transform_chunk (YelpTransform *transform)
{
    gchar *chunk_id;

    transform->idle_funcs--;
    if (transform->released)
	return FALSE;

    chunk_id = (gchar *) g_async_queue_try_pop (transform->queue);

    if (chunk_id) {
	if (transform->func)
	    transform->func (transform,
			     YELP_TRANSFORM_CHUNK,
			     chunk_id,
			     transform->user_data);
	else
	    g_free (chunk_id);
    }

    return FALSE;
}

static gboolean
transform_error (YelpTransform *transform)
{
    YelpError *error;

    transform->idle_funcs--;
    if (transform->released)
	return FALSE;

    g_mutex_lock (transform->mutex);

    error = transform->error;
    transform->error = NULL;

    g_mutex_unlock (transform->mutex);

    if (transform->func)
	transform->func (transform,
			 YELP_TRANSFORM_ERROR,
			 error, transform->user_data);
    else
	yelp_error_free (error);

    return FALSE;
}

static gboolean
transform_final (YelpTransform *transform)
{
    transform->idle_funcs--;

    if (transform->released)
	return FALSE;

    /* FIXME: check for anything remaining on the queue */
    if (transform->func)
	transform->func (transform,
			 YELP_TRANSFORM_FINAL,
			 NULL, transform->user_data);

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
    xmlChar *page_id = NULL;
    gchar   *temp;
    xmlChar *page_buf;
    gint     buf_size;
    xsltStylesheetPtr style = NULL;
    const char *old_outfile;
    xmlDocPtr   new_doc = NULL;
    xmlDocPtr   old_doc;
    xmlNodePtr  old_insert;

    debug_print (DB_FUNCTION, "entering\n");

    if (ctxt->state == XSLT_STATE_STOPPED)
	return;

    if (!ctxt || !node || !inst || !comp)
	return;

    transform = (YelpTransform *) ctxt->_private;

    page_id = xsltEvalAttrValueTemplate (ctxt, inst,
					 (const xmlChar *) "href",
					 NULL);
    if (page_id == NULL || *page_id == '\0') {
	if (page_id)
	    xmlFree (page_id);
	else
	    xsltTransformError (ctxt, NULL, inst,
				_("No href attribute found on "
				  "yelp:document\n"));
	/* FIXME: put a real error here */
	goto done;
    }
    debug_print (DB_ARG, "  page_id = \"%s\"\n", page_id);

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

    g_mutex_lock (transform->mutex);

    temp = g_strdup ((gchar *) page_id);
    xmlFree (page_id);

    g_async_queue_push (transform->queue, g_strdup ((gchar *) temp));
    g_hash_table_insert (transform->chunks, temp, page_buf);
    transform->idle_funcs++;
    g_idle_add ((GSourceFunc) transform_chunk, transform);

    g_mutex_unlock (transform->mutex);

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

static void
xslt_yelp_input (xmlXPathParserContextPtr ctxt, int nargs)
{
    xsltTransformContextPtr tctxt;
    xmlXPathObjectPtr ret;
    YelpTransform *transform;

    tctxt = xsltXPathGetTransformContext (ctxt);
    transform = (YelpTransform *) tctxt->_private;

    /* FIXME: pretty sure this eats transform->input, memory corruption will follow */
    transform->input_xslt = xsltNewDocument (tctxt, transform->input);

    ret = xmlXPathNewNodeSet (xmlDocGetRootElement (transform->input));
    xsltExtensionInstructionResultRegister (tctxt, ret);
    valuePush (ctxt, ret);
}
