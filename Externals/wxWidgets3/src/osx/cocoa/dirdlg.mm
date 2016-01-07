/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/dirdlg.mm
// Purpose:     wxDirDialog
// Author:      Stefan Csomor
// Modified by:
// Created:     2008-08-30
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
#include "wx/evtloop.h"
#include "wx/modalhook.h"

#include "wx/osx/private.h"

wxIMPLEMENT_CLASS(wxDirDialog, wxDialog);

void wxDirDialog::Init()
{
    m_sheetDelegate = nil;
}

void wxDirDialog::Create(wxWindow *parent, const wxString& message,
        const wxString& defaultPath, long style, const wxPoint& WXUNUSED(pos),
        const wxSize& WXUNUSED(size), const wxString& WXUNUSED(name))
{
    m_parent = parent;

    SetMessage( message );
    SetWindowStyle(style);
    SetPath(defaultPath);
    m_sheetDelegate = [[ModalDialogDelegate alloc] init];
    [(ModalDialogDelegate*)m_sheetDelegate setImplementation: this];
}

wxDirDialog::~wxDirDialog()
{
    [m_sheetDelegate release];
}

WX_NSOpenPanel wxDirDialog::OSXCreatePanel() const
{
    NSOpenPanel *oPanel = [NSOpenPanel openPanel];
    [oPanel setCanChooseDirectories:YES];
    [oPanel setResolvesAliases:YES];
    [oPanel setCanChooseFiles:NO];

    wxCFStringRef cf( m_message );
    [oPanel setMessage:cf.AsNSString()];

    if ( !HasFlag(wxDD_DIR_MUST_EXIST) )
        [oPanel setCanCreateDirectories:YES];

    return oPanel;
}

// We use several deprecated methods of NSOpenPanel in the code below, we
// should replace them with newer equivalents now that we don't support OS X
// versions which didn't have them (pre 10.6), but until then, get rid of
// the warning.
wxGCC_WARNING_SUPPRESS(deprecated-declarations)

void wxDirDialog::ShowWindowModal()
{
    wxNonOwnedWindow* parentWindow = NULL;

    if (GetParent())
        parentWindow = dynamic_cast<wxNonOwnedWindow*>(wxGetTopLevelParent(GetParent()));

    wxCHECK_RET(parentWindow, "Window modal display requires parent.");

    m_modality = wxDIALOG_MODALITY_WINDOW_MODAL;

    NSOpenPanel *oPanel = OSXCreatePanel();

    NSWindow* nativeParent = parentWindow->GetWXWindow();
    wxCFStringRef dir( m_path );
    [oPanel beginSheetForDirectory:dir.AsNSString() file:nil types: nil
        modalForWindow: nativeParent modalDelegate: m_sheetDelegate
        didEndSelector: @selector(sheetDidEnd:returnCode:contextInfo:)
        contextInfo: nil];
}

int wxDirDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

    wxCFEventLoopPauseIdleEvents pause;

    NSOpenPanel *oPanel = OSXCreatePanel();

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
        SetPath( wxCFStringRef::AsStringWithNormalizationFormC([[oPanel filenames] objectAtIndex:0]));
        result = wxID_OK;
    }
    SetReturnCode(result);

    if (GetModality() == wxDIALOG_MODALITY_WINDOW_MODAL)
        SendWindowModalDialogEvent ( wxEVT_WINDOW_MODAL_DIALOG_CLOSED  );
}

wxGCC_WARNING_RESTORE(deprecated-declarations)

#endif // wxUSE_DIRDLG
