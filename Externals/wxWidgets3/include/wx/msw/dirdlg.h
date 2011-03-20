/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/dirdlg.h
// Purpose:     wxDirDialog class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// RCS-ID:      $Id: dirdlg.h 52834 2008-03-26 15:06:00Z FM $
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DIRDLG_H_
#define _WX_DIRDLG_H_

class WXDLLIMPEXP_CORE wxDirDialog : public wxDirDialogBase
{
public:
    wxDirDialog(wxWindow *parent,
                const wxString& message = wxDirSelectorPromptStr,
                const wxString& defaultPath = wxEmptyString,
                long style = wxDD_DEFAULT_STYLE,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                const wxString& name = wxDirDialogNameStr);

    void SetPath(const wxString& path);

    virtual int ShowModal();

private:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDirDialog)
};

#endif
    // _WX_DIRDLG_H_
