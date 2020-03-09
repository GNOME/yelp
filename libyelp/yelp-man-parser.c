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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <gio/gio.h>
#include <gio/gunixinputstream.h>
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

    gchar            *section;   /* The name of the current section */

    /* The width and height of a character according to troff. */
    guint char_width;
    guint char_height;

    /* Count the number of lines we've parsed (needed to get prologue) */
    guint line_no;

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

    /* Count the number of 'N' lines we've seen since the last h
     * command. This is because for some reason N doesn't
     * automatically move the position forward. Thus immediately after
     * one, you see a h24 or the like. Unless there's a space. Then it
     * might be wh48. This is set in parse_N (obviously) and used in
     * parse_h.
     */
    guint N_count;

    /* Keep track of whether the last character was a space. We can't
     * just do this by looking at the last char of accumulator,
     * because if there's a font change, it gets zeroed. This gets set
     * to TRUE by parse_w and is FALSE the rest of the time.
     */
    gboolean last_char_was_space;

    /* Keep track of the size of the last vertical jump - used to tell
     * whether we need to insert extra space above a line.
     */
    gint last_vertical_jump;

    /* The title we read earlier (eg 'Foo(2)') */
    gchar *title_str;
};

static gboolean parser_parse_line (YelpManParser *parser, GError **error);
static gboolean parse_prologue_line (YelpManParser *parser, GError **error);

/* Parsers for different types of line */
typedef gboolean (*LineParser)(YelpManParser *, GError **);
#define DECLARE_LINE_PARSER(name) \
    static gboolean (name) (YelpManParser *parser, GError **error);

DECLARE_LINE_PARSER (parse_xf)
DECLARE_LINE_PARSER (parse_f)
DECLARE_LINE_PARSER (parse_V)
DECLARE_LINE_PARSER (parse_H)
DECLARE_LINE_PARSER (parse_v)
DECLARE_LINE_PARSER (parse_h)
DECLARE_LINE_PARSER (parse_text)
DECLARE_LINE_PARSER (parse_w)
DECLARE_LINE_PARSER (parse_body_text)
DECLARE_LINE_PARSER (parse_n)
DECLARE_LINE_PARSER (parse_N)
DECLARE_LINE_PARSER (parse_C)
DECLARE_LINE_PARSER (parse_p)

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
    { "N", parse_N },
    { "C", parse_C },
    { "p", parse_p },
    { NULL, NULL }
};

/******************************************************************************/
/* Parser helper functions (managing the state of the various parsing
 * bits) */
static void finish_span (YelpManParser *parser);
static guint dx_to_em_count (YelpManParser *parser, guint dx);
static void append_nbsps (YelpManParser *parser, guint k);
static void deal_with_newlines (YelpManParser *parser);
static void new_sheet (YelpManParser *parser);
static void register_title (YelpManParser *parser,
                            const gchar* name, const gchar* section);
static void right_truncate_common (gchar *dst, const gchar *src);
static gboolean cheeky_call_parse_line (YelpManParser *parser,
                                        GError **error,
                                        gchar first_char,
                                        const gchar *text);
static void cleanup_parsed_page (YelpManParser *parser);
static gboolean parse_last_line (YelpManParser *parser, gchar* line);
static void unicode_strstrip (gchar *str);

/*
  A link_inserter takes
    (1) an array of offsets for the different spans within the string
    (2) the match info from the regex match

  It's then responsible for mangling the XML tree to insert the actual
  link. Finally, it should return the offset into the string of the
  end of what it's just dealt with. If necessary, it should also fix
  up offsets to point correctly at the last node inserted.
 */
typedef struct {
    gsize      start, end;
    xmlNodePtr elt;
} offset_elt_pair;

typedef gsize (*link_inserter)(offset_elt_pair *,
                               const GMatchInfo *);

static void fixup_links (YelpManParser *parser,
                         const GRegex *matcher,
                         link_inserter inserter);

static gsize man_link_inserter (offset_elt_pair *offsets,
                                const GMatchInfo *match_info);
static gsize http_link_inserter (offset_elt_pair *offsets,
                                 const GMatchInfo *match_info);

/******************************************************************************/
/* Translations for the 'C' command. This is indeed hackish, but the
 * -Tutf8 output doesn't seem to give include files so we can do this
 * at runtime :-(
 *
 * On my machine, this data's at /usr/share/groff/current/tmac/ in
 * latin1.tmac, unicode.tmac and I worked out the lq and rq from
 * running man: I'm not sure where that comes from!
 */
