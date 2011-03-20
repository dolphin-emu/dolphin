/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/printdlg.cpp
// Purpose:     wxPrintDialog, wxPageSetupDialog
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: printdlg.cpp 64068 2010-04-20 19:09:38Z SC $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_PRINTING_ARCHITECTURE

#include "wx/printdlg.h"

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
#include "wx/statline.h"

int wxMacPrintDialog::ShowModal()
{
    m_printDialogData.GetPrintData().ConvertToNative();
    ((wxOSXPrintData*)m_printDialogData.GetPrintData().GetNativeData())->TransferFrom( &m_printDialogData );

    int result = wxID_CANCEL;

    OSErr err = noErr;
    Boolean accepted;
    wxOSXPrintData* nativeData = (wxOSXPrintData*)m_printDialogData.GetPrintData().GetNativeData();
    wxDialog::OSXBeginModalDialog();
    err = PMSessionPrintDialog(nativeData->GetPrintSession(), nativeData->GetPrintSettings(),
        nativeData->GetPageFormat(), &accepted );
    wxDialog::OSXEndModalDialog();

    if ((err == noErr) && !accepted)
    {
        // user clicked Cancel button
        err = kPMCancel;
    }

    if (err == noErr)
    {
        result = wxID_OK;
    }

    if ((err != noErr) && (err != kPMCancel))
    {
        wxString message;

        message.Printf( wxT("Print Error %d"), err );
        wxMessageDialog dialog( NULL, message, wxEmptyString, wxICON_HAND | wxOK );
        dialog.ShowModal();
    }

    if (result == wxID_OK)
    {
        m_printDialogData.GetPrintData().ConvertFromNative();
        ((wxOSXPrintData*)m_printDialogData.GetPrintData().GetNativeData())->TransferTo( &m_printDialogData );
    }
    return result;
}

int wxMacPageSetupDialog::ShowModal()
{
    m_pageSetupData.GetPrintData().ConvertToNative();
    wxOSXPrintData* nativeData = (wxOSXPrintData*)m_pageSetupData.GetPrintData().GetNativeData();
    nativeData->TransferFrom( &m_pageSetupData );

    int result = wxID_CANCEL;
    OSErr err = noErr;
    Boolean accepted;

    wxDialog::OSXBeginModalDialog();
    err = PMSessionPageSetupDialog( nativeData->GetPrintSession(), nativeData->GetPageFormat(),
        &accepted );
    wxDialog::OSXEndModalDialog();

    if ((err == noErr) && !accepted)
    {
        // user clicked Cancel button
        err = kPMCancel;
    }

    // If the user did not cancel, flatten and save the PageFormat object
    // with our document.
    if (err == noErr)
    {
        result = wxID_OK;
    }

    if ((err != noErr) && (err != kPMCancel))
    {
        wxString message;

        message.Printf( wxT("Print Error %d"), err );
        wxMessageDialog dialog( NULL, message, wxEmptyString, wxICON_HAND | wxOK );
        dialog.ShowModal();
    }

    if (result == wxID_OK)
    {
        m_pageSetupData.GetPrintData().ConvertFromNative();
        m_pageSetupData.SetPaperSize( m_pageSetupData.GetPrintData().GetPaperSize() );
        nativeData->TransferTo( &m_pageSetupData );
    }
    return result;
}

#endif
