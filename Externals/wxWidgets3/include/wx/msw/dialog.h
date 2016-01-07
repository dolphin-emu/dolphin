/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/dialog.h
// Purpose:     wxDialog class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DIALOG_H_
#define _WX_DIALOG_H_

#include "wx/panel.h"

extern WXDLLIMPEXP_DATA_CORE(const char) wxDialogNameStr[];

class WXDLLIMPEXP_FWD_CORE wxDialogModalData;

// Dialog boxes
class WXDLLIMPEXP_CORE wxDialog : public wxDialogBase
{
public:
    wxDialog() { Init(); }

    // full ctor
    wxDialog(wxWindow *parent, wxWindowID id,
             const wxString& title,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = wxDEFAULT_DIALOG_STYLE,
             const wxString& name = wxDialogNameStr)
    {
        Init();

        (void)Create(parent, id, title, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_DIALOG_STYLE,
                const wxString& name = wxDialogNameStr);

    virtual ~wxDialog();

    // return true if we're showing the dialog modally
    virtual bool IsModal() const { return m_modalData != NULL; }

    // show the dialog modally and return the value passed to EndModal()
    virtual int ShowModal();

    // may be called to terminate the dialog with the given return code
    virtual void EndModal(int retCode);


    // implementation only from now on
    // -------------------------------

    // override some base class virtuals
    virtual bool Show(bool show = true);
    virtual void SetWindowStyleFlag(long style);

    // Windows callbacks
    WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam);

protected:
    // common part of all ctors
    void Init();

private:
    // these functions deal with the gripper window shown in the corner of
    // resizable dialogs
    void CreateGripper();
    void DestroyGripper();
    void ShowGripper(bool show);
    void ResizeGripper();

    // this function is used to adjust Z-order of new children relative to the
    // gripper if we have one
    void OnWindowCreate(wxWindowCreateEvent& event);

    // gripper window for a resizable dialog, NULL if we're not resizable
    WXHWND m_hGripper;

    // this pointer is non-NULL only while the modal event loop is running
    wxDialogModalData *m_modalData;

    wxDECLARE_DYNAMIC_CLASS(wxDialog);
    wxDECLARE_NO_COPY_CLASS(wxDialog);
};

#endif
    // _WX_DIALOG_H_
