/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/utils.cpp
// Purpose:     Various utilities
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: utils.cpp 67681 2011-05-03 16:29:04Z DS $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////


#include "wx/wxprec.h"

#include "wx/utils.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/app.h"
    #if wxUSE_GUI
        #include "wx/toplevel.h"
        #include "wx/font.h"
    #endif
#endif

#include "wx/apptrait.h"

#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// #include "MoreFilesX.h"

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5
    #include <AudioToolbox/AudioServices.h>
#endif

#include "wx/osx/private.h"
#if wxUSE_GUI
    #include "wx/osx/private/timer.h"
#endif // wxUSE_GUI

#include "wx/evtloop.h"

#if defined(__MWERKS__) && wxUSE_UNICODE
#if __MWERKS__ < 0x4100
    #include <wtime.h>
#endif
#endif

#if wxUSE_BASE

// Emit a beeeeeep
void wxBell()
{
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5
    if ( AudioServicesPlayAlertSound != NULL )
        AudioServicesPlayAlertSound(kUserPreferredAlert);
    else
#endif
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
        AlertSoundPlay();
#else
    {
    }
#endif
}

#endif // wxUSE_BASE

#if wxUSE_GUI

wxTimerImpl* wxGUIAppTraits::CreateTimerImpl(wxTimer *timer)
{
    return new wxOSXTimerImpl(timer);
}

int gs_wxBusyCursorCount = 0;
extern wxCursor    gMacCurrentCursor;
wxCursor        gMacStoredActiveCursor;

// Set the cursor to the busy cursor for all windows
void wxBeginBusyCursor(const wxCursor *cursor)
{
    if (gs_wxBusyCursorCount++ == 0)
    {
        gMacStoredActiveCursor = gMacCurrentCursor;
        cursor->MacInstall();

        wxSetCursor(*cursor);
    }
    //else: nothing to do, already set
}

// Restore cursor to normal
void wxEndBusyCursor()
{
    wxCHECK_RET( gs_wxBusyCursorCount > 0,
        wxT("no matching wxBeginBusyCursor() for wxEndBusyCursor()") );

    if (--gs_wxBusyCursorCount == 0)
    {
        gMacStoredActiveCursor.MacInstall();
        gMacStoredActiveCursor = wxNullCursor;

        wxSetCursor(wxNullCursor);
    }
}

// true if we're between the above two calls
bool wxIsBusy()
{
    return (gs_wxBusyCursorCount > 0);
}

#endif // wxUSE_GUI

#if wxUSE_BASE

wxString wxMacFindFolderNoSeparator( short        vol,
              OSType       folderType,
              Boolean      createFolder)
{
    FSRef fsRef;
    wxString strDir;

    if ( FSFindFolder( vol, folderType, createFolder, &fsRef) == noErr)
    {
        strDir = wxMacFSRefToPath( &fsRef );
    }

    return strDir;
}

wxString wxMacFindFolder( short        vol,
              OSType       folderType,
              Boolean      createFolder)
{
    return wxMacFindFolderNoSeparator(vol, folderType, createFolder) + wxFILE_SEP_PATH;
}

#endif // wxUSE_BASE


// ============================================================================
// GUI-only functions from now on
// ============================================================================

#if wxUSE_GUI

// ----------------------------------------------------------------------------
// Miscellaneous functions
// ----------------------------------------------------------------------------

void wxGetMousePosition( int* x, int* y )
{
    Point pt;
    GetGlobalMouse(&pt);
    if ( x )
        *x = pt.h;
    if ( y )
        *y = pt.v;
};

void wxClientDisplayRect(int *x, int *y, int *width, int *height)
{
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
    HIRect bounds ;
    HIWindowGetAvailablePositioningBounds(kCGNullDirectDisplay,kHICoordSpace72DPIGlobal,
            &bounds);
    if ( x )
        *x = bounds.origin.x;
    if ( y )
        *y = bounds.origin.y;
    if ( width )
        *width = bounds.size.width;
    if ( height )
        *height = bounds.size.height;
#else
    Rect r;
    GetAvailableWindowPositioningBounds( GetMainDevice() , &r );
    if ( x )
        *x = r.left;
    if ( y )
        *y = r.top;
    if ( width )
        *width = r.right - r.left;
    if ( height )
        *height = r.bottom - r.top;
#endif
}

#endif // wxUSE_GUI

#if wxUSE_GUI

