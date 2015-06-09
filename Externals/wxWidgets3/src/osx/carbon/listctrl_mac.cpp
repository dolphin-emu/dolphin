/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/listctrl_mac.cpp
// Purpose:     wxListCtrl
// Author:      Julian Smart
// Modified by: Agron Selimaj
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_LISTCTRL

#include "wx/listctrl.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/settings.h"
#endif

#include "wx/osx/uma.h"

#include "wx/imaglist.h"
#include "wx/sysopt.h"
#include "wx/timer.h"

#include "wx/hashmap.h"

WX_DECLARE_HASH_MAP( int, wxListItem*, wxIntegerHash, wxIntegerEqual, wxListItemList );

#include "wx/listimpl.cpp"
WX_DEFINE_LIST(wxColumnList)

// so we can check for column clicks
static const EventTypeSpec eventList[] =
{
    { kEventClassControl, kEventControlHit },
    { kEventClassControl, kEventControlDraw }
};

static pascal OSStatus wxMacListCtrlEventHandler( EventHandlerCallRef handler , EventRef event , void *data )
{
    OSStatus result = eventNotHandledErr ;

    wxMacCarbonEvent cEvent( event ) ;

    ControlRef controlRef ;
    cEvent.GetParameter( kEventParamDirectObject , &controlRef ) ;

    wxListCtrl *window = (wxListCtrl*) data ;
    wxListEvent le( wxEVT_LIST_COL_CLICK, window->GetId() );
    le.SetEventObject( window );

    switch ( GetEventKind( event ) )
    {
        // check if the column was clicked on and fire an event if so
        case kEventControlHit :
            {
                ControlPartCode result = cEvent.GetParameter<ControlPartCode>(kEventParamControlPart, typeControlPartCode) ;
                if (result == kControlButtonPart){
                    DataBrowserPropertyID col;
                    GetDataBrowserSortProperty(controlRef, &col);

                    DataBrowserTableViewColumnIndex column = 0;
                    verify_noerr( GetDataBrowserTableViewColumnPosition( controlRef, col, &column ) );

                    le.m_col = column;
                    // FIXME: we can't use the sort property for virtual listctrls
                    // so we need to find a better way to determine which column was clicked...
                    if (!window->IsVirtual())
                        window->HandleWindowEvent( le );
                }
                result = CallNextEventHandler(handler, event);
                break;
            }
        case kEventControlDraw:
            {
                CGContextRef context = cEvent.GetParameter<CGContextRef>(kEventParamCGContextRef, typeCGContextRef) ;
                window->MacSetDrawingContext(context);
                result = CallNextEventHandler(handler, event);
                window->MacSetDrawingContext(NULL);
                break;
            }
        default :
            break ;
    }


    return result ;
}

DEFINE_ONE_SHOT_HANDLER_GETTER( wxMacListCtrlEventHandler )

class wxMacListCtrlItem : public wxMacDataItem
{
public:
    wxMacListCtrlItem();

    virtual void Notification(wxMacDataItemBrowserControl *owner ,
        DataBrowserItemNotification message,
        DataBrowserItemDataRef itemData ) const;

    virtual void SetColumnInfo( unsigned int column, wxListItem* item );
    virtual wxListItem* GetColumnInfo( unsigned int column );
    virtual bool HasColumnInfo( unsigned int column );

    virtual void SetColumnTextValue( unsigned int column, const wxString& text );
    virtual wxString GetColumnTextValue( unsigned int column );

    virtual int GetColumnImageValue( unsigned int column );
    virtual void SetColumnImageValue( unsigned int column, int imageIndex );

    virtual ~wxMacListCtrlItem();
protected:
    wxListItemList m_rowItems;
};

DataBrowserDrawItemUPP gDataBrowserDrawItemUPP = NULL;
//DataBrowserEditItemUPP gDataBrowserEditItemUPP = NULL;
DataBrowserHitTestUPP gDataBrowserHitTestUPP = NULL;

// TODO: Make a better name!!
class wxMacDataBrowserListCtrlControl : public wxMacDataItemBrowserControl
{
public:
    wxMacDataBrowserListCtrlControl( wxWindow *peer, const wxPoint& pos, const wxSize& size, long style );
    wxMacDataBrowserListCtrlControl() {}
    virtual ~wxMacDataBrowserListCtrlControl();

    // create a list item (can be a subclass of wxMacListBoxItem)

    virtual void MacInsertItem( unsigned int n, wxListItem* item );
    virtual void MacSetColumnInfo( unsigned int row, unsigned int column, wxListItem* item );
    virtual void MacGetColumnInfo( unsigned int row, unsigned int column, wxListItem& item );
    virtual void UpdateState(wxMacDataItem* dataItem, wxListItem* item);
    int GetFlags() { return m_flags; }

protected:
    // we need to override to provide specialized handling for virtual wxListCtrls
    virtual OSStatus GetSetItemData(DataBrowserItemID itemID,
                        DataBrowserPropertyID property,
                        DataBrowserItemDataRef itemData,
                        Boolean changeValue );

    virtual void    ItemNotification(
                        DataBrowserItemID itemID,
                        DataBrowserItemNotification message,
                        DataBrowserItemDataRef itemData);

    virtual Boolean CompareItems(DataBrowserItemID itemOneID,
                        DataBrowserItemID itemTwoID,
                        DataBrowserPropertyID sortProperty);

    static pascal void    DataBrowserDrawItemProc(ControlRef browser,
                        DataBrowserItemID item,
                        DataBrowserPropertyID property,
                        DataBrowserItemState itemState,
                        const Rect *theRect,
                        SInt16 gdDepth,
                        Boolean colorDevice);

    virtual void        DrawItem(DataBrowserItemID itemID,
                            DataBrowserPropertyID property,
                            DataBrowserItemState itemState,
                            const Rect *itemRect,
                            SInt16 gdDepth,
                            Boolean colorDevice);

    static pascal Boolean  DataBrowserEditTextProc(ControlRef browser,
                                    DataBrowserItemID item,
                                    DataBrowserPropertyID property,
                                    CFStringRef theString,
                                    Rect *maxEditTextRect,
                                    Boolean *shrinkToFit);

    static pascal Boolean  DataBrowserHitTestProc(ControlRef WXUNUSED(browser),
                                    DataBrowserItemID WXUNUSED(itemID),
                                    DataBrowserPropertyID WXUNUSED(property),
                                    const Rect *WXUNUSED(theRect),
                                    const Rect *WXUNUSED(mouseRect)) { return true; }

    virtual bool        ConfirmEditText(DataBrowserItemID item,
                                    DataBrowserPropertyID property,
                                    CFStringRef theString,
                                    Rect *maxEditTextRect,
                                    Boolean *shrinkToFit);



    wxClientDataType m_clientDataItemsType;
    bool m_isVirtual;
    int m_flags;
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxMacDataBrowserListCtrlControl)
};

class wxMacListCtrlEventDelegate : public wxEvtHandler
{
public:
    wxMacListCtrlEventDelegate( wxListCtrl* list, int id );
    virtual bool ProcessEvent( wxEvent& event );

private:
    wxListCtrl* m_list;
    int         m_id;
};

wxMacListCtrlEventDelegate::wxMacListCtrlEventDelegate( wxListCtrl* list, int id )
{
    m_list = list;
    m_id = id;
}

bool wxMacListCtrlEventDelegate::ProcessEvent( wxEvent& event )
{
    int id = event.GetId();
    wxObject* obj = event.GetEventObject();

    // even though we use a generic list ctrl underneath, make sure
    // we present ourselves as wxListCtrl.
    event.SetEventObject( m_list );
    event.SetId( m_id );

    if ( !event.IsKindOf( CLASSINFO( wxCommandEvent ) ) )
    {
        if (m_list->GetEventHandler()->ProcessEvent( event ))
        {
            event.SetId(id);
            event.SetEventObject(obj);
            return true;
        }
    }
    // Also try with the original id
    bool success = wxEvtHandler::ProcessEvent(event);
    event.SetId(id);
    event.SetEventObject(obj);
    if (!success && id != m_id)
        success = wxEvtHandler::ProcessEvent(event);
    return success;
}

//-----------------------------------------------------------------------------
// wxListCtrlRenameTimer (internal)
//-----------------------------------------------------------------------------

class wxListCtrlRenameTimer: public wxTimer
{
private:
    wxListCtrl *m_owner;

public:
    wxListCtrlRenameTimer( wxListCtrl *owner );
    void Notify();
};

//-----------------------------------------------------------------------------
// wxListCtrlTextCtrlWrapper: wraps a wxTextCtrl to make it work for inline editing
//-----------------------------------------------------------------------------

class wxListCtrlTextCtrlWrapper : public wxEvtHandler
{
public:
    // NB: text must be a valid object but not Create()d yet
    wxListCtrlTextCtrlWrapper(wxListCtrl *owner,
                          wxTextCtrl *text,
                          long itemEdit);

    wxTextCtrl *GetText() const { return m_text; }

    void AcceptChangesAndFinish();

protected:
    void OnChar( wxKeyEvent &event );
    void OnKeyUp( wxKeyEvent &event );
    void OnKillFocus( wxFocusEvent &event );

    bool AcceptChanges();
    void Finish();

private:
    wxListCtrl         *m_owner;
    wxTextCtrl         *m_text;
    wxString            m_startValue;
    long              m_itemEdited;
    bool                m_finished;
    bool                m_aboutToFinish;

    DECLARE_EVENT_TABLE()
};

//-----------------------------------------------------------------------------
// wxListCtrlRenameTimer (internal)
//-----------------------------------------------------------------------------

wxListCtrlRenameTimer::wxListCtrlRenameTimer( wxListCtrl *owner )
{
    m_owner = owner;
}

void wxListCtrlRenameTimer::Notify()
{
    m_owner->OnRenameTimer();
}

//-----------------------------------------------------------------------------
// wxListCtrlTextCtrlWrapper (internal)
//-----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxListCtrlTextCtrlWrapper, wxEvtHandler)
    EVT_CHAR           (wxListCtrlTextCtrlWrapper::OnChar)
    EVT_KEY_UP         (wxListCtrlTextCtrlWrapper::OnKeyUp)
    EVT_KILL_FOCUS     (wxListCtrlTextCtrlWrapper::OnKillFocus)
END_EVENT_TABLE()

wxListCtrlTextCtrlWrapper::wxListCtrlTextCtrlWrapper(wxListCtrl *owner,
                                             wxTextCtrl *text,
                                             long itemEdit)
              : m_startValue(owner->GetItemText(itemEdit)),
                m_itemEdited(itemEdit)
{
    m_owner = owner;
    m_text = text;
    m_finished = false;
    m_aboutToFinish = false;

    wxRect rectLabel;
    int offset = 8;
    owner->GetItemRect(itemEdit, rectLabel);

    m_text->Create(owner, wxID_ANY, m_startValue,
                   wxPoint(rectLabel.x+offset,rectLabel.y),
                   wxSize(rectLabel.width-offset,rectLabel.height));
    m_text->SetFocus();

    m_text->PushEventHandler(this);
}

void wxListCtrlTextCtrlWrapper::Finish()
{
    if ( !m_finished )
    {
        m_finished = true;

        m_text->RemoveEventHandler(this);
        m_owner->FinishEditing(m_text);

        wxPendingDelete.Append( this );
    }
}

bool wxListCtrlTextCtrlWrapper::AcceptChanges()
{
    const wxString value = m_text->GetValue();

    if ( value == m_startValue )
        // nothing changed, always accept
        return true;

    if ( !m_owner->OnRenameAccept(m_itemEdited, value) )
        // vetoed by the user
        return false;

    // accepted, do rename the item
    m_owner->SetItemText(m_itemEdited, value);

    return true;
}

void wxListCtrlTextCtrlWrapper::AcceptChangesAndFinish()
{
    m_aboutToFinish = true;

    // Notify the owner about the changes
    AcceptChanges();

    // Even if vetoed, close the control (consistent with MSW)
    Finish();
}

void wxListCtrlTextCtrlWrapper::OnChar( wxKeyEvent &event )
{
    switch ( event.m_keyCode )
    {
        case WXK_RETURN:
            AcceptChangesAndFinish();
            break;

        case WXK_ESCAPE:
            m_owner->OnRenameCancelled( m_itemEdited );
            Finish();
            break;

        default:
            event.Skip();
    }
}

void wxListCtrlTextCtrlWrapper::OnKeyUp( wxKeyEvent &event )
{
    if (m_finished)
    {
        event.Skip();
        return;
    }

    // auto-grow the textctrl:
    wxSize parentSize = m_owner->GetSize();
    wxPoint myPos = m_text->GetPosition();
    wxSize mySize = m_text->GetSize();
    int sx, sy;
    m_text->GetTextExtent(m_text->GetValue() + wxT("MM"), &sx, &sy);
    if (myPos.x + sx > parentSize.x)
        sx = parentSize.x - myPos.x;
    if (mySize.x > sx)
        sx = mySize.x;
    m_text->SetSize(sx, wxDefaultCoord);

    event.Skip();
}

