/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/dataview.cpp
// Purpose:     wxDataViewCtrl native carbon implementation
// Author:
// Id:          $Id: dataview.cpp 58317 2009-01-23
// Copyright:   (c) 2009
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if (wxUSE_DATAVIEWCTRL == 1) && !defined(wxUSE_GENERICDATAVIEWCTRL)

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/toplevel.h"
    #include "wx/font.h"
    #include "wx/settings.h"
    #include "wx/utils.h"
#endif

#include "wx/osx/carbon/dataview.h"
#include "wx/osx/private.h"
#include "wx/osx/uma.h"
#include "wx/renderer.h"

#include <limits>

// ============================================================================
// Variables used locally in dataview.cpp
// ============================================================================
static DataBrowserGetContextualMenuUPP gDataBrowserTableViewGetContextualMenuUPP = NULL;
static DataBrowserItemCompareUPP       gDataBrowserTableViewItemCompareUPP       = NULL;
static DataBrowserItemDataUPP          gDataBrowserTableViewItemDataUPP          = NULL;
static DataBrowserItemNotificationUPP  gDataBrowserTableViewItemNotificationUPP  = NULL;

static DataBrowserAcceptDragUPP  gDataBrowserTableViewAcceptDragUPP  = NULL;
static DataBrowserAddDragItemUPP gDataBrowserTableViewAddDragItemUPP = NULL;
static DataBrowserReceiveDragUPP gDataBrowserTableViewReceiveDragUPP = NULL;

static DataBrowserDrawItemUPP gDataBrowserTableViewDrawItemUPP = NULL;
static DataBrowserEditItemUPP gDataBrowserTableViewEditItemUPP = NULL;
static DataBrowserHitTestUPP  gDataBrowserTableViewHitTestUPP  = NULL;
static DataBrowserTrackingUPP gDataBrowserTableViewTrackingUPP = NULL;

// ============================================================================
// Functions used locally in dataview.cpp
// ============================================================================
static DataBrowserItemID* CreateDataBrowserItemIDArray(size_t& noOfEntries, wxDataViewItemArray const& items) // returns a newly allocated pointer to valid data browser item IDs
{
  size_t const noOfItems = items.GetCount();

  DataBrowserItemID* itemIDs(new DataBrowserItemID[noOfItems]);


 // convert all valid data view items to data browser items
  noOfEntries = 0;
  for (size_t i=0; i<noOfItems; ++i)
    if (items[i].IsOk())
    {
      itemIDs[noOfEntries] = reinterpret_cast<DataBrowserItemID>(items[i].GetID());
      ++noOfEntries;
    }
 // done:
  return itemIDs;
}

static const EventTypeSpec eventList[] =
{
    { kEventClassControl, kEventControlHit },
    { kEventClassControl, kEventControlDraw }
};

static pascal OSStatus DataBrowserCtrlEventHandler(EventHandlerCallRef handler, EventRef EventReference, void* Data)
{
  wxDataViewCtrl* DataViewCtrlPtr((wxDataViewCtrl*) Data); // the 'Data' variable always contains a pointer to the data view control that installed the handler

  wxMacCarbonEvent CarbonEvent(EventReference) ;


  switch (GetEventKind(EventReference))
  {
    case kEventControlDraw:
      {
        OSStatus status;

        DataViewCtrlPtr->MacSetDrawingContext(CarbonEvent.GetParameter<CGContextRef>(kEventParamCGContextRef,typeCGContextRef));
        status = ::CallNextEventHandler(handler,EventReference);
        DataViewCtrlPtr->MacSetDrawingContext(NULL);
        return status;
      }
    case kEventControlHit :
      if (CarbonEvent.GetParameter<ControlPartCode>(kEventParamControlPart,typeControlPartCode) == kControlButtonPart) // we only care about the header
      {
        ControlRef            controlReference;
        DataBrowserPropertyID columnPropertyID;
        DataBrowserSortOrder  order;
        unsigned long         columnIndex;
        wxDataViewColumn*     column;
        OSStatus              status;
        wxDataViewEvent       DataViewEvent(wxEVT_COMMAND_DATAVIEW_COLUMN_HEADER_CLICK,DataViewCtrlPtr->GetId());

        CarbonEvent.GetParameter(kEventParamDirectObject,&controlReference);
       // determine the column that triggered the event (this is the column that is responsible for sorting the data view):
        status = ::GetDataBrowserSortProperty(controlReference,&columnPropertyID);
        wxCHECK(status == noErr,status);
        status = ::GetDataBrowserTableViewColumnPosition(controlReference,columnPropertyID,&columnIndex);
        if (status == errDataBrowserPropertyNotFound) // user clicked into part of the header that does not have a property
          return ::CallNextEventHandler(handler,EventReference);
        wxCHECK(status == noErr,status);
        column = DataViewCtrlPtr->GetColumn(columnIndex);
       // set the column sort order:
        status = ::GetDataBrowserSortOrder(controlReference,&order);
        wxCHECK(status == noErr,status);
        column->SetSortOrderVariable(order == kDataBrowserOrderIncreasing);
       // initialize wxWidget event handler:
        DataViewEvent.SetEventObject(DataViewCtrlPtr);
        DataViewEvent.SetColumn(columnIndex);
        DataViewEvent.SetDataViewColumn(column);
       // finally sent the equivalent wxWidget event:
        DataViewCtrlPtr->HandleWindowEvent(DataViewEvent);
        return ::CallNextEventHandler(handler,EventReference);
      }
      else
        return eventNotHandledErr;
  }
  return eventNotHandledErr;
}

static bool InitializeColumnDescription(DataBrowserListViewColumnDesc& columnDescription, wxDataViewColumn const* columnPtr, wxCFStringRef const& title)
{
 // set properties for the column:
  columnDescription.propertyDesc.propertyID    = columnPtr->GetNativeData()->GetPropertyID();
  columnDescription.propertyDesc.propertyType  = columnPtr->GetRenderer()->GetNativeData()->GetPropertyType();
  columnDescription.propertyDesc.propertyFlags = kDataBrowserListViewSelectionColumn; // make the column selectable
  if (columnPtr->IsReorderable())
    columnDescription.propertyDesc.propertyFlags |= kDataBrowserListViewMovableColumn;
  if (columnPtr->IsResizeable())
  {
    columnDescription.headerBtnDesc.minimumWidth = 0;
    columnDescription.headerBtnDesc.maximumWidth = 30000; // 32767 is the theoretical maximum though but 30000 looks nicer
  }
  else
  {
    columnDescription.headerBtnDesc.minimumWidth = columnPtr->GetWidth();
    columnDescription.headerBtnDesc.maximumWidth = columnPtr->GetWidth();
  }
  if (columnPtr->IsSortable())
    columnDescription.propertyDesc.propertyFlags |= kDataBrowserListViewSortableColumn;
  if ((columnPtr->GetRenderer()->GetMode() == wxDATAVIEW_CELL_EDITABLE) ||
      (columnPtr->GetRenderer()->GetMode() == wxDATAVIEW_CELL_ACTIVATABLE))
    columnDescription.propertyDesc.propertyFlags |= kDataBrowserPropertyIsEditable;
  if ((columnDescription.propertyDesc.propertyType == kDataBrowserCustomType) ||
      (columnDescription.propertyDesc.propertyType == kDataBrowserDateTimeType) ||
      (columnDescription.propertyDesc.propertyType == kDataBrowserIconAndTextType) ||
      (columnDescription.propertyDesc.propertyType == kDataBrowserTextType))
    columnDescription.propertyDesc.propertyFlags |= kDataBrowserListViewTypeSelectColumn; // enables generally the possibility to have user input for the mentioned types
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
  columnDescription.propertyDesc.propertyFlags |= kDataBrowserListViewNoGapForIconInHeaderButton;
#endif
 // set header's properties:
  columnDescription.headerBtnDesc.version            = kDataBrowserListViewLatestHeaderDesc;
  columnDescription.headerBtnDesc.titleOffset        = 0;
  columnDescription.headerBtnDesc.titleString        = ::CFStringCreateCopy(kCFAllocatorDefault,title);
  columnDescription.headerBtnDesc.initialOrder       = kDataBrowserOrderIncreasing; // choose one of the orders as "undefined" is not supported anyway (s. ControlDefs.h in the HIToolbox framework)
  columnDescription.headerBtnDesc.btnFontStyle.flags = kControlUseFontMask | kControlUseJustMask;
  switch (columnPtr->GetAlignment())
  {
    case wxALIGN_CENTER:
    case wxALIGN_CENTER_HORIZONTAL:
      columnDescription.headerBtnDesc.btnFontStyle.just = teCenter;
      break;
    case wxALIGN_LEFT:
      columnDescription.headerBtnDesc.btnFontStyle.just = teFlushLeft;
      break;
    case wxALIGN_RIGHT:
      columnDescription.headerBtnDesc.btnFontStyle.just = teFlushRight;
      break;
    default:
      columnDescription.headerBtnDesc.btnFontStyle.just = teFlushDefault;
  }
  columnDescription.headerBtnDesc.btnFontStyle.font  = kControlFontViewSystemFont;
  columnDescription.headerBtnDesc.btnFontStyle.style = normal;
  if (columnPtr->GetBitmap().IsOk())
  {
    columnDescription.headerBtnDesc.btnContentInfo.contentType = kControlContentIconRef;
    columnDescription.headerBtnDesc.btnContentInfo.u.iconRef = columnPtr->GetBitmap().GetIconRef();
  }
  else
  {
    // not text only as we otherwise could not add a bitmap later
    // columnDescription.headerBtnDesc.btnContentInfo.contentType = kControlContentTextOnly;
    columnDescription.headerBtnDesc.btnContentInfo.contentType = kControlContentIconRef;
    columnDescription.headerBtnDesc.btnContentInfo.u.iconRef = NULL;
  }

 // done:
  return true;
}

// ============================================================================
// Type definitions of locally used function pointers
// ============================================================================
DEFINE_ONE_SHOT_HANDLER_GETTER(DataBrowserCtrlEventHandler)

// ============================================================================
// Helper functions for dataview implementation on OSX
// ============================================================================
wxWidgetImplType* CreateDataView(wxWindowMac* wxpeer, wxWindowMac* WXUNUSED(parent),  wxWindowID WXUNUSED(id), wxPoint const& pos, wxSize const& size,
                                 long style, long WXUNUSED(extraStyle))
{
  return new wxMacDataViewDataBrowserListViewControl(wxpeer,pos,size,style);
}

// ============================================================================
// wxMacDataBrowserTableViewControl
// ============================================================================
pascal Boolean wxMacDataBrowserTableViewControl::DataBrowserCompareProc(ControlRef browser, DataBrowserItemID itemOneID, DataBrowserItemID itemTwoID, DataBrowserPropertyID sortProperty)
{
  wxMacDataBrowserTableViewControl* ControlPtr(dynamic_cast<wxMacDataBrowserTableViewControl*>(wxMacControl::GetReferenceFromNativeControl(browser)));


  if (ControlPtr != NULL)
    return ControlPtr->DataBrowserCompareProc(itemOneID,itemTwoID,sortProperty);
  else
    return FALSE;
}

pascal void wxMacDataBrowserTableViewControl::DataBrowserGetContextualMenuProc(ControlRef browser, MenuRef* menu, UInt32* helpType, CFStringRef* helpItemString, AEDesc* selection)
{
  wxMacDataBrowserTableViewControl* ControlPtr(dynamic_cast<wxMacDataBrowserTableViewControl*>(wxMacControl::GetReferenceFromNativeControl(browser)));


  if (ControlPtr != NULL)
    ControlPtr->DataBrowserGetContextualMenuProc(menu,helpType,helpItemString,selection);
}

pascal OSStatus wxMacDataBrowserTableViewControl::DataBrowserGetSetItemDataProc(ControlRef browser, DataBrowserItemID itemID, DataBrowserPropertyID propertyID, DataBrowserItemDataRef itemData, Boolean getValue)
{
  wxMacDataBrowserTableViewControl* ControlPtr(dynamic_cast<wxMacDataBrowserTableViewControl*>(wxMacControl::GetReferenceFromNativeControl(browser)));


  if (ControlPtr != NULL)
    return ControlPtr->DataBrowserGetSetItemDataProc(itemID,propertyID,itemData,getValue);
  else
    return errDataBrowserPropertyNotSupported;
}

pascal void wxMacDataBrowserTableViewControl::DataBrowserItemNotificationProc(ControlRef browser, DataBrowserItemID itemID, DataBrowserItemNotification message, DataBrowserItemDataRef itemData)
{
  wxMacDataBrowserTableViewControl* ControlPtr(dynamic_cast<wxMacDataBrowserTableViewControl*>(wxMacControl::GetReferenceFromNativeControl(browser)));


  if (ControlPtr != NULL)
    ControlPtr->DataBrowserItemNotificationProc(itemID,message,itemData);
}

pascal void wxMacDataBrowserTableViewControl::DataBrowserDrawItemProc(ControlRef browser, DataBrowserItemID itemID, DataBrowserPropertyID propertyID, DataBrowserItemState state, Rect const* rectangle, SInt16 bitDepth, Boolean colorDevice)
{
  wxMacDataBrowserTableViewControl* ControlPtr(dynamic_cast<wxMacDataBrowserTableViewControl*>(wxMacControl::GetReferenceFromNativeControl(browser)));


  if (ControlPtr != NULL)
    ControlPtr->DataBrowserDrawItemProc(itemID,propertyID,state,rectangle,bitDepth,colorDevice);
}

pascal Boolean wxMacDataBrowserTableViewControl::DataBrowserEditItemProc(ControlRef browser, DataBrowserItemID itemID, DataBrowserPropertyID propertyID, CFStringRef theString, Rect* maxEditTextRect, Boolean* shrinkToFit)
{
  wxMacDataBrowserTableViewControl* ControlPtr(dynamic_cast<wxMacDataBrowserTableViewControl*>(wxMacControl::GetReferenceFromNativeControl(browser)));


  return ((ControlPtr != NULL) && ControlPtr->DataBrowserEditItemProc(itemID,propertyID,theString,maxEditTextRect,shrinkToFit));
}

pascal Boolean wxMacDataBrowserTableViewControl::DataBrowserHitTestProc(ControlRef browser, DataBrowserItemID itemID, DataBrowserPropertyID propertyID, Rect const* theRect, Rect const* mouseRect)
{
  wxMacDataBrowserTableViewControl* ControlPtr(dynamic_cast<wxMacDataBrowserTableViewControl*>(wxMacControl::GetReferenceFromNativeControl(browser)));


  return ((ControlPtr != NULL) && ControlPtr->DataBrowserHitTestProc(itemID,propertyID,theRect,mouseRect));
}

pascal DataBrowserTrackingResult wxMacDataBrowserTableViewControl::DataBrowserTrackingProc(ControlRef browser, DataBrowserItemID itemID, DataBrowserPropertyID propertyID, Rect const* theRect, Point startPt, EventModifiers modifiers)
{
  wxMacDataBrowserTableViewControl* ControlPtr(dynamic_cast<wxMacDataBrowserTableViewControl*>(wxMacControl::GetReferenceFromNativeControl(browser)));


  if (ControlPtr != NULL)
    return ControlPtr->DataBrowserTrackingProc(itemID,propertyID,theRect,startPt,modifiers);
  else
    return kDataBrowserNothingHit;
}

pascal Boolean wxMacDataBrowserTableViewControl::DataBrowserAcceptDragProc(ControlRef browser, DragReference dragRef, DataBrowserItemID itemID)
{
  wxMacDataBrowserTableViewControl* controlPtr(dynamic_cast<wxMacDataBrowserTableViewControl*>(wxMacControl::GetReferenceFromNativeControl(browser)));


  return ((controlPtr != NULL) && controlPtr->DataBrowserAcceptDragProc(dragRef,itemID));
}

pascal Boolean wxMacDataBrowserTableViewControl::DataBrowserAddDragItemProc(ControlRef browser, DragReference dragRef, DataBrowserItemID itemID, ItemReference* itemRef)
{
  wxMacDataBrowserTableViewControl* controlPtr(dynamic_cast<wxMacDataBrowserTableViewControl*>(wxMacControl::GetReferenceFromNativeControl(browser)));


  return ((controlPtr != NULL) && controlPtr->DataBrowserAddDragItemProc(dragRef,itemID,itemRef));
}

