/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2025 knuxify <knuxify@gmail.com>
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: knuxify <knuxify@gmail.com>
 */

#ifndef __YELP_SEARCH_RESULT_H__
#define __YELP_SEARCH_RESULT_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define YELP_TYPE_SEARCH_RESULT yelp_search_result_get_type()
G_DECLARE_FINAL_TYPE (YelpSearchResult, yelp_search_result, YELP, SEARCH_RESULT, GObject)

YelpSearchResult *yelp_search_result_new (void);

G_END_DECLS

enum {
    COMPLETION_FLAG_ACTIVATE_SEARCH = 1<<0,
    COMPLETION_FLAG_BOOKMARKED = 1<<1,
};

#endif /* __YELP_SEARCH_RESULT_H__ */
