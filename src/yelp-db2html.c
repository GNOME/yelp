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

/* stylesheet location based on Linux Standard Base      *
 * http://www.linuxbase.org/spec/gLSB/gLSB/sgmlr002.html */
#define STYLESHEET_PATH DATADIR"/sgml/docbook/yelp"
#define STYLESHEET STYLESHEET_PATH"/yelp-customization.xsl"

gboolean
yelp_db2html_convert (YelpURI             *uri,
                      xmlOutputBufferPtr   buf, 
                      GError             **error)
{
	static xsltStylesheetPtr  stylesheet = NULL;
        xmlDocPtr                 db_doc;
        xmlDocPtr                 final_doc;
	const gchar              *params[16 + 1];
	gchar                    *pathname;
        GTimer                   *timer;
        
	db_doc    = NULL;

        timer = g_timer_new ();

	d(g_print ("Convert file: %s\n", yelp_uri_get_path (uri)));
	
	/* libxml housekeeping */
	xmlSubstituteEntitiesDefault(1);
	xmlLoadExtDtdDefaultValue = 1;

	/* parse the stylesheet */
        if (!stylesheet) {
                stylesheet  = xsltParseStylesheetFile (STYLESHEET);
        }
        
        if (!stylesheet) {
                g_set_error (error,
                             YELP_ERROR,
                             YELP_ERROR_DOCBOOK_2_HTML,
                             _("Error while parsing the stylesheet, make sure you have your docbook environment setup correctly."));
                             
                /* FIXME: Set GError */
                return FALSE;
        }

	if (yelp_uri_get_type (uri) == YELP_URI_TYPE_DOCBOOK_XML) {
		db_doc = xmlParseFile (yelp_uri_get_path (uri));
	} else {
                db_doc = docbParseFile (yelp_uri_get_path (uri), "UTF-8");
	}

	if (db_doc == NULL) {
                /* FIXME: Set something in the GError */
                g_set_error (error,
                             YELP_ERROR,
                             YELP_ERROR_DOCBOOK_2_HTML,
                             _("Couldn't parse the document '%s'."),
                             yelp_uri_get_path (uri));
                
                return FALSE;
	}
	
	/* retrieve path component of filename passed in at
	   command line */
	pathname = g_path_get_dirname (yelp_uri_get_path (uri));
        
	/* set params to be passed to stylesheet */
	params[0] = "gdb_docname";
	params[1] = g_strconcat("\"", yelp_uri_get_path (uri), "\"", NULL) ;
	params[2] = "gdb_pathname";
	params[3] = g_strconcat("\"", pathname, "\"", NULL) ;
	params[4] = "gdb_stylesheet_path";
        params[5] = STYLESHEET_PATH;
        params[6] = NULL;
        
        g_free (pathname);

	if (yelp_uri_get_section (uri)) {
  		params[6] = "gdb_rootid"; 
		params[7] = g_strconcat("\"", 
                                        yelp_uri_get_section (uri), 
                                        "\"", 
                                        NULL) ;
		params[8] = NULL;
	}

        final_doc = xsltApplyStylesheet (stylesheet, db_doc, params);

        xmlFree (db_doc);

        if (!final_doc) {
                g_set_error (error,
                             YELP_ERROR,
                             YELP_ERROR_DOCBOOK_2_HTML,
                             _("Error while applying the stylesheet."));
                return FALSE;
        }

	/* Output the results to the OutputBuffer */
        xsltSaveResultTo (buf, final_doc, stylesheet);

        xmlFree (final_doc);

        d(g_print ("docbook -> html took: %f s\n", g_timer_elapsed (timer, 0)));
	return TRUE;
}
