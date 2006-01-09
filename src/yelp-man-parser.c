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
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <libxml/tree.h>
#include <string.h>

#include "yelp-io-channel.h"
#include "yelp-man-parser.h"
#include "yelp-utils.h"

#define d(x)

#define PARSER_CUR (*(parser->cur) != '\0' \
    && (parser->cur - parser->buffer < parser->length))

static void        parser_parse_line        (YelpManParser *parser);
static void        parser_handle_linetag    (YelpManParser *parser);
static void        parser_ensure_P          (YelpManParser *parser);
static void        parser_read_until        (YelpManParser *parser,
					     gchar          delim);
static void        parser_escape_tags       (YelpManParser *parser,
					     gchar        **tags,
					     gint           ntags);
static xmlNodePtr  parser_append_text       (YelpManParser *parser);
static xmlNodePtr  parser_append_given_text (YelpManParser *parser,
					     gchar         *text);
static void        parser_append_given_text_handle_escapes 
					    (YelpManParser *parser,
					     gchar         *text,
					     gboolean      make_links);
static xmlNodePtr  parser_append_node       (YelpManParser *parser,
					     gchar         *name);
static xmlNodePtr  parser_append_node_attr  (YelpManParser *parser,
					     gchar         *name,
					     gchar         *attr,
					     gchar         *value);
static void        parser_stack_push_node   (YelpManParser *parser,
					     xmlNodePtr     node);
static xmlNodePtr  parser_stack_pop_node    (YelpManParser *parser,
					     gchar         *name);
static void        parser_parse_table       (YelpManParser *parser);

typedef struct _StackElem StackElem;
struct _YelpManParser {
    xmlDocPtr     doc;           /* The top-level XML document */
    xmlNodePtr    ins;           /* The insertion node */

    GIOChannel   *channel;       /* GIOChannel for the entire document */

    gchar        *buffer;        /* The buffer, line at a time */
    gint          length;        /* The buffer length */

    gchar        *anc;           /* The anchor point in the document */
    gchar        *cur;           /* Our current position in the document */

    gchar        *token;         /* see ignore flag; we ignore the parsing stream until
				  * this string is found in the stream */
    gboolean      make_links;    /* Allow auto-generated hyperlinks to be disabled. */
    gboolean      ignore;        /* when true, ignore stream until "token" is found  */
	
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
    GError **errormsg = NULL;
	
    parser->channel = yelp_io_channel_new_file (file, NULL);

    if (!parser->channel)
	return NULL;

    parser->doc = xmlNewDoc (BAD_CAST "1.0");
    parser->ins = xmlNewNode (NULL, BAD_CAST "Man");
	xmlDocSetRootElement (parser->doc, parser->ins);

    parser->make_links = TRUE;

    while (g_io_channel_read_line (parser->channel,
				   &(parser->buffer),
				   (gsize *) &(parser->length),
				   NULL, errormsg)
	   == G_IO_STATUS_NORMAL) {

	parser_parse_line (parser);

	g_free (parser->buffer);
    }

    if (errormsg)
	    g_print ("Error in g_io_channel_read_line()\n");

    g_io_channel_shutdown (parser->channel, FALSE, NULL);

    return parser->doc;
}

xmlDocPtr
yelp_man_parser_parse_doc (YelpManParser *parser,
			   YelpDocInfo   *doc_info)
{
    gchar     *file;
    xmlDocPtr  doc = NULL;

    g_return_val_if_fail (parser != NULL, NULL);
    g_return_val_if_fail (doc_info != NULL, NULL);
    g_return_val_if_fail (yelp_doc_info_get_type (doc_info) != YELP_DOC_TYPE_MAN, NULL);

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
    if (parser->channel)
	g_io_channel_unref (parser->channel);
    
    g_free (parser);
}

/******************************************************************************/

static void
parser_parse_line (YelpManParser *parser) {
    parser->anc = parser->buffer;
    parser->cur = parser->buffer;
    
    /* check to see if we are ignoring input */
    if (parser->ignore) {
	gchar *ptr;
	ptr = strstr (parser->buffer, parser->token);
    	if (ptr != NULL) {
	    while (PARSER_CUR)
		parser->anc = ++parser->cur;
	    g_free (parser->token);
	    parser->ignore = FALSE;
	} else {
	    /* return to get another line of input  */
	    return;
	}
    } else {
	switch (*(parser->buffer)) {
	case '.':
	    parser_handle_linetag (parser);
    	    /* we are ignoring everything until parser->token, 
     	     * so return and get next line */
    	    if (parser->ignore)
	        return;
	    break;
	case '\n':
	    parser->ins = xmlDocGetRootElement (parser->doc);
	    break;
	case '\'':
	    parser->cur = parser->buffer + parser->length - 1;
	    parser->anc = parser->cur;
	default:
	    break;
	}
    }
    
    parser_read_until (parser, '\n');
     
    if (parser->cur != parser->anc)
	parser_append_text (parser);
    
    if (PARSER_CUR) {
	parser->cur++;
	parser_append_text (parser);
    }
}

/* creates a single string from all the macro arguments */
static gchar *
args_concat_all (GSList *args)
{
    GSList *ptr = NULL;
    gchar **str_array = NULL;
    gchar *retval = NULL;
    gint i = 0;
    
    if (!args)
	return NULL;

    str_array = g_malloc0 ((sizeof (gchar *)) * (g_slist_length (args)+1) );

    ptr = args;
    while (ptr && ptr->data) {
	str_array[i++] = ptr->data;
	ptr = g_slist_next (ptr);
    }
    
    str_array[i] = NULL;

    retval = g_strjoinv (" ", str_array);

    g_free (str_array);

    return retval;
}

/* handler to ignore a macro by reading until the null character */
static void
macro_ignore_handler (YelpManParser *parser, gchar *macro, GSList *args)
{ 
    //g_print ("ignoring...");
	
    while (PARSER_CUR) {
	parser->anc = ++parser->cur;
        //g_print ("-");
    }
    
    //g_print ("\n");
}