pascal Boolean wxMacDataBrowserTableViewControl::DataBrowserReceiveDragProc(ControlRef browser, DragReference dragRef, DataBrowserItemID itemID)
{
  wxMacDataBrowserTableViewControl* controlPtr(dynamic_cast<wxMacDataBrowserTableViewControl*>(wxMacControl::GetReferenceFromNativeControl(browser)));


  return ((controlPtr != NULL) && controlPtr->DataBrowserReceiveDragProc(dragRef,itemID));
}

wxMacDataBrowserTableViewControl::wxMacDataBrowserTableViewControl(wxWindow* peer, wxPoint const& pos, wxSize const& size, long style)
                                 :wxMacControl(peer)
{
  Rect bounds = wxMacGetBoundsForControl(peer,pos,size);
  OSStatus err = ::CreateDataBrowserControl(MAC_WXHWND(peer->MacGetTopLevelWindowRef()),&bounds,kDataBrowserListView,&(m_controlRef));


  SetReferenceInNativeControl();
  verify_noerr(err);

  ::InstallControlEventHandler(m_controlRef, GetDataBrowserCtrlEventHandlerUPP(), GetEventTypeCount(eventList), eventList, peer, (EventHandlerRef *)&m_macDataViewCtrlEventHandler);

 // setup standard callbacks:
  if (gDataBrowserTableViewGetContextualMenuUPP == NULL) gDataBrowserTableViewGetContextualMenuUPP = NewDataBrowserGetContextualMenuUPP(wxMacDataBrowserTableViewControl::DataBrowserGetContextualMenuProc);
  if (gDataBrowserTableViewItemCompareUPP       == NULL) gDataBrowserTableViewItemCompareUPP       = NewDataBrowserItemCompareUPP      (wxMacDataBrowserTableViewControl::DataBrowserCompareProc);
  if (gDataBrowserTableViewItemDataUPP          == NULL) gDataBrowserTableViewItemDataUPP          = NewDataBrowserItemDataUPP         (wxMacDataBrowserTableViewControl::DataBrowserGetSetItemDataProc);
  if (gDataBrowserTableViewItemNotificationUPP == NULL)
  {
    gDataBrowserTableViewItemNotificationUPP =
#if TARGET_API_MAC_OSX
      (DataBrowserItemNotificationUPP) NewDataBrowserItemNotificationWithItemUPP(wxMacDataBrowserTableViewControl::DataBrowserItemNotificationProc);
#else
      NewDataBrowserItemNotificationUPP(wxMacDataBrowserTableViewControl::DataBrowserItemNotificationProc);
#endif
  }
 // setup drag and drop callbacks:
  if (gDataBrowserTableViewAcceptDragUPP  == NULL) gDataBrowserTableViewAcceptDragUPP  = NewDataBrowserAcceptDragUPP (wxMacDataBrowserTableViewControl::DataBrowserAcceptDragProc);
  if (gDataBrowserTableViewAddDragItemUPP == NULL) gDataBrowserTableViewAddDragItemUPP = NewDataBrowserAddDragItemUPP(wxMacDataBrowserTableViewControl::DataBrowserAddDragItemProc);
  if (gDataBrowserTableViewReceiveDragUPP == NULL) gDataBrowserTableViewReceiveDragUPP = NewDataBrowserReceiveDragUPP(wxMacDataBrowserTableViewControl::DataBrowserReceiveDragProc);

  DataBrowserCallbacks callbacks; // variable definition

  InitializeDataBrowserCallbacks(&callbacks,kDataBrowserLatestCallbacks);
  callbacks.u.v1.getContextualMenuCallback = gDataBrowserTableViewGetContextualMenuUPP;
  callbacks.u.v1.itemDataCallback          = gDataBrowserTableViewItemDataUPP;
  callbacks.u.v1.itemCompareCallback       = gDataBrowserTableViewItemCompareUPP;
  callbacks.u.v1.itemNotificationCallback  = gDataBrowserTableViewItemNotificationUPP;
  callbacks.u.v1.acceptDragCallback        = gDataBrowserTableViewAcceptDragUPP;
  callbacks.u.v1.addDragItemCallback       = gDataBrowserTableViewAddDragItemUPP;
  callbacks.u.v1.receiveDragCallback       = gDataBrowserTableViewReceiveDragUPP;
  SetCallbacks(&callbacks);

 // setup callbacks for customized items:
  if (gDataBrowserTableViewDrawItemUPP == NULL) gDataBrowserTableViewDrawItemUPP = NewDataBrowserDrawItemUPP(wxMacDataBrowserTableViewControl::DataBrowserDrawItemProc);
  if (gDataBrowserTableViewEditItemUPP == NULL) gDataBrowserTableViewEditItemUPP = NewDataBrowserEditItemUPP(wxMacDataBrowserTableViewControl::DataBrowserEditItemProc);
  if (gDataBrowserTableViewHitTestUPP  == NULL) gDataBrowserTableViewHitTestUPP  = NewDataBrowserHitTestUPP (wxMacDataBrowserTableViewControl::DataBrowserHitTestProc);
  if (gDataBrowserTableViewTrackingUPP == NULL) gDataBrowserTableViewTrackingUPP = NewDataBrowserTrackingUPP(wxMacDataBrowserTableViewControl::DataBrowserTrackingProc);

  DataBrowserCustomCallbacks customCallbacks; // variable definition

  InitializeDataBrowserCustomCallbacks(&customCallbacks,kDataBrowserLatestCallbacks);
  customCallbacks.u.v1.drawItemCallback = gDataBrowserTableViewDrawItemUPP;
  customCallbacks.u.v1.editTextCallback = gDataBrowserTableViewEditItemUPP;
  customCallbacks.u.v1.hitTestCallback  = gDataBrowserTableViewHitTestUPP;
  customCallbacks.u.v1.trackingCallback = gDataBrowserTableViewTrackingUPP;
  SetCustomCallbacks(&customCallbacks);

 // style setting:
  EnableCellSizeModification( ((style & wxDV_VARIABLE_LINE_HEIGHT) != 0), true );

  DataBrowserSelectionFlags flags; // variable definition

  if (GetSelectionFlags(&flags) == noErr) // get default settings
  {
    if ((style & wxDV_MULTIPLE) != 0)
      flags &= ~kDataBrowserSelectOnlyOne;
    else
      flags |= kDataBrowserSelectOnlyOne;
    (void) SetSelectionFlags(flags);
  }

  OptionBits attributes; // variable definition

  if (GetAttributes(&attributes) == noErr) // get default settings
  {
    if ((style & wxDV_VERT_RULES) != 0)
      attributes |= kDataBrowserAttributeListViewDrawColumnDividers;
    else
      attributes &= ~kDataBrowserAttributeListViewDrawColumnDividers;
    if ((style & wxDV_ROW_LINES) != 0)
      attributes |= kDataBrowserAttributeListViewAlternatingRowColors;
    else
      attributes &= ~kDataBrowserAttributeListViewAlternatingRowColors;
    (void) SetAttributes(attributes);
  }

  if ((style & wxDV_NO_HEADER) != 0)
    SetHeaderButtonHeight(0);
}

wxMacDataBrowserTableViewControl::~wxMacDataBrowserTableViewControl()
{
  ::RemoveEventHandler((EventHandlerRef) m_macDataViewCtrlEventHandler);
}

//
// callback handling
//
OSStatus wxMacDataBrowserTableViewControl::SetCallbacks(DataBrowserCallbacks const* callbacks)
{
  return ::SetDataBrowserCallbacks(m_controlRef,callbacks);
}

OSStatus wxMacDataBrowserTableViewControl::SetCustomCallbacks(DataBrowserCustomCallbacks const* customCallbacks)
{
  return ::SetDataBrowserCustomCallbacks(m_controlRef,customCallbacks);
}

//
// DnD handling
//
OSStatus wxMacDataBrowserTableViewControl::EnableAutomaticDragTracking(bool enable)
{
  return ::SetAutomaticControlDragTrackingEnabledForWindow(::GetControlOwner(m_controlRef),enable);
}

//
// header handling
//
OSStatus wxMacDataBrowserTableViewControl::GetHeaderDesc(DataBrowserPropertyID propertyID, DataBrowserListViewHeaderDesc* desc) const
{
  desc->version = kDataBrowserListViewLatestHeaderDesc; // if this statement is missing the next call will fail (NOT DOCUMENTED!!)
  return ::GetDataBrowserListViewHeaderDesc(m_controlRef,propertyID,desc);
}

OSStatus wxMacDataBrowserTableViewControl::SetHeaderDesc(DataBrowserPropertyID propertyID, DataBrowserListViewHeaderDesc* desc)
{
  return ::SetDataBrowserListViewHeaderDesc(m_controlRef,propertyID,desc);
}

//
// layout handling
//
OSStatus wxMacDataBrowserTableViewControl::AutoSizeColumns()
{
  return AutoSizeDataBrowserListViewColumns(m_controlRef);
}

OSStatus wxMacDataBrowserTableViewControl::EnableCellSizeModification(bool enableHeight, bool enableWidth)
{
  return ::SetDataBrowserTableViewGeometry(GetControlRef(),enableWidth,enableHeight);
}

OSStatus wxMacDataBrowserTableViewControl::GetAttributes(OptionBits* attributes)
{
  return ::DataBrowserGetAttributes(GetControlRef(),attributes);
}

OSStatus wxMacDataBrowserTableViewControl::GetColumnWidth(DataBrowserPropertyID propertyID, UInt16* width) const
{
  return ::GetDataBrowserTableViewNamedColumnWidth(m_controlRef,propertyID,width);
}

OSStatus wxMacDataBrowserTableViewControl::GetDefaultColumnWidth( UInt16 *width ) const
{
    return GetDataBrowserTableViewColumnWidth(m_controlRef, width );
}

OSStatus wxMacDataBrowserTableViewControl::GetDefaultRowHeight(UInt16* height) const
{
  return ::GetDataBrowserTableViewRowHeight(m_controlRef,height);
}

OSStatus wxMacDataBrowserTableViewControl::GetHeaderButtonHeight(UInt16 *height)
{
  return ::GetDataBrowserListViewHeaderBtnHeight(m_controlRef,height);
}

OSStatus wxMacDataBrowserTableViewControl::GetPartBounds(DataBrowserItemID item, DataBrowserPropertyID propertyID, DataBrowserPropertyPart part, Rect* bounds)
{
  return ::GetDataBrowserItemPartBounds(m_controlRef,item,propertyID,part,bounds);
}

OSStatus wxMacDataBrowserTableViewControl::GetRowHeight(DataBrowserItemID item, UInt16* height) const
{
  return ::GetDataBrowserTableViewItemRowHeight(m_controlRef,item,height);
}

OSStatus wxMacDataBrowserTableViewControl::GetScrollPosition( UInt32 *top , UInt32 *left ) const
{
    return GetDataBrowserScrollPosition(m_controlRef, top , left );
}

OSStatus wxMacDataBrowserTableViewControl::SetAttributes(OptionBits attributes)
{
  return ::DataBrowserChangeAttributes(GetControlRef(),attributes,~attributes);
}

OSStatus wxMacDataBrowserTableViewControl::SetColumnWidth(DataBrowserPropertyID propertyID, UInt16 width)
{
  return ::SetDataBrowserTableViewNamedColumnWidth(m_controlRef,propertyID,width);
}

OSStatus wxMacDataBrowserTableViewControl::SetDefaultColumnWidth(UInt16 width)
{
  return ::SetDataBrowserTableViewColumnWidth(m_controlRef,width);
}

OSStatus wxMacDataBrowserTableViewControl::SetDefaultRowHeight(UInt16 height)
{
  return ::SetDataBrowserTableViewRowHeight(m_controlRef,height);
}

OSStatus wxMacDataBrowserTableViewControl::SetHasScrollBars(bool horiz, bool vert)
{
  return ::SetDataBrowserHasScrollBars(m_controlRef,horiz,vert);
}

OSStatus wxMacDataBrowserTableViewControl::SetHeaderButtonHeight(UInt16 height)
{
  return ::SetDataBrowserListViewHeaderBtnHeight(m_controlRef,height);
}

OSStatus wxMacDataBrowserTableViewControl::SetHiliteStyle(DataBrowserTableViewHiliteStyle hiliteStyle)
{
  return ::SetDataBrowserTableViewHiliteStyle(m_controlRef,hiliteStyle);
}

OSStatus wxMacDataBrowserTableViewControl::SetIndent(float Indent)
{
  return ::DataBrowserSetMetric(m_controlRef,kDataBrowserMetricDisclosureColumnPerDepthGap,true,Indent);
}

OSStatus wxMacDataBrowserTableViewControl::SetItemRowHeight(DataBrowserItemID item, UInt16 height)
{
  return ::SetDataBrowserTableViewItemRowHeight(m_controlRef,item,height);
}

OSStatus wxMacDataBrowserTableViewControl::SetScrollPosition(UInt32 top, UInt32 left)
{
  return ::SetDataBrowserScrollPosition(m_controlRef,top,left);
}

//
// column handling
//
OSStatus wxMacDataBrowserTableViewControl::GetColumnCount(UInt32* numColumns) const
{
  return ::GetDataBrowserTableViewColumnCount(m_controlRef,numColumns);
}

OSStatus wxMacDataBrowserTableViewControl::GetColumnIndex(DataBrowserPropertyID propertyID, DataBrowserTableViewColumnIndex* index) const
{
  return ::GetDataBrowserTableViewColumnPosition(m_controlRef,propertyID,index);
}

OSStatus wxMacDataBrowserTableViewControl::GetFreePropertyID(DataBrowserPropertyID* propertyID) const
{
  for (*propertyID=kMinPropertyID; *propertyID<std::numeric_limits<DataBrowserPropertyID>::max(); ++(*propertyID))
    if (IsUsedPropertyID(*propertyID) == errDataBrowserPropertyNotFound)
      return noErr;
  return errDataBrowserPropertyNotSupported;
}

OSStatus wxMacDataBrowserTableViewControl::GetPropertyFlags(DataBrowserPropertyID propertyID, DataBrowserPropertyFlags *flags) const
{
  return ::GetDataBrowserPropertyFlags(m_controlRef,propertyID,flags);
}

OSStatus wxMacDataBrowserTableViewControl::GetPropertyID(DataBrowserItemDataRef itemData, DataBrowserPropertyID* propertyID) const
{
  return ::GetDataBrowserItemDataProperty(itemData,propertyID);
}

OSStatus wxMacDataBrowserTableViewControl::GetPropertyID(DataBrowserTableViewColumnIndex index, DataBrowserTableViewColumnID* propertyID) const
{
  return ::GetDataBrowserTableViewColumnProperty(m_controlRef,index,propertyID);
}

OSStatus wxMacDataBrowserTableViewControl::IsUsedPropertyID(DataBrowserPropertyID propertyID) const
{
 // as the Mac interface does not provide a function that checks if the property id is in use or not a function is chosen that should not
 // lead to a large overhead for the call but returns an error code if the property id does not exist, here we use the function that returns
 // the column position for the property id:
  DataBrowserTableViewColumnIndex index;

  return ::GetDataBrowserTableViewColumnPosition(m_controlRef,propertyID,&index);
}

OSStatus wxMacDataBrowserTableViewControl::RemoveColumnByProperty(DataBrowserTableViewColumnID propertyID)
{
  return ::RemoveDataBrowserTableViewColumn(m_controlRef,propertyID);
}

OSStatus wxMacDataBrowserTableViewControl::RemoveColumnByIndex(DataBrowserTableViewColumnIndex index)
{
  DataBrowserTableViewColumnID propertyID;


  GetPropertyID(index,&propertyID);
  return ::RemoveDataBrowserTableViewColumn(m_controlRef,propertyID);
}

OSStatus wxMacDataBrowserTableViewControl::SetColumnIndex(DataBrowserPropertyID propertyID, DataBrowserTableViewColumnIndex index)
{
  return ::SetDataBrowserTableViewColumnPosition(m_controlRef,propertyID,index);
}

OSStatus wxMacDataBrowserTableViewControl::SetDisclosureColumn(DataBrowserPropertyID propertyID, Boolean expandableRows)
{
  return ::SetDataBrowserListViewDisclosureColumn(m_controlRef,propertyID,expandableRows);
}

