/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/dirdlg.h
// Purpose:     wxDirDialog class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: dirdlg.h 67896 2011-06-09 00:28:28Z SC $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DIRDLG_H_
#define _WX_DIRDLG_H_

class WXDLLIMPEXP_CORE wxDirDialog : public wxDirDialogBase
{
public:
    wxDirDialog(wxWindow *parent,
                const wxString& message = wxDirSelectorPromptStr,
                const wxString& defaultPath = wxT(""),
                long style = wxDD_DEFAULT_STYLE,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                const wxString& name = wxDirDialogNameStr);

#if wxOSX_USE_COCOA
    ~wxDirDialog();
#endif

    virtual int ShowModal();

#if wxOSX_USE_COCOA
    virtual void ShowWindowModal();
    virtual void ModalFinishedCallback(void* panel, int returnCode);
#endif

protected:

    DECLARE_DYNAMIC_CLASS(wxDirDialog)

#if wxOSX_USE_COCOA
    WX_NSObject m_sheetDelegate;
#endif
};

#endif
    // _WX_DIRDLG_H_