void wxListCtrlTextCtrlWrapper::OnKillFocus( wxFocusEvent &event )
{
    if ( !m_finished && !m_aboutToFinish )
    {
        if ( !AcceptChanges() )
            m_owner->OnRenameCancelled( m_itemEdited );

        Finish();
    }

    // We must let the native text control handle focus
    event.Skip();
}

// ============================================================================
// implementation
// ============================================================================

wxMacDataBrowserListCtrlControl* wxListCtrl::GetListPeer() const
{
    return dynamic_cast<wxMacDataBrowserListCtrlControl*> ( GetPeer() );
}

// ----------------------------------------------------------------------------
// wxListCtrl construction
// ----------------------------------------------------------------------------

void wxListCtrl::Init()
{
    m_imageListNormal = NULL;
    m_imageListSmall = NULL;
    m_imageListState = NULL;

    // keep track of if we created our own image lists, or if they were assigned
    // to us.
    m_ownsImageListNormal = m_ownsImageListSmall = m_ownsImageListState = false;
    m_colCount = 0;
    m_count = 0;
    m_textCtrl = NULL;
    m_genericImpl = NULL;
    m_dbImpl = NULL;
    m_compareFunc = NULL;
    m_compareFuncData = 0;
    m_colsInfo = wxColumnList();
    m_textColor = wxNullColour;
    m_bgColor = wxNullColour;
    m_textctrlWrapper = NULL;
    m_current = -1;
    m_renameTimer = NULL;
}

class wxGenericListCtrlHook : public wxGenericListCtrl
{
public:
    wxGenericListCtrlHook(wxListCtrl* parent,
                          wxWindowID id,
                          const wxPoint& pos,
                          const wxSize& size,
                          long style,
                          const wxValidator& validator,
                          const wxString& name)
        : wxGenericListCtrl(parent, id, pos, size, style, validator, name),
          m_nativeListCtrl(parent)
    {
    }

protected:
    virtual wxListItemAttr * OnGetItemAttr(long item) const
    {
        return m_nativeListCtrl->OnGetItemAttr(item);
    }

    virtual int OnGetItemImage(long item) const
    {
        return m_nativeListCtrl->OnGetItemImage(item);
    }

    virtual int OnGetItemColumnImage(long item, long column) const
    {
        return m_nativeListCtrl->OnGetItemColumnImage(item, column);
    }

    virtual wxString OnGetItemText(long item, long column) const
    {
        return m_nativeListCtrl->OnGetItemText(item, column);
    }

    wxListCtrl* m_nativeListCtrl;

};

void wxListCtrl::OnLeftDown(wxMouseEvent& event)
{
    if ( m_textctrlWrapper )
    {
        m_current = -1;
        m_textctrlWrapper->AcceptChangesAndFinish();
    }

    int hitResult;
    long current = HitTest(event.GetPosition(), hitResult);
    if ((current == m_current) &&
        (hitResult & wxLIST_HITTEST_ONITEMLABEL) &&
        HasFlag(wxLC_EDIT_LABELS) )
    {
        if ( m_renameTimer )
            m_renameTimer->Start( 250, true );
    }
    else
    {
        m_current = current;
    }
    event.Skip();
}

void wxListCtrl::OnDblClick(wxMouseEvent& event)
{
    if ( m_renameTimer && m_renameTimer->IsRunning() )
        m_renameTimer->Stop();
    event.Skip();
}

#if wxABI_VERSION >= 20801
void wxListCtrl::OnRightDown(wxMouseEvent& event)
{
    if (m_dbImpl)
        FireMouseEvent(wxEVT_LIST_ITEM_RIGHT_CLICK, event.GetPosition());
    event.Skip();
}

void wxListCtrl::OnMiddleDown(wxMouseEvent& event)
{
    if (m_dbImpl)
        FireMouseEvent(wxEVT_LIST_ITEM_MIDDLE_CLICK, event.GetPosition());
    event.Skip();
}

void wxListCtrl::FireMouseEvent(wxEventType eventType, wxPoint position)
{
    wxListEvent le( eventType, GetId() );
    le.SetEventObject(this);
    le.m_pointDrag = position;
    le.m_itemIndex = -1;

    int flags;
    long item = HitTest(position, flags);
    if (flags & wxLIST_HITTEST_ONITEM)
    {
        le.m_itemIndex = item;
        le.m_item.m_itemId = item;
        GetItem(le.m_item);
        HandleWindowEvent(le);
    }
}

void wxListCtrl::OnChar(wxKeyEvent& event)
{


    if (m_dbImpl)
    {
        wxListEvent le( wxEVT_LIST_KEY_DOWN, GetId() );
        le.SetEventObject(this);
        le.m_code = event.GetKeyCode();
        le.m_itemIndex = -1;

        if (m_current == -1)
        {
            // if m_current isn't set, check if there's been a selection
            // made before continuing
            m_current = GetNextItem(-1, wxLIST_NEXT_BELOW, wxLIST_STATE_SELECTED);
        }

        // We need to determine m_current ourselves when navigation keys
        // are used. Note that PAGEUP and PAGEDOWN do not alter the current
        // item on native Mac ListCtrl, so we only handle up and down keys.
        switch ( event.GetKeyCode() )
        {
            case WXK_UP:
                if ( m_current > 0 )
                    m_current -= 1;
                else
                    m_current = 0;

                break;

            case WXK_DOWN:
                if ( m_current < GetItemCount() - 1 )
                    m_current += 1;
                else
                    m_current = GetItemCount() - 1;

                break;
        }

        if (m_current != -1)
        {
            le.m_itemIndex = m_current;
            le.m_item.m_itemId = m_current;
            GetItem(le.m_item);
            HandleWindowEvent(le);
        }
    }
    event.Skip();
}
#endif

bool wxListCtrl::Create(wxWindow *parent,
                        wxWindowID id,
                        const wxPoint& pos,
                        const wxSize& size,
                        long style,
                        const wxValidator& validator,
                        const wxString& name)
{

    // for now, we'll always use the generic list control for ICON and LIST views,
    // because they dynamically change the number of columns on resize.
    // Also, allow the user to set it to use the list ctrl as well.
    if ( (wxSystemOptions::HasOption( wxMAC_ALWAYS_USE_GENERIC_LISTCTRL )
            && (wxSystemOptions::GetOptionInt( wxMAC_ALWAYS_USE_GENERIC_LISTCTRL ) == 1)) ||
            (style & wxLC_ICON) || (style & wxLC_SMALL_ICON) || (style & wxLC_LIST) )
    {
        long paneStyle = style;
        paneStyle &= ~wxSIMPLE_BORDER;
        paneStyle &= ~wxDOUBLE_BORDER;
        paneStyle &= ~wxSUNKEN_BORDER;
        paneStyle &= ~wxRAISED_BORDER;
        paneStyle &= ~wxSTATIC_BORDER;
        if ( !wxWindow::Create(parent, id, pos, size, paneStyle | wxNO_BORDER, name) )
            return false;

        // since the generic control is a child, make sure we position it at 0, 0
        m_genericImpl = new wxGenericListCtrlHook(this, id, wxPoint(0, 0), size, style, validator, name);
        m_genericImpl->PushEventHandler( new wxMacListCtrlEventDelegate( this, GetId() ) );
        return true;
    }

    else
    {
        DontCreatePeer();
        if ( !wxWindow::Create(parent, id, pos, size, style & ~(wxHSCROLL | wxVSCROLL), name) )
            return false;
        m_dbImpl = new wxMacDataBrowserListCtrlControl( this, pos, size, style );
        SetPeer(m_dbImpl);

        MacPostControlCreate( pos, size );

        InstallControlEventHandler( GetPeer()->GetControlRef() , GetwxMacListCtrlEventHandlerUPP(),
            GetEventTypeCount(eventList), eventList, this,
            (EventHandlerRef *)&m_macListCtrlEventHandler);

        m_renameTimer = new wxListCtrlRenameTimer( this );

        Connect( wxID_ANY, wxEVT_CHAR, wxCharEventHandler(wxListCtrl::OnChar), NULL, this );
        Connect( wxID_ANY, wxEVT_LEFT_DOWN, wxMouseEventHandler(wxListCtrl::OnLeftDown), NULL, this );
        Connect( wxID_ANY, wxEVT_LEFT_DCLICK, wxMouseEventHandler(wxListCtrl::OnDblClick), NULL, this );
        Connect( wxID_ANY, wxEVT_MIDDLE_DOWN, wxMouseEventHandler(wxListCtrl::OnMiddleDown), NULL, this );
        Connect( wxID_ANY, wxEVT_RIGHT_DOWN, wxMouseEventHandler(wxListCtrl::OnRightDown), NULL, this );
    }

    return true;
}

wxListCtrl::~wxListCtrl()
{
    if (m_genericImpl)
    {
        m_genericImpl->PopEventHandler(/* deleteHandler = */ true);
    }

    if (m_ownsImageListNormal)
        delete m_imageListNormal;
    if (m_ownsImageListSmall)
        delete m_imageListSmall;
    if (m_ownsImageListState)
        delete m_imageListState;

    delete m_renameTimer;

    WX_CLEAR_LIST(wxColumnList, m_colsInfo);
}

/*static*/
wxVisualAttributes
wxListCtrl::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    wxVisualAttributes attr;

    attr.colFg = wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOWTEXT );
    attr.colBg = wxSystemSettings::GetColour( wxSYS_COLOUR_LISTBOX );
    static wxFont font = wxFont(wxOSX_SYSTEM_FONT_VIEWS);
    attr.font = font;

    return attr;
}

// ----------------------------------------------------------------------------
// set/get/change style
// ----------------------------------------------------------------------------

// Add or remove a single window style
void wxListCtrl::SetSingleStyle(long style, bool add)
{
    long flag = GetWindowStyleFlag();

    // Get rid of conflicting styles
    if ( add )
    {
        if ( style & wxLC_MASK_TYPE)
            flag = flag & ~wxLC_MASK_TYPE;
        if ( style & wxLC_MASK_ALIGN )
            flag = flag & ~wxLC_MASK_ALIGN;
        if ( style & wxLC_MASK_SORT )
            flag = flag & ~wxLC_MASK_SORT;
    }

    if ( add )
        flag |= style;
    else
        flag &= ~style;

    SetWindowStyleFlag(flag);
}

// Set the whole window style
void wxListCtrl::SetWindowStyleFlag(long flag)
{
    if ( flag != m_windowStyle )
    {
        m_windowStyle = flag;

        if (m_genericImpl)
        {
            m_genericImpl->SetWindowStyleFlag(flag);
        }

        Refresh();
    }
}

void wxListCtrl::DoSetSize( int x, int y, int width, int height, int sizeFlags )
{
    wxListCtrlBase::DoSetSize(x, y, width, height, sizeFlags);

    if (m_genericImpl)
        m_genericImpl->SetSize(0, 0, width, height, sizeFlags);

    // determine if we need a horizontal scrollbar, and add it if so
    if (m_dbImpl)
    {
        int totalWidth = 0;
        for (int column = 0; column < GetColumnCount(); column++)
        {
              totalWidth += m_dbImpl->GetColumnWidth( column );
        }

        if ( !(m_dbImpl->GetFlags() & wxHSCROLL) )
        {
            Boolean vertScrollBar;
            GetDataBrowserHasScrollBars( m_dbImpl->GetControlRef(), NULL, &vertScrollBar );
            if (totalWidth > width)
                SetDataBrowserHasScrollBars( m_dbImpl->GetControlRef(), true, vertScrollBar );
            else
                SetDataBrowserHasScrollBars( m_dbImpl->GetControlRef(), false, vertScrollBar );
        }
    }
}

bool wxListCtrl::SetFont(const wxFont& font)
{
    bool rv = wxListCtrlBase::SetFont(font);
    if (m_genericImpl)
        rv = m_genericImpl->SetFont(font);
    return rv;
}

bool wxListCtrl::SetForegroundColour(const wxColour& colour)
{
    bool rv = true;
    if (m_genericImpl)
        rv = m_genericImpl->SetForegroundColour(colour);
    if (m_dbImpl)
        SetTextColour(colour);
    return rv;
}

bool wxListCtrl::SetBackgroundColour(const wxColour& colour)
{
    bool rv = true;
    if (m_genericImpl)
        rv = m_genericImpl->SetBackgroundColour(colour);
    if (m_dbImpl)
        m_bgColor = colour;
    return rv;
}

wxColour wxListCtrl::GetBackgroundColour() const
{
    if (m_genericImpl)
        return m_genericImpl->GetBackgroundColour();
    if (m_dbImpl)
        return m_bgColor;

    return wxNullColour;
}

void wxListCtrl::Freeze ()
{
    if (m_genericImpl)
        m_genericImpl->Freeze();
    wxListCtrlBase::Freeze();
}

