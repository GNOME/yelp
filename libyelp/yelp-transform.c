/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2009 Shaun McCance  <shaunm@gnome.org>
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
#include <glib-object.h>
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

static void      yelp_transform_init        (YelpTransform           *transform);
static void      yelp_transform_class_init  (YelpTransformClass      *klass);
static void      yelp_transform_dispose     (GObject                 *object);
static void      yelp_transform_finalize    (GObject                 *object);

static void      transform_run              (YelpTransform           *transform);
static gboolean  transform_free             (YelpTransform           *transform);
static void      transform_set_error        (YelpTransform           *transform,
                                             YelpError               *error);

static gboolean  transform_chunk            (YelpTransform           *transform);
static gboolean  transform_error            (YelpTransform           *transform);
static gboolean  transform_final            (YelpTransform           *transform);

static void      xslt_yelp_document         (xsltTransformContextPtr  ctxt,
                                             xmlNodePtr               node,
                                             xmlNodePtr               inst,
                                             xsltStylePreCompPtr      comp);
static void      xslt_yelp_cache            (xsltTransformContextPtr  ctxt,
                                             xmlNodePtr               node,
                                             xmlNodePtr               inst,
                                             xsltStylePreCompPtr      comp);
static void      xslt_yelp_aux              (xmlXPathParserContextPtr ctxt,
                                             int                      nargs);

enum {
    CHUNK_READY,
    FINISHED,
    ERROR,
    LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (YelpTransform, yelp_transform, G_TYPE_OBJECT);
#define GET_PRIV(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_TRANSFORM, YelpTransformPrivate))

typedef struct _YelpTransformPrivate YelpTransformPrivate;
struct _YelpTransformPrivate {
    xmlDocPtr                input;
    xmlDocPtr                output;
    xsltStylesheetPtr        stylesheet;
    xsltTransformContextPtr  context;

    xmlDocPtr                aux;
    xsltDocumentPtr          aux_xslt;

    gchar                 **params;

    GThread                *thread;
    GMutex                 *mutex;
    GAsyncQueue            *queue;
    GHashTable             *chunks;

    gboolean                running;
    gboolean                cancelled;

    GError                 *error;

    /* FIXME: remove
    YelpTransformFunc       func;
    gpointer                user_data;
    */
};

/******************************************************************************/

static void
yelp_transform_init (YelpTransform *transform)
{
    YelpTransformPrivate *priv = GET_PRIV (transform);
    priv->queue = g_async_queue_new ();
    priv->chunks = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          NULL);
}

static void
yelp_transform_class_init (YelpTransformClass *klass)
{
    exsltRegisterAll ();

    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = yelp_transform_dispose;
    object_class->finalize = yelp_transform_finalize;

    signals[CHUNK_READY] = g_signal_new ("chunk-ready",
                                   G_TYPE_FROM_CLASS (klass),
                                   G_SIGNAL_RUN_LAST,
                                   0, NULL, NULL,
                                   g_cclosure_marshal_VOID__STRING,
                                   G_TYPE_NONE, 1, G_TYPE_STRING);
    signals[FINISHED] = g_signal_new ("finished",
                                      G_TYPE_FROM_CLASS (klass),
                                      G_SIGNAL_RUN_LAST,
                                      0, NULL, NULL,
                                      g_cclosure_marshal_VOID__VOID,
                                      G_TYPE_NONE, 0);
    signals[ERROR] = g_signal_new ("error",
                                   G_TYPE_FROM_CLASS (klass),
                                   G_SIGNAL_RUN_LAST,
                                   0, NULL, NULL,
                                   g_cclosure_marshal_VOID__VOID,
                                   G_TYPE_NONE, 0);

    g_type_class_add_private (klass, sizeof (YelpTransformPrivate));
}

