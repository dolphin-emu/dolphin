/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/datavgen.cpp
// Purpose:     wxDataViewCtrl generic implementation
// Author:      Robert Roebling
// Modified by: Francesco Montorsi, Guru Kathiresan, Bo Yang
// Id:          $Id: datavgen.cpp 67158 2011-03-09 09:44:03Z VZ $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_DATAVIEWCTRL

#include "wx/dataview.h"

#ifdef wxUSE_GENERICDATAVIEWCTRL

#ifndef WX_PRECOMP
    #ifdef __WXMSW__
        #include "wx/msw/private.h"
        #include "wx/msw/wrapwin.h"
        #include "wx/msw/wrapcctl.h" // include <commctrl.h> "properly"
    #endif
    #include "wx/sizer.h"
    #include "wx/log.h"
    #include "wx/dcclient.h"
    #include "wx/timer.h"
    #include "wx/settings.h"
    #include "wx/msgdlg.h"
    #include "wx/dcscreen.h"
    #include "wx/frame.h"
#endif

#include "wx/stockitem.h"
#include "wx/calctrl.h"
#include "wx/popupwin.h"
#include "wx/renderer.h"
#include "wx/dcbuffer.h"
#include "wx/icon.h"
#include "wx/list.h"
#include "wx/listimpl.cpp"
#include "wx/imaglist.h"
#include "wx/headerctrl.h"
#include "wx/dnd.h"
#include "wx/stopwatch.h"

//-----------------------------------------------------------------------------
// classes
//-----------------------------------------------------------------------------

class wxDataViewColumn;
class wxDataViewHeaderWindow;
class wxDataViewCtrl;

//-----------------------------------------------------------------------------
// classes
//-----------------------------------------------------------------------------

static const int SCROLL_UNIT_X = 15;

// the cell padding on the left/right
static const int PADDING_RIGHTLEFT = 3;

// the expander space margin
static const int EXPANDER_MARGIN = 4;

#ifdef __WXMSW__
static const int EXPANDER_OFFSET = 4;
#else
static const int EXPANDER_OFFSET = 1;
#endif

// Below is the compare stuff.
// For the generic implementation, both the leaf nodes and the nodes are sorted for
// fast search when needed
static wxDataViewModel* g_model;
static int g_column = -2;
static bool g_asending = true;

//-----------------------------------------------------------------------------
// wxDataViewColumn
//-----------------------------------------------------------------------------

void wxDataViewColumn::Init(int width, wxAlignment align, int flags)
{
    m_width = width;
    m_minWidth = 0;
    m_align = align;
    m_flags = flags;
    m_sort = false;
    m_sortAscending = true;
}

int wxDataViewColumn::GetWidth() const
{
    switch ( m_width )
    {
        case wxCOL_WIDTH_DEFAULT:
            return wxDVC_DEFAULT_WIDTH;

        case wxCOL_WIDTH_AUTOSIZE:
            wxCHECK_MSG( m_owner, wxDVC_DEFAULT_WIDTH, "no owner control" );
            return m_owner->GetBestColumnWidth(m_owner->GetColumnIndex(this));

        default:
            return m_width;
    }
}

void wxDataViewColumn::UpdateDisplay()
{
    if (m_owner)
    {
        int idx = m_owner->GetColumnIndex( this );
        m_owner->OnColumnChange( idx );
    }
}

//-----------------------------------------------------------------------------
// wxDataViewHeaderWindow
//-----------------------------------------------------------------------------

class wxDataViewHeaderWindow : public wxHeaderCtrl
{
public:
    wxDataViewHeaderWindow(wxDataViewCtrl *parent)
        : wxHeaderCtrl(parent)
    {
    }

    wxDataViewCtrl *GetOwner() const
        { return static_cast<wxDataViewCtrl *>(GetParent()); }

protected:
    // implement/override wxHeaderCtrl functions by forwarding them to the main
    // control
    virtual const wxHeaderColumn& GetColumn(unsigned int idx) const
    {
        return *(GetOwner()->GetColumn(idx));
    }

    virtual bool UpdateColumnWidthToFit(unsigned int idx, int widthTitle)
    {
        wxDataViewCtrl * const owner = GetOwner();

        int widthContents = owner->GetBestColumnWidth(idx);
        owner->GetColumn(idx)->SetWidth(wxMax(widthTitle, widthContents));
        owner->OnColumnChange(idx);

        return true;
    }

private:
    bool SendEvent(wxEventType type, unsigned int n)
    {
        wxDataViewCtrl * const owner = GetOwner();
        wxDataViewEvent event(type, owner->GetId());

        event.SetEventObject(owner);
        event.SetColumn(n);
        event.SetDataViewColumn(owner->GetColumn(n));
        event.SetModel(owner->GetModel());

        // for events created by wxDataViewHeaderWindow the
        // row / value fields are not valid
        return owner->GetEventHandler()->ProcessEvent(event);
    }

    void OnClick(wxHeaderCtrlEvent& event)
    {
        const unsigned idx = event.GetColumn();

        if ( SendEvent(wxEVT_COMMAND_DATAVIEW_COLUMN_HEADER_CLICK, idx) )
            return;

        // default handling for the column click is to sort by this column or
        // toggle its sort order
        wxDataViewCtrl * const owner = GetOwner();
        wxDataViewColumn * const col = owner->GetColumn(idx);
        if ( !col->IsSortable() )
        {
            // no default handling for non-sortable columns
            event.Skip();
            return;
        }

        if ( col->IsSortKey() )
        {
            // already using this column for sorting, just change the order
            col->ToggleSortOrder();
        }
        else // not using this column for sorting yet
        {
            // first unset the old sort column if any
            int oldSortKey = owner->GetSortingColumnIndex();
            if ( oldSortKey != wxNOT_FOUND )
            {
                owner->GetColumn(oldSortKey)->UnsetAsSortKey();
                owner->OnColumnChange(oldSortKey);
            }

            owner->SetSortingColumnIndex(idx);
            col->SetAsSortKey();
        }

        wxDataViewModel * const model = owner->GetModel();
        if ( model )
            model->Resort();

        owner->OnColumnChange(idx);
    }

    void OnRClick(wxHeaderCtrlEvent& event)
    {
        if ( !SendEvent(wxEVT_COMMAND_DATAVIEW_COLUMN_HEADER_RIGHT_CLICK,
                        event.GetColumn()) )
            event.Skip();
    }

    void OnResize(wxHeaderCtrlEvent& event)
    {
        wxDataViewCtrl * const owner = GetOwner();

        const unsigned col = event.GetColumn();
        owner->GetColumn(col)->SetWidth(event.GetWidth());
        GetOwner()->OnColumnChange(col);
    }

    void OnEndReorder(wxHeaderCtrlEvent& event)
    {
        wxDataViewCtrl * const owner = GetOwner();
        owner->ColumnMoved(owner->GetColumn(event.GetColumn()),
                        event.GetNewOrder());
    }

    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxDataViewHeaderWindow);
};

BEGIN_EVENT_TABLE(wxDataViewHeaderWindow, wxHeaderCtrl)
    EVT_HEADER_CLICK(wxID_ANY, wxDataViewHeaderWindow::OnClick)
    EVT_HEADER_RIGHT_CLICK(wxID_ANY, wxDataViewHeaderWindow::OnRClick)

    EVT_HEADER_RESIZING(wxID_ANY, wxDataViewHeaderWindow::OnResize)
    EVT_HEADER_END_RESIZE(wxID_ANY, wxDataViewHeaderWindow::OnResize)

    EVT_HEADER_END_REORDER(wxID_ANY, wxDataViewHeaderWindow::OnEndReorder)
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
// wxDataViewRenameTimer
//-----------------------------------------------------------------------------

class wxDataViewRenameTimer: public wxTimer
{
private:
    wxDataViewMainWindow *m_owner;

public:
    wxDataViewRenameTimer( wxDataViewMainWindow *owner );
    void Notify();
};

//-----------------------------------------------------------------------------
// wxDataViewTreeNode
//-----------------------------------------------------------------------------

class wxDataViewTreeNode;
WX_DEFINE_ARRAY( wxDataViewTreeNode *, wxDataViewTreeNodes );
WX_DEFINE_ARRAY( void* , wxDataViewTreeLeaves);

int LINKAGEMODE wxGenericTreeModelNodeCmp( wxDataViewTreeNode ** node1,
                                           wxDataViewTreeNode ** node2);
int LINKAGEMODE wxGenericTreeModelItemCmp( void ** id1, void ** id2);

class wxDataViewTreeNode
{
public:
    wxDataViewTreeNode( wxDataViewTreeNode * parent = NULL )
    {
        m_parent = parent;
        if (!parent)
            m_open = true;
        else
            m_open = false;
        m_hasChildren = false;
        m_subTreeCount  = 0;
    }

    ~wxDataViewTreeNode()
    {
    }

    wxDataViewTreeNode * GetParent() const { return m_parent; }
    void SetParent( wxDataViewTreeNode * parent ) { m_parent = parent; }
    wxDataViewTreeNodes &  GetNodes() { return m_nodes; }
    wxDataViewTreeLeaves & GetChildren() { return m_leaves; }

    void AddNode( wxDataViewTreeNode * node )
    {
        m_leaves.Add( node->GetItem().GetID() );
        if (g_column >= -1)
            m_leaves.Sort( &wxGenericTreeModelItemCmp );
        m_nodes.Add( node );
        if (g_column >= -1)
            m_nodes.Sort( &wxGenericTreeModelNodeCmp );
    }
    void AddLeaf( void * leaf )
    {
        m_leaves.Add( leaf );
        if (g_column >= -1)
            m_leaves.Sort( &wxGenericTreeModelItemCmp );
    }

    wxDataViewItem & GetItem() { return m_item; }
    const wxDataViewItem & GetItem() const { return m_item; }
    void SetItem( const wxDataViewItem & item ) { m_item = item; }

    unsigned int GetChildrenNumber() const { return m_leaves.GetCount(); }
    unsigned int GetNodeNumber() const { return m_nodes.GetCount(); }
    int GetIndentLevel() const
    {
        int ret = 0;
        const wxDataViewTreeNode * node = this;
        while( node->GetParent()->GetParent() != NULL )
        {
            node = node->GetParent();
            ret ++;
        }
        return ret;
    }

    bool IsOpen() const
    {
        return m_open;
    }

    void ToggleOpen()
    {
        int len = m_nodes.GetCount();
        int sum = 0;
        for ( int i = 0;i < len; i ++)
            sum += m_nodes[i]->GetSubTreeCount();

        sum += m_leaves.GetCount();
        if (m_open)
        {
            ChangeSubTreeCount(-sum);
            m_open = !m_open;
        }
        else
        {
            m_open = !m_open;
            ChangeSubTreeCount(sum);
        }
    }
    bool HasChildren() const { return m_hasChildren; }
    void SetHasChildren( bool has ){ m_hasChildren = has; }

    void SetSubTreeCount( int num ) { m_subTreeCount = num; }
    int GetSubTreeCount() const { return m_subTreeCount; }
    void ChangeSubTreeCount( int num )
    {
        if( !m_open )
            return;
        m_subTreeCount += num;
        if( m_parent )
            m_parent->ChangeSubTreeCount(num);
    }

    void Resort()
    {
        if (g_column >= -1)
        {
            m_nodes.Sort( &wxGenericTreeModelNodeCmp );
            int len = m_nodes.GetCount();
            for (int i = 0; i < len; i ++)
                m_nodes[i]->Resort();
            m_leaves.Sort( &wxGenericTreeModelItemCmp );
        }
    }

private:
    wxDataViewTreeNode  *m_parent;
    wxDataViewTreeNodes  m_nodes;
    wxDataViewTreeLeaves m_leaves;
    wxDataViewItem       m_item;
    bool                 m_open;
    bool                 m_hasChildren;
    int                  m_subTreeCount;
};

int LINKAGEMODE wxGenericTreeModelNodeCmp( wxDataViewTreeNode ** node1,
                                           wxDataViewTreeNode ** node2)
{
    return g_model->Compare( (*node1)->GetItem(), (*node2)->GetItem(), g_column, g_asending );
}

int LINKAGEMODE wxGenericTreeModelItemCmp( void ** id1, void ** id2)
{
    return g_model->Compare( *id1, *id2, g_column, g_asending );
}


//-----------------------------------------------------------------------------
// wxDataViewMainWindow
//-----------------------------------------------------------------------------

WX_DEFINE_SORTED_USER_EXPORTED_ARRAY_SIZE_T(unsigned int, wxDataViewSelection,
                                            WXDLLIMPEXP_ADV);
WX_DECLARE_LIST(wxDataViewItem, ItemList);
WX_DEFINE_LIST(ItemList)

class wxDataViewMainWindow: public wxWindow
{
public:
    wxDataViewMainWindow( wxDataViewCtrl *parent,
                            wxWindowID id,
                            const wxPoint &pos = wxDefaultPosition,
                            const wxSize &size = wxDefaultSize,
                            const wxString &name = wxT("wxdataviewctrlmainwindow") );
    virtual ~wxDataViewMainWindow();

    bool IsList() const { return GetOwner()->GetModel()->IsListModel(); }
    bool IsVirtualList() const { return m_root == NULL; }

    // notifications from wxDataViewModel
    bool ItemAdded( const wxDataViewItem &parent, const wxDataViewItem &item );
    bool ItemDeleted( const wxDataViewItem &parent, const wxDataViewItem &item );
    bool ItemChanged( const wxDataViewItem &item );
    bool ValueChanged( const wxDataViewItem &item, unsigned int model_column );
    bool Cleared();
    void Resort()
    {
        if (!IsVirtualList())
        {
            SortPrepare();
            m_root->Resort();
        }
        UpdateDisplay();
    }

    void SortPrepare()
    {
        g_model = GetOwner()->GetModel();
        wxDataViewColumn* col = GetOwner()->GetSortingColumn();
        if( !col )
        {
            if (g_model->HasDefaultCompare())
                g_column = -1;
            else
                g_column = -2;

            g_asending = true;
            return;
        }
        g_column = col->GetModelColumn();
        g_asending = col->IsSortOrderAscending();
    }

    void SetOwner( wxDataViewCtrl* owner ) { m_owner = owner; }
    wxDataViewCtrl *GetOwner() { return m_owner; }
    const wxDataViewCtrl *GetOwner() const { return m_owner; }

#if wxUSE_DRAG_AND_DROP
    wxBitmap CreateItemBitmap( unsigned int row, int &indent );
#endif // wxUSE_DRAG_AND_DROP
    void OnPaint( wxPaintEvent &event );
    void OnArrowChar(unsigned int newCurrent, const wxKeyEvent& event);
    void OnChar( wxKeyEvent &event );
    void OnMouse( wxMouseEvent &event );
    void OnSetFocus( wxFocusEvent &event );
    void OnKillFocus( wxFocusEvent &event );

    void UpdateDisplay();
    void RecalculateDisplay();
    void OnInternalIdle();

    void OnRenameTimer();

    void ScrollWindow( int dx, int dy, const wxRect *rect = NULL );
    void ScrollTo( int rows, int column );

    unsigned GetCurrentRow() const { return m_currentRow; }
    bool HasCurrentRow() { return m_currentRow != (unsigned int)-1; }
    void ChangeCurrentRow( unsigned int row );

    bool IsSingleSel() const { return !GetParent()->HasFlag(wxDV_MULTIPLE); }
    bool IsEmpty() { return GetRowCount() == 0; }

    int GetCountPerPage() const;
    int GetEndOfLastCol() const;
    unsigned int GetFirstVisibleRow() const;

    // I change this method to un const because in the tree view,
    // the displaying number of the tree are changing along with the
    // expanding/collapsing of the tree nodes
    unsigned int GetLastVisibleRow();
    unsigned int GetRowCount();

    wxDataViewItem GetSelection() const;
    wxDataViewSelection GetSelections(){ return m_selection; }
    void SetSelections( const wxDataViewSelection & sel )
        { m_selection = sel; UpdateDisplay(); }
    void Select( const wxArrayInt& aSelections );
    void SelectAllRows( bool on );
    void SelectRow( unsigned int row, bool on );
    void SelectRows( unsigned int from, unsigned int to, bool on );
    void ReverseRowSelection( unsigned int row );
    bool IsRowSelected( unsigned int row );
    void SendSelectionChangedEvent( const wxDataViewItem& item);

    void RefreshRow( unsigned int row );
    void RefreshRows( unsigned int from, unsigned int to );
    void RefreshRowsAfter( unsigned int firstRow );

    // returns the colour to be used for drawing the rules
    wxColour GetRuleColour() const
    {
        return wxSystemSettings::GetColour(wxSYS_COLOUR_3DLIGHT);
    }

    wxRect GetLineRect( unsigned int row ) const;

    int GetLineStart( unsigned int row ) const;  // row * m_lineHeight in fixed mode
    int GetLineHeight( unsigned int row ) const; // m_lineHeight in fixed mode
    int GetLineAt( unsigned int y ) const;       // y / m_lineHeight in fixed mode

