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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include <libxml/tree.h>
#include <string.h>

#include "yelp-man-parser.h"

#define d(x)

#define PARSER_CUR (*(parser->cur) != '\0' \
    && (parser->cur - parser->buffer < parser->length))

static void        parser_handle_linetag (YelpManParser *parser);
static void        parser_handle_inline  (YelpManParser *parser);
static void        parser_ensure_P       (YelpManParser *parser);
static void        parser_read_until     (YelpManParser *parser,
					  gchar          delim);
static void        parser_escape_tags    (YelpManParser *parser,
					  gchar        **tags,
					  gint           ntags);
static void        parser_append_token   (YelpManParser *parser);
static xmlNodePtr  parser_append_text    (YelpManParser *parser);
static xmlNodePtr  parser_append_node    (YelpManParser *parser,
					  gchar         *name);

typedef struct _StackElem StackElem;
struct _YelpManParser {
    xmlDocPtr     doc;           // The top-level XML document
    xmlNodePtr    ins;           // The insertion node

    GIOChannel   *channel;       // GIOChannel for the entire document

    gchar        *buffer;        // The buffer, line at a time
    gint          length;        // The buffer length

    gchar        *anc;           // The anchor point in the document
    gchar        *cur;           // Our current position in the document
};

YelpManParser *
yelp_man_parser_new (void)
{
    YelpManParser *parser = g_new0 (YelpManParser, 1);

    return parser;
}

xmlDocPtr
yelp_man_parser_parse_file (YelpManParser   *parser,
			    gchar           *file)
{
    parser->channel = g_io_channel_new_file (file, "r", NULL);

    if (!parser->channel)
	return NULL;

    parser->doc = xmlNewDoc ("1.0");
    parser->ins = xmlNewNode (NULL, BAD_CAST "Man");
    xmlDocSetRootElement (parser->doc, parser->ins);

    while (g_io_channel_read_line (parser->channel,
				   &(parser->buffer),
				   &(parser->length),
				   NULL, NULL)
	   == G_IO_STATUS_NORMAL) {

	parser->anc = parser->buffer;
	parser->cur = parser->buffer;

	switch (*(parser->buffer)) {
	case '.':
	    parser_handle_linetag (parser);
	    break;
	case '\n':
	    parser->ins = xmlDocGetRootElement (parser->doc);
	    break;
	default:
	    break;
	}

	parser_read_until (parser, '\n');

	if (parser->cur != parser->anc)
	    parser_append_text (parser);

	if (PARSER_CUR) {
	    parser->cur++;
	    parser_append_text (parser);
	}

	g_free (parser->buffer);
    }

    g_io_channel_shutdown (parser->channel, FALSE, NULL);

    return parser->doc;
}

void
yelp_man_parser_free (YelpManParser *parser)
{
    g_io_channel_unref (parser->channel);
    g_free (parser);
}

/******************************************************************************/

static void
parser_handle_linetag (YelpManParser *parser) {
    gchar c, *str;

    while (PARSER_CUR
	   && *(parser->cur) != ' '
	   && *(parser->cur) != '\n')
	parser->cur++;

    c = *(parser->cur);
    *(parser->cur) = '\0';

    str = g_strdup (parser->anc + 1);
    *(parser->cur) = c;

    if (*(parser->cur) == ' ')
	parser->cur++;
    parser->anc = parser->cur;

    if (!strcmp (str, "\\\"")) {
	while (PARSER_CUR)
	    parser->anc = ++parser->cur;
    }
    else if (!strcmp (str, "B") || !strcmp (str, "I") ||
	     !strcmp (str, "SM")) {
	parser_ensure_P (parser);
	parser->ins = parser_append_node (parser, str);
	g_free (str);

	parser_append_token (parser);
	parser->ins = parser->ins->parent;
    }
    else if (!strcmp (str, "IR") || !strcmp (str, "RI") ||
	     !strcmp (str, "IB") || !strcmp (str, "BI") ||
	     !strcmp (str, "RB") || !strcmp (str, "BR") ) {

	gint  i;
	gchar a[2], b[2];
	a[0] = str[0]; b[0] = str[1]; a[1] = b[1] = '\0';

	parser_ensure_P (parser);

	for (i = 0; i < 6; i++) {
	    if (PARSER_CUR && *(parser->cur) != '\n') {
		parser->ins = parser_append_node (parser, a);
		parser_append_token (parser);
		parser->ins = parser->ins->parent;
	    } else break;
	    if (PARSER_CUR && *(parser->cur) != '\n') {
		parser->ins = parser_append_node (parser, b);
		parser_append_token (parser);
		parser->ins = parser->ins->parent;
	    } else break;
	}
    }
    else if (!strcmp (str, "P") || !strcmp (str, "PP")) {
	parser->ins = xmlDocGetRootElement (parser->doc);
	parser_ensure_P (parser);
    }
    else if (!strcmp (str, "SH") || !strcmp (str, "SS")) {
	gint i;
	for (i = 0; i < 6; i++) {
	    if (PARSER_CUR && *(parser->cur) != '\n') {
		parser->ins = xmlDocGetRootElement (parser->doc);
		parser->ins = parser_append_node (parser, str);
		parser_append_token (parser);
		parser->ins = parser->ins->parent;
	    } else break;
	}
    }
    else if (!strcmp (str, "TH")) {
	parser->ins = xmlDocGetRootElement (parser->doc);
	parser->ins = parser_append_node (parser, str);

	if (PARSER_CUR && *(parser->cur) != '\n') {
	    parser->ins = parser_append_node (parser, "Title");
	    parser_append_token (parser);
	    parser->ins = parser->ins->parent;
	}
	if (PARSER_CUR && *(parser->cur) != '\n') {
	    parser->ins = parser_append_node (parser, "Section");
	    parser_append_token (parser);
	    parser->ins = parser->ins->parent;
	}
	if (PARSER_CUR && *(parser->cur) != '\n') {
	    parser->ins = parser_append_node (parser, "Date");
	    parser_append_token (parser);
	    parser->ins = parser->ins->parent;
	}
	if (PARSER_CUR && *(parser->cur) != '\n') {
	    parser->ins = parser_append_node (parser, "Commentary");
	    parser_append_token (parser);
	    parser->ins = parser->ins->parent;
	}
	if (PARSER_CUR && *(parser->cur) != '\n') {
	    parser->ins = parser_append_node (parser, "Name");
	    parser_append_token (parser);
	    parser->ins = parser->ins->parent;
	}

	parser->ins = parser->ins->parent;
    }
    else if (!strcmp (str, "TP")) {
	parser->ins = xmlDocGetRootElement (parser->doc);
	g_free (parser->buffer);
	if (g_io_channel_read_line (parser->channel,
				&(parser->buffer),
				&(parser->length),
				NULL, NULL)
	    == G_IO_STATUS_NORMAL) {

	    parser->ins = parser_append_node (parser, "Term");
	    parser_read_until (parser, '\n');
	}
    }
    else {
	g_warning ("No rule matching the tag '%s'\n", str);
    }
}

