/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/dataview.h
// Purpose:     wxDataViewCtrl GTK+2 implementation header
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTKDATAVIEWCTRL_H_
#define _WX_GTKDATAVIEWCTRL_H_

#include "wx/list.h"

class WXDLLIMPEXP_FWD_ADV wxDataViewCtrlInternal;

struct _GtkTreePath;

// ---------------------------------------------------------
// wxDataViewColumn
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewColumn: public wxDataViewColumnBase
{
public:
    wxDataViewColumn( const wxString &title, wxDataViewRenderer *renderer,
                      unsigned int model_column, int width = wxDVC_DEFAULT_WIDTH,
                      wxAlignment align = wxALIGN_CENTER,
                      int flags = wxDATAVIEW_COL_RESIZABLE );
    wxDataViewColumn( const wxBitmap &bitmap, wxDataViewRenderer *renderer,
                      unsigned int model_column, int width = wxDVC_DEFAULT_WIDTH,
                      wxAlignment align = wxALIGN_CENTER,
                      int flags = wxDATAVIEW_COL_RESIZABLE );


    // setters:

    virtual void SetTitle( const wxString &title ) wxOVERRIDE;
    virtual void SetBitmap( const wxBitmap &bitmap ) wxOVERRIDE;

    virtual void SetOwner( wxDataViewCtrl *owner ) wxOVERRIDE;

    virtual void SetAlignment( wxAlignment align ) wxOVERRIDE;

    virtual void SetSortable( bool sortable ) wxOVERRIDE;
    virtual void SetSortOrder( bool ascending ) wxOVERRIDE;

    virtual void SetResizeable( bool resizable ) wxOVERRIDE;
    virtual void SetHidden( bool hidden ) wxOVERRIDE;

    virtual void SetMinWidth( int minWidth ) wxOVERRIDE;
    virtual void SetWidth( int width ) wxOVERRIDE;

    virtual void SetReorderable( bool reorderable ) wxOVERRIDE;

    virtual void SetFlags(int flags) wxOVERRIDE { SetIndividualFlags(flags); }

    // getters:

    virtual wxString GetTitle() const wxOVERRIDE;
    virtual wxAlignment GetAlignment() const wxOVERRIDE;

    virtual bool IsSortable() const wxOVERRIDE;
    virtual bool IsSortOrderAscending() const wxOVERRIDE;
    virtual bool IsSortKey() const wxOVERRIDE;

    virtual bool IsResizeable() const wxOVERRIDE;
    virtual bool IsHidden() const wxOVERRIDE;

    virtual int GetWidth() const wxOVERRIDE;
    virtual int GetMinWidth() const wxOVERRIDE;

    virtual bool IsReorderable() const wxOVERRIDE;

    virtual int GetFlags() const wxOVERRIDE { return GetFromIndividualFlags(); }

    // implementation
    GtkWidget* GetGtkHandle() const { return m_column; }

private:
    // holds the GTK handle
    GtkWidget   *m_column;

    // holds GTK handles for title/bitmap in the header
    GtkWidget   *m_image;
    GtkWidget   *m_label;

    // delayed connection to mouse events
    friend class wxDataViewCtrl;
    void OnInternalIdle();
    bool    m_isConnected;

    void Init(wxAlignment align, int flags, int width);
};

WX_DECLARE_LIST_WITH_DECL(wxDataViewColumn, wxDataViewColumnList,
                          class WXDLLIMPEXP_ADV);

