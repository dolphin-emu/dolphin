///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/taskbar.h
// Purpose:     wxTaskBarIcon class for GTK2
// Author:      Paul Cornett
// Created:     2009-02-08
// Copyright:   (c) 2009 Paul Cornett
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_TASKBARICON_H_
#define _WX_GTK_TASKBARICON_H_

class WXDLLIMPEXP_ADV wxTaskBarIcon: public wxTaskBarIconBase
{
public:
    wxTaskBarIcon(wxTaskBarIconType iconType = wxTBI_DEFAULT_TYPE);
    ~wxTaskBarIcon();
    virtual bool SetIcon(const wxIcon& icon, const wxString& tooltip = wxString()) wxOVERRIDE;
    virtual bool RemoveIcon() wxOVERRIDE;
    virtual bool PopupMenu(wxMenu* menu) wxOVERRIDE;
    bool IsOk() const { return true; }
    bool IsIconInstalled() const;

    class Private;

private:
    Private* m_priv;

    wxDECLARE_DYNAMIC_CLASS(wxTaskBarIcon);
    wxDECLARE_NO_COPY_CLASS(wxTaskBarIcon);
};

#endif // _WX_GTK_TASKBARICON_H_
