/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2007 Shaun McCance  <shaunm@gnome.org>
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
#include <libxml/tree.h>

#include "yelp-error.h"
#include "yelp-man.h"
#include "yelp-man-parser.h"
#include "yelp-transform.h"
#include "yelp-debug.h"
#include "yelp-settings.h"

#define STYLESHEET DATADIR"/yelp/xslt/man2html.xsl"

#define YELP_MAN_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), YELP_TYPE_MAN, YelpManPriv))

typedef enum {
    MAN_STATE_BLANK,   /* Brand new, run transform as needed */
    MAN_STATE_PARSING, /* Parsing/transforming document, please wait */
    MAN_STATE_PARSED,  /* All done, if we ain't got it, it ain't here */
    MAN_STATE_STOP     /* Stop everything now, object to be disposed */
} ManState;

struct _YelpManPriv {
    gchar      *filename;
    ManState    state;

    GMutex     *mutex;
    GThread    *thread;

    xmlDocPtr   xmldoc;

    gboolean    process_running;
    gboolean    transform_running;

    YelpTransform *transform;
};

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

static void           man_class_init       (YelpManClass        *klass);
static void           man_init             (YelpMan             *man);
static void           man_try_dispose      (GObject             *object);
static void           man_dispose          (GObject             *object);

/* YelpDocument */
static void           man_request          (YelpDocument        *document,
					    gint                 req_id,
					    gboolean             handled,
					    gchar               *page_id,
					    YelpDocumentFunc     func,
					    gpointer             user_data);

/* YelpTransform */
static void           transform_func          (YelpTransform       *transform,
					       YelpTransformSignal  signal,
					       gpointer             func_data,
					       YelpMan             *man);
static void           transform_page_func     (YelpTransform       *transform,
					       gchar               *page_id,
					       YelpMan             *man);
static void           transform_final_func    (YelpTransform       *transform,
					       YelpMan             *man);

/* Threaded */
static void           man_process             (YelpMan             *man);

static YelpDocumentClass *parent_class;

GType
yelp_man_get_type (void)
{
    static GType type = 0;
    if (!type) {
	static const GTypeInfo info = {
	    sizeof (YelpManClass),
	    NULL, NULL,
	    (GClassInitFunc) man_class_init,
	    NULL, NULL,
	    sizeof (YelpMan),
	    0,
	    (GInstanceInitFunc) man_init,
	};
	type = g_type_register_static (YELP_TYPE_DOCUMENT,
				       "YelpMan", 
				       &info, 0);
    }
    return type;
}

static void
man_class_init (YelpManClass *klass)
{
    GObjectClass      *object_class   = G_OBJECT_CLASS (klass);
    YelpDocumentClass *document_class = YELP_DOCUMENT_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->dispose = man_try_dispose;

    document_class->request = man_request;
    document_class->cancel = NULL;

    g_type_class_add_private (klass, sizeof (YelpManPriv));
}

static void
man_init (YelpMan *man)
{
    YelpManPriv *priv;

    priv = man->priv = YELP_MAN_GET_PRIVATE (man);

    priv->state = MAN_STATE_BLANK;

    priv->mutex = g_mutex_new ();
}

static void
man_try_dispose (GObject *object)
{
    YelpManPriv *priv;

    g_assert (object != NULL && YELP_IS_MAN (object));
    priv = YELP_MAN (object)->priv;
    g_mutex_lock (priv->mutex);
    if (priv->process_running || priv->transform_running) {
	priv->state = MAN_STATE_STOP;
	g_idle_add ((GSourceFunc) man_try_dispose, object);
	g_mutex_unlock (priv->mutex);
    } else {
	g_mutex_unlock (priv->mutex);
	man_dispose (object);
    }
}

static void
man_dispose (GObject *object)
{
    YelpMan *man = YELP_MAN (object);

    g_free (man->priv->filename);

    if (man->priv->xmldoc)
	xmlFreeDoc (man->priv->xmldoc);

    g_mutex_free (man->priv->mutex);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/******************************************************************************/

YelpDocument *
yelp_man_new (gchar *filename)
{
    YelpMan *man;

    g_return_val_if_fail (filename != NULL, NULL);

    man = (YelpMan *) g_object_new (YELP_TYPE_MAN, NULL);
    man->priv->filename = g_strdup (filename);

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  filename = \"%s\"\n", filename);

    yelp_document_add_page_id (YELP_DOCUMENT (man), "x-yelp-index", "index");

    return (YelpDocument *) man;
}


/******************************************************************************/
/** YelpDocument **************************************************************/

