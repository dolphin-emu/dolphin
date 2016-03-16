/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/listbox.h
// Purpose:     wxListBox class declaration
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_LISTBOX_H_
#define _WX_GTK_LISTBOX_H_

struct _wxTreeEntry;
struct _GtkTreeIter;

//-----------------------------------------------------------------------------
// wxListBox
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxListBox : public wxListBoxBase
{
public:
    // ctors and such
    wxListBox()
    {
        Init();
    }
    wxListBox( wxWindow *parent, wxWindowID id,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            int n = 0, const wxString choices[] = (const wxString *) NULL,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxListBoxNameStr )
    {
        Init();
        Create(parent, id, pos, size, n, choices, style, validator, name);
    }
    wxListBox( wxWindow *parent, wxWindowID id,
            const wxPoint& pos,
            const wxSize& size,
            const wxArrayString& choices,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxListBoxNameStr )
    {
        Init();
        Create(parent, id, pos, size, choices, style, validator, name);
    }
    virtual ~wxListBox();

    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                int n = 0, const wxString choices[] = (const wxString *) NULL,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxListBoxNameStr);
    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos,
                const wxSize& size,
                const wxArrayString& choices,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxListBoxNameStr);

    virtual unsigned int GetCount() const wxOVERRIDE;
    virtual wxString GetString(unsigned int n) const wxOVERRIDE;
    virtual void SetString(unsigned int n, const wxString& s) wxOVERRIDE;
    virtual int FindString(const wxString& s, bool bCase = false) const wxOVERRIDE;

    virtual bool IsSelected(int n) const wxOVERRIDE;
    virtual int GetSelection() const wxOVERRIDE;
    virtual int GetSelections(wxArrayInt& aSelections) const wxOVERRIDE;

    virtual void EnsureVisible(int n) wxOVERRIDE;

    virtual int GetTopItem() const wxOVERRIDE;

    virtual void Update() wxOVERRIDE;

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // implementation from now on

    virtual GtkWidget *GetConnectWidget() wxOVERRIDE;

    struct _GtkTreeView   *m_treeview;
    struct _GtkListStore  *m_liststore;

#if wxUSE_CHECKLISTBOX
    bool       m_hasCheckBoxes;
#endif // wxUSE_CHECKLISTBOX

    struct _wxTreeEntry* GTKGetEntry(unsigned pos) const;

    void GTKDisableEvents();
    void GTKEnableEvents();

    void GTKOnSelectionChanged();
    void GTKOnActivated(int item);

protected:
    virtual void DoClear() wxOVERRIDE;
    virtual void DoDeleteOneItem(unsigned int n) wxOVERRIDE;
    virtual wxSize DoGetBestSize() const wxOVERRIDE;
    virtual void DoApplyWidgetStyle(GtkRcStyle *style) wxOVERRIDE;
    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const wxOVERRIDE;

    virtual void DoSetSelection(int n, bool select) wxOVERRIDE;

    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type) wxOVERRIDE;
    virtual int DoInsertOneItem(const wxString& item, unsigned int pos) wxOVERRIDE;

    virtual void DoSetFirstItem(int n) wxOVERRIDE;
    virtual void DoSetItemClientData(unsigned int n, void* clientData) wxOVERRIDE;
    virtual void* DoGetItemClientData(unsigned int n) const wxOVERRIDE;
    virtual int DoListHitTest(const wxPoint& point) const wxOVERRIDE;

    // get the iterator for the given index, returns false if invalid
    bool GTKGetIteratorFor(unsigned pos, _GtkTreeIter *iter) const;

    // get the index for the given iterator, return wxNOT_FOUND on failure
    int GTKGetIndexFor(_GtkTreeIter& iter) const;

    // common part of DoSetFirstItem() and EnsureVisible()
    void DoScrollToCell(int n, float alignY, float alignX);

private:
    void Init(); //common construction

    wxDECLARE_DYNAMIC_CLASS(wxListBox);
};

#endif // _WX_GTK_LISTBOX_H_