    // Some useful functions for row and item mapping
    wxDataViewItem GetItemByRow( unsigned int row ) const;
    int GetRowByItem( const wxDataViewItem & item ) const;

    // Methods for building the mapping tree
    void BuildTree( wxDataViewModel  * model );
    void DestroyTree();
    void HitTest( const wxPoint & point, wxDataViewItem & item, wxDataViewColumn* &column );
    wxRect GetItemRect( const wxDataViewItem & item, const wxDataViewColumn* column );

    void Expand( unsigned int row );
    void Collapse( unsigned int row );
    bool IsExpanded( unsigned int row ) const;
    bool HasChildren( unsigned int row ) const;

#if wxUSE_DRAG_AND_DROP
    bool EnableDragSource( const wxDataFormat &format );
    bool EnableDropTarget( const wxDataFormat &format );

    void RemoveDropHint();
    wxDragResult OnDragOver( wxDataFormat format, wxCoord x, wxCoord y, wxDragResult def );
    bool OnDrop( wxDataFormat format, wxCoord x, wxCoord y );
    wxDragResult OnData( wxDataFormat format, wxCoord x, wxCoord y, wxDragResult def );
    void OnLeave();
#endif // wxUSE_DRAG_AND_DROP

private:
    wxDataViewTreeNode * GetTreeNodeByRow( unsigned int row ) const;
    // We did not need this temporarily
    // wxDataViewTreeNode * GetTreeNodeByItem( const wxDataViewItem & item );

    int RecalculateCount();

    wxDataViewEvent SendExpanderEvent( wxEventType type, const wxDataViewItem & item );

    wxDataViewTreeNode * FindNode( const wxDataViewItem & item );

private:
    wxDataViewCtrl             *m_owner;
    int                         m_lineHeight;
    bool                        m_dirty;

    wxDataViewColumn           *m_currentCol;
    unsigned int                m_currentRow;
    wxDataViewSelection         m_selection;

    wxDataViewRenameTimer      *m_renameTimer;
    bool                        m_lastOnSame;

    bool                        m_hasFocus;

#if wxUSE_DRAG_AND_DROP
    int                         m_dragCount;
    wxPoint                     m_dragStart;

    bool                        m_dragEnabled;
    wxDataFormat                m_dragFormat;

    bool                        m_dropEnabled;
    wxDataFormat                m_dropFormat;
    bool                        m_dropHint;
    unsigned int                m_dropHintLine;
#endif // wxUSE_DRAG_AND_DROP

    // for double click logic
    unsigned int m_lineLastClicked,
        m_lineBeforeLastClicked,
        m_lineSelectSingleOnUp;

    // the pen used to draw horiz/vertical rules
    wxPen m_penRule;

    // the pen used to draw the expander and the lines
    wxPen m_penExpander;

    // This is the tree structure of the model
    wxDataViewTreeNode * m_root;
    int m_count;

    // This is the tree node under the cursor
    wxDataViewTreeNode * m_underMouse;

private:
    DECLARE_DYNAMIC_CLASS(wxDataViewMainWindow)
    DECLARE_EVENT_TABLE()
};

// ---------------------------------------------------------
// wxGenericDataViewModelNotifier
// ---------------------------------------------------------

class wxGenericDataViewModelNotifier: public wxDataViewModelNotifier
{
public:
    wxGenericDataViewModelNotifier( wxDataViewMainWindow *mainWindow )
        { m_mainWindow = mainWindow; }

    virtual bool ItemAdded( const wxDataViewItem & parent, const wxDataViewItem & item )
        { return m_mainWindow->ItemAdded( parent , item ); }
    virtual bool ItemDeleted( const wxDataViewItem &parent, const wxDataViewItem &item )
        { return m_mainWindow->ItemDeleted( parent, item ); }
    virtual bool ItemChanged( const wxDataViewItem & item )
        { return m_mainWindow->ItemChanged(item);  }
    virtual bool ValueChanged( const wxDataViewItem & item , unsigned int col )
        { return m_mainWindow->ValueChanged( item, col ); }
    virtual bool Cleared()
        { return m_mainWindow->Cleared(); }
    virtual void Resort()
        { m_mainWindow->Resort(); }

    wxDataViewMainWindow    *m_mainWindow;
};

// ---------------------------------------------------------
// wxDataViewRenderer
// ---------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxDataViewRenderer, wxDataViewRendererBase)

wxDataViewRenderer::wxDataViewRenderer( const wxString &varianttype,
                                        wxDataViewCellMode mode,
                                        int align) :
    wxDataViewCustomRendererBase( varianttype, mode, align )
{
    m_align = align;
    m_mode = mode;
    m_ellipsizeMode = wxELLIPSIZE_MIDDLE;
    m_dc = NULL;
}

wxDataViewRenderer::~wxDataViewRenderer()
{
    delete m_dc;
}

wxDC *wxDataViewRenderer::GetDC()
{
    if (m_dc == NULL)
    {
        if (GetOwner() == NULL)
            return NULL;
        if (GetOwner()->GetOwner() == NULL)
            return NULL;
        m_dc = new wxClientDC( GetOwner()->GetOwner() );
    }

    return m_dc;
}

void wxDataViewRenderer::SetAlignment( int align )
{
    m_align=align;
}

int wxDataViewRenderer::GetAlignment() const
{
    return m_align;
}

// ---------------------------------------------------------
// wxDataViewCustomRenderer
// ---------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxDataViewCustomRenderer, wxDataViewRenderer)

wxDataViewCustomRenderer::wxDataViewCustomRenderer( const wxString &varianttype,
                        wxDataViewCellMode mode, int align ) :
    wxDataViewRenderer( varianttype, mode, align )
{
}

// ---------------------------------------------------------
// wxDataViewTextRenderer
// ---------------------------------------------------------

IMPLEMENT_CLASS(wxDataViewTextRenderer, wxDataViewRenderer)

wxDataViewTextRenderer::wxDataViewTextRenderer( const wxString &varianttype,
                                                wxDataViewCellMode mode, int align ) :
    wxDataViewRenderer( varianttype, mode, align )
{
}

bool wxDataViewTextRenderer::SetValue( const wxVariant &value )
{
    m_text = value.GetString();

    return true;
}

bool wxDataViewTextRenderer::GetValue( wxVariant& WXUNUSED(value) ) const
{
    return false;
}

bool wxDataViewTextRenderer::HasEditorCtrl() const
{
    return true;
}

wxControl* wxDataViewTextRenderer::CreateEditorCtrl( wxWindow *parent,
        wxRect labelRect, const wxVariant &value )
{
    wxTextCtrl* ctrl = new wxTextCtrl( parent, wxID_ANY, value,
                                       wxPoint(labelRect.x,labelRect.y),
                                       wxSize(labelRect.width,labelRect.height) );

    // select the text in the control an place the cursor at the end
    ctrl->SetInsertionPointEnd();
    ctrl->SelectAll();

    return ctrl;
}

bool wxDataViewTextRenderer::GetValueFromEditorCtrl( wxControl *editor, wxVariant &value )
{
    wxTextCtrl *text = (wxTextCtrl*) editor;
    value = text->GetValue();
    return true;
}

bool wxDataViewTextRenderer::Render(wxRect rect, wxDC *dc, int state)
{
    RenderText(m_text, 0, rect, dc, state);
    return true;
}

wxSize wxDataViewTextRenderer::GetSize() const
{
    if (!m_text.empty())
        return GetTextExtent(m_text);
    else
        return wxSize(wxDVC_DEFAULT_RENDERER_SIZE,wxDVC_DEFAULT_RENDERER_SIZE);
}

// ---------------------------------------------------------
// wxDataViewBitmapRenderer
// ---------------------------------------------------------

IMPLEMENT_CLASS(wxDataViewBitmapRenderer, wxDataViewRenderer)

wxDataViewBitmapRenderer::wxDataViewBitmapRenderer( const wxString &varianttype,
                                                    wxDataViewCellMode mode, int align ) :
    wxDataViewRenderer( varianttype, mode, align )
{
}

bool wxDataViewBitmapRenderer::SetValue( const wxVariant &value )
{
    if (value.GetType() == wxT("wxBitmap"))
        m_bitmap << value;
    if (value.GetType() == wxT("wxIcon"))
        m_icon << value;

    return true;
}

bool wxDataViewBitmapRenderer::GetValue( wxVariant& WXUNUSED(value) ) const
{
    return false;
}

bool wxDataViewBitmapRenderer::Render( wxRect cell, wxDC *dc, int WXUNUSED(state) )
{
    if (m_bitmap.Ok())
        dc->DrawBitmap( m_bitmap, cell.x, cell.y );
    else if (m_icon.Ok())
        dc->DrawIcon( m_icon, cell.x, cell.y );

    return true;
}

wxSize wxDataViewBitmapRenderer::GetSize() const
{
    if (m_bitmap.Ok())
        return wxSize( m_bitmap.GetWidth(), m_bitmap.GetHeight() );
    else if (m_icon.Ok())
        return wxSize( m_icon.GetWidth(), m_icon.GetHeight() );

    return wxSize(wxDVC_DEFAULT_RENDERER_SIZE,wxDVC_DEFAULT_RENDERER_SIZE);
}

// ---------------------------------------------------------
// wxDataViewToggleRenderer
// ---------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxDataViewToggleRenderer, wxDataViewRenderer)

wxDataViewToggleRenderer::wxDataViewToggleRenderer( const wxString &varianttype,
                        wxDataViewCellMode mode, int align ) :
    wxDataViewRenderer( varianttype, mode, align )
{
    m_toggle = false;
}

bool wxDataViewToggleRenderer::SetValue( const wxVariant &value )
{
    m_toggle = value.GetBool();

    return true;
}

bool wxDataViewToggleRenderer::GetValue( wxVariant &WXUNUSED(value) ) const
{
    return false;
}

bool wxDataViewToggleRenderer::Render( wxRect cell, wxDC *dc, int WXUNUSED(state) )
{
    int flags = 0;
    if (m_toggle)
        flags |= wxCONTROL_CHECKED;
    if (GetMode() != wxDATAVIEW_CELL_ACTIVATABLE ||
        GetEnabled() == false)
        flags |= wxCONTROL_DISABLED;

    // check boxes we draw must always have the same, standard size (if it's
    // bigger than the cell size the checkbox will be truncated because the
    // caller had set the clipping rectangle to prevent us from drawing outside
    // the cell)
    cell.SetSize(GetSize());

    wxRendererNative::Get().DrawCheckBox(
            GetOwner()->GetOwner(),
            *dc,
            cell,
            flags );

    return true;
}

bool wxDataViewToggleRenderer::WXOnLeftClick(const wxPoint& WXUNUSED(cursor),
                                             const wxRect& WXUNUSED(cell),
                                             wxDataViewModel *model,
                                             const wxDataViewItem& item,
                                             unsigned int col)
{
    if (model->IsEnabled(item, col))
    {
        model->ChangeValue(!m_toggle, item, col);
        return true;
    }

    return false;
}

wxSize wxDataViewToggleRenderer::GetSize() const
{
    // the window parameter is not used by GetCheckBoxSize() so it's
    // safe to pass NULL
    return wxRendererNative::Get().GetCheckBoxSize(NULL);
}

// ---------------------------------------------------------
// wxDataViewProgressRenderer
// ---------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxDataViewProgressRenderer, wxDataViewRenderer)

wxDataViewProgressRenderer::wxDataViewProgressRenderer( const wxString &label,
    const wxString &varianttype, wxDataViewCellMode mode, int align ) :
    wxDataViewRenderer( varianttype, mode, align )
{
    m_label = label;
    m_value = 0;
}

bool wxDataViewProgressRenderer::SetValue( const wxVariant &value )
{
    m_value = (long) value;

    if (m_value < 0) m_value = 0;
    if (m_value > 100) m_value = 100;

    return true;
}

bool wxDataViewProgressRenderer::GetValue( wxVariant &value ) const
{
    value = (long) m_value;
    return true;
}

bool
wxDataViewProgressRenderer::Render(wxRect rect, wxDC *dc, int WXUNUSED(state))
{
    // deflate the rect to leave a small border between bars in adjacent rows
    wxRect bar = rect.Deflate(0, 1);

    dc->SetBrush( *wxTRANSPARENT_BRUSH );
    dc->SetPen( *wxBLACK_PEN );
    dc->DrawRectangle( bar );

    bar.width = (int)(bar.width * m_value / 100.);
    dc->SetPen( *wxTRANSPARENT_PEN );

    const wxDataViewItemAttr& attr = GetAttr();
    dc->SetBrush( attr.HasColour() ? wxBrush(attr.GetColour())
                                  : *wxBLUE_BRUSH );
    dc->DrawRectangle( bar );

    return true;
}

wxSize wxDataViewProgressRenderer::GetSize() const
{
    return wxSize(40,12);
}

// ---------------------------------------------------------
// wxDataViewDateRenderer
// ---------------------------------------------------------

#define wxUSE_DATE_RENDERER_POPUP (wxUSE_CALENDARCTRL && wxUSE_POPUPWIN)

#if wxUSE_DATE_RENDERER_POPUP

class wxDataViewDateRendererPopupTransient: public wxPopupTransientWindow
{
public:
    wxDataViewDateRendererPopupTransient( wxWindow* parent, wxDateTime *value,
        wxDataViewModel *model, const wxDataViewItem & item, unsigned int col) :
        wxPopupTransientWindow( parent, wxBORDER_SIMPLE ),
        m_item( item )
    {
        m_model = model;
        m_col = col;
        m_cal = new wxCalendarCtrl( this, wxID_ANY, *value );
        wxBoxSizer *sizer = new wxBoxSizer( wxHORIZONTAL );
        sizer->Add( m_cal, 1, wxGROW );
        SetSizer( sizer );
        sizer->Fit( this );
    }

    void OnCalendar( wxCalendarEvent &event );

    wxCalendarCtrl      *m_cal;
    wxDataViewModel *m_model;
    unsigned int               m_col;
    const wxDataViewItem &   m_item;

protected:
    virtual void OnDismiss()
    {
    }

private:
    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(wxDataViewDateRendererPopupTransient,wxPopupTransientWindow)
    EVT_CALENDAR( wxID_ANY, wxDataViewDateRendererPopupTransient::OnCalendar )
END_EVENT_TABLE()

void wxDataViewDateRendererPopupTransient::OnCalendar( wxCalendarEvent &event )
{
    m_model->ChangeValue( event.GetDate(), m_item, m_col );
    DismissAndNotify();
}

#endif // wxUSE_DATE_RENDERER_POPUP

IMPLEMENT_ABSTRACT_CLASS(wxDataViewDateRenderer, wxDataViewRenderer)

wxDataViewDateRenderer::wxDataViewDateRenderer( const wxString &varianttype,
                        wxDataViewCellMode mode, int align ) :
    wxDataViewRenderer( varianttype, mode, align )
{
}

bool wxDataViewDateRenderer::SetValue( const wxVariant &value )
{
    m_date = value.GetDateTime();

    return true;
}

bool wxDataViewDateRenderer::GetValue( wxVariant &value ) const
{
    value = m_date;
    return true;
}

bool wxDataViewDateRenderer::Render( wxRect cell, wxDC *dc, int state )
{
    wxString tmp = m_date.FormatDate();
    RenderText( tmp, 0, cell, dc, state );
    return true;
}

wxSize wxDataViewDateRenderer::GetSize() const
{
    return GetTextExtent(m_date.FormatDate());
}

bool wxDataViewDateRenderer::WXOnActivate(const wxRect& WXUNUSED(cell),
                                          wxDataViewModel *model,
                                          const wxDataViewItem& item,
                                          unsigned int col)
{
    wxDateTime dtOld = m_date;

#if wxUSE_DATE_RENDERER_POPUP
    wxDataViewDateRendererPopupTransient *popup = new wxDataViewDateRendererPopupTransient(
        GetOwner()->GetOwner()->GetParent(), &dtOld, model, item, col);
    wxPoint pos = wxGetMousePosition();
    popup->Move( pos );
    popup->Layout();
    popup->Popup( popup->m_cal );
#else // !wxUSE_DATE_RENDERER_POPUP
    wxMessageBox(dtOld.Format());
#endif // wxUSE_DATE_RENDERER_POPUP/!wxUSE_DATE_RENDERER_POPUP

    return true;
}

// ---------------------------------------------------------
// wxDataViewIconTextRenderer
// ---------------------------------------------------------

IMPLEMENT_CLASS(wxDataViewIconTextRenderer, wxDataViewRenderer)

wxDataViewIconTextRenderer::wxDataViewIconTextRenderer(
const wxString &varianttype, wxDataViewCellMode mode, int align ) :
    wxDataViewRenderer( varianttype, mode, align )
{
    SetMode(mode);
    SetAlignment(align);
}

bool wxDataViewIconTextRenderer::SetValue( const wxVariant &value )
{
    m_value << value;
    return true;
}

bool wxDataViewIconTextRenderer::GetValue( wxVariant& WXUNUSED(value) ) const
{
    return false;
}

bool wxDataViewIconTextRenderer::Render(wxRect rect, wxDC *dc, int state)
{
    int xoffset = 0;

    const wxIcon& icon = m_value.GetIcon();
    if ( icon.IsOk() )
    {
        dc->DrawIcon(icon, rect.x, rect.y + (rect.height - icon.GetHeight())/2);
        xoffset = icon.GetWidth()+4;
    }

    RenderText(m_value.GetText(), xoffset, rect, dc, state);

    return true;
}

wxSize wxDataViewIconTextRenderer::GetSize() const
{
    if (!m_value.GetText().empty())
    {
        wxSize size = GetTextExtent(m_value.GetText());

        if (m_value.GetIcon().IsOk())
            size.x += m_value.GetIcon().GetWidth() + 4;
        return size;
    }
    return wxSize(80,20);
}

wxControl* wxDataViewIconTextRenderer::CreateEditorCtrl(wxWindow *parent, wxRect labelRect, const wxVariant& value)
{
    wxDataViewIconText iconText;
    iconText << value;

    wxString text = iconText.GetText();

    // adjust the label rect to take the width of the icon into account
    if (iconText.GetIcon().IsOk())
    {
        int w = iconText.GetIcon().GetWidth() + 4;
        labelRect.x += w;
        labelRect.width -= w;
    }

    wxTextCtrl* ctrl = new wxTextCtrl( parent, wxID_ANY, text,
                                       wxPoint(labelRect.x,labelRect.y),
                                       wxSize(labelRect.width,labelRect.height) );

    // select the text in the control an place the cursor at the end
    ctrl->SetInsertionPointEnd();
    ctrl->SelectAll();

    return ctrl;
}

bool wxDataViewIconTextRenderer::GetValueFromEditorCtrl( wxControl *editor, wxVariant& value )
{
    wxTextCtrl *text = (wxTextCtrl*) editor;

    wxDataViewIconText iconText(text->GetValue(), m_value.GetIcon());
    value << iconText;
    return true;
}

//-----------------------------------------------------------------------------
// wxDataViewDropTarget
//-----------------------------------------------------------------------------

#if wxUSE_DRAG_AND_DROP

class wxBitmapCanvas: public wxWindow
{
public:
    wxBitmapCanvas( wxWindow *parent, const wxBitmap &bitmap, const wxSize &size ) :
    wxWindow( parent, wxID_ANY, wxPoint(0,0), size )
    {
        m_bitmap = bitmap;
        Connect( wxEVT_PAINT, wxPaintEventHandler(wxBitmapCanvas::OnPaint) );
    }

