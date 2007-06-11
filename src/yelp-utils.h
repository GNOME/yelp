/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003 Shaun McCance  <shaunm@gnome.org>
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

#ifndef __YELP_UTILS_H__
#define __YELP_UTILS_H__

#include <glib/gi18n.h>
#ifndef DON_UTIL
typedef struct _YelpDocInfo YelpDocInfo;

typedef enum {
    YELP_DOC_TYPE_ERROR = 0,
    YELP_DOC_TYPE_DOCBOOK_XML,
    YELP_DOC_TYPE_DOCBOOK_SGML,
    YELP_DOC_TYPE_HTML,
    YELP_DOC_TYPE_XHTML,
    YELP_DOC_TYPE_MAN,
    YELP_DOC_TYPE_INFO,
    YELP_DOC_TYPE_TOC,
    YELP_DOC_TYPE_EXTERNAL,
    YELP_DOC_TYPE_SEARCH
} YelpDocType;

typedef enum {
    YELP_URI_TYPE_ERROR    = 0,
    YELP_URI_TYPE_FILE     = 1 << 0,
    YELP_URI_TYPE_GHELP    = 1 << 1,
    YELP_URI_TYPE_MAN      = 1 << 2,
    YELP_URI_TYPE_INFO     = 1 << 3,
    YELP_URI_TYPE_TOC      = 1 << 4,
    YELP_URI_TYPE_EXTERNAL = 1 << 5,
    YELP_URI_TYPE_SEARCH   = 1 << 6,

    YELP_URI_TYPE_NO_FILE =
      YELP_URI_TYPE_GHELP    |
      YELP_URI_TYPE_MAN      |
      YELP_URI_TYPE_INFO     |
      YELP_URI_TYPE_TOC      |
      YELP_URI_TYPE_EXTERNAL |
      YELP_URI_TYPE_SEARCH,
    YELP_URI_TYPE_ANY =
      YELP_URI_TYPE_FILE    |
      YELP_URI_TYPE_NO_FILE
} YelpURIType;

#include "yelp-pager.h"

const char *        yelp_dot_dir                (void);
YelpDocInfo *       yelp_doc_info_new           (const gchar   *uri,
                                                 gboolean trust_uri);
YelpDocInfo *       yelp_doc_info_get           (const gchar   *uri,
                                                 gboolean trust_uri);
void                yelp_doc_info_add_uri       (YelpDocInfo   *doc_info,
						 const gchar   *uri,
						 YelpURIType    type);
YelpDocInfo *       yelp_doc_info_ref           (YelpDocInfo   *doc);
void                yelp_doc_info_unref         (YelpDocInfo   *doc);
void                yelp_doc_info_free          (YelpDocInfo   *doc);

YelpPager *         yelp_doc_info_get_pager     (YelpDocInfo   *doc);
void                yelp_doc_info_set_pager     (YelpDocInfo   *doc,
						 YelpPager     *pager);

const gchar *       yelp_doc_info_get_id          (YelpDocInfo   *doc);
void                yelp_doc_info_set_id          (YelpDocInfo   *doc,
						   gchar         *id);
const gchar *       yelp_doc_info_get_title       (YelpDocInfo   *doc);
void                yelp_doc_info_set_title       (YelpDocInfo   *doc,
						   gchar         *title);
const gchar *       yelp_doc_info_get_description (YelpDocInfo   *doc);
void                yelp_doc_info_set_description (YelpDocInfo   *doc,
						   gchar         *desc);
const gchar *       yelp_doc_info_get_language    (YelpDocInfo   *doc);
void                yelp_doc_info_set_language    (YelpDocInfo   *doc,
						   gchar         *language);
const gchar *       yelp_doc_info_get_category    (YelpDocInfo   *doc);
void                yelp_doc_info_set_category    (YelpDocInfo   *doc,
						   gchar         *category);
gint                yelp_doc_info_cmp_language    (YelpDocInfo   *doc1,
						   YelpDocInfo   *doc2);

YelpDocType         yelp_doc_info_get_type      (YelpDocInfo   *doc);
gchar *             yelp_doc_info_get_uri       (YelpDocInfo   *doc,
						 gchar         *frag_id,
						 YelpURIType    uri_type);
gchar *             yelp_doc_info_get_filename  (YelpDocInfo   *doc);
gboolean            yelp_doc_info_equal         (YelpDocInfo   *doc1,
						 YelpDocInfo   *doc2);

gchar *             yelp_uri_get_fragment       (const gchar   *uri);
gchar *             yelp_uri_get_relative       (gchar         *base,
						 gchar         *ref);
gchar **            yelp_get_info_paths         (void);

gchar **            yelp_get_man_paths       (void);

#else
typedef enum {
    YELP_TYPE_DOC = 0,
    YELP_TYPE_MAN,
    YELP_TYPE_INFO,
    YELP_TYPE_HTML,
    YELP_TYPE_TOC,
    YELP_TYPE_SEARCH,
    YELP_TYPE_NOT_FOUND,
    YELP_TYPE_EXTERNAL,
    YELP_TYPE_ERROR
} YelpSpoonType;


/* Generic resolver function.  Takes in the uri (which can be
 * anything) and returns the type (enum above)
 * The result is filled with a new string that the callee
 * must free, except when returning YELP_TYPE_ERROR, when it will
 * be NULL.  The result is the base filename for the document.
 * The section will be filled when the requested uri has a section
 * otherwise, it will be NULL
 * Both *result and *section must be NULL when calling (otherwise
 * we throw an error
 */
YelpSpoonType             yelp_uri_resolve        (gchar *uri, 
						   gchar **result,
						   gchar **section);
#endif

#endif /* __YELP_UTILS_H__ */
