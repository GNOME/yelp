/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2010 Shaun McCance <shaunm@gnome.org>
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
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <libxml/tree.h>
#include <gio/gio.h>
#include <string.h>
#include <math.h>

#include "yelp-error.h"
#include "yelp-man-parser.h"

#define MAN_FONTS 8

/* The format has two copies of the title like MAN(1) at the top,
 * possibly with a string of text in between for the collection.
 *
 * Start with the parser on START, then HAVE_TITLE when we've read the
 * first word with parentheses. At that point, stick new words into
 * the "collection" tag. Then finally switch to BODY when we've seen
 * the second copy of the one with parentheses.
 */
typedef enum ManParserState
{
    START,
    HAVE_TITLE,
    BODY
} ManParserState;

/* See parse_body_text for how this is used. */
typedef enum ManParserSectionState
{
    SECTION_TITLE,
    SECTION_BODY
} ManParserSectionState;

struct _YelpManParser {
    xmlDocPtr     doc;           /* The top-level XML document */
    xmlNodePtr    header;        /* The header node */
    xmlNodePtr    section_node;  /* The current section */
    xmlNodePtr    sheet_node;    /* The current sheet */

    GDataInputStream *stream;    /* The GIO input stream to read from */
    gchar            *buffer;    /* The buffer, line at a time */
    gsize             length;    /* The buffer length */

    /* The width and height of a character according to troff. */
    guint char_width;
    guint char_height;

    /* Count the number of lines we've parsed (needed to get prologue) */
    guint lines_parsed;

    /* The x f k name command sets the k'th register to be name. */
    gchar* font_registers[MAN_FONTS];

    /* The current font. Should be the index of one of the
     * font_registers. Starts at 0 (of course!)
     */
    guint current_font;

    /* See description of ManParserState above */
    ManParserState state;

    /* Vertical and horizontal position as far as the troff output is
     * concerned. (Measured from top-left).
     */
    guint vpos, hpos;

    /* Text accumulator (needed since it comes through in dribs &
     * drabs...) */
    GString *accumulator;

    /* See parse_body_text for how this is used. */
    ManParserSectionState section_state;

    /* The indent of the current sheet */
    guint sheet_indent;

    /* Set to TRUE if there's been a newline since the last text was
     * parsed. */
    gboolean newline;
};

static gboolean parser_parse_line (YelpManParser *parser, GError **error);
static gboolean parse_prologue_line (YelpManParser *parser, GError **error);

/* Parsers for different types of line */
typedef gboolean (*LineParser)(YelpManParser *, GError **);
#define DECLARE_LINE_PARSER(name) \
    static gboolean (name) (YelpManParser *parser, GError **error);

DECLARE_LINE_PARSER (parse_xf);
DECLARE_LINE_PARSER (parse_f);
DECLARE_LINE_PARSER (parse_V);
DECLARE_LINE_PARSER (parse_H);
DECLARE_LINE_PARSER (parse_v);
DECLARE_LINE_PARSER (parse_h);
DECLARE_LINE_PARSER (parse_text);
DECLARE_LINE_PARSER (parse_w);
DECLARE_LINE_PARSER (parse_body_text);
DECLARE_LINE_PARSER (parse_n);

/* Declare a sort of alist registry of parsers for different lines. */
struct LineParsePair
{
    const gchar *prefix;
    LineParser handler;
};
static struct LineParsePair line_parsers[] = {
    { "x f", parse_xf }, { "f", parse_f },
    { "V", parse_V }, { "H", parse_H },
    { "v", parse_v }, { "h", parse_h },
    { "t", parse_text },
    { "w", parse_w },
    { "n", parse_n },
    { NULL, NULL }
};

/******************************************************************************/
/* Parser helper functions (managing the state of the various parsing
 * bits) */
static void finish_span (YelpManParser *parser);
static guint dx_to_em_count (YelpManParser *parser, guint dx);

/******************************************************************************/

YelpManParser *
yelp_man_parser_new (void)
{
    YelpManParser *parser = g_new0 (YelpManParser, 1);
    parser->accumulator = g_string_sized_new (1024);
    return parser;
}

