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

#include "config.h"

#include <gtkmozembed.h>
#include <gtkmozembed_internal.h>
#include <nsIWebBrowser.h>
#include <nsIWebBrowserFind.h>
#include <nsIClipboardCommands.h>
#include <nsICommandManager.h>
#include <nsCOMPtr.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsReadableUtils.h>
#include <nsString.h>
#include <nsIPrefService.h>
#include <nsIServiceManager.h>
#include <nsIDOMMouseEvent.h>
#include <nsIDOMNSEvent.h>
#include <nsIDOMEventTarget.h>
#include <nsIDOMNode.h>
#include <nsIDOMHTMLAnchorElement.h>
#include <stdlib.h>

#include "yelp-gecko-utils.h"

static gboolean
yelp_util_split_font_string (const gchar *font_name, gchar **name, gint *size)
{
	gchar *tmp_name, *ch;
	
	tmp_name = g_strdup (font_name);

	ch = g_utf8_strrchr (tmp_name, -1, ' ');
	if (!ch || ch == tmp_name) {
		g_free (tmp_name);
		return FALSE;
	}

	*ch = '\0';

	*name = g_strdup (tmp_name);
	*size = strtol (ch + 1, (char **) NULL, 10);
	
	return TRUE;
}

static gboolean
gecko_prefs_set_bool (const gchar *key, gboolean value)
{
	nsCOMPtr<nsIPrefService> prefService =
		do_GetService (NS_PREFSERVICE_CONTRACTID);
	nsCOMPtr<nsIPrefBranch> pref;
	prefService->GetBranch ("", getter_AddRefs (pref));

	if (pref) {
		nsresult rv = pref->SetBoolPref (key, value);
		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}
	
	return FALSE;
}

static gboolean
gecko_prefs_set_string (const gchar *key, const gchar *value)
{
	nsCOMPtr<nsIPrefService> prefService =
		do_GetService (NS_PREFSERVICE_CONTRACTID);
	nsCOMPtr<nsIPrefBranch> pref;
	prefService->GetBranch ("", getter_AddRefs (pref));

	if (pref) {
		nsresult rv = pref->SetCharPref (key, value);
		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}
	
	return FALSE;
}

static gboolean
gecko_prefs_set_int (const gchar *key, gint value)
{
	nsCOMPtr<nsIPrefService> prefService =
		do_GetService (NS_PREFSERVICE_CONTRACTID);
	nsCOMPtr<nsIPrefBranch> pref;
	prefService->GetBranch ("", getter_AddRefs (pref));

	if (pref) {
		nsresult rv = pref->SetIntPref (key, value);
		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}
	
	return FALSE;
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
	case YELP_COLOR_TEXT:
		gecko_prefs_set_string ("browser.display.foreground_color",
					color);
		break;
	case YELP_COLOR_ANCHOR:
		gecko_prefs_set_string ("browser.anchor_color",
					color);
		break;
	case YELP_COLOR_BACKGROUND:
		gecko_prefs_set_string ("browser.display.background_color",
					color);
		break;
	}
}

extern "C" void
yelp_gecko_set_font (YelpFontType font_type, const gchar *fontname)
{
	gchar *name;
	gint   size;

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
	}

	g_free (name);
}		   

extern "C" gboolean
yelp_gecko_find (GtkMozEmbed  *embed,
		 const gchar  *str,
		 gboolean      match_case,
		 gboolean      wrap,
		 gboolean      forward)
{
    PRBool didFind;
    nsCString matchString;

    matchString.Assign (str);

    nsCOMPtr<nsIWebBrowser> webBrowser;
    gtk_moz_embed_get_nsIWebBrowser (embed, getter_AddRefs(webBrowser));

    nsCOMPtr<nsIWebBrowserFind> finder (do_GetInterface(webBrowser));
    NS_ENSURE_TRUE (finder, NS_ERROR_FAILURE);

    finder->SetFindBackwards (!forward);
    finder->SetSearchString (ToNewUnicode (matchString));
    finder->SetMatchCase (match_case);
    finder->SetWrapFind (wrap);

    finder->FindNext (&didFind);

    return didFind;
}

extern "C" gboolean
yelp_gecko_copy_selection (GtkMozEmbed *embed)
{
	nsCOMPtr<nsIWebBrowser> webBrowser;
	gtk_moz_embed_get_nsIWebBrowser (embed, getter_AddRefs(webBrowser));

	nsCOMPtr<nsIClipboardCommands> clip (do_GetInterface(webBrowser));
	NS_ENSURE_TRUE (clip, NS_ERROR_FAILURE);
	
	clip->CopySelection();
}

extern "C" gboolean
yelp_gecko_select_all (GtkMozEmbed *embed)
{
	nsCOMPtr<nsIWebBrowser> webBrowser;
	gtk_moz_embed_get_nsIWebBrowser (embed, getter_AddRefs(webBrowser));

	nsCOMPtr<nsICommandManager> cmdManager;
	cmdManager = do_GetInterface (webBrowser);
	NS_ENSURE_TRUE (cmdManager, NS_ERROR_FAILURE);

	cmdManager->DoCommand ("cmd_selectAll", nsnull, nsnull);
}

extern "C" gchar*
yelp_gecko_mouse_event (GtkMozEmbed  *html, gpointer dom_event)
{
	PRUint16 buttonCode;

	g_return_val_if_fail (dom_event != NULL, FALSE);

	nsCOMPtr<nsIDOMMouseEvent> event (do_QueryInterface 
					  ((nsIDOMEvent*) dom_event)); 

	if (!event) {
		return NULL;
	}

	event->GetButton (&buttonCode);

	if(buttonCode == 2){ 
		/*Mozilla uses 2 as its right mouse button code*/
		nsresult result;
		nsAutoString nodename;
		gchar *uri;

		nsCOMPtr<nsIDOMNSEvent> nsEvent = 
			do_QueryInterface(event, &result);

		if (NS_FAILED(result) || !nsEvent) 
			return NULL;
		
		nsCOMPtr<nsIDOMEventTarget> OriginalTarget;
		result = nsEvent->GetOriginalTarget(getter_AddRefs(OriginalTarget));
		if (NS_FAILED(result) || !OriginalTarget) 
			return NULL;
		
		nsCOMPtr<nsIDOMNode> OriginalNode = 
			do_QueryInterface(OriginalTarget);

		if (!OriginalNode) return NULL;
		
		OriginalNode->GetNodeName(nodename);
		uri = g_new(char, 150);

		if (nodename.EqualsIgnoreCase("a")){
			
			nsCOMPtr<nsIDOMNode> node = 
				do_QueryInterface(OriginalTarget, &result);
			if (NS_FAILED(result) || !node) return NULL;
			
			nsCOMPtr <nsIDOMHTMLAnchorElement> anchor =
				do_QueryInterface(node);
			anchor->GetHref (nodename);
			uri = nodename.ToCString( uri, 150);
			return uri;
		}
	}
	return NULL;
}
