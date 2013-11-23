///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/listbox.cpp
// Purpose:     wxListBox
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_LISTBOX

#include "wx/listbox.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/utils.h"
    #include "wx/settings.h"
    #include "wx/arrstr.h"
    #include "wx/dcclient.h"
#endif

#include "wx/osx/private.h"

// ============================================================================
// list box control implementation
// ============================================================================

wxWidgetImplType* wxWidgetImpl::CreateListBox( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    wxMacDataBrowserListControl* control = new wxMacDataBrowserListControl( wxpeer, pos, size, style );
    // TODO CHECK control->SetClientDataType( m_clientDataItemsType );
    return control;
}

int wxMacDataBrowserListControl::DoListHitTest(const wxPoint& inpoint) const
{
    OSStatus err;

    // There are few reasons why this is complicated:
    // 1) There is no native HitTest function for Mac
    // 2) GetDataBrowserItemPartBounds only works on visible items
    // 3) We can't do it through GetDataBrowserTableView[Item]RowHeight
    //    because what it returns is basically inaccurate in the context
    //    of the coordinates we want here, but we use this as a guess
    //    for where the first visible item lies

    wxPoint point = inpoint;

    // get column property ID (req. for call to itempartbounds)
    DataBrowserTableViewColumnID colId = 0;
    err = GetDataBrowserTableViewColumnProperty(GetControlRef(), 0, &colId);
    wxCHECK_MSG(err == noErr, wxNOT_FOUND, wxT("Unexpected error from GetDataBrowserTableViewColumnProperty"));

    // OK, first we need to find the first visible item we have -
    // this will be the "low" for our binary search. There is no real
    // easy way around this, as we will need to do a SLOW linear search
    // until we find a visible item, but we can do a cheap calculation
    // via the row height to speed things up a bit
    UInt32 scrollx, scrolly;
    err = GetDataBrowserScrollPosition(GetControlRef(), &scrollx, &scrolly);
    wxCHECK_MSG(err == noErr, wxNOT_FOUND, wxT("Unexpected error from GetDataBrowserScrollPosition"));

    UInt16 height;
    err = GetDataBrowserTableViewRowHeight(GetControlRef(), &height);
    wxCHECK_MSG(err == noErr, wxNOT_FOUND, wxT("Unexpected error from GetDataBrowserTableViewRowHeight"));

    wxListBox *list = wxDynamicCast( GetWXPeer() , wxListBox );

    // these indices are 0-based, as usual, so we need to add 1 to them when
    // passing them to data browser functions which use 1-based indices
    int low = scrolly / height,
        high = list->GetCount() - 1;

    // search for the first visible item (note that the scroll guess above
    // is the low bounds of where the item might lie so we only use that as a
    // starting point - we should reach it within 1 or 2 iterations of the loop)
    while ( low <= high )
    {
        Rect bounds;
        err = GetDataBrowserItemPartBounds(
            GetControlRef(), low + 1, colId,
            kDataBrowserPropertyEnclosingPart,
            &bounds); // note +1 to translate to Mac ID
        if ( err == noErr )
            break;

        // errDataBrowserItemNotFound is expected as it simply means that the
        // item is not currently visible -- but other errors are not
        wxCHECK_MSG( err == errDataBrowserItemNotFound, wxNOT_FOUND,
                     wxT("Unexpected error from GetDataBrowserItemPartBounds") );

        low++;
    }

    // NOW do a binary search for where the item lies, searching low again if
    // we hit an item that isn't visible
    while ( low <= high )
    {
        int mid = (low + high) / 2;

        Rect bounds;
        err = GetDataBrowserItemPartBounds(
            GetControlRef(), mid + 1, colId,
            kDataBrowserPropertyEnclosingPart,
            &bounds); //note +1 to trans to mac id
        wxCHECK_MSG( err == noErr || err == errDataBrowserItemNotFound,
                     wxNOT_FOUND,
                     wxT("Unexpected error from GetDataBrowserItemPartBounds") );

        if ( err == errDataBrowserItemNotFound )
        {
            // item not visible, attempt to find a visible one
            // search lower
            high = mid - 1;
        }
        else // visible item, do actual hitttest
        {
            // if point is within the bounds, return this item (since we assume
            // all x coords of items are equal we only test the x coord in
            // equality)
            if ((point.x >= bounds.left && point.x <= bounds.right) &&
                (point.y >= bounds.top && point.y <= bounds.bottom) )
            {
                // found!
                return mid;
            }

            if ( point.y < bounds.top )
                // index(bounds) greater than key(point)
                high = mid - 1;
            else
                // index(bounds) less than key(point)
                low = mid + 1;
        }
    }

    return wxNOT_FOUND;
}