    void OnPaint( wxPaintEvent &WXUNUSED(event) )
    {
        wxPaintDC dc(this);
        dc.DrawBitmap( m_bitmap, 0, 0);
    }

    wxBitmap m_bitmap;
};

class wxDataViewDropSource: public wxDropSource
{
public:
    wxDataViewDropSource( wxDataViewMainWindow *win, unsigned int row ) :
        wxDropSource( win )
    {
        m_win = win;
        m_row = row;
        m_hint = NULL;
    }

    ~wxDataViewDropSource()
    {
        delete m_hint;
    }

    virtual bool GiveFeedback( wxDragResult WXUNUSED(effect) )
    {
        wxPoint pos = wxGetMousePosition();

        if (!m_hint)
        {
            int liney = m_win->GetLineStart( m_row );
            int linex = 0;
            m_win->GetOwner()->CalcUnscrolledPosition( 0, liney, NULL, &liney );
            m_win->ClientToScreen( &linex, &liney );
            m_dist_x = pos.x - linex;
            m_dist_y = pos.y - liney;

            int indent = 0;
            wxBitmap ib = m_win->CreateItemBitmap( m_row, indent );
            m_dist_x -= indent;
            m_hint = new wxFrame( m_win->GetParent(), wxID_ANY, wxEmptyString,
                                        wxPoint(pos.x - m_dist_x, pos.y + 5 ),
                                        ib.GetSize(),
                                        wxFRAME_TOOL_WINDOW |
                                        wxFRAME_FLOAT_ON_PARENT |
                                        wxFRAME_NO_TASKBAR |
                                        wxNO_BORDER );
            new wxBitmapCanvas( m_hint, ib, ib.GetSize() );
            m_hint->Show();
        }
        else
        {
            m_hint->Move( pos.x - m_dist_x, pos.y + 5  );
            m_hint->SetTransparent( 128 );
        }

        return false;
    }

    wxDataViewMainWindow   *m_win;
    unsigned int            m_row;
    wxFrame                *m_hint;
    int m_dist_x,m_dist_y;
};


class wxDataViewDropTarget: public wxDropTarget
{
public:
    wxDataViewDropTarget( wxDataObject *obj, wxDataViewMainWindow *win ) :
        wxDropTarget( obj )
    {
        m_win = win;
    }

    virtual wxDragResult OnDragOver( wxCoord x, wxCoord y, wxDragResult def )
    {
        wxDataFormat format = GetMatchingPair();
        if (format == wxDF_INVALID)
            return wxDragNone;
        return m_win->OnDragOver( format, x, y, def);
    }

    virtual bool OnDrop( wxCoord x, wxCoord y )
    {
        wxDataFormat format = GetMatchingPair();
        if (format == wxDF_INVALID)
            return false;
        return m_win->OnDrop( format, x, y );
    }

    virtual wxDragResult OnData( wxCoord x, wxCoord y, wxDragResult def )
    {
        wxDataFormat format = GetMatchingPair();
        if (format == wxDF_INVALID)
            return wxDragNone;
        if (!GetData())
            return wxDragNone;
        return m_win->OnData( format, x, y, def );
    }

    virtual void OnLeave()
        { m_win->OnLeave(); }

    wxDataViewMainWindow   *m_win;
};

#endif // wxUSE_DRAG_AND_DROP

//-----------------------------------------------------------------------------
// wxDataViewRenameTimer
//-----------------------------------------------------------------------------

wxDataViewRenameTimer::wxDataViewRenameTimer( wxDataViewMainWindow *owner )
{
    m_owner = owner;
}

void wxDataViewRenameTimer::Notify()
{
    m_owner->OnRenameTimer();
}

//-----------------------------------------------------------------------------
// wxDataViewMainWindow
//-----------------------------------------------------------------------------

// The tree building helper, declared firstly
static void BuildTreeHelper( const wxDataViewModel * model,  const wxDataViewItem & item,
                             wxDataViewTreeNode * node);

int LINKAGEMODE wxDataViewSelectionCmp( unsigned int row1, unsigned int row2 )
{
    if (row1 > row2) return 1;
    if (row1 == row2) return 0;
    return -1;
}

IMPLEMENT_ABSTRACT_CLASS(wxDataViewMainWindow, wxWindow)

BEGIN_EVENT_TABLE(wxDataViewMainWindow,wxWindow)
    EVT_PAINT         (wxDataViewMainWindow::OnPaint)
    EVT_MOUSE_EVENTS  (wxDataViewMainWindow::OnMouse)
    EVT_SET_FOCUS     (wxDataViewMainWindow::OnSetFocus)
    EVT_KILL_FOCUS    (wxDataViewMainWindow::OnKillFocus)
    EVT_CHAR          (wxDataViewMainWindow::OnChar)
END_EVENT_TABLE()

wxDataViewMainWindow::wxDataViewMainWindow( wxDataViewCtrl *parent, wxWindowID id,
    const wxPoint &pos, const wxSize &size, const wxString &name ) :
    wxWindow( parent, id, pos, size, wxWANTS_CHARS|wxBORDER_NONE, name ),
    m_selection( wxDataViewSelectionCmp )

{
    SetOwner( parent );

    m_lastOnSame = false;
    m_renameTimer = new wxDataViewRenameTimer( this );

    // TODO: user better initial values/nothing selected
    m_currentCol = NULL;
    m_currentRow = 0;

    m_lineHeight = wxMax( 17, GetCharHeight() + 2 ); // 17 = mini icon height + 1

#if wxUSE_DRAG_AND_DROP
    m_dragCount = 0;
    m_dragStart = wxPoint(0,0);

    m_dragEnabled = false;
    m_dropEnabled = false;
    m_dropHint = false;
    m_dropHintLine = (unsigned int) -1;
#endif // wxUSE_DRAG_AND_DROP

    m_lineLastClicked = (unsigned int) -1;
    m_lineBeforeLastClicked = (unsigned int) -1;
    m_lineSelectSingleOnUp = (unsigned int) -1;

    m_hasFocus = false;

    SetBackgroundColour( *wxWHITE );

    SetBackgroundStyle(wxBG_STYLE_CUSTOM);

    m_penRule = wxPen(GetRuleColour());

    // compose a pen whichcan draw black lines
    // TODO: maybe there is something system colour to use
    m_penExpander = wxPen(wxColour(0,0,0));

    m_root = new wxDataViewTreeNode( NULL );
    m_root->SetHasChildren(true);

    // Make m_count = -1 will cause the class recaculate the real displaying number of rows.
    m_count = -1;
    m_underMouse = NULL;
    UpdateDisplay();
}

wxDataViewMainWindow::~wxDataViewMainWindow()
{
    DestroyTree();
    delete m_renameTimer;
}


#if wxUSE_DRAG_AND_DROP
bool wxDataViewMainWindow::EnableDragSource( const wxDataFormat &format )
{
    m_dragFormat = format;
    m_dragEnabled = format != wxDF_INVALID;

    return true;
}

bool wxDataViewMainWindow::EnableDropTarget( const wxDataFormat &format )
{
    m_dropFormat = format;
    m_dropEnabled = format != wxDF_INVALID;

    if (m_dropEnabled)
        SetDropTarget( new wxDataViewDropTarget( new wxCustomDataObject( format ), this ) );

    return true;
}

void wxDataViewMainWindow::RemoveDropHint()
{
    if (m_dropHint)
    {
            m_dropHint = false;
            RefreshRow( m_dropHintLine );
            m_dropHintLine = (unsigned int) -1;
    }
}

wxDragResult wxDataViewMainWindow::OnDragOver( wxDataFormat format, wxCoord x,
                                               wxCoord y, wxDragResult def )
{
    int xx = x;
    int yy = y;
    m_owner->CalcUnscrolledPosition( xx, yy, &xx, &yy );
    unsigned int row = GetLineAt( yy );

    if ((row >= GetRowCount()) || (xx > GetEndOfLastCol()))
    {
        RemoveDropHint();
        return wxDragNone;
    }

    wxDataViewItem item = GetItemByRow( row );

    wxDataViewModel *model = GetOwner()->GetModel();

    wxDataViewEvent event( wxEVT_COMMAND_DATAVIEW_ITEM_DROP_POSSIBLE, m_owner->GetId() );
    event.SetEventObject( m_owner );
    event.SetItem( item );
    event.SetModel( model );
    event.SetDataFormat( format );
    if (!m_owner->HandleWindowEvent( event ))
    {
        RemoveDropHint();
        return wxDragNone;
    }

    if (!event.IsAllowed())
    {
        RemoveDropHint();
        return wxDragNone;
    }


    if (m_dropHint && (row != m_dropHintLine))
        RefreshRow( m_dropHintLine );
    m_dropHint = true;
    m_dropHintLine = row;
    RefreshRow( row );

    return def;
}

bool wxDataViewMainWindow::OnDrop( wxDataFormat format, wxCoord x, wxCoord y )
{
    RemoveDropHint();

    int xx = x;
    int yy = y;
    m_owner->CalcUnscrolledPosition( xx, yy, &xx, &yy );
    unsigned int row = GetLineAt( yy );

    if ((row >= GetRowCount()) || (xx > GetEndOfLastCol()))
        return false;

    wxDataViewItem item = GetItemByRow( row );

    wxDataViewModel *model = GetOwner()->GetModel();

    wxDataViewEvent event( wxEVT_COMMAND_DATAVIEW_ITEM_DROP_POSSIBLE, m_owner->GetId() );
    event.SetEventObject( m_owner );
    event.SetItem( item );
    event.SetModel( model );
    event.SetDataFormat( format );
    if (!m_owner->HandleWindowEvent( event ))
        return false;

    if (!event.IsAllowed())
        return false;

    return true;
}

wxDragResult wxDataViewMainWindow::OnData( wxDataFormat format, wxCoord x, wxCoord y,
                                           wxDragResult def )
{
    int xx = x;
    int yy = y;
    m_owner->CalcUnscrolledPosition( xx, yy, &xx, &yy );
    unsigned int row = GetLineAt( yy );

    if ((row >= GetRowCount()) || (xx > GetEndOfLastCol()))
        return wxDragNone;

    wxDataViewItem item = GetItemByRow( row );

    wxDataViewModel *model = GetOwner()->GetModel();

    wxCustomDataObject *obj = (wxCustomDataObject *) GetDropTarget()->GetDataObject();

    wxDataViewEvent event( wxEVT_COMMAND_DATAVIEW_ITEM_DROP, m_owner->GetId() );
    event.SetEventObject( m_owner );
    event.SetItem( item );
    event.SetModel( model );
    event.SetDataFormat( format );
    event.SetDataSize( obj->GetSize() );
    event.SetDataBuffer( obj->GetData() );
    if (!m_owner->HandleWindowEvent( event ))
        return wxDragNone;

    if (!event.IsAllowed())
        return wxDragNone;

    return def;
}

void wxDataViewMainWindow::OnLeave()
{
    RemoveDropHint();
}

wxBitmap wxDataViewMainWindow::CreateItemBitmap( unsigned int row, int &indent )
{
    int height = GetLineHeight( row );
    int width = 0;
    unsigned int cols = GetOwner()->GetColumnCount();
    unsigned int col;
    for (col = 0; col < cols; col++)
    {
        wxDataViewColumn *column = GetOwner()->GetColumnAt(col);
        if (column->IsHidden())
            continue;      // skip it!
        width += column->GetWidth();
    }

    indent = 0;
    if (!IsList())
    {
        wxDataViewTreeNode *node = GetTreeNodeByRow(row);
        indent = GetOwner()->GetIndent() * node->GetIndentLevel();
        indent = indent + m_lineHeight;
            // try to use the m_lineHeight as the expander space

        if(!node->HasChildren())
            delete node;
    }
    width -= indent;

    wxBitmap bitmap( width, height );
    wxMemoryDC dc( bitmap );
    dc.SetFont( GetFont() );
    dc.SetPen( *wxBLACK_PEN );
    dc.SetBrush( *wxWHITE_BRUSH );
    dc.DrawRectangle( 0,0,width,height );

    wxDataViewModel *model = m_owner->GetModel();

    wxDataViewColumn *expander = GetOwner()->GetExpanderColumn();
    if (!expander)
    {
        // TODO-RTL: last column for RTL support
        expander = GetOwner()->GetColumnAt( 0 );
        GetOwner()->SetExpanderColumn(expander);
    }


    int x = 0;
    for (col = 0; col < cols; col++)
    {
        wxDataViewColumn *column = GetOwner()->GetColumnAt( col );
        wxDataViewRenderer *cell = column->GetRenderer();

        if (column->IsHidden())
            continue;       // skip it!

        width = column->GetWidth();

        if (column == expander)
            width -= indent;

        wxDataViewItem item = GetItemByRow( row );
        cell->PrepareForItem(model, item, column->GetModelColumn());

        wxRect item_rect(x, 0, width, height);
        item_rect.Deflate(PADDING_RIGHTLEFT, 0);

        // dc.SetClippingRegion( item_rect );
        cell->WXCallRender(item_rect, &dc, 0);
        // dc.DestroyClippingRegion();

        x += width;
    }

    return bitmap;
}

#endif // wxUSE_DRAG_AND_DROP


void wxDataViewMainWindow::OnPaint( wxPaintEvent &WXUNUSED(event) )
{
    wxDataViewModel *model = GetOwner()->GetModel();
    wxAutoBufferedPaintDC dc( this );

#ifdef __WXMSW__
    dc.SetBrush(GetOwner()->GetBackgroundColour());
    dc.SetPen( *wxTRANSPARENT_PEN );
    dc.DrawRectangle(GetClientSize());
#endif

    // prepare the DC
    GetOwner()->PrepareDC( dc );
    dc.SetFont( GetFont() );

    wxRect update = GetUpdateRegion().GetBox();
    m_owner->CalcUnscrolledPosition( update.x, update.y, &update.x, &update.y );

    // compute which items needs to be redrawn
    unsigned int item_start = GetLineAt( wxMax(0,update.y) );
    unsigned int item_count =
        wxMin( (int)(  GetLineAt( wxMax(0,update.y+update.height) ) - item_start + 1),
            (int)(GetRowCount( ) - item_start));
    unsigned int item_last = item_start + item_count;

    // Send the event to wxDataViewCtrl itself.
    wxWindow * const parent = GetParent();
    wxDataViewEvent cache_event(wxEVT_COMMAND_DATAVIEW_CACHE_HINT, parent->GetId());
    cache_event.SetEventObject(parent);
    cache_event.SetCache(item_start, item_last - 1);
    parent->ProcessWindowEvent(cache_event);

    // compute which columns needs to be redrawn
    unsigned int cols = GetOwner()->GetColumnCount();
    if ( !cols )
    {
        // we assume that we have at least one column below and painting an
        // empty control is unnecessary anyhow
        return;
    }

    unsigned int col_start = 0;
    unsigned int x_start;
    for (x_start = 0; col_start < cols; col_start++)
    {
        wxDataViewColumn *col = GetOwner()->GetColumnAt(col_start);
        if (col->IsHidden())
            continue;      // skip it!

        unsigned int w = col->GetWidth();
        if (x_start+w >= (unsigned int)update.x)
            break;

        x_start += w;
    }

    unsigned int col_last = col_start;
    unsigned int x_last = x_start;
    for (; col_last < cols; col_last++)
    {
        wxDataViewColumn *col = GetOwner()->GetColumnAt(col_last);
        if (col->IsHidden())
            continue;      // skip it!

        if (x_last > (unsigned int)update.GetRight())
            break;

        x_last += col->GetWidth();
    }

    // Draw horizontal rules if required
    if ( m_owner->HasFlag(wxDV_HORIZ_RULES) )
    {
        dc.SetPen(m_penRule);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);

        for (unsigned int i = item_start; i <= item_last; i++)
        {
            int y = GetLineStart( i );
            dc.DrawLine(x_start, y, x_last, y);
        }
    }