// ----------------------------------------------------------------------------
// Native Struct Conversions
// ----------------------------------------------------------------------------

void wxMacRectToNative( const wxRect *wx , Rect *n )
{
    n->left = wx->x;
    n->top = wx->y;
    n->right = wx->x + wx->width;
    n->bottom = wx->y + wx->height;
}

void wxMacNativeToRect( const Rect *n , wxRect* wx )
{
    wx->x = n->left;
    wx->y = n->top;
    wx->width = n->right - n->left;
    wx->height = n->bottom - n->top;
}

void wxMacPointToNative( const wxPoint* wx , Point *n )
{
    n->h = wx->x;
    n->v = wx->y;
}

void wxMacNativeToPoint( const Point *n , wxPoint* wx )
{
    wx->x = n->h;
    wx->y = n->v;
}

// ----------------------------------------------------------------------------
// Carbon Event Support
// ----------------------------------------------------------------------------

OSStatus wxMacCarbonEvent::GetParameter(EventParamName inName, EventParamType inDesiredType, UInt32 inBufferSize, void * outData)
{
    return ::GetEventParameter( m_eventRef , inName , inDesiredType , NULL , inBufferSize , NULL , outData );
}

OSStatus wxMacCarbonEvent::SetParameter(EventParamName inName, EventParamType inType, UInt32 inBufferSize, const void * inData)
{
    return ::SetEventParameter( m_eventRef , inName , inType , inBufferSize , inData );
}

// ----------------------------------------------------------------------------
// Control Access Support
// ----------------------------------------------------------------------------


// ============================================================================
// DataBrowser Wrapper
// ============================================================================
//
// basing on DataBrowserItemIDs
//

IMPLEMENT_ABSTRACT_CLASS( wxMacDataBrowserControl , wxMacControl )

pascal void wxMacDataBrowserControl::DataBrowserItemNotificationProc(
    ControlRef browser,
    DataBrowserItemID itemID,
    DataBrowserItemNotification message,
    DataBrowserItemDataRef itemData )
{
    wxMacDataBrowserControl* ctl = wxDynamicCast(wxMacControl::GetReferenceFromNativeControl( browser ), wxMacDataBrowserControl);
    if ( ctl != 0 )
    {
        ctl->ItemNotification(itemID, message, itemData);
    }
}

pascal OSStatus wxMacDataBrowserControl::DataBrowserGetSetItemDataProc(
    ControlRef browser,
    DataBrowserItemID itemID,
    DataBrowserPropertyID property,
    DataBrowserItemDataRef itemData,
    Boolean changeValue )
{
    OSStatus err = errDataBrowserPropertyNotSupported;
    wxMacDataBrowserControl* ctl = wxDynamicCast(wxMacControl::GetReferenceFromNativeControl( browser ), wxMacDataBrowserControl);
    if ( ctl != 0 )
    {
        err = ctl->GetSetItemData(itemID, property, itemData, changeValue);
    }
    return err;
}

pascal Boolean wxMacDataBrowserControl::DataBrowserCompareProc(
    ControlRef browser,
    DataBrowserItemID itemOneID,
    DataBrowserItemID itemTwoID,
    DataBrowserPropertyID sortProperty)
{
    wxMacDataBrowserControl* ctl = wxDynamicCast(wxMacControl::GetReferenceFromNativeControl( browser ), wxMacDataBrowserControl);
    if ( ctl != 0 )
    {
        return ctl->CompareItems(itemOneID, itemTwoID, sortProperty);
    }
    return false;
}

DataBrowserItemDataUPP gDataBrowserItemDataUPP = NULL;
DataBrowserItemNotificationUPP gDataBrowserItemNotificationUPP = NULL;
DataBrowserItemCompareUPP gDataBrowserItemCompareUPP = NULL;