// ============================================================================
// data browser based implementation
// ============================================================================

wxMacListBoxItem::wxMacListBoxItem()
        :wxMacDataItem()
{
}

wxMacListBoxItem::~wxMacListBoxItem()
{
}

OSStatus wxMacListBoxItem::GetSetData(wxMacDataItemBrowserControl *owner ,
    DataBrowserPropertyID property,
    DataBrowserItemDataRef itemData,
    bool changeValue )
{
    wxMacDataBrowserListControl *lb = wxDynamicCast(owner,wxMacDataBrowserListControl);
    OSStatus err = errDataBrowserPropertyNotSupported;
    if ( !changeValue )
    {
        if ( property >= kMinColumnId )
        {
            wxMacDataBrowserColumn* col = lb->GetColumnFromProperty( property );
            unsigned int n = owner->GetLineFromItem( this );
            wxListBox *list = wxDynamicCast( owner->GetWXPeer() , wxListBox );
            wxMacDataBrowserCellValue valueholder(itemData);
            list->GetValueCallback( n , col, valueholder );

            err = noErr;
        }
        else
        {
            if ( property == kDataBrowserItemIsEditableProperty )
            {
                DataBrowserPropertyID propertyToEdit ;
                GetDataBrowserItemDataProperty( itemData, &propertyToEdit );
                wxMacDataBrowserColumn* col = lb->GetColumnFromProperty( propertyToEdit );

                verify_noerr(SetDataBrowserItemDataBooleanValue( itemData, col->IsEditable() ));
                err = noErr;
            }

        }

    }
    else
    {
        if ( property >= kMinColumnId )
        {
            wxMacDataBrowserColumn* col = lb->GetColumnFromProperty( property );

            unsigned int n = owner->GetLineFromItem( this );
            wxListBox *list = wxDynamicCast( owner->GetWXPeer() , wxListBox );
            wxMacDataBrowserCellValue valueholder(itemData);
            list->SetValueCallback( n , col, valueholder );

            /*
            // we have to change this behind the back, since Check() would be triggering another update round
            bool newVal = !m_isChecked;
            verify_noerr(SetDataBrowserItemDataButtonValue( itemData, newVal ? kThemeButtonOn : kThemeButtonOff ));
            m_isChecked = newVal;
            err = noErr;

            wxCommandEvent event( wxEVT_CHECKLISTBOX, checklist->GetId() );
            event.SetInt( owner->GetLineFromItem( this ) );
            event.SetEventObject( checklist );
            checklist->HandleWindowEvent( event );

            */
            err = noErr;
        }
    }

    // call inherited if not ours
    if ( err == errDataBrowserPropertyNotSupported )
    {
        err = wxMacDataItem::GetSetData(owner, property, itemData, changeValue);
    }

    return err;
}

void wxMacListBoxItem::Notification(wxMacDataItemBrowserControl *owner ,
    DataBrowserItemNotification message,
    DataBrowserItemDataRef WXUNUSED(itemData) ) const
{
    wxMacDataBrowserListControl *lb = wxDynamicCast(owner,wxMacDataBrowserListControl);

    // we want to depend on as little as possible to make sure tear-down of controls is safe

    if ( message == kDataBrowserItemRemoved)
    {
        delete this;
        return;
    }

    wxListBox *list = wxDynamicCast( lb->GetWXPeer() , wxListBox );
    wxCHECK_RET( list != NULL , wxT("Listbox expected"));

    if (message == kDataBrowserItemDoubleClicked)
    {
        unsigned int n = owner->GetLineFromItem( this );
        list->HandleLineEvent( n, true );
        return;
    }
}

