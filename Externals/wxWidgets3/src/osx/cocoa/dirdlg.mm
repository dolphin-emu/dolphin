/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/dirdlg.mm
// Purpose:     wxDirDialog
// Author:      Stefan Csomor
// Modified by:
// Created:     2008-08-30
// RCS-ID:      $Id: dirdlg.mm 67232 2011-03-18 15:10:15Z DS $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_DIRDLG && !defined(__WXUNIVERSAL__)

#include "wx/dirdlg.h"

#ifndef WX_PRECOMP
    #include "wx/msgdlg.h"
    #include "wx/filedlg.h"
    #include "wx/app.h"
#endif

#include "wx/filename.h"

#include "wx/osx/private.h"

IMPLEMENT_CLASS(wxDirDialog, wxDialog)

wxDirDialog::wxDirDialog(wxWindow *parent, const wxString& message,
        const wxString& defaultPath, long style, const wxPoint& WXUNUSED(pos),
        const wxSize& WXUNUSED(size), const wxString& WXUNUSED(name))
{
    m_parent = parent;

    SetMessage( message );
    SetWindowStyle(style);
    SetPath(defaultPath);
}

void wxDirDialog::ShowWindowModal()
{
    wxCFStringRef dir( m_path );
    
    m_modality = wxDIALOG_MODALITY_WINDOW_MODAL;
    
    NSOpenPanel *oPanel = [NSOpenPanel openPanel];
    [oPanel setCanChooseDirectories:YES];
    [oPanel setResolvesAliases:YES];
    [oPanel setCanChooseFiles:NO];

    wxCFStringRef cf( m_message );
    [oPanel setMessage:cf.AsNSString()];

    if ( HasFlag(wxDD_NEW_DIR_BUTTON) )
        [oPanel setCanCreateDirectories:YES];
    
    wxNonOwnedWindow* parentWindow = NULL;
    
    if (GetParent())
        parentWindow = dynamic_cast<wxNonOwnedWindow*>(wxGetTopLevelParent(GetParent()));
    
    wxASSERT_MSG(parentWindow, "Window modal display requires parent.");
    
    if (parentWindow)
    {
        NSWindow* nativeParent = parentWindow->GetWXWindow();
        ModalDialogDelegate* sheetDelegate = [[ModalDialogDelegate alloc] init];
        [sheetDelegate setImplementation: this];
        [oPanel beginSheetForDirectory:dir.AsNSString() file:nil types: nil
            modalForWindow: nativeParent modalDelegate: sheetDelegate
            didEndSelector: @selector(sheetDidEnd:returnCode:contextInfo:)
            contextInfo: nil];
    }
}

int wxDirDialog::ShowModal()
{
    NSOpenPanel *oPanel = [NSOpenPanel openPanel];
    [oPanel setCanChooseDirectories:YES];
    [oPanel setResolvesAliases:YES];
    [oPanel setCanChooseFiles:NO];

    wxCFStringRef cf( m_message );
    [oPanel setMessage:cf.AsNSString()];

    if ( HasFlag(wxDD_NEW_DIR_BUTTON) )
        [oPanel setCanCreateDirectories:YES];

    wxCFStringRef dir( m_path );

    m_path = wxEmptyString;

    int returnCode = -1;

    returnCode = (NSInteger)[oPanel runModalForDirectory:dir.AsNSString() file:nil types:nil];
    ModalFinishedCallback(oPanel, returnCode);

    return GetReturnCode();
}

void wxDirDialog::ModalFinishedCallback(void* panel, int returnCode)
{
    int result = wxID_CANCEL;

    if (returnCode == NSOKButton )
    {
        NSOpenPanel* oPanel = (NSOpenPanel*)panel;
        SetPath( wxCFStringRef::AsString([[oPanel filenames] objectAtIndex:0]));
        result = wxID_OK;
    }
    SetReturnCode(result);
    
    if (GetModality() == wxDIALOG_MODALITY_WINDOW_MODAL)
        SendWindowModalDialogEvent ( wxEVT_WINDOW_MODAL_DIALOG_CLOSED  );
}

#endif // wxUSE_DIRDLG
