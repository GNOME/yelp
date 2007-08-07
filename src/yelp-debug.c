/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2006 Brent Smith <gnome@nextreality.net>
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
 */

#include <config.h>
#include <glib/gi18n.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <unistd.h>
#include <time.h>

#include "yelp-debug.h"

/**
 * @file:     you should pass the __FILE__ constant as this parameter
 * @line:     you should pass the __LINE__ constant as this parameter
 * @function: you should pass the __FUNCTION__ constant as this parameter
 * @flags:    should be one of #YelpDebugEnums. Arguments can be bitwise OR'd together.
 * @format:   the printf style format
 *
 * This function is not meant to be called directly, instead you should use the
 * debug_print() macro which will pass the __FILE__, __LINE__, and __FUNCTION__ 
 * constants for you.
 * The flags passed to the function identify the type of debug statement.  Some action
 * (usually a print to the console) will take place depending on if the corresponding
 * flag is set in the YELP_DEBUG environment variable.  YELP_DEBUG is a colon separated 
 * list of zero or more the following values: 
 * "log"              - debug_print calls with DB_LOG, DB_INFO, DB_DEBUG, DB_WARN, and
 *                      DB_ERROR will be printed to stdout
 * "info"             - debug_print calls with DB_INFO, DB_DEBUG, DB_WARN and DB_ERROR 
 *                      will be printed to stdout
 * "debug"            - debug_print calls with DB_DEBUG, DB_WARN, and DB_ERROR will be 
 *                      printed to stdout
 * "warn"             - debug_print calls with DB_WARN and DB_ERROR will be printed to 
 *                      stdout
 * "error"            - debug_print calls with DB_ERROR will be printed to stdout
 * "function-calls"   - debug_print (DB_FUNCTION, ...) calls will be printed to stdout
 *                      along with the function name
 * "function-args"    - debug_print (DB_ARG, ...) calls will be printed to stdout
 * "enable-profiling" - debug_print (DB_PROFILE, ...) calls will make an access() call
 *                      with a "MARK:" at the beginning: this is for profiling with 
 *                      Federico's plot-timeline.py python script: 
 *                      see http://primates.ximian.com/~federico/docs/login-profile/plot-timeline.py
 * "all"              - Turns on all options.
 * 
 */
void yelp_debug (const gchar *file,     guint line, 
                 const gchar *function, guint flags, const gchar *format, ...)
{
	static gboolean  first_call = TRUE;
	static guint     debug_flags = 0;
	static gchar   **debug_files = NULL;
	va_list          args;
	const gchar     *debugenv = NULL;
	gchar           *formatted = NULL;
	gchar           *str = NULL;
	gint             i;

	const GDebugKey debug_keys[] = {
	    { "function-calls",   DB_FUNCTION },
	    { "function-args",    DB_ARG }, 
	    { "enable-profiling", DB_PROFILE },
	    { "log",              DB_LOG },
	    { "info",             DB_INFO },
	    { "debug",            DB_DEBUG },
	    { "warn",             DB_WARN },
	    { "error",            DB_ERROR },
	    { "all",              DB_ALL }
	};

	/* figure out which debug flags were set in the environment on the first
	 * call to this function */
	if (first_call) {
		debugenv = g_getenv ("YELP_DEBUG");

		if (debugenv != NULL)
			debug_flags = g_parse_debug_string (debugenv, debug_keys, 
		                                            G_N_ELEMENTS (debug_keys));
		else
			debug_flags = 0;

		/* turn on all flags if "all" option was specified */
		if (debug_flags & DB_ALL)
			debug_flags |= (DB_LOG | DB_INFO | DB_DEBUG | DB_WARN | DB_ERROR | 
			                DB_FUNCTION | DB_ARG | DB_PROFILE);

		/* turn on all higher severity logging levels; for instance if DB_LOG is set
		 * in debug_flags, then we turn on INFO, DEBUG, WARN and ERROR */
		else if (debug_flags & DB_LOG)
			debug_flags |= (DB_INFO | DB_DEBUG | DB_WARN | DB_ERROR);
		else if (debug_flags & DB_INFO)
			debug_flags |= (DB_DEBUG | DB_WARN | DB_ERROR);
		else if (debug_flags & DB_DEBUG)
			debug_flags |= (DB_WARN | DB_ERROR);
		else if (debug_flags & DB_WARN)
			debug_flags |= (DB_ERROR);

		debugenv = g_getenv ("YELP_DEBUG_FILTER");

		if (debugenv != NULL && *debugenv != '\0')
			debug_files = g_strsplit (debugenv, ":", -1);
		else
			debug_files = NULL;
	
		first_call = FALSE;
	}

	/* if none of the flags are set either by the calling function
	 * or in the YELP_DEBUG environment variable, then just return */
	if ((flags & debug_flags) == 0)
		return;

	/* if the YELP_DEBUG_FILTER environment variable was specified with
	 * a colon delimited list of source files, then debug_files will not be
	 * NULL and will contain a list of files for which we print out debug
	 * statements.  Check if this function was called from one of the files
	 * in the list.  If not, return. */
	if (debug_files != NULL) {
		for (i=0; debug_files[i] != NULL; i++) {
			if (*(debug_files[i]) == '\0')
				continue;
			if (g_strrstr (file, debug_files[i]) != NULL)
				break;
		}

		/* if we are on the NULL element, then a match was not found so
		 * we should return now */
		if (debug_files[i] == NULL)
			return;
	}
	
	va_start (args, format);

	if (flags & DB_FUNCTION) {
		g_fprintf  (stdout, "%s:%u: %s: ", file, line, function);
		g_vfprintf (stdout, format, args);
	} else if ((flags & DB_LOG)   ||
	           (flags & DB_INFO)  ||
	           (flags & DB_DEBUG) ||
	           (flags & DB_WARN)  ||
	           (flags & DB_ERROR) ||
	           (flags & DB_ARG)) {
		g_fprintf  (stdout, "%s:%u: ", file, line);
		g_vfprintf (stdout, format, args);
	}

	if (flags & DB_PROFILE) {
		time_t t;
		struct tm *tmp;
		gchar timestamp[20];

		t = time (NULL);
		tmp = localtime(&t);

		strftime (timestamp, 20, "%H:%M:%S", tmp);
		formatted = g_strdup_vprintf (format, args);

		g_fprintf (stdout, "PROFILE [%s]: %s\n", timestamp, formatted);
		str = g_strdup_printf ("MARK: %s: %s", g_get_prgname(), formatted);
		access (str, F_OK);
		g_free (formatted);
		g_free (str);
	}

	va_end (args);
}