void wxListCtrl::Thaw ()
{
    if (m_genericImpl)
        m_genericImpl->Thaw();
    wxListCtrlBase::Thaw();
}

void wxListCtrl::Update ()
{
    if (m_genericImpl)
        m_genericImpl->Update();
    wxListCtrlBase::Update();
}

// ----------------------------------------------------------------------------
// accessors
// ----------------------------------------------------------------------------

// Gets information about this column
bool wxListCtrl::GetColumn(int col, wxListItem& item) const
{
    if (m_genericImpl)
        return m_genericImpl->GetColumn(col, item);

    bool success = true;

    if (m_dbImpl)
    {
        wxColumnList::compatibility_iterator node = m_colsInfo.Item( col );
        wxASSERT_MSG( node, wxT("invalid column index in wxMacListCtrlItem") );
        wxListItem* column = node->GetData();

        long mask = column->GetMask();
        if (mask & wxLIST_MASK_TEXT)
            item.SetText(column->GetText());
        if (mask & wxLIST_MASK_DATA)
            item.SetData(column->GetData());
        if (mask & wxLIST_MASK_IMAGE)
            item.SetImage(column->GetImage());
        if (mask & wxLIST_MASK_STATE)
            item.SetState(column->GetState());
        if (mask & wxLIST_MASK_FORMAT)
            item.SetAlign(column->GetAlign());
        if (mask & wxLIST_MASK_WIDTH)
            item.SetWidth(column->GetWidth());
    }

    return success;
}

// Sets information about this column
bool wxListCtrl::SetColumn(int col, const wxListItem& item)
{
    if (m_genericImpl)
        return m_genericImpl->SetColumn(col, item);

    if (m_dbImpl)
    {
        wxASSERT_MSG( col < (int)m_colsInfo.GetCount(), wxT("invalid column index in wxMacListCtrlItem") );

        long mask = item.GetMask();
        {
            wxListItem listItem;
            GetColumn( col, listItem );

            if (mask & wxLIST_MASK_TEXT)
                listItem.SetText(item.GetText());
            if (mask & wxLIST_MASK_DATA)
                listItem.SetData(item.GetData());
            if (mask & wxLIST_MASK_IMAGE)
                listItem.SetImage(item.GetImage());
            if (mask & wxLIST_MASK_STATE)
                listItem.SetState(item.GetState());
            if (mask & wxLIST_MASK_FORMAT)
                listItem.SetAlign(item.GetAlign());
            if (mask & wxLIST_MASK_WIDTH)
                listItem.SetWidth(item.GetWidth());
        }

        // change the appearance in the databrowser.
        DataBrowserListViewHeaderDesc columnDesc;
        columnDesc.version=kDataBrowserListViewLatestHeaderDesc;

        DataBrowserTableViewColumnID id = 0;
        verify_noerr( m_dbImpl->GetColumnIDFromIndex( col, &id ) );
        verify_noerr( m_dbImpl->GetHeaderDesc( id, &columnDesc ) );

        /*
        if (item.GetMask() & wxLIST_MASK_TEXT)
        {
            wxFontEncoding enc;
            if ( m_font.IsOk() )
                enc = GetFont().GetEncoding();
            else
                enc = wxLocale::GetSystemEncoding();
            wxCFStringRef cfTitle;
            cfTitle.Assign( item.GetText() , enc );
            if(columnDesc.titleString)
                CFRelease(columnDesc.titleString);
            columnDesc.titleString = cfTitle;
        }
        */

        if (item.GetMask() & wxLIST_MASK_IMAGE && item.GetImage() != -1 )
        {
            wxImageList* imageList = GetImageList(wxIMAGE_LIST_SMALL);
            if (imageList && imageList->GetImageCount() > 0 )
            {
                wxBitmap bmp = imageList->GetBitmap( item.GetImage() );
                IconRef icon = bmp.GetIconRef();
                columnDesc.btnContentInfo.u.iconRef = icon;
                columnDesc.btnContentInfo.contentType = kControlContentIconRef;
            }
        }

        verify_noerr( m_dbImpl->SetHeaderDesc( id, &columnDesc ) );

    }
    return true;
}

int wxListCtrl::GetColumnCount() const
{
    if (m_genericImpl)
        return m_genericImpl->GetColumnCount();

    if (m_dbImpl)
    {
        UInt32 count;
        m_dbImpl->GetColumnCount(&count);
        return count;
    }

    return m_colCount;
}

// Gets the column width
int wxListCtrl::GetColumnWidth(int col) const
{
    if (m_genericImpl)
        return m_genericImpl->GetColumnWidth(col);

    if (m_dbImpl)
    {
        return m_dbImpl->GetColumnWidth(col);
    }

    return 0;
}

// Sets the column width
bool wxListCtrl::SetColumnWidth(int col, int width)
{
    if (m_genericImpl)
        return m_genericImpl->SetColumnWidth(col, width);

    if (m_dbImpl)
    {
        if ( width == wxLIST_AUTOSIZE_USEHEADER )
        {
            width = 150; // FIXME
        }

        if (col == -1)
        {
            for (int column = 0; column < GetColumnCount(); column++)
            {
                wxListItem colInfo;
                GetColumn(column, colInfo);

                colInfo.SetWidth(width);
                SetColumn(column, colInfo);

                const int mywidth = (width == wxLIST_AUTOSIZE)
                                    ? CalcColumnAutoWidth(column) : width;
                m_dbImpl->SetColumnWidth(column, mywidth);
            }
        }
        else
        {
            if ( width == wxLIST_AUTOSIZE )
                width = CalcColumnAutoWidth(col);

            wxListItem colInfo;
            GetColumn(col, colInfo);

            colInfo.SetWidth(width);
            SetColumn(col, colInfo);
            m_dbImpl->SetColumnWidth(col, width);
        }
        return true;
    }

    return false;
}

// Gets the number of items that can fit vertically in the
// visible area of the list control (list or report view)
// or the total number of items in the list control (icon
// or small icon view)
int wxListCtrl::GetCountPerPage() const
{
    if (m_genericImpl)
        return m_genericImpl->GetCountPerPage();

    if (m_dbImpl)
    {
        UInt16 height = 1;
        m_dbImpl->GetDefaultRowHeight( &height );
        if (height > 0)
            return GetClientSize().y / height;
    }

    return 1;
}

// Gets the edit control for editing labels.
wxTextCtrl* wxListCtrl::GetEditControl() const
{
    if (m_genericImpl)
        return m_genericImpl->GetEditControl();

    return NULL;
}

// Gets information about the item
bool wxListCtrl::GetItem(wxListItem& info) const
{
    if (m_genericImpl)
        return m_genericImpl->GetItem(info);

    if (m_dbImpl)
    {
        if (!IsVirtual())
        {
            if (info.m_itemId >= 0 && info.m_itemId < GetItemCount())
            {
                m_dbImpl->MacGetColumnInfo(info.m_itemId, info.m_col, info);
                // MacGetColumnInfo returns erroneous information in the state field, so zero it.
                info.SetState(0);
                if (info.GetMask() & wxLIST_MASK_STATE)
                {
                    DataBrowserItemID id = (DataBrowserItemID)m_dbImpl->GetItemFromLine(info.m_itemId);
                    if (IsDataBrowserItemSelected( m_dbImpl->GetControlRef(), id ))
                        info.SetState(info.GetState() | wxLIST_STATE_SELECTED);
                }
            }
        }
        else
        {
            if (info.m_itemId >= 0 && info.m_itemId < GetItemCount())
            {
                info.SetText( OnGetItemText(info.m_itemId, info.m_col) );
                info.SetImage( OnGetItemColumnImage(info.m_itemId, info.m_col) );
                if (info.GetMask() & wxLIST_MASK_STATE)
                {
                    if (IsDataBrowserItemSelected( m_dbImpl->GetControlRef(), info.m_itemId+1 ))
                        info.SetState(info.GetState() | wxLIST_STATE_SELECTED);
                }

                wxListItemAttr* attrs = OnGetItemAttr( info.m_itemId );
                if (attrs)
                {
                    info.SetFont( attrs->GetFont() );
                    info.SetBackgroundColour( attrs->GetBackgroundColour() );
                    info.SetTextColour( attrs->GetTextColour() );
                }
            }
        }
    }
    bool success = true;
    return success;
}

// Sets information about the item
bool wxListCtrl::SetItem(wxListItem& info)
{
    if (m_genericImpl)
        return m_genericImpl->SetItem(info);

    if (m_dbImpl)
        m_dbImpl->MacSetColumnInfo( info.m_itemId, info.m_col, &info );

    return true;
}

long wxListCtrl::SetItem(long index, int col, const wxString& label, int imageId)
{
    if (m_genericImpl)
        return m_genericImpl->SetItem(index, col, label, imageId);

    wxListItem info;
    info.m_text = label;
    info.m_mask = wxLIST_MASK_TEXT;
    info.m_itemId = index;
    info.m_col = col;
    if ( imageId > -1 )
    {
        info.m_image = imageId;
        info.m_mask |= wxLIST_MASK_IMAGE;
    }
    return SetItem(info);
}


// Gets the item state
int wxListCtrl::GetItemState(long item, long stateMask) const
{
    if (m_genericImpl)
        return m_genericImpl->GetItemState(item, stateMask);

    if (m_dbImpl)
    {
        if ( HasFlag(wxLC_VIRTUAL) )
        {
            if (stateMask == wxLIST_STATE_SELECTED)
            {
                if (IsDataBrowserItemSelected( m_dbImpl->GetControlRef(), item+1 ))
                    return wxLIST_STATE_SELECTED;
                else
                    return 0;
            }
        }
        else
        {
            wxListItem info;

            info.m_mask = wxLIST_MASK_STATE;
            info.m_stateMask = stateMask;
            info.m_itemId = item;

            if (!GetItem(info))
                return 0;

            return info.m_state;
        }
    }

    return 0;
}

// Sets the item state
bool wxListCtrl::SetItemState(long item, long state, long stateMask)
{
    if (m_genericImpl)
        return m_genericImpl->SetItemState(item, state, stateMask);

    if (m_dbImpl)
    {
        DataBrowserSetOption option = kDataBrowserItemsAdd;
        if ( (stateMask & wxLIST_STATE_SELECTED) && state == 0 )
            option = kDataBrowserItemsRemove;

        if (item == -1)
        {
            if ( HasFlag(wxLC_VIRTUAL) )
            {
                wxMacDataItemBrowserSelectionSuppressor suppressor(m_dbImpl);
                m_dbImpl->SetSelectedAllItems(option);
            }
            else
            {
                for(int i = 0; i < GetItemCount();i++)
                {
                    wxListItem info;
                    info.m_itemId = i;
                    info.m_mask = wxLIST_MASK_STATE;
                    info.m_stateMask = stateMask;
                    info.m_state = state;
                    SetItem(info);
                }
            }
        }
        else
        {
            if ( HasFlag(wxLC_VIRTUAL) )
            {
                long itemID = item+1;
                bool isSelected = IsDataBrowserItemSelected(m_dbImpl->GetControlRef(), (DataBrowserItemID)itemID );
                bool isSelectedState = (state == wxLIST_STATE_SELECTED);

                // toggle the selection state if wxListInfo state and actual state don't match.
                if ( (stateMask & wxLIST_STATE_SELECTED) && isSelected != isSelectedState )
                {
                SetDataBrowserSelectedItems(m_dbImpl->GetControlRef(), 1, (DataBrowserItemID*)&itemID, option);
            }
            }
            else
            {
                wxListItem info;
                info.m_itemId = item;
                info.m_mask = wxLIST_MASK_STATE;
                info.m_stateMask = stateMask;
                info.m_state = state;
                return SetItem(info);
            }
        }
    }
    return true;
}

// Sets the item image
bool wxListCtrl::SetItemImage(long item, int image, int WXUNUSED(selImage))
{
    return SetItemColumnImage(item, 0, image);
}

// Sets the item image
bool wxListCtrl::SetItemColumnImage(long item, long column, int image)
{
    if (m_genericImpl)
        return m_genericImpl->SetItemColumnImage(item, column, image);

    wxListItem info;

    info.m_mask = wxLIST_MASK_IMAGE;
    info.m_image = image;
    info.m_itemId = item;
    info.m_col = column;

    return SetItem(info);
}

// Gets the item text
wxString wxListCtrl::GetItemText(long item, int column) const
{
    if (m_genericImpl)
        return m_genericImpl->GetItemText(item, column);

    wxListItem info;

    info.m_mask = wxLIST_MASK_TEXT;
    info.m_itemId = item;
    info.m_col = column;

    if (!GetItem(info))
        return wxEmptyString;
    return info.m_text;
}

// Sets the item text
void wxListCtrl::SetItemText(long item, const wxString& str)
{
    if (m_genericImpl)
        return m_genericImpl->SetItemText(item, str);

    wxListItem info;

    info.m_mask = wxLIST_MASK_TEXT;
    info.m_itemId = item;
    info.m_text = str;

    SetItem(info);
}