struct StringPair
{
    const gchar *from;
    gunichar to;
};
static const struct StringPair char_translations[] = {
    { "r!", 161 },
    { "ct", 162 },
    { "Po", 163 },
    { "Cs", 164 },
    { "Ye", 165 },
    { "bb", 166 },
    { "sc", 167 },
    { "ad", 168 },
    { "co", 169 },
    { "Of", 170 },
    { "Fo", 171 },
    { "tno", 172 },
    { "%", 173 },
    { "rg", 174 },
    { "a-", 175 },
    { "de", 176 },
    { "t+-", 177 },
    { "S2", 178 },
    { "S3", 179 },
    { "aa", 180 },
    { "mc", 181 },
    { "ps", 182 },
    { "pc", 183 },
    { "ac", 184 },
    { "S1", 185 },
    { "Om", 186 },
    { "Fc", 187 },
    { "14", 188 },
    { "12", 189 },
    { "34", 190 },
    { "r?", 191 },
    { "`A", 192 },
    { "'A", 193 },
    { "^A", 194 },
    { "~A", 195 },
    { ":A", 196 },
    { "oA", 197 },
    { "AE", 198 },
    { ",C", 199 },
    { "`E", 200 },
    { "'E", 201 },
    { "^E", 202 },
    { ":E", 203 },
    { "`I", 204 },
    { "'I", 205 },
    { "^I", 206 },
    { ":I", 207 },
    { "-D", 208 },
    { "~N", 209 },
    { "`O", 210 },
    { "'O", 211 },
    { "^O", 212 },
    { "~O", 213 },
    { ":O", 214 },
    { "tmu", 215 },
    { "/O", 216 },
    { "`U", 217 },
    { "'U", 218 },
    { "^U", 219 },
    { ":U", 220 },
    { "'Y", 221 },
    { "TP", 222 },
    { "ss", 223 },
    { "`a", 224 },
    { "'a", 225 },
    { "^a", 226 },
    { "~a", 227 },
    { ":a", 228 },
    { "oa", 229 },
    { "ae", 230 },
    { ",c", 231 },
    { "`e", 232 },
    { "'e", 233 },
    { "^e", 234 },
    { ":e", 235 },
    { "`i", 236 },
    { "'i", 237 },
    { "^i", 238 },
    { ":i", 239 },
    { "Sd", 240 },
    { "~n", 241 },
    { "`o", 242 },
    { "'o", 243 },
    { "^o", 244 },
    { "~o", 245 },
    { ":o", 246 },
    { "tdi", 247 },
    { "/o", 248 },
    { "`u", 249 },
    { "'u", 250 },
    { "^u", 251 },
    { ":u", 252 },
    { "'y", 253 },
    { "Tp", 254 },
    { ":y", 255 },
    { "hy", '-' },
    { "oq", '`' },
    { "cq", '\'' },
    { "lq", 8220 }, // left smart quotes
    { "rq", 8221 }, // right smart quotes
    { "en", 8211 }, // en-dash
    { "em", 8212 }, // em-dash
    { "la", 10216 }, // left angle bracket
    { "ra", 10217 }, // left angle bracket
    { "rs", '\\' },
    { "<=", 8804 }, // < or equal to sign
    { ">=", 8805 }, // > or equal to sign
    { "aq", '\'' },
    { "tm", 8482 }, // trademark symbol
    { NULL, 0 }
};

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
    gint ystdout;
    GError *err = NULL;
    const gchar *argv[] = { "man", "-Z", "-Tutf8", "-EUTF-8", path, NULL };
    gchar **my_argv;

    /* g_strdupv() should accept a "const gchar **". */
    my_argv = g_strdupv ((gchar **) argv);

    if (!g_spawn_async_with_pipes (NULL, my_argv, NULL,
                                   G_SPAWN_SEARCH_PATH, NULL, NULL,
                                   NULL, NULL, &ystdout, NULL, &err)) {
        /* We failed to run the man program. Return a "Huh?" error. */
        *error = g_error_new (YELP_ERROR, YELP_ERROR_UNKNOWN,
                              "%s", err->message);
        g_error_free (err);
        g_strfreev (my_argv);
        return NULL;
    }

    g_strfreev (my_argv);

    return (GInputStream*) g_unix_input_stream_new (ystdout, TRUE);
}

xmlDocPtr
yelp_man_parser_parse_file (YelpManParser *parser,
                            gchar *path,
                            GError **error)
{
    GInputStream *troff_stream;
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

       parser->line_no++;
       ret = parser_parse_line (parser, error);

       g_free (parser->buffer);

       if (!ret) {
           xmlFreeDoc (parser->doc);
           parser->doc = NULL;
           break;
       }
    }

    cleanup_parsed_page (parser);

    g_object_unref (parser->stream);

    return parser->doc;
}