static void
macro_bold_small_italic_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    gchar *str = NULL;

    parser_ensure_P (parser);
    parser->ins = parser_append_node (parser, macro);
    
    if (args && args->data) {
	str = args_concat_all (args);
	parser_append_given_text_handle_escapes (parser, str, TRUE);
	g_free (str);
    }
    
    parser->ins = parser->ins->parent;
}

static void
macro_roman_bold_small_italic_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    GSList *ptr = NULL;
    gchar a[2], b[2];
    gboolean toggle = TRUE;
    
    a[0] = macro[0];
    b[0] = macro[1];
    a[1] = b[1] = '\0';

    parser_ensure_P (parser);

    ptr = args;
    while (ptr && ptr->data) {
	if (toggle)
	    parser->ins = parser_append_node (parser, a);
	else
	    parser->ins = parser_append_node (parser, b);
	
	parser_append_given_text_handle_escapes (parser, ptr->data, TRUE);
	parser->ins = parser->ins->parent;
	
	toggle = (toggle) ? 0 : 1;
	ptr = g_slist_next (ptr);
    }
}

static void
macro_new_paragraph_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    xmlNodePtr tmpNode;
	
    /* Clean up from 'lists'. If this is null we don't care. */
    tmpNode = parser_stack_pop_node (parser, "IP");
	
    tmpNode = parser_stack_pop_node (parser, "P");
    if (tmpNode != NULL) {
	parser->ins = tmpNode->parent;
    }

    parser_ensure_P (parser);
}

static void
macro_insert_self_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    parser_append_node (parser, macro);
}

static void
macro_title_header_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    GSList *ptr = NULL;
    gchar *fields[5] = { "Title", "Section", "Date", "Commentary", "Name" };
    gint i;
	
    parser->ins = parser_append_node (parser, macro);

    ptr = args;
    for (i=0; i < 5; i++) {
	if (ptr && ptr->data) {
	    parser->ins = parser_append_node (parser, fields[i]);
	    parser_append_given_text_handle_escapes (parser, ptr->data, FALSE);
	    parser->ins = parser->ins->parent;	
	    ptr = g_slist_next (ptr);
	} else 
	    break;
    }

    parser->ins = parser->ins->parent;
}

static void
macro_section_header_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    static gint id = 0;
    GIOStatus retval;
    GError **errormsg = NULL;
    gchar *str = NULL;
    gchar *macro_uc = g_strdup (macro);
    gchar *ptr;
    gchar  idval[20];
    
    if (!args) {
	retval = g_io_channel_read_line (parser->channel,
				         &str,
				         NULL, NULL, errormsg);
	if (retval != G_IO_STATUS_NORMAL) {
	    g_warning ("g_io_channel_read_line != G_IO_STATUS_NORMAL\n");
	}
    } else 
	str = args_concat_all (args);

    for (ptr = macro_uc; *ptr != '\0'; ptr++)
    	*ptr = g_ascii_toupper (*ptr);
    
    parser_stack_pop_node (parser, "IP");

    g_snprintf (idval, 20, "%d", ++id);
    
    /* Sections should be their own, well, section */
    parser->ins = xmlDocGetRootElement (parser->doc);
    parser->ins = parser_append_node_attr (parser, macro_uc, "id", idval);
    parser_append_given_text_handle_escapes (parser, str, FALSE);
    parser->ins = parser->ins->parent;
    
    if (str)
	g_free (str);
}

static void
macro_spacing_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    parser->ins = parser_append_node (parser, macro);

    if (args && args->data) {
	parser->ins = parser_append_node (parser, "Count");
	parser_append_given_text (parser, args->data);
	parser->ins = parser->ins->parent;
    }

    parser->ins = parser->ins->parent;
}
	
/* this is used to define or redefine a macro until ".." 
 * is reached. */
static void
macro_define_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
	parser->ignore = TRUE;
	parser->token = g_strdup("..");
}
	
static void
macro_tp_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    xmlNodePtr tmpNode = NULL;
    GError **errormsg = NULL;

    tmpNode = parser_stack_pop_node (parser, "IP");

    if (tmpNode != NULL)
	parser->ins = tmpNode->parent;

    parser->ins = parser_append_node (parser, "IP");

    if (args && args->data) {
        parser->ins = parser_append_node (parser, "Indent");
	parser_append_given_text (parser, args->data);
        parser->ins = parser->ins->parent;
    }

    g_free (parser->buffer);

    if (g_io_channel_read_line (parser->channel,
                                &(parser->buffer), 
				(gsize *)&(parser->length), 
				NULL, errormsg)
            == G_IO_STATUS_NORMAL) {
	parser->ins = parser_append_node (parser, "Tag");
	parser_parse_line (parser);
	parser->ins = parser->ins->parent;
    }

    parser_stack_push_node (parser, parser->ins);
}
	
static void
macro_ip_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    xmlNodePtr tmpNode;
    
    tmpNode = parser_stack_pop_node (parser, "IP");

    if (tmpNode != NULL)
	parser->ins = tmpNode->parent;

    parser->ins = parser_append_node (parser, macro);

    if (args && args->data) {
	parser->ins = parser_append_node (parser, "Tag");
	parser_append_given_text_handle_escapes (parser, args->data, TRUE);
	parser->ins = parser->ins->parent;
	    
	if (args->next && args->next->data) {
	    parser->ins = parser_append_node (parser, "Indent");
	    parser_append_given_text_handle_escapes (parser, args->next->data, TRUE);
	    parser->ins = parser->ins->parent;
	}
    }

    parser_stack_push_node (parser, parser->ins);
}
	
static void
macro_hanging_paragraph_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    parser_stack_pop_node (parser, "IP");

    parser->ins = parser_append_node (parser, macro);

    if (args && args->data) {
	parser->ins = parser_append_node (parser, "Indent");
	parser_append_given_text (parser, args->data);
	parser->ins = parser->ins->parent;
    }
}

/* BSD mandoc macros
 * Since mandoc man pages are required to begin with Dd, Dt, Os,
 * we will use this to create the TH tag.
 */
