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

#include "yelp-io-channel.h"
#include "yelp-man-parser.h"
#include "yelp-utils.h"

#define d(x)

#define PARSER_CUR (*(parser->cur) != '\0' \
    && (parser->cur - parser->buffer < parser->length))

static void        parser_handle_linetag    (YelpManParser *parser);
static void        parser_handle_inline     (YelpManParser *parser);
static void        parser_ensure_P          (YelpManParser *parser);
static void        parser_read_until        (YelpManParser *parser,
					     gchar          delim);
static void        parser_escape_tags       (YelpManParser *parser,
					     gchar        **tags,
					     gint           ntags);
static void        parser_append_token      (YelpManParser *parser);
static xmlNodePtr  parser_append_text       (YelpManParser *parser);
static xmlNodePtr  parser_append_given_text (YelpManParser *parser,
					     gchar         *text);
static xmlNodePtr  parser_append_node       (YelpManParser *parser,
					     gchar         *name);
static void        parser_stack_push_node   (YelpManParser *parser,
					     xmlNodePtr     node);
static xmlNodePtr  parser_stack_pop_node    (YelpManParser *parser,
					     gchar         *name);
static void        parser_parse_table       (YelpManParser *parser);

typedef struct _StackElem StackElem;
struct _YelpManParser {
    xmlDocPtr     doc;           // The top-level XML document
    xmlNodePtr    ins;           // The insertion node

    GIOChannel   *channel;       // GIOChannel for the entire document

    gchar        *buffer;        // The buffer, line at a time
    gint          length;        // The buffer length

    gchar        *anc;           // The anchor point in the document
    gchar        *cur;           // Our current position in the document

