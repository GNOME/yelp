/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#ifndef __YELP_DEBUG_H__
#define __YELP_DEBUG_H__

G_BEGIN_DECLS

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

typedef enum {
	DB_FUNCTION   = 1 << 0,
	DB_ARG        = 1 << 1,
	DB_PROFILE    = 1 << 2,
	DB_LOG        = 1 << 3,
	DB_INFO       = 1 << 4,
	DB_DEBUG      = 1 << 5,
	DB_WARN       = 1 << 6,
	DB_ERROR      = 1 << 7,
	DB_ALL        = 1 << 8
} YelpDebugEnums;

#if __STDC_VERSION__ < 199901L
# if __GNUC__ >= 2
#  define __func__ __FUNCTION__
# else
#  define __func__ "<unknown>"
# endif
#endif

/* __VA_ARGS__ is C99 compliant but may not work on older versions of cpp, 
 * so we provide a fallback */
#ifdef YELP_DEBUG
# if __STDC_VERSION__ < 199901L
#  define debug_print(format, args...) yelp_debug (__FILE__, __LINE__, __func__, format, args)
# else
#  define debug_print(format, ...) yelp_debug (__FILE__, __LINE__, __func__, format, __VA_ARGS__)
# endif
#else
# if __STDC_VERSION__ < 199901L
#  define debug_print(format, args...)
# else
#  define debug_print(format, ...)
# endif
#endif

#ifdef YELP_DEBUG
# define d(x) x
#else
# define d(x)
#endif

void yelp_debug (const gchar *file, guint line, 
                 const gchar *function, guint flags, const gchar *format, ...);

G_END_DECLS

#endif /* __YELP_DEBUG_H__ */
