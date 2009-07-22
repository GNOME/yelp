/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2007 Don Scorgie <dscorgie@svn.gnome.org>
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
 * Author: Don Scorgie <dscorgie@svn.gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libxml/tree.h>

#include "yelp-error.h"
#include "yelp-info.h"
#include "yelp-info-parser.h"
#include "yelp-transform.h"
#include "yelp-debug.h"
#include "yelp-settings.h"

#define STYLESHEET DATADIR"/yelp/xslt/info2html.xsl"

#define YELP_INFO_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_INFO, YelpInfoPriv))

typedef enum {
    INFO_STATE_BLANK,   /* Brand new, run transform as needed */
    INFO_STATE_PARSING, /* Parsing/transforming document, please wait */
    INFO_STATE_PARSED,  /* All done, if we ain't got it, it ain't here */
    INFO_STATE_STOP     /* Stop everything now, object to be disposed */
} InfoState;

struct _YelpInfoPriv {
    gchar      *filename;
    InfoState    state;

    GMutex     *mutex;
    GThread    *thread;

    xmlDocPtr   xmldoc;
    GtkTreeModel  *sections;

    gboolean    process_running;
    gboolean    transform_running;

    YelpTransform *transform;
};


static void           info_class_init       (YelpInfoClass        *klass);
static void           info_init             (YelpInfo             *info);
static void           info_try_dispose      (GObject             *object);
static void           info_dispose          (GObject             *object);

/* YelpDocument */
static void           info_request          (YelpDocument        *document,
					    gint                 req_id,
					    gboolean             handled,
					    gchar               *page_id,
					    YelpDocumentFunc     func,
					    gpointer             user_data);

/* YelpTransform */
static void           transform_func          (YelpTransform       *transform,
					       YelpTransformSignal  signal,
					       gpointer             func_data,
					       YelpInfo             *info);
static void           transform_page_func     (YelpTransform       *transform,
					       gchar               *page_id,
					       YelpInfo             *info);
static void           transform_final_func    (YelpTransform       *transform,
					       YelpInfo             *info);
static gpointer       info_get_sections       (YelpDocument        *document);

/* Threaded */
static void           info_process             (YelpInfo             *info);

static YelpDocumentClass *parent_class;

GType
yelp_info_get_type (void)
{
    static GType type = 0;
    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpInfoClass),
	    NULL, NULL,
	    (GClassInitFunc) info_class_init,
	    NULL, NULL,
	    sizeof (YelpInfo),
	    0,
	    (GInstanceInitFunc) info_init,
	};
	type = g_type_register_static (YELP_TYPE_DOCUMENT,
				       "YelpInfo", 
				       &info, 0);
    }
    return type;
}

static void
info_class_init (YelpInfoClass *klass)
{
    GObjectClass      *object_class   = G_OBJECT_CLASS (klass);
    YelpDocumentClass *document_class = YELP_DOCUMENT_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = info_try_dispose;

    document_class->request = info_request;
    document_class->cancel = NULL;
    document_class->get_sections = info_get_sections;

    g_type_class_add_private (klass, sizeof (YelpInfoPriv));
}

static void
info_init (YelpInfo *info)
{
    YelpInfoPriv *priv;

    priv = info->priv = YELP_INFO_GET_PRIVATE (info);

    priv->state = INFO_STATE_BLANK;

    priv->xmldoc = NULL;

    priv->mutex = g_mutex_new ();
}

static void
info_try_dispose (GObject *object)
{
    YelpInfoPriv *priv;

    g_assert (object != NULL && YELP_IS_INFO (object));
    priv = YELP_INFO (object)->priv;

    g_mutex_lock (priv->mutex);
    if (priv->process_running || priv->transform_running) {
	priv->state = INFO_STATE_STOP;
	g_idle_add ((GSourceFunc) info_try_dispose, object);
	g_mutex_unlock (priv->mutex);
    } else {
	g_mutex_unlock (priv->mutex);
	info_dispose (object);
    }
}

static void
info_dispose (GObject *object)
{
    YelpInfo *info = YELP_INFO (object);

    g_free (info->priv->filename);

    if (info->priv->xmldoc)
	xmlFreeDoc (info->priv->xmldoc);

    g_mutex_free (info->priv->mutex);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

YelpDocument *
yelp_info_new (gchar *filename)
{
    YelpInfo *info;

    g_return_val_if_fail (filename != NULL, NULL);

    info = (YelpInfo *) g_object_new (YELP_TYPE_INFO, NULL);
    info->priv->filename = g_strdup (filename);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  filename = \"%s\"\n", filename);

    yelp_document_add_page_id (YELP_DOCUMENT (info), "x-yelp-index", "index");

    return (YelpDocument *) info;
}


/******************************************************************************/
/** YelpDocument **************************************************************/

