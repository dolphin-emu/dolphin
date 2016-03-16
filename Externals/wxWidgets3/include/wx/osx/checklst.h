///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/checklst.h
// Purpose:     wxCheckListBox class - a listbox with checkable items
//              Note: this is an optional class.
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MAC_CHECKLST_H_
#define _WX_MAC_CHECKLST_H_

class WXDLLIMPEXP_CORE wxCheckListBox : public wxCheckListBoxBase
{
public:
    // ctors
    wxCheckListBox() { Init(); }
    wxCheckListBox(wxWindow *parent,
                   wxWindowID id,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   int nStrings = 0,
                   const wxString *choices = NULL,
                   long style = 0,
                   const wxValidator& validator = wxDefaultValidator,
                   const wxString& name = wxListBoxNameStr)
    {
        Init();

        Create(parent, id, pos, size, nStrings, choices, style, validator, name);
    }
    wxCheckListBox(wxWindow *parent,
                   wxWindowID id,
                   const wxPoint& pos,
                   const wxSize& size,
                   const wxArrayString& choices,
                   long style = 0,
                   const wxValidator& validator = wxDefaultValidator,
                   const wxString& name = wxListBoxNameStr)
    {
        Init();

        Create(parent, id, pos, size, choices, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                int nStrings = 0,
                const wxString *choices = NULL,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxListBoxNameStr);
    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos,
                const wxSize& size,
                const wxArrayString& choices,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxListBoxNameStr);

    // items may be checked
    bool  IsChecked(unsigned int uiIndex) const wxOVERRIDE;
    void  Check(unsigned int uiIndex, bool bCheck = true) wxOVERRIDE;

    // data callbacks
    virtual void GetValueCallback( unsigned int n, wxListWidgetColumn* col , wxListWidgetCellValue& value ) wxOVERRIDE;
    virtual void SetValueCallback( unsigned int n, wxListWidgetColumn* col , wxListWidgetCellValue& value ) wxOVERRIDE;

protected:
   // override all methods which add/delete items to update m_checks array as
    // well
    virtual void OnItemInserted(unsigned int pos) wxOVERRIDE;
    virtual void DoDeleteOneItem(unsigned int n) wxOVERRIDE;
    virtual void DoClear() wxOVERRIDE;

    // the array containing the checked status of the items
    wxArrayInt m_checks;

    wxListWidgetColumn* m_checkColumn ;

    void Init();

private:
    wxDECLARE_EVENT_TABLE();
    wxDECLARE_DYNAMIC_CLASS(wxCheckListBox);
};

#endif // _WX_MAC_CHECKLST_H_
