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
 * Author: Brent Smith  <gnome@nextreality.net>
 */

#include "yelp-page.h"

#include <glib.h>

struct _YelpPage {
	gchar *page_id;
	gchar *title;
	gchar *contents;
	gchar *prev_id;
	gchar *next_id;
	gchar *toc_id;
};


YelpPage *
yelp_page_new (void)
{
	YelpPage *page;

	page = g_slice_new0 (YelpPage);

	return page;
}

void 
yelp_page_set_id (YelpPage *page, const gchar *page_id)
{
	g_return_if_fail (page != NULL);
	if (page_id == NULL)
		return;

	if (page->page_id)
		g_free (page->page_id);

	page->page_id = g_strdup (page_id);

	return;
}

const gchar *
yelp_page_get_id (YelpPage *page)
{
	g_return_if_fail (page != NULL);

	return (const gchar *) page->page_id;
}

void 
yelp_page_set_title (YelpPage *page, const gchar *title)
{
	g_return_if_fail (page != NULL);
	if (title == NULL)
		return;

	if (page->title)
		g_free (page->title);

	page->title = g_strdup (title);

	return;
}

const gchar *
yelp_page_get_title (YelpPage *page)
{
	g_return_if_fail (page != NULL);

	return (const gchar *)page->title;
}

void 
yelp_page_set_contents (YelpPage *page, const gchar *contents)
{
	g_return_if_fail (page != NULL);
	if (contents == NULL)
		return;

	if (page->contents)
		g_free (page->contents);

	page->contents = g_strdup (contents);

	return;
}

const gchar *
yelp_page_get_contents (YelpPage *page)
{
	g_return_if_fail (page != NULL);

	return (const gchar *)page->contents;
}

void 
yelp_page_set_prev_id (YelpPage *page, const gchar *prev_id)
{
	g_return_if_fail (page != NULL);
	if (prev_id == NULL)
		return;

	if (page->prev_id)
		g_free (page->prev_id);

	page->prev_id = g_strdup (prev_id);

	return;
}

const gchar *
yelp_page_get_prev_id (YelpPage *page)
{
	g_return_if_fail (page != NULL);

	return (const gchar *)page->prev_id;
}

void 
yelp_page_set_next_id (YelpPage *page, const gchar *next_id)
{
	g_return_if_fail (page != NULL);
	if (next_id == NULL)
		return;

	if (page->next_id)
		g_free (page->next_id);

	page->next_id = g_strdup (next_id);

	return;
}

const gchar *
yelp_page_get_next_id (YelpPage *page)
{
	g_return_if_fail (page != NULL);

	return (const gchar *)page->next_id;
}

void 
yelp_page_set_toc_id (YelpPage *page, const gchar *toc_id)
{
	g_return_if_fail (page != NULL);
	if (toc_id == NULL)
		return;

	if (page->toc_id)
		g_free (page->toc_id);

	page->toc_id = g_strdup (toc_id);

	return;
}

const gchar *
yelp_page_get_toc_id (YelpPage *page)
{
	g_return_if_fail (page != NULL);

	return (const gchar *)page->toc_id;
}

void
yelp_page_free (YelpPage *page)
{
	if (page == NULL)
		return;

	g_slice_free (YelpPage, page);
}
