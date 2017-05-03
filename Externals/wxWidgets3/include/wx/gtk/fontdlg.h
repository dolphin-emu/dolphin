/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/fontdlg.h
// Purpose:     wxFontDialog
// Author:      Robert Roebling
// Created:
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_FONTDLG_H_
#define _WX_GTK_FONTDLG_H_

//-----------------------------------------------------------------------------
// wxFontDialog
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFontDialog : public wxFontDialogBase
{
public:
    wxFontDialog() : wxFontDialogBase() { /* must be Create()d later */ }
    wxFontDialog(wxWindow *parent)
        : wxFontDialogBase(parent) { Create(parent); }
    wxFontDialog(wxWindow *parent, const wxFontData& data)
        : wxFontDialogBase(parent, data) { Create(parent, data); }

    virtual ~wxFontDialog();

protected:
    // create the GTK dialog
    virtual bool DoCreate(wxWindow *parent) wxOVERRIDE;

    wxDECLARE_DYNAMIC_CLASS(wxFontDialog);
};

#endif
