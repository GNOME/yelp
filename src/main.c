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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomevfs/gnome-vfs.h>
#include "yelp-base.h"

int
main (int argc, char **argv)
{
	GnomeProgram *program;
	YelpBase     *base;
	GtkWidget    *window;
	
  	g_thread_init (NULL);

	program = gnome_program_init (PACKAGE, VERSION,
				      LIBGNOMEUI_MODULE,
				      argc, argv, NULL);

/* 	gtk_init (&argc, &argv); */

	gnome_vfs_init ();
	
        base = yelp_base_new ();
	
	window = yelp_base_new_window (base);

	if (argc >= 2) {
		yelp_window_open_uri (window, argv[1]);
	}
	
	gtk_widget_show_all (window);

	gtk_main ();

        return 0;
}