OSStatus wxMacDataBrowserTableViewControl::SetPropertyFlags(DataBrowserPropertyID propertyID, DataBrowserPropertyFlags flags)
{
  return ::SetDataBrowserPropertyFlags(m_controlRef,propertyID,flags);
}

//
// item handling
//
OSStatus wxMacDataBrowserTableViewControl::AddItems(DataBrowserItemID container, UInt32 numItems, DataBrowserItemID const* items, DataBrowserPropertyID preSortProperty)
{
  return ::AddDataBrowserItems(m_controlRef,container,numItems,items,preSortProperty);
}

OSStatus wxMacDataBrowserTableViewControl::GetFreeItemID(DataBrowserItemID* id) const
{
  ItemCount noOfItems;

  OSStatus status;


  status = GetItemCount(&noOfItems);
  wxCHECK_MSG(status == noErr,status,_("Could not retrieve number of items"));
  if (noOfItems == 0)
  {
    *id = 1;
    return noErr;
  }
  else
  {
   // as there might be a lot of items in the data browser and mostly the data is added item by item the largest used ID number is roughly in the order of magnitude
   // as the number of items; therefore, start from the number of items to look for a new ID:
    for (*id=noOfItems; *id<std::numeric_limits<DataBrowserItemID>::max(); ++(*id))
      if (IsUsedItemID(*id) == errDataBrowserItemNotFound)
        return noErr;
   // as the first approach was not successful, try from the beginning:
    for (*id=0; *id<noOfItems; ++(*id))
      if (IsUsedItemID(*id) == errDataBrowserItemNotFound)
        return noErr;
   // sorry, data browser is full:
    return errDataBrowserItemNotAdded;
  }
}

OSStatus wxMacDataBrowserTableViewControl::GetItemCount(DataBrowserItemID container, Boolean recurse, DataBrowserItemState state, ItemCount* numItems) const
{
  return GetDataBrowserItemCount(m_controlRef,container,recurse,state,numItems);
}

OSStatus wxMacDataBrowserTableViewControl::GetItemID( DataBrowserTableViewRowIndex row, DataBrowserItemID * item ) const
{
  return GetDataBrowserTableViewItemID(m_controlRef,row,item);
}

OSStatus wxMacDataBrowserTableViewControl::GetItems(DataBrowserItemID container, Boolean recurse, DataBrowserItemState state, Handle items) const
{
  return GetDataBrowserItems(m_controlRef,container,recurse,state,items);
}

OSStatus wxMacDataBrowserTableViewControl::GetItemRow(DataBrowserItemID item, DataBrowserTableViewRowIndex* row) const
{
  return GetDataBrowserTableViewItemRow(m_controlRef,item,row);
}

OSStatus wxMacDataBrowserTableViewControl::GetItemState(DataBrowserItemID item, DataBrowserItemState* state) const
{
  return ::GetDataBrowserItemState(m_controlRef,item,state);
}

OSStatus wxMacDataBrowserTableViewControl::IsUsedItemID(DataBrowserItemID itemID) const
{
 // as the Mac interface does not provide a function that checks if the property id is in use or not a function is chosen that should not
 // lead to a large overhead for the call but returns an error code if the property id does not exist, here we use the function that returns
 // the column position for the property id:
  DataBrowserTableViewColumnIndex index;

  return ::GetDataBrowserTableViewItemRow(m_controlRef,itemID,&index);
}

OSStatus wxMacDataBrowserTableViewControl::RemoveItems(DataBrowserItemID container, UInt32 numItems, DataBrowserItemID const* items, DataBrowserPropertyID preSortProperty)
{
  return ::RemoveDataBrowserItems(m_controlRef,container,numItems,items,preSortProperty);
}

OSStatus wxMacDataBrowserTableViewControl::RevealItem(DataBrowserItemID item, DataBrowserPropertyID propertyID, DataBrowserRevealOptions options) const
{
  return ::RevealDataBrowserItem(m_controlRef,item,propertyID,options);
}

OSStatus wxMacDataBrowserTableViewControl::UpdateItems(DataBrowserItemID container, UInt32 numItems, DataBrowserItemID const* items, DataBrowserPropertyID preSortProperty,
                                                       DataBrowserPropertyID propertyID) const
{
  return UpdateDataBrowserItems(m_controlRef,container,numItems,items,preSortProperty,propertyID);
}

//
// item selection
//
size_t wxMacDataBrowserTableViewControl::GetSelectedItemIDs(wxArrayDataBrowserItemID& itemIDs) const
{
  DataBrowserItemID* itemIDPtr;
  Handle handle(::NewHandle(0));

  size_t noOfItems;


  wxCHECK_MSG(GetItems(kDataBrowserNoItem,true,kDataBrowserItemIsSelected,handle) == noErr,0,_("Could not get selected items."));
  noOfItems = static_cast<size_t>(::GetHandleSize(handle)/sizeof(DataBrowserItemID));
  itemIDs.Empty();
  itemIDs.Alloc(noOfItems);
  HLock(handle);
  itemIDPtr = (DataBrowserItemID*) (*handle);
  for (size_t i=0; i<noOfItems; ++i)
  {
    itemIDs.Add(*itemIDPtr);
    ++itemIDPtr;
  }
  HUnlock(handle);
  DisposeHandle(handle);
  return noOfItems;
}

OSStatus wxMacDataBrowserTableViewControl::GetSelectionAnchor(DataBrowserItemID* first, DataBrowserItemID* last) const
{
  return ::GetDataBrowserSelectionAnchor(m_controlRef,first,last);
}

OSStatus wxMacDataBrowserTableViewControl::GetSelectionFlags(DataBrowserSelectionFlags* flags) const
{
  return ::GetDataBrowserSelectionFlags(m_controlRef,flags);
}

bool wxMacDataBrowserTableViewControl::IsItemSelected(DataBrowserItemID item) const
{
  return ::IsDataBrowserItemSelected(m_controlRef,item);
}

OSStatus wxMacDataBrowserTableViewControl::SetSelectionFlags(DataBrowserSelectionFlags flags)
{
  return ::SetDataBrowserSelectionFlags(m_controlRef,flags);
}

OSStatus wxMacDataBrowserTableViewControl::SetSelectedItems(UInt32 numItems, DataBrowserItemID const* items, DataBrowserSetOption operation)
{
  return ::SetDataBrowserSelectedItems(m_controlRef, numItems, items, operation );
}

OSStatus wxMacDataBrowserTableViewControl::GetSortOrder(DataBrowserSortOrder* order) const
{
  return ::GetDataBrowserSortOrder(m_controlRef,order);
}

OSStatus wxMacDataBrowserTableViewControl::GetSortProperty(DataBrowserPropertyID* propertyID) const
{
  return ::GetDataBrowserSortProperty(m_controlRef,propertyID);
}

OSStatus wxMacDataBrowserTableViewControl::Resort(DataBrowserItemID container, Boolean sortChildren)
{
  return ::SortDataBrowserContainer(m_controlRef,container,sortChildren);
}

OSStatus wxMacDataBrowserTableViewControl::SetSortOrder(DataBrowserSortOrder order)
{
  return ::SetDataBrowserSortOrder(m_controlRef,order);
}

OSStatus wxMacDataBrowserTableViewControl::SetSortProperty(DataBrowserPropertyID propertyID)
{
  return ::SetDataBrowserSortProperty(m_controlRef,propertyID);
}

//
// container handling
//
OSStatus wxMacDataBrowserTableViewControl::CloseContainer(DataBrowserItemID containerID)
{
  return ::CloseDataBrowserContainer(m_controlRef,containerID);
}

OSStatus wxMacDataBrowserTableViewControl::OpenContainer(DataBrowserItemID containerID)
{
  return ::OpenDataBrowserContainer(m_controlRef,containerID);
}

IMPLEMENT_ABSTRACT_CLASS(wxMacDataBrowserTableViewControl,wxMacControl)

// ============================================================================
// wxMacDataBrowserListViewControl
// ============================================================================
#pragma mark -
//
// column handling
//
OSStatus wxMacDataBrowserListViewControl::AddColumn(DataBrowserListViewColumnDesc *columnDesc, DataBrowserTableViewColumnIndex position)
{
  return AddDataBrowserListViewColumn(m_controlRef,columnDesc,position);
}

// ============================================================================
// wxMacDataViewDataBrowserListViewControl
// ============================================================================
#pragma mark -
//
// constructors / destructor
//
wxMacDataViewDataBrowserListViewControl::wxMacDataViewDataBrowserListViewControl(wxWindow* peer, wxPoint const& pos, wxSize const& size, long style)
                                        :wxMacDataBrowserListViewControl(peer,pos,size,style)
{
  if ((style & wxBORDER_NONE) != 0)
    SetData(kControlNoPart,kControlDataBrowserIncludesFrameAndFocusTag,(Boolean) false);
  (void) EnableAutomaticDragTracking();
  (void) SetHiliteStyle(kDataBrowserTableViewFillHilite);
}

//
// column related methods (inherited from wxDataViewWidgetImpl)
//
bool wxMacDataViewDataBrowserListViewControl::ClearColumns()
{
  UInt32 noOfColumns;


  wxCHECK_MSG(GetColumnCount(&noOfColumns) == noErr,false,_("Could not determine number of columns."));
  for (UInt32 i=0; i<noOfColumns; ++i)
    wxCHECK_MSG(RemoveColumnByIndex(0) == noErr,false,_("Could not remove column."));
  return true;
}

bool wxMacDataViewDataBrowserListViewControl::DeleteColumn(wxDataViewColumn* columnPtr)
{
  return (RemoveColumnByProperty(columnPtr->GetNativeData()->GetPropertyID()) == noErr);
}

void wxMacDataViewDataBrowserListViewControl::DoSetExpanderColumn(wxDataViewColumn const* columnPtr)
{
  SetDisclosureColumn(columnPtr->GetNativeData()->GetPropertyID(),false); // second parameter explicitly passed to ensure that arrow is centered
}

wxDataViewColumn* wxMacDataViewDataBrowserListViewControl::GetColumn(unsigned int pos) const
{
  DataBrowserPropertyID propertyID;


  if (GetPropertyID(pos,&propertyID) == noErr)
    return GetColumnPtr(propertyID);
  else
    return NULL;
}

int wxMacDataViewDataBrowserListViewControl::GetColumnPosition(wxDataViewColumn const* columnPtr) const
{
  if (columnPtr != NULL)
  {
    DataBrowserTableViewColumnIndex Position;

    wxCHECK_MSG(GetColumnIndex(columnPtr->GetNativeData()->GetPropertyID(),&Position) == noErr,wxNOT_FOUND,_("Could not determine column's position"));
    return static_cast<int>(Position);
  }
  else
    return wxNOT_FOUND;
}

bool wxMacDataViewDataBrowserListViewControl::InsertColumn(unsigned int pos, wxDataViewColumn* columnPtr)
{
  DataBrowserListViewColumnDesc columnDescription;

  DataBrowserPropertyID newPropertyID;

  UInt32 noOfColumns;

  wxCFStringRef title(columnPtr->GetTitle(),m_font.Ok() ? dynamic_cast<wxDataViewCtrl*>(GetWXPeer())->GetFont().GetEncoding() : wxLocale::GetSystemEncoding());


 // try to get new ID for the column:
  wxCHECK_MSG(GetFreePropertyID(&newPropertyID) == noErr,false,_("Cannot create new column's ID. Probably max. number of columns reached."));
 // set native data:
  columnPtr->GetNativeData()->SetPropertyID(newPropertyID);
 // create a column description, add column to the native control and do some final layout adjustments:
  wxCHECK_MSG(::InitializeColumnDescription(columnDescription,columnPtr,title),  false,_("Column description could not be initialized."));
  wxCHECK_MSG(AddColumn(&columnDescription,pos) == noErr,                  false,_("Column could not be added."));
  wxCHECK_MSG(SetColumnWidth(newPropertyID,columnPtr->GetWidth()) == noErr,false,_("Column width could not be set."));
  wxCHECK_MSG(GetColumnCount(&noOfColumns) == noErr,                       false,_("Number of columns could not be determined."));
  if (noOfColumns == 1)
  {
    wxDataViewCtrl* dataViewCtrlPtr(dynamic_cast<wxDataViewCtrl*>(GetWXPeer()));

    wxCHECK_MSG(dataViewCtrlPtr != NULL,false,_("wxWidget's control not initialized."));
    dataViewCtrlPtr->AddChildren(wxDataViewItem());
    return true;
  }
  else
    return Update(columnPtr);
}

//
// item related methods (inherited from wxDataViewWidgetImpl)
//
bool wxMacDataViewDataBrowserListViewControl::Add(wxDataViewItem const& parent, wxDataViewItem const& item)
{
  DataBrowserItemID itemID(reinterpret_cast<DataBrowserItemID>(item.GetID()));


  return ((  parent.IsOk()  && AddItem(reinterpret_cast<DataBrowserItemID>(parent.GetID()),&itemID) == noErr) ||
          (!(parent.IsOk()) && AddItem(kDataBrowserNoItem,&itemID) == noErr));
}

bool wxMacDataViewDataBrowserListViewControl::Add(wxDataViewItem const& parent, wxDataViewItemArray const& items)
{
  bool noFailureFlag;

  DataBrowserItemID* itemIDs;

  size_t noOfEntries;


 // convert all valid data view items to data browser items:
  itemIDs = ::CreateDataBrowserItemIDArray(noOfEntries,items);
 // insert all valid items into control:
  noFailureFlag = ((noOfEntries == 0) ||
                   !(parent.IsOk()) && (AddItems(kDataBrowserNoItem,noOfEntries,itemIDs,kDataBrowserItemNoProperty) == noErr) ||
                   parent.IsOk() && (AddItems(reinterpret_cast<DataBrowserItemID>(parent.GetID()),noOfEntries,itemIDs,kDataBrowserItemNoProperty) == noErr));
 // give allocated array space free again:
  delete[] itemIDs;
 // done:
  return noFailureFlag;
}

void wxMacDataViewDataBrowserListViewControl::Collapse(wxDataViewItem const& item)
{
  CloseContainer(reinterpret_cast<DataBrowserItemID>(item.GetID()));
}

void wxMacDataViewDataBrowserListViewControl::EnsureVisible(wxDataViewItem const& item, const wxDataViewColumn* columnPtr)
{
  DataBrowserPropertyID propertyID;


  if (columnPtr != NULL)
    propertyID = columnPtr->GetNativeData()->GetPropertyID();
  else
    propertyID = kDataBrowserNoItem;
  RevealItem(reinterpret_cast<DataBrowserItemID>(item.GetID()),propertyID,kDataBrowserRevealOnly);
}

void wxMacDataViewDataBrowserListViewControl::Expand(wxDataViewItem const& item)
{
  OpenContainer(reinterpret_cast<DataBrowserItemID>(item.GetID()));
}

unsigned int wxMacDataViewDataBrowserListViewControl::GetCount() const
{
  ItemCount noOfItems;


  wxCHECK_MSG(GetItemCount(&noOfItems) == noErr,0,_("Could not determine number of items"));
  return noOfItems;
}

wxRect wxMacDataViewDataBrowserListViewControl::GetRectangle(wxDataViewItem const& item, wxDataViewColumn const* columnPtr)
{
  Rect MacRectangle;


  if (GetPartBounds(reinterpret_cast<DataBrowserItemID>(item.GetID()),columnPtr->GetNativeData()->GetPropertyID(),kDataBrowserPropertyContentPart,&MacRectangle) == noErr)
  {
    wxRect rectangle;

    ::wxMacNativeToRect(&MacRectangle,&rectangle);
    return rectangle;
  }
  else
    return wxRect();
}

bool wxMacDataViewDataBrowserListViewControl::IsExpanded(wxDataViewItem const& item) const
{
  DataBrowserItemState state = 0;

  if (GetItemState(reinterpret_cast<DataBrowserItemID>(item.GetID()),&state) != noErr)
     return false;

  return ((state & kDataBrowserContainerIsOpen) != 0);
}

bool wxMacDataViewDataBrowserListViewControl::Reload()
{
  bool noFailureFlag;

  wxDataViewItemArray dataViewChildren;


  noFailureFlag = (RemoveItems() == noErr);
  SetScrollPosition(0,0); // even after having removed all items the scrollbars may remain at their old position -> reset them
  GetDataViewCtrl()->GetModel()->GetChildren(wxDataViewItem(),dataViewChildren);
  GetDataViewCtrl()->GetModel()->ItemsAdded(wxDataViewItem(),dataViewChildren);

  return noFailureFlag;
}

