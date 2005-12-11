/*
 *  Copyright (C) 2000-2004 Marco Pesenti Gritti
 *  Copyright (C) 2003-2005 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include <mozilla-config.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "Yelper.h"

#undef MOZILLA_INTERNAL_API
#include "nsEmbedString.h"
#define MOZILLA_INTERNAL_API 1

#include <nsIWebBrowserSetup.h>
#include <nsIClipboardCommands.h>
#include <nsICommandManager.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIPrefService.h>
#include <nsIServiceManager.h>
#include <nsIDOMDocument.h>
#include <nsIDOMMouseEvent.h>
#include <nsIDOMNSEvent.h>
#include <nsIDOMEventTarget.h>
#include <nsIDOMNode.h>
#include <nsIDOMHTMLAnchorElement.h>
#include <nsIWebBrowserPrint.h>

#ifdef TYPEAHEADFIND
#include <nsIDocShell.h>
#define NS_TYPEAHEADFIND_CONTRACTID "@mozilla.org/typeaheadfind;1"
#endif /* TYPEAHEADFIND */

#include <stdlib.h>
#include <unistd.h>

#include <gtkmozembed_internal.h>

#include <yelp-gecko-services.h>

#ifdef GNOME_ENABLE_DEBUG
#define d(x) x
#else
#define d(x)
#endif

Yelper::Yelper (GtkMozEmbed *aEmbed)
: mInitialised(PR_FALSE)
, mEmbed(aEmbed)
{
	d (g_print ("Yelper ctor [%p]\n", this));
}

Yelper::~Yelper ()
{
	d (g_print ("Yelper dtor [%p]\n", this));
}

nsresult
Yelper::Init ()
{
	if (mInitialised) return NS_OK;

	nsresult rv = NS_ERROR_FAILURE;
	gtk_moz_embed_get_nsIWebBrowser (mEmbed,
					 getter_AddRefs (mWebBrowser));
	NS_ENSURE_TRUE (mWebBrowser, rv);

	nsCOMPtr<nsIWebBrowserSetup> setup (do_QueryInterface (mWebBrowser, &rv));
	NS_ENSURE_SUCCESS (rv, rv);

	setup->SetProperty (nsIWebBrowserSetup::SETUP_USE_GLOBAL_HISTORY, PR_FALSE);

	rv = mWebBrowser->GetContentDOMWindow (getter_AddRefs (mDOMWindow));
	NS_ENSURE_SUCCESS (rv, rv);

	/* This will instantiate an about:blank doc if necessary */
	nsCOMPtr<nsIDOMDocument> domDocument;
	rv = mDOMWindow->GetDocument (getter_AddRefs (domDocument));
	NS_ENSURE_SUCCESS (rv, rv);

#ifdef TYPEAHEADFIND
	mFinder = do_CreateInstance (NS_TYPEAHEADFIND_CONTRACTID, &rv);
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIDocShell> docShell (do_GetInterface (mWebBrowser, &rv));
	NS_ENSURE_SUCCESS (rv, rv);

	rv = mFinder->Init (docShell);
	NS_ENSURE_SUCCESS (rv, rv);

	mFinder->SetFocusLinks (PR_TRUE);
#else
	mFinder = do_GetInterface (mWebBrowser, &rv);
	NS_ENSURE_SUCCESS (rv, rv);
#endif /* TYPEAHEADFIND */

	mInitialised = PR_TRUE;

	return NS_OK;
}

void
Yelper::Destroy ()
{
	mEmbed = nsnull;
	mWebBrowser = nsnull;
	mDOMWindow = nsnull;
	mFinder = nsnull;

	mInitialised = PR_FALSE;
}

void
Yelper::DoCommand (const char *aCommand)
{
	if (!mInitialised) return;

	nsCOMPtr<nsICommandManager> cmdManager (do_GetInterface (mWebBrowser));
	if (!cmdManager) return;

	cmdManager->DoCommand (aCommand, nsnull, nsnull);
}