static void
yelp_transform_dispose (GObject *object)
{
    YelpTransformPrivate *priv = GET_PRIV (object);

    if (priv->queue) {
        gchar *chunk_id;
        while ((chunk_id = (gchar *) g_async_queue_try_pop (priv->queue)))
            g_free (chunk_id);
        g_async_queue_unref (priv->queue);
        priv->queue = NULL;
    }

    /* FIXME */
    GHashTable             *chunks;

    G_OBJECT_CLASS (yelp_transform_parent_class)->dispose (object);
}

static void
yelp_transform_finalize (GObject *object)
{
    YelpTransformPrivate *priv = GET_PRIV (object);
    xsltDocumentPtr xsltdoc;

    if (priv->output)
        xmlFreeDoc (priv->output);
    if (priv->stylesheet)
        xsltFreeStylesheet (priv->stylesheet);
    /* We do not free input or aux.  They belong to the caller, which
       must ensure they exist for the lifetime of the transform.  We
       have to set priv->aux_xslt->doc (which is priv->aux) to NULL
       before xsltFreeTransformContext.  Otherwise it will be freed,
       which we don't want.
     */
    if (priv->aux_xslt)
        priv->aux_xslt->doc = NULL;
    if (priv->context)
        xsltFreeTransformContext (priv->context);
    if (priv->error)
        g_error_free (priv->error);

    g_strfreev (priv->params);
    g_mutex_free (priv->mutex);

    G_OBJECT_CLASS (yelp_transform_parent_class)->finalize (object);
}

/******************************************************************************/

YelpTransform *
yelp_transform_new ()
{
    return (YelpTransform *) g_object_new (YELP_TYPE_TRANSFORM, NULL);
}

gboolean
yelp_transform_set_stylesheet (YelpTransform *transform,
                               const gchar   *stylesheet,
                               GError       **error)
{
    YelpTransformPrivate *priv = GET_PRIV (transform);

    priv->stylesheet = xsltParseStylesheetFile (BAD_CAST stylesheet);
    if (priv->stylesheet == NULL) {
        if (error)
            g_set_error(error, YELP_ERROR, YELP_ERROR_PROCESSING,
                        _("The XSLT stylesheet ‘%s’ is either missing or not valid."),
                        stylesheet);
        return FALSE;
    }
    return TRUE;
}

void
yelp_transform_set_aux (YelpTransform *transform,
                        xmlDocPtr      aux)
{
    YelpTransformPrivate *priv = GET_PRIV (transform);
    priv->aux = aux;
}

gboolean
yelp_transform_start (YelpTransform       *transform,
                      xmlDocPtr            document,
                      const gchar * const *params,
                      GError             **error)
{
    YelpTransformPrivate *priv = GET_PRIV (transform);

    priv->input = document;

    priv->context = xsltNewTransformContext (priv->stylesheet,
                                             priv->input);
    if (priv->context == NULL) {
        if (error)
            g_set_error (error, YELP_ERROR, YELP_ERROR_PROCESSING,
                         _("The XSLT stylesheet could not be compiled."));
        return FALSE;
    }

    priv->params = g_strdupv ((gchar **) params);

    priv->context->_private = transform;
    xsltRegisterExtElement (priv->context,
                            BAD_CAST "document",
                            BAD_CAST YELP_NAMESPACE,
                            (xsltTransformFunction) xslt_yelp_document);
    xsltRegisterExtElement (priv->context,
                            BAD_CAST "cache",
                            BAD_CAST YELP_NAMESPACE,
                            (xsltTransformFunction) xslt_yelp_cache);
    xsltRegisterExtFunction (priv->context,
                             BAD_CAST "input",
                             BAD_CAST YELP_NAMESPACE,
                             (xmlXPathFunction) xslt_yelp_aux);

    priv->mutex = g_mutex_new ();
    g_mutex_lock (priv->mutex);
    priv->running = TRUE;
    g_object_ref (transform);
    priv->thread = g_thread_create ((GThreadFunc) transform_run,
                                    transform, FALSE, NULL);
    g_mutex_unlock (priv->mutex);

    return TRUE;
}

