/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#include <libgnome/gnome-program.h>
#include <libgnomevfs/gnome-vfs.h>
#include "ghelp-uri.h"

int
main (int argc, char **argv)
{
	GnomeVFSResult  result;
        gchar          *uri;
	GnomeVFSURI    *vfs_uri;
	
        if (argc <= 1) {
                g_print (_("Usage '%s uri'\n"), argv[0]);
                exit (1);
        }

	result = ghelp_uri_transform (argv[1], &uri);

        switch (result) {
	case GNOME_VFS_OK:
                g_print (_("Transformed URI: %s\n"), uri);
		vfs_uri = gnome_vfs_uri_new (uri);
		if (!vfs_uri) {
			g_print ("Eeeeeek\n");
		}
		
                exit (0);
		break;
	case GNOME_VFS_ERROR_NOT_FOUND:
		g_print (_("File not found: %s\n"), argv[1]);
		exit (1);
		break;
	case GNOME_VFS_ERROR_INVALID_URI:
		g_print (_("Invalid URI: %s\n"), argv[1]);
		exit (1);
		break;
	default:
		break;
	};
	
	return 0;
}
