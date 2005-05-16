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

#include <nsIClipboardCommands.h>
#include <nsICommandManager.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIPrefService.h>
#include <nsIServiceManager.h>
#include <nsIDOMMouseEvent.h>
#include <nsIDOMNSEvent.h>
#include <nsIDOMEventTarget.h>
#include <nsIDOMNode.h>
#include <nsIDOMHTMLAnchorElement.h>

#ifdef TYPEAHEADFIND
#include <nsIDocShell.h>
#define NS_TYPEAHEADFIND_CONTRACTID "@mozilla.org/typeaheadfind;1"
#endif /* TYPEAHEADFIND */

#include <stdlib.h>

#include <gtkmozembed_internal.h>

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

	mWebBrowser->GetContentDOMWindow (getter_AddRefs (mDOMWindow));
	NS_ENSURE_TRUE (mDOMWindow, rv);

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
