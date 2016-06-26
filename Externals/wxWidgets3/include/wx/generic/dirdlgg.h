/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/dirdlgg.h
// Purpose:     wxGenericDirCtrl class
//              Builds on wxDirCtrl class written by Robert Roebling for the
//              wxFile application, modified by Harm van der Heijden.
//              Further modified for Windows.
// Author:      Robert Roebling, Harm van der Heijden, Julian Smart et al
// Modified by:
// Created:     21/3/2000
// Copyright:   (c) Robert Roebling, Harm van der Heijden, Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DIRDLGG_H_
#define _WX_DIRDLGG_H_

class WXDLLIMPEXP_FWD_CORE wxGenericDirCtrl;
class WXDLLIMPEXP_FWD_CORE wxTextCtrl;
class WXDLLIMPEXP_FWD_CORE wxTreeEvent;

// we may be included directly as well as from wx/dirdlg.h (FIXME)
extern WXDLLIMPEXP_DATA_CORE(const char) wxDirDialogNameStr[];
extern WXDLLIMPEXP_DATA_CORE(const char) wxDirSelectorPromptStr[];

#ifndef wxDD_DEFAULT_STYLE
#define wxDD_DEFAULT_STYLE      (wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER)
#endif

#include "wx/dialog.h"

//-----------------------------------------------------------------------------
// wxGenericDirDialog
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGenericDirDialog : public wxDirDialogBase
{
public:
    wxGenericDirDialog() : wxDirDialogBase() { }

    wxGenericDirDialog(wxWindow* parent,
                       const wxString& title = wxDirSelectorPromptStr,
                       const wxString& defaultPath = wxEmptyString,
                       long style = wxDD_DEFAULT_STYLE,
                       const wxPoint& pos = wxDefaultPosition,
                       const wxSize& sz = wxDefaultSize,//Size(450, 550),
                       const wxString& name = wxDirDialogNameStr);

    bool Create(wxWindow* parent,
                const wxString& title = wxDirSelectorPromptStr,
                const wxString& defaultPath = wxEmptyString,
                long style = wxDD_DEFAULT_STYLE,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& sz = wxDefaultSize,//Size(450, 550),
                       const wxString& name = wxDirDialogNameStr);

    //// Accessors
    void SetPath(const wxString& path) wxOVERRIDE;
    wxString GetPath() const wxOVERRIDE;

    //// Overrides
    virtual int ShowModal() wxOVERRIDE;
    virtual void EndModal(int retCode) wxOVERRIDE;

    // this one is specific to wxGenericDirDialog
    wxTextCtrl* GetInputCtrl() const { return m_input; }

protected:
    //// Event handlers
    void OnCloseWindow(wxCloseEvent& event);
    void OnOK(wxCommandEvent& event);
    void OnTreeSelected(wxTreeEvent &event);
    void OnTreeKeyDown(wxTreeEvent &event);
    void OnNew(wxCommandEvent& event);
    void OnGoHome(wxCommandEvent& event);
    void OnShowHidden(wxCommandEvent& event);

    wxGenericDirCtrl* m_dirCtrl;
    wxTextCtrl*       m_input;

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_DYNAMIC_CLASS(wxGenericDirDialog);
};

#endif // _WX_DIRDLGG_H_
