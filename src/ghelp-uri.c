/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 CodeFactory AB
 * Copyright (C) 2001 Mikael Hallendal <micke@codefactory.se>
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
 * Author: Mikael Hallendal <micke@codefactory.se>
 */

#include <libgnomevfs/gnome-vfs-mime.h>
#include <string.h>
#include "ghelp-uri.h"

/*
 * 1:  ghelp:nautilus
 * 2:  ghelp:AisleRiot2/Klondike
 * 3:  ghelp:///opt/gnome/share/AisleRiot2/help/it/Klondike.xml?winning
 * 4:  ghelp:/usr/share/oldapp/help/index.html
 */

#define G "/usr/share/"
#define L "C"

typedef enum {
	GHELP_FILE_TYPE_XML,
	GHELP_FILE_TYPE_SGML,
	GHELP_FILE_TYPE_HTML,
	GHELP_FILE_TYPE_UNKNOWN
} GHelpFileType;

typedef struct {
	gchar         *file;
	gchar         *section;
	GHelpFileType  type;
} GHelpURI;

static GHelpURI *      ghelp_uri_new                   (void);
static gchar *         ghelp_uri_split_string          (GHelpURI       *uri,
                                                        const gchar    *str);
static GnomeVFSResult  ghelp_uri_handle_relative_path  (GHelpURI       *uri,
                                                        const gchar    *str);
static GnomeVFSResult  ghelp_uri_handle_absolute_path  (GHelpURI       *uri,
                                                        const gchar    *str);
static GnomeVFSResult  ghelp_uri_setup_from_file       (GHelpURI       *uri,
                                                        const gchar    *file);
static gchar *         ghelp_uri_to_string             (GHelpURI       *uri);

static gchar *         ghelp_uri_check_uri_locale_type (const gchar    *base,
                                                        const gchar    *extra,
                                                        const gchar    *locale,
                                                        GHelpFileType   type);
static char *          ghelp_uri_shell_quote           (const char     *str);

static GHelpURI *
ghelp_uri_new (void)
{
	GHelpURI *uri;
	
	uri = g_new0 (GHelpURI, 1);
	uri->type = GHELP_FILE_TYPE_UNKNOWN;

	return uri;
}


/* Sets uri->section if it is present, and returns the base */
static gchar *
ghelp_uri_split_string (GHelpURI *uri, const gchar *str)
{
        gchar *ch;
        gchar *base;
        
        ch = strrchr (str, '?');
        
        if (!ch) {
                ch = strrchr (str, '#');
        }

        if (!ch) {
                base = g_strdup (str);
        } else {
                uri->section = g_strdup (ch + 1);
                base = g_strndup (str, ch - str);
        }

        /* Remove trailing spaces, if any */
        g_strchomp (base);

        return base;
}

static GnomeVFSResult
ghelp_uri_handle_relative_path (GHelpURI *uri, const gchar *str)
{
        gchar          *base;
        gchar          *new_uri;
        GnomeVFSResult  result;
        gchar          *extra;
        gchar          *period;
        
	g_print ("Relative: '%s'\n", str);

        base = ghelp_uri_split_string (uri, str);

        extra = strchr (base, '/');
        
        if (extra) {
                *extra = '\0';
                extra++;
                
                period = strchr (extra, '.');
                
                if (period) {
                        *period = '\0';
                }
        }
        
        /* Calculate the correct path */

        /* For all locales and all types: */

        new_uri = ghelp_uri_check_uri_locale_type (base, extra, L,
                                                   GHELP_FILE_TYPE_XML);
        if (!new_uri) {
                new_uri = ghelp_uri_check_uri_locale_type (base, extra, L,
                                                           GHELP_FILE_TYPE_SGML);
                if (!new_uri) {
                        new_uri = ghelp_uri_check_uri_locale_type (base, extra,
                                                                   L, GHELP_FILE_TYPE_HTML);
                        if (!new_uri) {
                                return GNOME_VFS_ERROR_NOT_FOUND;
                        }
                }
        }

        g_free (base);
        
        result = ghelp_uri_setup_from_file (uri, new_uri);

        return result;
}

static GnomeVFSResult
ghelp_uri_handle_absolute_path (GHelpURI *uri, const gchar *str)
{
        gchar          *base;
        GnomeVFSResult  result;
	
        base   = ghelp_uri_split_string (uri, str);
	result = ghelp_uri_setup_from_file (uri, base);

	g_free (base);
	
	return result;
}