    // Draw vertical rules if required
    if ( m_owner->HasFlag(wxDV_VERT_RULES) )
    {
        dc.SetPen(m_penRule);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);

        // NB: Vertical rules are drawn in the last pixel of a column so that
        //     they align perfectly with native MSW wxHeaderCtrl as well as for
        //     consistency with MSW native list control. There's no vertical
        //     rule at the most-left side of the control.

        int x = x_start - 1;
        for (unsigned int i = col_start; i < col_last; i++)
        {
            wxDataViewColumn *col = GetOwner()->GetColumnAt(i);
            if (col->IsHidden())
                continue;       // skip it

            x += col->GetWidth();

            dc.DrawLine(x, GetLineStart( item_start ),
                        x, GetLineStart( item_last ) );
        }
    }

    // redraw the background for the items which are selected/current
    for (unsigned int item = item_start; item < item_last; item++)
    {
        bool selected = m_selection.Index( item ) != wxNOT_FOUND;
        if (selected || item == m_currentRow)
        {
            int flags = selected ? (int)wxCONTROL_SELECTED : 0;
            if (item == m_currentRow)
                flags |= wxCONTROL_CURRENT;
            if (m_hasFocus)
                flags |= wxCONTROL_FOCUSED;

            wxRect rect( x_start, GetLineStart( item ),
                         x_last - x_start, GetLineHeight( item ) );
            wxRendererNative::Get().DrawItemSelectionRect
                                (
                                    this,
                                    dc,
                                    rect,
                                    flags
                                );
        }
    }

#if wxUSE_DRAG_AND_DROP
    if (m_dropHint)
    {
        wxRect rect( x_start, GetLineStart( m_dropHintLine ),
                     x_last - x_start, GetLineHeight( m_dropHintLine ) );
        dc.SetPen( *wxBLACK_PEN );
        dc.SetBrush( *wxTRANSPARENT_BRUSH );
        dc.DrawRectangle( rect );
    }
#endif // wxUSE_DRAG_AND_DROP

    wxDataViewColumn *expander = GetOwner()->GetExpanderColumn();
    if (!expander)
    {
        // TODO-RTL: last column for RTL support
        expander = GetOwner()->GetColumnAt( 0 );
        GetOwner()->SetExpanderColumn(expander);
    }

    // redraw all cells for all rows which must be repainted and all columns
    wxRect cell_rect;
    cell_rect.x = x_start;
    for (unsigned int i = col_start; i < col_last; i++)
    {
        wxDataViewColumn *col = GetOwner()->GetColumnAt( i );
        wxDataViewRenderer *cell = col->GetRenderer();
        cell_rect.width = col->GetWidth();

        if ( col->IsHidden() || cell_rect.width <= 0 )
            continue;       // skip it!

        for (unsigned int item = item_start; item < item_last; item++)
        {
            // get the cell value and set it into the renderer
            wxDataViewTreeNode *node = NULL;
            wxDataViewItem dataitem;

            if (!IsVirtualList())
            {
                node = GetTreeNodeByRow(item);
                if( node == NULL )
                    continue;

                dataitem = node->GetItem();

                if ((i > 0) && model->IsContainer(dataitem) &&
                    !model->HasContainerColumns(dataitem))
                    continue;
            }
            else
            {
                dataitem = wxDataViewItem( wxUIntToPtr(item+1) );
            }

            cell->PrepareForItem(model, dataitem, col->GetModelColumn());

            // update cell_rect
            cell_rect.y = GetLineStart( item );
            cell_rect.height = GetLineHeight( item );

            // deal with the expander
            int indent = 0;
            if ((!IsList()) && (col == expander))
            {
                // Calculate the indent first
                indent = GetOwner()->GetIndent() * node->GetIndentLevel();

                // we reserve m_lineHeight of horizontal space for the expander
                // but leave EXPANDER_MARGIN around the expander itself
                int exp_x = cell_rect.x + indent + EXPANDER_MARGIN;

                indent += m_lineHeight;

                // draw expander if needed and visible
                if ( node->HasChildren() && exp_x < cell_rect.GetRight() )
                {
                    dc.SetPen( m_penExpander );
                    dc.SetBrush( wxNullBrush );

                    int exp_size = m_lineHeight - 2*EXPANDER_MARGIN;
                    int exp_y = cell_rect.y + (cell_rect.height - exp_size)/2
                                   + EXPANDER_MARGIN - EXPANDER_OFFSET;

                    const wxRect rect(exp_x, exp_y, exp_size, exp_size);

                    int flag = 0;
                    if ( m_underMouse == node )
                        flag |= wxCONTROL_CURRENT;
                    if ( node->IsOpen() )
                        flag |= wxCONTROL_EXPANDED;

                    // ensure that we don't overflow the cell (which might
                    // happen if the column is very narrow)
                    wxDCClipper clip(dc, cell_rect);

                    wxRendererNative::Get().DrawTreeItemButton( this, dc, rect, flag);
                }

                // force the expander column to left-center align
                cell->SetAlignment( wxALIGN_CENTER_VERTICAL );
            }
            if (node && !node->HasChildren())
            {
                // Yes, if the node does not have any child, it must be a leaf which
                // mean that it is a temporarily created by GetTreeNodeByRow
                wxDELETE(node);
            }

            wxRect item_rect = cell_rect;
            item_rect.Deflate(PADDING_RIGHTLEFT, 0);

            // account for the tree indent (harmless if we're not indented)
            item_rect.x += indent;
            item_rect.width -= indent;

            if ( item_rect.width <= 0 )
                continue;

            int state = 0;
            if (m_hasFocus && (m_selection.Index(item) != wxNOT_FOUND))
                state |= wxDATAVIEW_CELL_SELECTED;

            // TODO: it would be much more efficient to create a clipping
            //       region for the entire column being rendered (in the OnPaint
            //       of wxDataViewMainWindow) instead of a single clip region for
            //       each cell. However it would mean that each renderer should
            //       respect the given wxRect's top & bottom coords, eventually
            //       violating only the left & right coords - however the user can
            //       make its own renderer and thus we cannot be sure of that.
            wxDCClipper clip(dc, item_rect);

            cell->WXCallRender(item_rect, &dc, state);
        }

        cell_rect.x += cell_rect.width;
    }
}

void wxDataViewMainWindow::OnRenameTimer()
{
    // We have to call this here because changes may just have
    // been made and no screen update taken place.
    if ( m_dirty )
    {
        // TODO: use wxTheApp->SafeYieldFor(NULL, wxEVT_CATEGORY_UI) instead
        //       (needs to be tested!)
        wxSafeYield();
    }

    wxDataViewItem item = GetItemByRow( m_currentRow );

    wxRect labelRect = GetItemRect(item, m_currentCol);

    m_currentCol->GetRenderer()->StartEditing( item, labelRect );
}

//-----------------------------------------------------------------------------
// Helper class for do operation on the tree node
//-----------------------------------------------------------------------------
class DoJob
{
public:
    DoJob() { }
    virtual ~DoJob() { }

    // The return value control how the tree-walker tranverse the tree
    // 0: Job done, stop tranverse and return
    // 1: Ignore the current node's subtree and continue
    // 2: Job not done, continue
    enum  { OK = 0 , IGR = 1, CONT = 2 };
    virtual int operator() ( wxDataViewTreeNode * node ) = 0;
    virtual int operator() ( void * n ) = 0;
};

bool Walker( wxDataViewTreeNode * node, DoJob & func )
{
    if( node==NULL )
        return false;

    switch( func( node ) )
    {
        case DoJob::OK :
            return true;
        case DoJob::IGR:
            return false;
        case DoJob::CONT:
        default:
            ;
    }

    const wxDataViewTreeNodes& nodes = node->GetNodes();
    const wxDataViewTreeLeaves& leaves = node->GetChildren();

    int len_nodes = nodes.GetCount();
    int len = leaves.GetCount();
    int i = 0, nodes_i = 0;

    for(; i < len; i ++ )
    {
        void * n = leaves[i];
        if( nodes_i < len_nodes && n == nodes[nodes_i]->GetItem().GetID() )
        {
            wxDataViewTreeNode * nd = nodes[nodes_i];
            nodes_i++;

            if( Walker( nd , func ) )
                return true;

        }
        else
            switch( func( n ) )
            {
                case DoJob::OK :
                    return true;
                case DoJob::IGR:
                    continue;
                case DoJob::CONT:
                default:
                ;
            }
    }
    return false;
}

bool wxDataViewMainWindow::ItemAdded(const wxDataViewItem & parent, const wxDataViewItem & item)
{
    GetOwner()->InvalidateColBestWidths();

    if (IsVirtualList())
    {
        wxDataViewVirtualListModel *list_model =
            (wxDataViewVirtualListModel*) GetOwner()->GetModel();
        m_count = list_model->GetCount();
        UpdateDisplay();
        return true;
    }

    SortPrepare();

    wxDataViewTreeNode * node;
    node = FindNode(parent);

    if( node == NULL )
        return false;

    node->SetHasChildren( true );

    if( g_model->IsContainer( item ) )
    {
        wxDataViewTreeNode * newnode = new wxDataViewTreeNode( node );
        newnode->SetItem(item);
        newnode->SetHasChildren( true );
        node->AddNode( newnode);
    }
    else
        node->AddLeaf( item.GetID() );

    node->ChangeSubTreeCount(1);

    m_count = -1;
    UpdateDisplay();

    return true;
}

static void DestroyTreeHelper( wxDataViewTreeNode * node);

bool wxDataViewMainWindow::ItemDeleted(const wxDataViewItem& parent,
                                       const wxDataViewItem& item)
{
    GetOwner()->InvalidateColBestWidths();

    if (IsVirtualList())
    {
        wxDataViewVirtualListModel *list_model =
            (wxDataViewVirtualListModel*) GetOwner()->GetModel();
        m_count = list_model->GetCount();

        if( m_currentRow > GetRowCount() )
            m_currentRow = m_count - 1;

        // TODO: why empty the entire selection?
        m_selection.Empty();

        UpdateDisplay();

        return true;
    }

    wxDataViewTreeNode * node = FindNode(parent);

    // Notice that it is possible that the item being deleted is not in the
    // tree at all, for example we could be deleting a never shown (because
    // collapsed) item in a tree model. So it's not an error if we don't know
    // about this item, just return without doing anything then.
    if ( !node || node->GetChildren().Index(item.GetID()) == wxNOT_FOUND )
        return false;

    int sub = -1;
    node->GetChildren().Remove( item.GetID() );
    // Manipolate selection
    if( m_selection.GetCount() > 1 )
    {
        m_selection.Empty();
    }
    bool isContainer = false;
    wxDataViewTreeNodes nds = node->GetNodes();
    for (size_t i = 0; i < nds.GetCount(); i ++)
    {
        if (nds[i]->GetItem() == item)
        {
            isContainer = true;
            break;
        }
    }
    if( isContainer )
    {
        wxDataViewTreeNode * n = NULL;
        wxDataViewTreeNodes nodes = node->GetNodes();
        int len = nodes.GetCount();
        for( int i = 0; i < len; i ++)
        {
            if( nodes[i]->GetItem() == item )
            {
                n = nodes[i];
                break;
            }
        }

        wxCHECK_MSG( n != NULL, false, "item not found" );

        node->GetNodes().Remove( n );
        sub -= n->GetSubTreeCount();
        ::DestroyTreeHelper(n);
    }
    // Make the row number invalid and get a new valid one when user call GetRowCount
    m_count = -1;
    node->ChangeSubTreeCount(sub);

    // Change the current row to the last row if the current exceed the max row number
    if( m_currentRow > GetRowCount() )
        m_currentRow = m_count - 1;

    UpdateDisplay();

    return true;
}

bool wxDataViewMainWindow::ItemChanged(const wxDataViewItem & item)
{
    GetOwner()->InvalidateColBestWidths();

    SortPrepare();
    g_model->Resort();

    // Send event
    wxWindow *parent = GetParent();
    wxDataViewEvent le(wxEVT_COMMAND_DATAVIEW_ITEM_VALUE_CHANGED, parent->GetId());
    le.SetEventObject(parent);
    le.SetModel(GetOwner()->GetModel());
    le.SetItem(item);
    parent->GetEventHandler()->ProcessEvent(le);

    return true;
}

bool wxDataViewMainWindow::ValueChanged( const wxDataViewItem & item, unsigned int model_column )
{
    int view_column = -1;
    unsigned int n_col = m_owner->GetColumnCount();
    for (unsigned i = 0; i < n_col; i++)
    {
        wxDataViewColumn *column = m_owner->GetColumn( i );
        if (column->GetModelColumn() == model_column)
        {
            view_column = (int) i;
            break;
        }
    }
    if (view_column == -1)
        return false;

    GetOwner()->InvalidateColBestWidth(view_column);

    // NOTE: to be valid, we cannot use e.g. INT_MAX - 1
/*#define MAX_VIRTUAL_WIDTH       100000

    wxRect rect( 0, row*m_lineHeight, MAX_VIRTUAL_WIDTH, m_lineHeight );
    m_owner->CalcScrolledPosition( rect.x, rect.y, &rect.x, &rect.y );
    Refresh( true, &rect );

    return true;
*/
    SortPrepare();
    g_model->Resort();

    // Send event
    wxWindow *parent = GetParent();
    wxDataViewEvent le(wxEVT_COMMAND_DATAVIEW_ITEM_VALUE_CHANGED, parent->GetId());
    le.SetEventObject(parent);
    le.SetModel(GetOwner()->GetModel());
    le.SetItem(item);
    le.SetColumn(view_column);
    le.SetDataViewColumn(GetOwner()->GetColumn(view_column));
    parent->GetEventHandler()->ProcessEvent(le);

    return true;
}

bool wxDataViewMainWindow::Cleared()
{
    GetOwner()->InvalidateColBestWidths();

    DestroyTree();
    m_selection.Clear();

    SortPrepare();
    BuildTree( GetOwner()->GetModel() );

    UpdateDisplay();

    return true;
}

void wxDataViewMainWindow::UpdateDisplay()
{
    m_dirty = true;
    m_underMouse = NULL;
}

void wxDataViewMainWindow::OnInternalIdle()
{
    wxWindow::OnInternalIdle();

    if (m_dirty)
    {
        RecalculateDisplay();
        m_dirty = false;
    }
}

void wxDataViewMainWindow::RecalculateDisplay()
{
    wxDataViewModel *model = GetOwner()->GetModel();
    if (!model)
    {
        Refresh();
        return;
    }

    int width = GetEndOfLastCol();
    int height = GetLineStart( GetRowCount() );

    SetVirtualSize( width, height );
    GetOwner()->SetScrollRate( 10, m_lineHeight );

    Refresh();
}

void wxDataViewMainWindow::ScrollWindow( int dx, int dy, const wxRect *rect )
{
    m_underMouse = NULL;

    wxWindow::ScrollWindow( dx, dy, rect );

    if (GetOwner()->m_headerArea)
        GetOwner()->m_headerArea->ScrollWindow( dx, 0 );
}