// Gets the item data
long wxListCtrl::GetItemData(long item) const
{
    if (m_genericImpl)
        return m_genericImpl->GetItemData(item);

    wxListItem info;

    info.m_mask = wxLIST_MASK_DATA;
    info.m_itemId = item;

    if (!GetItem(info))
        return 0;
    return info.m_data;
}

// Sets the item data
bool wxListCtrl::SetItemPtrData(long item, wxUIntPtr data)
{
    if (m_genericImpl)
        return m_genericImpl->SetItemData(item, data);

    wxListItem info;

    info.m_mask = wxLIST_MASK_DATA;
    info.m_itemId = item;
    info.m_data = data;

    return SetItem(info);
}

wxRect wxListCtrl::GetViewRect() const
{
    wxASSERT_MSG( !HasFlag(wxLC_REPORT | wxLC_LIST),
                    wxT("wxListCtrl::GetViewRect() only works in icon mode") );

    if (m_genericImpl)
        return m_genericImpl->GetViewRect();

    wxRect rect;
    return rect;
}

bool wxListCtrl::GetSubItemRect( long item, long subItem, wxRect& rect, int code ) const
{
    if (m_genericImpl)
        return m_genericImpl->GetSubItemRect(item, subItem, rect, code);

    // TODO: implement for DataBrowser implementation
    return false;
}

// Gets the item rectangle
bool wxListCtrl::GetItemRect(long item, wxRect& rect, int code) const
{
    if (m_genericImpl)
        return m_genericImpl->GetItemRect(item, rect, code);


    if (m_dbImpl)
    {
        DataBrowserItemID id;

        DataBrowserTableViewColumnID col = 0;
        verify_noerr( m_dbImpl->GetColumnIDFromIndex( 0, &col ) );

        Rect bounds;
        DataBrowserPropertyPart part = kDataBrowserPropertyEnclosingPart;
        if ( code == wxLIST_RECT_LABEL )
            part = kDataBrowserPropertyTextPart;
        else if ( code == wxLIST_RECT_ICON )
            part = kDataBrowserPropertyIconPart;

        if ( !(GetWindowStyleFlag() & wxLC_VIRTUAL) )
        {
            wxMacDataItem* thisItem = m_dbImpl->GetItemFromLine(item);
            id = (DataBrowserItemID) thisItem;
        }
        else
            id = item+1;

        GetDataBrowserItemPartBounds( m_dbImpl->GetControlRef(), id, col, part, &bounds );

        rect.x = bounds.left;
        rect.y = bounds.top;
        rect.width = bounds.right - bounds.left; //GetClientSize().x; // we need the width of the whole row, not just the item.
        rect.height = bounds.bottom - bounds.top;
        //fprintf("id = %d, bounds = %d, %d, %d, %d\n", id, rect.x, rect.y, rect.width, rect.height);
    }
    return true;
}

// Gets the item position
bool wxListCtrl::GetItemPosition(long item, wxPoint& pos) const
{
    if (m_genericImpl)
        return m_genericImpl->GetItemPosition(item, pos);

    bool success = false;

    if (m_dbImpl)
    {
        wxRect itemRect;
        GetItemRect(item, itemRect);
        pos = itemRect.GetPosition();
        success = true;
    }

    return success;
}

// Sets the item position.
bool wxListCtrl::SetItemPosition(long item, const wxPoint& pos)
{
    if (m_genericImpl)
        return m_genericImpl->SetItemPosition(item, pos);

    return false;
}

// Gets the number of items in the list control
int wxListCtrl::GetItemCount() const
{
    if (m_genericImpl)
        return m_genericImpl->GetItemCount();

    if (m_dbImpl)
        return m_dbImpl->MacGetCount();

    return m_count;
}

void wxListCtrl::SetItemSpacing( int spacing, bool isSmall )
{
    if (m_genericImpl)
        m_genericImpl->SetItemSpacing(spacing, isSmall);
}

wxSize wxListCtrl::GetItemSpacing() const
{
    if (m_genericImpl)
        return m_genericImpl->GetItemSpacing();

    return wxSize(0, 0);
}

void wxListCtrl::SetItemTextColour( long item, const wxColour &col )
{
    if (m_genericImpl)
    {
        m_genericImpl->SetItemTextColour(item, col);
        return;
    }

    wxListItem info;
    info.m_itemId = item;
    info.SetTextColour( col );
    SetItem( info );
}

wxColour wxListCtrl::GetItemTextColour( long item ) const
{
    if (m_genericImpl)
        return m_genericImpl->GetItemTextColour(item);

    if (m_dbImpl)
    {
        wxListItem info;
        if (GetItem(info))
            return info.GetTextColour();
    }
    return wxNullColour;
}

void wxListCtrl::SetItemBackgroundColour( long item, const wxColour &col )
{
    if (m_genericImpl)
    {
        m_genericImpl->SetItemBackgroundColour(item, col);
        return;
    }

    wxListItem info;
    info.m_itemId = item;
    info.SetBackgroundColour( col );
    SetItem( info );
}

wxColour wxListCtrl::GetItemBackgroundColour( long item ) const
{
    if (m_genericImpl)
        return m_genericImpl->GetItemBackgroundColour(item);

    if (m_dbImpl)
    {
        wxListItem info;
        if (GetItem(info))
            return info.GetBackgroundColour();
    }
    return wxNullColour;
}

void wxListCtrl::SetItemFont( long item, const wxFont &f )
{
    if (m_genericImpl)
    {
        m_genericImpl->SetItemFont(item, f);
        return;
    }

    wxListItem info;
    info.m_itemId = item;
    info.SetFont( f );
    SetItem( info );
}

wxFont wxListCtrl::GetItemFont( long item ) const
{
    if (m_genericImpl)
        return m_genericImpl->GetItemFont(item);

    if (m_dbImpl)
    {
        wxListItem info;
        if (GetItem(info))
            return info.GetFont();
    }

    return wxNullFont;
}

// Gets the number of selected items in the list control
int wxListCtrl::GetSelectedItemCount() const
{
    if (m_genericImpl)
        return m_genericImpl->GetSelectedItemCount();

    if (m_dbImpl)
        return m_dbImpl->GetSelectedItemCount(NULL, true);

    return 0;
}

// Gets the text colour of the listview
wxColour wxListCtrl::GetTextColour() const
{
    if (m_genericImpl)
        return m_genericImpl->GetTextColour();

    // TODO: we need owner drawn list items to customize text color.
    if (m_dbImpl)
        return m_textColor;

    return wxNullColour;
}

// Sets the text colour of the listview
void wxListCtrl::SetTextColour(const wxColour& col)
{
    if (m_genericImpl)
    {
        m_genericImpl->SetTextColour(col);
        return;
    }

    if (m_dbImpl)
        m_textColor = col;
}

// Gets the index of the topmost visible item when in
// list or report view
long wxListCtrl::GetTopItem() const
{
    if (m_genericImpl)
        return m_genericImpl->GetTopItem();

    if (m_dbImpl)
    {
        int flags = 0;
        long item = HitTest( wxPoint(1, 1), flags);
        if (flags == wxLIST_HITTEST_ONITEM)
            return item;
    }

    return 0;
}

// Searches for an item, starting from 'item'.
// 'geometry' is one of
// wxLIST_NEXT_ABOVE/ALL/BELOW/LEFT/RIGHT.
// 'state' is a state bit flag, one or more of
// wxLIST_STATE_DROPHILITED/FOCUSED/SELECTED/CUT.
// item can be -1 to find the first item that matches the
// specified flags.
// Returns the item or -1 if unsuccessful.
long wxListCtrl::GetNextItem(long item, int geom, int state) const
{
    if (m_genericImpl)
        return m_genericImpl->GetNextItem(item, geom, state);

    // TODO: implement all geometry and state options?
    if ( m_dbImpl )
    {
        if ( geom == wxLIST_NEXT_ALL || geom == wxLIST_NEXT_BELOW )
        {
            long count = m_dbImpl->MacGetCount() ;
            for ( long line = item + 1 ; line < count; line++ )
            {
                DataBrowserItemID id = line + 1;
                if ( !IsVirtual() )
                    id = (DataBrowserItemID)m_dbImpl->GetItemFromLine(line);

                if ( (state & wxLIST_STATE_FOCUSED) && (m_current == line))
                    return line;

                if ( (state == wxLIST_STATE_DONTCARE ) )
                    return line;

                if ( (state & wxLIST_STATE_SELECTED) && IsDataBrowserItemSelected(m_dbImpl->GetControlRef(), id ) )
                    return line;
            }
        }

        if ( geom == wxLIST_NEXT_ABOVE )
        {
            int item2 = item;
            if ( item2 == -1 )
                item2 = m_dbImpl->MacGetCount();

            for ( long line = item2 - 1 ; line >= 0; line-- )
            {
                DataBrowserItemID id = line + 1;
                if ( !IsVirtual() )
                    id = (DataBrowserItemID)m_dbImpl->GetItemFromLine(line);

                if ( (state & wxLIST_STATE_FOCUSED) && (m_current == line))
                    return line;

                if ( (state == wxLIST_STATE_DONTCARE ) )
                    return line;

                if ( (state & wxLIST_STATE_SELECTED) && IsDataBrowserItemSelected(m_dbImpl->GetControlRef(), id ) )
                    return line;
            }
        }
    }

    return -1;
}


wxImageList *wxListCtrl::GetImageList(int which) const
{
    if (m_genericImpl)
        return m_genericImpl->GetImageList(which);

    if ( which == wxIMAGE_LIST_NORMAL )
    {
        return m_imageListNormal;
    }
    else if ( which == wxIMAGE_LIST_SMALL )
    {
        return m_imageListSmall;
    }
    else if ( which == wxIMAGE_LIST_STATE )
    {
        return m_imageListState;
    }
    return NULL;
}

void wxListCtrl::SetImageList(wxImageList *imageList, int which)
{
    if (m_genericImpl)
    {
        m_genericImpl->SetImageList(imageList, which);
        return;
    }

    if ( which == wxIMAGE_LIST_NORMAL )
    {
        if (m_ownsImageListNormal) delete m_imageListNormal;
        m_imageListNormal = imageList;
        m_ownsImageListNormal = false;
    }
    else if ( which == wxIMAGE_LIST_SMALL )
    {
        if (m_ownsImageListSmall) delete m_imageListSmall;
        m_imageListSmall = imageList;
        m_ownsImageListSmall = false;
    }
    else if ( which == wxIMAGE_LIST_STATE )
    {
        if (m_ownsImageListState) delete m_imageListState;
        m_imageListState = imageList;
        m_ownsImageListState = false;
    }
}

void wxListCtrl::AssignImageList(wxImageList *imageList, int which)
{
    if (m_genericImpl)
    {
        m_genericImpl->AssignImageList(imageList, which);
        return;
    }

    SetImageList(imageList, which);
    if ( which == wxIMAGE_LIST_NORMAL )
        m_ownsImageListNormal = true;
    else if ( which == wxIMAGE_LIST_SMALL )
        m_ownsImageListSmall = true;
    else if ( which == wxIMAGE_LIST_STATE )
        m_ownsImageListState = true;
}

// ----------------------------------------------------------------------------
// Operations
// ----------------------------------------------------------------------------

// Arranges the items
bool wxListCtrl::Arrange(int flag)
{
    if (m_genericImpl)
        return m_genericImpl->Arrange(flag);
    return false;
}

// Deletes an item
bool wxListCtrl::DeleteItem(long item)
{
    if (m_genericImpl)
        return m_genericImpl->DeleteItem(item);

    if (m_dbImpl)
    {
        m_dbImpl->MacDelete(item);
        wxListEvent event( wxEVT_LIST_DELETE_ITEM, GetId() );
        event.SetEventObject( this );
        event.m_itemIndex = item;
        HandleWindowEvent( event );

    }
    return true;
}

// Deletes all items
bool wxListCtrl::DeleteAllItems()
{
    m_current = -1;
    if (m_genericImpl)
        return m_genericImpl->DeleteAllItems();

    if (m_dbImpl)
    {
        m_dbImpl->MacClear();
        wxListEvent event( wxEVT_LIST_DELETE_ALL_ITEMS, GetId() );
        event.SetEventObject( this );
        HandleWindowEvent( event );
    }
    return true;
}

// Deletes all items
bool wxListCtrl::DeleteAllColumns()
{
    if (m_genericImpl)
        return m_genericImpl->DeleteAllColumns();

    if (m_dbImpl)
    {
        UInt32 cols;
        m_dbImpl->GetColumnCount(&cols);
        for (UInt32 col = 0; col < cols; col++)
        {
                DeleteColumn(0);
        }
    }

    return true;
}

// Deletes a column
bool wxListCtrl::DeleteColumn(int col)
{
    if (m_genericImpl)
        return m_genericImpl->DeleteColumn(col);

    if (m_dbImpl)
    {
        OSStatus err = m_dbImpl->RemoveColumn(col);
        return err == noErr;
    }

    return true;
}