wxMacDataBrowserControl::wxMacDataBrowserControl( wxWindow* peer,
                                                  const wxPoint& pos,
                                                  const wxSize& size,
                                                  long WXUNUSED(style))
                       : wxMacControl( peer )
{
    Rect bounds = wxMacGetBoundsForControl( peer, pos, size );
    OSStatus err = ::CreateDataBrowserControl(
        MAC_WXHWND(peer->MacGetTopLevelWindowRef()),
        &bounds, kDataBrowserListView, &m_controlRef );
    SetReferenceInNativeControl();
    verify_noerr( err );
    if ( gDataBrowserItemCompareUPP == NULL )
        gDataBrowserItemCompareUPP = NewDataBrowserItemCompareUPP(DataBrowserCompareProc);
    if ( gDataBrowserItemDataUPP == NULL )
        gDataBrowserItemDataUPP = NewDataBrowserItemDataUPP(DataBrowserGetSetItemDataProc);
    if ( gDataBrowserItemNotificationUPP == NULL )
    {
        gDataBrowserItemNotificationUPP =
            (DataBrowserItemNotificationUPP) NewDataBrowserItemNotificationWithItemUPP(DataBrowserItemNotificationProc);
    }

    DataBrowserCallbacks callbacks;
    InitializeDataBrowserCallbacks( &callbacks, kDataBrowserLatestCallbacks );

    callbacks.u.v1.itemDataCallback = gDataBrowserItemDataUPP;
    callbacks.u.v1.itemCompareCallback = gDataBrowserItemCompareUPP;
    callbacks.u.v1.itemNotificationCallback = gDataBrowserItemNotificationUPP;
    SetCallbacks( &callbacks );

}

OSStatus wxMacDataBrowserControl::GetItemCount( DataBrowserItemID container,
    Boolean recurse,
    DataBrowserItemState state,
    ItemCount *numItems) const
{
    return GetDataBrowserItemCount( m_controlRef, container, recurse, state, numItems );
}

OSStatus wxMacDataBrowserControl::GetItems( DataBrowserItemID container,
    Boolean recurse,
    DataBrowserItemState state,
    Handle items) const
{
    return GetDataBrowserItems( m_controlRef, container, recurse, state, items );
}

OSStatus wxMacDataBrowserControl::SetSelectionFlags( DataBrowserSelectionFlags options )
{
    return SetDataBrowserSelectionFlags( m_controlRef, options );
}

OSStatus wxMacDataBrowserControl::AddColumn( DataBrowserListViewColumnDesc *columnDesc,
        DataBrowserTableViewColumnIndex position )
{
    return AddDataBrowserListViewColumn( m_controlRef, columnDesc, position );
}

OSStatus wxMacDataBrowserControl::GetColumnIDFromIndex( DataBrowserTableViewColumnIndex position, DataBrowserTableViewColumnID* id )
{
    return GetDataBrowserTableViewColumnProperty( m_controlRef, position, id );
}

OSStatus wxMacDataBrowserControl::RemoveColumn( DataBrowserTableViewColumnIndex position )
{
    DataBrowserTableViewColumnID id;
    GetColumnIDFromIndex( position, &id );
    return RemoveDataBrowserTableViewColumn( m_controlRef, id );
}

OSStatus wxMacDataBrowserControl::AutoSizeColumns()
{
    return AutoSizeDataBrowserListViewColumns(m_controlRef);
}

OSStatus wxMacDataBrowserControl::SetHasScrollBars( bool horiz, bool vert )
{
    return SetDataBrowserHasScrollBars( m_controlRef, horiz, vert );
}

OSStatus wxMacDataBrowserControl::SetHiliteStyle( DataBrowserTableViewHiliteStyle hiliteStyle )
{
    return SetDataBrowserTableViewHiliteStyle( m_controlRef, hiliteStyle );
}

OSStatus wxMacDataBrowserControl::SetHeaderButtonHeight(UInt16 height)
{
    return SetDataBrowserListViewHeaderBtnHeight( m_controlRef, height );
}

OSStatus wxMacDataBrowserControl::GetHeaderButtonHeight(UInt16 *height)
{
    return GetDataBrowserListViewHeaderBtnHeight( m_controlRef, height );
}

OSStatus wxMacDataBrowserControl::SetCallbacks(const DataBrowserCallbacks *callbacks)
{
    return SetDataBrowserCallbacks( m_controlRef, callbacks );
}

OSStatus wxMacDataBrowserControl::UpdateItems(
    DataBrowserItemID container,
    UInt32 numItems,
    const DataBrowserItemID *items,
    DataBrowserPropertyID preSortProperty,
    DataBrowserPropertyID propertyID ) const
{
    return UpdateDataBrowserItems( m_controlRef, container, numItems, items, preSortProperty, propertyID );
}

bool wxMacDataBrowserControl::IsItemSelected( DataBrowserItemID item ) const
{
    return IsDataBrowserItemSelected( m_controlRef, item );
}