void
Yelper::SetFindProperties (const char *aSearchString,
			   PRBool aCaseSensitive,
			   PRBool aWrap)
{
	if (!mInitialised) return;

#ifdef TYPEAHEADFIND
	mFinder->SetCaseSensitive (aCaseSensitive);
	/* aWrap not supported for typeaheadfind
	 * search string is set in ::Find for typeaheadfind
	 */
#else
	nsEmbedString uSearchString;
	NS_CStringToUTF16 (nsEmbedCString (aSearchString ? aSearchString : ""),
			NS_CSTRING_ENCODING_UTF8, uSearchString);

	mFinder->SetSearchString (uSearchString.get ());
	mFinder->SetMatchCase (aCaseSensitive);
	mFinder->SetWrapFind (aWrap);
#endif /* TYPEAHEADFIND */
}

PRBool
Yelper::Find (const char *aSearchString)
{
	if (!mInitialised) return PR_FALSE;

#ifdef TYPEAHEADFIND
	nsEmbedString uSearchString;
	NS_CStringToUTF16 (nsEmbedCString (aSearchString ? aSearchString : ""),
			   NS_CSTRING_ENCODING_UTF8, uSearchString);

	nsresult rv;
	PRUint16 found = nsITypeAheadFind::FIND_NOTFOUND;
	rv = mFinder->Find (uSearchString, PR_FALSE /* links only? */, &found);

	return NS_SUCCEEDED (rv) && (found == nsITypeAheadFind::FIND_FOUND || found == nsITypeAheadFind::FIND_WRAPPED);
#else
	nsEmbedString uSearchString;
	NS_CStringToUTF16 (nsEmbedCString (aSearchString ? aSearchString : ""),
			   NS_CSTRING_ENCODING_UTF8, uSearchString);

	mFinder->SetSearchString (uSearchString.get ());
	mFinder->SetFindBackwards (PR_FALSE);
	
	nsresult rv;
	PRBool didFind = PR_FALSE;
	rv = mFinder->FindNext (&didFind);
	
	return NS_SUCCEEDED (rv) && didFind;
#endif /* TYPEAHEADFIND */
}

PRBool
Yelper::FindAgain (PRBool aForward)
{
	if (!mInitialised) return PR_FALSE;

#ifdef TYPEAHEADFIND
	nsresult rv;
	PRUint16 found = nsITypeAheadFind::FIND_NOTFOUND;
	if (aForward) {
		rv = mFinder->FindNext (&found);
	}
	else {
		rv = mFinder->FindPrevious (&found);
	}

	return NS_SUCCEEDED (rv) && (found == nsITypeAheadFind::FIND_FOUND || found == nsITypeAheadFind::FIND_WRAPPED);
#else
	mFinder->SetFindBackwards (!aForward);
	
	nsresult rv;
	PRBool didFind = PR_FALSE;
	rv = mFinder->FindNext (&didFind);
	
	return NS_SUCCEEDED (rv) && didFind;
#endif /* TYPEAHEADFIND */
}

void
Yelper::ProcessMouseEvent (void* aEvent)
{
	g_return_if_fail (aEvent != NULL);

	nsIDOMEvent *domEvent = static_cast<nsIDOMEvent*>(aEvent);
	nsCOMPtr<nsIDOMMouseEvent> event (do_QueryInterface (domEvent));
	if (!event) return;

	PRUint16 button = 2;
	event->GetButton (&button);

	/* Mozilla uses 2 as its right mouse button code */
	if (button != 2) return;

	nsCOMPtr<nsIDOMNSEvent> nsEvent (do_QueryInterface (event));
	if (!nsEvent) return;

	nsresult rv;
	nsCOMPtr<nsIDOMEventTarget> originalTarget;
	rv = nsEvent->GetOriginalTarget (getter_AddRefs (originalTarget));
	if (NS_FAILED (rv) || !originalTarget) return;
	
	nsCOMPtr <nsIDOMHTMLAnchorElement> anchor (do_QueryInterface (originalTarget));
	if (!anchor) return;

	nsEmbedString href;
	rv = anchor->GetHref (href);
	if (NS_FAILED (rv) || !href.Length ()) return;

	nsEmbedCString cHref;
	NS_UTF16ToCString (href, NS_CSTRING_ENCODING_UTF8, cHref);
	if (!cHref.Length ()) return;

	g_signal_emit_by_name (mEmbed, "popupmenu_requested", cHref.get());
}