static void
macro_mandoc_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    gchar *str = NULL;
	
    if (g_str_equal (macro, "Dd")) {
	parser->ins = parser_append_node (parser, "TH");

	str = args_concat_all (args);
	
	if (args && args->data) {
            parser->ins = parser_append_node (parser, "Date");
            parser_append_given_text (parser, str);
            parser->ins = parser->ins->parent;
        }

	g_free (str);
    } 
    else if (g_str_equal (macro, "Dt")) {
	if (args && args->data) {
            parser->ins = parser_append_node (parser, "Title");
            parser_append_given_text (parser, args->data);
            parser->ins = parser->ins->parent;
        }

	if (args && args->next && args->next->data) {
            parser->ins = parser_append_node (parser, "Section");
            parser_append_given_text (parser, args->next->data);
            parser->ins = parser->ins->parent;
	}
    } 
    else if (g_str_equal (macro, "Os")) {
	if (args && args->data) {
            parser->ins = parser_append_node (parser, "Os");
            parser_append_given_text (parser, args->data);
            parser->ins = parser->ins->parent;
        }

	/* Leave the TH tag */
	parser->ins = parser->ins->parent;
    }    
}

static void
macro_url_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    xmlNodePtr tmpNode = NULL;
    
    if (g_str_equal (macro, "UR")) {
	/* If someone wants to do automatic hyperlink wizardry outside
	 * for the parser, then this should instead generate a tag.
         */
	if (args && args->data) {
	    if (g_str_equal (args->data, ":"))
		parser->make_links = FALSE;
	    else {
		parser->ins = parser_append_node (parser, macro);
	    
		parser_stack_push_node (parser, parser->ins);
	    
		parser->ins = parser_append_node (parser, "URI");
		parser_append_given_text (parser, args->data);
		parser->ins = parser->ins->parent;
	    }
	}
    } 
    else if (g_str_equal (macro, "UE")) {
	
	if (parser->make_links) {
	    tmpNode = parser_stack_pop_node (parser, "UR");

	    if (tmpNode == NULL)
		d (g_warning ("Found unexpected tag: '%s'\n", macro));
	    else
		parser->ins = tmpNode->parent;
	} else
	    parser->make_links = TRUE;
	
    } 
    else if (g_str_equal (macro, "UN")) {

	if (args && args->data) {
	    parser->ins = parser_append_node (parser, macro);
	    parser_append_given_text (parser, args->data);
	    parser->ins = parser->ins->parent;
	}
	
    }
}

/* relative margin indent; FIXME: this takes a parameter that tells
 * how many indents to do, which needs to be implemented to fix 
 * some man page formatting options */
static void
macro_rs_re_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    xmlNodePtr tmpNode;

    if (g_str_equal (macro, "RS")) {
	parser->ins = parser_append_node (parser, macro);

	parser_stack_push_node (parser, parser->ins);

	if (args && args->data) {
            parser->ins = parser_append_node (parser, "Indent");
            parser_append_given_text (parser, args->data);
            parser->ins = parser->ins->parent;
        }
    } 
    else if (g_str_equal (macro, "RE")) {
	parser_stack_pop_node (parser, "IP");

	tmpNode = parser_stack_pop_node (parser, "RS");

	if (tmpNode == NULL)
	    d (g_warning ("Found unexpected tag: '%s'\n", macro));
	else
	    parser->ins = tmpNode->parent;
    }
}

static void
macro_mandoc_list_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    xmlNodePtr tmpNode;

    if (g_str_equal (macro, "Bl")) {
       
	parser->ins = parser_append_node (parser, macro);
	    
	if (args && args->data) {
	    gchar *listtype = (gchar *)args->data;
		
	    if (g_str_equal (listtype, "-hang") ||
		g_str_equal (listtype, "-ohang") ||
		g_str_equal (listtype, "-tag") ||
		g_str_equal (listtype, "-diag") ||
		g_str_equal (listtype, "-inset")
	       ) {
		listtype++;
		xmlNewProp (parser->ins, BAD_CAST "listtype", 
		            BAD_CAST listtype);
		/* TODO: check for -width, -offset, -compact */
	    } else if (g_str_equal (listtype, "-column")) {
		/* TODO: support this */;
	    } else if (g_str_equal (listtype, "-item") ||
		       g_str_equal (listtype, "-bullet") ||
		       g_str_equal (listtype, "-hyphen") ||
		       g_str_equal (listtype, "-dash")
		      ) {
		listtype++;
		xmlNewProp (parser->ins, BAD_CAST "listtype", 
		            BAD_CAST listtype);
		/* TODO: check for -offset, -compact */
	    }
	}
	    
        parser_stack_push_node (parser, parser->ins);
    }
    else if (g_str_equal (macro, "El")) {
    
    	tmpNode = parser_stack_pop_node (parser, "It");

	if (tmpNode != NULL)
	    parser->ins = tmpNode->parent;

        tmpNode = parser_stack_pop_node (parser, "Bl");

        if (tmpNode == NULL)
            d (g_warning ("Found unexpected tag: '%s'\n", macro));
        else
            parser->ins = tmpNode->parent;
    }
}

static void
macro_verbatim_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    xmlNodePtr tmpNode;
    
    if (g_str_equal (macro, "nf") || g_str_equal (macro, "Vb")) {
	parser->ins = parser_append_node (parser, "Verbatim");
	parser_stack_push_node (parser, parser->ins);
    } 
    else if (g_str_equal (macro, "fi") || g_str_equal (macro, "Ve")) {
	tmpNode = parser_stack_pop_node (parser, "Verbatim");

	if (tmpNode == NULL)
	    d (g_warning ("Found unexpected tag: '%s'\n", macro));
	else
	    parser->ins = tmpNode->parent;
    }
}

/* many mandoc macros have their arguments parsed so that other
 * macros can be called to operate on their arguments.  This table
 * indicates which macros are _parsed_ for other callable macros, 
 * and which are _callable_ from other macros: see mdoc(7) for more
 * details
 */

#define MANDOC_NONE 0x01
#define MANDOC_PARSED 0x01
#define MANDOC_CALLABLE 0x02

struct MandocMacro {
    gchar *macro;
    gint flags;
};

