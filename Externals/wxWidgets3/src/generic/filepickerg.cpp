///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/filepickerg.cpp
// Purpose:     wxGenericFileDirButton class implementation
// Author:      Francesco Montorsi
// Modified by:
// Created:     15/04/2006
// RCS-ID:      $Id: filepickerg.cpp 70732 2012-02-28 02:05:01Z VZ $
// Copyright:   (c) Francesco Montorsi
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_FILEPICKERCTRL || wxUSE_DIRPICKERCTRL

#include "wx/filename.h"
#include "wx/filepicker.h"

#include "wx/scopedptr.h"


// ============================================================================
// implementation
// ============================================================================

IMPLEMENT_DYNAMIC_CLASS(wxGenericFileButton, wxButton)
IMPLEMENT_DYNAMIC_CLASS(wxGenericDirButton, wxButton)

// ----------------------------------------------------------------------------
// wxGenericFileButton
// ----------------------------------------------------------------------------

bool wxGenericFileDirButton::Create(wxWindow *parent,
                                    wxWindowID id,
                                    const wxString& label,
                                    const wxString& path,
                                    const wxString& message,
                                    const wxString& wildcard,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    const wxValidator& validator,
                                    const wxString& name)
{
    m_pickerStyle = style;

    // If the special wxPB_SMALL flag is used, ignore the provided label and
    // use the shortest possible label and the smallest possible button fitting
    // it.
    long styleButton = 0;
    wxString labelButton;
    if ( m_pickerStyle & wxPB_SMALL )
    {
        labelButton = _("...");
        styleButton = wxBU_EXACTFIT;
    }
    else
    {
        labelButton = label;
    }

    // create this button
    if ( !wxButton::Create(parent, id, labelButton,
                           pos, size, styleButton, validator, name) )
    {
        wxFAIL_MSG( wxT("wxGenericFileButton creation failed") );
        return false;
    }

    // and handle user clicks on it
    Connect(GetId(), wxEVT_COMMAND_BUTTON_CLICKED,
            wxCommandEventHandler(wxGenericFileDirButton::OnButtonClick),
            NULL, this);

    // create the dialog associated with this button
    m_path = path;
    m_message = message;
    m_wildcard = wildcard;

    return true;
}

void wxGenericFileDirButton::OnButtonClick(wxCommandEvent& WXUNUSED(ev))
{
    wxScopedPtr<wxDialog> p(CreateDialog());
    if (p->ShowModal() == wxID_OK)
    {
        // save updated path in m_path
        UpdatePathFromDialog(p.get());

        // fire an event
        wxFileDirPickerEvent event(GetEventType(), this, GetId(), m_path);
        GetEventHandler()->ProcessEvent(event);
    }
}

void wxGenericFileDirButton::SetInitialDirectory(const wxString& dir)
{
    m_initialDir = dir;
}

// ----------------------------------------------------------------------------
// wxGenericFileutton
// ----------------------------------------------------------------------------

void
wxGenericFileButton::DoSetInitialDirectory(wxFileDialog* dialog,
                                           const wxString& dir)
{
    if ( m_path.find_first_of(wxFileName::GetPathSeparators()) ==
            wxString::npos )
    {
        dialog->SetDirectory(dir);
    }
}

wxDialog *wxGenericFileButton::CreateDialog()
{
    wxFileDialog* const dialog = new wxFileDialog
                                     (
                                        GetDialogParent(),
                                        m_message,
                                        wxEmptyString,
                                        wxEmptyString,
                                        m_wildcard,
                                        GetDialogStyle()
                                     );

    // If there is no default file or if it doesn't have any path, use the
    // explicitly set initial directory.
    //
    // Notice that it is important to call this before SetPath() below as if we
    // do have m_initialDir and no directory in m_path, we need to interpret
    // the path as being relative with respect to m_initialDir.
    if ( !m_initialDir.empty() )
        DoSetInitialDirectory(dialog, m_initialDir);

    // This sets both the default file name and the default directory of the
    // dialog if m_path contains directory part.
    dialog->SetPath(m_path);

    return dialog;
}

// ----------------------------------------------------------------------------
// wxGenericDirButton
// ----------------------------------------------------------------------------

wxDialog *wxGenericDirButton::CreateDialog()
{
    wxDirDialog* const dialog = new wxDirDialog
                                    (
                                        GetDialogParent(),
                                        m_message,
                                        m_path.empty() ? m_initialDir : m_path,
                                        GetDialogStyle()
                                    );
    return dialog;
}

#endif      // wxUSE_FILEPICKERCTRL || wxUSE_DIRPICKERCTRL