bool wxMacDataViewDataBrowserListViewControl::Remove(wxDataViewItem const& parent, wxDataViewItem const& item)
{
  DataBrowserItemID itemID(reinterpret_cast<DataBrowserItemID>(item.GetID()));


  return (RemoveItem(reinterpret_cast<DataBrowserItemID>(parent.GetID()),&itemID) == noErr);
}

bool wxMacDataViewDataBrowserListViewControl::Remove(wxDataViewItem const& parent, wxDataViewItemArray const& items)
{
  bool noFailureFlag;

  DataBrowserItemID* itemIDs;

  size_t noOfEntries;


 // convert all valid data view items to data browser items:
  itemIDs = ::CreateDataBrowserItemIDArray(noOfEntries,items);
 // insert all valid items into control:
  noFailureFlag = ((noOfEntries == 0) ||
                   !(parent.IsOk()) && (RemoveItems(kDataBrowserNoItem,noOfEntries,itemIDs,kDataBrowserItemNoProperty) == noErr) ||
                   parent.IsOk() && (RemoveItems(reinterpret_cast<DataBrowserItemID>(parent.GetID()),noOfEntries,itemIDs,kDataBrowserItemNoProperty) == noErr));
 // give allocated array space free again:
  delete[] itemIDs;
 // done:
  return noFailureFlag;
}

bool wxMacDataViewDataBrowserListViewControl::Update(wxDataViewColumn const* columnPtr)
{
  return (UpdateItems(kDataBrowserNoItem,0,NULL,kDataBrowserItemNoProperty,columnPtr->GetNativeData()->GetPropertyID()) == noErr);
}

bool wxMacDataViewDataBrowserListViewControl::Update(wxDataViewItem const& parent, wxDataViewItem const& item)
{
  DataBrowserItemID itemID(reinterpret_cast<DataBrowserItemID>(item.GetID()));


  if (parent.IsOk())
    return (UpdateItem(reinterpret_cast<DataBrowserItemID>(parent.GetID()),&itemID) == noErr);
  else
    return (UpdateItem(kDataBrowserNoItem,&itemID) == noErr);
}

bool wxMacDataViewDataBrowserListViewControl::Update(wxDataViewItem const& parent, wxDataViewItemArray const& items)
{
  bool noFailureFlag;

  DataBrowserItemID* itemIDs;

  size_t noOfEntries;


 // convert all valid data view items to data browser items:
  itemIDs = ::CreateDataBrowserItemIDArray(noOfEntries,items);
  if (parent.IsOk())
    noFailureFlag = (UpdateItems(reinterpret_cast<DataBrowserItemID>(parent.GetID()),noOfEntries,itemIDs,kDataBrowserItemNoProperty,kDataBrowserItemNoProperty) == noErr);
  else
    noFailureFlag = (UpdateItems(kDataBrowserNoItem,noOfEntries,itemIDs,kDataBrowserItemNoProperty,kDataBrowserItemNoProperty) == noErr);
 // release allocated array space:
  delete[] itemIDs;
 // done:
  return noFailureFlag;
}

//
// model related methods
//
bool wxMacDataViewDataBrowserListViewControl::AssociateModel(wxDataViewModel* WXUNUSED(model))
{
  return true;
}

//
// selection related methods (inherited from wxDataViewWidgetImpl)
//
wxDataViewItem wxMacDataViewDataBrowserListViewControl::GetCurrentItem() const
{
    wxFAIL_MSG( "unimplemented for Carbon" );

    return wxDataViewItem();
}

void wxMacDataViewDataBrowserListViewControl::SetCurrentItem(const wxDataViewItem& WXUNUSED(item))
{
    wxFAIL_MSG( "unimplemented for Carbon" );
}

int wxMacDataViewDataBrowserListViewControl::GetSelections(wxDataViewItemArray& sel) const
{
  size_t noOfSelectedItems;

  wxArrayDataBrowserItemID itemIDs;


  noOfSelectedItems = GetSelectedItemIDs(itemIDs);
  sel.Empty();
  sel.SetCount(noOfSelectedItems);
  for (size_t i=0; i<noOfSelectedItems; ++i)
    sel[i] = wxDataViewItem(reinterpret_cast<void*>(itemIDs[i]));
  return static_cast<int>(noOfSelectedItems);
}

bool wxMacDataViewDataBrowserListViewControl::IsSelected(wxDataViewItem const& item) const
{
  return IsItemSelected(reinterpret_cast<DataBrowserItemID>(item.GetID()));
}

void wxMacDataViewDataBrowserListViewControl::Select(wxDataViewItem const& item)
{
  DataBrowserItemID itemID(reinterpret_cast<DataBrowserItemID>(item.GetID()));


  SetSelectedItems(1,&itemID,kDataBrowserItemsAdd);
}

void wxMacDataViewDataBrowserListViewControl::SelectAll()
{
  DataBrowserItemID* itemIDPtr;

  Handle handle(::NewHandle(0));

  size_t noOfItems;


  wxCHECK_RET(GetItems(kDataBrowserNoItem,true,kDataBrowserItemAnyState,handle) == noErr,_("Could not get items."));
  noOfItems = static_cast<size_t>(::GetHandleSize(handle)/sizeof(DataBrowserItemID));
  ::HLock(handle);
  itemIDPtr = (DataBrowserItemID*) (*handle);
  SetSelectedItems(noOfItems,itemIDPtr,kDataBrowserItemsAssign);
  ::HUnlock(handle);
  ::DisposeHandle(handle);
}

void wxMacDataViewDataBrowserListViewControl::Unselect(wxDataViewItem const& item)
{
  DataBrowserItemID itemID(reinterpret_cast<DataBrowserItemID>(item.GetID()));


  SetSelectedItems(1,&itemID,kDataBrowserItemsRemove);
}

void wxMacDataViewDataBrowserListViewControl::UnselectAll()
{
  DataBrowserItemID* itemIDPtr;

  Handle handle(::NewHandle(0));

  size_t noOfItems;


  wxCHECK_RET(GetItems(kDataBrowserNoItem,true,kDataBrowserItemAnyState,handle) == noErr,_("Could not get items."));
  noOfItems = static_cast<size_t>(::GetHandleSize(handle)/sizeof(DataBrowserItemID));
  ::HLock(handle);
  itemIDPtr = (DataBrowserItemID*) (*handle);
  SetSelectedItems(noOfItems,itemIDPtr,kDataBrowserItemsRemove);
  ::HUnlock(handle);
  ::DisposeHandle(handle);
}

//
// sorting related methods
//
wxDataViewColumn* wxMacDataViewDataBrowserListViewControl::GetSortingColumn() const
{
  DataBrowserPropertyID propertyID;


  if (GetSortProperty(&propertyID) == noErr)
    return GetColumnPtr(propertyID);
  else
    return NULL;
}

void wxMacDataViewDataBrowserListViewControl::Resort()
{
    (void) wxMacDataBrowserListViewControl::Resort();
}

//
// other methods (inherited from wxDataViewWidgetImpl)
//
void
wxMacDataViewDataBrowserListViewControl::HitTest(const wxPoint& WXUNUSED(point),
                                                 wxDataViewItem& item,
                                                 wxDataViewColumn*& columnPtr) const
{
 // not yet implemented:
  item = wxDataViewItem();
  columnPtr = NULL;
}

void wxMacDataViewDataBrowserListViewControl::DoSetIndent(int indent)
{
  SetIndent(static_cast<float>(indent));
}

void wxMacDataViewDataBrowserListViewControl::SetRowHeight(wxDataViewItem const& item, unsigned int height)
{
  SetItemRowHeight(reinterpret_cast<DataBrowserItemID>(item.GetID()),static_cast<UInt16>(height));
}

void wxMacDataViewDataBrowserListViewControl::OnSize()
{
  UInt32 noOfColumns;


  GetColumnCount(&noOfColumns);
  if (noOfColumns <= 1) // no horizontal scroll bar and the only column expands to the width of the whole control
  {
    SetHasScrollBars(false,true);
    AutoSizeColumns();
  }
  else // otherwise keep the current column size and have scrollbars in both directions
    SetHasScrollBars(true,true);
}

//
// callback functions (inherited from wxMacDataBrowserTableViewControl)
//
Boolean wxMacDataViewDataBrowserListViewControl::DataBrowserCompareProc(DataBrowserItemID itemOneID, DataBrowserItemID itemTwoID, DataBrowserPropertyID sortProperty)
{
  DataBrowserSortOrder sortOrder;

  DataBrowserTableViewColumnIndex modelColumnIndex;

  wxDataViewCtrl* dataViewCtrlPtr(dynamic_cast<wxDataViewCtrl*>(GetWXPeer()));


  wxCHECK_MSG(dataViewCtrlPtr != NULL,            false,_("Pointer to data view control not set correctly."));
  wxCHECK_MSG(dataViewCtrlPtr->GetModel() != NULL,false,_("Pointer to model not set correctly."));
  if (sortProperty >= kMinPropertyID)
  {
   // variable definition and initialization:
    wxDataViewColumn* ColumnPtr(GetColumnPtr(sortProperty));

    wxCHECK_MSG(ColumnPtr != NULL,false,_("Could not determine column index."));
    modelColumnIndex = ColumnPtr->GetModelColumn();
  }
  else
    modelColumnIndex = 0;
  GetSortOrder(&sortOrder);
  return static_cast<Boolean>(dataViewCtrlPtr->GetModel()->Compare(wxDataViewItem(reinterpret_cast<void*>(itemOneID)),wxDataViewItem(reinterpret_cast<void*>(itemTwoID)),
                              modelColumnIndex,sortOrder != kDataBrowserOrderDecreasing) < 0);
}

void wxMacDataViewDataBrowserListViewControl::DataBrowserGetContextualMenuProc(MenuRef* menu, UInt32* helpType, CFStringRef* helpItemString, AEDesc* WXUNUSED(selection))
 // In this method we do not supply a contextual menu handler at all but only send a wxEVT_COMMAND_DATAVIEW_ITEM_CONTEXT_MENU.
{
  wxArrayDataBrowserItemID itemIDs;

  wxDataViewCtrl* dataViewCtrlPtr(dynamic_cast<wxDataViewCtrl*>(GetWXPeer()));


  wxCHECK_RET(dataViewCtrlPtr != NULL,_("wxWidget control pointer is not a data view pointer"));
 // initialize parameters so that no context menu will be displayed automatically by the native data browser:
  *menu           = NULL;
  *helpType       = kCMHelpItemNoHelp;
  *helpItemString = NULL;
 // create information for a context menu event:
  wxDataViewEvent dataViewEvent(wxEVT_COMMAND_DATAVIEW_ITEM_CONTEXT_MENU,dataViewCtrlPtr->GetId());

  dataViewEvent.SetEventObject(dataViewCtrlPtr);
  dataViewEvent.SetModel(dataViewCtrlPtr->GetModel());
 // get the item information;
 // theoretically more than one ID can be returned but the event can only handle one item, therefore all item related data is using the data of the first item in the array:
  if (GetSelectedItemIDs(itemIDs) > 0)
    dataViewEvent.SetItem(wxDataViewItem(reinterpret_cast<void*>(itemIDs[0])));
 // finally send the equivalent wxWidget event:
  dataViewCtrlPtr->GetEventHandler()->ProcessEvent(dataViewEvent);
}

OSStatus wxMacDataViewDataBrowserListViewControl::DataBrowserGetSetItemDataProc(DataBrowserItemID itemID, DataBrowserPropertyID propertyID, DataBrowserItemDataRef itemData, Boolean getValue)
{
  if (getValue)
  {
   // variable definitions:
    wxDataViewCtrl* dataViewCtrlPtr;

    dataViewCtrlPtr = dynamic_cast<wxDataViewCtrl*>(GetWXPeer());
    wxCHECK_MSG(dataViewCtrlPtr != NULL,errDataBrowserNotConfigured,_("Pointer to data view control not set correctly."));
    if (dataViewCtrlPtr->IsDeleting())
      return noErr; // if a delete process is running the data of editable fields cannot be saved because the associated model variable may already have been deleted
    else
    {
     // variable definitions:
      OSStatus          errorStatus;
      wxDataViewColumn* dataViewColumnPtr;

      wxCHECK_MSG(dataViewCtrlPtr->GetModel() != NULL,errDataBrowserNotConfigured,_("Pointer to model not set correctly."));
      dataViewColumnPtr = GetColumnPtr(propertyID);
      wxCHECK_MSG((dataViewColumnPtr != NULL) && (dataViewColumnPtr->GetRenderer() != NULL),errDataBrowserNotConfigured,_("There is no column or renderer for the specified column index."));

      wxDataViewItem dvItem(reinterpret_cast<void*>(itemID));
      unsigned int   col = dataViewColumnPtr->GetModelColumn();

      switch (dataViewColumnPtr->GetRenderer()->GetNativeData()->GetPropertyType())
      {
        case kDataBrowserCheckboxType:
          {
           // variable definition:
            ThemeButtonValue buttonValue;

            errorStatus = ::GetDataBrowserItemDataButtonValue(itemData,&buttonValue);
            if (errorStatus == noErr)
            {
              if (buttonValue == kThemeButtonOn)
              {
               // variable definition and initialization:
                wxVariant modifiedData(true);

                if (dataViewCtrlPtr->GetModel()->ChangeValue(modifiedData, dvItem, col))
                    return noErr;
                else
                    return errDataBrowserInvalidPropertyData;
              }
              else if (buttonValue == kThemeButtonOff)
              {
               // variable definition and initialization:
                wxVariant modifiedData(false);

                if (dataViewCtrlPtr->GetModel()->ChangeValue(modifiedData, dvItem, col))
                    return noErr;
                else
                    return errDataBrowserInvalidPropertyData;
              }
              else
                return errDataBrowserInvalidPropertyData;
            }
            else
              return errorStatus;
          } /* block */
        case kDataBrowserTextType:
          {
           // variable definitions:
            CFStringRef stringReference;

            errorStatus = ::GetDataBrowserItemDataText(itemData,&stringReference);
            if (errorStatus == noErr)
            {
             // variable definitions and initializations:
#if wxCHECK_VERSION(2,9,0)
              wxCFStringRef modifiedString(stringReference);
#else
              wxMacCFStringHolder modifiedString(stringReference);
#endif
              wxVariant           modifiedData(modifiedString.AsString());

                if (dataViewCtrlPtr->GetModel()->ChangeValue(modifiedData, dvItem, col))
                    return noErr;
                else
                    return errDataBrowserInvalidPropertyData;
            }
            else
              return errorStatus;
          } /* block */
        default:
          return errDataBrowserPropertyNotSupported;
      }
    }
  }
  else
  {
    if (propertyID >= kMinPropertyID) // in case data columns set the data
    {
     // variable definitions:
      wxVariant         variant;
      wxDataViewColumn* dataViewColumnPtr;
      wxDataViewCtrl*   dataViewCtrlPtr;

      dataViewCtrlPtr = dynamic_cast<wxDataViewCtrl*>(GetWXPeer());
      wxCHECK_MSG(dataViewCtrlPtr             != NULL,errDataBrowserNotConfigured,_("Pointer to data view control not set correctly."));
      wxCHECK_MSG(dataViewCtrlPtr->GetModel() != NULL,errDataBrowserNotConfigured,_("Pointer to model not set correctly."));
      dataViewColumnPtr = GetColumnPtr(propertyID);
      wxCHECK_MSG(dataViewColumnPtr                != NULL,errDataBrowserNotConfigured,_("No column for the specified column position existing."));
      wxCHECK_MSG(dataViewColumnPtr->GetRenderer() != NULL,errDataBrowserNotConfigured,_("No renderer specified for column."));
      dataViewCtrlPtr->GetModel()->GetValue(variant,wxDataViewItem(reinterpret_cast<void*>(itemID)),dataViewColumnPtr->GetModelColumn());
      if (!(variant.IsNull()))
      {
        dataViewColumnPtr->GetRenderer()->GetNativeData()->SetItemDataRef(itemData);
        dataViewColumnPtr->GetRenderer()->SetValue(variant);
        wxCHECK_MSG(dataViewColumnPtr->GetRenderer()->MacRender(),errDataBrowserNotConfigured,_("Rendering failed."));
      }
      return noErr;
    }
    else // react on special system requests
    {
      switch (propertyID)
      {
        case kDataBrowserContainerIsClosableProperty:
          {
           // variable definitions:
            wxDataViewCtrl* dataViewCtrlPtr(dynamic_cast<wxDataViewCtrl*>(GetWXPeer()));

            wxCHECK_MSG(dataViewCtrlPtr != NULL,errDataBrowserNotConfigured,_("Pointer to data view control not set correctly."));
           // initialize wxWidget event:
            wxDataViewEvent dataViewEvent(wxEVT_COMMAND_DATAVIEW_ITEM_COLLAPSING,dataViewCtrlPtr->GetId()); // variable definition

            dataViewEvent.SetEventObject(dataViewCtrlPtr);
            dataViewEvent.SetItem       (wxDataViewItem(reinterpret_cast<void*>(itemID)));
            dataViewEvent.SetModel      (dataViewCtrlPtr->GetModel());
           // finally send the equivalent wxWidget event:
            dataViewCtrlPtr->GetEventHandler()->ProcessEvent(dataViewEvent);
           // opening the container is allowed if not vetoed:
            return ::SetDataBrowserItemDataBooleanValue(itemData,dataViewEvent.IsAllowed());
          } /* block */
        case kDataBrowserContainerIsOpenableProperty:
          {
           // variable definitions:
            wxDataViewCtrl* dataViewCtrlPtr(dynamic_cast<wxDataViewCtrl*>(GetWXPeer()));

            wxCHECK_MSG(dataViewCtrlPtr != NULL,errDataBrowserNotConfigured,_("Pointer to data view control not set correctly."));
           // initialize wxWidget event:
            wxDataViewEvent dataViewEvent(wxEVT_COMMAND_DATAVIEW_ITEM_EXPANDING,dataViewCtrlPtr->GetId()); // variable definition

            dataViewEvent.SetEventObject(dataViewCtrlPtr);
            dataViewEvent.SetItem       (wxDataViewItem(reinterpret_cast<void*>(itemID)));
            dataViewEvent.SetModel      (dataViewCtrlPtr->GetModel());
           // finally send the equivalent wxWidget event:
            dataViewCtrlPtr->GetEventHandler()->ProcessEvent(dataViewEvent);
           // opening the container is allowed if not vetoed:
            return ::SetDataBrowserItemDataBooleanValue(itemData,dataViewEvent.IsAllowed());
          } /* block */
        case kDataBrowserItemIsContainerProperty:
          {
           // variable definition:
            wxDataViewCtrl* dataViewCtrlPtr(dynamic_cast<wxDataViewCtrl*>(GetWXPeer()));

            wxCHECK_MSG(dataViewCtrlPtr             != NULL,errDataBrowserNotConfigured,_("Pointer to data view control not set correctly."));
            wxCHECK_MSG(dataViewCtrlPtr->GetModel() != NULL,errDataBrowserNotConfigured,_("Pointer to model not set correctly."));
            return ::SetDataBrowserItemDataBooleanValue(itemData,dataViewCtrlPtr->GetModel()->IsContainer(wxDataViewItem(reinterpret_cast<void*>(itemID))));
          } /* block */
        case kDataBrowserItemIsEditableProperty:
          return ::SetDataBrowserItemDataBooleanValue(itemData,true);
      }
    }
  }
  return errDataBrowserPropertyNotSupported;
}

