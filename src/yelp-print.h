/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2005 Don Scorgie <Don@Scorgie.org>
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
 * Author: Don Scorgie <Don@Scorgie.org>
 */

#ifndef YELP_PRINT_H
#define YELP_PRINT_H

#include <glib.h>

/* Needed to fill the struct */
#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>

#include "yelp-window.h"

G_BEGIN_DECLS


typedef struct _YelpPrintInfo
{
    GtkPrintSettings *config;
    GtkPageSetup     *setup;
    GtkPrinter       *printer;
    GtkWidget        *print_dialog;

    char *tempfile;
    guint print_idle_id;
    guint cancel_print_id;

    gboolean cancelled;
    gboolean moz_finished;
    gboolean started;
    gboolean previewed;
    
    char *header_left_string;
    char *header_center_string;
    char *header_right_string;
    char *footer_left_string;
    char *footer_center_string;
    char *footer_right_string;
    YelpWindow *owner;
    GtkWidget *dialog;
    gpointer html_frame;
    GtkWidget *progress;
    GtkWidget *fake_win;
    GtkWidget *content_box;

    /* Preview buttons */
    GtkWidget *GoBack;
    GtkWidget *GoForward;
    GtkWidget *GoFirst;
    GtkWidget *GoLast;
    GtkWidget *Close;
    gint npages;
    gint currentpage;


} YelpPrintInfo;

void          yelp_print_run               (YelpWindow *window, 
					    gpointer html,
					    gpointer fake_win,
					    gpointer content_box);
void          yelp_print_moz_finished      (YelpPrintInfo *info);
void          yelp_print_cancel            (YelpPrintInfo *info);
void          yelp_print_info_free         (YelpPrintInfo *info);
void          yelp_print_update_progress   (YelpPrintInfo *info,
					    gdouble percentage);
G_END_DECLS

#endif