IMPLEMENT_DYNAMIC_CLASS( wxMacDataBrowserListControl , wxMacDataItemBrowserControl )

wxMacDataBrowserListControl::wxMacDataBrowserListControl( wxWindow *peer, const wxPoint& pos, const wxSize& size, long style)
    : wxMacDataItemBrowserControl( peer, pos, size, style )
{
    m_nextColumnId = 0 ;

    OSStatus err = noErr;
    m_clientDataItemsType = wxClientData_None;
    if ( style & wxLB_SORT )
        m_sortOrder = SortOrder_Text_Ascending;

    DataBrowserSelectionFlags  options = kDataBrowserDragSelect;
    if ( style & wxLB_MULTIPLE )
    {
        options |= kDataBrowserAlwaysExtendSelection | kDataBrowserCmdTogglesSelection;
    }
    else if ( style & wxLB_EXTENDED )
    {
        options |= kDataBrowserCmdTogglesSelection;
    }
    else
    {
        options |= kDataBrowserSelectOnlyOne;
    }
    err = SetSelectionFlags( options );
    verify_noerr( err );

    DataBrowserListViewColumnDesc columnDesc;
    columnDesc.headerBtnDesc.titleOffset = 0;
    columnDesc.headerBtnDesc.version = kDataBrowserListViewLatestHeaderDesc;

    columnDesc.headerBtnDesc.btnFontStyle.flags =
        kControlUseFontMask | kControlUseJustMask;

    columnDesc.headerBtnDesc.btnContentInfo.contentType = kControlNoContent;
    columnDesc.headerBtnDesc.btnFontStyle.just = teFlushDefault;
    columnDesc.headerBtnDesc.btnFontStyle.font = kControlFontViewSystemFont;
    columnDesc.headerBtnDesc.btnFontStyle.style = normal;
    columnDesc.headerBtnDesc.titleString = NULL;
/*
    columnDesc.headerBtnDesc.minimumWidth = 0;
    columnDesc.headerBtnDesc.maximumWidth = 10000;

    columnDesc.propertyDesc.propertyID = kTextColumnId;
    columnDesc.propertyDesc.propertyType = kDataBrowserTextType;
    columnDesc.propertyDesc.propertyFlags = kDataBrowserTableViewSelectionColumn;
    columnDesc.propertyDesc.propertyFlags |= kDataBrowserListViewTypeSelectColumn;

    verify_noerr( AddColumn( &columnDesc, kDataBrowserListViewAppendColumn ) );
*/
    columnDesc.headerBtnDesc.minimumWidth = 0;
    columnDesc.headerBtnDesc.maximumWidth = 0;
    columnDesc.propertyDesc.propertyID = kNumericOrderColumnId;
    columnDesc.propertyDesc.propertyType = kDataBrowserPropertyRelevanceRankPart;
    columnDesc.propertyDesc.propertyFlags = kDataBrowserTableViewSelectionColumn;
    columnDesc.propertyDesc.propertyFlags |= kDataBrowserListViewTypeSelectColumn;

    verify_noerr( AddColumn( &columnDesc, kDataBrowserListViewAppendColumn ) );

/*
    SetDataBrowserSortProperty( m_controlRef , kTextColumnId);
    if ( m_sortOrder == SortOrder_Text_Ascending )
    {
        SetDataBrowserSortProperty( m_controlRef , kTextColumnId);
        SetDataBrowserSortOrder( m_controlRef , kDataBrowserOrderIncreasing);
    }
    else
*/
    {
        SetDataBrowserSortProperty( m_controlRef , kNumericOrderColumnId);
        SetDataBrowserSortOrder( m_controlRef , kDataBrowserOrderIncreasing);
    }

    verify_noerr( AutoSizeColumns() );
    verify_noerr( SetHiliteStyle(kDataBrowserTableViewFillHilite ) );
    verify_noerr( SetHeaderButtonHeight( 0 ) );
    err = SetHasScrollBars( (style & wxHSCROLL) != 0 , true );
#if 0
    // shouldn't be necessary anymore under 10.2
    GetPeer()->SetData( kControlNoPart, kControlDataBrowserIncludesFrameAndFocusTag, (Boolean)false );
    GetPeer()->SetNeedsFocusRect( true );
#endif
}