void wxMacDataViewDataBrowserListViewControl::DataBrowserItemNotificationProc(DataBrowserItemID itemID, DataBrowserItemNotification message, DataBrowserItemDataRef itemData)
{
  wxDataViewCtrl* dataViewCtrlPtr(dynamic_cast<wxDataViewCtrl*>(GetWXPeer()));


 // check if the data view control pointer still exists because this call back function can still be called when the control has already been deleted:
  if (dataViewCtrlPtr != NULL)
    switch (message)
    {
      case kDataBrowserContainerClosed:
        dataViewCtrlPtr->FinishCustomItemEditing(); // stop editing of a custom item first (if necessary)
        {
         // initialize wxWidget event:
          wxDataViewEvent dataViewEvent(wxEVT_COMMAND_DATAVIEW_ITEM_COLLAPSED,dataViewCtrlPtr->GetId()); // variable definition

          dataViewEvent.SetEventObject(dataViewCtrlPtr);
          dataViewEvent.SetItem(wxDataViewItem(reinterpret_cast<void*>(itemID)));
         // finally send the equivalent wxWidget event:
          dataViewCtrlPtr->GetEventHandler()->ProcessEvent(dataViewEvent);
        } /* block */
        break;
      case kDataBrowserContainerOpened:
        dataViewCtrlPtr->FinishCustomItemEditing(); // stop editing of a custom item first (if necessary)
        {
         // initialize wxWidget event:
          wxDataViewEvent dataViewEvent(wxEVT_COMMAND_DATAVIEW_ITEM_EXPANDED,dataViewCtrlPtr->GetId()); // variable definition

          dataViewEvent.SetEventObject(dataViewCtrlPtr);
          dataViewEvent.SetItem(wxDataViewItem(reinterpret_cast<void*>(itemID)));
         // finally send the equivalent wxWidget event:
          dataViewCtrlPtr->GetEventHandler()->ProcessEvent(dataViewEvent);
         // add children to the expanded container:
          dataViewCtrlPtr->AddChildren(wxDataViewItem(reinterpret_cast<void*>(itemID)));
        } /* block */
        break;
      case kDataBrowserEditStarted:
        dataViewCtrlPtr->FinishCustomItemEditing(); // stop editing of a custom item first (if necessary)
        {
         // initialize wxWidget event:
          DataBrowserPropertyID propertyID;
          wxDataViewEvent dataViewEvent(wxEVT_COMMAND_DATAVIEW_ITEM_EDITING_STARTED,dataViewCtrlPtr->GetId()); // variable definition

          dataViewEvent.SetEventObject(dataViewCtrlPtr);
          dataViewEvent.SetItem(wxDataViewItem(reinterpret_cast<void*>(itemID)));
          if (GetPropertyID(itemData,&propertyID) == noErr)
          {
           // variable definition and initialization:
            DataBrowserTableViewColumnIndex columnIndex;

            wxCHECK_RET(GetColumnIndex(propertyID,&columnIndex),_("Column index not found."));
            dataViewEvent.SetColumn(columnIndex);
            dataViewEvent.SetDataViewColumn(GetColumnPtr(propertyID));
          }
         // finally send the equivalent wxWidget event:
          dataViewCtrlPtr->GetEventHandler()->ProcessEvent(dataViewEvent);
        } /* block */
        break;
      case kDataBrowserEditStopped:
        {
         // initialize wxWidget event:
          DataBrowserPropertyID propertyID;
          wxDataViewEvent dataViewEvent(wxEVT_COMMAND_DATAVIEW_ITEM_EDITING_DONE,dataViewCtrlPtr->GetId()); // variable definition

          dataViewEvent.SetEventObject(dataViewCtrlPtr);
          dataViewEvent.SetItem(wxDataViewItem(reinterpret_cast<void*>(itemID)));
          if (GetPropertyID(itemData,&propertyID) == noErr)
          {
           // variable definition and initialization:
            DataBrowserTableViewColumnIndex columnIndex;

            wxCHECK_RET(GetColumnIndex(propertyID,&columnIndex),_("Column index not found."));
            dataViewEvent.SetColumn(columnIndex);
            dataViewEvent.SetDataViewColumn(GetColumnPtr(propertyID));
          }
         // finally send the equivalent wxWidget event:
          dataViewCtrlPtr->GetEventHandler()->ProcessEvent(dataViewEvent);
        } /* block */
        break;
      case kDataBrowserItemAdded:
        dataViewCtrlPtr->FinishCustomItemEditing();
        break;
      case kDataBrowserItemDeselected:
        dataViewCtrlPtr->FinishCustomItemEditing();
        break;
      case kDataBrowserItemDoubleClicked:
        {
         // initialize wxWidget event:
          wxDataViewEvent dataViewEvent(wxEVT_COMMAND_DATAVIEW_ITEM_ACTIVATED,dataViewCtrlPtr->GetId()); // variable definition

          dataViewEvent.SetEventObject(dataViewCtrlPtr);
          dataViewEvent.SetItem(wxDataViewItem(reinterpret_cast<void*>(itemID)));
         // finally send the equivalent wxWidget event:
          dataViewCtrlPtr->GetEventHandler()->ProcessEvent(dataViewEvent);
        } /* block */
        break;
      case kDataBrowserItemRemoved:
        dataViewCtrlPtr->FinishCustomItemEditing(); // stop editing of a custom item first (if necessary)
        break;
      case kDataBrowserItemSelected:
        break; // not implemented by wxWidgets; see kDataBrowserSelectionSetChanged
      case kDataBrowserSelectionSetChanged:
        {
         // initialize wxWidget event:
          wxDataViewEvent dataViewEvent(wxEVT_COMMAND_DATAVIEW_SELECTION_CHANGED,dataViewCtrlPtr->GetId()); // variable definition

          dataViewEvent.SetEventObject(dataViewCtrlPtr);
          dataViewEvent.SetModel      (dataViewCtrlPtr->GetModel());
         // finally send the equivalent wxWidget event:
          dataViewCtrlPtr->GetEventHandler()->ProcessEvent(dataViewEvent);
        } /* block */
        break;
      case kDataBrowserTargetChanged: // no idea if this notification is ever sent
        break;
      case kDataBrowserUserStateChanged:
        {
         // finish custom item editing if necessary:
          dataViewCtrlPtr->FinishCustomItemEditing();
         // update column widths:
          for (size_t i=0; i<dataViewCtrlPtr->GetColumnCount(); ++i)
          {
           // constant definition for abbreviational purposes:
            wxDataViewColumn* const columnPtr = dataViewCtrlPtr->GetColumnPtr(i);
           // variable definition:
            UInt16 columnWidth;

            wxCHECK_RET(GetColumnWidth(columnPtr->GetNativeData()->GetPropertyID(),&columnWidth) == noErr,_("Column width could not be determined"));
            columnPtr->SetWidthVariable(columnWidth);
          }
         // update sorting orders:
          DataBrowserPropertyID propertyID; // variable definition

          if ((GetSortProperty(&propertyID) == noErr) && (propertyID >= kMinPropertyID))
          {
            DataBrowserSortOrder            sortOrder;
            DataBrowserTableViewColumnIndex columnIndex;

            if ((GetSortOrder(&sortOrder) == noErr) && (GetColumnIndex(propertyID,&columnIndex) == noErr))
            {
             // variable definition and initialization:
              wxDataViewColumn* columnPtr;
              columnPtr = dataViewCtrlPtr->GetColumn(columnIndex);
             // check if the sort order has changed:
              if (  columnPtr->IsSortOrderAscending()  && (sortOrder == kDataBrowserOrderDecreasing) ||
                  !(columnPtr->IsSortOrderAscending()) && (sortOrder == kDataBrowserOrderIncreasing))
              {
                columnPtr->SetSortOrder(!(columnPtr->IsSortOrderAscending()));
               // initialize wxWidget event:
                wxDataViewEvent dataViewEvent(wxEVT_COMMAND_DATAVIEW_COLUMN_SORTED,dataViewCtrlPtr->GetId()); // variable defintion

                dataViewEvent.SetEventObject(dataViewCtrlPtr);
                dataViewEvent.SetColumn(columnIndex);
                dataViewEvent.SetDataViewColumn(columnPtr);
               // finally send the equivalent wxWidget event:
                dataViewCtrlPtr->GetEventHandler()->ProcessEvent(dataViewEvent);
              }
            }
          }
        } /* block */
        break;
    }
}

void
wxMacDataViewDataBrowserListViewControl::DataBrowserDrawItemProc(DataBrowserItemID itemID,
        DataBrowserPropertyID propertyID,
        DataBrowserItemState state,
        Rect const* rectangle,
        SInt16 WXUNUSED(bitDepth),
        Boolean WXUNUSED(colorDevice))
{
  DataBrowserTableViewColumnIndex columnIndex;

  wxDataViewColumn* dataViewColumnPtr;

  wxDataViewCtrl* dataViewCtrlPtr;

  wxDataViewCustomRenderer* dataViewCustomRendererPtr;

  wxVariant dataToRender;

  dataViewCtrlPtr = dynamic_cast<wxDataViewCtrl*>(GetWXPeer());
  wxCHECK_RET(dataViewCtrlPtr != NULL,                               _("Pointer to data view control not set correctly."));
  wxCHECK_RET(dataViewCtrlPtr->GetModel() != NULL,                   _("Pointer to model not set correctly."));
  wxCHECK_RET(GetColumnIndex(propertyID,&columnIndex) == noErr,_("Could not determine column index."));
  dataViewColumnPtr = GetColumnPtr(propertyID);
  wxCHECK_RET(dataViewColumnPtr != NULL,_("No column for the specified column existing."));
  dataViewCustomRendererPtr = dynamic_cast<wxDataViewCustomRenderer*>(dataViewColumnPtr->GetRenderer());
  wxCHECK_RET(dataViewCustomRendererPtr != NULL,_("No renderer or invalid renderer type specified for custom data column."));
  dataViewCtrlPtr->GetModel()->GetValue(dataToRender,wxDataViewItem(reinterpret_cast<void*>(itemID)),dataViewColumnPtr->GetModelColumn());
  dataViewCustomRendererPtr->SetValue(dataToRender);

  wxDataViewItem dataItem( reinterpret_cast<void*>(itemID) );
  dataViewCtrlPtr->GetModel()->GetValue(dataToRender,dataItem,dataViewColumnPtr->GetModelColumn());
  dataViewCustomRendererPtr->SetValue(dataToRender);

 // try to determine the content's size (drawable part):
  Rect      content;
  RgnHandle rgn(NewRgn());
  UInt16    headerHeight;

  if (GetControlRegion(m_controlRef,kControlContentMetaPart,rgn) == noErr)
    GetRegionBounds(rgn,&content);
  else
    GetControlBounds(m_controlRef, &content);
  ::DisposeRgn(rgn);
 // space for the header
  GetHeaderButtonHeight(&headerHeight);
  content.top += headerHeight;
 // extra space for the frame (todo: do not how to determine the space automatically from the control)
  content.top    += 5;
  content.left   += 5;
  content.right  -= 3;
  content.bottom -= 3;
 // extra space for the scrollbars:
  content.bottom -= wxSystemSettings::GetMetric(wxSYS_HSCROLL_Y);
  content.right  -= wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);

  wxDC *dc = dataViewCustomRendererPtr->GetDC();

  int active_border_fudge = 0;
  if (dataViewCtrlPtr->HasFocus() && !dataViewCtrlPtr->HasFlag( wxBORDER_NONE ))
     active_border_fudge = 1;
  else
     active_border_fudge = -2;

  wxRect cellrect( static_cast<int>(rectangle->left + active_border_fudge),
                   static_cast<int>(rectangle->top + active_border_fudge),
                   static_cast<int>(1+rectangle->right-rectangle->left),
                   static_cast<int>(rectangle->bottom-rectangle->top) );

  bool is_active = IsControlActive( m_controlRef );
  if (state == kDataBrowserItemIsSelected)
  {
      wxColour col( wxMacCreateCGColorFromHITheme( (is_active) ?
                             kThemeBrushAlternatePrimaryHighlightColor
                             : kThemeBrushSecondaryHighlightColor ) );

      wxRect rect = cellrect;
      Rect itemrect;
      GetDataBrowserItemPartBounds( m_controlRef, itemID, propertyID,
         kDataBrowserPropertyEnclosingPart, &itemrect );
      rect.x = itemrect.left-2;
      rect.width = itemrect.right-itemrect.left+3;

      wxDCPenChanger setPen(*dc, *wxTRANSPARENT_PEN);
      wxDCBrushChanger setBrush(*dc, col);
      dc->DrawRectangle(rect);
  }

  wxDataViewModel *model = dataViewCtrlPtr->GetModel();
  if ((columnIndex == 0) || !model->IsContainer(dataItem) || model->HasContainerColumns(dataItem))
  {
      // make sure that 'Render' can draw only in the allowed area:
      dc->SetClippingRegion(content.left,content.top,content.right-content.left+1,content.bottom-content.top+1);
      (void) (dataViewCustomRendererPtr->WXCallRender( cellrect, dc,
                                            ((state == kDataBrowserItemIsSelected) ? wxDATAVIEW_CELL_SELECTED : 0)));
      dc->DestroyClippingRegion(); // probably not necessary
  }

  dataViewCustomRendererPtr->SetDC(NULL);
}