struct MandocMacro manual_macros[] = {
    { "Ad", MANDOC_PARSED | MANDOC_CALLABLE },
    { "An", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Ar", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Cd", MANDOC_NONE },
    { "Cm", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Dv", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Er", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Ev", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Fa", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Fd", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Fl", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Fn", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Ic", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Li", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Nd", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Nm", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Op", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Ot", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Pa", MANDOC_PARSED | MANDOC_CALLABLE },
    { "St", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Tn", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Va", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Vt", MANDOC_PARSED | MANDOC_CALLABLE },
    { "Xr", MANDOC_PARSED | MANDOC_CALLABLE },
    { NULL, MANDOC_NONE }
};

static gboolean
is_mandoc_manual_macro_parsed (gchar *macro)
{
    gint i;
	
    for (i=0; manual_macros[i].macro != NULL; i++) {
        if (g_str_equal (macro, manual_macros[i].macro) &&
	    (manual_macros[i].flags & MANDOC_PARSED) == MANDOC_PARSED
	   ) {
		return TRUE;
	}
    }

    return FALSE;
}

static gboolean
is_mandoc_manual_macro_callable (gchar *macro)
{
    gint i;
	
    for (i=0; manual_macros[i].macro != NULL; i++) {
        if (g_str_equal (macro, manual_macros[i].macro) &&
	    (manual_macros[i].flags & MANDOC_CALLABLE) == MANDOC_CALLABLE
	   ) {
		return TRUE;
	}
    }

    return FALSE;
}

static void
macro_mandoc_utility_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    GSList *ptr = NULL;
    gchar *str = NULL;
    gchar *manpage, *uri;

    g_return_if_fail (macro != NULL);
    
    if (is_mandoc_manual_macro_parsed (macro)) {
	parser->ins = parser_append_node (parser, macro);
	
	ptr = args;
	while (ptr && ptr->data) {
	    if (is_mandoc_manual_macro_callable ((gchar *)ptr->data)) {
	    	macro_mandoc_utility_handler (parser, (gchar *)ptr->data, ptr->next);
		break;
	    } else {
		parser_append_given_text_handle_escapes (parser, (gchar *)ptr->data, TRUE);
	    }
	    ptr = ptr->next;
	    if (ptr && ptr->data)
		parser_append_given_text (parser, " ");
	}
	
	parser->ins = parser->ins->parent;
    } else {
	parser->ins = parser_append_node (parser, macro);
	str = args_concat_all (args);
	parser->ins = parser->ins->parent;
    
	g_free (str);
    }

    return;
	
    if (g_str_equal (macro, "Op")) {
	    
    } else if (g_str_equal (macro, "Nm")) {

	if (str) {
	    parser_ensure_P (parser);
		
	    parser->ins = parser_append_node (parser, "B");
	    parser_append_given_text_handle_escapes (parser, str, TRUE);
	    parser->ins = parser->ins->parent;
        }
    }
    else if (g_str_equal (macro, "Nd")) {
    
	if (str) {
	    parser_append_given_text (parser, " -- ");
	    parser_append_given_text_handle_escapes (parser, str, TRUE);
	}
    }
    else if (g_str_equal (macro, "Xr")) {
	    
	if (args && args->data && args->next && args->next->data) {
	    
	    manpage = g_strdup_printf ("%s(%s)", (gchar *)args->data, (gchar *)args->next->data);
	    uri = g_strdup_printf ("man:%s", manpage);
	    
	    parser_ensure_P (parser);
	    
	    parser->ins = parser_append_node (parser, "UR");
	    parser->ins = parser_append_node (parser, "URI");
	    parser_append_given_text (parser, uri);
	    parser->ins = parser->ins->parent;
	    parser_append_given_text (parser, manpage);
	    parser->ins = parser->ins->parent;
	   
	    ptr = args->next->next;

	    while (ptr && ptr->data) {
		parser_append_given_text (parser, ptr->data);
		ptr = g_slist_next (ptr);
	    }
	    
	    g_free (uri);
	    g_free (manpage);
	}
    }

    g_free (str);
}

static void
macro_mandoc_listitem_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
    GSList *ptr = NULL;
    xmlNodePtr tmpNode;
    
    tmpNode = parser_stack_pop_node (parser, "It");

    if (tmpNode != NULL)
	parser->ins = tmpNode->parent;

    parser->ins = parser_append_node (parser, macro);

    if (args && args->data) {
	parser->ins = parser_append_node (parser, "ItTag");
	
	ptr = args;
	while (ptr && ptr->data) {
	    if (is_mandoc_manual_macro_callable ((gchar *)ptr->data)) {
	    	macro_mandoc_utility_handler (parser, (gchar *)ptr->data, ptr->next);
		break;
	    } else {
		parser_append_given_text (parser, (gchar *)ptr->data);
	    }
	    ptr = ptr->next;
	    if (ptr && ptr->data)
		parser_append_given_text (parser, " ");
	}
	
	parser->ins = parser->ins->parent;
    }

    parser_stack_push_node (parser, parser->ins);
}

static void
macro_template_handler (YelpManParser *parser, gchar *macro, GSList *args)
{
}

/* the handler functions for each macro all have this form:
 *   - the calling function, parser_handle_linetag owns the "macro", and "args"
 *     parameters, so do not free them.
 */
typedef void (*MacroFunc)(YelpManParser *parser, gchar *macro, GSList *args);

struct MacroHandler {
    gchar *macro;
    MacroFunc handler;
};

/* We are calling all of these macros, when in reality some of them are
 * requests (lowercase, defined by groff system), and some of them are
 * macros (varying case, defined by man/mdoc/ms/tbl extensions)
 *
 * A great resource to figure out what each of these does is the groff
 * info page.  Also groff(7), man(7), and mdoc(7) are useful as well.
 */