OSStatus wxMacDataBrowserControl::AddItems(
    DataBrowserItemID container,
    UInt32 numItems,
    const DataBrowserItemID *items,
    DataBrowserPropertyID preSortProperty )
{
    return AddDataBrowserItems( m_controlRef, container, numItems, items, preSortProperty );
}

OSStatus wxMacDataBrowserControl::RemoveItems(
    DataBrowserItemID container,
    UInt32 numItems,
    const DataBrowserItemID *items,
    DataBrowserPropertyID preSortProperty )
{
    return RemoveDataBrowserItems( m_controlRef, container, numItems, items, preSortProperty );
}

OSStatus wxMacDataBrowserControl::RevealItem(
    DataBrowserItemID item,
    DataBrowserPropertyID propertyID,
    DataBrowserRevealOptions options ) const
{
    return RevealDataBrowserItem( m_controlRef, item, propertyID, options );
}

OSStatus wxMacDataBrowserControl::SetSelectedItems(
    UInt32 numItems,
    const DataBrowserItemID *items,
    DataBrowserSetOption operation )
{
    return SetDataBrowserSelectedItems( m_controlRef, numItems, items, operation );
}

OSStatus wxMacDataBrowserControl::GetSelectionAnchor( DataBrowserItemID *first, DataBrowserItemID *last ) const
{
    return GetDataBrowserSelectionAnchor( m_controlRef, first, last );
}

OSStatus wxMacDataBrowserControl::GetItemID( DataBrowserTableViewRowIndex row, DataBrowserItemID * item ) const
{
    return GetDataBrowserTableViewItemID( m_controlRef, row, item );
}

OSStatus wxMacDataBrowserControl::GetItemRow( DataBrowserItemID item, DataBrowserTableViewRowIndex * row ) const
{
    return GetDataBrowserTableViewItemRow( m_controlRef, item, row );
}

OSStatus wxMacDataBrowserControl::SetDefaultRowHeight( UInt16 height )
{
    return SetDataBrowserTableViewRowHeight( m_controlRef , height );
}

OSStatus wxMacDataBrowserControl::GetDefaultRowHeight( UInt16 * height ) const
{
    return GetDataBrowserTableViewRowHeight( m_controlRef, height );
}

OSStatus wxMacDataBrowserControl::SetRowHeight( DataBrowserItemID item , UInt16 height)
{
    return SetDataBrowserTableViewItemRowHeight( m_controlRef, item , height );
}

OSStatus wxMacDataBrowserControl::GetRowHeight( DataBrowserItemID item , UInt16 *height) const
{
    return GetDataBrowserTableViewItemRowHeight( m_controlRef, item , height);
}

OSStatus wxMacDataBrowserControl::GetColumnWidth( DataBrowserPropertyID column , UInt16 *width ) const
{
    return GetDataBrowserTableViewNamedColumnWidth( m_controlRef , column , width );
}

OSStatus wxMacDataBrowserControl::SetColumnWidth( DataBrowserPropertyID column , UInt16 width )
{
    return SetDataBrowserTableViewNamedColumnWidth( m_controlRef , column , width );
}

OSStatus wxMacDataBrowserControl::GetDefaultColumnWidth( UInt16 *width ) const
{
    return GetDataBrowserTableViewColumnWidth( m_controlRef , width );
}

OSStatus wxMacDataBrowserControl::SetDefaultColumnWidth( UInt16 width )
{
    return SetDataBrowserTableViewColumnWidth( m_controlRef , width );
}

OSStatus wxMacDataBrowserControl::GetColumnCount(UInt32* numColumns) const
{
    return GetDataBrowserTableViewColumnCount( m_controlRef, numColumns);
}

OSStatus wxMacDataBrowserControl::GetColumnPosition( DataBrowserPropertyID column,
    DataBrowserTableViewColumnIndex *position) const
{
    return GetDataBrowserTableViewColumnPosition( m_controlRef , column , position);
}

OSStatus wxMacDataBrowserControl::SetColumnPosition( DataBrowserPropertyID column, DataBrowserTableViewColumnIndex position)
{
    return SetDataBrowserTableViewColumnPosition( m_controlRef , column , position);
}

OSStatus wxMacDataBrowserControl::GetScrollPosition( UInt32 *top , UInt32 *left ) const
{
    return GetDataBrowserScrollPosition( m_controlRef , top , left );
}

OSStatus wxMacDataBrowserControl::SetScrollPosition( UInt32 top , UInt32 left )
{
    return SetDataBrowserScrollPosition( m_controlRef , top , left );
}

