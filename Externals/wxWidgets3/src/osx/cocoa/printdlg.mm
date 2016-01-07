/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/printdlg.mm
// Purpose:     wxPrintDialog, wxPageSetupDialog
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_PRINTING_ARCHITECTURE

#include "wx/printdlg.h"
#include "wx/modalhook.h"

#ifndef WX_PRECOMP
    #include "wx/object.h"
    #include "wx/dcprint.h"
    #include "wx/msgdlg.h"
    #include "wx/textctrl.h"
    #include "wx/sizer.h"
    #include "wx/stattext.h"
#endif

#include "wx/osx/printdlg.h"
#include "wx/osx/private/print.h"
#include "wx/osx/private.h"

wxIMPLEMENT_CLASS(wxOSXCocoaPrintData, wxOSXPrintData);

wxOSXCocoaPrintData::wxOSXCocoaPrintData()
{
    m_macPrintInfo = [[NSPrintInfo alloc] init];
    m_macPageFormat = (PMPageFormat) [m_macPrintInfo PMPageFormat];
    m_macPrintSettings = (PMPrintSettings) [m_macPrintInfo PMPrintSettings];
    m_macPrintSession = (PMPrintSession) [m_macPrintInfo PMPrintSession] ;
    PMGetPageFormatPaper(m_macPageFormat, &m_macPaper);
}

wxOSXCocoaPrintData::~wxOSXCocoaPrintData()
{
    [m_macPrintInfo release];
}

void wxOSXCocoaPrintData::UpdateFromPMState()
{
    [m_macPrintInfo updateFromPMPageFormat];
    [m_macPrintInfo updateFromPMPrintSettings];
}

void wxOSXCocoaPrintData::UpdateToPMState()
{
    m_macPageFormat = (PMPageFormat) [m_macPrintInfo PMPageFormat];
    m_macPrintSettings = (PMPrintSettings) [m_macPrintInfo PMPrintSettings];
    m_macPrintSession = (PMPrintSession) [m_macPrintInfo PMPrintSession] ;
}

int wxMacPrintDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

    m_printDialogData.GetPrintData().ConvertToNative();

    int result = wxID_CANCEL;

    NSPrintPanel* panel = [NSPrintPanel printPanel];
    NSPrintInfo* printInfo = ((wxOSXCocoaPrintData*)m_printDialogData.GetPrintData().GetNativeData())->GetNSPrintInfo();

    NSMutableDictionary* dict = [printInfo printSettings];
    [dict setValue:[NSNumber numberWithInt:m_printDialogData.GetMinPage()] forKey:@"com_apple_print_PrintSettings_PMFirstPage"];
    [dict setValue:[NSNumber numberWithInt:m_printDialogData.GetMaxPage()] forKey:@"com_apple_print_PrintSettings_PMLastPage"];

    if ( (NSInteger)[panel runModalWithPrintInfo:printInfo] == NSOKButton )
    {
        result = wxID_OK;
        m_printDialogData.GetPrintData().ConvertFromNative();
        ((wxOSXPrintData*)m_printDialogData.GetPrintData().GetNativeData())->TransferTo( &m_printDialogData );
    }

    return result;
}

int wxMacPageSetupDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

    m_pageSetupData.GetPrintData().ConvertToNative();
    ((wxOSXCocoaPrintData*)m_pageSetupData.GetPrintData().GetNativeData())->TransferFrom( &m_pageSetupData );

    int result = wxID_CANCEL;

    NSPageLayout *pageLayout = [NSPageLayout pageLayout];
    NSPrintInfo* printInfo = ((wxOSXCocoaPrintData*)m_pageSetupData.GetPrintData().GetNativeData())->GetNSPrintInfo();
    if ( [pageLayout runModalWithPrintInfo:printInfo] == NSOKButton )
    {
        result = wxID_OK;
        m_pageSetupData.GetPrintData().ConvertFromNative();
        m_pageSetupData.SetPaperSize( m_pageSetupData.GetPrintData().GetPaperSize() );
    }

    return result;
}

#endif