// Clears items, and columns if there are any.
void wxListCtrl::ClearAll()
{
    if (m_genericImpl)
    {
        m_genericImpl->ClearAll();
        return;
    }

    if (m_dbImpl)
    {
        DeleteAllItems();
        DeleteAllColumns();
    }
}

wxTextCtrl* wxListCtrl::EditLabel(long item, wxClassInfo* textControlClass)
{
    if (m_genericImpl)
        return m_genericImpl->EditLabel(item, textControlClass);

    if (m_dbImpl)
    {
        wxCHECK_MSG( (item >= 0) && ((long)item < GetItemCount()), NULL,
                     wxT("wrong index in wxListCtrl::EditLabel()") );

        wxASSERT_MSG( textControlClass->IsKindOf(CLASSINFO(wxTextCtrl)),
                     wxT("EditLabel() needs a text control") );

        wxListEvent le( wxEVT_LIST_BEGIN_LABEL_EDIT, GetParent()->GetId() );
        le.SetEventObject( this );
        le.m_itemIndex = item;
        le.m_col = 0;
        GetItem( le.m_item );

        if ( GetParent()->HandleWindowEvent( le ) && !le.IsAllowed() )
        {
            // vetoed by user code
            return NULL;
        }

        wxTextCtrl * const text = (wxTextCtrl *)textControlClass->CreateObject();
        m_textctrlWrapper = new wxListCtrlTextCtrlWrapper(this, text, item);
        return m_textctrlWrapper->GetText();
    }
    return NULL;
}

// End label editing, optionally cancelling the edit
bool wxListCtrl::EndEditLabel(bool WXUNUSED(cancel))
{
    // TODO: generic impl. doesn't have this method - is it needed for us?
    if (m_genericImpl)
        return true; // m_genericImpl->EndEditLabel(cancel);

    if (m_dbImpl)
    {
        DataBrowserTableViewColumnID id = 0;
        verify_noerr( m_dbImpl->GetColumnIDFromIndex( 0, &id ) );
        verify_noerr( SetDataBrowserEditItem(m_dbImpl->GetControlRef(), kDataBrowserNoItem, id ) );
    }
    return true;
}

// Ensures this item is visible
bool wxListCtrl::EnsureVisible(long item)
{
    if (m_genericImpl)
        return m_genericImpl->EnsureVisible(item);

    if (m_dbImpl)
    {
        wxMacDataItem* dataItem = m_dbImpl->GetItemFromLine(item);
        m_dbImpl->RevealItem(dataItem, kDataBrowserRevealWithoutSelecting);
    }

    return true;
}

// Find an item whose label matches this string, starting from the item after 'start'
// or the beginning if 'start' is -1.
long wxListCtrl::FindItem(long start, const wxString& str, bool partial)
{
    if (m_genericImpl)
        return m_genericImpl->FindItem(start, str, partial);

    wxString str_upper = str.Upper();

    long idx = start;
    if (idx < 0)
        idx = 0;
    long count = GetItemCount();

    while (idx < count)
    {
        wxString line_upper = GetItemText(idx).Upper();
        if (!partial)
        {
            if (line_upper == str_upper )
                return idx;
        }
        else
        {
            if (line_upper.find(str_upper) == 0)
                return idx;
        }

        idx++;
    };

    return wxNOT_FOUND;
}

// Find an item whose data matches this data, starting from the item after 'start'
// or the beginning if 'start' is -1.
long wxListCtrl::FindItem(long start, long data)
{
    if (m_genericImpl)
        return m_genericImpl->FindItem(start, data);

    long idx = start;
    if (idx < 0)
        idx = 0;
    long count = GetItemCount();

    while (idx < count)
    {
        if (GetItemData(idx) == data)
            return idx;
        idx++;
    };

    return wxNOT_FOUND;
}

// Find an item nearest this position in the specified direction, starting from
// the item after 'start' or the beginning if 'start' is -1.
long wxListCtrl::FindItem(long start, const wxPoint& pt, int direction)
{
    if (m_genericImpl)
        return m_genericImpl->FindItem(start, pt, direction);
    return -1;
}

static void calculateCGDrawingBounds(CGRect inItemRect, CGRect *outIconRect, CGRect *outTextRect, bool hasIcon);

// Determines which item (if any) is at the specified point,
// giving details in 'flags' (see wxLIST_HITTEST_... flags above)
long
wxListCtrl::HitTest(const wxPoint& point, int& flags, long *ptrSubItem) const
{
    if (ptrSubItem)
        *ptrSubItem = -1;

    if (m_genericImpl)
        return m_genericImpl->HitTest(point, flags, ptrSubItem);

    flags = wxLIST_HITTEST_NOWHERE;
    if (m_dbImpl)
    {
        int colHeaderHeight = 22; // TODO: Find a way to get this value from the db control?
        UInt16 rowHeight = 0;
        m_dbImpl->GetDefaultRowHeight(&rowHeight);

        int y = point.y;
        // get the actual row by taking scroll position into account
        UInt32 offsetX, offsetY;
        m_dbImpl->GetScrollPosition( &offsetY, &offsetX );
        y += offsetY;

        if ( !(GetWindowStyleFlag() & wxLC_NO_HEADER) )
            y -= colHeaderHeight;

        if ( y < 0 )
            return -1;

        int row = y / rowHeight;
        DataBrowserItemID id;
        m_dbImpl->GetItemID( (DataBrowserTableViewRowIndex) row, &id );

        CGPoint click_point = CGPointMake( point.x, point.y );
        if (row < GetItemCount() )
        {
            short column;
            for( column = 0; column < GetColumnCount(); column++ )
            {
               Rect enclosingRect;
               CGRect enclosingCGRect, iconCGRect, textCGRect;
               int imgIndex = -1;
               wxMacListCtrlItem* lcItem;

               WXUNUSED_UNLESS_DEBUG( OSStatus status = ) m_dbImpl->GetItemPartBounds( id, kMinColumnId + column, kDataBrowserPropertyEnclosingPart, &enclosingRect );
               wxASSERT( status == noErr );

               enclosingCGRect = CGRectMake(enclosingRect.left,
                                            enclosingRect.top,
                                            enclosingRect.right - enclosingRect.left,
                                            enclosingRect.bottom - enclosingRect.top);

               if (column >= 0)
               {
                   if ( !(GetWindowStyleFlag() & wxLC_VIRTUAL ) )
                   {
                       lcItem = (wxMacListCtrlItem*) id;
                       if (lcItem->HasColumnInfo(column))
                       {
                           wxListItem* item = lcItem->GetColumnInfo(column);

                           if (item->GetMask() & wxLIST_MASK_IMAGE)
                           {
                               imgIndex = item->GetImage();
                           }
                       }
                   }
                   else
                   {
                       long itemNum = (long)id-1;
                       if (itemNum >= 0 && itemNum < GetItemCount())
                       {
                           imgIndex = OnGetItemColumnImage( itemNum, column );
                       }
                   }
               }

               calculateCGDrawingBounds(enclosingCGRect, &iconCGRect, &textCGRect, (imgIndex != -1) );

               if ( CGRectContainsPoint( iconCGRect, click_point ) )
               {
                   flags = wxLIST_HITTEST_ONITEMICON;
                   if (ptrSubItem)
                       *ptrSubItem = column;
                   return row;
               }
               else if ( CGRectContainsPoint( textCGRect, click_point ) )
               {
                   flags = wxLIST_HITTEST_ONITEMLABEL;
                   if (ptrSubItem)
                       *ptrSubItem = column;
                   return row;
               }
           }

           if ( !(GetWindowStyleFlag() & wxLC_VIRTUAL ) )
           {
               wxMacListCtrlItem* lcItem;
               lcItem = (wxMacListCtrlItem*) id;
               if (lcItem)
               {
                   flags = wxLIST_HITTEST_ONITEM;
                   if (ptrSubItem)
                       *ptrSubItem = column;
                   return row;
               }
           }
           else
           {
               flags = wxLIST_HITTEST_ONITEM;
               if (ptrSubItem)
                   *ptrSubItem = column;
               return row;
           }
         }
         else
         {
             if ( wxControl::HitTest( point ) )
                 flags = wxLIST_HITTEST_NOWHERE;
         }
    }

    return -1;
}

int wxListCtrl::GetScrollPos(int orient) const
{
    if (m_genericImpl)
        return m_genericImpl->GetScrollPos(orient);

    if (m_dbImpl)
    {
        UInt32 offsetX, offsetY;
        m_dbImpl->GetScrollPosition( &offsetY, &offsetX );
        if ( orient == wxHORIZONTAL )
           return offsetX;
        else
           return offsetY;
    }

    return 0;
}

// Inserts an item, returning the index of the new item if successful,
// -1 otherwise.
long wxListCtrl::InsertItem(wxListItem& info)
{
    wxASSERT_MSG( !IsVirtual(), wxT("can't be used with virtual controls") );

    if (m_genericImpl)
        return m_genericImpl->InsertItem(info);

    if (m_dbImpl && !IsVirtual())
    {
        int count = GetItemCount();

        if (info.m_itemId > count)
            info.m_itemId = count;

        m_dbImpl->MacInsertItem(info.m_itemId, &info );

        wxListEvent event( wxEVT_LIST_INSERT_ITEM, GetId() );
        event.SetEventObject( this );
        event.m_itemIndex = info.m_itemId;
        HandleWindowEvent( event );
        return info.m_itemId;
    }
    return -1;
}

long wxListCtrl::InsertItem(long index, const wxString& label)
{
    if (m_genericImpl)
        return m_genericImpl->InsertItem(index, label);

    wxListItem info;
    info.m_text = label;
    info.m_mask = wxLIST_MASK_TEXT;
    info.m_itemId = index;
    return InsertItem(info);
}

// Inserts an image item
long wxListCtrl::InsertItem(long index, int imageIndex)
{
    if (m_genericImpl)
        return m_genericImpl->InsertItem(index, imageIndex);

    wxListItem info;
    info.m_image = imageIndex;
    info.m_mask = wxLIST_MASK_IMAGE;
    info.m_itemId = index;
    return InsertItem(info);
}

// Inserts an image/string item
long wxListCtrl::InsertItem(long index, const wxString& label, int imageIndex)
{
    if (m_genericImpl)
        return m_genericImpl->InsertItem(index, label, imageIndex);

    wxListItem info;
    info.m_image = imageIndex;
    info.m_text = label;
    info.m_mask = wxLIST_MASK_IMAGE | wxLIST_MASK_TEXT;
    info.m_itemId = index;
    return InsertItem(info);
}

// For list view mode (only), inserts a column.
long wxListCtrl::DoInsertColumn(long col, const wxListItem& item)
{
    if (m_genericImpl)
        return m_genericImpl->InsertColumn(col, item);

    if (m_dbImpl)
    {
        int width = item.GetWidth();
        if ( !(item.GetMask() & wxLIST_MASK_WIDTH) )
            width = 150;

        DataBrowserPropertyType type = kDataBrowserCustomType; //kDataBrowserTextType;
        wxImageList* imageList = GetImageList(wxIMAGE_LIST_SMALL);
        if (imageList && imageList->GetImageCount() > 0)
        {
            wxBitmap bmp = imageList->GetBitmap(0);
            //if (bmp.IsOk())
            //    type = kDataBrowserIconAndTextType;
        }

        SInt16 just = teFlushDefault;
        if (item.GetMask() & wxLIST_MASK_FORMAT)
        {
            if (item.GetAlign() == wxLIST_FORMAT_LEFT)
                just = teFlushLeft;
            else if (item.GetAlign() == wxLIST_FORMAT_CENTER)
                just = teCenter;
            else if (item.GetAlign() == wxLIST_FORMAT_RIGHT)
                just = teFlushRight;
        }
        m_dbImpl->InsertColumn(col, type, item.GetText(), just, width);

        wxListItem* listItem = new wxListItem(item);
        m_colsInfo.Insert( col, listItem );
        SetColumn(col, item);

        // set/remove options based on the wxListCtrl type.
        DataBrowserTableViewColumnID id;
        m_dbImpl->GetColumnIDFromIndex(col, &id);
        DataBrowserPropertyFlags flags;
        verify_noerr(m_dbImpl->GetPropertyFlags(id, &flags));
        if (GetWindowStyleFlag() & wxLC_EDIT_LABELS)
            flags |= kDataBrowserPropertyIsEditable;

        if (GetWindowStyleFlag() & wxLC_VIRTUAL){
            flags &= ~kDataBrowserListViewSortableColumn;
        }
        verify_noerr(m_dbImpl->SetPropertyFlags(id, flags));
    }

    return col;
}

// scroll the control by the given number of pixels (exception: in list view,
// dx is interpreted as number of columns)
bool wxListCtrl::ScrollList(int dx, int dy)
{
    if (m_genericImpl)
        return m_genericImpl->ScrollList(dx, dy);

    if (m_dbImpl)
    {
        // Notice that the parameter order is correct here: first argument is
        // the "top" displacement, second one is the "left" one.
        m_dbImpl->SetScrollPosition(dy, dx);
    }
    return true;
}


