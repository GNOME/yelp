/*
 *  Copyright (C) 2002 Philip Langdale
 *  Copyright (C) 2003-2004 Christian Persch
 *  Copyright (C) 2005 Juerg Billeter
 *  Copyright (C) 2005 Don Scorgie <DonScorgie@Blueyonder.co.uk>
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
 */

#include <mozilla-config.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "nsError.h"
#undef MOZILLA_INTERNAL_API
#include <nsEmbedString.h>
#define MOZILLA_INTERNAL_API 1

#include "yelp-gecko-services.h"

#include <nsIPrintSettings.h>
#include <nsCOMPtr.h>
#include <nsIComponentRegistrar.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIGenericFactory.h>
#include <nsILocalFile.h>

/* Implementation file */
NS_IMPL_ISUPPORTS3(GPrintingPromptService, nsIPrintingPromptService, nsIWebProgressListener, nsIPrintProgressParams)

GPrintingPromptService::GPrintingPromptService()
{
	mPrintInfo = NULL;
}

GPrintingPromptService::~GPrintingPromptService()
{
	if (mPrintInfo != NULL)
	{
		yelp_print_info_free (mPrintInfo);
	}
}

/* void showPrintDialog (in nsIDOMWindow parent, in nsIWebBrowserPrint webBrowserPrint, in nsIPrintSettings printSettings); */
NS_IMETHODIMP GPrintingPromptService::ShowPrintDialog(nsIDOMWindow *parent, nsIWebBrowserPrint *webBrowserPrint, nsIPrintSettings *printSettings)
{
	return NS_OK;
  
}

/* void showProgress (in nsIDOMWindow parent, in nsIWebBrowserPrint webBrowserPrint, in nsIPrintSettings printSettings, in nsIObserver openDialogObserver, in boolean isForPrinting, out nsIWebProgressListener webProgressListener, out nsIPrintProgressParams printProgressParams, out boolean notifyOnOpen); */
NS_IMETHODIMP GPrintingPromptService::ShowProgress(nsIDOMWindow *parent, nsIWebBrowserPrint *webBrowserPrint, nsIPrintSettings *printSettings, nsIObserver *openDialogObserver, PRBool isForPrinting, nsIWebProgressListener **webProgressListener, nsIPrintProgressParams **printProgressParams, PRBool *notifyOnOpen)
{
  return NS_OK;
}

/* void showPageSetup (in nsIDOMWindow parent, in nsIPrintSettings printSettings, in nsIObserver printObserver); */
NS_IMETHODIMP GPrintingPromptService::ShowPageSetup(nsIDOMWindow *parent, nsIPrintSettings *printSettings, 
						    nsIObserver *printObserver)
{
  return NS_OK;
}

/* void showPrinterProperties (in nsIDOMWindow parent, in wstring printerName, in nsIPrintSettings printSettings); */
NS_IMETHODIMP GPrintingPromptService::ShowPrinterProperties(nsIDOMWindow *parent, const PRUnichar *printerName, nsIPrintSettings *printSettings)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP GPrintingPromptService::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
	return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP GPrintingPromptService::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
    return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP GPrintingPromptService::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP GPrintingPromptService::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP GPrintingPromptService::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute wstring docTitle; */
NS_IMETHODIMP GPrintingPromptService::GetDocTitle(PRUnichar * *aDocTitle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP GPrintingPromptService::SetDocTitle(const PRUnichar * aDocTitle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute wstring docURL; */
NS_IMETHODIMP GPrintingPromptService::GetDocURL(PRUnichar * *aDocURL)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP GPrintingPromptService::SetDocURL(const PRUnichar * aDocURL)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMPL_ISUPPORTS1(PrintListener, nsIWebProgressListener)

  PrintListener::PrintListener (YelpPrintInfo *in, nsIWebBrowserPrint *p)
{
  info = in;
  print = p;
  cancel_happened = FALSE;
  /*NULL*/

}

PrintListener::~PrintListener ()
{
  /*NULL*/
}

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP PrintListener::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP PrintListener::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
  yelp_print_update_progress (info, 
		      (1.0 * aCurTotalProgress) / (aMaxTotalProgress * 1.0));

  if (info->cancelled && !cancel_happened) {
    /* This doesn't seem to actually cancel anything.
     * therefore, the best course of action is to ignore it
     * until we've finished printing to the file
     * and then free it - Mozilla bug #253926
     */
    print->Cancel();
    cancel_happened = TRUE;
  }
  if (aCurTotalProgress == 100 && aMaxTotalProgress == 100) /* 100% finished */
    yelp_print_moz_finished (info);
  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP PrintListener::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP PrintListener::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP PrintListener::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(GPrintingPromptService)

static const nsModuleComponentInfo sAppComps[] = {
    {
	G_PRINTINGPROMPTSERVICE_CLASSNAME,
	G_PRINTINGPROMPTSERVICE_CID,
	G_PRINTINGPROMPTSERVICE_CONTRACTID,
	GPrintingPromptServiceConstructor
    },
};



void
yelp_register_printing ()
{
    gboolean ret = TRUE;
    nsresult rv;
    nsresult result;

    nsCOMPtr<nsIComponentRegistrar> cr;
    
    result = NS_GetComponentRegistrar(getter_AddRefs(cr));
    nsCOMPtr<nsIComponentManager> cm;
    NS_GetComponentManager (getter_AddRefs (cm));
    nsCOMPtr<nsIGenericFactory> componentFactory;
    rv = NS_NewGenericFactory(getter_AddRefs(componentFactory),
			      &(sAppComps[0]));
    
    if (NS_FAILED(rv) || !componentFactory)
	{
	    g_warning ("Failed to make a factory for %s\n", sAppComps[0].mDescription);
	    
	    ret = FALSE;
	}
   
    rv = cr->RegisterFactory(sAppComps[0].mCID,
			     sAppComps[0].mDescription,
			     sAppComps[0].mContractID,
			     componentFactory);
    if (NS_FAILED(rv))
	{
	    g_warning ("Failed to register %s\n", sAppComps[0].mDescription);
	    
	    ret = FALSE;
	}
    
}
