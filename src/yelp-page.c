/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2006 Brent Smith  <gnome@nextreality.net>
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
 * Authors: Brent Smith  <gnome@nextreality.net>
 *          Shaun McCance  <shaunm@gnome.org>
 */

#include <glib.h>
#include <string.h>

#include "yelp-page.h"

static GIOStatus   page_read_string  (YelpPage    *page,
				      gchar       *buffer,
				      gsize        count,
				      gsize       *bytes_read,
				      YelpError  **error);
static GIOStatus   page_read_file    (YelpPage    *page,
				      gchar       *buffer,
				      gsize        count,
				      gsize       *bytes_read,
				      YelpError  **error);

YelpPage *
yelp_page_new_string (YelpDocument  *document,
		      gchar         *id,
		      const gchar   *content,
		      YelpPageMime   mime)
{
    YelpPage *page;

    page = g_slice_new0 (YelpPage);

    page->mime = mime;

    if (document)
	page->document = g_object_ref (document);
    page->source = YELP_PAGE_SOURCE_STRING;
    page->id = g_strdup (id);

    page->content = (gchar *) content;
    page->content_len = strlen (content);

    return page;
}

gsize
yelp_page_get_length (YelpPage      *page)
{
    g_return_val_if_fail (page != NULL, 0);

    return page->content_len;
}

GIOStatus
yelp_page_read (YelpPage    *page,
		gchar       *buffer,
		gsize        count,
		gsize       *bytes_read,
		YelpError  **error)
{
    /* FIXME: set error */
    g_return_val_if_fail (page != NULL, G_IO_STATUS_ERROR);

    if (page->source == YELP_PAGE_SOURCE_STRING)
	return page_read_string (page, buffer, count, bytes_read, error);
    else
	return page_read_file (page, buffer, count, bytes_read, error);
}

static GIOStatus
page_read_string (YelpPage    *page,
		  gchar       *buffer,
		  gsize        count,
		  gsize       *bytes_read,
		  YelpError  **error)
{
    gint real_count = 0;
    g_return_val_if_fail (page != NULL, G_IO_STATUS_ERROR);

    if (count < 0) {
	real_count = (page->content_len - page->content_offset) + 1;
    } else {
	real_count = count;
    }


    if (page->content_offset == page->content_len) {
	/* FIXME: set the error */
	return G_IO_STATUS_EOF;
    }
    else if (page->content_offset > page->content_len) {
	/* FIXME: set the error */
	return G_IO_STATUS_ERROR;
    }
    else if (page->content_offset + real_count <= page->content_len) {
	strncpy (buffer, page->content + page->content_offset, count);
	page->content_offset += count;
	*bytes_read = count;
	return G_IO_STATUS_NORMAL;
    }
    else {
	strcpy (buffer, page->content + page->content_offset);
	*bytes_read = strlen (buffer);
	page->content_offset += *bytes_read;
	return G_IO_STATUS_NORMAL;
    }
}

static GIOStatus
page_read_file (YelpPage    *page,
		gchar       *buffer,
		gsize        count,
		gsize       *bytes_read,
		YelpError  **error)
{
    g_return_val_if_fail (page != NULL, G_IO_STATUS_ERROR);
    /* FIXME: just use yelp-io-channel? */
    return G_IO_STATUS_ERROR;
}

void
yelp_page_free (YelpPage *page)
{
    g_return_if_fail (page != NULL);

    if (page->document) {
	yelp_document_release_page (page->document, page);
	g_object_unref (page->document);
    }

    if (page->title)
	g_free (page->title);

    if (page->id)
	g_free (page->id);
    if (page->prev_id)
	g_free (page->prev_id);
    if (page->next_id)
	g_free (page->next_id);
    if (page->up_id)
	g_free (page->up_id);
    if (page->root_id)
	g_free (page->root_id);

    g_slice_free (YelpPage, page);
}