struct MacroHandler macro_handlers[] = {
    { "\\\"", macro_ignore_handler },                /* groff: comment */ 
    { "ad", macro_ignore_handler },                  /* groff: set adjusting mode */ 
    { "Ad", macro_mandoc_utility_handler },          /* mandoc: Address */ 
    { "An", macro_mandoc_utility_handler },          /* mandoc: Author name */ 
    { "Ar", macro_mandoc_utility_handler },          /* mandoc: Command line argument */ 
    { "B",  macro_bold_small_italic_handler },       /* man: set bold font */
    { "Bd", macro_ignore_handler },                  /* mandoc: Begin-display block */
    { "BI", macro_roman_bold_small_italic_handler }, /* man: bold italic font */
    { "Bl", macro_mandoc_list_handler },             /* mandoc: begin list */
    { "bp", macro_ignore_handler },                  /* groff: break page */ 
    { "br", macro_insert_self_handler },             /* groff: line break */
    { "BR", macro_roman_bold_small_italic_handler }, /* man: set bold roman font */
    { "Cd", macro_mandoc_utility_handler },          /* mandoc: Configuration declaration */ 
    { "Cm", macro_mandoc_utility_handler },          /* mandoc: Command line argument modifier */ 
    { "ce", macro_ignore_handler },                  /* groff: center text */
    { "Dd", macro_mandoc_handler },                  /* mandoc: Document date */
    { "de", macro_define_handler },                  /* groff: define macro */
    { "ds", macro_ignore_handler },                  /* groff: define string variable */
    { "D1", macro_ignore_handler },                  /* mandoc: Indent and display one text line */
    { "Dl", macro_ignore_handler },                  /* mandoc: Indent and display one line of literal text */
    { "Dt", macro_mandoc_handler },                  /* mandoc: Document title */
    { "Dv", macro_mandoc_utility_handler },          /* mandoc: Defined variable */ 
    { "Ed", macro_ignore_handler },                  /* mandoc: End-display block */
    { "El", macro_mandoc_list_handler },             /* mandoc: end list */ 
    { "Er", macro_mandoc_utility_handler },          /* mandoc: Error number */ 
    { "Ev", macro_mandoc_utility_handler },          /* mandoc: Environment variable */ 
    { "Fa", macro_mandoc_utility_handler },          /* mandoc: Function argument */ 
    { "Fd", macro_mandoc_utility_handler },          /* mandoc: Function declaration */ 
    { "fi", macro_verbatim_handler },                /* groff: activate fill mode */
    { "Fl", macro_mandoc_utility_handler },          /* mandoc: ? */ 
    { "Fn", macro_mandoc_utility_handler },          /* mandoc: Function call */ 
    { "ft", macro_ignore_handler },                  /* groff: change font */
    { "HP", macro_hanging_paragraph_handler },       /* man: paragraph with hanging left indentation */
    { "hy", macro_ignore_handler },                  /* groff: enable hyphenation */
    { "I",  macro_bold_small_italic_handler },       /* man: set italic font */
    { "Ic", macro_mandoc_utility_handler },          /* mandoc: Interactive Command */ 
    { "ie", macro_ignore_handler },                  /* groff: else portion of if-else */
    { "if", macro_ignore_handler },                  /* groff: if statement */
    { "ig", macro_ignore_handler },                  /* groff: comment until '..' or '.END' */
    { "ih", macro_ignore_handler },                  /* ? */
    { "IX", macro_ignore_handler },                  /* ms: print index to stderr */
    { "IB", macro_roman_bold_small_italic_handler }, /* man: set italic bold font */
    { "IP", macro_ip_handler },                      /* man: indented paragraph */
    { "IR", macro_roman_bold_small_italic_handler }, /* man: set italic roman font */
    { "It", macro_mandoc_listitem_handler },         /* mandoc: item in list */
    { "Li", macro_mandoc_utility_handler },          /* mandoc: Literal text */ 
    { "LP", macro_new_paragraph_handler },           /* man: line break and left margin and indentation are reset */
    { "na", macro_ignore_handler },                  /* groff: disable adjusting */
    { "Nd", macro_mandoc_utility_handler },          /* mandoc: description of utility/program */
    { "ne", macro_ignore_handler },                  /* groff: force space at bottom of page */
    { "nf", macro_verbatim_handler },                /* groff: no fill mode */
    { "nh", macro_ignore_handler },                  /* groff: disable hyphenation */
    { "Nd", macro_mandoc_utility_handler },          /* mandoc: ? */
    { "Nm", macro_mandoc_utility_handler },          /* mandoc: Command/utility/program name*/
    { "Op", macro_mandoc_utility_handler },          /* mandoc: Option */
    { "Os", macro_mandoc_handler },                  /* mandoc: Operating System */
    { "Ot", macro_mandoc_utility_handler },          /* mandoc: Old style function type (Fortran) */
    { "P",  macro_new_paragraph_handler },           /* man: line break and left margin and indentation are reset */
    { "Pa", macro_mandoc_utility_handler },          /* mandoc: Pathname or filename */
    { "PP", macro_new_paragraph_handler },           /* man: line break and left margin and indentation are reset */
    { "Pp", macro_new_paragraph_handler },           /* man: line break and left margin and indentation are reset */
    { "ps", macro_ignore_handler },                  /* groff: change type size */
    { "RB", macro_roman_bold_small_italic_handler }, /* man: set roman bold font */
    { "RE", macro_ignore_handler },                  /* man: move left margin back to NNN */
    { "RI", macro_roman_bold_small_italic_handler }, /* man: set roman italic font */
    { "RS", macro_ignore_handler },                  /* man: move left margin to right by NNN */
    { "SH", macro_section_header_handler },          /* man: unnumbered section heading */
    { "Sh", macro_section_header_handler },          /* man: unnumbered section heading */
    { "SM", macro_bold_small_italic_handler },       /* man: set font size one SMaller */
    { "so", macro_ignore_handler },                  /* groff: include file */
    { "sp", macro_spacing_handler },                 /* groff: */
    { "SS", macro_section_header_handler },          /* man: unnumbered subsection heading */
    { "Ss", macro_section_header_handler },          /* man: unnumbered subsection heading */
    { "St", macro_mandoc_utility_handler },          /* mandoc: Standards (-p1003.2, -p1003.1 or -ansiC) */
    { "TH", macro_title_header_handler },            /* man: set title of man page */
    { "TP", macro_tp_handler },                      /* man: set indented paragraph with label */
    { "UR", macro_url_handler },                     /* man: URL start hyperlink */
    { "UE", macro_url_handler },                     /* man: URL end hyperlink */
    { "UN", macro_ignore_handler },                  /* ? */ 
    { "TE", macro_ignore_handler },                  /* ms: table */
    { "Tn", macro_mandoc_utility_handler },          /* mandoc: Trade or type name (small Caps). */
    { "ti", macro_ignore_handler },                  /* groff: temporary indent */
    { "tr", macro_ignore_handler },                  /* groff: translate characters */
    { "TS", macro_ignore_handler },                  /* ms: table with optional header */
    { "Va", macro_mandoc_utility_handler },          /* mandoc: Variable name */
    { "Vb", macro_verbatim_handler },                /* pod2man: start of verbatim text */
    { "Ve", macro_verbatim_handler },                /* pod2man: end of verbatim text */
    { "Vt", macro_mandoc_utility_handler },          /* mandoc: Variable type (Fortran only) */
    { "Xr", macro_mandoc_utility_handler },          /* mandoc: Manual page cross reference */
    { NULL, NULL } 
};