void wxDataViewMainWindow::ScrollTo( int rows, int column )
{
    m_underMouse = NULL;

    int x, y;
    m_owner->GetScrollPixelsPerUnit( &x, &y );
    int sy = GetLineStart( rows )/y;
    int sx = 0;
    if( column != -1 )
    {
        wxRect rect = GetClientRect();
        int colnum = 0;
        int x_start, w = 0;
        int xx, yy, xe;
        m_owner->CalcUnscrolledPosition( rect.x, rect.y, &xx, &yy );
        for (x_start = 0; colnum < column; colnum++)
        {
            wxDataViewColumn *col = GetOwner()->GetColumnAt(colnum);
            if (col->IsHidden())
                continue;      // skip it!

            w = col->GetWidth();
            x_start += w;
        }

        int x_end = x_start + w;
        xe = xx + rect.width;
        if( x_end > xe )
        {
            sx = ( xx + x_end - xe )/x;
        }
        if( x_start < xx )
        {
            sx = x_start/x;
        }
    }
    m_owner->Scroll( sx, sy );
}

int wxDataViewMainWindow::GetCountPerPage() const
{
    wxSize size = GetClientSize();
    return size.y / m_lineHeight;
}

int wxDataViewMainWindow::GetEndOfLastCol() const
{
    int width = 0;
    unsigned int i;
    for (i = 0; i < GetOwner()->GetColumnCount(); i++)
    {
        const wxDataViewColumn *c =
            const_cast<wxDataViewCtrl*>(GetOwner())->GetColumnAt( i );

        if (!c->IsHidden())
            width += c->GetWidth();
    }
    return width;
}

unsigned int wxDataViewMainWindow::GetFirstVisibleRow() const
{
    int x = 0;
    int y = 0;
    m_owner->CalcUnscrolledPosition( x, y, &x, &y );

    return GetLineAt( y );
}

unsigned int wxDataViewMainWindow::GetLastVisibleRow()
{
    wxSize client_size = GetClientSize();
    m_owner->CalcUnscrolledPosition( client_size.x, client_size.y,
                                    &client_size.x, &client_size.y );

    // we should deal with the pixel here
    unsigned int row = GetLineAt(client_size.y) - 1;

    return wxMin( GetRowCount()-1, row );
}

unsigned int wxDataViewMainWindow::GetRowCount()
{
    if ( m_count == -1 )
    {
        m_count = RecalculateCount();
        UpdateDisplay();
    }
    return m_count;
}

void wxDataViewMainWindow::ChangeCurrentRow( unsigned int row )
{
    m_currentRow = row;

    // send event
}

void wxDataViewMainWindow::SelectAllRows( bool on )
{
    if (IsEmpty())
        return;

    if (on)
    {
        m_selection.Clear();
        for (unsigned int i = 0; i < GetRowCount(); i++)
            m_selection.Add( i );
        Refresh();
    }
    else
    {
        unsigned int first_visible = GetFirstVisibleRow();
        unsigned int last_visible = GetLastVisibleRow();
        unsigned int i;
        for (i = 0; i < m_selection.GetCount(); i++)
        {
            unsigned int row = m_selection[i];
            if ((row >= first_visible) && (row <= last_visible))
                RefreshRow( row );
        }
        m_selection.Clear();
    }
}

void wxDataViewMainWindow::SelectRow( unsigned int row, bool on )
{
    if (m_selection.Index( row ) == wxNOT_FOUND)
    {
        if (on)
        {
            m_selection.Add( row );
            RefreshRow( row );
        }
    }
    else
    {
        if (!on)
        {
            m_selection.Remove( row );
            RefreshRow( row );
        }
    }
}

void wxDataViewMainWindow::SelectRows( unsigned int from, unsigned int to, bool on )
{
    if (from > to)
    {
        unsigned int tmp = from;
        from = to;
        to = tmp;
    }

    unsigned int i;
    for (i = from; i <= to; i++)
    {
        if (m_selection.Index( i ) == wxNOT_FOUND)
        {
            if (on)
                m_selection.Add( i );
        }
        else
        {
            if (!on)
                m_selection.Remove( i );
        }
    }
    RefreshRows( from, to );
}

void wxDataViewMainWindow::Select( const wxArrayInt& aSelections )
{
    for (size_t i=0; i < aSelections.GetCount(); i++)
    {
        int n = aSelections[i];

        m_selection.Add( n );
        RefreshRow( n );
    }
}

void wxDataViewMainWindow::ReverseRowSelection( unsigned int row )
{
    if (m_selection.Index( row ) == wxNOT_FOUND)
        m_selection.Add( row );
    else
        m_selection.Remove( row );
    RefreshRow( row );
}

bool wxDataViewMainWindow::IsRowSelected( unsigned int row )
{
    return (m_selection.Index( row ) != wxNOT_FOUND);
}

void wxDataViewMainWindow::SendSelectionChangedEvent( const wxDataViewItem& item)
{
    wxWindow *parent = GetParent();
    wxDataViewEvent le(wxEVT_COMMAND_DATAVIEW_SELECTION_CHANGED, parent->GetId());

    le.SetEventObject(parent);
    le.SetModel(GetOwner()->GetModel());
    le.SetItem( item );

    parent->GetEventHandler()->ProcessEvent(le);
}

void wxDataViewMainWindow::RefreshRow( unsigned int row )
{
    wxRect rect( 0, GetLineStart( row ), GetEndOfLastCol(), GetLineHeight( row ) );
    m_owner->CalcScrolledPosition( rect.x, rect.y, &rect.x, &rect.y );

    wxSize client_size = GetClientSize();
    wxRect client_rect( 0, 0, client_size.x, client_size.y );
    wxRect intersect_rect = client_rect.Intersect( rect );
    if (intersect_rect.width > 0)
        Refresh( true, &intersect_rect );
}

void wxDataViewMainWindow::RefreshRows( unsigned int from, unsigned int to )
{
    if (from > to)
    {
        unsigned int tmp = to;
        to = from;
        from = tmp;
    }

    wxRect rect( 0, GetLineStart( from ), GetEndOfLastCol(), GetLineStart( (to-from+1) ) );
    m_owner->CalcScrolledPosition( rect.x, rect.y, &rect.x, &rect.y );

    wxSize client_size = GetClientSize();
    wxRect client_rect( 0, 0, client_size.x, client_size.y );
    wxRect intersect_rect = client_rect.Intersect( rect );
    if (intersect_rect.width > 0)
        Refresh( true, &intersect_rect );
}

void wxDataViewMainWindow::RefreshRowsAfter( unsigned int firstRow )
{
    wxSize client_size = GetClientSize();
    int start = GetLineStart( firstRow );
    m_owner->CalcScrolledPosition( start, 0, &start, NULL );
    if (start > client_size.y) return;

    wxRect rect( 0, start, client_size.x, client_size.y - start );

    Refresh( true, &rect );
}

void wxDataViewMainWindow::OnArrowChar(unsigned int newCurrent, const wxKeyEvent& event)
{
    wxCHECK_RET( newCurrent < GetRowCount(),
                wxT("invalid item index in OnArrowChar()") );

    // if there is no selection, we cannot move it anywhere
    if (!HasCurrentRow())
        return;

    unsigned int oldCurrent = m_currentRow;

    // in single selection we just ignore Shift as we can't select several
    // items anyhow
    if ( event.ShiftDown() && !IsSingleSel() )
    {
        RefreshRow( oldCurrent );

        ChangeCurrentRow( newCurrent );

        // select all the items between the old and the new one
        if ( oldCurrent > newCurrent )
        {
            newCurrent = oldCurrent;
            oldCurrent = m_currentRow;
        }

        SelectRows( oldCurrent, newCurrent, true );
        if (oldCurrent!=newCurrent)
            SendSelectionChangedEvent(GetItemByRow(m_selection[0]));
    }
    else // !shift
    {
        RefreshRow( oldCurrent );

        // all previously selected items are unselected unless ctrl is held
        if ( !event.ControlDown() )
            SelectAllRows(false);

        ChangeCurrentRow( newCurrent );

        if ( !event.ControlDown() )
        {
            SelectRow( m_currentRow, true );
            SendSelectionChangedEvent(GetItemByRow(m_currentRow));
        }
        else
            RefreshRow( m_currentRow );
    }

    GetOwner()->EnsureVisible( m_currentRow, -1 );
}

wxRect wxDataViewMainWindow::GetLineRect( unsigned int row ) const
{
    wxRect rect;
    rect.x = 0;
    rect.y = GetLineStart( row );
    rect.width = GetEndOfLastCol();
    rect.height = GetLineHeight( row );

    return rect;
}

int wxDataViewMainWindow::GetLineStart( unsigned int row ) const
{
    const wxDataViewModel *model = GetOwner()->GetModel();

    if (GetOwner()->GetWindowStyle() & wxDV_VARIABLE_LINE_HEIGHT)
    {
        // TODO make more efficient

        int start = 0;

        unsigned int r;
        for (r = 0; r < row; r++)
        {
            const wxDataViewTreeNode* node = GetTreeNodeByRow(r);
            if (!node) return start;

            wxDataViewItem item = node->GetItem();

            if (node && !node->HasChildren())
            {
                // Yes, if the node does not have any child, it must be a leaf which
                // mean that it is a temporarily created by GetTreeNodeByRow
                wxDELETE(node);
            }

            unsigned int cols = GetOwner()->GetColumnCount();
            unsigned int col;
            int height = m_lineHeight;
            for (col = 0; col < cols; col++)
            {
                const wxDataViewColumn *column = GetOwner()->GetColumn(col);
                if (column->IsHidden())
                    continue;      // skip it!

                if ((col != 0) &&
                    model->IsContainer(item) &&
                    !model->HasContainerColumns(item))
                    continue;      // skip it!

                wxDataViewRenderer *renderer =
                    const_cast<wxDataViewRenderer*>(column->GetRenderer());
                renderer->PrepareForItem(model, item, column->GetModelColumn());

                height = wxMax( height, renderer->GetSize().y );
            }

            start += height;
        }

        return start;
    }
    else
    {
        return row * m_lineHeight;
    }
}

int wxDataViewMainWindow::GetLineAt( unsigned int y ) const
{
    const wxDataViewModel *model = GetOwner()->GetModel();

    // check for the easy case first
    if ( !GetOwner()->HasFlag(wxDV_VARIABLE_LINE_HEIGHT) )
        return y / m_lineHeight;

    // TODO make more efficient
    unsigned int row = 0;
    unsigned int yy = 0;
    for (;;)
    {
        const wxDataViewTreeNode* node = GetTreeNodeByRow(row);
        if (!node)
        {
            // not really correct...
            return row + ((y-yy) / m_lineHeight);
        }

        wxDataViewItem item = node->GetItem();

        if (node && !node->HasChildren())
        {
            // Yes, if the node does not have any child, it must be a leaf which
            // mean that it is a temporarily created by GetTreeNodeByRow
            wxDELETE(node);
        }

        unsigned int cols = GetOwner()->GetColumnCount();
        unsigned int col;
        int height = m_lineHeight;
        for (col = 0; col < cols; col++)
        {
            const wxDataViewColumn *column = GetOwner()->GetColumn(col);
            if (column->IsHidden())
                continue;      // skip it!

            if ((col != 0) &&
                model->IsContainer(item) &&
                !model->HasContainerColumns(item))
                continue;      // skip it!

            wxDataViewRenderer *renderer =
                const_cast<wxDataViewRenderer*>(column->GetRenderer());
            renderer->PrepareForItem(model, item, column->GetModelColumn());

            height = wxMax( height, renderer->GetSize().y );
        }

        yy += height;
        if (y < yy)
            return row;

        row++;
    }
}

int wxDataViewMainWindow::GetLineHeight( unsigned int row ) const
{
    const wxDataViewModel *model = GetOwner()->GetModel();

    if (GetOwner()->GetWindowStyle() & wxDV_VARIABLE_LINE_HEIGHT)
    {
        wxASSERT( !IsVirtualList() );

        const wxDataViewTreeNode* node = GetTreeNodeByRow(row);
        // wxASSERT( node );
        if (!node) return m_lineHeight;

        wxDataViewItem item = node->GetItem();

        if (node && !node->HasChildren())
        {
                // Yes, if the node does not have any child, it must be a leaf which
                // mean that it is a temporarily created by GetTreeNodeByRow
            wxDELETE(node);
        }

        int height = m_lineHeight;

        unsigned int cols = GetOwner()->GetColumnCount();
        unsigned int col;
        for (col = 0; col < cols; col++)
        {
            const wxDataViewColumn *column = GetOwner()->GetColumn(col);
            if (column->IsHidden())
                continue;      // skip it!

            if ((col != 0) &&
                model->IsContainer(item) &&
                !model->HasContainerColumns(item))
                continue;      // skip it!

            wxDataViewRenderer *renderer =
                const_cast<wxDataViewRenderer*>(column->GetRenderer());
            renderer->PrepareForItem(model, item, column->GetModelColumn());

            height = wxMax( height, renderer->GetSize().y );
        }

        return height;
    }
    else
    {
        return m_lineHeight;
    }
}

class RowToItemJob: public DoJob
{
public:
    RowToItemJob( unsigned int row , int current )
        { this->row = row; this->current = current; }
    virtual ~RowToItemJob() {}

    virtual int operator() ( wxDataViewTreeNode * node )
    {
        current ++;
        if( current == static_cast<int>(row))
        {
            ret = node->GetItem();
            return DoJob::OK;
        }

        if( node->GetSubTreeCount() + current < static_cast<int>(row) )
        {
            current += node->GetSubTreeCount();
            return  DoJob::IGR;
        }
        else
        {
            // If the current has no child node, we can find the desired item of the row
            // number directly.
            // This if can speed up finding in some case, and will has a very good effect
            // when it comes to list view
            if( node->GetNodes().GetCount() == 0)
            {
                int index = static_cast<int>(row) - current - 1;
                ret = node->GetChildren().Item( index );
                return DoJob::OK;
            }
            return DoJob::CONT;
        }
    }

    virtual int operator() ( void * n )
    {
        current ++;
        if( current == static_cast<int>(row))
        {
            ret = wxDataViewItem( n );
            return DoJob::OK;
        }
        return DoJob::CONT;
    }

    wxDataViewItem GetResult() const
        { return ret; }

private:
    unsigned int row;
    int current;
    wxDataViewItem ret;
};

wxDataViewItem wxDataViewMainWindow::GetItemByRow(unsigned int row) const
{
    if (IsVirtualList())
    {
        return wxDataViewItem( wxUIntToPtr(row+1) );
    }
    else
    {
        RowToItemJob job( row, -2 );
        Walker( m_root , job );
        return job.GetResult();
    }
}

class RowToTreeNodeJob: public DoJob
{
public:
    RowToTreeNodeJob( unsigned int row , int current, wxDataViewTreeNode * node )
    {
        this->row = row;
        this->current = current;
        ret = NULL;
        parent = node;
    }
    virtual ~RowToTreeNodeJob(){ }

    virtual int operator() ( wxDataViewTreeNode * node )
    {
        current ++;
        if( current == static_cast<int>(row))
        {
            ret = node;
            return DoJob::OK;
        }

        if( node->GetSubTreeCount() + current < static_cast<int>(row) )
        {
            current += node->GetSubTreeCount();
            return  DoJob::IGR;
        }
        else
        {
            parent = node;

            // If the current node has no children, we can find the desired item of the
            // row number directly.
            // This if can speed up finding in some case, and will have a very good
            // effect for list views.
            if( node->GetNodes().GetCount() == 0)
            {
                int index = static_cast<int>(row) - current - 1;
                void * n = node->GetChildren().Item( index );
                ret = new wxDataViewTreeNode( parent );
                ret->SetItem( wxDataViewItem( n ));
                ret->SetHasChildren(false);
                return DoJob::OK;
            }
            return DoJob::CONT;
        }
    }

    virtual int operator() ( void * n )
    {
        current ++;
        if( current == static_cast<int>(row))
        {
            ret = new wxDataViewTreeNode( parent );
            ret->SetItem( wxDataViewItem( n ));
            ret->SetHasChildren(false);
            return DoJob::OK;
        }

        return DoJob::CONT;
    }

    wxDataViewTreeNode * GetResult() const
        { return ret; }

private:
    unsigned int row;
    int current;
    wxDataViewTreeNode * ret;
    wxDataViewTreeNode * parent;
};

wxDataViewTreeNode * wxDataViewMainWindow::GetTreeNodeByRow(unsigned int row) const
{
    wxASSERT( !IsVirtualList() );

    RowToTreeNodeJob job( row , -2, m_root );
    Walker( m_root , job );
    return job.GetResult();
}

wxDataViewEvent wxDataViewMainWindow::SendExpanderEvent( wxEventType type,
                                                         const wxDataViewItem & item )
{
    wxWindow *parent = GetParent();
    wxDataViewEvent le(type, parent->GetId());

    le.SetEventObject(parent);
    le.SetModel(GetOwner()->GetModel());
    le.SetItem( item );

    parent->GetEventHandler()->ProcessEvent(le);
    return le;
}

bool wxDataViewMainWindow::IsExpanded( unsigned int row ) const
{
    if (IsList())
        return false;

    wxDataViewTreeNode * node = GetTreeNodeByRow(row);
    if (!node)
        return false;

    if (!node->HasChildren())
    {
        delete node;
        return false;
    }

    return node->IsOpen();
}