static void
info_request (YelpDocument     *document,
	     gint              req_id,
	     gboolean          handled,
	     gchar            *page_id,
	     YelpDocumentFunc  func,
	     gpointer          user_data)
{
    YelpInfo *info;
    YelpInfoPriv *priv;
    YelpError *error;

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  req_id  = %i\n", req_id);
    debug_print (DB_ARG, "  page_id = \"%s\"\n", page_id);

    g_assert (document != NULL && YELP_IS_INFO (document));

    if (handled)
	return;

    info = YELP_INFO (document);
    priv = info->priv;

    g_mutex_lock (priv->mutex);

    switch (priv->state) {
    case INFO_STATE_BLANK:
	priv->state = INFO_STATE_PARSING;
	priv->process_running = TRUE;
	priv->thread = g_thread_create ((GThreadFunc) info_process, info, FALSE, NULL);
	break;
    case INFO_STATE_PARSING:
	break;
    case INFO_STATE_PARSED:
    case INFO_STATE_STOP:
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
		YelpInfo             *info)
{
    YelpInfoPriv *priv;

    debug_print (DB_FUNCTION, "entering\n");

    g_assert (info != NULL && YELP_IS_INFO (info));

    priv = info->priv;

    g_assert (transform == priv->transform);

    if (priv->state == INFO_STATE_STOP) {
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
	transform_page_func (transform, (gchar *) func_data, info);
	break;
    case YELP_TRANSFORM_ERROR:
	yelp_document_error_pending (YELP_DOCUMENT (info), (YelpError *) func_data);
	yelp_transform_release (transform);
	priv->transform = NULL;
	priv->transform_running = FALSE;
	break;
    case YELP_TRANSFORM_FINAL:
	transform_final_func (transform, info);
	break;
    }
}

static void
transform_page_func (YelpTransform *transform,
		     gchar         *page_id,
		     YelpInfo       *info)
{
    YelpInfoPriv *priv;
    gchar *content;

    debug_print (DB_FUNCTION, "entering\n");

    priv = info->priv;
    g_mutex_lock (priv->mutex);

    content = yelp_transform_eat_chunk (transform, page_id);

    yelp_document_add_page (YELP_DOCUMENT (info), page_id, content);

    g_free (page_id);

    g_mutex_unlock (priv->mutex);
}

static void
transform_final_func (YelpTransform *transform, YelpInfo *info)
{
    YelpError *error;
    YelpInfoPriv *priv = info->priv;

    debug_print (DB_FUNCTION, "entering\n");

    g_mutex_lock (priv->mutex);

    error = yelp_error_new (_("Page not found"),
			    _("The requested page was not found in the document %s."),
			    priv->filename);
    yelp_document_final_pending (YELP_DOCUMENT (info), error);

    yelp_transform_release (transform);
    priv->transform = NULL;
    priv->transform_running = FALSE;
    priv->state = INFO_STATE_PARSED;

    if (priv->xmldoc)
	xmlFreeDoc (priv->xmldoc);
    priv->xmldoc = NULL;

    g_mutex_unlock (priv->mutex);
}


/******************************************************************************/
/** Threaded ******************************************************************/

static void
info_process (YelpInfo *info)
{
    YelpInfoPriv *priv;
    YelpError *error = NULL;
    YelpDocument *document;
    GtkTreeModel *model;

    gint  params_i = 0;
    gint  params_max = 10;
    gchar **params = NULL;


    debug_print (DB_FUNCTION, "entering\n");

    g_assert (info != NULL && YELP_IS_INFO (info));
    g_object_ref (info);
    priv = info->priv;
    document = YELP_DOCUMENT (info);

    if (!g_file_test (priv->filename, G_FILE_TEST_IS_REGULAR)) {
	error = yelp_error_new (_("File not found"),
				_("The file ‘%s’ does not exist."),
				priv->filename);
	yelp_document_error_pending (document, error);
	goto done;
    }

    priv->sections = (GtkTreeModel  *) yelp_info_parser_parse_file (priv->filename);
    if (!model) {
	/* TODO: Handle errors - exit out somehow */
    }

    priv->xmldoc = yelp_info_parser_parse_tree ((GtkTreeStore *) priv->sections);

    if (priv->xmldoc == NULL) {
	error = yelp_error_new (_("Could not parse file"),
				_("The file ‘%s’ could not be parsed because it is"
				  " not a well-formed info page."),
				priv->filename);
	yelp_document_error_pending (document, error);
    }

    g_mutex_lock (priv->mutex);
    if (priv->state == INFO_STATE_STOP) {
	g_mutex_unlock (priv->mutex);
	goto done;
    }

    priv->transform = yelp_transform_new (STYLESHEET,
					  (YelpTransformFunc) transform_func,
					  info);
    priv->transform_running = TRUE;
    
    params = g_new0 (gchar *, params_max);
    yelp_settings_params (&params, &params_i, &params_max);

    params[params_i] = NULL;


    yelp_transform_start (priv->transform,
			  priv->xmldoc,
			  params);
    g_strfreev (params);
    g_mutex_unlock (priv->mutex);

 done:
    priv->process_running = FALSE;
    g_object_unref (info);
}

static gpointer
info_get_sections (YelpDocument *document)
{
    YelpInfo *info = (YelpInfo *) document;
    
    return (gpointer) (info->priv->sections);
}