Boolean
wxMacDataViewDataBrowserListViewControl::DataBrowserEditItemProc(
    DataBrowserItemID WXUNUSED(itemID),
    DataBrowserPropertyID WXUNUSED(propertyID),
    CFStringRef WXUNUSED(theString),
    Rect* WXUNUSED(maxEditTextRect),
    Boolean* WXUNUSED(shrinkToFit))
{
  return false;
}

Boolean wxMacDataViewDataBrowserListViewControl::DataBrowserHitTestProc(DataBrowserItemID WXUNUSED(itemID), DataBrowserPropertyID WXUNUSED(property), Rect const* WXUNUSED(theRect), Rect const* WXUNUSED(mouseRect))
{
  return true;
}

DataBrowserTrackingResult wxMacDataViewDataBrowserListViewControl::DataBrowserTrackingProc(DataBrowserItemID itemID, DataBrowserPropertyID propertyID, Rect const* theRect, Point WXUNUSED(startPt), EventModifiers WXUNUSED(modifiers))
{
  wxDataViewColumn* dataViewColumnPtr;

  wxDataViewCtrl* dataViewCtrlPtr;

  wxDataViewCustomRenderer* dataViewCustomRendererPtr;

  wxDataViewItem dataViewCustomRendererItem;


  dataViewCtrlPtr = dynamic_cast<wxDataViewCtrl*>(GetWXPeer());
  wxCHECK_MSG(dataViewCtrlPtr != NULL,            false,_("Pointer to data view control not set correctly."));
  wxCHECK_MSG(dataViewCtrlPtr->GetModel() != NULL,false,_("Pointer to model not set correctly."));
  dataViewCustomRendererItem = reinterpret_cast<void*>(itemID);
  wxCHECK_MSG(dataViewCustomRendererItem.IsOk(),kDataBrowserNothingHit,_("Invalid data view item"));
  dataViewColumnPtr = GetColumnPtr(propertyID);
  wxCHECK_MSG(dataViewColumnPtr != NULL,kDataBrowserNothingHit,_("No column existing."));
  dataViewCustomRendererPtr = dynamic_cast<wxDataViewCustomRenderer*>(dataViewColumnPtr->GetRenderer());
  wxCHECK_MSG(dataViewCustomRendererPtr != NULL,kDataBrowserNothingHit,_("No renderer or invalid renderer type specified for custom data column."));
 // if the currently edited item is identical to the to be edited nothing is done (this hit should only be handled in the control itself):
  if (dataViewCtrlPtr->GetCustomRendererItem() == dataViewCustomRendererItem)
    return kDataBrowserContentHit;
 // an(other) item is going to be edited and therefore the current editing - if existing - has to be finished:
  if (dataViewCtrlPtr->GetCustomRendererPtr() != NULL)
  {
    dataViewCtrlPtr->GetCustomRendererPtr()->FinishEditing();
    dataViewCtrlPtr->SetCustomRendererItem(wxDataViewItem());
    dataViewCtrlPtr->SetCustomRendererPtr (NULL);
  }
 // check if renderer has got a valid editor control for editing; if this is the case start editing of the new item:
  if (dataViewCustomRendererPtr->HasEditorCtrl())
  {
   // variable definition:
    wxRect wxRectangle;

    ::wxMacNativeToRect(theRect,&wxRectangle);
    dataViewCustomRendererPtr->StartEditing(dataViewCustomRendererItem,wxRectangle);
    dataViewCtrlPtr->SetCustomRendererItem(dataViewCustomRendererItem);
    dataViewCtrlPtr->SetCustomRendererPtr (dataViewCustomRendererPtr);
  }
  return kDataBrowserContentHit;
}

Boolean wxMacDataViewDataBrowserListViewControl::DataBrowserAcceptDragProc(DragReference dragRef, DataBrowserItemID itemID)
{
  bool acceptDrag;

  UInt16 noOfDraggedItems;

  wxDataViewCtrl* dataViewCtrlPtr;


  dataViewCtrlPtr = dynamic_cast<wxDataViewCtrl*>(GetWXPeer());
  wxCHECK_MSG(dataViewCtrlPtr != NULL,            false,_("Pointer to data view control not set correctly."));
  wxCHECK_MSG(dataViewCtrlPtr->GetModel() != NULL,false,_("Pointer to model not set correctly."));

 // send a drag possible event for each available and item und proceed with it unless the event is vetoed:
  ::CountDragItems(dragRef,&noOfDraggedItems);
  for (UInt16 indexDraggedItem=1; indexDraggedItem<=noOfDraggedItems; ++indexDraggedItem)
  {
   // collect native information:
    ItemReference          itemRef;
    wxDataObjectComposite* dataObjects;
    wxMemoryBuffer         buffer;

    ::GetDragItemReferenceNumber(dragRef,indexDraggedItem,&itemRef); // the index begins with 1!
    dataObjects = GetDnDDataObjects(dragRef,itemRef);
   // create wxWidget's event:
    wxDataViewEvent dataViewEvent(wxEVT_COMMAND_DATAVIEW_ITEM_DROP_POSSIBLE,dataViewCtrlPtr->GetId());

    dataViewEvent.SetEventObject(dataViewCtrlPtr);
    dataViewEvent.SetItem(reinterpret_cast<void*>(itemID)); // this is the item that receives the event
                                                            // (can be an invalid item ID, this is especially useful if the dataview does not contain any items)
    dataViewEvent.SetModel(dataViewCtrlPtr->GetModel());
    dataViewEvent.SetDataObject(dataObjects);
    dataViewEvent.SetDataFormat(GetDnDDataFormat(dataObjects));
    if (dataViewEvent.GetDataFormat().GetType() != wxDF_INVALID)
    {
      dataViewEvent.SetDataSize(dataObjects->GetDataSize(dataViewEvent.GetDataFormat().GetType()));
      dataObjects->GetDataHere(dataViewEvent.GetDataFormat().GetType(),buffer.GetWriteBuf(dataViewEvent.GetDataSize()));
      buffer.UngetWriteBuf(dataViewEvent.GetDataSize());
      dataViewEvent.SetDataBuffer(buffer.GetData());
    }

   // send event:
    acceptDrag = dataViewCtrlPtr->HandleWindowEvent(dataViewEvent) && dataViewEvent.IsAllowed();
    delete dataObjects;
    if (!acceptDrag)
      return false;
  }
  return true;
}

Boolean wxMacDataViewDataBrowserListViewControl::DataBrowserAddDragItemProc(DragReference dragRef, DataBrowserItemID itemID, ItemReference* itemRef)
{
  Boolean addDragItem;

  wxDataViewCtrl* dataViewCtrlPtr;

  wxDataViewItem dataViewItem;


  dataViewCtrlPtr = dynamic_cast<wxDataViewCtrl*>(GetWXPeer());
  wxCHECK_MSG(dataViewCtrlPtr != NULL,            false,_("Pointer to data view control not set correctly."));
  wxCHECK_MSG(dataViewCtrlPtr->GetModel() != NULL,false,_("Pointer to model not set correctly."));
  dataViewItem = reinterpret_cast<void*>(itemID);
  wxCHECK_MSG(dataViewItem.IsOk(),false,_("Invalid data view item"));

 // send a begin drag event and proceed with dragging unless the event is vetoed:
  wxDataViewEvent dataViewEvent(wxEVT_COMMAND_DATAVIEW_ITEM_BEGIN_DRAG,dataViewCtrlPtr->GetId());

  dataViewEvent.SetEventObject(dataViewCtrlPtr);
  dataViewEvent.SetItem(dataViewItem);
  dataViewEvent.SetModel(dataViewCtrlPtr->GetModel());
 // the dataview event object is also initialized with a default set of data; as it is a set of data and the user should be able to easily complete
 // the object a wxDataObjectComposite object is used;
 // currently, the composite object only contains a TAB concatenated string of all data:
  wxDataObjectComposite* dataObject(new wxDataObjectComposite());

  dataObject->Add(new wxTextDataObject(::ConcatenateDataViewItemValues(dataViewCtrlPtr,dataViewItem)));
  dataViewEvent.SetDataObject(dataObject);
 // check if event has not been vetoed:
  addDragItem = dataViewCtrlPtr->HandleWindowEvent(dataViewEvent) && dataViewEvent.IsAllowed();
  if (addDragItem)
  {
   // for the internal drag & drop functions create two flavors:
   //  - the data browser's item id;
   //  - the data contained the dataview event object (if available).
   // Definition: a flavor is the type dependent representation of identical data.
   // Example: a number can be represented by its value and by its value converted to a string. In this case the flavor
   //          of the number's internal representation is typeSInt32 while its string representation has got the flavor 'TEXT'.
   // Item id is one of the flavors:
    wxCHECK_MSG(::AddDragItemFlavor(dragRef,*itemRef,typeUInt32,&itemID,sizeof(itemID),0) == noErr,false,_("Unable to handle native drag&drop data"));
   // if the dataview event object contains data it is used for additional flavors; all natively known flavors are supported:
    if (dataViewEvent.GetDataObject() != NULL)
    {
     // constant definition for abbreviational purposes:
      size_t const noOfFormats = dataViewEvent.GetDataObject()->GetFormatCount();

      if (noOfFormats > 0)
      {
       // variable definition:
        wxDataFormat* dataFormats;

        dataFormats = new wxDataFormat[noOfFormats];
        dataViewEvent.GetDataObject()->GetAllFormats(dataFormats,wxDataObject::Get);
        for (size_t i=0; i<noOfFormats; ++i)
          switch (dataFormats[i].GetType())
          {
            case wxDF_INVALID:
              wxFAIL_MSG(_("Data object has invalid data format"));
              break;
            case wxDF_TEXT:
              {
               // constant definition for abbreviational purposes:
                size_t const dataSize = dataViewEvent.GetDataObject()->GetDataSize(wxDF_TEXT);
               // variable definition and initialization:
                wxMemoryBuffer dataObject(dataSize);

                dataViewEvent.GetDataObject()->GetDataHere(wxDF_TEXT,dataObject.GetWriteBuf(dataSize));
                dataObject.UngetWriteBuf(dataSize);
                if (::AddDragItemFlavor(dragRef,*itemRef,'TEXT',dataObject.GetData(),dataSize,0) != noErr)
                  wxFAIL_MSG(_("Adding flavor TEXT failed"));
              } /* block */
              break;
            case wxDF_UNICODETEXT:
              {
               // constant definition for abbreviational purposes:
                size_t const dataSize = dataViewEvent.GetDataObject()->GetDataSize(wxDF_TEXT);
               // as there is no direct access to the data copy it to a memory buffer:
                wxMemoryBuffer dataObject(dataSize);

                dataViewEvent.GetDataObject()->GetDataHere(wxDF_TEXT,dataObject.GetWriteBuf(dataSize));
                dataObject.UngetWriteBuf(dataSize);
               // if the data is stored in unicode format the internal representation is utf-8 (not mentioned in the documentation but in the source code);
               // DnD uses fixed utf-16 representation -> use the OSX functions for a conversion:
                CFDataRef   osxData  (::CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,reinterpret_cast<UInt8*>(dataObject.GetData()),dataSize,kCFAllocatorNull));
                CFStringRef osxString(::CFStringCreateFromExternalRepresentation(kCFAllocatorDefault,osxData,kCFStringEncodingUTF8));

               // the osxString contains now the data and therefore the previously occupied memory can be released and re-used:
                ::CFRelease(osxData);
                osxData = ::CFStringCreateExternalRepresentation(kCFAllocatorDefault,osxString,kCFStringEncodingUTF16,32);
                if (::AddDragItemFlavor(dragRef,*itemRef,'utxt',::CFDataGetBytePtr(osxData),::CFDataGetLength(osxData),0) != noErr)
                  wxFAIL_MSG(_("Adding flavor utxt failed"));
               // clean up:
                ::CFRelease(osxData);
                ::CFRelease(osxString);
              } /* block */
              break;
            case wxDF_BITMAP:
            case wxDF_METAFILE:
            case wxDF_SYLK:
            case wxDF_DIF:
            case wxDF_TIFF:
            case wxDF_OEMTEXT:
            case wxDF_DIB:
            case wxDF_PALETTE:
            case wxDF_PENDATA:
            case wxDF_RIFF:
            case wxDF_WAVE:
            case wxDF_ENHMETAFILE:
            case wxDF_FILENAME:
            case wxDF_LOCALE:
            case wxDF_PRIVATE:
            case wxDF_HTML:
              break; // not (yet) supported data formats
            default:
              wxFAIL_MSG(_("Unknown data format"));
          }
        delete[] dataFormats;
      }
    }
  }
 // clean-up and return result:
  delete dataObject;
  return addDragItem;
}

Boolean wxMacDataViewDataBrowserListViewControl::DataBrowserReceiveDragProc(DragReference dragRef, DataBrowserItemID itemID)
{
  UInt16 noOfDraggedItems;

  wxDataViewCtrl* dataViewCtrlPtr;


  dataViewCtrlPtr = dynamic_cast<wxDataViewCtrl*>(GetWXPeer());
  wxCHECK_MSG(dataViewCtrlPtr != NULL,            false,_("Pointer to data view control not set correctly."));
  wxCHECK_MSG(dataViewCtrlPtr->GetModel() != NULL,false,_("Pointer to model not set correctly."));

 // send a drag possible event for each available and item und proceed with it unless the event is vetoed:
  ::CountDragItems(dragRef,&noOfDraggedItems);
  for (UInt16 indexDraggedItem=1; indexDraggedItem<=noOfDraggedItems; ++indexDraggedItem)
  {
    bool                   receiveDrag;
    ItemReference          itemRef;
    wxDataObjectComposite* dataObjects;
    wxMemoryBuffer         buffer;

   // collect native information:
    ::GetDragItemReferenceNumber(dragRef,indexDraggedItem,&itemRef); // the index begins with 1!
    dataObjects = GetDnDDataObjects(dragRef,itemRef);
  // create wxWidget's event:
    wxDataViewEvent dataViewEvent(wxEVT_COMMAND_DATAVIEW_ITEM_DROP,dataViewCtrlPtr->GetId());

    dataViewEvent.SetEventObject(dataViewCtrlPtr);
    dataViewEvent.SetItem(reinterpret_cast<void*>(itemID)); // this is the item that receives the event
                                                            // (can be an invalid item ID, this is especially useful if the dataview does not contain any items)
    dataViewEvent.SetModel(dataViewCtrlPtr->GetModel());
    dataViewEvent.SetDataObject(dataObjects);
    dataViewEvent.SetDataFormat(GetDnDDataFormat(dataObjects));
    if (dataViewEvent.GetDataFormat().GetType() != wxDF_INVALID)
    {
      dataViewEvent.SetDataSize(dataObjects->GetDataSize(dataViewEvent.GetDataFormat().GetType()));
      dataObjects->GetDataHere(dataViewEvent.GetDataFormat().GetType(),buffer.GetWriteBuf(dataViewEvent.GetDataSize()));
      buffer.UngetWriteBuf(dataViewEvent.GetDataSize());
      dataViewEvent.SetDataBuffer(buffer.GetData());
    }

   // send event:
    receiveDrag = dataViewCtrlPtr->HandleWindowEvent(dataViewEvent) && dataViewEvent.IsAllowed();
    delete dataObjects;
    if (!receiveDrag)
      return false;
  }
  return true;
}

