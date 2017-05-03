/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/filedlg.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTKFILEDLG_H_
#define _WX_GTKFILEDLG_H_

#include "wx/gtk/filectrl.h"    // for wxGtkFileChooser

//-------------------------------------------------------------------------
// wxFileDialog
//-------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFileDialog: public wxFileDialogBase
{
public:
    wxFileDialog() { }

    wxFileDialog(wxWindow *parent,
                 const wxString& message = wxFileSelectorPromptStr,
                 const wxString& defaultDir = wxEmptyString,
                 const wxString& defaultFile = wxEmptyString,
                 const wxString& wildCard = wxFileSelectorDefaultWildcardStr,
                 long style = wxFD_DEFAULT_STYLE,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& sz = wxDefaultSize,
                 const wxString& name = wxFileDialogNameStr);
    bool Create(wxWindow *parent,
                 const wxString& message = wxFileSelectorPromptStr,
                 const wxString& defaultDir = wxEmptyString,
                 const wxString& defaultFile = wxEmptyString,
                 const wxString& wildCard = wxFileSelectorDefaultWildcardStr,
                 long style = wxFD_DEFAULT_STYLE,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& sz = wxDefaultSize,
                 const wxString& name = wxFileDialogNameStr);
    virtual ~wxFileDialog();

    virtual wxString GetPath() const wxOVERRIDE;
    virtual void GetPaths(wxArrayString& paths) const wxOVERRIDE;
    virtual wxString GetFilename() const wxOVERRIDE;
    virtual void GetFilenames(wxArrayString& files) const wxOVERRIDE;
    virtual int GetFilterIndex() const wxOVERRIDE;

    virtual void SetMessage(const wxString& message) wxOVERRIDE;
    virtual void SetPath(const wxString& path) wxOVERRIDE;
    virtual void SetDirectory(const wxString& dir) wxOVERRIDE;
    virtual void SetFilename(const wxString& name) wxOVERRIDE;
    virtual void SetWildcard(const wxString& wildCard) wxOVERRIDE;
    virtual void SetFilterIndex(int filterIndex) wxOVERRIDE;

    virtual int ShowModal() wxOVERRIDE;

    virtual bool SupportsExtraControl() const wxOVERRIDE { return true; }

    // Implementation only.
    void GTKSelectionChanged(const wxString& filename);


protected:
    // override this from wxTLW since the native
    // form doesn't have any m_wxwindow
    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO) wxOVERRIDE;


private:
    void OnFakeOk( wxCommandEvent &event );
    void OnSize(wxSizeEvent&);
    virtual void AddChildGTK(wxWindowGTK* child) wxOVERRIDE;

    wxGtkFileChooser    m_fc;

    wxDECLARE_DYNAMIC_CLASS(wxFileDialog);
    wxDECLARE_EVENT_TABLE();
};

#endif // _WX_GTKFILEDLG_H_