/*
  This function is responsible for taking a path to a man file and
  returning something in the groff intermediate output format for us
  to use.

  If something goes wrong, we return NULL and set error to be a
  YelpError describing the problem.
*/
static GInputStream*
get_troff (gchar *path, GError **error)
{
    gint stdout;
    GError *err = NULL;
    gchar *argv[] = { "man", "-Z", "-Tutf8", NULL, NULL };

    argv[3] = path;

    if (!g_spawn_async_with_pipes (NULL, argv, NULL,
                                   G_SPAWN_SEARCH_PATH, NULL, NULL,
                                   NULL, NULL, &stdout, NULL, &err)) {
        /* We failed to run the man program. Return a "Huh?" error. */
        *error = g_error_new (YELP_ERROR, YELP_ERROR_UNKNOWN,
                              err->message);
        g_error_free (err);
        return NULL;
    }

    return (GInputStream*) g_unix_input_stream_new (stdout, TRUE);
}

xmlDocPtr
yelp_man_parser_parse_file (YelpManParser *parser,
                            gchar *path,
                            GError **error)
{
    GInputStream *troff_stream;
    gchar *line;
    gsize len;
    gboolean ret;
    xmlNodePtr root;

    troff_stream = get_troff (path, error);
    if (!troff_stream) return NULL;

    parser->stream = g_data_input_stream_new (troff_stream);

    parser->doc = xmlNewDoc (BAD_CAST "1.0");
    root = xmlNewNode (NULL, BAD_CAST "Man");
    xmlDocSetRootElement (parser->doc, root);

    parser->header = xmlNewNode (NULL, BAD_CAST "header");
    xmlAddChild (root, parser->header);

    while (1) {
       parser->buffer =
       g_data_input_stream_read_line (parser->stream,
                                      &(parser->length),
                                      NULL, NULL);
       if (parser->buffer == NULL) break;

       ret = parser_parse_line (parser, error);

       g_free (parser->buffer);

       if (!ret) {
           xmlFreeDoc (parser->doc);
           parser->doc = NULL;
           break;
       }
    }

    g_object_unref (parser->stream);

    return parser->doc;
}

void
yelp_man_parser_free (YelpManParser *parser)
{
    guint k;
    if (parser) {
        for (k=0; k<MAN_FONTS; k++)
            g_free (parser->font_registers[k]);
    }
    g_string_free (parser->accumulator, TRUE);
    g_free (parser);
}

/******************************************************************************/

/* Sets the k'th font register to be name. Copies name, so free it
 * afterwards. k should be in [0,MAN_FONTS). It seems that man always
 * gives us ones at least 1, but groff_out(5) says non-negative.
 */
static void
set_font_register (YelpManParser *parser, guint k, const gchar* name)
{
    if (k > MAN_FONTS) {
        g_warning ("Tried to set nonexistant font register %d to %s",
                   k, name);
        return;
    }
    g_free (parser->font_registers[k]);
    parser->font_registers[k] = g_strdup (name);
}

static const gchar*
get_font (const YelpManParser *parser)
{
    guint k = parser->current_font;
    if (k > MAN_FONTS ||
        parser->font_registers[k] == NULL) {

        g_warning ("Tried to get nonexistant font register %d", k);

        return "";
    }

    return parser->font_registers[k];
}

/******************************************************************************/

/*
  Convenience macros to scan a string, checking for the correct number
  of things read.

  Also to raise an error. Add an %s to the end of the format string,
  which automatically gets given parser->buffer.
 */
#define SSCANF(fmt,num,...)                                 \
    (sscanf (parser->buffer, (fmt), __VA_ARGS__) != (num))

#define PARSE_ERROR(...)                                    \
    g_error_new (YELP_ERROR, YELP_ERROR_PROCESSING,         \
                 __VA_ARGS__, parser->buffer)
#define RAISE_PARSE_ERROR(...)                              \
    { *error = PARSE_ERROR (__VA_ARGS__); return FALSE; }

static gboolean
parser_parse_line (YelpManParser *parser, GError **error)
{
    if (parser->lines_parsed < 3)
        return parse_prologue_line (parser, error);

    const struct LineParsePair *p = line_parsers;
    while (p->handler != NULL) {
        if (g_str_has_prefix (parser->buffer, p->prefix)) {
            return p->handler(parser, error);
        }
        p++;
    }
    return TRUE;
}

static gboolean
parse_prologue_line (YelpManParser *parser, GError **error)
{
    parser->lines_parsed++;
    if (parser->lines_parsed != 2) return TRUE;

    /* This is the interesting line, which should look like
              x res 240 24 40
       The interesting bits are the 24 and the 40, which are the
       width and height of a character as far as -Tutf8 is
       concerned.
    */
    if (SSCANF ("x %*s %*u %u %u", 2,
                &parser->char_width, &parser->char_height)) {
        RAISE_PARSE_ERROR ("Wrong 'x res' line from troff: %s");
    }

    return TRUE;
}

