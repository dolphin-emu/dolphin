/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/dialog.cpp
// Purpose:
// Author:      Robert Roebling
// Id:          $Id: dialog.cpp 66559 2011-01-04 09:20:10Z SC $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "wx/dialog.h"

#ifndef WX_PRECOMP
    #include "wx/cursor.h"
#endif // WX_PRECOMP

#include "wx/evtloop.h"

#include "wx/scopedptr.h"

#include <gtk/gtk.h>

// this is defined in src/gtk/toplevel.cpp
extern int wxOpenModalDialogsCount;

wxDEFINE_TIED_SCOPED_PTR_TYPE(wxGUIEventLoop)


//-----------------------------------------------------------------------------
// wxDialog
//-----------------------------------------------------------------------------

void wxDialog::Init()
{
    m_modalLoop = NULL;
    m_returnCode = 0;
    m_modalShowing = false;
    m_themeEnabled = true;
}

wxDialog::wxDialog( wxWindow *parent,
                    wxWindowID id, const wxString &title,
                    const wxPoint &pos, const wxSize &size,
                    long style, const wxString &name )
{
    Init();

    (void)Create( parent, id, title, pos, size, style, name );
}

bool wxDialog::Create( wxWindow *parent,
                       wxWindowID id, const wxString &title,
                       const wxPoint &pos, const wxSize &size,
                       long style, const wxString &name )
{
    SetExtraStyle(GetExtraStyle() | wxTOPLEVEL_EX_DIALOG);

    // all dialogs should have tab traversal enabled
    style |= wxTAB_TRAVERSAL;

    return wxTopLevelWindow::Create(parent, id, title, pos, size, style, name);
}

bool wxDialog::Show( bool show )
{
    if (!show && IsModal())
    {
        EndModal( wxID_CANCEL );
    }

    if (show && CanDoLayoutAdaptation())
        DoLayoutAdaptation();

    bool ret = wxDialogBase::Show(show);

    if (show)
        InitDialog();

    return ret;
}

wxDialog::~wxDialog()
{
    // if the dialog is modal, this will end its event loop
    if ( IsModal() )
        EndModal(wxID_CANCEL);
}

bool wxDialog::IsModal() const
{
    return m_modalShowing;
}

void wxDialog::SetModal( bool WXUNUSED(flag) )
{
    wxFAIL_MSG( wxT("wxDialog:SetModal obsolete now") );
}

int wxDialog::ShowModal()
{
    wxASSERT_MSG( !IsModal(), "ShowModal() can't be called twice" );

    // release the mouse if it's currently captured as the window having it
    // will be disabled when this dialog is shown -- but will still keep the
    // capture making it impossible to do anything in the modal dialog itself
    wxWindow * const win = wxWindow::GetCapture();
    if ( win )
        win->GTKReleaseMouseAndNotify();

    wxWindow * const parent = GetParentForModalDialog();
    if ( parent )
    {
        gtk_window_set_transient_for( GTK_WINDOW(m_widget),
                                      GTK_WINDOW(parent->m_widget) );
    }

    wxBusyCursorSuspender cs; // temporarily suppress the busy cursor

    Show( true );

    m_modalShowing = true;

    wxOpenModalDialogsCount++;

    // NOTE: gtk_window_set_modal internally calls gtk_grab_add() !
    gtk_window_set_modal(GTK_WINDOW(m_widget), TRUE);

    // Run modal dialog event loop.
    {
        wxGUIEventLoopTiedPtr modal(&m_modalLoop, new wxGUIEventLoop());
        m_modalLoop->Run();
    }

    gtk_window_set_modal(GTK_WINDOW(m_widget), FALSE);

    wxOpenModalDialogsCount--;

    return GetReturnCode();
}

void wxDialog::EndModal( int retCode )
{
    SetReturnCode( retCode );

    if (!IsModal())
    {
        wxFAIL_MSG( "either wxDialog:EndModal called twice or ShowModal wasn't called" );
        return;
    }

    m_modalShowing = false;

    // Ensure Exit() is only called once. The dialog's event loop may be terminated
    // externally due to an uncaught exception.
    if (m_modalLoop && m_modalLoop->IsRunning())
        m_modalLoop->Exit();

    Show( false );
}