wxMacDataBrowserListControl::~wxMacDataBrowserListControl()
{
}

void wxMacDataBrowserListControl::ItemNotification(
                        DataBrowserItemID itemID,
                        DataBrowserItemNotification message,
                        DataBrowserItemDataRef itemData)
{
    wxListBox *list = wxDynamicCast( GetWXPeer() , wxListBox );
    wxCHECK_RET( list != NULL , wxT("Listbox expected"));

    if (list->HasMultipleSelection() && (message == kDataBrowserSelectionSetChanged) && (!list->MacGetBlockEvents()))
    {
        list->CalcAndSendEvent();
        return;
    }

    if ((message == kDataBrowserSelectionSetChanged) && (!list->MacGetBlockEvents()))
    {
        wxCommandEvent event( wxEVT_LISTBOX, list->GetId() );

        int sel = list->GetSelection();
        if ((sel < 0) || (sel > (int) list->GetCount()))  // OS X can select an item below the last item (why?)
           return;
        list->HandleLineEvent( sel, false );
        return;
    }

    // call super for item level(wxMacDataItem->Notification) callback processing
    wxMacDataItemBrowserControl::ItemNotification( itemID, message, itemData);
}


/*
wxWindow * wxMacDataBrowserListControl::GetPeer() const
{
    return wxDynamicCast( wxMacControl::GetWX() , wxWindow );
}
*/

//
// List Methods
//

wxMacDataBrowserColumn* wxMacDataBrowserListControl::DoInsertColumn( unsigned int pos, DataBrowserPropertyID property,
                                const wxString& title, bool editable,
                                DataBrowserPropertyType colType, SInt16 just, int width )
{
    DataBrowserListViewColumnDesc columnDesc;
    columnDesc.headerBtnDesc.titleOffset = 0;
    columnDesc.headerBtnDesc.version = kDataBrowserListViewLatestHeaderDesc;

    columnDesc.headerBtnDesc.btnFontStyle.flags =
        kControlUseFontMask | kControlUseJustMask;

    columnDesc.headerBtnDesc.btnContentInfo.contentType = kControlContentTextOnly;
    columnDesc.headerBtnDesc.btnFontStyle.just = just;
    columnDesc.headerBtnDesc.btnFontStyle.font = kControlFontViewSystemFont;
    columnDesc.headerBtnDesc.btnFontStyle.style = normal;

    // TODO: Why is m_font not defined when we enter wxLC_LIST mode, but is
    // defined for other modes?
    wxFontEncoding enc;
    if ( m_font.IsOk() )
        enc = m_font.GetEncoding();
    else
        enc = wxLocale::GetSystemEncoding();
    wxCFStringRef cfTitle( title, enc );
    columnDesc.headerBtnDesc.titleString = cfTitle;

    columnDesc.headerBtnDesc.minimumWidth = 0;
    columnDesc.headerBtnDesc.maximumWidth = 30000;

    columnDesc.propertyDesc.propertyID = property;
    columnDesc.propertyDesc.propertyType = colType;
    columnDesc.propertyDesc.propertyFlags = kDataBrowserListViewSortableColumn;
    columnDesc.propertyDesc.propertyFlags |= kDataBrowserListViewTypeSelectColumn;
    columnDesc.propertyDesc.propertyFlags |= kDataBrowserListViewNoGapForIconInHeaderButton;

    if ( editable )
        columnDesc.propertyDesc.propertyFlags |= kDataBrowserPropertyIsMutable;

    verify_noerr( AddColumn( &columnDesc, pos ) );

    if (width > 0)
    {
        wxMacDataBrowserControl::SetColumnWidth(property, width);
    }

    wxMacDataBrowserColumn *col = new wxMacDataBrowserColumn( property, colType, editable );

    m_columns.Insert( col, pos );

    return col;
}

