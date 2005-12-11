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

#ifndef __YELPER_H__
#define __YELPER_H__

#include <gtkmozembed.h>

#include <nsCOMPtr.h>
#include <nsIWebBrowser.h>
#include <nsIDOMWindow.h>
#ifdef TYPEAHEADFIND
#include <nsITypeAheadFind.h>
#else
#include <nsIWebBrowserFind.h>
#endif
#include <nsIPrintSettings.h>
#include <yelp-print.h>

class Yelper
{
public:
	Yelper (GtkMozEmbed *aEmbed);
	~Yelper ();

	nsresult Init ();
	void Destroy ();

	void DoCommand (const char *aCommand);

	void SetFindProperties (const char *aSearchString,
				PRBool aCaseSensitive,
				PRBool aWrap);
	PRBool Find (const char *aSearchString);
	PRBool FindAgain (PRBool aForward);

	void ProcessMouseEvent (void *aEvent);

	nsresult Print (YelpPrintInfo *print_info, PRBool preview,
			int *prev_pages);	
	nsresult PrintPreviewNavigate (int page_no);
	nsresult PrintPreviewEnd ();


private:
	PRBool mInitialised;
	GtkMozEmbed *mEmbed;
	nsCOMPtr<nsIWebBrowser> mWebBrowser;
	nsCOMPtr<nsIDOMWindow> mDOMWindow;
#ifdef TYPEAHEADFIND
	nsCOMPtr<nsITypeAheadFind> mFinder;
#else
	nsCOMPtr<nsIWebBrowserFind> mFinder;
#endif

	void SetPrintSettings (YelpPrintInfo *settings, nsIPrintSettings *target);
};

#endif /* !__YELPER_H__ */