OSStatus wxMacDataBrowserControl::GetSortProperty( DataBrowserPropertyID *column ) const
{
    return GetDataBrowserSortProperty( m_controlRef , column );
}

OSStatus wxMacDataBrowserControl::SetSortProperty( DataBrowserPropertyID column )
{
    return SetDataBrowserSortProperty( m_controlRef , column );
}

OSStatus wxMacDataBrowserControl::GetSortOrder( DataBrowserSortOrder *order ) const
{
    return GetDataBrowserSortOrder( m_controlRef , order );
}

OSStatus wxMacDataBrowserControl::SetSortOrder( DataBrowserSortOrder order )
{
    return SetDataBrowserSortOrder( m_controlRef , order );
}

OSStatus wxMacDataBrowserControl::GetPropertyFlags( DataBrowserPropertyID property,
    DataBrowserPropertyFlags *flags ) const
{
    return GetDataBrowserPropertyFlags( m_controlRef , property , flags );
}

OSStatus wxMacDataBrowserControl::SetPropertyFlags( DataBrowserPropertyID property,
    DataBrowserPropertyFlags flags )
{
    return SetDataBrowserPropertyFlags( m_controlRef , property , flags );
}

OSStatus wxMacDataBrowserControl::GetHeaderDesc( DataBrowserPropertyID property,
    DataBrowserListViewHeaderDesc *desc ) const
{
    return GetDataBrowserListViewHeaderDesc( m_controlRef , property , desc );
}

OSStatus wxMacDataBrowserControl::SetHeaderDesc( DataBrowserPropertyID property,
    DataBrowserListViewHeaderDesc *desc )
{
    return SetDataBrowserListViewHeaderDesc( m_controlRef , property , desc );
}

OSStatus wxMacDataBrowserControl::SetDisclosureColumn( DataBrowserPropertyID property ,
    Boolean expandableRows )
{
    return SetDataBrowserListViewDisclosureColumn( m_controlRef, property, expandableRows);
}

OSStatus wxMacDataBrowserControl::GetItemPartBounds( DataBrowserItemID item, DataBrowserPropertyID property, DataBrowserPropertyPart part, Rect * bounds )
{
    return GetDataBrowserItemPartBounds( m_controlRef, item, property, part, bounds);
}

// ============================================================================
// Higher-level Databrowser
// ============================================================================
//
// basing on data item objects
//

wxMacDataItem::wxMacDataItem()
{
//    m_data = NULL;

    m_order = 0;
//    m_colId = kTextColumnId; // for compat with existing wx*ListBox impls.
}

wxMacDataItem::~wxMacDataItem()
{
}

void wxMacDataItem::SetOrder( SInt32 order )
{
    m_order = order;
}

SInt32 wxMacDataItem::GetOrder() const
{
    return m_order;
}
/*
void wxMacDataItem::SetData( void* data)
{
    m_data = data;
}

void* wxMacDataItem::GetData() const
{
    return m_data;
}

short wxMacDataItem::GetColumn()
{
    return m_colId;
}

void wxMacDataItem::SetColumn( short col )
{
    m_colId = col;
}

void wxMacDataItem::SetLabel( const wxString& str)
{
    m_label = str;
    m_cfLabel = wxCFStringRef( str , wxLocale::GetSystemEncoding());
}

const wxString& wxMacDataItem::GetLabel() const
{
    return m_label;
}
*/

bool wxMacDataItem::IsLessThan(wxMacDataItemBrowserControl *WXUNUSED(owner) ,
    const wxMacDataItem* rhs,
    DataBrowserPropertyID sortProperty) const
{
    bool retval = false;

    if ( sortProperty == kNumericOrderColumnId )
        retval = m_order < rhs->m_order;

    return retval;
}

OSStatus wxMacDataItem::GetSetData( wxMacDataItemBrowserControl *WXUNUSED(owner) ,
    DataBrowserPropertyID property,
    DataBrowserItemDataRef itemData,
    bool changeValue )
{
    OSStatus err = errDataBrowserPropertyNotSupported;
    if ( !changeValue )
    {
        if ( property == kNumericOrderColumnId )
        {
            err = ::SetDataBrowserItemDataValue( itemData, m_order );
            err = noErr;
        }
    }

    return err;
}

void wxMacDataItem::Notification(wxMacDataItemBrowserControl *WXUNUSED(owner) ,
    DataBrowserItemNotification WXUNUSED(message),
    DataBrowserItemDataRef WXUNUSED(itemData) ) const
{
}