bool wxListCtrl::SortItems(wxListCtrlCompare fn, wxIntPtr data)
{
    if (m_genericImpl)
        return m_genericImpl->SortItems(fn, data);

    if (m_dbImpl)
    {
        m_compareFunc = fn;
        m_compareFuncData = data;
        SortDataBrowserContainer( m_dbImpl->GetControlRef(), kDataBrowserNoItem, true);

        // we need to do this after each call, else we get a crash from wxPython when
        // SortItems is called the second time.
        m_compareFunc = NULL;
        m_compareFuncData = 0;
    }

    return true;
}

void wxListCtrl::OnRenameTimer()
{
    wxCHECK_RET( HasCurrent(), wxT("unexpected rename timer") );

    EditLabel( m_current );
}

bool wxListCtrl::OnRenameAccept(long itemEdit, const wxString& value)
{
    wxListEvent le( wxEVT_LIST_END_LABEL_EDIT, GetId() );
    le.SetEventObject( this );
    le.m_itemIndex = itemEdit;

    GetItem( le.m_item );
    le.m_item.m_text = value;
    return !HandleWindowEvent( le ) ||
                le.IsAllowed();
}

void wxListCtrl::OnRenameCancelled(long itemEdit)
{
    // let owner know that the edit was cancelled
    wxListEvent le( wxEVT_LIST_END_LABEL_EDIT, GetParent()->GetId() );

    le.SetEditCanceled(true);

    le.SetEventObject( this );
    le.m_itemIndex = itemEdit;

    GetItem( le.m_item );
    HandleWindowEvent( le );
}

// ----------------------------------------------------------------------------
// virtual list controls
// ----------------------------------------------------------------------------

wxString wxListCtrl::OnGetItemText(long WXUNUSED(item), long WXUNUSED(col)) const
{
    // this is a pure virtual function, in fact - which is not really pure
    // because the controls which are not virtual don't need to implement it
    wxFAIL_MSG( wxT("wxListCtrl::OnGetItemText not supposed to be called") );

    return wxEmptyString;
}

int wxListCtrl::OnGetItemImage(long WXUNUSED(item)) const
{
    wxCHECK_MSG(!GetImageList(wxIMAGE_LIST_SMALL),
                -1,
                wxT("List control has an image list, OnGetItemImage or OnGetItemColumnImage should be overridden."));
    return -1;
}

int wxListCtrl::OnGetItemColumnImage(long item, long column) const
{
    if (!column)
        return OnGetItemImage(item);

    return -1;
}

void wxListCtrl::SetItemCount(long count)
{
    wxASSERT_MSG( IsVirtual(), wxT("this is for virtual controls only") );

    if (m_genericImpl)
    {
        m_genericImpl->SetItemCount(count);
        return;
    }

    if (m_dbImpl)
    {
        // we need to temporarily disable the new item creation notification
        // procedure to speed things up
        // FIXME: Even this doesn't seem to help much...

        // FIXME: Find a more efficient way to do this.
        m_dbImpl->MacClear();

        DataBrowserCallbacks callbacks;
        DataBrowserItemNotificationUPP itemUPP;
        GetDataBrowserCallbacks(m_dbImpl->GetControlRef(), &callbacks);
        itemUPP = callbacks.u.v1.itemNotificationCallback;
        callbacks.u.v1.itemNotificationCallback = 0;
        m_dbImpl->SetCallbacks(&callbacks);
        ::AddDataBrowserItems(m_dbImpl->GetControlRef(), kDataBrowserNoItem,
            count, NULL, kDataBrowserItemNoProperty);
        callbacks.u.v1.itemNotificationCallback = itemUPP;
        m_dbImpl->SetCallbacks(&callbacks);
    }
    m_count = count;
}

void wxListCtrl::RefreshItem(long item)
{
    if (m_genericImpl)
    {
        m_genericImpl->RefreshItem(item);
        return;
    }

    if (m_dbImpl)
    {
        DataBrowserItemID id;

        if ( !IsVirtual() )
        {
            wxMacDataItem* thisItem = m_dbImpl->GetItemFromLine(item);
            id = (DataBrowserItemID) thisItem;
        }
        else
            id = item+1;

        m_dbImpl->wxMacDataBrowserControl::UpdateItems
                  (
                    kDataBrowserNoItem,
                    1, &id,
                    kDataBrowserItemNoProperty, // preSortProperty
                    kDataBrowserNoItem // update all columns
                  );
    }
}

void wxListCtrl::RefreshItems(long itemFrom, long itemTo)
{
    if (m_genericImpl)
    {
        m_genericImpl->RefreshItems(itemFrom, itemTo);
        return;
    }

    if (m_dbImpl)
    {
        const long count = itemTo - itemFrom + 1;
        DataBrowserItemID *ids = new DataBrowserItemID[count];

        if ( !IsVirtual() )
        {
            for ( long i = 0; i < count; i++ )
            {
                wxMacDataItem* thisItem = m_dbImpl->GetItemFromLine(itemFrom+i);
                ids[i] = (DataBrowserItemID) thisItem;
            }
        }
        else
        {
            for ( long i = 0; i < count; i++ )
                ids[i] = itemFrom+i+1;
        }

        m_dbImpl->wxMacDataBrowserControl::UpdateItems
                  (
                    kDataBrowserNoItem,
                    count, ids,
                    kDataBrowserItemNoProperty, // preSortProperty
                    kDataBrowserNoItem // update all columns
                  );

        delete[] ids;
    }
}

void wxListCtrl::SetDropTarget( wxDropTarget *dropTarget )
{
#if wxUSE_DRAG_AND_DROP
    if (m_genericImpl)
        m_genericImpl->SetDropTarget( dropTarget );

    if (m_dbImpl)
        wxWindow::SetDropTarget( dropTarget );
#endif
}

wxDropTarget *wxListCtrl::GetDropTarget() const
{
#if wxUSE_DRAG_AND_DROP
    if (m_genericImpl)
        return m_genericImpl->GetDropTarget();

    if (m_dbImpl)
        return wxWindow::GetDropTarget();
#endif
    return NULL;
}

#if wxABI_VERSION >= 20801
void wxListCtrl::SetFocus()
{
        if (m_genericImpl)
        {
                m_genericImpl->SetFocus();
                return;
        }

        wxWindow::SetFocus();
}
#endif

// wxMac internal data structures

wxMacListCtrlItem::~wxMacListCtrlItem()
{
    WX_CLEAR_HASH_MAP( wxListItemList, m_rowItems );
}

void wxMacListCtrlItem::Notification(wxMacDataItemBrowserControl *owner ,
    DataBrowserItemNotification message,
    DataBrowserItemDataRef WXUNUSED(itemData) ) const
{

    wxMacDataBrowserListCtrlControl *lb = wxDynamicCast(owner, wxMacDataBrowserListCtrlControl);

    // we want to depend on as little as possible to make sure tear-down of controls is safe
    if ( message == kDataBrowserItemRemoved)
    {
        delete this;
        return;
    }
    else if ( message == kDataBrowserItemAdded )
    {
        // we don't issue events on adding, the item is not really stored in the list yet, so we
        // avoid asserts by gettting out now
        return  ;
    }

    wxListCtrl *list = wxDynamicCast( owner->GetWXPeer() , wxListCtrl );
    if ( list && lb )
    {
        bool trigger = false;

        wxListEvent event( wxEVT_LIST_ITEM_SELECTED, list->GetId() );
        bool isSingle = (list->GetWindowStyle() & wxLC_SINGLE_SEL) != 0;

        event.SetEventObject( list );
        event.m_itemIndex = owner->GetLineFromItem( this ) ;
        event.m_item.m_itemId = event.m_itemIndex;
        list->GetItem(event.m_item);

        switch (message)
        {
            case kDataBrowserItemDeselected:
                event.SetEventType(wxEVT_LIST_ITEM_DESELECTED);
                if ( !isSingle )
                    trigger = !lb->IsSelectionSuppressed();
                break;

            case kDataBrowserItemSelected:
                trigger = !lb->IsSelectionSuppressed();
                break;

            case kDataBrowserItemDoubleClicked:
                event.SetEventType( wxEVT_LIST_ITEM_ACTIVATED );
                trigger = true;
                break;

            case kDataBrowserEditStarted :
                // TODO : how to veto ?
                event.SetEventType( wxEVT_LIST_BEGIN_LABEL_EDIT ) ;
                trigger = true ;
                break ;

            case kDataBrowserEditStopped :
                // TODO probably trigger only upon the value store callback, because
                // here IIRC we cannot veto
                event.SetEventType( wxEVT_LIST_END_LABEL_EDIT ) ;
                trigger = true ;
                break ;

            default:
                break;
        }

        if ( trigger )
        {
            // direct notification is not always having the listbox GetSelection() having in synch with event
            wxPostEvent( list->GetEventHandler(), event );
        }
    }

}

IMPLEMENT_DYNAMIC_CLASS(wxMacDataBrowserListCtrlControl, wxMacDataItemBrowserControl )

wxMacDataBrowserListCtrlControl::wxMacDataBrowserListCtrlControl( wxWindow *peer, const wxPoint& pos, const wxSize& size, long style)
    : wxMacDataItemBrowserControl( peer, pos, size, style )
{
    OSStatus err = noErr;
    m_clientDataItemsType = wxClientData_None;
    m_isVirtual = false;
    m_flags = 0;

    if ( style & wxLC_VIRTUAL )
        m_isVirtual = true;

    DataBrowserSelectionFlags  options = kDataBrowserDragSelect;
    if ( style & wxLC_SINGLE_SEL )
    {
        options |= kDataBrowserSelectOnlyOne;
    }
    else
    {
        options |= kDataBrowserCmdTogglesSelection;
    }

    err = SetSelectionFlags( options );
    verify_noerr( err );

    DataBrowserCustomCallbacks callbacks;
    InitializeDataBrowserCustomCallbacks( &callbacks, kDataBrowserLatestCustomCallbacks );

    if ( gDataBrowserDrawItemUPP == NULL )
        gDataBrowserDrawItemUPP = NewDataBrowserDrawItemUPP(DataBrowserDrawItemProc);

    if ( gDataBrowserHitTestUPP == NULL )
        gDataBrowserHitTestUPP = NewDataBrowserHitTestUPP(DataBrowserHitTestProc);

    callbacks.u.v1.drawItemCallback = gDataBrowserDrawItemUPP;
    callbacks.u.v1.hitTestCallback = gDataBrowserHitTestUPP;

    SetDataBrowserCustomCallbacks( GetControlRef(), &callbacks );

    if ( style & wxLC_LIST )
    {
        InsertColumn(0, kDataBrowserIconAndTextType, wxEmptyString, -1, -1);
        verify_noerr( AutoSizeColumns() );
    }

    if ( style & wxLC_LIST || style & wxLC_NO_HEADER )
        verify_noerr( SetHeaderButtonHeight( 0 ) );

    if ( m_isVirtual )
        SetSortProperty( kMinColumnId - 1 );
    else
        SetSortProperty( kMinColumnId );

    m_sortOrder = SortOrder_None;

    if ( style & wxLC_SORT_DESCENDING )
    {
        SetSortOrder( kDataBrowserOrderDecreasing );
    }
    else if ( style & wxLC_SORT_ASCENDING )
    {
        SetSortOrder( kDataBrowserOrderIncreasing );
    }

    if ( style & wxLC_VRULES )
    {
        verify_noerr( DataBrowserChangeAttributes(m_controlRef, kDataBrowserAttributeListViewDrawColumnDividers, kDataBrowserAttributeNone) );
    }

    verify_noerr( SetHiliteStyle(kDataBrowserTableViewFillHilite ) );
    verify_noerr( SetHasScrollBars( (style & wxHSCROLL) != 0 , true ) );
}

pascal Boolean wxMacDataBrowserListCtrlControl::DataBrowserEditTextProc(
        ControlRef browser,
        DataBrowserItemID itemID,
        DataBrowserPropertyID property,
        CFStringRef theString,
        Rect *maxEditTextRect,
        Boolean *shrinkToFit)
{
    Boolean result = false;
    wxMacDataBrowserListCtrlControl* ctl = wxDynamicCast(wxMacControl::GetReferenceFromNativeControl( browser ), wxMacDataBrowserListCtrlControl);
    if ( ctl != 0 )
    {
        result = ctl->ConfirmEditText(itemID, property, theString, maxEditTextRect, shrinkToFit);
        theString = CFSTR("Hello!");
    }
    return result;
}

bool wxMacDataBrowserListCtrlControl::ConfirmEditText(
        DataBrowserItemID WXUNUSED(itemID),
        DataBrowserPropertyID WXUNUSED(property),
        CFStringRef WXUNUSED(theString),
        Rect *WXUNUSED(maxEditTextRect),
        Boolean *WXUNUSED(shrinkToFit))
{
    return false;
}

