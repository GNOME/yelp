/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Marco Pesenti Gritti
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
 * Author: Marco Pesenti Gritti <marco@gnome.org>
 */

#include "mozilla-config.h"

#include "config.h"

#include <nsCOMPtr.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIServiceManager.h>
#include <nsIPrefService.h>
#include <stdlib.h>

#include "yelp-gecko-utils.h"

static gboolean
yelp_util_split_font_string (const gchar *font_name, gchar **name, gint *size)
{
	PangoFontDescription *desc;
	PangoFontMask mask = (PangoFontMask) (PANGO_FONT_MASK_FAMILY | PANGO_FONT_MASK_SIZE);
	gboolean retval = FALSE;

	desc = pango_font_description_from_string (font_name);
	if (!desc) return FALSE;

	if ((pango_font_description_get_set_fields (desc) & mask) == mask) {
		*size = PANGO_PIXELS (pango_font_description_get_size (desc));
		*name = g_strdup (pango_font_description_get_family (desc));
		retval = TRUE;
	}

	pango_font_description_free (desc);

	return retval;
}

static gboolean
gecko_prefs_set_bool (const gchar *key, gboolean value)
{
	nsresult rv;
	nsCOMPtr<nsIPrefService> prefService (do_GetService (NS_PREFSERVICE_CONTRACTID, &rv));
	NS_ENSURE_SUCCESS (rv, FALSE);

	nsCOMPtr<nsIPrefBranch> pref;
	rv = prefService->GetBranch ("", getter_AddRefs (pref));
	NS_ENSURE_SUCCESS (rv, FALSE);

	rv = pref->SetBoolPref (key, value);

	return NS_SUCCEEDED (rv) != PR_FALSE;
}

static gboolean
gecko_prefs_set_string (const gchar *key, const gchar *value)
{
	nsresult rv;
	nsCOMPtr<nsIPrefService> prefService (do_GetService (NS_PREFSERVICE_CONTRACTID, &rv));
	NS_ENSURE_SUCCESS (rv, FALSE);

	nsCOMPtr<nsIPrefBranch> pref;
	rv = prefService->GetBranch ("", getter_AddRefs (pref));
	NS_ENSURE_SUCCESS (rv, FALSE);

	rv = pref->SetCharPref (key, value);

	return NS_SUCCEEDED (rv) != PR_FALSE;
}

static gboolean
gecko_prefs_set_int (const gchar *key, gint value)
{
	nsresult rv;
	nsCOMPtr<nsIPrefService> prefService (do_GetService (NS_PREFSERVICE_CONTRACTID, &rv));
	NS_ENSURE_SUCCESS (rv, FALSE);

	nsCOMPtr<nsIPrefBranch> pref;
	rv = prefService->GetBranch ("", getter_AddRefs (pref));
	NS_ENSURE_SUCCESS (rv, FALSE);

	rv = pref->SetIntPref (key, value);

	return NS_SUCCEEDED (rv) != PR_FALSE;
}

extern "C" void
yelp_gecko_set_caret (gboolean value)
{
	gecko_prefs_set_bool ("accessibility.browsewithcaret", value);
}

extern "C" void
yelp_gecko_set_color (YelpColorType type, const gchar *color)
{
	gecko_prefs_set_bool ("browser.display.use_system_colors", FALSE);
	switch (type) {
	case YELP_COLOR_FG:
		gecko_prefs_set_string ("browser.display.foreground_color",
					color);
		break;
	case YELP_COLOR_BG:
		gecko_prefs_set_string ("browser.display.background_color",
					color);
		break;
	case YELP_COLOR_ANCHOR:
		gecko_prefs_set_string ("browser.anchor_color",
					color);
		break;
	default:
		break;
	}
}

extern "C" void
yelp_gecko_set_font (YelpFontType font_type, const gchar *fontname)
{
	gchar *name;
	gint   size = 0;

	g_return_if_fail (fontname != NULL);

	name = NULL;
	if (!yelp_util_split_font_string (fontname, &name, &size)) {
		g_free (name);
		return;
	}

	gecko_prefs_set_string ("font.size.unit", "pt");

	switch (font_type) {
	case YELP_FONT_VARIABLE:
		gecko_prefs_set_string ("font.name.variable.x-western", 
					name);
		gecko_prefs_set_int ("font.size.variable.x-western", 
				     size);
		gecko_prefs_set_int ("font.minimum-size.x-western", 
				     8);
		break;
	case YELP_FONT_FIXED:
		gecko_prefs_set_string ("font.name.monospace.x-western", 
					name);
		gecko_prefs_set_int ("font.size.monospace.x-western", 
				     size);
		gecko_prefs_set_string ("font.name.fixed.x-western", 
					name);
		gecko_prefs_set_int ("font.size.fixed.x-western", 
				     size);
		break;
	default:
		break;
	}

	g_free (name);
}		   
