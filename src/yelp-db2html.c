/*
 * gnome-db2html3 - by John Fleck - based on Daniel Veillard's
 * xsltproc: user program for the XSL Transformation engine
 * 
 * Copyright (C) John Fleck, 2001
 * Copyright (C) Mikael Hallendal, 2002
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.,  59 Temple Place - Suite 330, Cambridge, MA 02139, USA.
 *
 */

#include <config.h>
#include <glib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <libgnome/gnome-i18n.h>
#include <libxml/xmlversion.h>
#include <libxml/xmlmemory.h>
#include <libxml/debugXML.h>
#include <libxml/HTMLtree.h>
#include <libxml/DOCBparser.h>
#include <libxml/catalog.h>
#include <libxslt/xsltconfig.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include "yelp-error.h"
#include "yelp-db2html.h"

#define d(x)

gboolean
yelp_db2html_convert (const gchar         *document,
                      xmlOutputBufferPtr   buf, 
                      GError             **error)
{
	static xsltStylesheetPtr gdb_xslreturn = NULL;
        char              *gdb_docname;
        xmlDocPtr          gdb_doc;
        xmlDocPtr          gdb_results;
	const char        *params[16 + 1];
	char              *gdb_pathname;    /* path to the file to be parsed */
	char              *gdb_rootid; /* id of sect, chapt, etc to be parsed */
	char             **gdb_split_docname;  /* placeholder for file type determination */
        char              *ptr;
	gboolean           has_rootid;
        GTimer            *timer;
        
	has_rootid = FALSE; 
	gdb_doc = NULL;
	gdb_rootid = NULL;

        timer = g_timer_new ();

        gdb_docname = g_strdup (document);

	d(g_print ("Convert file: %s\n", gdb_docname));

	
	/* check to see if gdb_docname has a ?sectid included */
	for (ptr = gdb_docname; *ptr; ptr++){
		if (*ptr == '?') {
			*ptr = '\000';
			if (*(ptr + 1)) {
				gdb_rootid = ptr;
				has_rootid = TRUE;
			}
		}
	}

	/* libxml housekeeping */
	xmlSubstituteEntitiesDefault(1);

	/* parse the stylesheet */
        if (!gdb_xslreturn) {
                gchar *gdb_stylesheet;
                /* stylesheet location based on Linux Standard Base      *
                 * http://www.linuxbase.org/spec/gLSB/gLSB/sgmlr002.html */
                gdb_stylesheet = g_strconcat (PREFIX "/share/sgml/docbook/gnome-customization-0.1/gnome-customization.xsl", NULL);

                gdb_xslreturn  = xsltParseStylesheetFile ((const xmlChar *) gdb_stylesheet);
                g_free (gdb_stylesheet);
        }
        
        if (!gdb_xslreturn) {
                g_set_error (error,
                             YELP_ERROR,
                             YELP_ERROR_DOCBOOK_2_HTML,
                             _("Error while parsing the stylesheet, make sure you have your docbook environment setup correctly."));
                             
                /* FIXME: Set GError */
                return FALSE;
        }
        
	/* check the file type by looking at name
	 * FIXME - we need to be more sophisticated about this
	 * then parse as either xml or sgml */

	gdb_split_docname = g_strsplit(gdb_docname, ".", 2);

        if (!g_file_test (gdb_docname, G_FILE_TEST_EXISTS)) {
                g_set_error (error, 
                             YELP_ERROR,
                             YELP_ERROR_DOCBOOK_2_HTML,
                             _("The document '%s' does not exist"),
                             gdb_docname);
                return FALSE;
        }

	if (!strcmp(gdb_split_docname[1], "sgml")) {
			gdb_doc = docbParseFile(gdb_docname, "UTF-8");
	} else {
		(gdb_doc = xmlParseFile(gdb_docname));
	}
	if (gdb_doc == NULL) {
                /* FIXME: Set something in the GError */
                g_set_error (error,
                             YELP_ERROR,
                             YELP_ERROR_DOCBOOK_2_HTML,
                             _("Couldn't parse the document '%s'."),
                             gdb_docname);
                
                g_free (gdb_docname);

                return FALSE;
	}
	
	/* retrieve path component of filename passed in at
	   command line */
	gdb_pathname = g_path_get_dirname (gdb_doc->URL);
	gdb_docname  = g_path_get_basename (gdb_doc->URL);

	for (ptr = gdb_docname; *ptr; ptr++){
		if (*ptr == '.') {
			*ptr = '\000';
			
		}
	}

	/* set params to be passed to stylesheet */
	params[0] = "gdb_docname";
	params[1] = g_strconcat("\"", gdb_doc->URL, "\"", NULL) ;
	params[2] = "gdb_pathname";
	params[3] = g_strconcat("\"", gdb_pathname, "\"", NULL) ;
	params[4] = NULL;

	if (has_rootid) {
                /* FIXME: Change this when sander has the new 
                   stylesheets ready */
                params[4] = "rootid";
/*  		params[4] = "gdb_rootid"; */
		params[5] = g_strconcat("\"", gdb_rootid + 1, "\"", NULL) ;
		params[6] = NULL;
	}

        gdb_results = xsltApplyStylesheet(gdb_xslreturn, gdb_doc, params);

        if (!gdb_results) {
                g_set_error (error,
                             YELP_ERROR,
                             YELP_ERROR_DOCBOOK_2_HTML,
                             _("Error while applying the stylesheet."));
                return FALSE;
        }

	/* Output the results to the OutputBuffer */
        xsltSaveResultTo (buf, gdb_results, gdb_xslreturn);

        xmlOutputBufferClose (buf);

        g_free (gdb_docname);
        xmlFree (gdb_results);

        d(g_print ("docbook -> html took: %f s\n", g_timer_elapsed (timer, 0)));
	return TRUE;
}