static void
parser_ensure_P (YelpManParser *parser)
{
    if (!xmlStrcmp (parser->ins->name, BAD_CAST "Man"))
	parser->ins = parser_append_node (parser, "P");
}

static void
parser_read_until (YelpManParser *parser,
		   gchar          delim)
{
    while (PARSER_CUR
	   && *(parser->cur) != '\n'
	   && *(parser->cur) != delim)
	if (*(parser->cur) == '\\')
	    parser_handle_inline (parser);
	else
	    parser->cur++;
}

static void
parser_escape_tags (YelpManParser *parser,
		    gchar        **tags,
		    gint           ntags)
{
    gint i;
    xmlNodePtr node = NULL;
    xmlNodePtr cur  = parser->ins;
    GSList *path = NULL;

    // Find the top node we can escape from
    while (cur->parent != (xmlNodePtr) parser->doc) {
	for (i = 0; i < ntags; i++)
	    if (!xmlStrcmp (cur->name, BAD_CAST tags[i])) {
		node = cur;
		break;
	    }
	path = g_slist_prepend (path, cur);
	cur = cur->parent;
    }

    // Walk back down, reproducing nodes we aren't escaping
    if (node) {
	GSList *c = path;
	while (c && (xmlNodePtr) c->data != node)
	    c = g_slist_next (c);

	parser->ins = node->parent;
	parser_ensure_P (parser);

	while ((c = c->next)) {
	    gboolean insert = TRUE;
	    cur = (xmlNodePtr) c->data;

	    for (i = 0; i < ntags; i++)
		if (!xmlStrcmp (cur->name, BAD_CAST tags[i])) {
		    insert = FALSE;
		    break;
		}
	    if (insert)
		parser->ins = parser_append_node (parser, (gchar *) cur->name);
	}
    }
}

static void
parser_append_token (YelpManParser *parser)
{
    while (*(parser->cur) == ' ')
	parser->anc = ++parser->cur;

    if (*(parser->cur) == '"') {
	parser->anc = ++parser->cur;
	parser_read_until (parser, '"');
    } else {
	parser_read_until (parser, ' ');
    }

    parser_append_text (parser);

    if (*(parser->cur) == '"')
	parser->anc = ++parser->cur;
}

static void
parser_handle_inline (YelpManParser *parser)
{
    gchar c, *str;
    gchar **escape;

    parser_append_text (parser);
    parser->anc = ++parser->cur;

    switch (*(parser->cur)) {
    case '\0':
	break;
    case '-':
	parser->cur++;
	parser_append_text (parser);
	parser->anc = parser->cur;
	break;
    case 'f':
	parser->cur++;
	if (!PARSER_CUR) break;
	parser->cur++;

	c = *(parser->cur);
	*(parser->cur) = '\0';
	str = g_strdup (parser->anc);
	*(parser->cur) = c;

	escape = g_new0 (gchar *, 2);
	escape[0] = "fB";
	escape[1] = "fI";
	parser_escape_tags (parser, escape, 2);
	g_free (escape);

	if (!strcmp (str, "fI") || !strcmp (str, "fB"))
	    parser->ins = parser_append_node (parser, str);
	else if (strcmp (str, "fR"))
	    g_warning ("No rule matching the tag '%s'\n", str);

	g_free (str);
	parser->anc = parser->cur;
	break;
    default:
	parser->cur++;

	c = *(parser->cur);
	*(parser->cur) = '\0';
	str = g_strdup (parser->anc);
	*(parser->cur) = c;

	g_warning ("No rule matching the tag '%s'\n", str);

	g_free (str);
	parser->anc--;
	break;
    }
}

static xmlNodePtr
parser_append_text (YelpManParser *parser)
{
    xmlNodePtr  node;
    gchar       c;

    if (parser->anc == parser->cur)
	return NULL;

    parser_ensure_P (parser);

    c = *(parser->cur);
    *(parser->cur) = '\0';

    node = xmlNewText (BAD_CAST parser->anc);
    xmlAddChild (parser->ins, node);

    *(parser->cur) = c;

    parser->anc = parser->cur;

    return node;
}

static xmlNodePtr
parser_append_node (YelpManParser *parser,
		    gchar         *name)
{
    xmlNodePtr node;

    node = xmlNewChild (parser->ins, NULL, name, NULL);

    return node;
}