IMPLEMENT_DYNAMIC_CLASS( wxMacDataItemBrowserControl , wxMacDataBrowserControl )

wxMacDataItemBrowserControl::wxMacDataItemBrowserControl( wxWindow* peer , const wxPoint& pos, const wxSize& size, long style) :
    wxMacDataBrowserControl( peer, pos, size, style )
{
    m_suppressSelection = false;
    m_sortOrder = SortOrder_None;
    m_clientDataItemsType = wxClientData_None;
}

wxMacDataItemBrowserSelectionSuppressor::wxMacDataItemBrowserSelectionSuppressor(wxMacDataItemBrowserControl *browser)
{
    m_former = browser->SuppressSelection(true);
    m_browser = browser;
}

wxMacDataItemBrowserSelectionSuppressor::~wxMacDataItemBrowserSelectionSuppressor()
{
    m_browser->SuppressSelection(m_former);
}

bool  wxMacDataItemBrowserControl::SuppressSelection( bool suppress )
{
    bool former = m_suppressSelection;
    m_suppressSelection = suppress;

    return former;
}

Boolean wxMacDataItemBrowserControl::CompareItems(DataBrowserItemID itemOneID,
    DataBrowserItemID itemTwoID,
    DataBrowserPropertyID sortProperty)
{
    wxMacDataItem* itemOne = (wxMacDataItem*) itemOneID;
    wxMacDataItem* itemTwo = (wxMacDataItem*) itemTwoID;

    Boolean retval = false;
    if ( itemOne != NULL )
        retval = itemOne->IsLessThan( this , itemTwo , sortProperty);
    return retval;
}

OSStatus wxMacDataItemBrowserControl::GetSetItemData(
    DataBrowserItemID itemID,
    DataBrowserPropertyID property,
    DataBrowserItemDataRef itemData,
    Boolean changeValue )
{
    wxMacDataItem* item = (wxMacDataItem*) itemID;
    OSStatus err = errDataBrowserPropertyNotSupported;
    switch( property )
    {
        case kDataBrowserContainerIsClosableProperty :
        case kDataBrowserContainerIsSortableProperty :
        case kDataBrowserContainerIsOpenableProperty :
            // right now default behaviour on these
            break;
        default :

            if ( item != NULL ){
                err = item->GetSetData( this, property , itemData , changeValue );
            }
            break;

    }
    return err;
}

void wxMacDataItemBrowserControl::ItemNotification(
    DataBrowserItemID itemID,
    DataBrowserItemNotification message,
    DataBrowserItemDataRef itemData)
{
    wxMacDataItem* item = (wxMacDataItem*) itemID;
    if (item != NULL)
        item->Notification( this, message, itemData);
}

unsigned int wxMacDataItemBrowserControl::GetItemCount(const wxMacDataItem* container,
        bool recurse , DataBrowserItemState state) const
{
    ItemCount numItems = 0;
    verify_noerr( wxMacDataBrowserControl::GetItemCount( (DataBrowserItemID)container,
        recurse, state, &numItems ) );
    return numItems;
}

unsigned int wxMacDataItemBrowserControl::GetSelectedItemCount( const wxMacDataItem* container,
        bool recurse ) const
{
    return GetItemCount( container, recurse, kDataBrowserItemIsSelected );

}

void wxMacDataItemBrowserControl::GetItems(const wxMacDataItem* container,
    bool recurse , DataBrowserItemState state, wxArrayMacDataItemPtr &items) const
{
    Handle handle = NewHandle(0);
    verify_noerr( wxMacDataBrowserControl::GetItems( (DataBrowserItemID)container ,
        recurse , state, handle) );

    int itemCount = GetHandleSize(handle)/sizeof(DataBrowserItemID);
    HLock( handle );
    wxMacDataItemPtr* itemsArray = (wxMacDataItemPtr*) *handle;
    for ( int i = 0; i < itemCount; ++i)
    {
        items.Add(itemsArray[i]);
    }
    HUnlock( handle );
    DisposeHandle( handle );
}

unsigned int wxMacDataItemBrowserControl::GetLineFromItem(const wxMacDataItem* item) const
{
    DataBrowserTableViewRowIndex row;
    OSStatus err = GetItemRow( (DataBrowserItemID) item , &row);
    wxCHECK( err == noErr, (unsigned)-1 );
    return row;
}