static void
man_request (YelpDocument     *document,
	     gint              req_id,
	     gboolean          handled,
	     gchar            *page_id,
	     YelpDocumentFunc  func,
	     gpointer          user_data)
{
    YelpMan *man;
    YelpManPriv *priv;
    YelpError *error;

    debug_print (DB_FUNCTION, "entering\n");
    debug_print (DB_ARG, "  req_id  = %i\n", req_id);
    debug_print (DB_ARG, "  page_id = \"%s\"\n", page_id);

    g_assert (document != NULL && YELP_IS_MAN (document));

    if (handled)
	return;

    man = YELP_MAN (document);
    priv = man->priv;

    g_mutex_lock (priv->mutex);

    switch (priv->state) {
    case MAN_STATE_BLANK:
	priv->state = MAN_STATE_PARSING;
	priv->process_running = TRUE;
	priv->thread = g_thread_create ((GThreadFunc) man_process, man, FALSE, NULL);
	break;
    case MAN_STATE_PARSING:
	break;
    case MAN_STATE_PARSED:
    case MAN_STATE_STOP:
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
		YelpMan             *man)
{
    YelpManPriv *priv;

    debug_print (DB_FUNCTION, "entering\n");

    g_assert (man != NULL && YELP_IS_MAN (man));

    priv = man->priv;

    g_assert (transform == priv->transform);

    if (priv->state == MAN_STATE_STOP) {
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
	transform_page_func (transform, (gchar *) func_data, man);
	break;
    case YELP_TRANSFORM_ERROR:
	yelp_document_error_pending (YELP_DOCUMENT (man), (YelpError *) func_data);
	yelp_transform_release (transform);
	priv->transform = NULL;
	priv->transform_running = FALSE;
	break;
    case YELP_TRANSFORM_FINAL:
	transform_final_func (transform, man);
	break;
    }
}

static void
transform_page_func (YelpTransform *transform,
		     gchar         *page_id,
		     YelpMan       *man)
{
    YelpManPriv *priv;
    gchar *content;

    debug_print (DB_FUNCTION, "entering\n");

    priv = man->priv;
    g_mutex_lock (priv->mutex);

    content = yelp_transform_eat_chunk (transform, page_id);

    yelp_document_add_page (YELP_DOCUMENT (man), page_id, content);

    g_free (page_id);

    g_mutex_unlock (priv->mutex);
}

static void
transform_final_func (YelpTransform *transform, YelpMan *man)
{
    YelpError *error;
    YelpManPriv *priv = man->priv;

    debug_print (DB_FUNCTION, "entering\n");
    g_mutex_lock (priv->mutex);
    error = yelp_error_new (_("Page not found"),
			    _("The requested page was not found in the document %s."),
			    priv->filename);
    yelp_document_final_pending (YELP_DOCUMENT (man), error);

    yelp_transform_release (transform);
    priv->transform = NULL;
    priv->transform_running = FALSE;
    priv->state = MAN_STATE_PARSED;

    if (priv->xmldoc)
	xmlFreeDoc (priv->xmldoc);
    priv->xmldoc = NULL;

    g_mutex_unlock (priv->mutex);
}


/******************************************************************************/
/** Threaded ******************************************************************/

static void
man_process (YelpMan *man)
{
    YelpManPriv *priv;
    const gchar   *language;
    const gchar   *encoding;
    YelpManParser *parser;
    YelpError *error = NULL;
    YelpDocument *document;
    gint i;

    gint  params_i = 0;
    gint  params_max = 10;
    gchar **params = NULL;

    debug_print (DB_FUNCTION, "entering\n");

    g_assert (man != NULL && YELP_IS_MAN (man));
    g_object_ref (man);
    priv = man->priv;
    document = YELP_DOCUMENT (man);

    if (!g_file_test (priv->filename, G_FILE_TEST_IS_REGULAR)) {
	error = yelp_error_new (_("File not found"),
				_("The file ‘%s’ does not exist."),
				priv->filename);
	yelp_document_error_pending (document, error);
	goto done;
    }

    /* FIXME: get the language */
    language = "C";

    /* default encoding if the language doesn't match below */
    encoding = g_getenv("MAN_ENCODING");
    if (encoding == NULL)
	encoding = "ISO-8859-1";

    if (language != NULL) {
	for (i = 0; langmap[i].language != NULL; i++) {
	    if (g_str_equal (language, langmap[i].language)) {
		encoding = langmap[i].encoding;
		break;
	    }
	}
    }

    parser = yelp_man_parser_new ();
    priv->xmldoc = yelp_man_parser_parse_file (parser, priv->filename, encoding);
    yelp_man_parser_free (parser);

    if (priv->xmldoc == NULL) {
	error = yelp_error_new (_("Could not parse file"),
				_("The file ‘%s’ could not be parsed because it is"
				  " not a well-formed man page."),
				priv->filename);
	yelp_document_error_pending (document, error);
    }

    g_mutex_lock (priv->mutex);
    if (priv->state == MAN_STATE_STOP) {
	g_mutex_unlock (priv->mutex);
	goto done;
    }

    priv->transform = yelp_transform_new (STYLESHEET,
					  (YelpTransformFunc) transform_func,
					  man);
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
    g_object_unref (man);
}