// ---------------------------------------------------------
// wxDataViewCtrl
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewCtrl: public wxDataViewCtrlBase
{
public:
    wxDataViewCtrl()
    {
        Init();
    }

    wxDataViewCtrl( wxWindow *parent, wxWindowID id,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxDataViewCtrlNameStr )
    {
        Init();

        Create(parent, id, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize, long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxDataViewCtrlNameStr);

    virtual ~wxDataViewCtrl();

    virtual bool AssociateModel( wxDataViewModel *model ) wxOVERRIDE;

    virtual bool PrependColumn( wxDataViewColumn *col ) wxOVERRIDE;
    virtual bool AppendColumn( wxDataViewColumn *col ) wxOVERRIDE;
    virtual bool InsertColumn( unsigned int pos, wxDataViewColumn *col ) wxOVERRIDE;

    virtual unsigned int GetColumnCount() const wxOVERRIDE;
    virtual wxDataViewColumn* GetColumn( unsigned int pos ) const wxOVERRIDE;
    virtual bool DeleteColumn( wxDataViewColumn *column ) wxOVERRIDE;
    virtual bool ClearColumns() wxOVERRIDE;
    virtual int GetColumnPosition( const wxDataViewColumn *column ) const wxOVERRIDE;

    virtual wxDataViewColumn *GetSortingColumn() const wxOVERRIDE;

    virtual int GetSelectedItemsCount() const wxOVERRIDE;
    virtual int GetSelections( wxDataViewItemArray & sel ) const wxOVERRIDE;
    virtual void SetSelections( const wxDataViewItemArray & sel ) wxOVERRIDE;
    virtual void Select( const wxDataViewItem & item ) wxOVERRIDE;
    virtual void Unselect( const wxDataViewItem & item ) wxOVERRIDE;
    virtual bool IsSelected( const wxDataViewItem & item ) const wxOVERRIDE;
    virtual void SelectAll() wxOVERRIDE;
    virtual void UnselectAll() wxOVERRIDE;

    virtual void EnsureVisible( const wxDataViewItem& item,
                                const wxDataViewColumn *column = NULL ) wxOVERRIDE;
    virtual void HitTest( const wxPoint &point,
                          wxDataViewItem &item,
                          wxDataViewColumn *&column ) const wxOVERRIDE;
    virtual wxRect GetItemRect( const wxDataViewItem &item,
                                const wxDataViewColumn *column = NULL ) const wxOVERRIDE;

    virtual bool SetRowHeight( int rowHeight ) wxOVERRIDE;

    virtual void EditItem(const wxDataViewItem& item, const wxDataViewColumn *column) wxOVERRIDE;

    virtual void Expand( const wxDataViewItem & item ) wxOVERRIDE;
    virtual void Collapse( const wxDataViewItem & item ) wxOVERRIDE;
    virtual bool IsExpanded( const wxDataViewItem & item ) const wxOVERRIDE;

    virtual bool EnableDragSource( const wxDataFormat &format ) wxOVERRIDE;
    virtual bool EnableDropTarget( const wxDataFormat &format ) wxOVERRIDE;

    virtual wxDataViewColumn *GetCurrentColumn() const wxOVERRIDE;

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    wxWindow *GetMainWindow() { return (wxWindow*) this; }

    GtkWidget *GtkGetTreeView() { return m_treeview; }
    wxDataViewCtrlInternal* GtkGetInternal() { return m_internal; }

    // Convert GTK path to our item. Returned item may be invalid if get_iter()
    // failed.
    wxDataViewItem GTKPathToItem(struct _GtkTreePath *path) const;

    virtual void OnInternalIdle() wxOVERRIDE;

    int GTKGetUniformRowHeight() const { return m_uniformRowHeight; }

protected:
    virtual void DoSetExpanderColumn() wxOVERRIDE;
    virtual void DoSetIndent() wxOVERRIDE;

    virtual void DoApplyWidgetStyle(GtkRcStyle *style) wxOVERRIDE;

private:
    void Init();

    virtual wxDataViewItem DoGetCurrentItem() const wxOVERRIDE;
    virtual void DoSetCurrentItem(const wxDataViewItem& item) wxOVERRIDE;

    // Return wxDataViewColumn matching the given GtkTreeViewColumn.
    //
    // If the input argument is NULL, return NULL too. Otherwise we must find
    // the matching column and assert if we didn't.
    wxDataViewColumn* FromGTKColumn(GtkTreeViewColumn *gtk_col) const;

    friend class wxDataViewCtrlDCImpl;
    friend class wxDataViewColumn;
    friend class wxDataViewCtrlInternal;

    GtkWidget               *m_treeview;
    wxDataViewCtrlInternal  *m_internal;
    wxDataViewColumnList     m_cols;
    wxDataViewItem           m_ensureVisibleDefered;

    // By default this is set to -1 and the height of the rows is determined by
    // GetRect() methods of the renderers but this can be set to a positive
    // value to force the height of all rows to the given value.
    int m_uniformRowHeight;

    virtual void AddChildGTK(wxWindowGTK* child) wxOVERRIDE;
    void GtkEnableSelectionEvents();
    void GtkDisableSelectionEvents();

    wxDECLARE_DYNAMIC_CLASS(wxDataViewCtrl);
    wxDECLARE_NO_COPY_CLASS(wxDataViewCtrl);
};


#endif // _WX_GTKDATAVIEWCTRL_H_