void
yelp_man_parser_free (YelpManParser *parser)
{
    guint k;

    if (parser == NULL)
        return;

    for (k=0; k<MAN_FONTS; k++)
        g_free (parser->font_registers[k]);
    g_string_free (parser->accumulator, TRUE);
    g_free (parser->title_str);
    g_free (parser->section);
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
    if (k >= MAN_FONTS) {
        g_warning ("Tried to set nonexistant font register %u to %s",
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
    if (k >= MAN_FONTS ||
        parser->font_registers[k] == NULL) {

        g_warning ("Tried to get nonexistant font register %u", k);

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
    const struct LineParsePair *p;

    if (parser->line_no <= 3)
        return parse_prologue_line (parser, error);

    p = line_parsers;
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
    if (parser->line_no != 2) return TRUE;

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
    gchar name[11];
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
    parser->last_vertical_jump += dy;
    parser->vpos += dy;
    return TRUE;
}

static gboolean
parse_h (YelpManParser *parser, GError **error)
{
    guint dx;
    int k;

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
     * whatever, so use the last_char_was_space flag
     * but... unfortunately some documents actually use stuff like
     * wh96 for spacing (eg the lists in perl(1)). So (very hackish!),
     * ignore double spaces, since that's probably just been put in to
     * make the text justified (eugh), but allow bigger jumps.
     *
     * Incidentally, the perl manual here has bizarre gaps in the
     * synopsis section. God knows why, but man displays them too so
     * it's not our fault! :-)
     */
    k = dx_to_em_count (parser, dx);

    if ((!parser->last_char_was_space) || (k > 2)) {

        k -= parser->N_count;
        if (k < 0) k = 0;

        append_nbsps (parser, k);
    }

    parser->N_count = 0;

    return TRUE;
}

static gboolean
parse_V (YelpManParser *parser, GError **error)
{
    guint y;
    if (SSCANF ("V%u", 1, &y)) {
        RAISE_PARSE_ERROR ("Invalid V line from troff: %s");
    }
    parser->last_vertical_jump += y - parser->vpos;
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
    const gchar *acc;

    /*
      Sneakily, this might get called with something other than t
      starting the buffer: see parse_C and parse_N.
    */
    if (parser->buffer[0] == 't') {
        parser->N_count = 0;
    }

    if (parser->state == START) {
        /* This should be the 'Title String(1)' line. It might come in
         * chunks (for example, it might be more than one line
         * long!). So just read bits until we get a (blah) bit: stick
         * everything in the accumulator and check for
         * parentheses. When we've got some, stick the parsed title in
         * the header and switch to HAVE_TITLE.
         *
         * The parse_n code will error out if we didn't manage to get
         * a title before the first newline and otherwise is in charge
         * of switching to body-parsing mode.
         */
        g_string_append (parser->accumulator, parser->buffer+1);

        acc = parser->accumulator->str;

        section = strchr (acc, '(');

        if (section) {
            section++;
            tmp = strchr (section, ')');
        }

        if (section && tmp) {
            /* We've got 'Blah (3)' or the like in the accumulator */
            if (*(tmp+1) != '\0') {
                RAISE_PARSE_ERROR ("Don't understand title line: '%s'");
            }
            parser->state = HAVE_TITLE;
            parser->title_str = g_strdup (acc);

            text = g_strndup (acc, (section - 1) - acc);
            section = g_strndup (section, tmp - section);

            register_title (parser, text, section);

            g_string_truncate (parser->accumulator, 0);

            g_free (text);
            parser->section = section;
        }

        return TRUE;
    }

    if (parser->state == BODY)
        return parse_body_text (parser, error);

    /* In state HAVE_TITLE */
    else {
        /* We expect (maybe!) to get some lines in between the two
         * occurrences of the title itself. So collect up all the text
         * we get and then we'll remove the copy of the title at the
         * end (hopefully) when we find a newline in parse_n.
         */
        g_string_append (parser->accumulator, parser->buffer+1);
        return TRUE;
    }
}

static gboolean
parse_body_text (YelpManParser *parser, GError **error)
{
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
    if (parser->section_state == SECTION_BODY &&
        (!parser->section_node || (parser->hpos == 0))) {
        g_string_truncate (parser->accumulator, 0);
        /* End the current sheet & section */
        parser->section_state = SECTION_TITLE;
        parser->sheet_node = NULL;

        parser->section_node =
            xmlAddChild (xmlDocGetRootElement (parser->doc),
                         xmlNewNode (NULL, BAD_CAST "section"));
    }

    if (parser->section_state != SECTION_TITLE) {
        deal_with_newlines (parser);
    }

    g_string_append (parser->accumulator, parser->buffer+1);

    /* Move hpos forward per char */
    parser->hpos += strlen (parser->buffer+1) * parser->char_width;

    parser->last_char_was_space = FALSE;

    return TRUE;
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
    parser->last_char_was_space = TRUE;

    ret = parser_parse_line (parser, error);

    parser->buffer--;
    return ret;
}

static gboolean
parse_n (YelpManParser *parser, GError **error)
{
    xmlNodePtr node;

    /* When we're in the header, the parse_n is responsible for
     * switching to body text. (See the body of parse_text() for more
     * of an explanation).
     */
    if (parser->state == START) {
        /* Oh no! We've not got a proper title yet! Ho hum, let's
           stick whatever's going into a 'title title' and have a null
           section. Sob.
        */
        register_title (parser,
                        parser->accumulator->str,
                        "unknown section");
        g_string_truncate (parser->accumulator, 0);
        parser->state = BODY;
        return TRUE;
    }

    if (parser->state == HAVE_TITLE) {
        /* What we've got so far is the manual's collection, followed
           by the title again. So we want to get rid of the latter if
           possible...
        */
        right_truncate_common (parser->accumulator->str,
                               parser->title_str);
        unicode_strstrip (parser->accumulator->str);

        xmlNewTextChild (parser->header,
                         NULL, BAD_CAST "collection",
                         BAD_CAST parser->accumulator->str);
        g_string_truncate (parser->accumulator, 0);
        parser->state = BODY;
        parser->section_state = SECTION_BODY;
        return TRUE;
    }

    /* parser->state == BODY */
    if (parser->section_state == SECTION_TITLE) {

        g_strchomp (parser->accumulator->str);
        xmlNewTextChild (parser->section_node, NULL,
                         BAD_CAST "title",
                         BAD_CAST parser->accumulator->str);
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
    parser->last_char_was_space = FALSE;

    return TRUE;
}

static void
finish_span (YelpManParser *parser)
{
    xmlNodePtr node;

    if (parser->accumulator->str[0] != '\0') {
        node = xmlNewTextChild (parser->sheet_node, NULL,
                                BAD_CAST "span",
                                BAD_CAST parser->accumulator->str);
        xmlNewProp (node, BAD_CAST "class",
                    BAD_CAST get_font (parser));
        g_string_truncate (parser->accumulator, 0);
    }
}

static guint
dx_to_em_count (YelpManParser *parser, guint dx)
{
    return (int)(dx / ((float)parser->char_width));
}

static gboolean
parse_N (YelpManParser *parser, GError **error)
{
    gint n;
    gchar tmp[2];

    if (SSCANF ("N%i", 1, &n)) {
        RAISE_PARSE_ERROR ("Strange format for N line: %s");
    }
    if (n > 127) {
        RAISE_PARSE_ERROR ("N line has non-7-bit character: %s");
    }
    if (n < -200) {
        RAISE_PARSE_ERROR ("Bizarrely many nbsps in N line: %s");
    }

    if (n < 0) {
        append_nbsps (parser, -n);
        parser->N_count += -n;
        return TRUE;
    }

    parser->N_count++;

    tmp[0] = (gchar)n;
    tmp[1] = '\0';

    return cheeky_call_parse_line (parser, error, 'N', tmp);
}

static void
append_nbsps (YelpManParser *parser, guint k)
{
    for (; k > 0; k--) {
        /* 0xc2 0xa0 is nonbreaking space in utf8 */
        g_string_append_c (parser->accumulator, 0xc2);
        g_string_append_c (parser->accumulator, 0xa0);
    }
}

static gboolean
parse_C (YelpManParser *parser, GError **error)
{
    gchar name[17];
    gunichar code = 0;
    guint k;
    gint len;

    if (SSCANF ("C%16s", 1, name)) {
        RAISE_PARSE_ERROR ("Can't understand special character: %s");
    }

    for (k=0; char_translations[k].from; k++) {
        if (g_str_equal (char_translations[k].from, name)) {
            code = char_translations[k].to;
            break;
        }
    }
    if (sscanf (name, "u%x", &k) == 1) {
        code = k;
    }

    if (!code) {
        g_warning ("Couldn't parse troff special character: '%s'",
                   name);
        code = 65533; /* Unicode replacement character */
    }

    /* Output buffer must be length >= 6. 16 >= 6, so we're ok. */
    len = g_unichar_to_utf8 (code, name);
    name[len] = '\0';

    parser->N_count++;

    return cheeky_call_parse_line (parser, error, 'C', name);
}

static void
deal_with_newlines (YelpManParser *parser)
{
    /*
      If newline is true, this is the first word on a line.

      In which case, we check to see whether hpos agrees with the
      current sheet's indent. If so (or if there isn't a sheet yet!),
      we just add to the accumulator. If not, start a new sheet with
      the correct indent.

      If we aren't the first word on the line, just add to the
      accumulator.
    */
    gchar tmp[64];
    guint jump_lines;
    gboolean made_sheet = FALSE, dont_jump = FALSE;

    /* This only happens at the start of a section, where there's
       already a gap
    */
    if (!parser->sheet_node) {
        dont_jump = TRUE;
    }

    if ((!parser->sheet_node) ||
        (parser->newline && (parser->hpos != parser->sheet_indent))) {
        new_sheet (parser);
        made_sheet = TRUE;
    }

    if (parser->newline) {
        if ((parser->last_vertical_jump > 0) && (!dont_jump)) {
            jump_lines =
                parser->last_vertical_jump/parser->char_height;
        } else {
            jump_lines = 1;
        }

        if (jump_lines > 1) {
            if (!made_sheet) new_sheet (parser);
            made_sheet = TRUE;
        }

        snprintf (tmp, 64, "%u", dx_to_em_count (parser, parser->hpos));
        xmlNewProp (parser->sheet_node,
                    BAD_CAST "indent", BAD_CAST tmp);

        if (made_sheet) {
            snprintf (tmp, 64, "%u", jump_lines-1);
            xmlNewProp (parser->sheet_node,
                        BAD_CAST "jump", BAD_CAST tmp);
        }
    }

    parser->newline = FALSE;
    parser->last_vertical_jump = 0;
}

static gboolean
parse_p (YelpManParser *parser, GError **error)
{
    parser->vpos = 0;
    parser->hpos = 0;
    return TRUE;
}

static void
new_sheet (YelpManParser *parser)
{
    /* We don't need to worry about finishing the current sheet,
       since the accumulator etc. get cleared on newlines and we
       know we're at the start of a line.
    */
    parser->sheet_node =
        xmlAddChild (parser->section_node,
                     xmlNewNode (NULL, BAD_CAST "sheet"));
    parser->sheet_indent = parser->hpos;
}

static void
register_title (YelpManParser *parser,
                const gchar* name, const gchar* section)
{
    xmlNewTextChild (parser->header,
                     NULL, BAD_CAST "title", BAD_CAST name);
    xmlNewTextChild (parser->header,
                     NULL, BAD_CAST "section", BAD_CAST section);
}

static void
right_truncate_common (gchar *dst, const gchar *src)
{
    guint len_src = strlen (src);
    guint len_dst = strlen (dst);

    guint k = (len_src < len_dst) ? len_src - 1 : len_dst - 1;

    dst += len_dst - 1;
    src += len_src - 1;

    while (k > 0) {
        if (*dst != *src) break;
        *dst = '\0';

        k--;
        dst--;
        src--;
    }
}

static gboolean
cheeky_call_parse_line (YelpManParser *parser, GError **error,
                        gchar first_char, const gchar* text)
{
    /* Do a cunning trick. There's all sorts of code that parse_text
     * does, which we don't want to duplicate in parse_N and
     * parse_C. So feed a buffer back to parse_text. Tada! Start it
     * with "C" or "N" rather than "t" so clever stuff in parse_text
     * can tell the difference.
     */
    gchar *tmp;
    gboolean ret;
    guint len = strlen (text);

    tmp = parser->buffer;
    parser->buffer = g_new (gchar, 2 + len);
    parser->buffer[0] = first_char;
    strncpy (parser->buffer + 1, text, len + 1);

    ret = parse_text (parser, error);

    g_free (parser->buffer);
    parser->buffer = tmp;

    return ret;
}

static void
cleanup_parsed_page (YelpManParser *parser)
{
    /* First job: the last line usually has the version, date and
     * title (again!). The code above misunderstands and parses this
     * as a section, so we need to "undo" this and stick the data in
     * the header where it belongs.
     *
     * parser->section_node should still point to it. We assume this
     * has happened if it has exactly one child element (the <title>
     * tag)
     */
    gchar *lastline;
    GRegex *regex;
    gchar regex_string [1024];

    if (xmlChildElementCount (parser->section_node) == 1) {
        lastline = (gchar *)xmlNodeGetContent (parser->section_node);

        /* If parse_last_line works, it sets the data from it in the
           <header> tag, so delete the final section. */
        if (parse_last_line (parser, lastline)) {
            xmlUnlinkNode (parser->section_node);
            xmlFreeNode (parser->section_node);
        }
        else {
            /* Oh dear. This would be unexpected and doesn't seem to
               happen with man on my system. But we probably shouldn't
               ditch the info, so let's leave the <section> tag and
               print a warning message to the console.
            */
            g_warning ("Unexpected final line in man document (%s)\n",
                       lastline);
        }

        xmlFree (lastline);
    }

    /* Next job: Go through and stick the links in. Text that looks
     * like man(1) should be converted to a link to man:man(1) and
     * urls should also be linkified.
     *
     * Unfortunately, it's not entirely clear what constitutes a valid
     * section. All sections must be alphanumeric and the logic we use
     * to avoid extra hits (eg "one or more widget(s)") is that either
     * the section must start with a digit or (if the current section
     * doesn't) must start with the same letter as the current
     * section.
     */
    snprintf (regex_string, 1024,
              "([a-zA-Z0-9\\-_.:]+)\\(((%c|[0-9])[a-zA-Z0-9]*)\\)",
              parser->section ? parser->section[0] : '0');
    regex = g_regex_new (regex_string, 0, 0, NULL);
    g_return_if_fail (regex);
    fixup_links (parser, regex, man_link_inserter);
    g_regex_unref (regex);

    /* Now for http:// links.
     */
    regex = g_regex_new ("https?:\\/\\/[\\w\\-_]+(\\.[\\w\\-_]+)+"
                         "([\\w\\-\\.,@?^=%&:/~\\+#]*"
                         "[\\w\\-\\@?^=%&/~\\+#])?",
                         0, 0, NULL);
    g_return_if_fail (regex);
    fixup_links (parser, regex, http_link_inserter);
    g_regex_unref (regex);
}

static gchar *
skip_whitespace (gchar *text)
{
    while (g_unichar_isspace (g_utf8_get_char (text))) {
        text = g_utf8_next_char (text);
    }
    return text;
}

static gchar *
last_non_whitespace (gchar *text)
{
    gchar *end = text + strlen(text);
    gchar *prev;

    prev = g_utf8_find_prev_char (text, end);
    if (!prev) {
        /* The string must have been zero-length. */
        return NULL;
    }

    while (g_unichar_isspace (g_utf8_get_char (prev))) {
        end = prev;
        prev = g_utf8_find_prev_char (text, prev);
        if (!prev) return NULL;
    }
    return end;
}

static gchar *
find_contiguous_whitespace (gchar *text, guint ws_len)
{
    guint counter = 0;
    gchar *ws_start = NULL;
    while (*text) {
        if (g_unichar_isspace (g_utf8_get_char (text))) {
            if (!counter) ws_start = text;
            counter++;
        }
        else counter = 0;

        if (counter == ws_len) return ws_start;

        text = g_utf8_next_char (text);
    }
    return NULL;
}

static gboolean
parse_last_line (YelpManParser *parser, gchar* line)
{
    /* We expect a line of the form
           '1.2.3      blah 2009       libfoo(1)'
       where the spaces are all nbsp's.

       Look for a gap of at least 3 in a row. If we find that, expand
       either side and declare the stuff before to be the version
       number and then the stuff afterwards to be the start of the
       date. Then do the same thing on the next gap, if there is one.
    */
    gchar *gap, *date_start;

    gchar *version;
    gchar *date;

    gap = find_contiguous_whitespace (line, 3);
    if (!gap) return FALSE;

    version = g_strndup (line, gap - line);

    date_start = skip_whitespace (gap);

    gap = find_contiguous_whitespace (date_start, 3);
    if (!gap) return FALSE;

    date = g_strndup (date_start, gap - date_start);

    xmlNewProp (parser->header, BAD_CAST "version", BAD_CAST version);
    xmlNewProp (parser->header, BAD_CAST "date", BAD_CAST date);

    g_free (version);
    g_free (date);

    return TRUE;
}

/* This should work like g_strstrip, but that's an ASCII-only version
 * and I want to strip the nbsp's that I so thoughtfully plaster
 * stuff with...
 */
static void
unicode_strstrip (gchar *str)
{
    gchar *start, *end;

    if (str == NULL) return;

    end = last_non_whitespace (str);

    if (!end) {
        /* String is zero-length or entirely whitespace */
        *str = '\0';
        return;
    }
    start = skip_whitespace (str);

    memmove (str, start, end - start);
    *(str + (end - start)) = '\0';
}

static void
sheet_fixup_links (xmlNodePtr sheet,
                   const GRegex *regex, link_inserter inserter)
{
    /*
      This works as follows: grab (<span>) nodes from a sheet in
      order and stick their contents into a string. Since a sheet
      won't be ludicrously long, we can just grab everything and then
      work over it, but we need to keep track of which node points at
      which bit of the string so we can call inserter helpfully. To do
      so, use byte offsets, since that seems less likely to go
      horribly wrong!
    */
    GString *accumulator = g_string_new ("");
    xmlNodePtr span;
    xmlChar *tmp;
    gsize offset = 0;
    gsize len;
    offset_elt_pair pair;
    GMatchInfo *match_info;

    /* Make pairs zero-terminated so that code can iterate through it
     * looking for something with elt = NULL. */
    GArray *pairs = g_array_new (TRUE, FALSE,
                                 sizeof (offset_elt_pair));

    g_return_if_fail (regex);
    g_return_if_fail (inserter);
    g_return_if_fail (sheet);

    for (span = sheet->children; span != NULL; span = span->next) {
        if (span->type != XML_ELEMENT_NODE) continue;

        if (strcmp ((const char*) span->name, "span") != 0) {

            if (strcmp ((const char*) span->name, "a") == 0)
                continue;

            if (strcmp ((const char*) span->name, "br") == 0) {
                /* If the last character in the accumulator is a
                 * hyphen, we don't want to include that in the link
                 * we make. If not, append a newline to the
                 * accumulator (so we don't mistakenly make links from
                 * "see\nthis(2)" to seethis(2).
                 *
                 * Either way, we add the <br> to the list of pairs
                 * since we might need to do stuff with it if it's in
                 * the middle of a link.
                 */
                len = strlen (accumulator->str);
                if (len > 0 && accumulator->str [len-1] == '-') {
                    g_string_truncate (accumulator, len - 1);
                    offset--;
                }
                else {
                    g_string_append_c (accumulator, '\n');
                    offset++;
                }
                pair.start = offset;
                pair.end = offset;
                pair.elt = span; /* Er, br in fact. */
                g_array_append_val (pairs, pair);

                continue;
            }

            g_warning ("Expected all child elements to be "
                       "<span>, <br> or <a>, but "
                       "have found a <%s>.",
                       (gchar *) span->name);
            continue;
        }

        tmp = xmlNodeGetContent (span);
        g_string_append (accumulator, (gchar *) tmp);
        len = strlen ((const char*) tmp);

        pair.start = offset;
        pair.end = offset + len;
        pair.elt = span;

        g_array_append_val (pairs, pair);

        offset += len;
        xmlFree (tmp);
    }

    /* We've got the data. Now try to match the regex against it as
     * many times as possible
     */
    offset = 0;
    g_regex_match_full (regex, accumulator->str,
                        -1, offset, 0, &match_info, NULL);
    while (g_match_info_matches (match_info)) {
        offset = inserter ((offset_elt_pair *)pairs->data,
                           match_info);

        g_match_info_free (match_info);

        g_regex_match_full (regex, accumulator->str,
                            -1, offset, 0, &match_info, NULL);
    }

    g_string_free (accumulator, TRUE);
    g_array_unref (pairs);
}

static void
fixup_links (YelpManParser *parser,
             const GRegex *regex, link_inserter inserter)
{
    /* Iterate over all the <sheet>'s in the xml document */
    xmlXPathContextPtr context;
    xmlXPathObjectPtr path_obj;
    xmlNodeSetPtr nodeset;
    gint i;

    context = xmlXPathNewContext (parser->doc);
    g_return_if_fail (context);

    path_obj = xmlXPathEvalExpression (BAD_CAST "//sheet", context);
    g_return_if_fail (path_obj);

    nodeset = path_obj->nodesetval;
    g_return_if_fail (nodeset);

    for (i = 0; i < nodeset->nodeNr; ++i) {
        sheet_fixup_links (nodeset->nodeTab[i], regex, inserter);
    }

    xmlXPathFreeObject (path_obj);
    xmlXPathFreeContext (context);
}

/*
  This inserts new_child under parent. If older_sibling is non-NULL,
  we stick it immediately after it. Otherwise, insert as the first
  child of the parent.

  Returns the inserted child.
 */
static xmlNodePtr
insert_child_after (xmlNodePtr parent, xmlNodePtr older_sibling,
                    xmlNodePtr new_child)
{
    g_return_val_if_fail (parent && new_child, new_child);

    if (older_sibling) {
        xmlAddNextSibling (older_sibling, new_child);
    }
    else if (parent->children == NULL) {
        xmlAddChild (parent, new_child);
    }
    else {
        xmlAddPrevSibling (parent->children, new_child);
    }

    return new_child;
}

static void
copy_prop (xmlNodePtr to, xmlNodePtr from, const xmlChar *name)
{
    xmlChar *prop = xmlGetProp (from, name);
    g_return_if_fail (prop);
    xmlSetProp (to, name, prop);
    xmlFree (prop);
}

static gsize
do_node_replacement (xmlNodePtr anchor_node,
                     offset_elt_pair *offsets,
                     gsize startpos, gsize endpos)
{
    xmlNodePtr node, sibling_before;
    gchar *gtmp;
    xmlChar *xtmp, *xshort;
    gsize look_from;

    /* Find the first element by searching through offsets. I suppose
     * a binary search would be cleverer, but I doubt that this will
     * take significant amounts of time.
     *
     * We should never fall off the end, but (just in case) the GArray
     * that holds the offsets is zero-terminated and elt should never
     * be NULL so we can stop if necessary
     */
    while ((offsets->end <= startpos) && offsets->elt) {
        offsets++;
    }
    g_return_val_if_fail (offsets->elt, endpos);

    /* xtmp is NULL by default, but we do this here so that if we read
     * the node in the if block below, we don't have to do it a second
     * time.
     */
    xtmp = NULL;
    sibling_before = offsets->elt->prev;
    look_from = startpos;

    /* Maybe there's text in the relevant span before the start of
     * the stuff we want to replace with a link.
     */
    if (startpos > offsets->start) {
        node = xmlNewNode (NULL, BAD_CAST "span");
        copy_prop (node, offsets->elt, BAD_CAST "class");

        xtmp = xmlNodeGetContent (offsets->elt);
        gtmp = g_strndup ((const gchar*)xtmp, startpos - offsets->start);
        xmlNodeAddContent (node, BAD_CAST gtmp);
        g_free (gtmp);

        sibling_before = insert_child_after (offsets->elt->parent,
                                             sibling_before, node);
    }

    insert_child_after (offsets->elt->parent,
                        sibling_before, anchor_node);

    /* The main loop. Here we work over each span that overlaps with
     * the link we're adding. We add a similar span as a child of the
     * anchor node and then delete the existing one.  */
    while (look_from < endpos) {
        if (!xtmp) xtmp = xmlNodeGetContent (offsets->elt);

        if (strcmp ((const gchar*)offsets->elt->name, "br") == 0) {
            node = xmlNewChild (anchor_node,
                                NULL, BAD_CAST "br", NULL);
            xmlUnlinkNode (offsets->elt);
            xmlFreeNode (offsets->elt);
            xmlFree (xtmp);
            xtmp = NULL;
            offsets++;
        }
        else if (endpos < offsets->end) {
            xshort = BAD_CAST g_strndup ((const gchar*)xtmp,
                                         endpos - offsets->start);

            node = xmlNewChild (anchor_node, NULL, BAD_CAST "span",
                                xshort + (look_from-offsets->start));
            copy_prop (node, offsets->elt, BAD_CAST "class");

            node = xmlNewNode (NULL, BAD_CAST "span");
            xmlNodeAddContent (node,
                               xtmp + (endpos - offsets->start));
            copy_prop (node, offsets->elt, BAD_CAST "class");
            xmlAddNextSibling (anchor_node, node);

            xmlFree (xshort);

            xmlUnlinkNode (offsets->elt);
            xmlFreeNode (offsets->elt);
            xmlFree (xtmp);
            xtmp = NULL;

            offsets->start = endpos;
            offsets->elt = node;
        }
        else {
            node = xmlNewChild (anchor_node, NULL, BAD_CAST "span",
                                xtmp + (look_from - offsets->start));
            copy_prop (node, offsets->elt, BAD_CAST "class");

            xmlUnlinkNode (offsets->elt);
            xmlFreeNode (offsets->elt);
            xmlFree (xtmp);
            xtmp = NULL;
            offsets++;
        }

        if (!offsets->elt) {
            /* We got to the end of a sheet and of the stuff we're
             * doing at the same time
             */
            return endpos;
        }

        look_from = offsets->start;
    }

    return offsets->start;
}

static gsize
do_link_insertion (const gchar *url,
                   offset_elt_pair *offsets,
                   gsize startpos, gsize endpos)
{
    xmlNodePtr anchor_node = xmlNewNode (NULL, BAD_CAST "a");

    xmlNewProp (anchor_node, BAD_CAST "href", BAD_CAST url);

    return do_node_replacement (anchor_node, offsets,
                                startpos, endpos);
}

static gsize
man_link_inserter (offset_elt_pair *offsets,
                   const GMatchInfo *match_info)
{
    gchar *name, *section;
    gchar url[1024];

    gint startpos, endpos;

    g_match_info_fetch_pos (match_info, 0, &startpos, &endpos);

    name = g_match_info_fetch (match_info, 1);
    section = g_match_info_fetch (match_info, 2);

    g_return_val_if_fail (name && section, endpos);

    snprintf (url, 1024, "man:%s(%s)", name, section);

    g_free (name);
    g_free (section);

    return do_link_insertion (url, offsets, startpos, endpos);
}

static gsize
http_link_inserter (offset_elt_pair *offsets,
                    const GMatchInfo *match_info)
{
    gchar *url;
    gint startpos, endpos;
    gsize ret;

    url = g_match_info_fetch (match_info, 0);
    g_match_info_fetch_pos (match_info, 0, &startpos, &endpos);

    ret = do_link_insertion (url, offsets, startpos, endpos);

    g_free (url);

    return ret;
}