nsresult 
Yelper::Print (YelpPrintInfo *print_info, PRBool preview, int *prev_pages)
{
  nsresult rv;

  nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface (mWebBrowser, &rv));
  NS_ENSURE_SUCCESS (rv, rv);

  nsCOMPtr<nsIPrintSettings> settings;

  rv = print->GetGlobalPrintSettings (getter_AddRefs (settings));
  NS_ENSURE_SUCCESS (rv, rv);

  SetPrintSettings (print_info, settings);

  nsCOMPtr<PrintListener> listener = new PrintListener (print_info, print);

  if (!preview)
    rv = print->Print (settings, listener);
  else {
    rv = print->PrintPreview (settings, mDOMWindow, nsnull);
    rv |= print->GetPrintPreviewNumPages (prev_pages);
  }
  return rv;

}

nsresult
Yelper::PrintPreviewNavigate (int page_no)
{
  nsresult rv;
  nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface (mWebBrowser, &rv));
  NS_ENSURE_SUCCESS (rv, rv);

  return print->PrintPreviewNavigate (0, page_no);
}



nsresult 
Yelper::PrintPreviewEnd ()
{
  nsresult rv;
  nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface (mWebBrowser, &rv));
  NS_ENSURE_SUCCESS (rv, rv);

  return print->ExitPrintPreview ();

}