static gboolean
parse_xf (YelpManParser *parser, GError **error)
{
    gchar name[10];
    guint k;

    if (SSCANF ("x f%*s %u %10s", 2, &k, name)) {
        RAISE_PARSE_ERROR ("Invalid 'x f' line from troff: %s");
    }
    set_font_register (parser, k, name);
    return TRUE;
}

static gboolean
parse_f (YelpManParser *parser, GError **error)
{
    guint k;
    if (SSCANF ("f%u", 1, &k)) {
        RAISE_PARSE_ERROR ("Invalid font line from troff: %s");
    }
    finish_span (parser);

    parser->current_font = k;

    return TRUE;
}

static gboolean
parse_v (YelpManParser *parser, GError **error)
{
    guint dy;
    if (SSCANF ("v%u", 1, &dy)) {
        RAISE_PARSE_ERROR ("Invalid v line from troff: %s");
    }
    parser->vpos += dy;
    return TRUE;
}

static gboolean
parse_h (YelpManParser *parser, GError **error)
{
    guint dx;
    guint k;
    const gchar *str;

    if (SSCANF ("h%u", 1, &dx)) {
        RAISE_PARSE_ERROR ("Invalid h line from troff: %s");
    }
    parser->hpos += dx;

    /* This is a bit hackish to be honest but... if we're in something
     * that'll end up in a span, a spacing h command means that a gap
     * should appear. It seems that the easiest way to get this is to
     * insert nonbreaking spaces (eugh!)
     *
     * Of course we don't want to do this when chained from wh24 or
     * whatever, so check that accumulator is nonempty and the last
     * character isn't ' '.
     */
    str = parser->accumulator->str;
    if ((parser->sheet_node) &&
        (str[0] != '\0') &&
        (str[strlen (str)-1] != ' ')) {

        dx = dx_to_em_count (parser, dx);
        for (k=0; k<dx; k++) {
            /* 0xc2 0xa0 is nonbreaking space in utf8 */
            g_string_append_c (parser->accumulator, 0xc2);
            g_string_append_c (parser->accumulator, 0xa0);
        }
    }

    return TRUE;
}

static gboolean
parse_V (YelpManParser *parser, GError **error)
{
    guint y;
    if (SSCANF ("V%u", 1, &y)) {
        RAISE_PARSE_ERROR ("Invalid V line from troff: %s");
    }
    parser->vpos = y;
    return TRUE;
}

static gboolean
parse_H (YelpManParser *parser, GError **error)
{
    guint x;
    if (SSCANF ("H%u", 1, &x)) {
        RAISE_PARSE_ERROR ("Invalid H line from troff: %s");
    }
    parser->hpos = x;
    return TRUE;
}

static gboolean
parse_text (YelpManParser *parser, GError **error)
{
    gchar *text, *section, *tmp;
    xmlNodePtr node;

    g_assert (parser->buffer[0] == 't');

    if (parser->state == START) {
        /* With a bit of luck, this will be the tBLAH(1) line. Can't
         * use sscanf to chop it up since that needs whitespace. */
        section = strchr (parser->buffer + 1, '(');
        if (!section)
            RAISE_PARSE_ERROR ("Expected t line with title. Got %s");
        text = g_strndup (parser->buffer + 1,
                          section - (parser->buffer + 1));

        // Skip over the (
        section++;

        tmp = strchr (section, ')');
        if (!tmp || (*(tmp+1) != '\0'))
            RAISE_PARSE_ERROR ("Strange format for t title line: %s");
        section = g_strndup (section, tmp - section);

        parser->state = HAVE_TITLE;

        xmlNewTextChild (parser->header,
                         NULL, BAD_CAST "title", text);
        xmlNewTextChild (parser->header,
                         NULL, BAD_CAST "section", section);

        g_free (text);
        g_free (section);

        /* The accumulator should currently be "". */
        g_assert (parser->accumulator &&
                  *(parser->accumulator->str) == '\0');

        return TRUE;
    }
    if (parser->state == HAVE_TITLE) {
        /* We expect (maybe!) to get some lines tThe wh24
         * tCollection. We've found (and can ignore!) the second
         * title line if there's a (). */
        if (strchr (parser->buffer+1, '(') &&
            strchr (parser->buffer+1, ')')) {
            parser->state = BODY;

            xmlNewTextChild (parser->header,
                             NULL, BAD_CAST "collection",
                             parser->accumulator->str);
            g_string_truncate (parser->accumulator, 0);

            return TRUE;
        }

        g_string_append (parser->accumulator, parser->buffer+1);

        return TRUE;
    }

    return parse_body_text (parser, error);
}