//
// drag & drop helper methods
//
wxDataFormat wxMacDataViewDataBrowserListViewControl::GetDnDDataFormat(wxDataObjectComposite* dataObjects)
{
  wxDataFormat resultFormat;


  if (dataObjects != NULL)
  {
    bool compatible(true);

    size_t const noOfFormats = dataObjects->GetFormatCount();
    size_t       indexFormat;

    wxDataFormat* formats;

   // get all formats and check afterwards if the formats are compatible; if they are compatible the preferred format is returned otherwise
   // wxDF_INVALID is returned;
   // currently compatible types (ordered by priority are):
   //  - wxDF_UNICODETEXT - wxDF_TEXT
    formats = new wxDataFormat[noOfFormats];
    dataObjects->GetAllFormats(formats);
    indexFormat = 0;
    while ((indexFormat < noOfFormats) && compatible)
    {
      switch (resultFormat.GetType())
      {
        case wxDF_INVALID:
          resultFormat.SetType(formats[indexFormat].GetType()); // first format (should only be reached if indexFormat == 0
          break;
        case wxDF_TEXT:
          if (formats[indexFormat].GetType() == wxDF_UNICODETEXT)
            resultFormat.SetType(wxDF_UNICODETEXT);
          else // incompatible
          {
            resultFormat.SetType(wxDF_INVALID);
            compatible = false;
          }
          break;
        case wxDF_UNICODETEXT:
          if (formats[indexFormat].GetType() != wxDF_TEXT)
          {
            resultFormat.SetType(wxDF_INVALID);
            compatible = false;
          }
          break;
        default:
          resultFormat.SetType(wxDF_INVALID); // not (yet) supported format
          compatible = false;
      }
      ++indexFormat;
    } /* while */
   // clean up:
    delete[] formats;
  }
  else
    resultFormat = wxDF_INVALID;
  return resultFormat;
}

wxDataObjectComposite* wxMacDataViewDataBrowserListViewControl::GetDnDDataObjects(DragReference dragRef, ItemReference itemRef) const
{
  UInt16 noOfFlavors;

  wxDataObjectComposite* dataObject;


  ::CountDragItemFlavors(dragRef,itemRef,&noOfFlavors);
  if (noOfFlavors > 0)
  {
   // as the native drag data can be separated into TEXT and UTXT a pointer to a wxTextDataObject is used to track the existence of 'TEXT' and 'utxt' flavors:
    wxTextDataObject* textDataObject(NULL);

    dataObject = new wxDataObjectComposite();
    for (UInt16 indexFlavor=1; indexFlavor<=noOfFlavors; ++indexFlavor)
    {
     // variable definition:
      FlavorType flavorDataObject;

      if (::GetFlavorType(dragRef,itemRef,indexFlavor,&flavorDataObject) == noErr) // GetFlavorType uses a 1 based index!
        switch (flavorDataObject)
        {
          case 'TEXT':
            if (textDataObject == NULL) // otherwise a 'utxt' flavor has already been found that gets priority compared to the 'TEXT' flavor
            {
             // variable definitions:
              Size           nativeDataSize;
              wxMemoryBuffer nativeData;

              if ((::GetFlavorDataSize(dragRef,itemRef,'TEXT',&nativeDataSize) == noErr) &&
                  (::GetFlavorData(dragRef,itemRef,'TEXT',nativeData.GetWriteBuf(nativeDataSize),&nativeDataSize,0) == noErr))
              {
                nativeData.UngetWriteBuf(nativeDataSize);
                textDataObject = new wxTextDataObject();
                if (textDataObject->SetData(nativeData.GetDataLen(),nativeData.GetData()))
                  dataObject->Add(textDataObject);
                else
                {
                  wxDELETE(textDataObject);
                }
              }
            } /* block */
            break;
          case 'utxt':
            {
             // variable definition:
              Size nativeDataSize;

              if (::GetFlavorDataSize(dragRef,itemRef,'utxt',&nativeDataSize) == noErr)
              {
                CFMutableDataRef draggedData;

                draggedData = ::CFDataCreateMutable(kCFAllocatorDefault,nativeDataSize);
                ::CFDataSetLength(draggedData,nativeDataSize);
                if (::GetFlavorData(dragRef,itemRef,'utxt',::CFDataGetMutableBytePtr(draggedData),&nativeDataSize,0) == noErr)
                {
                 // convert internally used UTF-16 representation to a UTF-8 representation:
                  CFDataRef   osxData;
                  CFStringRef osxString;

                  osxString = ::CFStringCreateFromExternalRepresentation(kCFAllocatorDefault,draggedData,kCFStringEncodingUTF16); // BOM character is handled by this function automatically
                  osxData   = ::CFStringCreateExternalRepresentation(kCFAllocatorDefault,osxString,kCFStringEncodingUTF8,32);
                  if (textDataObject == NULL)
                  {
                    textDataObject = new wxTextDataObject();
                    if (textDataObject->SetData(::CFDataGetLength(osxData),::CFDataGetBytePtr(osxData)))
                      dataObject->Add(textDataObject);
                    else
                    {
                      wxDELETE(textDataObject);
                    }
                  }
                  else // overwrite data because the 'utxt' flavor has priority over the 'TEXT' flavor
                    (void) textDataObject->SetData(::CFDataGetLength(osxData),::CFDataGetBytePtr(osxData));
                 // clean up:
                  ::CFRelease(osxData);
                  ::CFRelease(osxString);
                }
               // clean up:
                ::CFRelease(draggedData);
              }
            } /* block */
            break;
        }
    }
  }
  else
    dataObject = NULL;
  return dataObject;
}

//
// other methods
//
wxDataViewColumn* wxMacDataViewDataBrowserListViewControl::GetColumnPtr(DataBrowserPropertyID propertyID) const
{
  wxDataViewCtrl* dataViewCtrlPtr(dynamic_cast<wxDataViewCtrl*>(GetWXPeer()));

  if (dataViewCtrlPtr != NULL)
  {
    size_t const noOfColumns = dataViewCtrlPtr->GetColumnCount();

    for (size_t i=0; i<noOfColumns; ++i)
      if (dataViewCtrlPtr->GetColumnPtr(i)->GetNativeData()->GetPropertyID() == propertyID)
        return dataViewCtrlPtr->GetColumnPtr(i);
  }
  return NULL;
}

// ---------------------------------------------------------
// wxDataViewRenderer
// ---------------------------------------------------------
wxDataViewRenderer::wxDataViewRenderer(wxString const& varianttype, wxDataViewCellMode mode, int align)
                   :wxDataViewRendererBase(varianttype,mode,align), m_alignment(align), m_mode(mode), m_NativeDataPtr(NULL)
{
}

wxDataViewRenderer::~wxDataViewRenderer()
{
  delete m_NativeDataPtr;
}

void wxDataViewRenderer::SetAlignment(int align)
{
  m_alignment = align;
}

namespace
{

// get the browser control or NULL if anything went wrong (it's not supposed to
// so we assert if it did)
wxMacDataViewDataBrowserListViewControl *
GetBrowserFromCol(wxDataViewColumn *col)
{
    wxCHECK_MSG( col, NULL, "should have a valid column" );

    wxDataViewCtrl * const dvc = col->GetOwner();
    wxCHECK_MSG( dvc, NULL, "column must be associated with the control" );

    return static_cast<wxMacDataViewDataBrowserListViewControl *>(dvc->GetPeer());
}

} // anonymous namespace

void wxDataViewRenderer::SetMode(wxDataViewCellMode mode)
{
    wxDataViewColumn * const col = GetOwner();
    wxMacDataViewDataBrowserListViewControl * const
        browser = GetBrowserFromCol(col);
    wxCHECK_RET( browser, "must be fully initialized" );

    const DataBrowserPropertyID colID = col->GetNativeData()->GetPropertyID();

    DataBrowserPropertyFlags flags;
    verify_noerr( browser->GetPropertyFlags(colID, &flags) );

    if ( (mode == wxDATAVIEW_CELL_EDITABLE) ||
            (mode == wxDATAVIEW_CELL_ACTIVATABLE) )
        flags |= kDataBrowserPropertyIsEditable;
    else
        flags &= ~kDataBrowserPropertyIsEditable;

    verify_noerr( browser->SetPropertyFlags(colID, flags) );
}

void wxDataViewRenderer::EnableEllipsize(wxEllipsizeMode mode)
{
    wxDataViewColumn * const col = GetOwner();

    wxMacDataViewDataBrowserListViewControl * const
        browser = GetBrowserFromCol(col);
    wxCHECK_RET( browser, "must be fully initialized" );

    const DataBrowserPropertyID colID = col->GetNativeData()->GetPropertyID();

    DataBrowserPropertyFlags flags;
    browser->GetPropertyFlags(colID, &flags);

    flags &= ~(kDataBrowserDoNotTruncateText |
               kDataBrowserTruncateTextAtStart |
               kDataBrowserTruncateTextMiddle |
               kDataBrowserTruncateTextAtEnd);

    int flagToSet = 0;
    switch ( mode )
    {
        case wxELLIPSIZE_NONE:
            flagToSet = kDataBrowserDoNotTruncateText;
            break;

        case wxELLIPSIZE_START:
            flagToSet = kDataBrowserTruncateTextAtStart;
            break;

        case wxELLIPSIZE_MIDDLE:
            flagToSet = kDataBrowserTruncateTextMiddle;
            break;

        case wxELLIPSIZE_END:
            flagToSet = kDataBrowserTruncateTextAtEnd;
            break;
    }

    wxCHECK_RET( flagToSet, "unknown wxEllipsizeMode value" );

    flags |= flagToSet;
    verify_noerr( browser->SetPropertyFlags(colID, flags) );
}

wxEllipsizeMode wxDataViewRenderer::GetEllipsizeMode() const
{
    wxDataViewColumn * const col = GetOwner();

    wxMacDataViewDataBrowserListViewControl * const
        browser = GetBrowserFromCol(col);
    wxCHECK_MSG( browser, wxELLIPSIZE_NONE, "must be fully initialized" );

    const DataBrowserPropertyID colID = col->GetNativeData()->GetPropertyID();

    DataBrowserPropertyFlags flags;
    browser->GetPropertyFlags(colID, &flags);

    if ( flags & kDataBrowserDoNotTruncateText )
        return wxELLIPSIZE_NONE;
    if ( flags & kDataBrowserTruncateTextAtStart )
        return wxELLIPSIZE_START;
    if ( flags & kDataBrowserTruncateTextAtEnd )
        return wxELLIPSIZE_END;

    // kDataBrowserTruncateTextMiddle == 0 so there is no need to test for it
    return wxELLIPSIZE_MIDDLE;
}

void wxDataViewRenderer::SetNativeData(wxDataViewRendererNativeData* newNativeDataPtr)
{
  delete m_NativeDataPtr;
  m_NativeDataPtr = newNativeDataPtr;
}

IMPLEMENT_ABSTRACT_CLASS(wxDataViewRenderer,wxDataViewRendererBase)

// ---------------------------------------------------------
// wxDataViewCustomRenderer
// ---------------------------------------------------------
wxDataViewCustomRenderer::wxDataViewCustomRenderer(wxString const& varianttype, wxDataViewCellMode mode, int align)
                         :wxDataViewCustomRendererBase(varianttype,mode,align),
                          m_editorCtrlPtr(NULL),
                          m_DCPtr(NULL)
{
  SetNativeData(new wxDataViewRendererNativeData(kDataBrowserCustomType));
}

bool wxDataViewCustomRenderer::MacRender()
{
  return true;
}

IMPLEMENT_ABSTRACT_CLASS(wxDataViewCustomRenderer, wxDataViewRenderer)

// ---------------------------------------------------------
// wxDataViewTextRenderer
// ---------------------------------------------------------
wxDataViewTextRenderer::wxDataViewTextRenderer(wxString const& varianttype, wxDataViewCellMode mode, int align)
                       :wxDataViewRenderer(varianttype,mode,align)
{
  SetNativeData(new wxDataViewRendererNativeData(kDataBrowserTextType));
}

bool wxDataViewTextRenderer::MacRender()
{
  wxCHECK_MSG(GetValue().GetType() == GetVariantType(),false,wxString(_("Text renderer cannot render value; value type: ")) << GetValue().GetType());

  wxCFStringRef cfString(GetValue().GetString(),(GetView()->GetFont().Ok() ? GetView()->GetFont().GetEncoding() : wxLocale::GetSystemEncoding()));
  return (::SetDataBrowserItemDataText(GetNativeData()->GetItemDataRef(),cfString) == noErr);
}

IMPLEMENT_CLASS(wxDataViewTextRenderer,wxDataViewRenderer)

// ---------------------------------------------------------
// wxDataViewBitmapRenderer
// ---------------------------------------------------------
wxDataViewBitmapRenderer::wxDataViewBitmapRenderer(wxString const& varianttype, wxDataViewCellMode mode, int align)
                         :wxDataViewRenderer(varianttype,mode,align)
{
  SetNativeData(new wxDataViewRendererNativeData(kDataBrowserIconType));
}

bool wxDataViewBitmapRenderer::MacRender()
 // This method returns 'true' if
 //  - the passed bitmap is valid and it could be assigned to the native data browser;
 //  - the passed bitmap is invalid (or is not initialized); this case simulates a non-existing bitmap.
 // In all other cases the method returns 'false'.
{
  wxCHECK_MSG(GetValue().GetType() == GetVariantType(),false,wxString(_("Bitmap renderer cannot render value; value type: ")) << GetValue().GetType());

  wxBitmap bitmap;

  bitmap << GetValue();
  return (!(bitmap.Ok()) || (::SetDataBrowserItemDataIcon(GetNativeData()->GetItemDataRef(),bitmap.GetIconRef()) == noErr));
}

IMPLEMENT_CLASS(wxDataViewBitmapRenderer,wxDataViewRenderer)

// ---------------------------------------------------------
// wxDataViewIconTextRenderer
// ---------------------------------------------------------
wxDataViewIconTextRenderer::wxDataViewIconTextRenderer(
    const wxString& varianttype,
    wxDataViewCellMode mode,
    int WXUNUSED(align))
                           :wxDataViewRenderer(varianttype,mode)
{
  SetNativeData(new wxDataViewRendererNativeData(kDataBrowserIconAndTextType));
}

bool wxDataViewIconTextRenderer::MacRender()
{
  wxCHECK_MSG(GetValue().GetType() == GetVariantType(),false,wxString(_("Icon & text renderer cannot render value; value type: ")) << GetValue().GetType());

  wxDataViewIconText iconText;

  iconText << GetValue();

  wxCFStringRef cfString(iconText.GetText(),(GetView()->GetFont().Ok() ? GetView()->GetFont().GetEncoding() : wxLocale::GetSystemEncoding()));

  if (iconText.GetIcon().IsOk())
    if (::SetDataBrowserItemDataIcon(GetNativeData()->GetItemDataRef(),MAC_WXHICON(iconText.GetIcon().GetHICON())) != noErr)
      return false;
  return (::SetDataBrowserItemDataText(GetNativeData()->GetItemDataRef(),cfString) == noErr);
}

IMPLEMENT_ABSTRACT_CLASS(wxDataViewIconTextRenderer,wxDataViewRenderer)


// ---------------------------------------------------------
// wxDataViewToggleRenderer
// ---------------------------------------------------------
wxDataViewToggleRenderer::wxDataViewToggleRenderer(
    const wxString& varianttype,
    wxDataViewCellMode mode,
    int WXUNUSED(align))
                         :wxDataViewRenderer(varianttype,mode)
{
  SetNativeData(new wxDataViewRendererNativeData(kDataBrowserCheckboxType));
}

bool wxDataViewToggleRenderer::MacRender()
{
  wxCHECK_MSG(GetValue().GetType() == GetVariantType(),false,wxString(_("Toggle renderer cannot render value; value type: ")) << GetValue().GetType());
  return (::SetDataBrowserItemDataButtonValue(GetNativeData()->GetItemDataRef(),GetValue().GetBool()) == noErr);
}

IMPLEMENT_ABSTRACT_CLASS(wxDataViewToggleRenderer,wxDataViewRenderer)

// ---------------------------------------------------------
// wxDataViewProgressRenderer
// ---------------------------------------------------------
wxDataViewProgressRenderer::wxDataViewProgressRenderer(
    const wxString& WXUNUSED(label),
    wxString const& varianttype,
    wxDataViewCellMode mode,
    int align)
                           :wxDataViewRenderer(varianttype,mode,align)
{
  SetNativeData(new wxDataViewRendererNativeData(kDataBrowserProgressBarType));
}

bool wxDataViewProgressRenderer::MacRender()
{
  wxCHECK_MSG(GetValue().GetType() == GetVariantType(),false,wxString(_("Progress renderer cannot render value type; value type: ")) << GetValue().GetType());
  return ((::SetDataBrowserItemDataMinimum(GetNativeData()->GetItemDataRef(),  0)                        == noErr) &&
          (::SetDataBrowserItemDataMaximum(GetNativeData()->GetItemDataRef(),100)                        == noErr) &&
          (::SetDataBrowserItemDataValue  (GetNativeData()->GetItemDataRef(),GetValue().GetLong()) == noErr));
}