wxListWidgetColumn* wxMacDataBrowserListControl::InsertTextColumn( unsigned pos, const wxString& title, bool editable,
                                wxAlignment just, int defaultWidth)
{
    DataBrowserPropertyID property = kMinColumnId + m_nextColumnId++;

    SInt16 j = teFlushLeft;
    if ( just & wxALIGN_RIGHT )
        j = teFlushRight;
    else if ( just & wxALIGN_CENTER_HORIZONTAL )
        j = teCenter;

    return DoInsertColumn( pos, property, title, editable, kDataBrowserTextType,  just, defaultWidth );
}

wxListWidgetColumn* wxMacDataBrowserListControl::InsertCheckColumn( unsigned pos , const wxString& title, bool editable,
                                wxAlignment just, int defaultWidth )
{
    DataBrowserPropertyID property = kMinColumnId + m_nextColumnId++;

    SInt16 j = teFlushLeft;
    if ( just & wxALIGN_RIGHT )
        j = teFlushRight;
    else if ( just & wxALIGN_CENTER_HORIZONTAL )
        j = teCenter;

    return DoInsertColumn( pos, property, title, editable, kDataBrowserCheckboxType,  just, defaultWidth );
}

wxMacDataBrowserColumn* wxMacDataBrowserListControl::GetColumnFromProperty( DataBrowserPropertyID property)
{
    for ( unsigned int i = 0; i < m_columns.size() ; ++ i )
        if ( m_columns[i]->GetProperty() == property )
            return m_columns[i];

    return NULL;
}

/*
wxMacDataItem* wxMacDataBrowserListControl::ListGetLineItem( unsigned int n )
{
    return (wxMacDataItem*) GetItemFromLine(n);
}
*/

unsigned int wxMacDataBrowserListControl::ListGetCount() const
{
    return MacGetCount();
}

void wxMacDataBrowserListControl::ListDelete( unsigned int n )
{
    MacDelete( n );
}

void wxMacDataBrowserListControl::ListInsert( unsigned int n )
{
    MacInsert( n , new wxMacListBoxItem() );
}

void wxMacDataBrowserListControl::ListClear()
{
    MacClear();
}

void wxMacDataBrowserListControl::ListDeselectAll()
{
    wxMacDataItemBrowserSelectionSuppressor suppressor(this);
    SetSelectedAllItems( kDataBrowserItemsRemove );
}

void wxMacDataBrowserListControl::ListSetSelection( unsigned int n, bool select, bool multi )
{
    wxMacDataItem* item = (wxMacDataItem*) GetItemFromLine(n);
    wxMacDataItemBrowserSelectionSuppressor suppressor(this);

    if ( IsItemSelected( item ) != select )
    {
        if ( select )
            SetSelectedItem( item, multi ? kDataBrowserItemsAdd : kDataBrowserItemsAssign );
        else
            SetSelectedItem( item, kDataBrowserItemsRemove );
    }

    ListScrollTo( n );
}

bool wxMacDataBrowserListControl::ListIsSelected( unsigned int n ) const
{
    wxMacDataItem* item = (wxMacDataItem*) GetItemFromLine(n);
    return IsItemSelected( item );
}

int wxMacDataBrowserListControl::ListGetSelection() const
{
    wxMacDataItemPtr first, last;
    GetSelectionAnchor( &first, &last );

    if ( first != NULL )
    {
        return GetLineFromItem( first );
    }

    return -1;
}

int wxMacDataBrowserListControl::ListGetSelections( wxArrayInt& aSelections ) const
{
    aSelections.Empty();
    wxArrayMacDataItemPtr selectedItems;
    GetItems( wxMacDataBrowserRootContainer, false , kDataBrowserItemIsSelected, selectedItems);

    int count = selectedItems.GetCount();

    for ( int i = 0; i < count; ++i)
    {
        aSelections.Add(GetLineFromItem(selectedItems[i]));
    }

    return count;
}