static void
parser_handle_linetag (YelpManParser *parser) {
    gchar c, *str, *ptr, *arg;
    GSList *arglist = NULL;
    GSList *listptr = NULL;
    MacroFunc handler_func = NULL;
    
    static GHashTable *macro_hash = NULL;

    /* check if we've created the hash of macros yet.  If not, make it */
    if (!macro_hash) {
	gint i;
	    
	macro_hash = g_hash_table_new (g_str_hash, g_str_equal);
	
	for (i=0; macro_handlers[i].macro != NULL; i++) {
	    g_hash_table_insert (macro_hash, 
	                         macro_handlers[i].macro, 
	                         macro_handlers[i].handler);
	}
    }

    /* FIXME: figure out a better way to handle these cases */
    /* special case, if the line is simply ".\n" then return */
    if (*(parser->cur+1) == '\n') {
    	++parser->cur;
	parser->anc = ++parser->cur;
	return;
    } 
    /* special case, if the line is simply "..\n" then return */
    else if (*(parser->cur+1) == '.' && *(parser->cur+2) == '\n') {
    	++parser->cur;
    	++parser->cur;
    	parser->anc = ++parser->cur;
    }
    
    /* skip any spaces after the control character . */
    while (PARSER_CUR && *(parser->cur) == ' ')
	    parser->cur++;
    
    while (PARSER_CUR
	   && *(parser->cur) != ' '
	   && ( (*parser->cur != '\\') || ((*parser->cur == '\\') && (*(parser->cur+1) == '\"')) )
	   && *(parser->cur) != '\n') {    
	if ((*parser->cur == '\\') && (*(parser->cur+1) == '\"')) {
	    parser->cur += 2;
	    break;
	}
	parser->cur++;
    }

    /* copy the macro/request into str */
    c = *(parser->cur);
    *(parser->cur) = '\0';
    str = g_strdup (parser->anc + 1);  /* skip control character '.' by adding one */
    *(parser->cur) = c;
    parser->anc = parser->cur;
    
    /* FIXME: need to handle escaped characters */
    /* perform argument parsing and store argument in a singly linked list */
    while (PARSER_CUR && *(parser->cur) != '\n') { 
	ptr = NULL;
	arg = NULL;
	    
	/* skip any whitespace */
	while (PARSER_CUR && *(parser->cur) == ' ')
	    parser->anc = ++parser->cur;
	
get_argument:
	/* search until we hit whitespace or an " */
	while (PARSER_CUR && 
               *(parser->cur) != '\n' &&
	       *(parser->cur) != ' ' &&
	       *(parser->cur) != '\"')
		parser->cur++;

	/* this checks for escaped spaces */
	if (PARSER_CUR && 
	    ((parser->cur - parser->buffer) > 0) &&
	    *(parser->cur) == ' ' &&
	    *(parser->cur-1) == '\\') {
		parser->cur++;
		goto get_argument;
	}
	
	if (*(parser->cur) == '\n' && (parser->cur == parser->anc)) {
	    break;
	}
	
	if (*(parser->cur) == '\"' && *(parser->cur-1) == ' ') {
	    /* quoted argument */
	    ptr = strchr (parser->cur+1, '\"');
	    if (ptr != NULL) {
		c = *(ptr);
		*(ptr) = '\0';
		arg = g_strdup (parser->anc+1);
		*(ptr) = c;
		parser->cur = ptr;
		parser->anc = ++parser->cur;
	    } else {
		/* unmatched double quote: include the " as part of the argument */
		parser->cur++;
		goto get_argument;
	    }
	} 
	else if (*(parser->cur) == '\"') {
	    /* quote in the middle of an argument */
	    c = *(parser->cur+1);
	    *(parser->cur+1) = '\0';
	    arg = g_strdup (parser->anc);
	    *(parser->cur+1) = c;
	    parser->anc = ++parser->cur;
	} 
	else if (*(parser->cur) == ' ') {
	    /* normal space separated argument */
	    c = *(parser->cur);
	    *(parser->cur) = '\0';
	    arg = g_strdup (parser->anc);
	    *(parser->cur) = c;
	    parser->anc = ++parser->cur;
	} 
	else if (*(parser->cur) == '\n' && *(parser->cur-1) != ' ') {
	    /* special case for EOL */
	    c = *(parser->cur);
	    *(parser->cur) = '\0';
	    arg = g_strdup (parser->anc);
	    *(parser->cur) = c;
	    parser->anc = parser->cur;
	} else
	    ;//g_warning ("FIXME: need to take into account this case...\n");

	arglist = g_slist_append (arglist, arg);
    }
    
    /* g_print ("handling macro (%s)\n", str);
    
    listptr = arglist;
    while (listptr && listptr->data) {
	g_print ("   arg = %s\n", (gchar *)listptr->data);
	listptr = g_slist_next (listptr);
    }
    */
    
    /* lookup the macro handler and call that function */
    handler_func = g_hash_table_lookup (macro_hash, str);
    if (handler_func)
	(*handler_func) (parser, str, arglist);
    
    /* in case macro is not defined in hash table, ignore rest of line */
    else
	macro_ignore_handler (parser, str, arglist);

    g_free (str);
    
    listptr = arglist;
    while (listptr && listptr->data) {
	g_free (listptr->data);
	listptr = g_slist_next (listptr);
    } 
    
    return;
    
    if (0) {
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
	d (g_warning ("Found unexpected tag: '%s'\n", str));
        g_free (str);
    }
    /* "ie" and "if" are conditional macros in groff
     * "ds" is to define a variable; see groff(7)
     * ignore anything between the \{ \}, otherwise ignore until
     * the end of the linee*/
    else if (g_str_equal (str, "ds") || g_str_equal (str, "ie")
	                             || g_str_equal (str, "if")) {
	/* skip any remaining spaces */
	while (PARSER_CUR && (*parser->cur == ' '))
	    parser->anc = ++parser->cur;
	
	/* skip the "stringvar" or "cond"; see groff(7) */
	while (PARSER_CUR && (*parser->cur != ' '))
	    parser->anc = ++parser->cur;
	
	/* skip any remaining spaces */
	while (PARSER_CUR && (*parser->cur == ' '))
	    parser->anc = ++parser->cur;
	
	/* check to see if the next two characters are the
	 * special "\{" sequence */
	if (*parser->cur == '\\' && *(parser->cur+1) == '{') {
	    parser->ignore = TRUE;
	    parser->token = g_strdup ("\\}");
	} else {
	    /* otherwise just ignore till the end of the line */
	    while (PARSER_CUR)
	        parser->anc = ++parser->cur;
	}
    }
    /* else conditional macro */
    else if (g_str_equal (str, "el")) {
	/* check to see if the next two characters are the
	 * special "\{" sequence */
	parser->ignore = 0;
	if (*parser->cur == '\\' && *(parser->cur+1) == '{') {
	    parser->ignore = TRUE;
	    parser->token = g_strdup ("\\}");
	} else {
	    /* otherwise just ignore till the end of the line */
	    while (PARSER_CUR)
	        parser->anc = ++parser->cur;
	}
    }

}

