/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/listbox.h
// Purpose:     wxListBox class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_LISTBOX_H_
#define _WX_LISTBOX_H_

// ----------------------------------------------------------------------------
// simple types
// ----------------------------------------------------------------------------
#include  "wx/dynarray.h"
#include  "wx/arrstr.h"

// forward decl for GetSelections()
class wxArrayInt;

// forward decl for wxListWidgetImpl implementation type.
class wxListWidgetImpl;

// List box item

WX_DEFINE_ARRAY( char* , wxListDataArray );

// ----------------------------------------------------------------------------
// List box control
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxListWidgetColumn;

class WXDLLIMPEXP_FWD_CORE wxListWidgetCellValue;

class WXDLLIMPEXP_CORE wxListBox : public wxListBoxBase
{
public:
    // ctors and such
    wxListBox();

    wxListBox(
        wxWindow *parent,
        wxWindowID winid,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        int n = 0, const wxString choices[] = NULL,
        long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxListBoxNameStr)
    {
        Create(parent, winid, pos, size, n, choices, style, validator, name);
    }

    wxListBox(
        wxWindow *parent,
        wxWindowID winid,
        const wxPoint& pos,
        const wxSize& size,
        const wxArrayString& choices,
        long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxListBoxNameStr)
    {
        Create(parent, winid, pos, size, choices, style, validator, name);
    }

    bool Create(
        wxWindow *parent,
        wxWindowID winid,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        int n = 0,
        const wxString choices[] = NULL,
        long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxListBoxNameStr);

    bool Create(
        wxWindow *parent,
        wxWindowID winid,
        const wxPoint& pos,
        const wxSize& size,
        const wxArrayString& choices,
        long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxListBoxNameStr);

    virtual ~wxListBox();

    // implement base class pure virtuals
    virtual void Refresh(bool eraseBack = true, const wxRect *rect = NULL) wxOVERRIDE;

    virtual unsigned int GetCount() const wxOVERRIDE;
    virtual wxString GetString(unsigned int n) const wxOVERRIDE;
    virtual void SetString(unsigned int n, const wxString& s) wxOVERRIDE;
    virtual int FindString(const wxString& s, bool bCase = false) const wxOVERRIDE;

    // data callbacks
    virtual void GetValueCallback( unsigned int n, wxListWidgetColumn* col , wxListWidgetCellValue& value );
    virtual void SetValueCallback( unsigned int n, wxListWidgetColumn* col , wxListWidgetCellValue& value );

    virtual bool IsSelected(int n) const wxOVERRIDE;
    virtual int GetSelection() const wxOVERRIDE;
    virtual int GetSelections(wxArrayInt& aSelections) const wxOVERRIDE;

    virtual void EnsureVisible(int n) wxOVERRIDE;

    virtual int GetTopItem() const wxOVERRIDE;

    virtual wxVisualAttributes GetDefaultAttributes() const wxOVERRIDE
    {
        return GetClassDefaultAttributes(GetWindowVariant());
    }

    // wxCheckListBox support
    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    wxListWidgetImpl* GetListPeer() const;

    bool MacGetBlockEvents() const { return m_blockEvents; }

    virtual void HandleLineEvent( unsigned int n, bool doubleClick );
protected:
    // callback for derived classes which may have to insert additional data
    // at a certain line - which cannot be predetermined for sorted list data
    virtual void OnItemInserted(unsigned int pos);

    virtual void DoClear() wxOVERRIDE;
    virtual void DoDeleteOneItem(unsigned int n) wxOVERRIDE;

    // from wxItemContainer
    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type) wxOVERRIDE;

    virtual void DoSetItemClientData(unsigned int n, void* clientData) wxOVERRIDE;
    virtual void* DoGetItemClientData(unsigned int n) const wxOVERRIDE;

    // from wxListBoxBase
    virtual void DoSetSelection(int n, bool select) wxOVERRIDE;
    virtual void DoSetFirstItem(int n) wxOVERRIDE;
    virtual int DoListHitTest(const wxPoint& point) const wxOVERRIDE;

    // free memory (common part of Clear() and dtor)
    // prevent collision with some BSD definitions of macro Free()
    void FreeData();

    virtual wxSize DoGetBestSize() const wxOVERRIDE;

    bool m_blockEvents;

    wxListWidgetColumn* m_textColumn;

    // data storage (copied from univ)

    // the array containing all items (it is sorted if the listbox has
    // wxLB_SORT style)
    union
    {
        wxArrayString *unsorted;
        wxSortedArrayString *sorted;
    } m_strings;

    // and this one the client data (either void or wxClientData)
    wxArrayPtrVoid m_itemsClientData;

private:

    wxDECLARE_DYNAMIC_CLASS(wxListBox);
    wxDECLARE_EVENT_TABLE();
};

#endif // _WX_LISTBOX_H_