void wxMacDataBrowserListControl::ListScrollTo( unsigned int n )
{
    UInt32 top , left ;
    GetScrollPosition( &top , &left ) ;
    wxMacDataItem * item = (wxMacDataItem*) GetItemFromLine( n );

    // there is a bug in RevealItem that leads to situations
    // in large lists, where the item does not get scrolled
    // into sight, so we do a pre-scroll if necessary
    UInt16 height ;
    GetRowHeight( (DataBrowserItemID) item , &height ) ;
    UInt32 linetop = n * ((UInt32) height );
    UInt32 linebottom = linetop + height;
    Rect rect ;
    GetControlBounds( m_controlRef, &rect );

    if ( linetop < top || linebottom > (top + rect.bottom - rect.top ) )
        SetScrollPosition( wxMax( n-2, 0 ) * ((UInt32)height) , left ) ;

    RevealItem( item , kDataBrowserRevealWithoutSelecting );
}

void wxMacDataBrowserListControl::UpdateLine( unsigned int n, wxListWidgetColumn* col )
{
    wxMacDataBrowserColumn* dbcol = dynamic_cast<wxMacDataBrowserColumn*> (col);
    wxMacDataItem * item = (wxMacDataItem*) GetItemFromLine( n );
    UpdateItem(wxMacDataBrowserRootContainer, item, dbcol ? dbcol->GetProperty() : kDataBrowserNoItem );
}

void wxMacDataBrowserListControl::UpdateLineToEnd( unsigned int n)
{
    // with databrowser inserting does not need updating the entire model, it's done by databrowser itself
    wxMacDataItem * item = (wxMacDataItem*) GetItemFromLine( n );
    UpdateItem(wxMacDataBrowserRootContainer, item, kDataBrowserNoItem );
}

// value setters

void wxMacDataBrowserCellValue::Set( CFStringRef value )
{
    SetDataBrowserItemDataText( m_data, value );
}

void wxMacDataBrowserCellValue::Set( const wxString& value )
{
    wxCFStringRef cf(value);
    SetDataBrowserItemDataText( m_data, (CFStringRef) cf);
}

void wxMacDataBrowserCellValue::Set( int value )
{
    SetDataBrowserItemDataValue( m_data, value );
}

void wxMacDataBrowserCellValue::Check( bool check )
{
    SetDataBrowserItemDataButtonValue( m_data, check ? kThemeButtonOn : kThemeButtonOff);
}

int wxMacDataBrowserCellValue::GetIntValue() const
{
    SInt32 value;
    GetDataBrowserItemDataValue( m_data, &value );
    return value;
}

wxString wxMacDataBrowserCellValue::GetStringValue() const
{
    CFStringRef value;
    GetDataBrowserItemDataText ( m_data, &value );
    wxCFStringRef cf(value);
    return cf.AsString();
}

#if 0

// in case we need that one day

// ============================================================================
// HIView owner-draw-based implementation
// ============================================================================

static pascal void ListBoxDrawProc(
    ControlRef browser, DataBrowserItemID item, DataBrowserPropertyID property,
    DataBrowserItemState itemState, const Rect *itemRect, SInt16 depth, Boolean isColorDevice )
{
    CFStringRef cfString;
    ThemeDrawingState themeState;
    long systemVersion;

    GetThemeDrawingState( &themeState );
    cfString = CFStringCreateWithFormat( NULL, NULL, CFSTR("Row %d"), item );

    //  In this sample we handle the "selected" state; all others fall through to our "active" state
    if ( itemState == kDataBrowserItemIsSelected )
    {
        ThemeBrush colorBrushID;

        // TODO: switch over to wxSystemSettingsNative::GetColour() when kThemeBrushSecondaryHighlightColor
        // is incorporated Panther DB starts using kThemeBrushSecondaryHighlightColor
        // for inactive browser highlighting
        if ( !IsControlActive( browser ) )
            colorBrushID = kThemeBrushSecondaryHighlightColor;
        else
            colorBrushID = kThemeBrushPrimaryHighlightColor;

        // First paint the hilite rect, then the text on top
        SetThemePen( colorBrushID, 32, true );
        PaintRect( itemRect );
        SetThemeDrawingState( themeState, false );
    }

    DrawThemeTextBox( cfString, kThemeApplicationFont, kThemeStateActive, true, itemRect, teFlushDefault, NULL );
    SetThemeDrawingState( themeState, true );

    if ( cfString != NULL )
        CFRelease( cfString );
}

#endif


#endif // wxUSE_LISTBOX