/*
  w is a sort of prefix argument. It indicates a space, so we register
  that here, then call parser_parse_line again on the rest of the
  string to deal with that.
 */
static gboolean
parse_w (YelpManParser *parser, GError **error)
{
    gboolean ret;

    if (parser->state != START) {
        g_string_append_c (parser->accumulator, ' ');
    }
    
    parser->buffer++;
    ret = parser_parse_line (parser, error);
    parser->buffer--;
    return ret;
}

static gboolean
parse_body_text (YelpManParser *parser, GError **error)
{
    gchar tmp[64];

    /*
      It's this function which is responsible for trying to get *some*
      semantic information back out of the manual page.

      The highest-level chopping up is into sections. We use the
      heuristic that if either
        (1) We haven't got a section yet or
        (2) text starts a line (hpos=0)
      then it's a section title.

      It's possible to have spaces in section titles, so we carry on
      accumulating the section title until the next newline.
    */
    if (parser->section_state != SECTION_TITLE && parser->hpos == 0) {
        g_string_truncate (parser->accumulator, 0);
        /* End the current sheet & section */
        parser->section_state = SECTION_TITLE;
        parser->sheet_node = NULL;

        parser->section_node =
            xmlAddChild (xmlDocGetRootElement (parser->doc),
                         xmlNewNode (NULL, BAD_CAST "section"));
    }
    if (parser->section_state == SECTION_TITLE) goto do_append;

    /*
      Here we've got real body text! If newline is true, this is the
      first word on a line.

      In which case, we check to see whether hpos agrees with the
      current sheet's indent. If so (or if there isn't a sheet yet!),
      we just add to the accumulator. If not, start a new sheet with
      the correct indent.

      If we aren't the first word on the line, just add to the
      accumulator.
    */
    if ((!parser->sheet_node) ||
        (parser->newline && (parser->hpos != parser->sheet_indent))) {
        /* We don't need to worry about finishing the current sheet,
           since the accumulator etc. get cleared on newlines and we
           know we're at the start of a line.
        */
        parser->sheet_node =
            xmlAddChild (parser->section_node,
                         xmlNewNode (NULL, BAD_CAST "sheet"));
        parser->sheet_indent = parser->hpos;

        /* The indent is specified in em's. */
        snprintf (tmp, 64, "%d",
                  (int)(dx_to_em_count (parser, parser->hpos) / 1.5));
        xmlNewProp (parser->sheet_node, BAD_CAST "indent", tmp);
    }

  do_append:
    g_string_append (parser->accumulator, parser->buffer+1);

    /* Move hpos forward per char */
    parser->hpos += strlen (parser->buffer+1) * parser->char_width;

    parser->newline = FALSE;

    return TRUE;
}

static gboolean
parse_n (YelpManParser *parser, GError **error)
{
    xmlNodePtr node;

    /* Don't care about newlines in the header bit */
    if (parser->state != BODY) return TRUE;

    if (parser->section_state == SECTION_TITLE) {
        g_strchomp (parser->accumulator->str);
        xmlNewTextChild (parser->section_node, NULL,
                         BAD_CAST "title", parser->accumulator->str);
        g_string_truncate (parser->accumulator, 0);

        parser->section_state = SECTION_BODY;
    }
    else if (parser->sheet_node != NULL) {
        /*
          In the body of a section, when we get to a newline we should
          have an accumulator with text in it and a non-null sheet
          (hopefully!).

          We know the current font, so add a span for that font
          containing the relevant text. Then add a <br/> tag.
        */
        finish_span (parser);
        node = xmlNewNode (NULL, BAD_CAST "br");
        xmlAddChild (parser->sheet_node, node);
    }

    parser->newline = TRUE;

    return TRUE;
}

static void
finish_span (YelpManParser *parser)
{
    xmlNodePtr node;

    if (parser->accumulator->str[0] != '\0') {
        node = xmlNewTextChild (parser->sheet_node, NULL,
                                BAD_CAST "span",
                                parser->accumulator->str);
        xmlNewProp (node, BAD_CAST "class", get_font (parser));
        g_string_truncate (parser->accumulator, 0);
    }
}

static guint
dx_to_em_count (YelpManParser *parser, guint dx)
{
    return (int)(dx / ((float)parser->char_width));
}
