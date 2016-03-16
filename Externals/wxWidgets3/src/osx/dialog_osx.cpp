/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/dialog_osx.cpp
// Purpose:     wxDialog class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/dialog.h"
#include "wx/evtloop.h"
#include "wx/modalhook.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/frame.h"
    #include "wx/settings.h"
#endif // WX_PRECOMP

#include "wx/osx/private.h"

static int s_openDialogs = 0;
bool wxDialog::OSXHasModalDialogsOpen()
{
    return s_openDialogs > 0;
}

void wxDialog::OSXBeginModalDialog()
{
    s_openDialogs++;
}

void wxDialog::OSXEndModalDialog()
{
    wxASSERT_MSG( s_openDialogs > 0, "incorrect internal modal dialog count");
    s_openDialogs--;
}

void wxDialog::Init()
{
    m_modality = wxDIALOG_MODALITY_NONE;
    m_eventLoop = NULL;
}

bool wxDialog::Create( wxWindow *parent,
    wxWindowID id,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxString& name )
{
    SetExtraStyle( GetExtraStyle() | wxTOPLEVEL_EX_DIALOG );

    // All dialogs should really have this style...
    style |= wxTAB_TRAVERSAL;

    // ...but not these styles
    style &= ~(wxYES | wxOK | wxNO); // | wxCANCEL

    if ( !wxTopLevelWindow::Create( parent, id, title, pos, size, style, name ) )
        return false;

    return true;
}

wxDialog::~wxDialog()
{
    SendDestroyEvent();

    // if the dialog is modal, this will end its event loop
    Show(false);
}

// On mac command-stop does the same thing as Esc, let the base class know
// about it
bool wxDialog::IsEscapeKey(const wxKeyEvent& event)
{
    if ( event.GetKeyCode() == '.' && event.GetModifiers() == wxMOD_CONTROL )
        return true;

    return wxDialogBase::IsEscapeKey(event);
}

bool wxDialog::IsModal() const
{
    return m_modality != wxDIALOG_MODALITY_NONE;
}

bool wxDialog::Show(bool show)
{
    if ( m_modality == wxDIALOG_MODALITY_WINDOW_MODAL )
    {
        if ( !wxWindow::Show(show) )
            // nothing to do
            return false;
    }
    else
    {
        if ( !wxDialogBase::Show(show) )
            // nothing to do
            return false;
    }

    if (show && CanDoLayoutAdaptation())
        DoLayoutAdaptation();

    if ( show )
        // usually will result in TransferDataToWindow() being called
        InitDialog();

    if ( !show )
    {
        const int modalityOrig = m_modality;

        // complete the 'hiding' before we send the event
        m_modality = wxDIALOG_MODALITY_NONE;

        switch ( modalityOrig )
        {
            case wxDIALOG_MODALITY_WINDOW_MODAL:
                EndWindowModal(); // OS X implementation method for cleanup
                SendWindowModalDialogEvent ( wxEVT_WINDOW_MODAL_DIALOG_CLOSED  );
                break;
            default:
                break;
        }
    }

    return true;
}

// Replacement for Show(true) for modal dialogs - returns return code
int wxDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

    m_modality = wxDIALOG_MODALITY_APP_MODAL;

    Show();

    wxModalEventLoop modalLoop(this);
    m_eventLoop = &modalLoop;

    wxDialog::OSXBeginModalDialog();
    modalLoop.Run();
    wxDialog::OSXEndModalDialog();

    m_eventLoop = NULL;

    return GetReturnCode();
}

void wxDialog::ShowWindowModal()
{
    m_modality = wxDIALOG_MODALITY_WINDOW_MODAL;

    Show();

    DoShowWindowModal();
}

wxDialogModality wxDialog::GetModality() const
{
    return m_modality;
}

// NB: this function (surprisingly) may be called for both modal and modeless
//     dialogs and should work for both of them
void wxDialog::EndModal(int retCode)
{
    if ( m_eventLoop )
        m_eventLoop->Exit(retCode);

    SetReturnCode(retCode);
    Show(false);

    // Prevent app frame from taking z-order precedence
    if( GetParent() )
        GetParent()->Raise();
}