void
Yelper::SetPrintSettings (YelpPrintInfo *settings, nsIPrintSettings * target)
{
    char *base;
    const char *temp_dir;
    int fd;
    const GnomePrintUnit *unit, *inch, *mm;
    double value;
    nsEmbedString tmp;
    
    const static PRUnichar pName[] = { 'P', 'o', 's', 't', 'S', 'c', 'r', 'i', 'p', 't', '/', 'd', 'e', 'f', 'a', 'u', 'l', 't', '\0' };
    target->SetPrinterName(nsEmbedString(pName).get());
    
    const static int frame_types[] = {
	nsIPrintSettings::kFramesAsIs,
	nsIPrintSettings::kSelectedFrame,
	nsIPrintSettings::kEachFrameSep
    };

    switch (settings->range)
	{
	case GNOME_PRINT_RANGE_CURRENT:
	case GNOME_PRINT_RANGE_SELECTION_UNSENSITIVE:
	case GNOME_PRINT_RANGE_ALL:
	    target->SetPrintRange (nsIPrintSettings::kRangeAllPages);
	    break;
	case GNOME_PRINT_RANGE_RANGE:
	    target->SetPrintRange (nsIPrintSettings::kRangeSpecifiedPageRange);
	    target->SetStartPageRange (settings->from_page);
	    target->SetEndPageRange (settings->to_page);
	    break;
	case GNOME_PRINT_RANGE_SELECTION:
	    target->SetPrintRange (nsIPrintSettings::kRangeSelection);
	    break;
	}
    
    mm = gnome_print_unit_get_by_abbreviation ((const guchar *) "mm");
    inch = gnome_print_unit_get_by_abbreviation ((const guchar *) "in");
    g_assert (mm != NULL && inch != NULL);
    
    /* top margin */
    if (gnome_print_config_get_length (settings->config,
				       (const guchar *) GNOME_PRINT_KEY_PAGE_MARGIN_TOP,
				       &value, &unit)
	&& gnome_print_convert_distance (&value, unit, inch))
	{
	    target->SetMarginTop (value);
	}
    
    /* bottom margin */
    if (gnome_print_config_get_length (settings->config,
				       (const guchar *) GNOME_PRINT_KEY_PAGE_MARGIN_BOTTOM,
				       &value, &unit)
	&& gnome_print_convert_distance (&value, unit, inch))
	{
	    target->SetMarginBottom (value);
	}
    
    /* left margin */
    if (gnome_print_config_get_length (settings->config,
				       (const guchar *) GNOME_PRINT_KEY_PAGE_MARGIN_LEFT,
				       &value, &unit)
	&& gnome_print_convert_distance (&value, unit, inch))
	{
	    target->SetMarginLeft (value);
	}
    
    /* right margin */
    if (gnome_print_config_get_length (settings->config,
				       (const guchar *) GNOME_PRINT_KEY_PAGE_MARGIN_RIGHT,
				       &value, &unit)
	&& gnome_print_convert_distance (&value, unit, inch))
	{
	    target->SetMarginRight (value);
	}
    
    
    
    NS_CStringToUTF16 (nsEmbedCString(settings->header_left_string),
		       NS_CSTRING_ENCODING_UTF8, tmp);
    target->SetHeaderStrLeft (tmp.get());
    
    NS_CStringToUTF16 (nsEmbedCString(settings->header_center_string),
		       NS_CSTRING_ENCODING_UTF8, tmp);
    target->SetHeaderStrCenter (tmp.get());
    
    NS_CStringToUTF16 (nsEmbedCString(settings->header_right_string),
		       NS_CSTRING_ENCODING_UTF8, tmp);
    target->SetHeaderStrRight (tmp.get());
    
    NS_CStringToUTF16 (nsEmbedCString(settings->footer_left_string),
		       NS_CSTRING_ENCODING_UTF8, tmp); 
    target->SetFooterStrLeft (tmp.get());
    
    NS_CStringToUTF16 (nsEmbedCString(settings->footer_center_string),
		       NS_CSTRING_ENCODING_UTF8, tmp);
    target->SetFooterStrCenter(tmp.get());
    
    NS_CStringToUTF16 (nsEmbedCString(settings->footer_right_string),
		       NS_CSTRING_ENCODING_UTF8, tmp);
    target->SetFooterStrRight(tmp.get());
    
    
    
    temp_dir = g_get_tmp_dir ();
    base = g_build_filename (temp_dir, "printXXXXXX", NULL);
    fd = g_mkstemp (base);
    close(fd);
    settings->tempfile = g_strdup (base);
    
    g_free (base);
    
    
    NS_CStringToUTF16 (nsEmbedCString(settings->tempfile),
		       NS_CSTRING_ENCODING_UTF8, tmp);
    target->SetPrintToFile (PR_TRUE);
    target->SetToFileName (tmp.get());
    
    
    /* paper size */
    target->SetPaperSize (nsIPrintSettings::kPaperSizeDefined);
    target->SetPaperSizeUnit (nsIPrintSettings::kPaperSizeMillimeters);
    
    if (gnome_print_config_get_length (settings->config,
				       (const guchar *) GNOME_PRINT_KEY_PAPER_WIDTH,
				       &value, &unit)
	&& gnome_print_convert_distance (&value, unit, mm))
	{
	    target->SetPaperWidth (value);	
	}
    
    if (gnome_print_config_get_length (settings->config,
				       (const guchar *) GNOME_PRINT_KEY_PAPER_HEIGHT,
				       &value, &unit)
	&& gnome_print_convert_distance (&value, unit, mm))
	{
	    target->SetPaperHeight (value);	
	}
    
    /* Mozilla bug https://bugzilla.mozilla.org/show_bug.cgi?id=307404
     * means that we cannot actually use any paper sizes except mozilla's
     * builtin list, and we must refer to them *by name*!
     */
#ifndef HAVE_GECKO_1_9
    /* Gnome-Print names some papers differently than what moz understands */
    static const struct
    {
	const char *gppaper;
	const char *mozpaper;
    }
    paper_table [] =
	{
	    { "USLetter", "Letter" },
	    { "USLegal", "Legal" }
	};
#endif /* !HAVE_GECKO_1_9 */
    
    /* paper name */
    char *string = (char *) gnome_print_config_get (settings->config,
						    (const guchar *) GNOME_PRINT_KEY_PAPER_SIZE);
    const char *paper = string;
    
#ifndef HAVE_GECKO_1_9
    for (PRUint32 i = 0; i < G_N_ELEMENTS (paper_table); i++)
	{
	    if (string != NULL &&
		g_ascii_strcasecmp (paper_table[i].gppaper, string) == 0)
		{
		    paper = paper_table[i].mozpaper;
		    break;
		}
	}
#endif /* !HAVE_GECKO_1_9 */
    
    NS_CStringToUTF16 (nsEmbedCString(paper),
		       NS_CSTRING_ENCODING_UTF8, tmp);
    target->SetPaperName (tmp.get());
    g_free (string);
    
    /* paper orientation */
    string = (char *) gnome_print_config_get (settings->config,
					      (const guchar *) GNOME_PRINT_KEY_ORIENTATION);
    if (string == NULL) string = g_strdup ("R0");
    
    if (strncmp (string, "R90", 3) == 0 || strncmp (string, "R270", 4) == 0)
	{
	    target->SetOrientation (nsIPrintSettings::kLandscapeOrientation);
	}
    else
	{
	    target->SetOrientation (nsIPrintSettings::kPortraitOrientation);
	}
    g_free (string);
    
    target->SetPrintInColor (TRUE);
    target->SetPrintFrameType (frame_types[settings->frame_type]);


}