static void
parser_ensure_P (YelpManParser *parser)
{
    if (xmlStrEqual (parser->ins->name, BAD_CAST "Man")) {
	parser->ins = parser_append_node (parser, "P");
	parser_stack_push_node (parser, parser->ins);
    }
}

static void
parser_read_until (YelpManParser *parser,
		   gchar          delim)
{
    gchar c;
    
    while (PARSER_CUR
	   && *(parser->cur) != '\n'
	   && *(parser->cur) != delim) {
	    parser->cur++;
    }

    if (parser->anc == parser->cur)
	return;
    
    c = *(parser->cur);
    *(parser->cur) = '\0';
    parser_append_given_text_handle_escapes (parser, parser->anc, TRUE);
    *(parser->cur) = c;
    
    parser->anc = parser->cur;
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
 
    /* Find the top node we can escape from */
    while (cur && cur != (xmlNodePtr)parser->doc && 
	   cur->parent && cur->parent != (xmlNodePtr) parser->doc) {
	for (i = 0; i < ntags; i++)
	    if (!xmlStrcmp (cur->name, BAD_CAST tags[i])) {
		node = cur;
		break;
	    }
	path = g_slist_prepend (path, cur);
	cur = cur->parent;
    }

    /* Walk back down, reproducing nodes we aren't escaping */
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
parser_append_given_text_handle_escapes (YelpManParser *parser, gchar *text, gboolean make_links)
{
    gchar *escape[] = { "fI", "fB" };
    gchar *baseptr, *ptr, *anc, *str;
    gint c, len;

    g_return_if_fail (parser != NULL);
   
    if (!text)
	return;

    baseptr = g_strdup (text);
    ptr = baseptr;
    anc = baseptr;
    len = strlen (baseptr);
    
    while (ptr && *ptr != '\0') {
    
	if (*ptr == '\\') {
	    
	    c = *ptr;
	    *ptr = '\0';
	    parser_append_given_text (parser, anc);
	    *ptr = c;
	    
	    anc = ++ptr;
	
	    switch (*ptr) {
	    case '\0':
	        break;
	    case '-':
	    case '\\':
	        ptr++;
	        c = *ptr;
	        *ptr = '\0';
	        parser_append_given_text (parser, anc);
	        *ptr = c;
	        anc = ptr;
	        break;
	    case 'f':
	        ptr++;
	        if ((ptr - baseptr) > len || *ptr == '\0') break;
	        ptr++;

	        c = *(ptr);
	        *(ptr) = '\0';
	        str = g_strdup (anc);
	        *(ptr) = c;

	        parser_ensure_P (parser);
	        parser_escape_tags (parser, escape, 2);

	        /* the \f escape sequence changes the font - R is Roman, 
	         * B is Bold, and I is italic */
	        if (g_str_equal (str, "fI") || g_str_equal (str, "fB"))
		    parser->ins = parser_append_node (parser, str);
	        else if (!g_str_equal (str, "fR") && !g_str_equal (str, "fP"))
		    d (g_warning ("No rule matching the tag '%s'\n", str));

	        g_free (str);
	        anc = ptr;
	        break;
	    case '(':
	        ptr++;
	        if ((ptr - baseptr) > len || *ptr == '\0') break;
	        ptr++;
	        if ((ptr - baseptr) > len || *ptr == '\0') break;
	        ptr++;

	        c = *(ptr);
	        *(ptr) = '\0';
	        str = g_strdup (anc);
	        *(ptr) = c;

	        if (g_str_equal (str, "(co"))
		    parser_append_given_text (parser, "©");
	        else if (g_str_equal (str, "(bu"))
		    parser_append_given_text (parser, "•");
	        else if (g_str_equal (str, "(em"))
		    parser_append_given_text (parser, "—");

	        g_free (str);
	        anc = ptr;
	        break;
	    case '*':
	        ptr++;
	        if ((ptr - baseptr) > len || *ptr == '\0') break;

	        if (*(ptr) == 'R') {
		    parser_append_given_text (parser, "®");
		    ptr++;
	        } else if (*(ptr) == '(') {
		    ptr++;
		    if ((ptr - baseptr) > len || *ptr == '\0') break;
		    ptr++;
		    if ((ptr - baseptr) > len || *ptr == '\0') break;
		    ptr++;
		    
		    c = *(ptr);
		    *(ptr) = '\0';
		    str = g_strdup (anc);
		    *(ptr) = c;

		    if (g_str_equal (str, "*(Tm"))
		        parser_append_given_text (parser, "™");
		    else if (g_str_equal (str, "*(lq"))
		        parser_append_given_text (parser, "“");
		    else if (g_str_equal (str, "*(rq"))
		        parser_append_given_text (parser, "”");

		    g_free (str);
	        }
	    
	        anc = ++ptr;
	        break;
	    case 'e':
	        anc = ++ptr;
	        parser_append_given_text (parser, "\\");
	        break;
	    case '&':
	        anc = ++ptr;
	        break;
	    case 's':
	        /* this handles (actually ignores) the groff macros \s[+-][0-9] */
	        ptr++;
	        if (*(ptr) == '+' || *(ptr) == '-') {
		    ptr++;
		    if (g_ascii_isdigit (*ptr)) {
			    ptr++;
		    }
	        } else if (g_ascii_isdigit (*ptr)) {
		    ptr++;
	        }
	        anc = ptr;
	        break;
	    case '"':
	        /* Marks comments till end of line. so we can ignore it. */
	        while (ptr && *ptr != '\0')
		    ptr++;
	        anc = ptr;
	        break;
	    case '^':
	    case '|':
	        /* 1/12th and 1/16th em respectively - ignore this and simply output a space */
	        anc = ++ptr;
	        break;
	    default:
	        ptr++;
		c = *(ptr);
		*(ptr) = '\0';
		parser_append_given_text (parser, anc);
		*(ptr) = c;

		anc++;
		break;
	    }
	    
	}
        else if ((make_links) && (*ptr == '(')) {
	    gchar *space_pos;
	    gchar *tmp_cur;
	    gchar *url;
	    gchar  c;
	   
	    space_pos = ptr;
	    
	    while (space_pos != anc && *(space_pos - 1) != ' ') {
		space_pos--;
	    }
	    
	    if (space_pos != ptr &&
	        g_ascii_isdigit(*(ptr+1)) &&
		*(ptr+2) == ')') {
	    
		ptr+=3;
	    
		parser_ensure_P (parser);
	    
		tmp_cur = ptr;
		ptr = space_pos;
	    
		c = (*ptr);
		*ptr = '\0';
		parser_append_given_text (parser, anc);
		*ptr = c;
		anc = ptr;
	    
		ptr = tmp_cur;
	    
		c = *(ptr);
		*(ptr) = '\0';
		url = g_strdup_printf ("man:%s", anc);
	    
		parser->ins = parser_append_node (parser, "UR");
	
		parser->ins = parser_append_node (parser, "URI");
		parser_append_given_text (parser, url);
		parser->ins = parser->ins->parent;
	    
		parser_append_given_text (parser, anc);
		parser->ins = parser->ins->parent;
	    
		*(ptr) = c;
		anc = ptr;
	    
		g_free (url);

	    } else {
		ptr++;
	    }    
	}
	else {
	    ptr++;
	}	

    } // end while

    c = *(ptr);
    *(ptr) = '\0';
    parser_append_given_text (parser, anc);
    *(ptr) = c;
   
    g_free (baseptr); 
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
    if (!name)
	return NULL;
	
    return xmlNewChild (parser->ins, NULL, BAD_CAST name, NULL);
}

static xmlNodePtr
parser_append_node_attr (YelpManParser *parser,
			 gchar         *name,
			 gchar         *attr,
			 gchar         *value)
{
    xmlNodePtr node = NULL;
    
    node = xmlNewChild (parser->ins, NULL, BAD_CAST name, NULL);
    xmlNewProp (node, BAD_CAST attr, BAD_CAST value);

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
    
    if (!xmlStrEqual (BAD_CAST name, popped->name))
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
				     (gsize *) &(parser->length),
				     NULL, NULL)
	     == G_IO_STATUS_NORMAL);
}