IMPLEMENT_ABSTRACT_CLASS(wxDataViewProgressRenderer,wxDataViewRenderer)

// ---------------------------------------------------------
// wxDataViewDateRenderer
// ---------------------------------------------------------
wxDataViewDateRenderer::wxDataViewDateRenderer(wxString const& varianttype, wxDataViewCellMode mode, int align)
                       :wxDataViewRenderer(varianttype,mode,align)
{
  SetNativeData(new wxDataViewRendererNativeData(kDataBrowserDateTimeType));
}

bool wxDataViewDateRenderer::MacRender()
{
  wxCHECK_MSG(GetValue().GetType() == GetVariantType(),false,wxString(_("Date renderer cannot render value; value type: ")) << GetValue().GetType());
  return (::SetDataBrowserItemDataDateTime(GetNativeData()->GetItemDataRef(),GetValue().GetDateTime().Subtract(wxDateTime(1,wxDateTime::Jan,1904)).GetSeconds().GetLo()) == noErr);
}

IMPLEMENT_ABSTRACT_CLASS(wxDataViewDateRenderer,wxDataViewRenderer)

// ---------------------------------------------------------
// wxDataViewColumn
// ---------------------------------------------------------
wxDataViewColumn::wxDataViewColumn(const wxString& title, wxDataViewRenderer* renderer, unsigned int model_column, int width, wxAlignment align, int flags)
                 :wxDataViewColumnBase(renderer, model_column), m_NativeDataPtr(new wxDataViewColumnNativeData()), m_title(title)
{
  InitCommon(width, align, flags);
  if ((renderer != NULL) && !renderer->IsCustomRenderer() &&
      (renderer->GetAlignment() == wxDVR_DEFAULT_ALIGNMENT))
      renderer->SetAlignment(align);
}

wxDataViewColumn::wxDataViewColumn(const wxBitmap& bitmap, wxDataViewRenderer* renderer, unsigned int model_column, int width, wxAlignment align, int flags)
                 :wxDataViewColumnBase(bitmap, renderer, model_column), m_NativeDataPtr(new wxDataViewColumnNativeData())
{
  InitCommon(width, align, flags);
  if ((renderer != NULL) && !renderer->IsCustomRenderer() &&
      (renderer->GetAlignment() == wxDVR_DEFAULT_ALIGNMENT))
      renderer->SetAlignment(align);
}

wxDataViewColumn::~wxDataViewColumn()
{
  delete m_NativeDataPtr;
}

int wxDataViewColumn::GetWidth() const
{
    // FIXME: This returns the last programmatically set width and will not work if
    //        the user changes the column's width by dragging it with the mouse.
    return m_width;
}

bool wxDataViewColumn::IsSortKey() const
{
    wxDataViewCtrl * const dataViewCtrlPtr(GetOwner());
    wxMacDataViewDataBrowserListViewControlPointer macDataViewListCtrlPtr(
        dynamic_cast<wxMacDataViewDataBrowserListViewControlPointer>(
            dataViewCtrlPtr->GetPeer()));

    DataBrowserPropertyID propertyID;
    return (macDataViewListCtrlPtr->GetSortProperty(&propertyID) == noErr) &&
           (propertyID == GetNativeData()->GetPropertyID());
}

void wxDataViewColumn::SetAlignment(wxAlignment align)
{
  wxDataViewCtrl* dataViewCtrlPtr(GetOwner());


  m_alignment = align;
  if (dataViewCtrlPtr != NULL)
  {
    wxMacDataViewDataBrowserListViewControlPointer macDataViewListCtrlPtr(dynamic_cast<wxMacDataViewDataBrowserListViewControlPointer>(dataViewCtrlPtr->GetPeer()));

    if (macDataViewListCtrlPtr != NULL)
    {
      DataBrowserListViewHeaderDesc headerDescription;

      wxCHECK_RET(macDataViewListCtrlPtr->GetHeaderDesc(GetNativeData()->GetPropertyID(),&headerDescription) == noErr,_("Could not get header description."));
      switch (align)
      {
        case wxALIGN_CENTER:
        case wxALIGN_CENTER_HORIZONTAL:
          headerDescription.btnFontStyle.just = teCenter;
          break;
        case wxALIGN_LEFT:
          headerDescription.btnFontStyle.just = teFlushLeft;
          break;
        case wxALIGN_RIGHT:
          headerDescription.btnFontStyle.just = teFlushRight;
          break;
        default:
          headerDescription.btnFontStyle.just = teFlushDefault;
      }
      wxCHECK_RET(macDataViewListCtrlPtr->SetHeaderDesc(GetNativeData()->GetPropertyID(),&headerDescription) == noErr,_("Could not set alignment."));
    }
  }
}

void wxDataViewColumn::SetBitmap(wxBitmap const& bitmap)
{
  wxDataViewCtrl* dataViewCtrlPtr(GetOwner());


  wxDataViewColumnBase::SetBitmap(bitmap);
  if (dataViewCtrlPtr != NULL)
  {
    wxMacDataViewDataBrowserListViewControlPointer macDataViewListCtrlPtr(dynamic_cast<wxMacDataViewDataBrowserListViewControlPointer>(dataViewCtrlPtr->GetPeer()));

    if (macDataViewListCtrlPtr != NULL)
    {
      DataBrowserListViewHeaderDesc headerDescription;

      wxCHECK_RET(macDataViewListCtrlPtr->GetHeaderDesc(GetNativeData()->GetPropertyID(),&headerDescription) == noErr,_("Could not get header description."));
      if (GetBitmap().Ok())
        headerDescription.btnContentInfo.u.iconRef = GetBitmap().GetIconRef();
      else
        headerDescription.btnContentInfo.u.iconRef = NULL;
      wxCHECK_RET(macDataViewListCtrlPtr->SetHeaderDesc(GetNativeData()->GetPropertyID(),&headerDescription) == noErr,_("Could not set icon."));
    }
  }
}

void wxDataViewColumn::SetMaxWidth(int maxWidth)
{
  wxDataViewCtrl* dataViewCtrlPtr(GetOwner());


  m_maxWidth = maxWidth;
  if (dataViewCtrlPtr != NULL)
  {
    wxMacDataViewDataBrowserListViewControlPointer macDataViewListCtrlPtr(dynamic_cast<wxMacDataViewDataBrowserListViewControlPointer>(dataViewCtrlPtr->GetPeer()));

    if (macDataViewListCtrlPtr != NULL)
    {
      DataBrowserListViewHeaderDesc headerDescription;

      wxCHECK_RET(macDataViewListCtrlPtr->GetHeaderDesc(GetNativeData()->GetPropertyID(),&headerDescription) == noErr,_("Could not get header description."));
      headerDescription.maximumWidth = static_cast<UInt16>(maxWidth);
      wxCHECK_RET(macDataViewListCtrlPtr->SetHeaderDesc(GetNativeData()->GetPropertyID(),&headerDescription) == noErr,_("Could not set maximum width."));
    }
  }
}

void wxDataViewColumn::SetMinWidth(int minWidth)
{
  wxDataViewCtrl* dataViewCtrlPtr(GetOwner());


  m_minWidth = minWidth;
  if (dataViewCtrlPtr != NULL)
  {
    wxMacDataViewDataBrowserListViewControlPointer macDataViewListCtrlPtr(dynamic_cast<wxMacDataViewDataBrowserListViewControlPointer>(dataViewCtrlPtr->GetPeer()));

    if (macDataViewListCtrlPtr != NULL)
    {
      DataBrowserListViewHeaderDesc headerDescription;

      wxCHECK_RET(macDataViewListCtrlPtr->GetHeaderDesc(GetNativeData()->GetPropertyID(),&headerDescription) == noErr,_("Could not get header description."));
      headerDescription.minimumWidth = static_cast<UInt16>(minWidth);
      wxCHECK_RET(macDataViewListCtrlPtr->SetHeaderDesc(GetNativeData()->GetPropertyID(),&headerDescription) == noErr,_("Could not set minimum width."));
    }
  }
}

void wxDataViewColumn::SetReorderable(bool reorderable)
{
 // first set the internal flag of the column:
  if (reorderable)
    m_flags |= wxDATAVIEW_COL_REORDERABLE;
  else
    m_flags &= ~wxDATAVIEW_COL_REORDERABLE;
 // if the column is associated with a control change also immediately the flags of the control:
  wxDataViewCtrl* dataViewCtrlPtr(GetOwner());

  if (dataViewCtrlPtr != NULL)
  {
    DataBrowserPropertyFlags                       flags;
    wxMacDataViewDataBrowserListViewControlPointer macDataViewListCtrlPtr(dynamic_cast<wxMacDataViewDataBrowserListViewControlPointer>(dataViewCtrlPtr->GetPeer()));

    wxCHECK_RET(macDataViewListCtrlPtr != NULL,                                                                  _("Valid pointer to native data view control does not exist"));
    wxCHECK_RET(macDataViewListCtrlPtr->GetPropertyFlags(GetNativeData()->GetPropertyID(),&flags) == noErr,_("Could not get property flags."));
    if (reorderable)
      flags |= kDataBrowserListViewMovableColumn;
    else
      flags &= ~kDataBrowserListViewMovableColumn;
    wxCHECK_RET(macDataViewListCtrlPtr->SetPropertyFlags(GetNativeData()->GetPropertyID(),flags) == noErr,_("Could not set property flags."));
  }
}

void wxDataViewColumn::SetResizeable(bool resizeable)
{
 // first set the internal flag of the column:
  if (resizeable)
    m_flags |= wxDATAVIEW_COL_RESIZABLE;
  else
    m_flags &= ~wxDATAVIEW_COL_RESIZABLE;
 // if the column is associated with a control change also immediately the flags of the control:
  wxDataViewCtrl* dataViewCtrlPtr(GetOwner());

  if (dataViewCtrlPtr != NULL)
  {
    wxMacDataViewDataBrowserListViewControlPointer macDataViewListCtrlPtr(dynamic_cast<wxMacDataViewDataBrowserListViewControlPointer>(dataViewCtrlPtr->GetPeer()));

    if (macDataViewListCtrlPtr != NULL)
    {
      DataBrowserListViewHeaderDesc headerDescription;

      verify_noerr(macDataViewListCtrlPtr->GetHeaderDesc(GetNativeData()->GetPropertyID(),&headerDescription));
      if (resizeable)
      {
        if (GetMinWidth() >= GetMaxWidth())
        {
          m_minWidth = 0;
          m_maxWidth = 30000;
        }
        headerDescription.minimumWidth = m_minWidth;
        headerDescription.maximumWidth = m_maxWidth;
      }
      else
      {
        headerDescription.minimumWidth = m_width;
        headerDescription.maximumWidth = m_width;
      }
      verify_noerr(macDataViewListCtrlPtr->SetHeaderDesc(GetNativeData()->GetPropertyID(),&headerDescription));
      macDataViewListCtrlPtr->SetSortProperty(GetNativeData()->GetPropertyID());
    }
  }
}

void wxDataViewColumn::SetSortable(bool sortable)
{
 // first set the internal flag of the column:
  if (sortable)
    m_flags |= wxDATAVIEW_COL_SORTABLE;
  else
    m_flags &= ~wxDATAVIEW_COL_SORTABLE;
 // if the column is associated with a control change also immediately the flags of the control:
  wxDataViewCtrl* dataViewCtrlPtr(GetOwner());

  if (dataViewCtrlPtr != NULL)
  {
    DataBrowserPropertyFlags                       flags;
    wxMacDataViewDataBrowserListViewControlPointer macDataViewListCtrlPtr(dynamic_cast<wxMacDataViewDataBrowserListViewControlPointer>(dataViewCtrlPtr->GetPeer()));

    wxCHECK_RET(macDataViewListCtrlPtr != NULL,                                                 _("Valid pointer to native data view control does not exist"));
    wxCHECK_RET(macDataViewListCtrlPtr->GetPropertyFlags(GetNativeData()->GetPropertyID(),&flags) == noErr,_("Could not get property flags."));
    if (sortable)
      flags |= kDataBrowserListViewSortableColumn;
    else
      flags &= ~kDataBrowserListViewSortableColumn;
    wxCHECK_RET(macDataViewListCtrlPtr->SetPropertyFlags(GetNativeData()->GetPropertyID(),flags) == noErr,_("Could not set property flags."));
  }
}

void wxDataViewColumn::SetSortOrder(bool ascending)
{
  wxDataViewCtrl* dataViewCtrlPtr(GetOwner());


  m_ascending = ascending;
  if (dataViewCtrlPtr != NULL)
  {
    wxMacDataViewDataBrowserListViewControlPointer macDataViewListCtrlPtr(dynamic_cast<wxMacDataViewDataBrowserListViewControlPointer>(dataViewCtrlPtr->GetPeer()));

    if (macDataViewListCtrlPtr != NULL)
    {
      DataBrowserListViewHeaderDesc headerDescription;

      verify_noerr(macDataViewListCtrlPtr->GetHeaderDesc(GetNativeData()->GetPropertyID(),&headerDescription));
      if (ascending)
        headerDescription.initialOrder = kDataBrowserOrderIncreasing;
      else
        headerDescription.initialOrder = kDataBrowserOrderDecreasing;
      verify_noerr(macDataViewListCtrlPtr->SetHeaderDesc(GetNativeData()->GetPropertyID(),&headerDescription));
      macDataViewListCtrlPtr->SetSortProperty(GetNativeData()->GetPropertyID());
    }
  }
}

void wxDataViewColumn::SetTitle(wxString const& title)
{
  wxDataViewCtrl* dataViewCtrlPtr(GetOwner());


  m_title = title;
  if (dataViewCtrlPtr != NULL)
  {
    wxMacDataViewDataBrowserListViewControlPointer macDataViewListCtrlPtr(dynamic_cast<wxMacDataViewDataBrowserListViewControlPointer>(dataViewCtrlPtr->GetPeer()));

    if (macDataViewListCtrlPtr != NULL)
    {
      DataBrowserListViewHeaderDesc headerDescription;
      wxCFStringRef           cfTitle(title,(dataViewCtrlPtr->GetFont().Ok() ? dataViewCtrlPtr->GetFont().GetEncoding() : wxLocale::GetSystemEncoding()));

      wxCHECK_RET(macDataViewListCtrlPtr->GetHeaderDesc(GetNativeData()->GetPropertyID(),&headerDescription) == noErr,_("Could not get header description."));
      headerDescription.titleString = cfTitle;
      wxCHECK_RET(macDataViewListCtrlPtr->SetHeaderDesc(GetNativeData()->GetPropertyID(),&headerDescription) == noErr,_("Could not set header description."));
    }
  }
}

void wxDataViewColumn::SetWidth(int width)
{
  wxDataViewCtrl* dataViewCtrlPtr(GetOwner());

  switch ( width )
  {
    case wxCOL_WIDTH_AUTOSIZE:
       // not implemented, fall through
    case wxCOL_WIDTH_DEFAULT:
      width = wxDVC_DEFAULT_WIDTH;
      break;
    default:
      break;
  }

  if ((width >= m_minWidth) && (width <= m_maxWidth))
  {
    m_width = width;
    if (dataViewCtrlPtr != NULL)
    {
      wxMacDataViewDataBrowserListViewControlPointer macDataViewListCtrlPtr(dynamic_cast<wxMacDataViewDataBrowserListViewControlPointer>(dataViewCtrlPtr->GetPeer()));

      if (macDataViewListCtrlPtr != NULL)
        wxCHECK_RET(macDataViewListCtrlPtr->SetColumnWidth(GetNativeData()->GetPropertyID(),static_cast<UInt16>(width)) == noErr,_("Could not set column width."));
    }
  }
}

void wxDataViewColumn::SetHidden(bool WXUNUSED(hidden))
{
    // How to do that?
}

bool wxDataViewColumn::IsHidden() const
{
    return true;
}


void wxDataViewColumn::SetAsSortKey(bool WXUNUSED(sort))
{
    // see wxGTK native wxDataViewColumn implementation
    wxFAIL_MSG( "not implemented" );
}

void wxDataViewColumn::SetNativeData(wxDataViewColumnNativeData* newNativeDataPtr)
{
  delete m_NativeDataPtr;
  m_NativeDataPtr = newNativeDataPtr;
}

#endif // wxUSE_DATAVIEWCTRL && !wxUSE_GENERICDATAVIEWCTRL