pascal void wxMacDataBrowserListCtrlControl::DataBrowserDrawItemProc(
        ControlRef browser,
        DataBrowserItemID itemID,
        DataBrowserPropertyID property,
        DataBrowserItemState itemState,
        const Rect *itemRect,
        SInt16 gdDepth,
        Boolean colorDevice)
{
    wxMacDataBrowserListCtrlControl* ctl = wxDynamicCast(wxMacControl::GetReferenceFromNativeControl( browser ), wxMacDataBrowserListCtrlControl);
    if ( ctl != 0 )
    {
        ctl->DrawItem(itemID, property, itemState, itemRect, gdDepth, colorDevice);
    }
}

// routines needed for DrawItem
enum
{
  kIconWidth = 16,
  kIconHeight = 16,
  kTextBoxHeight = 14,
  kIconTextSpacingV = 2,
  kItemPadding = 4,
  kContentHeight = kIconHeight + kTextBoxHeight + kIconTextSpacingV
};

static void calculateCGDrawingBounds(CGRect inItemRect, CGRect *outIconRect, CGRect *outTextRect, bool hasIcon = false)
{
  float textBottom;
  float iconW = 0;
  float padding = kItemPadding;
  if (hasIcon)
  {
    iconW = kIconWidth;
    padding = padding*2;
  }

  textBottom = inItemRect.origin.y;

  *outIconRect = CGRectMake(inItemRect.origin.x + kItemPadding,
                textBottom + kIconTextSpacingV, kIconWidth,
                kIconHeight);

  *outTextRect = CGRectMake(inItemRect.origin.x + padding + iconW,
                textBottom + kIconTextSpacingV, inItemRect.size.width - padding - iconW,
                inItemRect.size.height - kIconTextSpacingV);
}

void wxMacDataBrowserListCtrlControl::DrawItem(
        DataBrowserItemID itemID,
        DataBrowserPropertyID property,
        DataBrowserItemState itemState,
        const Rect *WXUNUSED(itemRect),
        SInt16 gdDepth,
        Boolean colorDevice)
{
    wxString text;
    wxFont font = wxNullFont;
    int imgIndex = -1;

    DataBrowserTableViewColumnIndex listColumn = 0;
    GetColumnPosition( property, &listColumn );

    wxListCtrl* list = wxDynamicCast( GetWXPeer() , wxListCtrl );
    wxMacListCtrlItem* lcItem;
    wxColour color = *wxBLACK;
    wxColour bgColor = wxNullColour;

    if (listColumn >= 0)
    {
        if (!m_isVirtual)
        {
            lcItem = (wxMacListCtrlItem*) itemID;
            if (lcItem->HasColumnInfo(listColumn))
            {
                wxListItem* item = lcItem->GetColumnInfo(listColumn);

                // we always use the 0 column to get font and text/background colors.
                if (lcItem->HasColumnInfo(0))
                {
                    wxListItem* firstItem = lcItem->GetColumnInfo(0);
                    color = firstItem->GetTextColour();
                    bgColor = firstItem->GetBackgroundColour();
                    font = firstItem->GetFont();
                }

                if (item->GetMask() & wxLIST_MASK_TEXT)
                    text = item->GetText();
                if (item->GetMask() & wxLIST_MASK_IMAGE)
                    imgIndex = item->GetImage();
            }

        }
        else
        {
            long itemNum = (long)itemID-1;
            if (itemNum >= 0 && itemNum < list->GetItemCount())
            {
                text = list->OnGetItemText( itemNum, listColumn );
                imgIndex = list->OnGetItemColumnImage( itemNum, listColumn );
                wxListItemAttr* attrs = list->OnGetItemAttr( itemNum );
                if (attrs)
                {
                    if (attrs->HasBackgroundColour())
                        bgColor = attrs->GetBackgroundColour();
                    if (attrs->HasTextColour())
                        color = attrs->GetTextColour();
                    if (attrs->HasFont())
                        font = attrs->GetFont();
                }
            }
        }
    }

    wxColour listBgColor = list->GetBackgroundColour();
    if (bgColor == wxNullColour)
        bgColor = listBgColor;

    if (!font.IsOk())
        font = list->GetFont();

    wxCFStringRef cfString( text, wxLocale::GetSystemEncoding() );

    Rect enclosingRect;
    CGRect enclosingCGRect, iconCGRect, textCGRect;
    Boolean active;
    ThemeDrawingState savedState = NULL;
    CGContextRef context = (CGContextRef)list->MacGetDrawingContext();
    wxMacCGContextStateSaver top_saver_cg( context );

    RGBColor labelColor;
    labelColor.red = 0;
    labelColor.green = 0;
    labelColor.blue = 0;

    RGBColor backgroundColor;
    backgroundColor.red = 255;
    backgroundColor.green = 255;
    backgroundColor.blue = 255;

    GetDataBrowserItemPartBounds(GetControlRef(), itemID, property, kDataBrowserPropertyEnclosingPart,
              &enclosingRect);

    enclosingCGRect = CGRectMake(enclosingRect.left,
                      enclosingRect.top,
                      enclosingRect.right - enclosingRect.left,
                      enclosingRect.bottom - enclosingRect.top);

    bool hasFocus = (wxWindow::FindFocus() == list);
    active = IsControlActive(GetControlRef());

    // don't paint the background over the vertical rule line
    if ( list->GetWindowStyleFlag() & wxLC_VRULES )
    {
        enclosingCGRect.origin.x += 1;
        enclosingCGRect.size.width -= 1;
    }
    if (itemState == kDataBrowserItemIsSelected)
    {

        GetThemeDrawingState(&savedState);

        if (active && hasFocus)
        {
            GetThemeBrushAsColor(kThemeBrushAlternatePrimaryHighlightColor, 32, true, &backgroundColor);
            GetThemeTextColor(kThemeTextColorWhite, gdDepth, colorDevice, &labelColor);
        }
        else
        {
            GetThemeBrushAsColor(kThemeBrushSecondaryHighlightColor, 32, true, &backgroundColor);
            GetThemeTextColor(kThemeTextColorBlack, gdDepth, colorDevice, &labelColor);
        }
        wxMacCGContextStateSaver cg( context );

        CGContextSetRGBFillColor(context, (CGFloat)backgroundColor.red / (CGFloat)USHRT_MAX,
                      (CGFloat)backgroundColor.green / (CGFloat)USHRT_MAX,
                      (CGFloat)backgroundColor.blue / (CGFloat)USHRT_MAX, (CGFloat) 1.0);
        CGContextFillRect(context, enclosingCGRect);
    }
    else
    {

        if (color.IsOk())
            color.GetRGBColor(&labelColor);
        else if (list->GetTextColour().IsOk())
            list->GetTextColour().GetRGBColor(&labelColor);

        if (bgColor.IsOk())
        {
            bgColor.GetRGBColor(&backgroundColor);
            CGContextSaveGState(context);

            CGContextSetRGBFillColor(context, (CGFloat)backgroundColor.red / (CGFloat)USHRT_MAX,
                          (CGFloat)backgroundColor.green / (CGFloat)USHRT_MAX,
                          (CGFloat)backgroundColor.blue / (CGFloat)USHRT_MAX, (CGFloat) 1.0);
            CGContextFillRect(context, enclosingCGRect);

            CGContextRestoreGState(context);
        }
    }

    calculateCGDrawingBounds(enclosingCGRect, &iconCGRect, &textCGRect, (imgIndex != -1) );

    if (imgIndex != -1)
    {
        wxImageList* imageList = list->GetImageList(wxIMAGE_LIST_SMALL);
        if (imageList && imageList->GetImageCount() > 0)
        {
            wxBitmap bmp = imageList->GetBitmap(imgIndex);
            IconRef icon = bmp.GetIconRef();

            wxMacCGContextStateSaver cg( context );

            CGContextTranslateCTM(context, 0,iconCGRect.origin.y + CGRectGetMaxY(iconCGRect));
            CGContextScaleCTM(context,1.0f,-1.0f);
            PlotIconRefInContext(context, &iconCGRect, kAlignNone,
              active ? kTransformNone : kTransformDisabled, NULL,
              kPlotIconRefNormalFlags, icon);
        }
    }

    HIThemeTextHorizontalFlush hFlush = kHIThemeTextHorizontalFlushLeft;
    HIThemeTextInfo info;

    info.version = kHIThemeTextInfoVersionOne;
    info.fontID = kThemeViewsFont;
    if (font.IsOk())
    {
        info.fontID = kThemeSpecifiedFont;
        info.font = (CTFontRef) font.OSXGetCTFont();
    }

    wxListItem item;
    list->GetColumn(listColumn, item);
    if (item.GetMask() & wxLIST_MASK_FORMAT)
    {
        if (item.GetAlign() == wxLIST_FORMAT_LEFT)
            hFlush = kHIThemeTextHorizontalFlushLeft;
        else if (item.GetAlign() == wxLIST_FORMAT_CENTER)
            hFlush = kHIThemeTextHorizontalFlushCenter;
        else if (item.GetAlign() == wxLIST_FORMAT_RIGHT)
        {
            hFlush = kHIThemeTextHorizontalFlushRight;
            textCGRect.origin.x -= kItemPadding; // give a little extra paddding
        }
    }

    info.state = active ? kThemeStateActive : kThemeStateInactive;
    info.horizontalFlushness = hFlush;
    info.verticalFlushness = kHIThemeTextVerticalFlushCenter;
    info.options = kHIThemeTextBoxOptionNone;
    info.truncationPosition = kHIThemeTextTruncationEnd;
    info.truncationMaxLines = 1;

    {
        wxMacCGContextStateSaver cg( context );
        CGContextSetRGBFillColor (context, (CGFloat)labelColor.red / (CGFloat)USHRT_MAX,
                      (CGFloat)labelColor.green / (CGFloat)USHRT_MAX,
                      (CGFloat)labelColor.blue / (CGFloat)USHRT_MAX, (CGFloat) 1.0);

        HIThemeDrawTextBox(cfString, &textCGRect, &info, context, kHIThemeOrientationNormal);
    }

#ifndef __LP64__
    if (savedState != NULL)
        SetThemeDrawingState(savedState, true);
#endif
}

OSStatus wxMacDataBrowserListCtrlControl::GetSetItemData(DataBrowserItemID itemID,
                        DataBrowserPropertyID property,
                        DataBrowserItemDataRef itemData,
                        Boolean changeValue )
{
    wxString text;
    int imgIndex = -1;

    DataBrowserTableViewColumnIndex listColumn = 0;
    verify_noerr( GetColumnPosition( property, &listColumn ) );

    OSStatus err = errDataBrowserPropertyNotSupported;
    wxListCtrl* list = wxDynamicCast( GetWXPeer() , wxListCtrl );
    wxMacListCtrlItem* lcItem = NULL;

    if (listColumn >= 0)
    {
        if (!m_isVirtual)
        {
            lcItem = (wxMacListCtrlItem*) itemID;
            if (lcItem && lcItem->HasColumnInfo(listColumn)){
                wxListItem* item = lcItem->GetColumnInfo(listColumn);
                if (item->GetMask() & wxLIST_MASK_TEXT)
                    text = item->GetText();
                if (item->GetMask() & wxLIST_MASK_IMAGE)
                    imgIndex = item->GetImage();
            }
        }
        else
        {
            long itemNum = (long)itemID-1;
            if (itemNum >= 0 && itemNum < list->GetItemCount())
            {
                text = list->OnGetItemText( itemNum, listColumn );
                imgIndex = list->OnGetItemColumnImage( itemNum, listColumn );
            }
        }
    }

    if ( !changeValue )
    {
        switch (property)
        {
            case kDataBrowserItemIsEditableProperty :
                if ( list && list->HasFlag( wxLC_EDIT_LABELS ) )
                {
                    verify_noerr(SetDataBrowserItemDataBooleanValue( itemData, true ));
                    err = noErr ;
                }
                break ;
            default :
                if ( property >= kMinColumnId )
                {
                    if (!text.IsEmpty()){
                        wxCFStringRef cfStr( text, wxLocale::GetSystemEncoding() );
                        err = ::SetDataBrowserItemDataText( itemData, cfStr );
                        err = noErr;
                    }



                    if ( imgIndex != -1 )
                    {
                        wxImageList* imageList = list->GetImageList(wxIMAGE_LIST_SMALL);
                        if (imageList && imageList->GetImageCount() > 0){
                            wxBitmap bmp = imageList->GetBitmap(imgIndex);
                            IconRef icon = bmp.GetIconRef();
                            ::SetDataBrowserItemDataIcon(itemData, icon);
                        }
                    }

                }
                break ;
        }

    }
    else
    {
        switch (property)
        {
             default:
                if ( property >= kMinColumnId )
                {
                    DataBrowserTableViewColumnIndex listColumn = 0;
                    verify_noerr( GetColumnPosition( property, &listColumn ) );

                    // TODO probably send the 'end edit' from here, as we
                    // can then deal with the veto
                    CFStringRef sr ;
                    verify_noerr( GetDataBrowserItemDataText( itemData , &sr ) ) ;
                    wxCFStringRef cfStr(sr) ;;
                    if (m_isVirtual)
                        list->SetItem( (long)itemData-1 , listColumn, cfStr.AsString() ) ;
                    else
                    {
                        if (lcItem)
                            lcItem->SetColumnTextValue( listColumn, cfStr.AsString() );
                    }
                    err = noErr ;
                }
                break;
        }
    }
    return err;
}

