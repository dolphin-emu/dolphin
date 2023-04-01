/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/dialog.mm
// Purpose:     wxDialog class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/dialog.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/frame.h"
    #include "wx/settings.h"
#endif // WX_PRECOMP

#include "wx/osx/private.h"

extern wxList wxModalDialogs;

void wxDialog::DoShowWindowModal()
{   
    wxTopLevelWindow* parent = static_cast<wxTopLevelWindow*>(wxGetTopLevelParent(GetParent()));
    
    wxASSERT_MSG(parent, "ShowWindowModal requires the dialog to have a parent.");
    
    NSWindow* parentWindow = parent->GetWXWindow();
    NSWindow* theWindow = GetWXWindow();
    
    [NSApp beginSheet: theWindow
            modalForWindow: parentWindow
            modalDelegate: theWindow
            didEndSelector: nil
            contextInfo: nil];
}

void wxDialog::EndWindowModal()
{
    [NSApp endSheet: GetWXWindow()];
    [GetWXWindow() orderOut:GetWXWindow()];
}
