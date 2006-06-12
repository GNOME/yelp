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
#include <config.h>

#include <stdlib.h>
#include <unistd.h>

#include <nsStringAPI.h>

#include <gtkmozembed.h>
#include <gtkmozembed_internal.h>
#include <nsComponentManagerUtils.h>
#include <nsICommandManager.h>
#include <nsIDocShell.h>
#include <nsIDOMDocument.h>
#include <nsIDOMEventTarget.h>
#include <nsIDOMHTMLAnchorElement.h>
#include <nsIDOMMouseEvent.h>
#include <nsIDOMNode.h>
#include <nsIDOMNSEvent.h>
#include <nsIDOMWindow.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIPrefService.h>
#include <nsIPrintSettings.h>
#include <nsITypeAheadFind.h>
#include <nsIWebBrowser.h>
#include <nsIWebBrowserPrint.h>
#include <nsIWebBrowserSetup.h>
#include <nsServiceManagerUtils.h>

#include "yelp-gecko-services.h"
#include "yelp-debug.h"

#include "Yelper.h"

#define NS_TYPEAHEADFIND_CONTRACTID "@mozilla.org/typeaheadfind;1"

Yelper::Yelper (GtkMozEmbed *aEmbed)
: mInitialised(PR_FALSE)
, mEmbed(aEmbed)
{
	debug_print (DB_DEBUG, "Yelper ctor [%p]\n", this);
}

Yelper::~Yelper ()
{
	debug_print (DB_DEBUG, "Yelper dtor [%p]\n", this);
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

	mFinder = do_CreateInstance (NS_TYPEAHEADFIND_CONTRACTID, &rv);
	NS_ENSURE_SUCCESS (rv, rv);

	nsCOMPtr<nsIDocShell> docShell (do_GetInterface (mWebBrowser, &rv));
	NS_ENSURE_SUCCESS (rv, rv);

	rv = mFinder->Init (docShell);
	NS_ENSURE_SUCCESS (rv, rv);

	mFinder->SetFocusLinks (PR_TRUE);

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

	mFinder->SetCaseSensitive (aCaseSensitive);
	/* aWrap not supported for typeaheadfind
	 * search string is set in ::Find for typeaheadfind
	 */
}

PRBool
Yelper::Find (const char *aSearchString)
{
	if (!mInitialised) return PR_FALSE;

	nsString uSearchString;
	NS_CStringToUTF16 (nsCString (aSearchString ? aSearchString : ""),
			   NS_CSTRING_ENCODING_UTF8, uSearchString);

	nsresult rv;
	PRUint16 found = nsITypeAheadFind::FIND_NOTFOUND;
	rv = mFinder->Find (uSearchString, PR_FALSE /* links only? */, &found);

	return NS_SUCCEEDED (rv) && (found == nsITypeAheadFind::FIND_FOUND || found == nsITypeAheadFind::FIND_WRAPPED);
}

PRBool
Yelper::FindAgain (PRBool aForward)
{
	if (!mInitialised) return PR_FALSE;

	nsresult rv;
	PRUint16 found = nsITypeAheadFind::FIND_NOTFOUND;
	if (aForward) {
		rv = mFinder->FindNext (&found);
	}
	else {
		rv = mFinder->FindPrevious (&found);
	}

	return NS_SUCCEEDED (rv) && (found == nsITypeAheadFind::FIND_FOUND || found == nsITypeAheadFind::FIND_WRAPPED);
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

	nsString href;
	rv = anchor->GetHref (href);
	if (NS_FAILED (rv) || !href.Length ()) return;

	nsCString cHref;
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

  PrintListener::SetPrintSettings (print_info, settings);

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
