/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Mikael Hallendal <micke@codefactory.se>
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
 * Author: Mikael Hallendal <micke@codefactory.se>
 */

#include <string.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnome/gnome-i18n.h>

#include "yelp-error.h"
#include "yelp-util.h"
#include "yelp-uri.h"

#define d(x) x

struct _YelpURI {
	YelpURIType  type;
	gchar       *path;
	gchar       *section;
	gint         ref_count;
};

static YelpURIType   uri_get_doc_type         (const gchar    *str_uri);
static gchar *       uri_get_doc_path         (const gchar    *str_uri,
					       GError        **error);
static gchar *       uri_get_doc_section      (const gchar    *str_uri);
static gchar *       uri_get_absolute_path    (const gchar    *path);

static YelpURIType
uri_get_doc_type (const gchar *str_uri)
{
        YelpURIType ret_val = YELP_URI_TYPE_UNKNOWN;

     	if (!strncmp (str_uri, "man:", 4)) {
                ret_val = YELP_URI_TYPE_MAN;
	}
	else if (!strncmp (str_uri, "info:", 5)) {
                ret_val = YELP_URI_TYPE_INFO;
	} 
        else if (!strncmp (str_uri, "ghelp:", 6)) {
		gchar *mime_type = NULL;
		gchar *docpath;

		docpath   = uri_get_doc_path (str_uri, NULL);
		
		if (!docpath) {
			return YELP_URI_TYPE_NON_EXISTENT;
		} 
		
		mime_type = gnome_vfs_get_mime_type (docpath);
		
		if (mime_type) {
			if (!g_strcasecmp (mime_type, "text/xml")) {
					ret_val = YELP_URI_TYPE_DOCBOOK_XML;
			}
			else if (!g_strcasecmp (mime_type, "text/sgml")) {
					ret_val = YELP_URI_TYPE_DOCBOOK_SGML;
			}
			else if (!g_strcasecmp (mime_type, "text/html")) {
				ret_val = YELP_URI_TYPE_HTML;
			}
			
			g_free (mime_type);
		}
		
		g_free (docpath);
	}

        return ret_val;
}

static gchar *
uri_get_doc_path (const gchar *str_uri, GError **error)
{
	gchar       *no_anchor_uri;
	gchar       *ret_val = NULL;
	const gchar *ch = NULL;
	
	/* remove the anchor from the doc path */
	if ((ch = strchr (str_uri, '?')) || (ch = strchr (str_uri, '#'))) {
		no_anchor_uri = g_strndup (str_uri, ch - str_uri);
	} else {
		no_anchor_uri = g_strdup (str_uri);
	}

	if (!g_ascii_strncasecmp (no_anchor_uri, "man:", 4)) {
		ret_val = g_strdup (no_anchor_uri + 4);
	}
	else if (!g_ascii_strncasecmp (no_anchor_uri, "info:", 5)) {
		ret_val = g_strdup (no_anchor_uri + 5);
	} 
	else if (!g_ascii_strncasecmp (no_anchor_uri, "ghelp:", 6)) {
		ret_val = uri_get_absolute_path (no_anchor_uri + 6);

		if (!g_file_test (ret_val, G_FILE_TEST_EXISTS)) {
			g_set_error (error,
				     YELP_ERROR,
				     YELP_ERROR_URI_NOT_EXIST,
				     _("%s does not exist."), str_uri);
			g_free (ret_val);
 			ret_val = NULL;
		}
	}
	
	g_free (no_anchor_uri);

	return ret_val;
}

static gchar *
uri_get_doc_section (const gchar *str_uri)
{
	const gchar *ch = NULL;
	
	if ((ch = strchr (str_uri, '?')) || (ch = strchr (str_uri, '#'))) {
		return g_strdup (ch + 1);
	} 
	
	return NULL;
}

static gchar *
uri_get_absolute_path (const gchar *path)
{
	gchar *ret_val = NULL;
	gchar *work_path;
	
	work_path = g_strdup (path);
	
	g_strstrip (work_path);

	if (path[0] == '/') {
		gint i = 1;
		gint len = strlen (work_path);
		
		while (i < len && work_path[i] == '/') {
			++i;
		}

		/* check_xml_promotion ? */

		ret_val = g_strdup (work_path + (i - 1));
	} else {
/* 
 * 1:  ghelp:nautilus
 * 2:  ghelp:AisleRiot2/Klondike
 */
	}
	
	g_free (work_path);

	return ret_val;
		/* ... implement jrb's uri scheme */
}

YelpURIReader *
yelp_uri_reader_new (YelpURIReaderOpenCallback  open_cb,
                     YelpURIReaderReadCallback  read_cb,
                     YelpURIReaderCloseCallback close_cb,
                     gpointer                   user_data)
{
        YelpURIReader *reader;
        
        reader = g_new0 (YelpURIReader, 1);
        
        reader->open_callback  = open_cb;
        reader->read_callback  = read_cb;
        reader->close_callback = close_cb;
        reader->user_data      = user_data;
        
        return reader;
}

YelpURI *
yelp_uri_new (const gchar *str_uri, GError **error)
{
	YelpURI *uri;
	GError  *tmp_error = NULL;

	uri = g_new0 (YelpURI, 1);

	uri->type      = uri_get_doc_type (str_uri);
	uri->path      = uri_get_doc_path (str_uri, &tmp_error);
	uri->section   = uri_get_doc_section (str_uri);
	uri->ref_count = 1;

	if (tmp_error) {
		g_propagate_error (error, tmp_error);
		g_error_free (tmp_error);
	}

	return uri;
}

YelpURIType
yelp_uri_get_type (YelpURI *uri)
{
	g_return_val_if_fail (uri != NULL, YELP_URI_TYPE_UNKNOWN);
	
	return uri->type;
}

const gchar *
yelp_uri_get_path (YelpURI *uri)
{
	g_return_val_if_fail (uri != NULL, NULL);
	
	return uri->path;
}

const gchar *
yelp_uri_get_section (YelpURI *uri)
{
	g_return_val_if_fail (uri != NULL, NULL);
	
	return uri->section;
}

gboolean
yelp_uri_read (YelpURI *uri, YelpURIReader *reader, GError **error)
{
        /* Read directly */
	return TRUE;
}

gboolean
yelp_uri_read_async (YelpURI *uri, YelpURIReader *reader, GError **error)
{
        /* For now read in a g_idle */
	return TRUE;
}

void
yelp_uri_ref (YelpURI *uri)
{
	g_return_if_fail (uri != NULL);
	
	uri->ref_count++;
}

void
yelp_uri_unref (YelpURI *uri)
{
	g_return_if_fail (uri != NULL);
	
	uri->ref_count--;

	if (uri->ref_count == 0) {
		d(g_print ("Freeing up URI\n"));
		
		g_free (uri->path);
		g_free (uri->section);
		g_free (uri);
	}
}