gchar *
yelp_transform_take_chunk (YelpTransform *transform,
                           const gchar   *chunk_id)
{
    YelpTransformPrivate *priv = GET_PRIV (transform);
    gchar *buf;

    g_mutex_lock (priv->mutex);

    buf = g_hash_table_lookup (priv->chunks, chunk_id);
    if (buf)
        g_hash_table_remove (priv->chunks, chunk_id);

    g_mutex_unlock (priv->mutex);

    /* The caller assumes ownership of this memory. */
    return buf;
}

void
yelp_transform_cancel (YelpTransform *transform)
{
    YelpTransformPrivate *priv = GET_PRIV (transform);
    g_mutex_lock (priv->mutex);
    if (priv->running) {
        priv->cancelled = TRUE;
        priv->context->state = XSLT_STATE_STOPPED;
    }
    g_mutex_unlock (priv->mutex);
}

GError *
yelp_transform_get_error (YelpTransform *transform)
{
    YelpTransformPrivate *priv = GET_PRIV (transform);
    GError *ret = NULL;

    g_mutex_lock (priv->mutex);
    if (priv->error)
        ret = g_error_copy (priv->error);
    g_mutex_unlock (priv->mutex);

    return ret;
}

/******************************************************************************/

static void
transform_run (YelpTransform *transform)
{
    YelpTransformPrivate *priv = GET_PRIV (transform);

    debug_print (DB_FUNCTION, "entering\n");

    priv->output = xsltApplyStylesheetUser (priv->stylesheet,
                                            priv->input,
                                            (const char **) priv->params,
                                            NULL, NULL,
                                            priv->context);
    g_mutex_lock (priv->mutex);
    priv->running = FALSE;
    if (!priv->cancelled) {
        g_idle_add ((GSourceFunc) transform_final, transform);
    }
    else {
        g_object_unref (transform);
    }
    g_mutex_unlock (priv->mutex);
}

static gboolean
transform_chunk (YelpTransform *transform)
{
    YelpTransformPrivate *priv = GET_PRIV (transform);
    gchar *chunk_id;

    debug_print (DB_FUNCTION, "entering\n");

    if (priv->cancelled)
        goto done;

    chunk_id = (gchar *) g_async_queue_try_pop (priv->queue);

    g_signal_emit (transform, signals[CHUNK_READY], 0, chunk_id);

    g_free (chunk_id);

 done:
    g_object_unref (transform);
    return FALSE;
}

static gboolean
transform_final (YelpTransform *transform)
{
    YelpTransformPrivate *priv = GET_PRIV (transform);

    debug_print (DB_FUNCTION, "entering\n");

    if (priv->cancelled)
        goto done;

    g_signal_emit (transform, signals[FINISHED], 0);

 done:
    g_object_unref (transform);
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
    YelpTransformPrivate *priv;
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

    transform = YELP_TRANSFORM (ctxt->_private);
    priv = GET_PRIV (transform);

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

    g_mutex_lock (priv->mutex);

    temp = g_strdup ((gchar *) page_id);
    xmlFree (page_id);

    g_async_queue_push (priv->queue, g_strdup ((gchar *) temp));
    g_hash_table_insert (priv->chunks, temp, page_buf);

    g_object_ref (transform);
    g_idle_add ((GSourceFunc) transform_chunk, transform);

    g_mutex_unlock (priv->mutex);

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
xslt_yelp_aux (xmlXPathParserContextPtr ctxt, int nargs)
{
    xsltTransformContextPtr tctxt;
    xmlXPathObjectPtr ret;
    YelpTransform *transform;
    YelpTransformPrivate *priv;

    tctxt = xsltXPathGetTransformContext (ctxt);
    transform = YELP_TRANSFORM (tctxt->_private);
    priv = GET_PRIV (transform);

    priv->aux_xslt = xsltNewDocument (tctxt, priv->aux);

    ret = xmlXPathNewNodeSet (xmlDocGetRootElement (priv->aux));
    xsltExtensionInstructionResultRegister (tctxt, ret);
    valuePush (ctxt, ret);
}