wxMacDataItem*  wxMacDataItemBrowserControl::GetItemFromLine(unsigned int n) const
{
    DataBrowserItemID id;
    OSStatus err =  GetItemID( (DataBrowserTableViewRowIndex) n , &id);
    wxCHECK( err == noErr, NULL );
    return (wxMacDataItem*) id;
}

void wxMacDataItemBrowserControl::UpdateItem(const wxMacDataItem *container,
        const wxMacDataItem *item , DataBrowserPropertyID property) const
{
    verify_noerr( wxMacDataBrowserControl::UpdateItems((DataBrowserItemID)container, 1,
        (DataBrowserItemID*) &item, kDataBrowserItemNoProperty /* notSorted */, property ) );
}

void wxMacDataItemBrowserControl::UpdateItems(const wxMacDataItem *container,
        wxArrayMacDataItemPtr &itemArray , DataBrowserPropertyID property) const
{
    unsigned int noItems = itemArray.GetCount();
    DataBrowserItemID *items = new DataBrowserItemID[noItems];
    for ( unsigned int i = 0; i < noItems; ++i )
        items[i] = (DataBrowserItemID) itemArray[i];

    verify_noerr( wxMacDataBrowserControl::UpdateItems((DataBrowserItemID)container, noItems,
        items, kDataBrowserItemNoProperty /* notSorted */, property ) );
    delete [] items;
}

static int column_id_counter = 0;

void wxMacDataItemBrowserControl::InsertColumn(int col, DataBrowserPropertyType colType,
                                            const wxString& title, SInt16 just, int defaultWidth)
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

    DataBrowserPropertyID id = kMinColumnId + column_id_counter;
    column_id_counter++;

    columnDesc.propertyDesc.propertyID = id;
    columnDesc.propertyDesc.propertyType = colType;
    columnDesc.propertyDesc.propertyFlags = kDataBrowserListViewSortableColumn;
    columnDesc.propertyDesc.propertyFlags |= kDataBrowserListViewTypeSelectColumn;
    columnDesc.propertyDesc.propertyFlags |= kDataBrowserListViewNoGapForIconInHeaderButton;

    verify_noerr( AddColumn( &columnDesc, col ) );

    if (defaultWidth > 0){
        SetColumnWidth(col, defaultWidth);
    }
}

void wxMacDataItemBrowserControl::SetColumnWidth(int colId, int width)
{
    DataBrowserPropertyID id;
    GetColumnIDFromIndex(colId, &id);
    verify_noerr( wxMacDataBrowserControl::SetColumnWidth(id, width));
}

int wxMacDataItemBrowserControl::GetColumnWidth(int colId)
{
    DataBrowserPropertyID id;
    GetColumnIDFromIndex(colId, &id);
    UInt16 result;
    verify_noerr( wxMacDataBrowserControl::GetColumnWidth(id, &result));
    return result;
}

void wxMacDataItemBrowserControl::AddItem(wxMacDataItem *container, wxMacDataItem *item)
{
    verify_noerr( wxMacDataBrowserControl::AddItems( (DataBrowserItemID)container, 1,
        (DataBrowserItemID*) &item, kDataBrowserItemNoProperty ) );
}

void wxMacDataItemBrowserControl::AddItems(wxMacDataItem *container, wxArrayMacDataItemPtr &itemArray )
{
    unsigned int noItems = itemArray.GetCount();
    DataBrowserItemID *items = new DataBrowserItemID[noItems];
    for ( unsigned int i = 0; i < noItems; ++i )
        items[i] = (DataBrowserItemID) itemArray[i];

    verify_noerr( wxMacDataBrowserControl::AddItems( (DataBrowserItemID)container, noItems,
        (DataBrowserItemID*) items, kDataBrowserItemNoProperty ) );
    delete [] items;
}

void wxMacDataItemBrowserControl::RemoveItem(wxMacDataItem *container, wxMacDataItem* item)
{
    OSStatus err = wxMacDataBrowserControl::RemoveItems( (DataBrowserItemID)container, 1,
        (DataBrowserItemID*) &item, kDataBrowserItemNoProperty );
    verify_noerr( err );
}