bool wxDataViewMainWindow::HasChildren( unsigned int row ) const
{
    if (IsList())
        return false;

    wxDataViewTreeNode * node = GetTreeNodeByRow(row);
    if (!node)
        return false;

    if (!node->HasChildren())
    {
        delete node;
        return false;
    }

    return true;
}

void wxDataViewMainWindow::Expand( unsigned int row )
{
    if (IsList())
        return;

    wxDataViewTreeNode * node = GetTreeNodeByRow(row);
    if (!node)
        return;

    if (!node->HasChildren())
    {
        delete node;
        return;
    }

            if (!node->IsOpen())
            {
                wxDataViewEvent e =
                    SendExpanderEvent(wxEVT_COMMAND_DATAVIEW_ITEM_EXPANDING, node->GetItem());

                // Check if the user prevent expanding
                if( e.GetSkipped() )
                    return;

                node->ToggleOpen();

                // build the children of current node
                if( node->GetChildrenNumber() == 0 )
                {
                    SortPrepare();
                    ::BuildTreeHelper(GetOwner()->GetModel(), node->GetItem(), node);
                }

                // By expanding the node all row indices that are currently in the selection list
                // and are greater than our node have become invalid. So we have to correct that now.
                const unsigned rowAdjustment = node->GetSubTreeCount();
                for(unsigned i=0; i<m_selection.size(); ++i)
                {
                    const unsigned testRow = m_selection[i];
                    // all rows above us are not affected, so skip them
                    if(testRow <= row)
                        continue;

                    m_selection[i] += rowAdjustment;
                }

                if(m_currentRow > row)
                    ChangeCurrentRow(m_currentRow + rowAdjustment);

                m_count = -1;
                UpdateDisplay();
                // Send the expanded event
                SendExpanderEvent(wxEVT_COMMAND_DATAVIEW_ITEM_EXPANDED,node->GetItem());
            }
}

void wxDataViewMainWindow::Collapse(unsigned int row)
{
    if (IsList())
        return;

    wxDataViewTreeNode *node = GetTreeNodeByRow(row);
    if (!node)
        return;

    if (!node->HasChildren())
    {
        delete node;
        return;
    }

        if (node->IsOpen())
        {
            wxDataViewEvent e =
                SendExpanderEvent(wxEVT_COMMAND_DATAVIEW_ITEM_COLLAPSING,node->GetItem());
            if( e.GetSkipped() )
                return;

            // Find out if there are selected items below the current node.
            bool selectCollapsingRow = false;
            const unsigned rowAdjustment = node->GetSubTreeCount();
            unsigned maxRowToBeTested = row + rowAdjustment;
            for(unsigned i=0; i<m_selection.size(); ++i)
            {
                const unsigned testRow = m_selection[i];
                if(testRow > row && testRow <= maxRowToBeTested)
                {
                    selectCollapsingRow = true;
                    // get out as soon as we have found a node that is selected
                    break;
                }
            }

            node->ToggleOpen();

            // If the node to be closed has selected items the user won't see those any longer.
            // We select the collapsing node in this case.
            if(selectCollapsingRow)
            {
                SelectAllRows(false);
                ChangeCurrentRow(row);
                SelectRow(row, true);
                SendSelectionChangedEvent(GetItemByRow(row));
            }
            else
            {
                // if there were no selected items below our node we still need to "fix" the
                // selection list to adjust for the changing of the row indices.
                // We actually do the opposite of what we are doing in Expand().
                for(unsigned i=0; i<m_selection.size(); ++i)
                {
                    const unsigned testRow = m_selection[i];
                    // all rows above us are not affected, so skip them
                    if(testRow <= row)
                        continue;

                    m_selection[i] -= rowAdjustment;
                }

                // if the "current row" is being collapsed away we change it to the current row ;-)
                if(m_currentRow > row && m_currentRow <= maxRowToBeTested)
                    ChangeCurrentRow(row);
                else if(m_currentRow > row)
                    ChangeCurrentRow(m_currentRow - rowAdjustment);
            }

            m_count = -1;
            UpdateDisplay();
            SendExpanderEvent(wxEVT_COMMAND_DATAVIEW_ITEM_COLLAPSED,node->GetItem());
        }
}

wxDataViewTreeNode * wxDataViewMainWindow::FindNode( const wxDataViewItem & item )
{
    const wxDataViewModel * model = GetOwner()->GetModel();
    if( model == NULL )
        return NULL;

    if (!item.IsOk())
        return m_root;

    // Compose the parent-chain for the item we are looking for
    wxVector<wxDataViewItem> parentChain;
    wxDataViewItem it( item );
    while( it.IsOk() )
    {
        parentChain.push_back(it);
        it = model->GetParent(it);
    }

    // Find the item along the parent-chain.
    // This algorithm is designed to speed up the node-finding method
    wxDataViewTreeNode* node = m_root;
    for( unsigned iter = parentChain.size()-1; iter>=0; --iter )
    {
        if( node->HasChildren() )
        {
            if( node->GetChildrenNumber() == 0 )
            {
                SortPrepare();
                ::BuildTreeHelper(model, node->GetItem(), node);
            }

            const wxDataViewTreeNodes& nodes = node->GetNodes();
            bool found = false;

            for (unsigned i = 0; i < nodes.GetCount(); ++i)
            {
                wxDataViewTreeNode* currentNode = nodes[i];
                if (currentNode->GetItem() == parentChain[iter])
                {
                    if (currentNode->GetItem() == item)
                        return currentNode;

                    node = currentNode;
                    found = true;
                    break;
                }
            }
            if (!found)
                return NULL;
        }
        else
            return NULL;
    }
    return NULL;
}

void wxDataViewMainWindow::HitTest( const wxPoint & point, wxDataViewItem & item,
                                    wxDataViewColumn* &column )
{
    wxDataViewColumn *col = NULL;
    unsigned int cols = GetOwner()->GetColumnCount();
    unsigned int colnum = 0;
    int x, y;
    m_owner->CalcUnscrolledPosition( point.x, point.y, &x, &y );
    for (unsigned x_start = 0; colnum < cols; colnum++)
    {
        col = GetOwner()->GetColumnAt(colnum);
        if (col->IsHidden())
            continue;      // skip it!

        unsigned int w = col->GetWidth();
        if (x_start+w >= (unsigned int)x)
            break;

        x_start += w;
    }

    column = col;
    item = GetItemByRow( GetLineAt( y ) );
}

wxRect wxDataViewMainWindow::GetItemRect( const wxDataViewItem & item,
                                          const wxDataViewColumn* column )
{
    int xpos = 0;
    int width = 0;

    unsigned int cols = GetOwner()->GetColumnCount();
    // If column is null the loop will compute the combined width of all columns.
    // Otherwise, it will compute the x position of the column we are looking for.
    for (unsigned int i = 0; i < cols; i++)
    {
        wxDataViewColumn* col = GetOwner()->GetColumnAt( i );

        if (col == column)
            break;

        if (col->IsHidden())
            continue;      // skip it!

        xpos += col->GetWidth();
        width += col->GetWidth();
    }

    if(column != 0)
    {
        // If we have a column, we need can get its width directly.
        if(column->IsHidden())
            width = 0;
        else
            width = column->GetWidth();

    }
    else
    {
        // If we have no column, we reset the x position back to zero.
        xpos = 0;
    }

    // we have to take an expander column into account and compute its indentation
    // to get the correct x position where the actual text is
    int indent = 0;
    int row = GetRowByItem(item);
    if (!IsList() && (column == 0 || GetOwner()->GetExpanderColumn() == column) )
    {
        wxDataViewTreeNode* node = GetTreeNodeByRow(row);
        indent = GetOwner()->GetIndent() * node->GetIndentLevel();
        indent = indent + m_lineHeight; // use m_lineHeight as the width of the expander

        if(!node->HasChildren())
            delete node;
    }

    wxRect itemRect( xpos + indent,
                     GetLineStart( row ),
                     width - indent,
                     GetLineHeight( row ) );

    GetOwner()->CalcScrolledPosition(  itemRect.x,  itemRect.y,
                                      &itemRect.x, &itemRect.y );

    return itemRect;
}

int wxDataViewMainWindow::RecalculateCount()
{
    if (IsVirtualList())
    {
        wxDataViewVirtualListModel *list_model =
            (wxDataViewVirtualListModel*) GetOwner()->GetModel();

        return list_model->GetCount();
    }
    else
    {
        return m_root->GetSubTreeCount();
    }
}

class ItemToRowJob : public DoJob
{
public:
    ItemToRowJob(const wxDataViewItem& item_, wxVector<wxDataViewItem>::reverse_iterator iter)
        : m_iter(iter),
        item(item_)
    {
        ret = -1;
    }

    // Maybe binary search will help to speed up this process
    virtual int operator() ( wxDataViewTreeNode * node)
    {
        ret ++;
        if( node->GetItem() == item )
        {
            return DoJob::OK;
        }

        if( node->GetItem() == *m_iter )
        {
            m_iter++;
            return DoJob::CONT;
        }
        else
        {
            ret += node->GetSubTreeCount();
            return DoJob::IGR;
        }

    }

    virtual int operator() ( void * n )
    {
        ret ++;
        if( n == item.GetID() )
            return DoJob::OK;
        return DoJob::CONT;
    }

    // the row number is begin from zero
    int GetResult() const
        { return ret -1; }

private:
    wxVector<wxDataViewItem>::reverse_iterator m_iter;
    wxDataViewItem item;
    int ret;

};

int wxDataViewMainWindow::GetRowByItem(const wxDataViewItem & item) const
{
    const wxDataViewModel * model = GetOwner()->GetModel();
    if( model == NULL )
        return -1;

    if (IsVirtualList())
    {
        return wxPtrToUInt( item.GetID() ) -1;
    }
    else
    {
        if( !item.IsOk() )
            return -1;

        // Compose the parent-chain of the item we are looking for
        wxVector<wxDataViewItem> parentChain;
        wxDataViewItem it( item );
        while( it.IsOk() )
        {
            parentChain.push_back(it);
            it = model->GetParent(it);
        }

        // add an 'invalid' item to represent our 'invisible' root node
        parentChain.push_back(wxDataViewItem());

        // the parent chain was created by adding the deepest parent first.
        // so if we want to start at the root node, we have to iterate backwards through the vector
        ItemToRowJob job( item, parentChain.rbegin() );
        Walker( m_root, job );
        return job.GetResult();
    }
}

static void BuildTreeHelper( const wxDataViewModel * model,  const wxDataViewItem & item,
                             wxDataViewTreeNode * node)
{
    if( !model->IsContainer( item ) )
        return;

    wxDataViewItemArray children;
    unsigned int num = model->GetChildren( item, children);

    unsigned int index = 0;
    while( index < num )
    {
        if( model->IsContainer( children[index] ) )
        {
            wxDataViewTreeNode * n = new wxDataViewTreeNode( node );
            n->SetItem(children[index]);
            n->SetHasChildren( true );
            node->AddNode( n );
        }
        else
        {
            node->AddLeaf( children[index].GetID() );
        }
        index ++;
    }
    node->SetSubTreeCount( num );
    wxDataViewTreeNode * n = node->GetParent();
    if( n != NULL)
        n->ChangeSubTreeCount(num);

}

void wxDataViewMainWindow::BuildTree(wxDataViewModel * model)
{
    DestroyTree();

    if (GetOwner()->GetModel()->IsVirtualListModel())
    {
        m_count = -1;
        return;
    }

    m_root = new wxDataViewTreeNode( NULL );
    m_root->SetHasChildren(true);

    // First we define a invalid item to fetch the top-level elements
    wxDataViewItem item;
    SortPrepare();
    BuildTreeHelper( model, item, m_root);
    m_count = -1;
}

static void DestroyTreeHelper( wxDataViewTreeNode * node )
{
    if( node->GetNodeNumber() != 0 )
    {
        int len = node->GetNodeNumber();
        wxDataViewTreeNodes& nodes = node->GetNodes();
        for (int i = 0; i < len; i++)
            DestroyTreeHelper(nodes[i]);
    }
    delete node;
}

void wxDataViewMainWindow::DestroyTree()
{
    if (!IsVirtualList())
    {
        ::DestroyTreeHelper(m_root);
            m_count = 0;
            m_root = NULL;
    }
}

void wxDataViewMainWindow::OnChar( wxKeyEvent &event )
{
    wxWindow * const parent = GetParent();

    // propagate the char event upwards
    wxKeyEvent eventForParent(event);
    eventForParent.SetEventObject(parent);
    if ( parent->ProcessWindowEvent(eventForParent) )
        return;

    if ( parent->HandleAsNavigationKey(event) )
        return;

    // no item -> nothing to do
    if (!HasCurrentRow())
    {
        event.Skip();
        return;
    }

    // don't use m_linesPerPage directly as it might not be computed yet
    const int pageSize = GetCountPerPage();
    wxCHECK_RET( pageSize, wxT("should have non zero page size") );

    switch ( event.GetKeyCode() )
    {
        case WXK_RETURN:
            {
                wxDataViewEvent le(wxEVT_COMMAND_DATAVIEW_ITEM_ACTIVATED,
                                   parent->GetId());
                le.SetItem( GetItemByRow(m_currentRow) );
                le.SetEventObject(parent);
                le.SetModel(GetOwner()->GetModel());

                parent->GetEventHandler()->ProcessEvent(le);
            }
            break;

        case WXK_UP:
            if ( m_currentRow > 0 )
                OnArrowChar( m_currentRow - 1, event );
            break;

        case WXK_DOWN:
            if ( m_currentRow < GetRowCount() - 1 )
                OnArrowChar( m_currentRow + 1, event );
            break;
        // Add the process for tree expanding/collapsing
        case WXK_LEFT:
        {
            if (IsList())
               break;

            wxDataViewTreeNode* node = GetTreeNodeByRow(m_currentRow);
            if (!node)
                break;

            if (node->HasChildren() && node->IsOpen())
            {
                Collapse(m_currentRow);
            }
            else    // if the node is already closed we move the selection to its parent
            {
                wxDataViewTreeNode *parent_node = node->GetParent();

                if(!node->HasChildren())
                    delete node;

                if (parent_node)
                {
                    int parent = GetRowByItem( parent_node->GetItem() );
                    if ( parent >= 0 )
                    {
                        unsigned int row = m_currentRow;
                        SelectRow( row, false);
                        SelectRow( parent, true );
                        ChangeCurrentRow( parent );
                        GetOwner()->EnsureVisible( parent, -1 );
                        SendSelectionChangedEvent( parent_node->GetItem() );
                    }
                }
            }
            break;
        }
        case WXK_RIGHT:
        {
            if (!IsExpanded( m_currentRow ))
                Expand( m_currentRow );
            else
            {
                unsigned int row = m_currentRow;
                SelectRow( row, false );
                SelectRow( row + 1, true );
                ChangeCurrentRow( row + 1 );
                GetOwner()->EnsureVisible( row + 1, -1 );
                SendSelectionChangedEvent( GetItemByRow(row+1) );
            }
            break;
        }
        case WXK_END:
        {
            if (!IsEmpty())
                OnArrowChar( GetRowCount() - 1, event );
            break;
        }
        case WXK_HOME:
            if (!IsEmpty())
                OnArrowChar( 0, event );
            break;

        case WXK_PAGEUP:
            {
                int steps = pageSize - 1;
                int index = m_currentRow - steps;
                if (index < 0)
                    index = 0;

                OnArrowChar( index, event );
            }
            break;

        case WXK_PAGEDOWN:
            {
                int steps = pageSize - 1;
                unsigned int index = m_currentRow + steps;
                unsigned int count = GetRowCount();
                if ( index >= count )
                    index = count - 1;

                OnArrowChar( index, event );
            }
            break;

        case WXK_F2:
            {
                if(m_selection.size() == 1)
                {
                    // TODO: we need to revise that when we have a concept for a 'current column'
                    GetOwner()->StartEditor(GetItemByRow(m_selection[0]), 0);
                }
            }
            break;

        default:
            event.Skip();
    }
}