static GnomeVFSResult
ghelp_uri_setup_from_file (GHelpURI *uri, const gchar *file)
{
	const gchar *mime_type;
	
	g_return_val_if_fail (uri != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (file != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);
	
	if (g_file_test (file, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK)) {
		uri->file = g_strdup (file);
	} else {
		return GNOME_VFS_ERROR_NOT_FOUND;
	}

	mime_type = gnome_vfs_get_file_mime_type (uri->file, NULL, FALSE);
	
	if (!g_strcasecmp (mime_type, "text/xml")) {
		uri->type = GHELP_FILE_TYPE_XML;
	}
	else if (!g_strcasecmp (mime_type, "text/sgml")) {
		uri->type = GHELP_FILE_TYPE_SGML;
	}
	else if (!g_strcasecmp (mime_type, "text/html")) {
		uri->type = GHELP_FILE_TYPE_HTML;
	} else {
		uri->type = GHELP_FILE_TYPE_UNKNOWN;
	}

	return GNOME_VFS_OK;
}

static gchar *
ghelp_uri_to_string (GHelpURI *ghelp_uri)
{
	const gchar *command;
	gchar       *parameter, *command_line, *escaped, *uri;

	g_return_val_if_fail (ghelp_uri != NULL, NULL);
	
	switch (ghelp_uri->type) {
	case GHELP_FILE_TYPE_XML:
		/* Fall through */
	case GHELP_FILE_TYPE_SGML:
		/* Pipe through gnome-db2html3 */
		command = "gnome-db2html2";
		
		if (ghelp_uri->section) {
			parameter = g_strconcat (ghelp_uri->file, "?",
						 ghelp_uri->section, NULL);
		} else {
			parameter = g_strdup (ghelp_uri->file);
		}
		break;
	case GHELP_FILE_TYPE_HTML:
		/* Concat file:// with ghelp_uri->file */
		escaped = gnome_vfs_escape_path_string (ghelp_uri->file);
		
		if (ghelp_uri->section) {
			uri = g_strconcat ("file://", escaped, "#",
					   ghelp_uri->section, NULL);
		} else {
			uri = g_strconcat ("file://", escaped, NULL);
		}

		g_free (escaped);
		
		return (uri);
		break;
	case GHELP_FILE_TYPE_UNKNOWN:
		return NULL;
	default:
		g_warning ("Unreachable switch reached");
		return NULL;
	};

	escaped = ghelp_uri_shell_quote (parameter);
	g_free (parameter);
	
	command_line = g_strconcat (command, " ", escaped, 
				    ";mime-type=text/html", NULL);
	g_free (escaped);
	
	escaped = gnome_vfs_escape_string (command_line);
	g_free (command_line);
	
	uri = g_strconcat ("pipe:", escaped, NULL);
	g_free (escaped);

	return uri;
}

static gchar *
ghelp_uri_check_uri_locale_type (const gchar   *base,
                                 const gchar   *extra,
                                 const gchar   *locale,
                                 GHelpFileType  type)
{
        gchar       *new_uri;
        const gchar *file;
        gchar       *file_with_extension;

        if (extra) {
                file = extra;
        } else {
                if (type == GHELP_FILE_TYPE_HTML) {
                        file = "index";
                } else {
                        file = base;
                }
        }
        
        switch (type) {
        case GHELP_FILE_TYPE_XML:
                file_with_extension = g_strconcat (file, ".xml", NULL);
                break;
        case GHELP_FILE_TYPE_SGML:
                file_with_extension = g_strconcat (file, ".sgml", NULL);
                break;
        case GHELP_FILE_TYPE_HTML:
                file_with_extension = g_strconcat (file, ".html", NULL);
                break;
        default:
                break;
        };

        new_uri = g_strdup_printf ("%s/gnome/help/%s/%s/%s", G, 
                                   base, locale, file_with_extension);
        
        if (g_file_test (new_uri, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK)) {
                return new_uri;
        }

        g_free (new_uri);
        
        return NULL;
}

/* This is a copy of eel_shell_quote. The best thing to do is to
 * use the one on GNOME for GNOME 2.0, but we could also move this
 * into gnome-vfs or some other library so we could share it.
 */
static gchar *
ghelp_uri_shell_quote (const gchar *str)
{
	const gchar *p;
	GString *quoted_string;
	gchar *quoted_str;

	/* All kinds of ways to do this fancier.
	 * - Detect when quotes aren't needed at all.
	 * - Use double quotes when they would look nicer.
	 * - Avoid sequences of quote/unquote in a row (like when you quote "'''").
	 * - Do it higher speed with strchr.
	 * - Allocate the GString with g_string_sized_new.
	 */

	g_return_val_if_fail (str != NULL, NULL);

	quoted_string = g_string_new ("'");

	for (p = str; *p != '\0'; p++) {
		if (*p == '\'') {
			/* Get out of quotes, do a quote, then back in. */
			g_string_append (quoted_string, "'\\''");
		} else {
			g_string_append_c (quoted_string, *p);
		}
	}

	g_string_append_c (quoted_string, '\'');

	/* Let go of the GString. */
	quoted_str = quoted_string->str;
	g_string_free (quoted_string, FALSE);

	return quoted_str;
}

GnomeVFSResult 
ghelp_uri_transform (const gchar *uri_str, gchar **new_uri)
{
        GHelpURI       *uri;
	gchar          *str;
	GnomeVFSResult  result;
        
        uri = ghelp_uri_new ();

	str = strstr (uri_str, "ghelp:");

	if (!str) {
		return GNOME_VFS_ERROR_INVALID_URI;
	}

	str += strlen ("ghelp:");
        
	if (*str == '/') {
		/* Absolute */
                result = ghelp_uri_handle_absolute_path (uri, str);
                
	} else {
		/* Scheme 1 or 2 */
		result = ghelp_uri_handle_relative_path (uri, str);
	}

	if (result == GNOME_VFS_OK) {
		*new_uri = ghelp_uri_to_string (uri);
	}

        return result;
}