void wxMacDataBrowserListCtrlControl::ItemNotification(DataBrowserItemID itemID,
    DataBrowserItemNotification message,
    DataBrowserItemDataRef itemData )
{
    wxMacListCtrlItem *item = NULL;
    if ( !m_isVirtual )
    {
        item = (wxMacListCtrlItem *) itemID;
    }

    // we want to depend on as little as possible to make sure tear-down of controls is safe
    if ( message == kDataBrowserItemRemoved )
    {
        if ( item )
            item->Notification(this, message, itemData);
        return;
    }
    else if ( message == kDataBrowserItemAdded )
    {
        // we don't issue events on adding, the item is not really stored in the list yet, so we
        // avoid asserts by getting out now
        if ( item )
            item->Notification(this, message, itemData);
        return  ;
    }

    wxListCtrl *list = wxDynamicCast( GetWXPeer() , wxListCtrl );
    if ( list )
    {
        bool trigger = false;

        wxListEvent event( wxEVT_LIST_ITEM_SELECTED, list->GetId() );

        event.SetEventObject( list );
        if ( !list->IsVirtual() )
        {
            DataBrowserTableViewRowIndex result = 0;
            verify_noerr( GetItemRow( itemID, &result ) ) ;
            event.m_itemIndex = result;
        }
        else
        {
            event.m_itemIndex = (long)itemID-1;
        }
        event.m_item.m_itemId = event.m_itemIndex;
        list->GetItem(event.m_item);

        switch (message)
        {
            case kDataBrowserItemDeselected:
                event.SetEventType(wxEVT_LIST_ITEM_DESELECTED);
                // as the generic implementation is also triggering this
                // event for single selection, we do the same (different than listbox)
                trigger = !IsSelectionSuppressed();
                break;

            case kDataBrowserItemSelected:
                trigger = !IsSelectionSuppressed();

                break;

            case kDataBrowserItemDoubleClicked:
                event.SetEventType( wxEVT_LIST_ITEM_ACTIVATED );
                trigger = true;
                break;

            case kDataBrowserEditStarted :
                // TODO : how to veto ?
                event.SetEventType( wxEVT_LIST_BEGIN_LABEL_EDIT ) ;
                trigger = true ;
                break ;

            case kDataBrowserEditStopped :
                // TODO probably trigger only upon the value store callback, because
                // here IIRC we cannot veto
                event.SetEventType( wxEVT_LIST_END_LABEL_EDIT ) ;
                trigger = true ;
                break ;

            default:
                break;
        }

        if ( trigger )
        {
            // direct notification is not always having the listbox GetSelection() having in synch with event
            wxPostEvent( list->GetEventHandler(), event );
        }
    }
}

Boolean wxMacDataBrowserListCtrlControl::CompareItems(DataBrowserItemID itemOneID,
                        DataBrowserItemID itemTwoID,
                        DataBrowserPropertyID sortProperty)
{

    bool retval = false;
    wxString itemText;
    wxString otherItemText;
    long itemOrder;
    long otherItemOrder;

    DataBrowserTableViewColumnIndex colId = 0;
    verify_noerr( GetColumnPosition( sortProperty, &colId ) );

    wxListCtrl* list = wxDynamicCast( GetWXPeer() , wxListCtrl );

    DataBrowserSortOrder sort;
    verify_noerr(GetSortOrder(&sort));

    if (colId >= 0)
    {
        if (!m_isVirtual)
        {
            wxMacListCtrlItem* item = (wxMacListCtrlItem*)itemOneID;
            wxMacListCtrlItem* otherItem = (wxMacListCtrlItem*)itemTwoID;

            itemOrder = item->GetOrder();
            otherItemOrder = otherItem->GetOrder();

            wxListCtrlCompare func = list->GetCompareFunc();
            if (func != NULL)
            {

                long item1 = -1;
                long item2 = -1;
                if (item && item->HasColumnInfo(0))
                    item1 = item->GetColumnInfo(0)->GetData();
                if (otherItem && otherItem->HasColumnInfo(0))
                    item2 = otherItem->GetColumnInfo(0)->GetData();

                if (item1 > -1 && item2 > -1)
                {
                    int result = func(item1, item2, list->GetCompareFuncData());
                    if (sort == kDataBrowserOrderIncreasing)
                        return result >= 0;
                    else
                        return result < 0;
                }
            }

            // we can't use the native control's sorting abilities, so just
            // sort by item id.
            return itemOrder < otherItemOrder;
        }
        else
        {

            long itemNum = (long)itemOneID;
            long otherItemNum = (long)itemTwoID;

            // virtual listctrls don't support sorting
            return itemNum < otherItemNum;
        }
    }
    else{
        // fallback for undefined cases
        retval = itemOneID < itemTwoID;
    }

    return retval;
}

wxMacDataBrowserListCtrlControl::~wxMacDataBrowserListCtrlControl()
{
}

void wxMacDataBrowserListCtrlControl::MacSetColumnInfo( unsigned int row, unsigned int column, wxListItem* item )
{
    wxMacDataItem* dataItem = GetItemFromLine(row);
    wxASSERT_MSG( dataItem, wxT("could not obtain wxMacDataItem for row in MacSetColumnInfo. Is row a valid wxListCtrl row?") );
    if (item)
    {
        wxMacListCtrlItem* listItem = static_cast<wxMacListCtrlItem *>(dataItem);
        bool hasInfo = listItem->HasColumnInfo( column );
        listItem->SetColumnInfo( column, item );
        listItem->SetOrder(row);
        UpdateState(dataItem, item);

        wxListCtrl* list = wxDynamicCast( GetWXPeer() , wxListCtrl );

        // NB: When this call was made before a control was completely shown, it would
        // update the item prematurely (i.e. no text would be listed) and, on show,
        // only the sorted column would be refreshed, meaning only first column text labels
        // would be shown. Making sure not to update items until the control is visible
        // seems to fix this issue.
        if (hasInfo && list->IsShown())
        {
            DataBrowserTableViewColumnID id = 0;
            verify_noerr( GetColumnIDFromIndex( column, &id ) );
            UpdateItem( wxMacDataBrowserRootContainer, listItem , id );
        }
    }
}

// apply changes that need to happen immediately, rather than when the
// databrowser control fires a callback.
void wxMacDataBrowserListCtrlControl::UpdateState(wxMacDataItem* dataItem, wxListItem* listItem)
{
    bool isSelected = IsItemSelected( dataItem );
    bool isSelectedState = (listItem->GetState() == wxLIST_STATE_SELECTED);

    // toggle the selection state if wxListInfo state and actual state don't match.
    if ( listItem->GetMask() & wxLIST_MASK_STATE && isSelected != isSelectedState )
    {
        DataBrowserSetOption options = kDataBrowserItemsAdd;
        if (!isSelectedState)
            options = kDataBrowserItemsRemove;
        SetSelectedItem(dataItem, options);
    }
    // TODO: Set column width if item width > than current column width
}

void wxMacDataBrowserListCtrlControl::MacGetColumnInfo( unsigned int row, unsigned int column, wxListItem& item )
{
    wxMacDataItem* dataItem = GetItemFromLine(row);
    wxASSERT_MSG( dataItem, wxT("could not obtain wxMacDataItem in MacGetColumnInfo. Is row a valid wxListCtrl row?") );
    // CS should this guard against dataItem = 0 ? , as item is not a pointer if (item) is not appropriate
    //if (item)
    {
        wxMacListCtrlItem* listItem = static_cast<wxMacListCtrlItem *>(dataItem);

        if (!listItem->HasColumnInfo( column ))
            return;

        wxListItem* oldItem = listItem->GetColumnInfo( column );

        if (oldItem)
        {
            long mask = item.GetMask();
            if ( !mask )
                // by default, get everything for backwards compatibility
                mask = -1;

            if ( mask & wxLIST_MASK_TEXT )
                item.SetText(oldItem->GetText());
            if ( mask & wxLIST_MASK_IMAGE )
                item.SetImage(oldItem->GetImage());
            if ( mask & wxLIST_MASK_DATA )
                item.SetData(oldItem->GetData());
            if ( mask & wxLIST_MASK_STATE )
                item.SetState(oldItem->GetState());
            if ( mask & wxLIST_MASK_WIDTH )
                item.SetWidth(oldItem->GetWidth());
            if ( mask & wxLIST_MASK_FORMAT )
                item.SetAlign(oldItem->GetAlign());

            item.SetTextColour(oldItem->GetTextColour());
            item.SetBackgroundColour(oldItem->GetBackgroundColour());
            item.SetFont(oldItem->GetFont());
        }
    }
}

void wxMacDataBrowserListCtrlControl::MacInsertItem( unsigned int n, wxListItem* item )
{

    wxMacDataItemBrowserControl::MacInsert(n, new wxMacListCtrlItem() );
    MacSetColumnInfo(n, 0, item);
}

wxMacListCtrlItem::wxMacListCtrlItem()
{
    m_rowItems = wxListItemList();
}

int wxMacListCtrlItem::GetColumnImageValue( unsigned int column )
{
    if ( HasColumnInfo(column) )
        return GetColumnInfo(column)->GetImage();

    return -1;
}

void wxMacListCtrlItem::SetColumnImageValue( unsigned int column, int imageIndex )
{
    if ( HasColumnInfo(column) )
        GetColumnInfo(column)->SetImage(imageIndex);
}

wxString wxMacListCtrlItem::GetColumnTextValue( unsigned int column )
{
/* TODO CHECK REMOVE
    if ( column == 0 )
        return GetLabel();
*/
    if ( HasColumnInfo(column) )
        return GetColumnInfo(column)->GetText();

    return wxEmptyString;
}

void wxMacListCtrlItem::SetColumnTextValue( unsigned int column, const wxString& text )
{
    if ( HasColumnInfo(column) )
        GetColumnInfo(column)->SetText(text);

/* TODO CHECK REMOVE
    // for compatibility with superclass APIs
    if ( column == 0 )
        SetLabel(text);
*/
}

wxListItem* wxMacListCtrlItem::GetColumnInfo( unsigned int column )
{
    wxASSERT_MSG( HasColumnInfo(column), wxT("invalid column index in wxMacListCtrlItem") );
    return m_rowItems[column];
}

bool wxMacListCtrlItem::HasColumnInfo( unsigned int column )
{
    return !(m_rowItems.find( column ) == m_rowItems.end());
}

void wxMacListCtrlItem::SetColumnInfo( unsigned int column, wxListItem* item )
{

    if ( !HasColumnInfo(column) )
    {
        wxListItem* listItem = new wxListItem(*item);
        m_rowItems[column] = listItem;
    }
    else
    {
        wxListItem* listItem = GetColumnInfo( column );
        long mask = item->GetMask();
        if (mask & wxLIST_MASK_TEXT)
            listItem->SetText(item->GetText());
        if (mask & wxLIST_MASK_DATA)
            listItem->SetData(item->GetData());
        if (mask & wxLIST_MASK_IMAGE)
            listItem->SetImage(item->GetImage());
        if (mask & wxLIST_MASK_STATE)
            listItem->SetState(item->GetState());
        if (mask & wxLIST_MASK_FORMAT)
            listItem->SetAlign(item->GetAlign());
        if (mask & wxLIST_MASK_WIDTH)
            listItem->SetWidth(item->GetWidth());

        if ( item->HasAttributes() )
        {
            if ( listItem->HasAttributes() )
                listItem->GetAttributes()->AssignFrom(*item->GetAttributes());
            else
            {
                listItem->SetTextColour(item->GetTextColour());
                listItem->SetBackgroundColour(item->GetBackgroundColour());
                listItem->SetFont(item->GetFont());
            }
        }
    }
}

int wxListCtrl::CalcColumnAutoWidth(int col) const
{
    int width = 0;

    for ( int i = 0; i < GetItemCount(); i++ )
    {
        wxListItem info;
        info.SetMask(wxLIST_MASK_TEXT | wxLIST_MASK_IMAGE);
        info.SetId(i);
        info.SetColumn(col);
        GetItem(info);

        const wxFont font = info.GetFont();

        int w = 0;
        if ( font.IsOk() )
            GetTextExtent(info.GetText(), &w, NULL, NULL, NULL, &font);
        else
            GetTextExtent(info.GetText(), &w, NULL);

        w += 2 * kItemPadding;

        if ( info.GetImage() != -1 )
            w += kIconWidth;

        width = wxMax(width, w);
    }

    return width;
}

#endif // wxUSE_LISTCTRL

