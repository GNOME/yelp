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
#include "config.h"

#include <stdlib.h>
#include <unistd.h>

#include <nsStringAPI.h>

#include <nsCOMPtr.h>
#include <nsIComponentManager.h>
#include <nsIComponentRegistrar.h>
#include <nsIGenericFactory.h>
#include <nsILocalFile.h>
#include <nsIPrintSettings.h>
#include <nsServiceManagerUtils.h>
#include <nsXPCOM.h>

#include "yelp-gecko-services.h"

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

/* static functions */

/* static */void
PrintListener::SetPrintSettings (YelpPrintInfo *settings,
				 nsIPrintSettings * target)
{
    char *base;
    const char *temp_dir;
    int fd;
    const GnomePrintUnit *unit, *inch, *mm;
    double value;
    nsString tmp;
    
    target->SetPrinterName(NS_LITERAL_STRING("PostScript/default").get());
    
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
    
    
    
    NS_CStringToUTF16 (nsDependentCString(settings->header_left_string),
		       NS_CSTRING_ENCODING_UTF8, tmp);
    target->SetHeaderStrLeft (tmp.get());
    
    NS_CStringToUTF16 (nsDependentCString(settings->header_center_string),
		       NS_CSTRING_ENCODING_UTF8, tmp);
    target->SetHeaderStrCenter (tmp.get());
    
    NS_CStringToUTF16 (nsDependentCString(settings->header_right_string),
		       NS_CSTRING_ENCODING_UTF8, tmp);
    target->SetHeaderStrRight (tmp.get());
    
    NS_CStringToUTF16 (nsDependentCString(settings->footer_left_string),
		       NS_CSTRING_ENCODING_UTF8, tmp); 
    target->SetFooterStrLeft (tmp.get());
    
    NS_CStringToUTF16 (nsDependentCString(settings->footer_center_string),
		       NS_CSTRING_ENCODING_UTF8, tmp);
    target->SetFooterStrCenter(tmp.get());
    
    NS_CStringToUTF16 (nsDependentCString(settings->footer_right_string),
		       NS_CSTRING_ENCODING_UTF8, tmp);
    target->SetFooterStrRight(tmp.get());
    
    
    
    temp_dir = g_get_tmp_dir ();
    base = g_build_filename (temp_dir, "printXXXXXX", NULL);
    fd = g_mkstemp (base);
    close(fd);
    settings->tempfile = g_strdup (base);
    
    g_free (base);
    
    
    NS_CStringToUTF16 (nsDependentCString(settings->tempfile),
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
    
    NS_CStringToUTF16 (nsDependentCString(paper),
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

/* component registration */

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
    nsresult rv;
    nsCOMPtr<nsIComponentRegistrar> cr;
    rv = NS_GetComponentRegistrar(getter_AddRefs(cr));
    NS_ENSURE_SUCCESS (rv, );

    nsCOMPtr<nsIComponentManager> cm;
    rv = NS_GetComponentManager (getter_AddRefs (cm));
    NS_ENSURE_SUCCESS (rv, );

    nsCOMPtr<nsIGenericFactory> componentFactory;
    rv = NS_NewGenericFactory(getter_AddRefs(componentFactory),
			      &(sAppComps[0]));
    
    if (NS_FAILED(rv) || !componentFactory)
	{
	    g_warning ("Failed to make a factory for %s\n", sAppComps[0].mDescription);
	    return;
	}
   
    rv = cr->RegisterFactory(sAppComps[0].mCID,
			     sAppComps[0].mDescription,
			     sAppComps[0].mContractID,
			     componentFactory);
    if (NS_FAILED(rv))
	{
	    g_warning ("Failed to register %s\n", sAppComps[0].mDescription);
	}
    
}