void wxMacDataItemBrowserControl::RemoveItems(wxMacDataItem *container, wxArrayMacDataItemPtr &itemArray)
{
    unsigned int noItems = itemArray.GetCount();
    DataBrowserItemID *items = new DataBrowserItemID[noItems];
    for ( unsigned int i = 0; i < noItems; ++i )
        items[i] = (DataBrowserItemID) itemArray[i];

    OSStatus err = wxMacDataBrowserControl::RemoveItems( (DataBrowserItemID)container, noItems,
        (DataBrowserItemID*) items, kDataBrowserItemNoProperty );
    verify_noerr( err );
    delete [] items;
}

void wxMacDataItemBrowserControl::RemoveAllItems(wxMacDataItem *container)
{
    SetScrollPosition(0, 0);
    OSStatus err = wxMacDataBrowserControl::RemoveItems( (DataBrowserItemID)container, 0 , NULL , kDataBrowserItemNoProperty );
    verify_noerr( err );
}

void wxMacDataItemBrowserControl::SetSelectedItem(wxMacDataItem* item , DataBrowserSetOption option)
{
    verify_noerr(wxMacDataBrowserControl::SetSelectedItems( 1, (DataBrowserItemID*) &item, option ));
}

void wxMacDataItemBrowserControl::SetSelectedAllItems(DataBrowserSetOption option)
{
    verify_noerr(wxMacDataBrowserControl::SetSelectedItems( 0 , NULL , option ));
}

void wxMacDataItemBrowserControl::SetSelectedItems(wxArrayMacDataItemPtr &itemArray , DataBrowserSetOption option)
{
    unsigned int noItems = itemArray.GetCount();
    DataBrowserItemID *items = new DataBrowserItemID[noItems];
    for ( unsigned int i = 0; i < noItems; ++i )
        items[i] = (DataBrowserItemID) itemArray[i];

    verify_noerr(wxMacDataBrowserControl::SetSelectedItems( noItems, (DataBrowserItemID*) items, option ));
    delete [] items;
}

Boolean wxMacDataItemBrowserControl::IsItemSelected( const wxMacDataItem* item) const
{
    return wxMacDataBrowserControl::IsItemSelected( (DataBrowserItemID) item);
}

void wxMacDataItemBrowserControl::RevealItem( wxMacDataItem* item, DataBrowserRevealOptions options)
{
    verify_noerr(wxMacDataBrowserControl::RevealItem( (DataBrowserItemID) item, kDataBrowserNoItem , options ) );
}

void wxMacDataItemBrowserControl::GetSelectionAnchor( wxMacDataItemPtr* first , wxMacDataItemPtr* last) const
{
    verify_noerr(wxMacDataBrowserControl::GetSelectionAnchor( (DataBrowserItemID*) first, (DataBrowserItemID*) last) );
}

wxClientDataType wxMacDataItemBrowserControl::GetClientDataType() const
{
     return m_clientDataItemsType;
}
void wxMacDataItemBrowserControl::SetClientDataType(wxClientDataType clientDataItemsType)
{
    m_clientDataItemsType = clientDataItemsType;
}

void wxMacDataItemBrowserControl::MacDelete( unsigned int n )
{
    wxMacDataItem* item = (wxMacDataItem*)GetItemFromLine( n );
    RemoveItem( wxMacDataBrowserRootContainer, item );
}

void wxMacDataItemBrowserControl::MacInsert( unsigned int n, wxMacDataItem* item)
{
    if ( m_sortOrder == SortOrder_None )
    {

        // increase the order of the lines to be shifted
        unsigned int lines = MacGetCount();
        for ( unsigned int i = n; i < lines; ++i)
        {
            wxMacDataItem* iter = (wxMacDataItem*) GetItemFromLine(i);
            iter->SetOrder( iter->GetOrder() + 1 );
        }

#if 0
        // I don't understand what this code is supposed to do, RR.
        SInt32 frontLineOrder = 0;
        if ( n > 0 )
        {
            wxMacDataItem* iter = (wxMacDataItem*) GetItemFromLine(n-1);
            frontLineOrder = iter->GetOrder()+1;
        }
#else
        item->SetOrder( n );
#endif
    }

    AddItem( wxMacDataBrowserRootContainer, item );
}

void wxMacDataItemBrowserControl::MacClear()
{
    wxMacDataItemBrowserSelectionSuppressor suppressor(this);
    RemoveAllItems(wxMacDataBrowserRootContainer);
}

unsigned int wxMacDataItemBrowserControl::MacGetCount() const
{
    return GetItemCount(wxMacDataBrowserRootContainer,false,kDataBrowserItemAnyState);
}

#endif // wxUSE_GUI