    GSList       *nodeStack;
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
    parser->channel = yelp_io_channel_new_file (file, NULL);

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

xmlDocPtr
yelp_man_parser_parse_doc (YelpManParser *parser,
			   YelpDocInfo   *doc_info)
{
    gchar     *file;
    xmlDocPtr  doc;

    g_return_val_if_fail (parser != NULL, NULL);
    g_return_val_if_fail (doc != NULL, NULL);
    g_return_val_if_fail (doc->type != YELP_DOC_TYPE_MAN, NULL);

    file = yelp_doc_info_get_filename (doc_info);

    if (!file)
	return NULL;

    doc = yelp_man_parser_parse_file (parser, file);

    g_free (file);

    return doc;
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
    xmlNodePtr tmpNode;

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

    if (g_str_equal (str, "\\\"")) {
	while (PARSER_CUR)
	    parser->anc = ++parser->cur;
    }
    else if (g_str_equal (str, "B") || g_str_equal (str, "I") ||
	     g_str_equal (str, "SM")) {
	parser_ensure_P (parser);
	parser->ins = parser_append_node (parser, str);
	g_free (str);

	parser_read_until (parser, '\n');
	parser_append_text (parser);
	parser->ins = parser->ins->parent;
    }
    else if (g_str_equal (str, "IR") || g_str_equal (str, "RI") ||
	     g_str_equal (str, "IB") || g_str_equal (str, "BI") ||
	     g_str_equal (str, "RB") || g_str_equal (str, "BR") ) {

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
    else if (g_str_equal (str, "P") || g_str_equal (str, "PP") ||
	     g_str_equal (str, "LP")) {
	parser_ensure_P (parser);
    }
    else if (g_str_equal (str, "br")) {
	parser_append_node (parser, str);
    }
    else if (g_str_equal (str, "sp")) {
	parser->ins = parser_append_node (parser, str);

	if (PARSER_CUR && *(parser->cur) != '\n') {
            parser->ins = parser_append_node (parser, "Count");
            parser_append_token (parser);
            parser->ins = parser->ins->parent;
        }

	parser->ins = parser->ins->parent;
    }
    else if (g_str_equal (str, "SH") || g_str_equal (str, "SS")) {
	gint i;
	for (i = 0; i < 6; i++) {
	    if (PARSER_CUR && *(parser->cur) != '\n') {
		parser->ins = parser_append_node (parser, str);
		parser_append_token (parser);
		parser->ins = parser->ins->parent;
	    } else break;
	}
    }
    else if (g_str_equal (str, "TH")) {
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
    else if (g_str_equal (str, "TP")) {
	g_free (parser->buffer);
	if (g_io_channel_read_line (parser->channel,
				&(parser->buffer),
				&(parser->length),
				NULL, NULL)
	    == G_IO_STATUS_NORMAL) {
	    parser->cur = parser->buffer;
            parser->anc = parser->buffer;
	    
	    parser->ins = parser_append_node (parser, "Term");
	    parser_read_until (parser, '\n');
	}
	parser->ins = parser->ins->parent;
    }
    else if (g_str_equal (str, "IP")) {
        parser->ins = parser_append_node (parser, str);
        g_free (str);

        if (PARSER_CUR && *(parser->cur) != '\n') {
            parser->ins = parser_append_node (parser, "Tag");
	    parser_read_until (parser, ' ');
            parser_append_token (parser);
            parser->ins = parser->ins->parent;
        }
	if (PARSER_CUR && *(parser->cur) != '\n') {
            parser->ins = parser_append_node (parser, "Indent");
            parser_append_token (parser);
            parser->ins = parser->ins->parent;
        }
    }
    else if (g_str_equal (str, "HP")) {
	parser->ins = parser_append_node (parser, str);
        g_free (str);

        if (PARSER_CUR && *(parser->cur) != '\n') {
            parser->ins = parser_append_node (parser, "Indent");
            parser_append_token (parser);
            parser->ins = parser->ins->parent;
        }
    }
    else if (g_str_equal (str, "RS")) {
	parser->ins = parser_append_node (parser, str);
	g_free (str);

	parser_stack_push_node (parser, parser->ins);

	if (PARSER_CUR && *(parser->cur) != '\n') {
            parser->ins = parser_append_node (parser, "Indent");
            parser_append_token (parser);
            parser->ins = parser->ins->parent;
        }
    }
    else if (g_str_equal (str, "RE")) {
	tmpNode = parser_stack_pop_node (parser, "RS");

	if (tmpNode == NULL)
	    g_warning ("Found unexpected tag: '%s'\n", str);
	else
	    parser->ins = tmpNode;

	g_free (str);
    }
    else if (g_str_equal (str, "UR")) {
	parser->ins = parser_append_node (parser, str);
        g_free (str);

	parser_stack_push_node (parser, parser->ins);

        if (PARSER_CUR && *(parser->cur) != '\n') {
            parser->ins = parser_append_node (parser, "URI");
            parser_append_token (parser);
            parser->ins = parser->ins->parent;
        }
    }
    else if (g_str_equal (str, "UE")) {
        tmpNode = parser_stack_pop_node (parser, "RS");

        if (tmpNode == NULL)
            g_warning ("Found unexpected tag: '%s'\n", str);
        else
            parser->ins = tmpNode;

        g_free (str);
    }
    else if (g_str_equal (str, "UN")) {
	parser->ins = parser_append_node (parser, str);
        g_free (str);

	parser_append_token (parser);
	parser->ins = parser->ins->parent;
    }

    /* Table (tbl) macros */
    else if (g_str_equal (str, "TS")) {
	parser->ins = parser_append_node (parser, "TABLE");
        g_free (str);

	parser_stack_push_node (parser, parser->ins);
	g_free (parser->buffer);
	parser_parse_table (parser);
    }
    else if (g_str_equal (str, "TE")) {
	/* We should only see this from within parser_parse_table */
	g_warning ("Found unexpected tag: '%s'\n", str);
        g_free (str);
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
    case '\\':
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

	if (g_str_equal (str, "fI") || g_str_equal (str, "fB"))
	    parser->ins = parser_append_node (parser, str);
	else if (!g_str_equal (str, "fR") && !g_str_equal (str, "fP"))
	    g_warning ("No rule matching the tag '%s'\n", str);

	g_free (str);
	parser->anc = parser->cur;
	break;
    case '(':
	parser->cur++;
	if (!PARSER_CUR) break;
	parser->cur++;
	if (!PARSER_CUR) break;
	parser->cur++;

	c = *(parser->cur);
	*(parser->cur) = '\0';
	str = g_strdup (parser->anc);
	*(parser->cur) = c;

	if (g_str_equal (str, "(co"))
	    parser_append_given_text (parser, "©");
	else if (g_str_equal (str, "(bu"))
	    parser_append_given_text (parser, "•");
	else if (g_str_equal (str, "(em"))
	    parser_append_given_text (parser, "—");

	g_free (str);
	parser->anc = parser->cur;
        break;
    case '*':
	parser->cur++;
	if (!PARSER_CUR) break;

	if (*(parser->cur) == 'R') {
	    parser_append_given_text (parser, "®");
	    parser->cur++;
	} else if (*(parser->cur) == '(') {
	    parser->cur++;
	    if (!PARSER_CUR) break;
	    parser->cur++;
	    if (!PARSER_CUR) break;
	    parser->cur++;
	    
	    c = *(parser->cur);
	    *(parser->cur) = '\0';
	    str = g_strdup (parser->anc);
	    *(parser->cur) = c;

	    if (g_str_equal (str, "*(Tm"))
		parser_append_given_text (parser, "™");
	    else if (g_str_equal (str, "*(lq"))
		parser_append_given_text (parser, "“");
	    else if (g_str_equal (str, "*(rq"))
		parser_append_given_text (parser, "”");

	    g_free (str);
	}
	
	parser->cur++;
	parser->anc = parser->cur;
	break;
    case 'e':
	parser->cur++;
	parser->anc = parser->cur;
	parser_append_given_text (parser, "\\");
	break;
    case '&':
	parser->cur++;
	parser->anc = parser->cur;
	break;
    default:
	parser->cur++;

	c = *(parser->cur);
	*(parser->cur) = '\0';
	str = g_strdup (parser->anc);
	*(parser->cur) = c;

	g_warning ("No rule matching the inline tag '%s'\n", str);

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

    c = *(parser->cur);
    *(parser->cur) = '\0';

    if (*(parser->anc) != '\n')
	parser_ensure_P (parser);

    node = xmlNewText (BAD_CAST parser->anc);
    xmlAddChild (parser->ins, node);

    *(parser->cur) = c;

    parser->anc = parser->cur;

    return node;
}

static xmlNodePtr
parser_append_given_text (YelpManParser *parser,
			  gchar         *text)
{
    xmlNodePtr  node;

    parser_ensure_P (parser);

    node = xmlNewText (BAD_CAST text);
    xmlAddChild (parser->ins, node);

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

static void        
parser_stack_push_node (YelpManParser *parser,
			xmlNodePtr     node)
{
    parser->nodeStack = g_slist_prepend (parser->nodeStack, node);
}

static xmlNodePtr  
parser_stack_pop_node (YelpManParser *parser,
		       gchar         *name)
{
    xmlNodePtr popped;

    if (parser->nodeStack == NULL)
	return NULL;
   
    popped = (xmlNodePtr) parser->nodeStack->data;
    
    if (!g_str_equal (name, popped->name))
	return NULL;
	
    parser->nodeStack = g_slist_remove (parser->nodeStack, popped);
    return popped;
}

/*
 *  Table (tbl) macro package parsing
 */

static void
parser_handle_table_options (YelpManParser *parser)
{
    /* FIXME: do something with the options */
    g_free (parser->buffer);

    return;
}

static void
parser_handle_row_options (YelpManParser *parser)
{
    /* FIXME: do something with these options */

    do {
	parser->anc = parser->buffer;
	parser->cur = parser->buffer;
	
	parser_read_until (parser, '.');
	
	
	if (*(parser->cur) == '.') {
	    g_free (parser->buffer);
	    break;
	}
	
	g_free (parser->buffer);

    } while (g_io_channel_read_line (parser->channel,
				     &(parser->buffer),
				     &(parser->length),
				     NULL, NULL)
	     == G_IO_STATUS_NORMAL);
}

static void
parser_parse_table (YelpManParser *parser)
{
    xmlNodePtr table_start;

    table_start = parser->ins;

    if (g_io_channel_read_line (parser->channel,
				&(parser->buffer),
				&(parser->length),
				NULL, NULL)
	== G_IO_STATUS_NORMAL) {
	parser->anc = parser->buffer;
	parser->cur = parser->buffer;
	
	parser_read_until (parser, ';');

	if (*(parser->cur) == ';') {
	    parser_handle_table_options (parser);

	    if (g_io_channel_read_line (parser->channel,
					&(parser->buffer),
					&(parser->length),
					NULL, NULL)
		== G_IO_STATUS_NORMAL) {
		parser->anc = parser->buffer;
		parser->cur = parser->buffer;
	    
		parser_read_until (parser, '\n');
	    } else
		return;
	}

	parser_handle_row_options (parser);

	/* Now this is where we go through all the rows */
	while (g_io_channel_read_line (parser->channel,
				       &(parser->buffer),
				       &(parser->length),
				       NULL, NULL)
	       == G_IO_STATUS_NORMAL) {
	    
	    parser->anc = parser->buffer;
	    parser->cur = parser->buffer;
	    
	    switch (*(parser->buffer)) {
	    case '.':
		if (*(parser->buffer + 1) == 'T'
		    && *(parser->buffer + 2) == 'E') {
		    if (parser_stack_pop_node (parser, "TABLE") == NULL)
			g_warning ("Found unexpected tag: 'TE'\n");
		    else {
			parser->ins = table_start;
		    
			parser->anc = parser->buffer + 3;
			parser->cur = parser->buffer + 3;
			return;
		    }
		} else {
		    parser_handle_linetag (parser);
		    break;
		}
	    case '\n':
		break;
	    default:
		break;
	    }
	    
	    parser->ins = parser_append_node (parser, "ROW");
	    while (PARSER_CUR && *(parser->cur) != '\n') {
		parser_read_until (parser, '\t');
		parser->ins = parser_append_node (parser, "CELL");
		parser_append_text (parser);
		parser->ins = parser->ins->parent;
		parser->anc++;
		parser->cur++;
	    }
	    g_free (parser->buffer);

	    parser->ins = table_start;
	}
    }
}