void wxDataViewMainWindow::OnMouse( wxMouseEvent &event )
{
    if (event.GetEventType() == wxEVT_MOUSEWHEEL)
    {
        // let the base handle mouse wheel events.
        event.Skip();
        return;
    }

    // set the focus to ourself if any of the mouse buttons are pressed
    if(event.ButtonDown() && !HasFocus())
        SetFocus();

    int x = event.GetX();
    int y = event.GetY();
    m_owner->CalcUnscrolledPosition( x, y, &x, &y );
    wxDataViewColumn *col = NULL;

    int xpos = 0;
    unsigned int cols = GetOwner()->GetColumnCount();
    unsigned int i;
    for (i = 0; i < cols; i++)
    {
        wxDataViewColumn *c = GetOwner()->GetColumnAt( i );
        if (c->IsHidden())
            continue;      // skip it!

        if (x < xpos + c->GetWidth())
        {
            col = c;
            break;
        }
        xpos += c->GetWidth();
    }
    if (!col)
    {
        event.Skip();
        return;
    }

    wxDataViewRenderer *cell = col->GetRenderer();
    unsigned int current = GetLineAt( y );
    if ((current >= GetRowCount()) || (x > GetEndOfLastCol()))
    {
        // Unselect all if below the last row ?
        event.Skip();
        return;
    }

    // Test whether the mouse is hovered on the tree item button
    bool hoverOverExpander = false;
    if ((!IsList()) && (GetOwner()->GetExpanderColumn() == col))
    {
        wxDataViewTreeNode * node = GetTreeNodeByRow(current);
        if( node!=NULL && node->HasChildren() )
        {
            int indent = node->GetIndentLevel();
            indent = GetOwner()->GetIndent()*indent;

            // we make the rectangle we are looking in a bit bigger than the actual
            // visual expander so the user can hit that little thing reliably
            wxRect rect( xpos + indent,
                        GetLineStart( current ) + (GetLineHeight(current) - m_lineHeight)/2,
                        m_lineHeight, m_lineHeight);
            if( rect.Contains(x, y) )
            {
                // So the mouse is over the expander
                hoverOverExpander = true;
                if (m_underMouse && m_underMouse != node)
                {
                    // wxLogMessage("Undo the row: %d", GetRowByItem(m_underMouse->GetItem()));
                    RefreshRow(GetRowByItem(m_underMouse->GetItem()));
                }
                if (m_underMouse != node)
                {
                    // wxLogMessage("Do the row: %d", current);
                    RefreshRow(current);
                }
                m_underMouse = node;
            }
        }
        if (node!=NULL && !node->HasChildren())
            delete node;
    }
    if (!hoverOverExpander)
    {
        if (m_underMouse != NULL)
        {
            // wxLogMessage("Undo the row: %d", GetRowByItem(m_underMouse->GetItem()));
            RefreshRow(GetRowByItem(m_underMouse->GetItem()));
            m_underMouse = NULL;
        }
    }

    wxDataViewModel *model = GetOwner()->GetModel();

#if wxUSE_DRAG_AND_DROP
    if (event.Dragging())
    {
        if (m_dragCount == 0)
        {
            // we have to report the raw, physical coords as we want to be
            // able to call HitTest(event.m_pointDrag) from the user code to
            // get the item being dragged
            m_dragStart = event.GetPosition();
        }

        m_dragCount++;

        if (m_dragCount != 3)
            return;

        if (event.LeftIsDown())
        {
            m_owner->CalcUnscrolledPosition( m_dragStart.x, m_dragStart.y,
                                             &m_dragStart.x, &m_dragStart.y );
            unsigned int drag_item_row = GetLineAt( m_dragStart.y );
            wxDataViewItem item = GetItemByRow( drag_item_row );

            // Notify cell about drag
            wxDataViewEvent event( wxEVT_COMMAND_DATAVIEW_ITEM_BEGIN_DRAG, m_owner->GetId() );
            event.SetEventObject( m_owner );
            event.SetItem( item );
            event.SetModel( model );
            if (!m_owner->HandleWindowEvent( event ))
                return;

            if (!event.IsAllowed())
                return;

            wxDataObject *obj = event.GetDataObject();
            if (!obj)
                return;

            wxDataViewDropSource drag( this, drag_item_row );
            drag.SetData( *obj );
            /* wxDragResult res = */ drag.DoDragDrop();
            delete obj;
        }
        return;
    }
    else
    {
        m_dragCount = 0;
    }
#endif // wxUSE_DRAG_AND_DROP

    bool simulateClick = false;

    if (event.ButtonDClick())
    {
        m_renameTimer->Stop();
        m_lastOnSame = false;
    }

    wxDataViewItem item = GetItemByRow(current);
    bool ignore_other_columns =
        ((GetOwner()->GetExpanderColumn() != col) &&
        (model->IsContainer(item)) &&
        (!model->HasContainerColumns(item)));

    if (event.LeftDClick())
    {
        if(hoverOverExpander)
        {
            // a double click on the expander will be converted into a "simulated" normal click
            simulateClick = true;
        }
        else if ( current == m_lineLastClicked )
        {
            if ((!ignore_other_columns) && (cell->GetMode() == wxDATAVIEW_CELL_ACTIVATABLE))
            {
                const unsigned colIdx = col->GetModelColumn();

                cell->PrepareForItem(model, item, colIdx);

                wxRect cell_rect( xpos, GetLineStart( current ),
                                col->GetWidth(), GetLineHeight( current ) );
                cell->WXOnActivate( cell_rect, model, item, colIdx );
            }
            else
            {
                wxWindow *parent = GetParent();
                wxDataViewEvent le(wxEVT_COMMAND_DATAVIEW_ITEM_ACTIVATED, parent->GetId());
                le.SetItem( item );
                le.SetColumn( col->GetModelColumn() );
                le.SetDataViewColumn( col );
                le.SetEventObject(parent);
                le.SetModel(GetOwner()->GetModel());

                parent->GetEventHandler()->ProcessEvent(le);
            }
            return;
        }
        else
        {
            // The first click was on another item, so don't interpret this as
            // a double click, but as a simple click instead
            simulateClick = true;
        }
    }

    if (event.LeftUp() && !hoverOverExpander)
    {
        if (m_lineSelectSingleOnUp != (unsigned int)-1)
        {
            // select single line
            SelectAllRows( false );
            SelectRow( m_lineSelectSingleOnUp, true );
            SendSelectionChangedEvent( GetItemByRow(m_lineSelectSingleOnUp) );
        }

        // If the user click the expander, we do not do editing even if the column
        // with expander are editable
        if (m_lastOnSame && !ignore_other_columns)
        {
            if ((col == m_currentCol) && (current == m_currentRow) &&
                (cell->GetMode() & wxDATAVIEW_CELL_EDITABLE) )
            {
                m_renameTimer->Start( 100, true );
            }
        }

        m_lastOnSame = false;
        m_lineSelectSingleOnUp = (unsigned int)-1;
    }
    else if(!event.LeftUp())
    {
        // This is necessary, because after a DnD operation in
        // from and to ourself, the up event is swallowed by the
        // DnD code. So on next non-up event (which means here and
        // now) m_lineSelectSingleOnUp should be reset.
        m_lineSelectSingleOnUp = (unsigned int)-1;
    }

    if (event.RightDown())
    {
        m_lineBeforeLastClicked = m_lineLastClicked;
        m_lineLastClicked = current;

        // If the item is already selected, do not update the selection.
        // Multi-selections should not be cleared if a selected item is clicked.
        if (!IsRowSelected(current))
        {
            SelectAllRows(false);
            ChangeCurrentRow(current);
            SelectRow(m_currentRow,true);
            SendSelectionChangedEvent(GetItemByRow( m_currentRow ) );
        }
    }
    else if (event.RightUp())
    {
        wxVariant value;
        model->GetValue( value, item, col->GetModelColumn() );
        wxWindow *parent = GetParent();
        wxDataViewEvent le(wxEVT_COMMAND_DATAVIEW_ITEM_CONTEXT_MENU, parent->GetId());
        le.SetItem( item );
        le.SetColumn( col->GetModelColumn() );
        le.SetDataViewColumn( col );
        le.SetEventObject(parent);
        le.SetModel(GetOwner()->GetModel());
        le.SetValue(value);
        parent->GetEventHandler()->ProcessEvent(le);
    }
    else if (event.MiddleDown())
    {
    }

    if((event.LeftDown() || simulateClick) && hoverOverExpander)
    {
        wxDataViewTreeNode* node = GetTreeNodeByRow(current);

        // hoverOverExpander being true tells us that our node must be
        // valid and have children.
        // So we don't need any extra checks.
        if( node->IsOpen() )
            Collapse(current);
        else
            Expand(current);
    }
    else if ((event.LeftDown() || simulateClick) && !hoverOverExpander)
    {
        m_lineBeforeLastClicked = m_lineLastClicked;
        m_lineLastClicked = current;

        unsigned int oldCurrentRow = m_currentRow;
        bool oldWasSelected = IsRowSelected(m_currentRow);

        bool cmdModifierDown = event.CmdDown();
        if ( IsSingleSel() || !(cmdModifierDown || event.ShiftDown()) )
        {
            if ( IsSingleSel() || !IsRowSelected(current) )
            {
                SelectAllRows( false );
                ChangeCurrentRow(current);
                SelectRow(m_currentRow,true);
                SendSelectionChangedEvent(GetItemByRow( m_currentRow ) );
            }
            else // multi sel & current is highlighted & no mod keys
            {
                m_lineSelectSingleOnUp = current;
                ChangeCurrentRow(current); // change focus
            }
        }
        else // multi sel & either ctrl or shift is down
        {
            if (cmdModifierDown)
            {
                ChangeCurrentRow(current);
                ReverseRowSelection(m_currentRow);
                SendSelectionChangedEvent(GetItemByRow(m_currentRow));
            }
            else if (event.ShiftDown())
            {
                ChangeCurrentRow(current);

                unsigned int lineFrom = oldCurrentRow,
                    lineTo = current;

                if ( lineTo < lineFrom )
                {
                    lineTo = lineFrom;
                    lineFrom = m_currentRow;
                }

                SelectRows(lineFrom, lineTo, true);
                SendSelectionChangedEvent(GetItemByRow(m_selection[0]) );
            }
            else // !ctrl, !shift
            {
                // test in the enclosing if should make it impossible
                wxFAIL_MSG( wxT("how did we get here?") );
            }
        }

        if (m_currentRow != oldCurrentRow)
            RefreshRow( oldCurrentRow );

        wxDataViewColumn *oldCurrentCol = m_currentCol;

        // Update selection here...
        m_currentCol = col;

        m_lastOnSame = !simulateClick && ((col == oldCurrentCol) &&
                        (current == oldCurrentRow)) && oldWasSelected;

        // Call LeftClick after everything else as under GTK+
        if (cell->GetMode() & wxDATAVIEW_CELL_ACTIVATABLE)
        {
            // notify cell about click
            cell->PrepareForItem(model, item, col->GetModelColumn());

            wxRect cell_rect( xpos, GetLineStart( current ),
                              col->GetWidth(), GetLineHeight( current ) );

            // Report position relative to the cell's custom area, i.e.
            // no the entire space as given by the control but the one
            // used by the renderer after calculation of alignment etc.

            // adjust the rectangle ourselves to account for the alignment
            wxRect rectItem = cell_rect;
            const int align = cell->GetAlignment();
            if ( align != wxDVR_DEFAULT_ALIGNMENT )
            {
                const wxSize size = cell->GetSize();

                if ( size.x >= 0 && size.x < cell_rect.width )
                {
                    if ( align & wxALIGN_CENTER_HORIZONTAL )
                        rectItem.x += (cell_rect.width - size.x)/2;
                    else if ( align & wxALIGN_RIGHT )
                        rectItem.x += cell_rect.width - size.x;
                    // else: wxALIGN_LEFT is the default
                }

                if ( size.y >= 0 && size.y < cell_rect.height )
                {
                    if ( align & wxALIGN_CENTER_VERTICAL )
                        rectItem.y += (cell_rect.height - size.y)/2;
                    else if ( align & wxALIGN_BOTTOM )
                        rectItem.y += cell_rect.height - size.y;
                    // else: wxALIGN_TOP is the default
                }
            }

            wxPoint pos( event.GetPosition() );
            pos.x -= rectItem.x;
            pos.y -= rectItem.y;

            m_owner->CalcUnscrolledPosition( pos.x, pos.y, &pos.x, &pos.y );

             /* ignore ret */ cell->WXOnLeftClick( pos, cell_rect,
                              model, item, col->GetModelColumn());
        }
    }
}

void wxDataViewMainWindow::OnSetFocus( wxFocusEvent &event )
{
    m_hasFocus = true;

    if (HasCurrentRow())
        Refresh();

    event.Skip();
}

void wxDataViewMainWindow::OnKillFocus( wxFocusEvent &event )
{
    m_hasFocus = false;

    if (HasCurrentRow())
        Refresh();

    event.Skip();
}

wxDataViewItem wxDataViewMainWindow::GetSelection() const
{
    if( m_selection.GetCount() != 1 )
        return wxDataViewItem();

    return GetItemByRow( m_selection.Item(0));
}

//-----------------------------------------------------------------------------
// wxDataViewCtrl
//-----------------------------------------------------------------------------

WX_DEFINE_LIST(wxDataViewColumnList)

IMPLEMENT_DYNAMIC_CLASS(wxDataViewCtrl, wxDataViewCtrlBase)
BEGIN_EVENT_TABLE(wxDataViewCtrl, wxDataViewCtrlBase)
    EVT_SIZE(wxDataViewCtrl::OnSize)
END_EVENT_TABLE()

wxDataViewCtrl::~wxDataViewCtrl()
{
    if (m_notifier)
        GetModel()->RemoveNotifier( m_notifier );

    m_cols.Clear();
    m_colsBestWidths.clear();
}

void wxDataViewCtrl::Init()
{
    m_cols.DeleteContents(true);
    m_notifier = NULL;

    // No sorting column at start
    m_sortingColumnIdx = wxNOT_FOUND;

    m_headerArea = NULL;
}

bool wxDataViewCtrl::Create(wxWindow *parent,
                            wxWindowID id,
                            const wxPoint& pos,
                            const wxSize& size,
                            long style,
                            const wxValidator& validator,
                            const wxString& name)
{
//    if ( (style & wxBORDER_MASK) == 0)
//        style |= wxBORDER_SUNKEN;

    Init();

    if (!wxControl::Create( parent, id, pos, size,
                            style | wxScrolledWindowStyle, validator, name))
        return false;

    SetInitialSize(size);

#ifdef __WXMAC__
    MacSetClipChildren( true );
#endif

    m_clientArea = new wxDataViewMainWindow( this, wxID_ANY );

    // We use the cursor keys for moving the selection, not scrolling, so call
    // this method to ensure wxScrollHelperEvtHandler doesn't catch all
    // keyboard events forwarded to us from wxListMainWindow.
    DisableKeyboardScrolling();

    if (HasFlag(wxDV_NO_HEADER))
        m_headerArea = NULL;
    else
        m_headerArea = new wxDataViewHeaderWindow(this);

    SetTargetWindow( m_clientArea );

    wxBoxSizer *sizer = new wxBoxSizer( wxVERTICAL );
    if (m_headerArea)
        sizer->Add( m_headerArea, 0, wxGROW );
    sizer->Add( m_clientArea, 1, wxGROW );
    SetSizer( sizer );

    return true;
}

wxBorder wxDataViewCtrl::GetDefaultBorder() const
{
    return wxBORDER_THEME;
}

#ifdef __WXMSW__
WXLRESULT wxDataViewCtrl::MSWWindowProc(WXUINT nMsg,
                                        WXWPARAM wParam,
                                        WXLPARAM lParam)
{
    WXLRESULT rc = wxDataViewCtrlBase::MSWWindowProc(nMsg, wParam, lParam);

#ifndef __WXWINCE__
    // we need to process arrows ourselves for scrolling
    if ( nMsg == WM_GETDLGCODE )
    {
        rc |= DLGC_WANTARROWS;
    }
#endif

    return rc;
}
#endif

wxSize wxDataViewCtrl::GetSizeAvailableForScrollTarget(const wxSize& size)
{
    wxSize newsize = size;
    if (!HasFlag(wxDV_NO_HEADER) && (m_headerArea))
    newsize.y -= m_headerArea->GetSize().y;

    return newsize;
}

void wxDataViewCtrl::OnSize( wxSizeEvent &WXUNUSED(event) )
{
    // We need to override OnSize so that our scrolled
    // window a) does call Layout() to use sizers for
    // positioning the controls but b) does not query
    // the sizer for their size and use that for setting
    // the scrollable area as set that ourselves by
    // calling SetScrollbar() further down.

    Layout();

    AdjustScrollbars();
}

void wxDataViewCtrl::SetFocus()
{
    if (m_clientArea)
        m_clientArea->SetFocus();
}

bool wxDataViewCtrl::AssociateModel( wxDataViewModel *model )
{
    if (!wxDataViewCtrlBase::AssociateModel( model ))
        return false;

    m_notifier = new wxGenericDataViewModelNotifier( m_clientArea );

    model->AddNotifier( m_notifier );

    m_clientArea->DestroyTree();

    m_clientArea->BuildTree(model);

    m_clientArea->UpdateDisplay();

    return true;
}

#if wxUSE_DRAG_AND_DROP

bool wxDataViewCtrl::EnableDragSource( const wxDataFormat &format )
{
    return m_clientArea->EnableDragSource( format );
}

bool wxDataViewCtrl::EnableDropTarget( const wxDataFormat &format )
{
    return m_clientArea->EnableDropTarget( format );
}

#endif // wxUSE_DRAG_AND_DROP

bool wxDataViewCtrl::AppendColumn( wxDataViewColumn *col )
{
    if (!wxDataViewCtrlBase::AppendColumn(col))
        return false;

    m_cols.Append( col );
    m_colsBestWidths.push_back(0);
    OnColumnsCountChanged();
    return true;
}