static void
parser_parse_table (YelpManParser *parser)
{
    xmlNodePtr table_start;
    gboolean empty_row;

    table_start = parser->ins;

    if (g_io_channel_read_line (parser->channel,
				&(parser->buffer),
				(gsize *) &(parser->length),
				NULL, NULL)
	== G_IO_STATUS_NORMAL) {
	parser->anc = parser->buffer;
	parser->cur = parser->buffer;
	
	parser_read_until (parser, ';');

	if (*(parser->cur) == ';') {
	    parser_handle_table_options (parser);

	    if (g_io_channel_read_line (parser->channel,
					&(parser->buffer),
					(gsize *) &(parser->length),
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
				       (gsize *) &(parser->length),
				       NULL, NULL)
	       == G_IO_STATUS_NORMAL) {
	    
	    parser->anc = parser->buffer;
	    parser->cur = parser->buffer;
	    
	    empty_row = FALSE;

	    switch (*(parser->buffer)) {
	    case '.':
		if (*(parser->buffer + 1) == 'T'
		    && *(parser->buffer + 2) == 'E') {
		    if (parser_stack_pop_node (parser, "TABLE") == NULL)
			d (g_warning ("Found unexpected tag: 'TE'\n"));
		    else {
			parser->ins = table_start;
			
			parser->anc = parser->buffer + 3;
			parser->cur = parser->buffer + 3;
			return;
		    }
		} else if (*(parser->buffer + 1) == 'T'
			   && *(parser->buffer + 2) == 'H') {
		    /* Do nothing */
		    empty_row = TRUE;
		} else {
		    parser_handle_linetag (parser);
		    break;
		}
	    case '\n':
		empty_row = TRUE;
		break;
	    default:
		break;
	    }
	    
	    if (!empty_row) {
		parser->ins = parser_append_node (parser, "ROW");
		while (PARSER_CUR && *(parser->cur) != '\n') {
		    parser_read_until (parser, '\t');
		    parser->ins = parser_append_node (parser, "CELL");
		    parser_append_text (parser);
		    parser->ins = parser->ins->parent;
		    parser->anc++;
		    parser->cur++;
		}
	    }

	    g_free (parser->buffer);

	    parser->ins = table_start;
	}
    }
}
