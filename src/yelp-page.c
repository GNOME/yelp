/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2006 Brent Smith  <gnome@nextreality.net>
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

#include "yelp-page.h"

#include <glib.h>

typedef enum {
    PAGE_SOURCE_STRING,
    PAGE_SOURCE_FILE
} PageSource;

struct _YelpPage {
    YelpDocument *document;
    PageSource    source;

    gchar  *title;
    gchar  *content;
    gsize   content_len;
    gsize   content_offset;

    gchar  *id;
    gchar  *prev_id;
    gchar  *next_id;
    gchar  *up_id;
    gchar  *toc_id;
};

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
		      const gchar   *content)
{
    YelpPage *page;

    page = g_slice_new0 (YelpPage);

    page->document = g_object_ref (document);
    page->source = PAGE_SOURCE_STRING;
    page->id = g_strdup (id);

    page->content = content;
    page->content_len = strlen (content);

    return page;
}

GIOStatus
yelp_page_read (YelpPage    *page,
		gchar       *buffer,
		gsize        count,
		gsize       *bytes_read,
		YelpError  **error)
{
    g_return_if_fail (page != NULL);

    if (page->source == PAGE_SOURCE_STRING)
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
    gsize *bytes;

    g_return_if_fail (page != NULL);

    if (page->content_offset == page->content_len) {
	return G_IO_STATUS_EOF;
    }
    else if (page->content_offset > page->content_len) {
	// FIXME: set the error
	return G_IO_STATUS_ERROR;
    }

    strncpy (buffer, page->content + page->content_offset, bytes);
    page->content_offset += strlen (buffer);

    return G_IO_STATUS_NORMAL;
}

static GIOStatus
page_read_file (YelpPage    *page,
		gchar       *buffer,
		gsize        count,
		gsize       *bytes_read,
		YelpError  **error)
{
    //FIXME: just use yelp-io-channel?
}


const gchar *
yelp_page_get_id (YelpPage *page)
{
    g_return_if_fail (page != NULL);
    return (const gchar *) page->id;
}

const gchar *
yelp_page_get_title (YelpPage *page)
{
    g_return_if_fail (page != NULL);
    return (const gchar *) page->title;
}

const gchar *
yelp_page_get_prev_id (YelpPage *page)
{
    g_return_if_fail (page != NULL);
    return (const gchar *) page->prev_id;
}

const gchar *
yelp_page_get_next_id (YelpPage *page)
{
    g_return_if_fail (page != NULL);
    return (const gchar *) page->next_id;
}

const gchar *
yelp_page_get_up_id (YelpPage *page)
{
    g_return_if_fail (page != NULL);
    return (const gchar *) page->up_id;
}

const gchar *
yelp_page_get_toc_id   (YelpPage      *page)
{
    g_return_if_fail (page != NULL);
    return (const gchar *) page->toc_id;
}

void
yelp_page_set_title (YelpPage *page, gchar *title)
{
    g_return_if_fail (page != NULL);
    if (page->title)
	g_free (page->title);
    page->title = g_strdup (title);
}

void
yelp_page_set_prev_id (YelpPage *page, gchar *prev_id)
{
    g_return_if_fail (page != NULL);
    if (page->prev_id)
	g_free (page->prev_id);
    page->prev_id = g_strdup (prev_id);
}

void
yelp_page_set_next_id (YelpPage *page, gchar *next_id)
{
    g_return_if_fail (page != NULL);
    if (page->next_id)
	g_free (page->next_id);
    page->next_id = g_strdup (next_id);
}

void
yelp_page_set_up_id (YelpPage *page, gchar *up_id)
{
    g_return_if_fail (page != NULL);
    if (page->up_id)
	g_free (page->up_id);
    page->up_id = g_strdup (up_id);
}

void
yelp_page_set_toc_id (YelpPage *page, gchar *toc_id)
{
    g_return_if_fail (page != NULL);
    if (page->toc_id)
	g_free (page->toc_id);
    page->toc_id = g_strdup (toc_id);
}

void
yelp_page_free (YelpPage *page)
{
    g_return_if_fail (page != NULL);

    if (page->document) {
	yelp_document_release_page (document);
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
    if (page->toc_id)
	g_free (page->toc_id);

    g_free (page);
}
