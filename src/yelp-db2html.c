/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
#ifdef LIBXML_XINCLUDE_ENABLED
#  include <libxml/xinclude.h>
#endif
#include <libxslt/xsltconfig.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include <libgnomevfs/gnome-vfs-init.h>

#include "yelp-error.h"

/* stylesheet location based on Linux Standard Base      *
 * http://www.linuxbase.org/spec/gLSB/gLSB/sgmlr002.html */
#define DB_STYLESHEET_PATH DATADIR"/sgml/docbook/yelp/docbook"
#define STYLESHEET DATADIR"/sgml/docbook/yelp/yelp-customization.xsl"

/* xmlParserInput * */
/* external_entity_loader (const char    *URL,  */
/*                         const char    *ID, */
/*                         xmlParserCtxt *ctxt); */

gint 
main (gint argc, gchar **argv) 
{
	xsltStylesheet *stylesheet = NULL;
/*         YelpURI        *uri; */
        xmlDocPtr       db_doc;
        xmlDocPtr       final_doc;
	const gchar    *params[16 + 1];
	gchar          *pathname;
        gchar          *docpath;
	db_doc = NULL;

        putenv ("XML_CATALOG_FILES=" DATADIR "/yelp/catalog");

        if (argc < 2) {
                g_print ("Usage 'yelp-db2html url'\n");
                exit (1);
        }

        docpath = argv[1];

	/* libxml housekeeping */
	xmlSubstituteEntitiesDefault(1);
	xmlLoadExtDtdDefaultValue = 1;

	/* parse the stylesheet */
        stylesheet  = xsltParseStylesheetFile (STYLESHEET);
        
        if (!stylesheet) {
                g_error ("Error while parsing the stylesheet, make sure you have your docbook environment setup correctly.");
                exit (1);
        }

	if (strstr (docpath, ".sgml")) {
                db_doc = docbParseFile (docpath, "UTF-8");
	} else {
		db_doc = xmlParseFile (docpath);
#ifdef LIBXML_XINCLUDE_ENABLED
		xmlXIncludeProcess (db_doc);
#endif
	}

	if (db_doc == NULL) {
                g_error ("Couldn't parse the document '%s'.", 
                         docpath);
                exit (1);
	}
	
	/* retrieve path component of filename passed in at
	   command line */
	pathname = g_path_get_dirname (docpath);
        
	/* set params to be passed to stylesheet */
	params[0] = "gdb_docname";
	params[1] = g_strconcat("\"", docpath, "\"", NULL) ;
	params[2] = "gdb_pathname";
	params[3] = g_strconcat("\"", pathname, "\"", NULL) ;
	params[4] = "gdb_stylesheet_path";
        params[5] = g_strconcat("\"", DB_STYLESHEET_PATH, "\"", NULL) ;
        params[6] = "gdb_multichunk";
        params[7] = "1";
        params[8] = NULL;
        
        g_free (pathname);

        final_doc = xsltApplyStylesheet (stylesheet, db_doc, params);

        xmlFree (db_doc);

        if (!final_doc) {
                g_error ("Error while applying the stylesheet.");
                exit (1);
        }

        xsltSaveResultToFile(stdout, final_doc, stylesheet);
        xsltFreeStylesheet(stylesheet);

        xmlFree (final_doc);
        xsltCleanupGlobals();
        xmlCleanupParser();

	return 0;
}