bool wxDataViewCtrl::PrependColumn( wxDataViewColumn *col )
{
    if (!wxDataViewCtrlBase::PrependColumn(col))
        return false;

    m_cols.Insert( col );
    m_colsBestWidths.insert(m_colsBestWidths.begin(), 0);
    OnColumnsCountChanged();
    return true;
}

bool wxDataViewCtrl::InsertColumn( unsigned int pos, wxDataViewColumn *col )
{
    if (!wxDataViewCtrlBase::InsertColumn(pos,col))
        return false;

    m_cols.Insert( pos, col );
    m_colsBestWidths.insert(m_colsBestWidths.begin() + pos, 0);
    OnColumnsCountChanged();
    return true;
}

void wxDataViewCtrl::OnColumnChange(unsigned int idx)
{
    if ( m_headerArea )
        m_headerArea->UpdateColumn(idx);

    m_clientArea->UpdateDisplay();
}

void wxDataViewCtrl::OnColumnsCountChanged()
{
    if (m_headerArea)
        m_headerArea->SetColumnCount(GetColumnCount());

    m_clientArea->UpdateDisplay();
}

void wxDataViewCtrl::DoSetExpanderColumn()
{
    m_clientArea->UpdateDisplay();
}

void wxDataViewCtrl::DoSetIndent()
{
    m_clientArea->UpdateDisplay();
}

unsigned int wxDataViewCtrl::GetColumnCount() const
{
    return m_cols.GetCount();
}

wxDataViewColumn* wxDataViewCtrl::GetColumn( unsigned int idx ) const
{
    return m_cols[idx];
}

wxDataViewColumn *wxDataViewCtrl::GetColumnAt(unsigned int pos) const
{
    // columns can't be reordered if there is no header window which allows
    // to do this
    const unsigned idx = m_headerArea ? m_headerArea->GetColumnsOrder()[pos]
                                    : pos;

    return GetColumn(idx);
}

int wxDataViewCtrl::GetColumnIndex(const wxDataViewColumn *column) const
{
    const unsigned count = m_cols.size();
    for ( unsigned n = 0; n < count; n++ )
    {
        if ( m_cols[n] == column )
            return n;
    }

    return wxNOT_FOUND;
}

unsigned int wxDataViewCtrl::GetBestColumnWidth(int idx) const
{
    if ( m_colsBestWidths[idx] != 0 )
        return m_colsBestWidths[idx];

    const int count = m_clientArea->GetRowCount();
    wxDataViewColumn *column = GetColumn(idx);
    wxDataViewRenderer *renderer =
        const_cast<wxDataViewRenderer*>(column->GetRenderer());

    class MaxWidthCalculator
    {
    public:
        MaxWidthCalculator(wxDataViewMainWindow *clientArea,
                           wxDataViewRenderer *renderer,
                           const wxDataViewModel *model,
                           unsigned column)
            : m_width(0),
              m_clientArea(clientArea),
              m_renderer(renderer),
              m_model(model),
              m_column(column)
        {
        }

        void UpdateWithWidth(int width)
        {
            m_width = wxMax(m_width, width);
        }

        void UpdateWithRow(int row)
        {
            wxDataViewItem item = m_clientArea->GetItemByRow(row);
            m_renderer->PrepareForItem(m_model, item, m_column);
            m_width = wxMax(m_width, m_renderer->GetSize().x);
        }

        int GetMaxWidth() const { return m_width; }

    private:
        int m_width;
        wxDataViewMainWindow *m_clientArea;
        wxDataViewRenderer *m_renderer;
        const wxDataViewModel *m_model;
        unsigned m_column;
    };

    MaxWidthCalculator calculator(m_clientArea, renderer,
                                  GetModel(), column->GetModelColumn());

    if ( m_headerArea )
    {
        int header_width = m_headerArea->GetTextExtent(column->GetTitle()).x;
        // Labels on native MSW header are indented on both sides
        header_width +=
            wxRendererNative::Get().GetHeaderButtonMargin(m_headerArea);
        calculator.UpdateWithWidth(header_width);
    }

    // The code below deserves some explanation. For very large controls, we
    // simply can't afford to calculate sizes for all items, it takes too
    // long. So the best we can do is to check the first and the last N/2
    // items in the control for some sufficiently large N and calculate best
    // sizes from that. That can result in the calculated best width being too
    // small for some outliers, but it's better to get slightly imperfect
    // result than to wait several seconds after every update. To avoid highly
    // visible miscalculations, we also include all currently visible items
    // no matter what.  Finally, the value of N is determined dynamically by
    // measuring how much time we spent on the determining item widths so far.

#if wxUSE_STOPWATCH
    int top_part_end = count;
    static const long CALC_TIMEOUT = 20/*ms*/;
    // don't call wxStopWatch::Time() too often
    static const unsigned CALC_CHECK_FREQ = 100;
    wxStopWatch timer;
#else
    // use some hard-coded limit, that's the best we can do without timer
    int top_part_end = wxMin(500, count);
#endif // wxUSE_STOPWATCH/!wxUSE_STOPWATCH

    int row = 0;

    for ( row = 0; row < top_part_end; row++ )
    {
#if wxUSE_STOPWATCH
        if ( row % CALC_CHECK_FREQ == CALC_CHECK_FREQ-1 &&
             timer.Time() > CALC_TIMEOUT )
            break;
#endif // wxUSE_STOPWATCH
        calculator.UpdateWithRow(row);
    }

    // row is the first unmeasured item now; that's our value of N/2

    if ( row < count )
    {
        top_part_end = row;

        // add bottom N/2 items now:
        const int bottom_part_start = wxMax(row, count - row);
        for ( row = bottom_part_start; row < count; row++ )
        {
            calculator.UpdateWithRow(row);
        }

        // finally, include currently visible items in the calculation:
        const wxPoint origin = CalcUnscrolledPosition(wxPoint(0, 0));
        int first_visible = m_clientArea->GetLineAt(origin.y);
        int last_visible = m_clientArea->GetLineAt(origin.y + GetClientSize().y);

        first_visible = wxMax(first_visible, top_part_end);
        last_visible = wxMin(bottom_part_start, last_visible);

        for ( row = first_visible; row < last_visible; row++ )
        {
            calculator.UpdateWithRow(row);
        }

        wxLogTrace("dataview",
                   "determined best size from %d top, %d bottom plus %d more visible items out of %d total",
                   top_part_end,
                   count - bottom_part_start,
                   wxMax(0, last_visible - first_visible),
                   count);
    }

    int max_width = calculator.GetMaxWidth();
    if ( max_width > 0 )
        max_width += 2 * PADDING_RIGHTLEFT;

    const_cast<wxDataViewCtrl*>(this)->m_colsBestWidths[idx] = max_width;
    return max_width;
}

void wxDataViewCtrl::ColumnMoved(wxDataViewColumn * WXUNUSED(col),
                                unsigned int WXUNUSED(new_pos))
{
    // do _not_ reorder m_cols elements here, they should always be in the
    // order in which columns were added, we only display the columns in
    // different order
    m_clientArea->UpdateDisplay();
}

bool wxDataViewCtrl::DeleteColumn( wxDataViewColumn *column )
{
    wxDataViewColumnList::compatibility_iterator ret = m_cols.Find( column );
    if (!ret)
        return false;

    m_colsBestWidths.erase(m_colsBestWidths.begin() + GetColumnIndex(column));
    m_cols.Erase(ret);
    OnColumnsCountChanged();

    return true;
}

bool wxDataViewCtrl::ClearColumns()
{
    m_cols.Clear();
    m_colsBestWidths.clear();
    OnColumnsCountChanged();
    return true;
}

void wxDataViewCtrl::InvalidateColBestWidth(int idx)
{
    m_colsBestWidths[idx] = 0;

    if ( m_headerArea )
        m_headerArea->UpdateColumn(idx);
}

void wxDataViewCtrl::InvalidateColBestWidths()
{
    m_colsBestWidths.clear();
    m_colsBestWidths.resize(m_cols.size());

    if ( m_headerArea )
    {
        const unsigned cols = m_headerArea->GetColumnCount();
        for ( unsigned i = 0; i < cols; i++ )
            m_headerArea->UpdateColumn(i);
    }
}

int wxDataViewCtrl::GetColumnPosition( const wxDataViewColumn *column ) const
{
#if 1
    unsigned int len = GetColumnCount();
    for ( unsigned int i = 0; i < len; i++ )
    {
        wxDataViewColumn * col = GetColumnAt(i);
        if (column==col)
            return i;
    }

    return wxNOT_FOUND;
#else
    // This returns the position in pixels which is not what we want.
    int ret = 0,
        dummy = 0;
    unsigned int len = GetColumnCount();
    for ( unsigned int i = 0; i < len; i++ )
    {
        wxDataViewColumn * col = GetColumnAt(i);
        if (col->IsHidden())
            continue;
        ret += col->GetWidth();
        if (column==col)
        {
            CalcScrolledPosition( ret, dummy, &ret, &dummy );
            break;
        }
    }
    return ret;
#endif
}

wxDataViewColumn *wxDataViewCtrl::GetSortingColumn() const
{
    return m_sortingColumnIdx == wxNOT_FOUND ? NULL
                                            : GetColumn(m_sortingColumnIdx);
}

wxDataViewItem wxDataViewCtrl::DoGetCurrentItem() const
{
    return GetItemByRow(m_clientArea->GetCurrentRow());
}

void wxDataViewCtrl::DoSetCurrentItem(const wxDataViewItem& item)
{
    const int row = m_clientArea->GetRowByItem(item);

    const unsigned oldCurrent = m_clientArea->GetCurrentRow();
    if ( static_cast<unsigned>(row) != oldCurrent )
    {
        m_clientArea->ChangeCurrentRow(row);
        m_clientArea->RefreshRow(oldCurrent);
        m_clientArea->RefreshRow(row);
    }
}

// Selection code with wxDataViewItem as parameters
wxDataViewItem wxDataViewCtrl::GetSelection() const
{
    return m_clientArea->GetSelection();
}

int wxDataViewCtrl::GetSelections( wxDataViewItemArray & sel ) const
{
    sel.Empty();
    wxDataViewSelection selection = m_clientArea->GetSelections();
    int len = selection.GetCount();
    for( int i = 0; i < len; i ++)
    {
        unsigned int row = selection[i];
        sel.Add( m_clientArea->GetItemByRow( row ) );
    }
    return len;
}

void wxDataViewCtrl::SetSelections( const wxDataViewItemArray & sel )
{
    wxDataViewSelection selection(wxDataViewSelectionCmp);

    wxDataViewItem last_parent;

    int len = sel.GetCount();
    for( int i = 0; i < len; i ++ )
    {
        wxDataViewItem item = sel[i];
        wxDataViewItem parent = GetModel()->GetParent( item );
        if (parent)
        {
            if (parent != last_parent)
                ExpandAncestors(item);
        }

        last_parent = parent;
        int row = m_clientArea->GetRowByItem( item );
        if( row >= 0 )
            selection.Add( static_cast<unsigned int>(row) );
    }

    m_clientArea->SetSelections( selection );
}

void wxDataViewCtrl::Select( const wxDataViewItem & item )
{
    ExpandAncestors( item );

    int row = m_clientArea->GetRowByItem( item );
    if( row >= 0 )
    {
        // Unselect all rows before select another in the single select mode
        if (m_clientArea->IsSingleSel())
            m_clientArea->SelectAllRows(false);

        m_clientArea->SelectRow(row, true);

        // Also set focus to the selected item
        m_clientArea->ChangeCurrentRow( row );
    }
}

void wxDataViewCtrl::Unselect( const wxDataViewItem & item )
{
    int row = m_clientArea->GetRowByItem( item );
    if( row >= 0 )
        m_clientArea->SelectRow(row, false);
}

bool wxDataViewCtrl::IsSelected( const wxDataViewItem & item ) const
{
    int row = m_clientArea->GetRowByItem( item );
    if( row >= 0 )
    {
        return m_clientArea->IsRowSelected(row);
    }
    return false;
}

// Selection code with row number as parameter
int wxDataViewCtrl::GetSelections( wxArrayInt & sel ) const
{
    sel.Empty();
    wxDataViewSelection selection = m_clientArea->GetSelections();
    int len = selection.GetCount();
    for( int i = 0; i < len; i ++)
    {
        unsigned int row = selection[i];
        sel.Add( row );
    }
    return len;
}

void wxDataViewCtrl::SetSelections( const wxArrayInt & sel )
{
    wxDataViewSelection selection(wxDataViewSelectionCmp);
    int len = sel.GetCount();
    for( int i = 0; i < len; i ++ )
    {
        int row = sel[i];
        if( row >= 0 )
            selection.Add( static_cast<unsigned int>(row) );
    }
    m_clientArea->SetSelections( selection );
}

void wxDataViewCtrl::Select( int row )
{
    if( row >= 0 )
    {
        if (m_clientArea->IsSingleSel())
            m_clientArea->SelectAllRows(false);
        m_clientArea->SelectRow( row, true );
    }
}

void wxDataViewCtrl::Unselect( int row )
{
    if( row >= 0 )
        m_clientArea->SelectRow(row, false);
}

bool wxDataViewCtrl::IsSelected( int row ) const
{
    if( row >= 0 )
        return m_clientArea->IsRowSelected(row);
    return false;
}

void wxDataViewCtrl::SelectRange( int from, int to )
{
    wxArrayInt sel;
    for( int i = from; i < to; i ++ )
        sel.Add( i );
    m_clientArea->Select(sel);
}

void wxDataViewCtrl::UnselectRange( int from, int to )
{
    wxDataViewSelection sel = m_clientArea->GetSelections();
    for( int i = from; i < to; i ++ )
        if( sel.Index( i ) != wxNOT_FOUND )
            sel.Remove( i );
    m_clientArea->SetSelections(sel);
}

void wxDataViewCtrl::SelectAll()
{
    m_clientArea->SelectAllRows(true);
}

void wxDataViewCtrl::UnselectAll()
{
    m_clientArea->SelectAllRows(false);
}

void wxDataViewCtrl::EnsureVisible( int row, int column )
{
    if( row < 0 )
        row = 0;
    if( row > (int) m_clientArea->GetRowCount() )
        row = m_clientArea->GetRowCount();

    int first = m_clientArea->GetFirstVisibleRow();
    int last = m_clientArea->GetLastVisibleRow();
    if( row < first )
        m_clientArea->ScrollTo( row, column );
    else if( row > last )
        m_clientArea->ScrollTo( row - last + first, column );
    else
        m_clientArea->ScrollTo( first, column );
}

void wxDataViewCtrl::EnsureVisible( const wxDataViewItem & item, const wxDataViewColumn * column )
{
    ExpandAncestors( item );

    m_clientArea->RecalculateDisplay();

    int row = m_clientArea->GetRowByItem(item);
    if( row >= 0 )
    {
        if( column == NULL )
            EnsureVisible(row, -1);
        else
            EnsureVisible( row, GetColumnIndex(column) );
    }

}

void wxDataViewCtrl::HitTest( const wxPoint & point, wxDataViewItem & item,
                              wxDataViewColumn* &column ) const
{
    m_clientArea->HitTest(point, item, column);
}

wxRect wxDataViewCtrl::GetItemRect( const wxDataViewItem & item,
                                    const wxDataViewColumn* column ) const
{
    return m_clientArea->GetItemRect(item, column);
}

wxDataViewItem wxDataViewCtrl::GetItemByRow( unsigned int row ) const
{
    return m_clientArea->GetItemByRow( row );
}

int wxDataViewCtrl::GetRowByItem( const wxDataViewItem & item ) const
{
    return m_clientArea->GetRowByItem( item );
}

void wxDataViewCtrl::Expand( const wxDataViewItem & item )
{
    ExpandAncestors( item );

    int row = m_clientArea->GetRowByItem( item );
    if (row != -1)
        m_clientArea->Expand(row);
}

void wxDataViewCtrl::Collapse( const wxDataViewItem & item )
{
    int row = m_clientArea->GetRowByItem( item );
    if (row != -1)
        m_clientArea->Collapse(row);
}

bool wxDataViewCtrl::IsExpanded( const wxDataViewItem & item ) const
{
    int row = m_clientArea->GetRowByItem( item );
    if (row != -1)
        return m_clientArea->IsExpanded(row);
    return false;
}

void wxDataViewCtrl::StartEditor( const wxDataViewItem & item, unsigned int column )
{
    wxDataViewColumn* col = GetColumn( column );
    if (!col)
        return;

    wxRect itemRect = GetItemRect(item, col);
    wxDataViewRenderer* renderer = col->GetRenderer();
    if (renderer->GetMode() == wxDATAVIEW_CELL_EDITABLE)
        renderer->StartEditing(item, itemRect);
}

#endif // !wxUSE_GENERICDATAVIEWCTRL

#endif // wxUSE_DATAVIEWCTRL
