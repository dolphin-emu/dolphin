///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/dataview.mm
// Purpose:     wxDataView
// Author:
// Modified by:
// Created:     2009-01-31
// RCS-ID:      $Id: dataview.mm$
// Copyright:
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if (wxUSE_DATAVIEWCTRL == 1) && !defined(wxUSE_GENERICDATAVIEWCTRL)

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/toplevel.h"
    #include "wx/font.h"
    #include "wx/settings.h"
    #include "wx/utils.h"
#endif

#include "wx/osx/private.h"
#include "wx/osx/cocoa/dataview.h"
#include "wx/renderer.h"
#include "wx/stopwatch.h"

// ============================================================================
// Constants used locally
// ============================================================================

#define DataViewPboardType @"OutlineViewItem"

// ============================================================================
// Classes used locally in dataview.mm
// ============================================================================

// ============================================================================
// wxPointerObject
// ============================================================================

@implementation wxPointerObject

-(id) init
{
    self = [super init];
    if (self != nil)
        self->pointer = NULL;
    return self;
}

-(id) initWithPointer:(void*) initPointer
{
    self = [super init];
    if (self != nil)
        self->pointer = initPointer;
    return self;
}

//
// inherited methods from NSObject
//
-(BOOL) isEqual:(id)object
{
    return (object != nil) &&
             ([object isKindOfClass:[wxPointerObject class]]) &&
                 (pointer == [((wxPointerObject*) object) pointer]);
}

-(NSUInteger) hash
{
    return (NSUInteger) pointer;
}

-(void*) pointer
{
    return pointer;
}

-(void) setPointer:(void*) newPointer
{
    pointer = newPointer;
}

@end

namespace
{

inline wxDataViewItem wxDataViewItemFromItem(id item)
{
    return wxDataViewItem([static_cast<wxPointerObject *>(item) pointer]);
}

inline wxDataViewItem wxDataViewItemFromMaybeNilItem(id item)
{
    return item == nil ? wxDataViewItem() : wxDataViewItemFromItem(item);
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// wxCustomRendererObject
// ----------------------------------------------------------------------------

@interface wxCustomRendererObject : NSObject <NSCopying>
{
@public
    wxDataViewCustomRenderer* customRenderer; // not owned by the class
}

    -(id) init;
    -(id) initWithRenderer:(wxDataViewCustomRenderer*)renderer;
@end

@implementation wxCustomRendererObject

-(id) init
{
    self = [super init];
    if (self != nil)
    {
        customRenderer = NULL;
    }
    return self;
}

-(id) initWithRenderer:(wxDataViewCustomRenderer*)renderer
{
    self = [super init];
    if (self != nil)
    {
        customRenderer = renderer;
    }
    return self;
}

-(id) copyWithZone:(NSZone*)zone
{
    wxCustomRendererObject* copy;

    copy = [[[self class] allocWithZone:zone] init];
    copy->customRenderer = customRenderer;

    return copy;
}
@end

// ----------------------------------------------------------------------------
// wxDVCNSTableColumn: exists only to override NSTableColumn:dataCellForRow:
// ----------------------------------------------------------------------------

@interface wxDVCNSTableColumn : NSTableColumn
{
}

    -(id) dataCellForRow:(NSInteger)row;
@end

@implementation wxDVCNSTableColumn

-(id) dataCellForRow:(NSInteger)row
{
    // what we want to do here is to simply return nil for the cells which
    // shouldn't show anything as otherwise we would show e.g. empty combo box
    // or progress cells in the columns using the corresponding types even for
    // the container rows which is wrong

    // half of the problem is just finding the objects we need from the column
    // pointer which is itself stashed inside wxPointerObject which we use as
    // our identifier
    const wxDataViewColumn * const
        dvCol = static_cast<wxDataViewColumn *>(
                    [(wxPointerObject *)[self identifier] pointer]
                );

    const wxDataViewCtrl * const dvc = dvCol->GetOwner();
    const wxCocoaDataViewControl * const
        peer = static_cast<wxCocoaDataViewControl *>(dvc->GetPeer());


    // once we do have everything, simply ask NSOutlineView for the item...
    const id item = peer->GetItemAtRow(row);
    if ( item )
    {
        // ... and if it succeeded, ask the model whether it has any value
        wxDataViewItem dvItem(wxDataViewItemFromItem(item));

        if ( !dvc->GetModel()->HasValue(dvItem, dvCol->GetModelColumn()) )
            return nil;
    }

    return [super dataCellForRow:row];
}

@end

// ============================================================================
// local helpers
// ============================================================================

namespace
{

// convert from NSObject to different C++ types: all these functions check
// that the conversion really makes sense and assert if it doesn't
wxString ObjectToString(NSObject *object)
{
    wxCHECK_MSG( [object isKindOfClass:[NSString class]], "",
                 wxString::Format
                 (
                    "string expected but got %s",
                    wxCFStringRef::AsString([object className])
                 ));

    return wxCFStringRef([((NSString*) object) retain]).AsString();
}

bool ObjectToBool(NSObject *object)
{
    // actually the value must be of NSCFBoolean class but it's private so we
    // can't check for it directly
    wxCHECK_MSG( [object isKindOfClass:[NSNumber class]], false,
                 wxString::Format
                 (
                    "number expected but got %s",
                    wxCFStringRef::AsString([object className])
                 ));

    return [(NSNumber *)object boolValue];
}

long ObjectToLong(NSObject *object)
{
    wxCHECK_MSG( [object isKindOfClass:[NSNumber class]], -1,
                 wxString::Format
                 (
                    "number expected but got %s",
                    wxCFStringRef::AsString([object className])
                 ));

    return [(NSNumber *)object longValue];
}

wxDateTime ObjectToDate(NSObject *object)
{
    wxCHECK_MSG( [object isKindOfClass:[NSDate class]], wxInvalidDateTime,
                 wxString::Format
                 (
                    "date expected but got %s",
                    wxCFStringRef::AsString([object className])
                 ));

    // get the number of seconds since 1970-01-01 UTC and this is the only
    // way to convert a double to a wxLongLong
    const wxLongLong seconds = [((NSDate*) object) timeIntervalSince1970];

    wxDateTime dt(1, wxDateTime::Jan, 1970);
    dt.Add(wxTimeSpan(0,0,seconds));

    // the user has entered a date in the local timezone but seconds
    // contains the number of seconds from date in the local timezone
    // since 1970-01-01 UTC; therefore, the timezone information has to be
    // transferred to wxWidgets, too:
    dt.MakeFromTimezone(wxDateTime::UTC);

    return dt;
}

NSInteger CompareItems(id item1, id item2, void* context)
{
    NSArray* const sortDescriptors = (NSArray*) context;

    NSUInteger const count = [sortDescriptors count];

    NSInteger result = NSOrderedSame;
    for ( NSUInteger i = 0; i < count && result == NSOrderedSame; ++i )
    {
        wxSortDescriptorObject* const
            sortDescriptor = (wxSortDescriptorObject*)
                [sortDescriptors objectAtIndex:i];

        int rc = [sortDescriptor modelPtr]->Compare
                 (
                     wxDataViewItemFromItem(item1),
                     wxDataViewItemFromItem(item2),
                     [sortDescriptor columnPtr]->GetModelColumn(),
                     [sortDescriptor ascending] == YES
                 );

        if ( rc < 0 )
            result = NSOrderedAscending;
        else if ( rc > 0 )
            result = NSOrderedDescending;
    }

    return result;
}

NSTextAlignment ConvertToNativeHorizontalTextAlignment(int alignment)
{
    if (alignment & wxALIGN_CENTER_HORIZONTAL)
        return NSCenterTextAlignment;
    else if (alignment & wxALIGN_RIGHT)
        return NSRightTextAlignment;
    else
        return NSLeftTextAlignment;
}

NSTableColumn* CreateNativeColumn(const wxDataViewColumn *column)
{
    wxDataViewRenderer * const renderer = column->GetRenderer();

    wxCHECK_MSG( renderer, NULL, "column should have a renderer" );

    wxDVCNSTableColumn * const nativeColumn(
        [[wxDVCNSTableColumn alloc] initWithIdentifier:
            [[[wxPointerObject alloc] initWithPointer:
                const_cast<wxDataViewColumn*>(column)]
             autorelease]]
    );

    // setting the size related parameters:
    int resizingMask;
    if (column->IsResizeable())
    {
        resizingMask = NSTableColumnUserResizingMask;
        [nativeColumn setMinWidth:column->GetMinWidth()];
        [nativeColumn setMaxWidth:column->GetMaxWidth()];
    }
    else // column is not resizeable [by user]
    {
        // if the control doesn't show a header, make the columns resize
        // automatically, this is particularly important for the single column
        // controls (such as wxDataViewTreeCtrl) as their unique column should
        // always take up all the available splace
        resizingMask = column->GetOwner()->HasFlag(wxDV_NO_HEADER)
                            ? NSTableColumnAutoresizingMask
                            : NSTableColumnNoResizing;
    }
    [nativeColumn setResizingMask:resizingMask];

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5
    // setting the visibility:
    [nativeColumn setHidden:static_cast<BOOL>(column->IsHidden())];
#endif

    wxDataViewRendererNativeData * const renderData = renderer->GetNativeData();

    // setting the header:
    [[nativeColumn headerCell] setAlignment:
        ConvertToNativeHorizontalTextAlignment(column->GetAlignment())];
    [[nativeColumn headerCell] setStringValue:
        [[wxCFStringRef(column->GetTitle()).AsNSString() retain] autorelease]];
    renderData->ApplyLineBreakMode([nativeColumn headerCell]);

    // setting data cell's properties:
    [[nativeColumn dataCell] setWraps:NO];
    // setting the default data cell:
    [nativeColumn setDataCell:renderData->GetColumnCell()];
    // setting the editablility:
    const bool isEditable = renderer->GetMode() == wxDATAVIEW_CELL_EDITABLE;

    [nativeColumn setEditable:isEditable];
    [[nativeColumn dataCell] setEditable:isEditable];

    return nativeColumn;
}

} // anonymous namespace

// ============================================================================
// Public helper functions for dataview implementation on OSX
// ============================================================================

wxWidgetImplType* CreateDataView(wxWindowMac* wxpeer,
                                 wxWindowMac* WXUNUSED(parent),
                                 wxWindowID WXUNUSED(id),
                                 const wxPoint& pos,
                                 const wxSize& size,
                                 long style,
                                 long WXUNUSED(extraStyle))
{
    return new wxCocoaDataViewControl(wxpeer,pos,size,style);
}

// ============================================================================
// wxSortDescriptorObject
// ============================================================================

@implementation wxSortDescriptorObject
-(id) init
{
    self = [super init];
    if (self != nil)
    {
        columnPtr = NULL;
        modelPtr  = NULL;
    }
    return self;
}

-(id)
initWithModelPtr:(wxDataViewModel*)initModelPtr
    sortingColumnPtr:(wxDataViewColumn*)initColumnPtr
    ascending:(BOOL)sortAscending
{
    self = [super initWithKey:@"dummy" ascending:sortAscending];
    if (self != nil)
    {
        columnPtr = initColumnPtr;
        modelPtr  = initModelPtr;
    }
    return self;
}

-(id) copyWithZone:(NSZone*)zone
{
    wxSortDescriptorObject* copy;


    copy = [super copyWithZone:zone];
    copy->columnPtr = columnPtr;
    copy->modelPtr  = modelPtr;

    return copy;
}

//
// access to model column's index
//
-(wxDataViewColumn*) columnPtr
{
    return columnPtr;
}

-(wxDataViewModel*) modelPtr
{
    return modelPtr;
}

-(void) setColumnPtr:(wxDataViewColumn*)newColumnPtr
{
    columnPtr = newColumnPtr;
}

-(void) setModelPtr:(wxDataViewModel*)newModelPtr
{
    modelPtr = newModelPtr;
}

@end

// ============================================================================
// wxCocoaOutlineDataSource
// ============================================================================
@implementation wxCocoaOutlineDataSource

//
// constructors / destructor
//
-(id) init
{
    self = [super init];
    if (self != nil)
    {
        implementation = NULL;
        model          = NULL;

        currentParentItem = nil;

        children = [[NSMutableArray alloc] init];
        items    = [[NSMutableSet   alloc] init];
    }
    return self;
}

-(void) dealloc
{
    [currentParentItem release];

    [children release];
    [items    release];

    [super dealloc];
}

//
// methods of informal protocol:
//
-(BOOL)
outlineView:(NSOutlineView*)outlineView
    acceptDrop:(id<NSDraggingInfo>)info
    item:(id)item childIndex:(NSInteger)index
{
    wxUnusedVar(outlineView);
    wxUnusedVar(index);
    
    NSArray* supportedTypes(
        [NSArray arrayWithObjects:DataViewPboardType,NSStringPboardType,nil]
    );

    NSPasteboard* pasteboard([info draggingPasteboard]);

    NSString* bestType([pasteboard availableTypeFromArray:supportedTypes]);

    if ( bestType == nil )
        return FALSE;

    wxDataViewCtrl * const dvc(implementation->GetDataViewCtrl());

    wxCHECK_MSG( dvc, false,
                     "Pointer to data view control not set correctly." );
    wxCHECK_MSG( dvc->GetModel(), false,
                    "Pointer to model not set correctly." );

    wxDataViewEvent event(wxEVT_COMMAND_DATAVIEW_ITEM_DROP, dvc->GetId());
    event.SetEventObject(dvc);
    event.SetItem(wxDataViewItemFromItem(item));
    event.SetModel(dvc->GetModel());

    BOOL dragSuccessful;
    if ( [bestType compare:DataViewPboardType] == NSOrderedSame )
    {
        NSArray* dataArray((NSArray*)
                      [pasteboard propertyListForType:DataViewPboardType]);
        NSUInteger indexDraggedItem, noOfDraggedItems([dataArray count]);

        indexDraggedItem = 0;
        while (indexDraggedItem < noOfDraggedItems)
        {
            wxDataObjectComposite* dataObjects(
                implementation->GetDnDDataObjects((NSData*)
                    [dataArray objectAtIndex:indexDraggedItem]));

            if (dataObjects && (dataObjects->GetFormatCount() > 0))
            {
                wxMemoryBuffer buffer;

                // copy data into data object:
                event.SetDataObject(dataObjects);
                event.SetDataFormat(
                    implementation->GetDnDDataFormat(dataObjects));
                // copy data into buffer:
                dataObjects->GetDataHere(
                    event.GetDataFormat().GetType(),
                    buffer.GetWriteBuf(event.GetDataSize()));
                buffer.UngetWriteBuf(event.GetDataSize());
                event.SetDataBuffer(buffer.GetData());
                // finally, send event:
                if (dvc->HandleWindowEvent(event) && event.IsAllowed())
                {
                    dragSuccessful = true;
                    ++indexDraggedItem;
                }
                else
                {
                    dragSuccessful   = true;
                    indexDraggedItem = noOfDraggedItems; // stop loop
                }
            }
            else
            {
                dragSuccessful   = false;
                indexDraggedItem = noOfDraggedItems; // stop loop
            }
            // clean-up:
            delete dataObjects;
        }
    }
    else
    {
        // needed to convert internally used UTF-16 representation to a UTF-8
        // representation
        CFDataRef              osxData;
        wxDataObjectComposite* dataObjects   (new wxDataObjectComposite());
        wxTextDataObject*      textDataObject(new wxTextDataObject());

        osxData = ::CFStringCreateExternalRepresentation
                    (
                     kCFAllocatorDefault,
                     (CFStringRef)[pasteboard stringForType:NSStringPboardType],
                     kCFStringEncodingUTF8,
                     32
                    );
        if (textDataObject->SetData(::CFDataGetLength(osxData),
                                    ::CFDataGetBytePtr(osxData)))
            dataObjects->Add(textDataObject);
        else
            delete textDataObject;
        // send event if data could be copied:
        if (dataObjects->GetFormatCount() > 0)
        {
            event.SetDataObject(dataObjects);
            event.SetDataFormat(implementation->GetDnDDataFormat(dataObjects));
            if (dvc->HandleWindowEvent(event) && event.IsAllowed())
                dragSuccessful = true;
            else
                dragSuccessful = false;
        }
        else
            dragSuccessful = false;
        // clean up:
        ::CFRelease(osxData);
        delete dataObjects;
    }
    return dragSuccessful;
}

-(id) outlineView:(NSOutlineView*)outlineView
    child:(NSInteger)index
    ofItem:(id)item
{
    wxUnusedVar(outlineView);

    if ((item == currentParentItem) &&
            (index < ((NSInteger) [self getChildCount])))
        return [self getChild:index];

    wxDataViewItemArray dataViewChildren;

    wxCHECK_MSG( model, 0, "Valid model in data source does not exist." );
    model->GetChildren(wxDataViewItemFromMaybeNilItem(item), dataViewChildren);
    [self bufferItem:item withChildren:&dataViewChildren];
    if ([sortDescriptors count] > 0)
        [children sortUsingFunction:CompareItems context:sortDescriptors];
    return [self getChild:index];
}

-(BOOL) outlineView:(NSOutlineView*)outlineView isItemExpandable:(id)item
{
    wxUnusedVar(outlineView);

    wxCHECK_MSG( model, 0, "Valid model in data source does not exist." );
    return model->IsContainer(wxDataViewItemFromItem(item));
}

-(NSInteger) outlineView:(NSOutlineView*)outlineView numberOfChildrenOfItem:(id)item
{
    wxUnusedVar(outlineView);

    NSInteger noOfChildren;

    wxDataViewItemArray dataViewChildren;


    wxCHECK_MSG( model, 0, "Valid model in data source does not exist." );
    noOfChildren = model->GetChildren(wxDataViewItemFromMaybeNilItem(item),
                                      dataViewChildren);
    [self bufferItem:item withChildren:&dataViewChildren];
    if ([sortDescriptors count] > 0)
        [children sortUsingFunction:CompareItems context:sortDescriptors];
    return noOfChildren;
}

-(id)
outlineView:(NSOutlineView*)outlineView
    objectValueForTableColumn:(NSTableColumn*)tableColumn
    byItem:(id)item
{
    wxUnusedVar(outlineView);

    wxCHECK_MSG( model, nil, "Valid model in data source does not exist." );

    wxDataViewColumn* col(static_cast<wxDataViewColumn*>([[tableColumn identifier] pointer]));
    const unsigned colIdx = col->GetModelColumn();

    wxDataViewItem dataViewItem(wxDataViewItemFromItem(item));

    if ( model->HasValue(dataViewItem, colIdx) )
    {
        wxVariant value;
        model->GetValue(value,dataViewItem, colIdx);
        col->GetRenderer()->SetValue(value);
    }

    return nil;
}

-(void)
outlineView:(NSOutlineView*)outlineView
    setObjectValue:(id)object
    forTableColumn:(NSTableColumn*)tableColumn
    byItem:(id)item
{
    wxUnusedVar(outlineView);

    wxDataViewColumn* col(static_cast<wxDataViewColumn*>([[tableColumn identifier] pointer]));

    col->GetRenderer()->
        OSXOnCellChanged(object, wxDataViewItemFromItem(item), col->GetModelColumn());
}

-(void) outlineView:(NSOutlineView*)outlineView sortDescriptorsDidChange:(NSArray*)oldDescriptors
{
    wxUnusedVar(oldDescriptors);
    
    // Warning: the new sort descriptors are guaranteed to be only of type
    // NSSortDescriptor! Therefore, the sort descriptors for the data source
    // have to be converted.
    NSArray* newDescriptors;

    NSMutableArray* wxSortDescriptors;

    NSUInteger noOfDescriptors;

    wxDataViewCtrl* const dvc = implementation->GetDataViewCtrl();


    // convert NSSortDescriptors to wxSortDescriptorObjects:
    newDescriptors    = [outlineView sortDescriptors];
    noOfDescriptors   = [newDescriptors count];
    wxSortDescriptors = [NSMutableArray arrayWithCapacity:noOfDescriptors];
    for (NSUInteger i=0; i<noOfDescriptors; ++i)
    {
        NSSortDescriptor* const newDescriptor = [newDescriptors objectAtIndex:i];

        [wxSortDescriptors addObject:[[[wxSortDescriptorObject alloc] initWithModelPtr:model
            sortingColumnPtr:dvc->GetColumn([[newDescriptor key] intValue])
            ascending:[newDescriptor ascending]] autorelease]];
    }
    [(wxCocoaOutlineDataSource*)[outlineView dataSource] setSortDescriptors:wxSortDescriptors];

    // send first the event to wxWidgets that the sorting has changed so that
    // the program can do special actions before the sorting actually starts:
    wxDataViewEvent event(wxEVT_COMMAND_DATAVIEW_COLUMN_SORTED,dvc->GetId()); // variable defintion

    event.SetEventObject(dvc);
    if (noOfDescriptors > 0)
    {
        wxDataViewColumn* const col = [[wxSortDescriptors objectAtIndex:0] columnPtr];

        event.SetColumn(dvc->GetColumnPosition(col));
        event.SetDataViewColumn(col);
    }
    dvc->GetEventHandler()->ProcessEvent(event);

    // start re-ordering the data;
    // children's buffer must be cleared first because it contains the old order:
    [self clearChildren];
    // sorting is done while reloading the data:
    [outlineView reloadData];
}

-(NSDragOperation) outlineView:(NSOutlineView*)outlineView validateDrop:(id<NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(NSInteger)index
{
    wxUnusedVar(outlineView);
    wxUnusedVar(index);

    NSArray* supportedTypes([NSArray arrayWithObjects:DataViewPboardType,NSStringPboardType,nil]);

    NSPasteboard* pasteboard([info draggingPasteboard]);

    NSString* bestType([pasteboard availableTypeFromArray:supportedTypes]);
    if (bestType == nil)
        return NSDragOperationNone;

    NSDragOperation dragOperation;
    wxDataViewCtrl* const dvc(implementation->GetDataViewCtrl());

    wxCHECK_MSG(dvc, false, "Pointer to data view control not set correctly.");
    wxCHECK_MSG(dvc->GetModel(), false, "Pointer to model not set correctly.");

    wxDataViewEvent
        event(wxEVT_COMMAND_DATAVIEW_ITEM_DROP_POSSIBLE,dvc->GetId());

    event.SetEventObject(dvc);
    event.SetItem(wxDataViewItemFromItem(item));
    event.SetModel(dvc->GetModel());
    if ([bestType compare:DataViewPboardType] == NSOrderedSame)
    {
        NSArray*               dataArray((NSArray*)[pasteboard propertyListForType:DataViewPboardType]);
        NSUInteger             indexDraggedItem, noOfDraggedItems([dataArray count]);

        indexDraggedItem = 0;
        while (indexDraggedItem < noOfDraggedItems)
        {
            wxDataObjectComposite* dataObjects(implementation->GetDnDDataObjects((NSData*)[dataArray objectAtIndex:indexDraggedItem]));

            if (dataObjects && (dataObjects->GetFormatCount() > 0))
            {
                wxMemoryBuffer buffer;

                // copy data into data object:
                event.SetDataObject(dataObjects);
                event.SetDataFormat(implementation->GetDnDDataFormat(dataObjects));
                // copy data into buffer:
                dataObjects->GetDataHere(event.GetDataFormat().GetType(),buffer.GetWriteBuf(event.GetDataSize()));
                buffer.UngetWriteBuf(event.GetDataSize());
                event.SetDataBuffer(buffer.GetData());
                // finally, send event:
                if (dvc->HandleWindowEvent(event) && event.IsAllowed())
                {
                    dragOperation = NSDragOperationEvery;
                    ++indexDraggedItem;
                }
                else
                {
                    dragOperation    = NSDragOperationNone;
                    indexDraggedItem = noOfDraggedItems; // stop loop
                }
            }
            else
            {
                dragOperation    = NSDragOperationNone;
                indexDraggedItem = noOfDraggedItems; // stop loop
            }
            // clean-up:
            delete dataObjects;
        }
    }
    else
    {
        // needed to convert internally used UTF-16 representation to a UTF-8
        // representation
        CFDataRef              osxData;
        wxDataObjectComposite* dataObjects   (new wxDataObjectComposite());
        wxTextDataObject*      textDataObject(new wxTextDataObject());

        osxData = ::CFStringCreateExternalRepresentation(kCFAllocatorDefault,(CFStringRef)[pasteboard stringForType:NSStringPboardType],kCFStringEncodingUTF8,32);
        if (textDataObject->SetData(::CFDataGetLength(osxData),::CFDataGetBytePtr(osxData)))
            dataObjects->Add(textDataObject);
        else
            delete textDataObject;
        // send event if data could be copied:
        if (dataObjects->GetFormatCount() > 0)
        {
            event.SetDataObject(dataObjects);
            event.SetDataFormat(implementation->GetDnDDataFormat(dataObjects));
            if (dvc->HandleWindowEvent(event) && event.IsAllowed())
                dragOperation = NSDragOperationEvery;
            else
                dragOperation = NSDragOperationNone;
        }
        else
            dragOperation = NSDragOperationNone;
        // clean up:
        ::CFRelease(osxData);
        delete dataObjects;
    }

    return dragOperation;
}

-(BOOL) outlineView:(NSOutlineView*)outlineView writeItems:(NSArray*)writeItems toPasteboard:(NSPasteboard*)pasteboard
{
    wxUnusedVar(outlineView);

    // the pasteboard will be filled up with an array containing the data as
    // returned by the events (including the data type) and a concatenation of
    // text (string) data; the text data will only be put onto the pasteboard
    // if for all items a string representation exists
    wxDataViewCtrl* const dvc = implementation->GetDataViewCtrl();

    wxDataViewItemArray dataViewItems;


    wxCHECK_MSG(dvc, false,"Pointer to data view control not set correctly.");
    wxCHECK_MSG(dvc->GetModel(),false,"Pointer to model not set correctly.");

    if ([writeItems count] > 0)
    {
        bool            dataStringAvailable(true); // a flag indicating if for all items a data string is available
        NSMutableArray* dataArray = [[NSMutableArray arrayWithCapacity:[writeItems count]] retain]; // data of all items
        wxString        dataString; // contains the string data of all items

        // send a begin drag event for all selected items and proceed with
        // dragging unless the event is vetoed:
        wxDataViewEvent
            event(wxEVT_COMMAND_DATAVIEW_ITEM_BEGIN_DRAG,dvc->GetId());

        event.SetEventObject(dvc);
        event.SetModel(dvc->GetModel());
        for (size_t itemCounter=0; itemCounter<[writeItems count]; ++itemCounter)
        {
            bool                   itemStringAvailable(false);              // a flag indicating if for the current item a string is available
            wxDataObjectComposite* itemObject(new wxDataObjectComposite()); // data object for current item
            wxString               itemString;                              // contains the TAB concatenated data of an item

            event.SetItem(
                wxDataViewItemFromItem([writeItems objectAtIndex:itemCounter]));
            itemString = ::ConcatenateDataViewItemValues(dvc,event.GetItem());
            itemObject->Add(new wxTextDataObject(itemString));
            event.SetDataObject(itemObject);
            // check if event has not been vetoed:
            if (dvc->HandleWindowEvent(event) && event.IsAllowed() && (event.GetDataObject()->GetFormatCount() > 0))
            {
                size_t const noOfFormats = event.GetDataObject()->GetFormatCount();
                wxDataFormat* dataFormats(new wxDataFormat[noOfFormats]);

                event.GetDataObject()->GetAllFormats(dataFormats,wxDataObject::Get);
                for (size_t formatCounter=0; formatCounter<noOfFormats; ++formatCounter)
                {
                    // constant definitions for abbreviational purposes:
                    wxDataFormatId const idDataFormat = dataFormats[formatCounter].GetType();
                    size_t const dataSize       = event.GetDataObject()->GetDataSize(idDataFormat);
                    size_t const dataBufferSize = sizeof(wxDataFormatId)+dataSize;
                    // variable definitions (used in all case statements):
                    wxMemoryBuffer dataBuffer(dataBufferSize);

                    dataBuffer.AppendData(&idDataFormat,sizeof(wxDataFormatId));
                    switch (idDataFormat)
                    {
                        case wxDF_TEXT:
                            // otherwise wxDF_UNICODETEXT already filled up
                            // the string; and the UNICODE representation has
                            // priority
                            if (!itemStringAvailable)
                            {
                                event.GetDataObject()->GetDataHere(wxDF_TEXT,dataBuffer.GetAppendBuf(dataSize));
                                dataBuffer.UngetAppendBuf(dataSize);
                                [dataArray addObject:[NSData dataWithBytes:dataBuffer.GetData() length:dataBufferSize]];
                                itemString = wxString(static_cast<char const*>(dataBuffer.GetData())+sizeof(wxDataFormatId),wxConvLocal);
                                itemStringAvailable = true;
                            }
                            break;
                        case wxDF_UNICODETEXT:
                            {
                                event.GetDataObject()->GetDataHere(wxDF_UNICODETEXT,dataBuffer.GetAppendBuf(dataSize));
                                dataBuffer.UngetAppendBuf(dataSize);
                                if (itemStringAvailable) // does an object already exist as an ASCII text (see wxDF_TEXT case statement)?
                                    [dataArray replaceObjectAtIndex:itemCounter withObject:[NSData dataWithBytes:dataBuffer.GetData() length:dataBufferSize]];
                                else
                                    [dataArray addObject:[NSData dataWithBytes:dataBuffer.GetData() length:dataBufferSize]];
                                itemString = wxString::FromUTF8(static_cast<char const*>(dataBuffer.GetData())+sizeof(wxDataFormatId),dataSize);
                                itemStringAvailable = true;
                            } /* block */
                            break;
                        default:
                            wxFAIL_MSG("Data object has invalid or unsupported data format");
                            [dataArray release];
                            return NO;
                    }
                }
                delete[] dataFormats;
                delete itemObject;
                if (dataStringAvailable)
                    if (itemStringAvailable)
                    {
                        if (itemCounter > 0)
                            dataString << wxT('\n');
                        dataString << itemString;
                    }
                    else
                        dataStringAvailable = false;
            }
            else
            {
                [dataArray release];
                delete itemObject;
                return NO; // dragging was vetoed or no data available
            }
        }
        if (dataStringAvailable)
        {
            wxCFStringRef osxString(dataString);

            [pasteboard declareTypes:[NSArray arrayWithObjects:DataViewPboardType,NSStringPboardType,nil] owner:nil];
            [pasteboard setPropertyList:dataArray forType:DataViewPboardType];
            [pasteboard setString:osxString.AsNSString() forType:NSStringPboardType];
        }
        else
        {
            [pasteboard declareTypes:[NSArray arrayWithObject:DataViewPboardType] owner:nil];
            [pasteboard setPropertyList:dataArray forType:DataViewPboardType];
        }
        return YES;
    }
    else
        return NO; // no items to drag (should never occur)
}

//
// buffer handling
//
-(void) addToBuffer:(wxPointerObject*)item
{
    [items addObject:item];
}

-(void) clearBuffer
{
    [items removeAllObjects];
}

-(wxPointerObject*) getDataViewItemFromBuffer:(const wxDataViewItem&)item
{
    return [items member:[[[wxPointerObject alloc] initWithPointer:item.GetID()] autorelease]];
}

-(wxPointerObject*) getItemFromBuffer:(wxPointerObject*)item
{
    return [items member:item];
}

-(BOOL) isInBuffer:(wxPointerObject*)item
{
    return [items containsObject:item];
}

-(void) removeFromBuffer:(wxPointerObject*)item
{
    [items removeObject:item];
}

//
// children handling
//
-(void) appendChild:(wxPointerObject*)item
{
    [children addObject:item];
}

-(void) clearChildren
{
    [children removeAllObjects];
}

-(wxPointerObject*) getChild:(NSUInteger)index
{
    return [children objectAtIndex:index];
}

-(NSUInteger) getChildCount
{
    return [children count];
}

-(void) removeChild:(NSUInteger)index
{
    [children removeObjectAtIndex:index];
}

//
// buffer handling
//
-(void) clearBuffers
{
    [self clearBuffer];
    [self clearChildren];
    [self setCurrentParentItem:nil];
}

//
// sorting
//
-(NSArray*) sortDescriptors
{
    return sortDescriptors;
}

-(void) setSortDescriptors:(NSArray*)newSortDescriptors
{
    [newSortDescriptors retain];
    [sortDescriptors release];
    sortDescriptors = newSortDescriptors;
}

//
// access to wxWidget's implementation
//
-(wxPointerObject*) currentParentItem
{
    return currentParentItem;
}

-(wxCocoaDataViewControl*) implementation
{
    return implementation;
}

-(wxDataViewModel*) model
{
    return model;
}

-(void) setCurrentParentItem:(wxPointerObject*)newCurrentParentItem
{
    [newCurrentParentItem retain];
    [currentParentItem release];
    currentParentItem = newCurrentParentItem;
}

-(void) setImplementation:(wxCocoaDataViewControl*) newImplementation
{
    implementation = newImplementation;
}

-(void) setModel:(wxDataViewModel*) newModel
{
    model = newModel;
}

//
// other methods
//
-(void) bufferItem:(wxPointerObject*)parentItem withChildren:(wxDataViewItemArray*)dataViewChildrenPtr
{
    NSInteger const noOfChildren = (*dataViewChildrenPtr).GetCount();

    [self setCurrentParentItem:parentItem];
    [self clearChildren];
    for (NSInteger indexChild=0; indexChild<noOfChildren; ++indexChild)
    {
        wxPointerObject* bufferedPointerObject;
        wxPointerObject* newPointerObject([[wxPointerObject alloc] initWithPointer:(*dataViewChildrenPtr)[indexChild].GetID()]);

        // The next statement and test looks strange but there is
        // unfortunately no workaround: due to the fact that two pointer
        // objects are identical if their pointers are identical - because the
        // method isEqual has been overloaded - the set operation will only
        // add a new pointer object if there is not already one in the set
        // having the same pointer. On the other side the children's array
        // would always add the new pointer object. This means that different
        // pointer objects are stored in the set and array. This will finally
        // lead to a crash as objects diverge. To solve this issue it is first
        // tested if the child already exists in the set and if it is the case
        // the sets object is going to be appended to the array, otheriwse the
        // new pointer object is added to the set and array:
        bufferedPointerObject = [self getItemFromBuffer:newPointerObject];
        if (bufferedPointerObject == nil)
        {
            [items    addObject:newPointerObject];
            [children addObject:newPointerObject];
        }
        else
            [children addObject:bufferedPointerObject];
        [newPointerObject release];
    }
}

@end

// ============================================================================
// wxCustomCell
// ============================================================================

@implementation wxCustomCell

-(NSSize) cellSize
{
    wxCustomRendererObject * const
        obj = static_cast<wxCustomRendererObject *>([self objectValue]);


    const wxSize size = obj->customRenderer->GetSize();
    return NSMakeSize(size.x, size.y);
}

//
// implementations
//
-(void) drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView
{
    wxCustomRendererObject * const
        obj = static_cast<wxCustomRendererObject *>([self objectValue]);
    if ( !obj )
    {
        // this may happen for the custom cells in container rows: they don't
        // have any values
        return;
    }

    wxDataViewCustomRenderer * const renderer = obj->customRenderer;

    wxDC * const dc = renderer->GetDC();
    renderer->WXCallRender(wxFromNSRect(controlView, cellFrame), dc, 0);
    renderer->SetDC(NULL);
}

-(NSRect) imageRectForBounds:(NSRect)cellFrame
{
    return cellFrame;
}

-(NSRect) titleRectForBounds:(NSRect)cellFrame
{
    return cellFrame;
}

@end

// ============================================================================
// wxImageTextCell
// ============================================================================
@implementation wxImageTextCell
//
// initialization
//
-(id) init
{
    self = [super init];
    if (self != nil)
    {
        // initializing the text part:
        [self setSelectable:YES];
        // initializing the image part:
        image       = nil;
        imageSize   = NSMakeSize(16,16);
        spaceImageText = 5.0;
        xImageShift    = 5.0;
    }
    return self;
}

-(id) copyWithZone:(NSZone*)zone
{
    wxImageTextCell* cell;


    cell = (wxImageTextCell*) [super copyWithZone:zone];
    cell->image          = [image retain];
    cell->imageSize      = imageSize;
    cell->spaceImageText = spaceImageText;
    cell->xImageShift    = xImageShift;

    return cell;
}

-(void) dealloc
{
    [image release];

    [super dealloc];
}

//
// alignment
//
-(NSTextAlignment) alignment
{
    return cellAlignment;
}

-(void) setAlignment:(NSTextAlignment)newAlignment
{
    cellAlignment = newAlignment;
    switch (newAlignment)
    {
        case NSCenterTextAlignment:
        case NSLeftTextAlignment:
        case NSJustifiedTextAlignment:
        case NSNaturalTextAlignment:
            [super setAlignment:NSLeftTextAlignment];
            break;
        case NSRightTextAlignment:
            [super setAlignment:NSRightTextAlignment];
            break;
        default:
            wxFAIL_MSG("Unknown alignment type.");
    }
}

//
// image access
//
-(NSImage*) image
{
    return image;
}

-(void) setImage:(NSImage*)newImage
{
    [newImage retain];
    [image release];
    image = newImage;
}

-(NSSize) imageSize
{
    return imageSize;
}

-(void) setImageSize:(NSSize) newImageSize
{
    imageSize = newImageSize;
}

//
// other methods
//
-(NSSize) cellImageSize
{
    return NSMakeSize(imageSize.width+xImageShift+spaceImageText,imageSize.height);
}

-(NSSize) cellSize
{
    NSSize cellSize([super cellSize]);


    if (imageSize.height > cellSize.height)
        cellSize.height = imageSize.height;
    cellSize.width += imageSize.width+xImageShift+spaceImageText;

    return cellSize;
}

-(NSSize) cellTextSize
{
    return [super cellSize];
}

//
// implementations
//
-(void) determineCellParts:(NSRect)cellFrame imagePart:(NSRect*)imageFrame textPart:(NSRect*)textFrame
{
    switch (cellAlignment)
    {
        case NSCenterTextAlignment:
            {
                CGFloat const cellSpace = cellFrame.size.width-[self cellSize].width;

                // if the cell's frame is smaller than its contents (at least
                // in x-direction) make sure that the image is visible:
                if (cellSpace <= 0)
                    NSDivideRect(cellFrame,imageFrame,textFrame,xImageShift+imageSize.width+spaceImageText,NSMinXEdge);
                else // otherwise center the image and text in the cell's frame
                    NSDivideRect(cellFrame,imageFrame,textFrame,xImageShift+imageSize.width+spaceImageText+0.5*cellSpace,NSMinXEdge);
            }
            break;
        case NSJustifiedTextAlignment:
        case NSLeftTextAlignment:
        case NSNaturalTextAlignment: // how to determine the natural writing direction? TODO
            NSDivideRect(cellFrame,imageFrame,textFrame,xImageShift+imageSize.width+spaceImageText,NSMinXEdge);
            break;
        case NSRightTextAlignment:
            {
                CGFloat const cellSpace = cellFrame.size.width-[self cellSize].width;

                // if the cell's frame is smaller than its contents (at least
                // in x-direction) make sure that the image is visible:
                if (cellSpace <= 0)
                    NSDivideRect(cellFrame,imageFrame,textFrame,xImageShift+imageSize.width+spaceImageText,NSMinXEdge);
                else // otherwise right align the image and text in the cell's frame
                    NSDivideRect(cellFrame,imageFrame,textFrame,xImageShift+imageSize.width+spaceImageText+cellSpace,NSMinXEdge);
            }
            break;
        default:
            *imageFrame = NSZeroRect;
            *textFrame  = NSZeroRect;
            wxFAIL_MSG("Unhandled alignment type.");
    }
}

-(void) drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView
{
    NSRect textFrame, imageFrame;


    [self determineCellParts:cellFrame imagePart:&imageFrame textPart:&textFrame];
    // draw the image part by ourselves;
    // check if the cell has to draw its own background (checking is done by
    // the parameter of the textfield's cell):
    if ([self drawsBackground])
    {
        [[self backgroundColor] set];
        NSRectFill(imageFrame);
    }
    if (image != nil)
    {
        // the image is slightly shifted (xImageShift) and has a fixed size
        // but the image's frame might be larger and starts currently on the
        // left side of the cell's frame; therefore, the origin and the
        // image's frame size have to be adjusted:
        if (imageFrame.size.width >= xImageShift+imageSize.width+spaceImageText)
        {
            imageFrame.origin.x += imageFrame.size.width-imageSize.width-spaceImageText;
            imageFrame.size.width = imageSize.width;
        }
        else
        {
            imageFrame.origin.x   += xImageShift;
            imageFrame.size.width -= xImageShift+spaceImageText;
        }
        // ...and the image has to be centered in the y-direction:
        if (imageFrame.size.height > imageSize.height)
            imageFrame.size.height = imageSize.height;
        imageFrame.origin.y += ceil(0.5*(cellFrame.size.height-imageFrame.size.height));

        // according to the documentation the coordinate system should be
        // flipped for NSTableViews (y-coordinate goes from top to bottom); to
        // draw an image correctly the coordinate system has to be transformed
        // to a bottom-top coordinate system, otherwise the image's
        // content is flipped:
        NSAffineTransform* coordinateTransform([NSAffineTransform transform]);

        if ([controlView isFlipped])
        {
            [coordinateTransform scaleXBy: 1.0 yBy:-1.0]; // first the coordinate system is brought back to bottom-top orientation
            [coordinateTransform translateXBy:0.0 yBy:(-2.0)*imageFrame.origin.y-imageFrame.size.height]; // the coordinate system has to be moved to compensate for the
            [coordinateTransform concat];                                                                 // other orientation and the position of the image's frame
        }
        [image drawInRect:imageFrame fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0]; // suggested method to draw the image
        // instead of compositeToPoint:operation:
        // take back previous transformation (if the view is not flipped the
        // coordinate transformation matrix contains the identity matrix and
        // the next two operations do not change the content's transformation
        // matrix):
        [coordinateTransform invert];
        [coordinateTransform concat];
    }
    // let the textfield cell draw the text part:
    if (textFrame.size.width > [self cellTextSize].width)
    {
        // for unknown reasons the alignment of the text cell is ignored;
        // therefore change the size so that alignment does not influence the
        // visualization anymore
        textFrame.size.width = [self cellTextSize].width;
    }
    [super drawWithFrame:textFrame inView:controlView];
}

-(void) editWithFrame:(NSRect)aRect inView:(NSView*)controlView editor:(NSText*)textObj delegate:(id)anObject event:(NSEvent*)theEvent
{
    NSRect textFrame, imageFrame;


    [self determineCellParts:aRect imagePart:&imageFrame textPart:&textFrame];
    [super editWithFrame:textFrame inView:controlView editor:textObj delegate:anObject event:theEvent];
}

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5
-(NSUInteger) hitTestForEvent:(NSEvent*)event inRect:(NSRect)cellFrame ofView:(NSView*)controlView
{
    NSPoint point = [controlView convertPoint:[event locationInWindow] fromView:nil];

    NSRect imageFrame, textFrame;


    [self determineCellParts:cellFrame imagePart:&imageFrame textPart:&textFrame];
    if (image != nil)
    {
        // the image is shifted...
        if (imageFrame.size.width >= xImageShift+imageSize.width+spaceImageText)
        {
            imageFrame.origin.x += imageFrame.size.width-imageSize.width-spaceImageText;
            imageFrame.size.width = imageSize.width;
        }
        else
        {
            imageFrame.origin.x   += xImageShift;
            imageFrame.size.width -= xImageShift+spaceImageText;
        }
        // ...and centered:
        if (imageFrame.size.height > imageSize.height)
            imageFrame.size.height = imageSize.height;
        imageFrame.origin.y += ceil(0.5*(cellFrame.size.height-imageFrame.size.height));
        // If the point is in the image rect, then it is a content hit (see
        // documentation for hitTestForEvent:inRect:ofView):
        if (NSMouseInRect(point, imageFrame, [controlView isFlipped]))
            return NSCellHitContentArea;
    }
    // if the image was not hit let's try the text part:
    if (textFrame.size.width > [self cellTextSize].width)
    {
        // for unknown reasons the alignment of the text cell is ignored;
        // therefore change the size so that alignment does not influence the
        // visualization anymore
        textFrame.size.width = [self cellTextSize].width;
    }

    return [super hitTestForEvent:event inRect:textFrame ofView:controlView];
}
#endif

-(NSRect) imageRectForBounds:(NSRect)cellFrame
{
    NSRect textFrame, imageFrame;


    [self determineCellParts:cellFrame imagePart:&imageFrame textPart:&textFrame];
    if (imageFrame.size.width >= xImageShift+imageSize.width+spaceImageText)
    {
        imageFrame.origin.x += imageFrame.size.width-imageSize.width-spaceImageText;
        imageFrame.size.width = imageSize.width;
    }
    else
    {
        imageFrame.origin.x   += xImageShift;
        imageFrame.size.width -= xImageShift+spaceImageText;
    }
    // ...and centered:
    if (imageFrame.size.height > imageSize.height)
        imageFrame.size.height = imageSize.height;
    imageFrame.origin.y += ceil(0.5*(cellFrame.size.height-imageFrame.size.height));

    return imageFrame;
}

-(void) selectWithFrame:(NSRect)aRect inView:(NSView*)controlView editor:(NSText*)textObj delegate:(id)anObject start:(NSInteger)selStart length:(NSInteger)selLength
{
    NSRect textFrame, imageFrame;


    [self determineCellParts:aRect imagePart:&imageFrame textPart:&textFrame];
    [super selectWithFrame:textFrame inView:controlView editor:textObj delegate:anObject start:selStart length:selLength];
}

-(NSRect) titleRectForBounds:(NSRect)cellFrame
{
    NSRect textFrame, imageFrame;


    [self determineCellParts:cellFrame imagePart:&imageFrame textPart:&textFrame];
    return textFrame;
}

@end

// ============================================================================
// wxCocoaOutlineView
// ============================================================================
@implementation wxCocoaOutlineView

//
// initializers / destructor
//
-(id) init
{
    self = [super init];
    if (self != nil)
    {
        currentlyEditedColumn =
            currentlyEditedRow = -1;

        [self registerForDraggedTypes:[NSArray arrayWithObjects:DataViewPboardType,NSStringPboardType,nil]];
        [self setDelegate:self];
        [self setDoubleAction:@selector(actionDoubleClick:)];
        [self setDraggingSourceOperationMask:NSDragOperationEvery forLocal:NO];
        [self setDraggingSourceOperationMask:NSDragOperationEvery forLocal:YES];
        [self setTarget:self];
    }
    return self;
}

//
// access to wxWidget's implementation
//
-(wxCocoaDataViewControl*) implementation
{
    return implementation;
}

-(void) setImplementation:(wxCocoaDataViewControl*) newImplementation
{
    implementation = newImplementation;
}

//
// actions
//
-(void) actionDoubleClick:(id)sender
{
    wxUnusedVar(sender);

    // actually the documentation (NSTableView 2007-10-31) for doubleAction:
    // and setDoubleAction: seems to be wrong as this action message is always
    // sent whether the cell is editable or not
    wxDataViewCtrl* const dvc = implementation->GetDataViewCtrl();

    wxDataViewEvent event(wxEVT_COMMAND_DATAVIEW_ITEM_ACTIVATED,dvc->GetId());


    event.SetEventObject(dvc);
    event.SetItem(wxDataViewItemFromItem([self itemAtRow:[self clickedRow]]));
    dvc->GetEventHandler()->ProcessEvent(event);
}


//
// contextual menus
//
-(NSMenu*) menuForEvent:(NSEvent*)theEvent
{
    wxUnusedVar(theEvent);

    // this method does not do any special menu event handling but only sends
    // an event message; therefore, the user has full control if a context
    // menu should be shown or not
    wxDataViewCtrl* const dvc = implementation->GetDataViewCtrl();

    wxDataViewEvent event(wxEVT_COMMAND_DATAVIEW_ITEM_CONTEXT_MENU,dvc->GetId());

    wxDataViewItemArray selectedItems;


    event.SetEventObject(dvc);
    event.SetModel(dvc->GetModel());
    // get the item information;
    // theoretically more than one ID can be returned but the event can only
    // handle one item, therefore only the first item of the array is
    // returned:
    if (dvc->GetSelections(selectedItems) > 0)
        event.SetItem(selectedItems[0]);
    dvc->GetEventHandler()->ProcessEvent(event);
    // nothing is done:
    return nil;
}

//
// delegate methods
//
-(void) outlineView:(NSOutlineView*)outlineView mouseDownInHeaderOfTableColumn:(NSTableColumn*)tableColumn
{
    wxDataViewColumn* const col(static_cast<wxDataViewColumn*>([[tableColumn identifier] pointer]));

    wxDataViewCtrl* const dvc = implementation->GetDataViewCtrl();

    wxDataViewEvent
        event(wxEVT_COMMAND_DATAVIEW_COLUMN_HEADER_CLICK,dvc->GetId());


    // first, send an event that the user clicked into a column's header:
    event.SetEventObject(dvc);
    event.SetColumn(dvc->GetColumnPosition(col));
    event.SetDataViewColumn(col);
    dvc->HandleWindowEvent(event);

    // now, check if the click may have had an influence on sorting, too;
    // the sorting setup has to be done only if the clicked table column is
    // sortable and has not been used for sorting before the click; if the
    // column is already responsible for sorting the native control changes
    // the sorting direction automatically and informs the data source via
    // outlineView:sortDescriptorsDidChange:
    if (col->IsSortable() && ([tableColumn sortDescriptorPrototype] == nil))
    {
        // remove the sort order from the previously sorted column table (it
        // can also be that no sorted column table exists):
        UInt32 const noOfColumns = [outlineView numberOfColumns];

        for (UInt32 i=0; i<noOfColumns; ++i)
            [[[outlineView tableColumns] objectAtIndex:i] setSortDescriptorPrototype:nil];
        // make column table sortable:
        NSArray*          sortDescriptors;
        NSSortDescriptor* sortDescriptor;

        sortDescriptor = [[NSSortDescriptor alloc] initWithKey:[NSString stringWithFormat:@"%d",[outlineView columnWithIdentifier:[tableColumn identifier]]]
            ascending:YES];
        sortDescriptors = [NSArray arrayWithObject:sortDescriptor];
        [tableColumn setSortDescriptorPrototype:sortDescriptor];
        [outlineView setSortDescriptors:sortDescriptors];
        [sortDescriptor release];
    }
}

-(BOOL) outlineView:(NSOutlineView*)outlineView shouldCollapseItem:(id)item
{
    wxUnusedVar(outlineView);

    wxDataViewCtrl* const dvc = implementation->GetDataViewCtrl();

    wxDataViewEvent event(wxEVT_COMMAND_DATAVIEW_ITEM_COLLAPSING,dvc->GetId());


    event.SetEventObject(dvc);
    event.SetItem       (wxDataViewItemFromItem(item));
    event.SetModel      (dvc->GetModel());
    // finally send the equivalent wxWidget event:
    dvc->GetEventHandler()->ProcessEvent(event);
    // opening the container is allowed if not vetoed:
    return event.IsAllowed();
}

-(BOOL) outlineView:(NSOutlineView*)outlineView shouldExpandItem:(id)item
{
    wxUnusedVar(outlineView);

    wxDataViewCtrl* const dvc = implementation->GetDataViewCtrl();

    wxDataViewEvent event(wxEVT_COMMAND_DATAVIEW_ITEM_EXPANDING,dvc->GetId());


    event.SetEventObject(dvc);
    event.SetItem       (wxDataViewItemFromItem(item));
    event.SetModel      (dvc->GetModel());
    // finally send the equivalent wxWidget event:
    dvc->GetEventHandler()->ProcessEvent(event);
    // opening the container is allowed if not vetoed:
    return event.IsAllowed();
}

-(BOOL) outlineView:(NSOutlineView*)outlineView shouldSelectTableColumn:(NSTableColumn*)tableColumn
{
    wxUnusedVar(tableColumn);
    wxUnusedVar(outlineView);

    return NO;
}

-(void) outlineView:(wxCocoaOutlineView*)outlineView
    willDisplayCell:(id)cell
    forTableColumn:(NSTableColumn*)tableColumn
    item:(id)item
{
    wxUnusedVar(outlineView);

    wxDataViewCtrl * const dvc = implementation->GetDataViewCtrl();
    wxDataViewModel * const model = dvc->GetModel();

    wxDataViewColumn * const
        dvCol(static_cast<wxDataViewColumn*>(
                    [[tableColumn identifier] pointer]
                    )
             );
    const unsigned colIdx = dvCol->GetModelColumn();

    wxDataViewItem dvItem(wxDataViewItemFromItem(item));

    if ( !model->HasValue(dvItem, colIdx) )
        return;

    wxDataViewRenderer * const renderer = dvCol->GetRenderer();
    wxDataViewRendererNativeData * const data = renderer->GetNativeData();

    // let the renderer know about what it's going to render next
    data->SetColumnPtr(tableColumn);
    data->SetItem(item);
    data->SetItemCell(cell);

    // use the attributes: notice that we need to do this whether we have them
    // or not as even if this cell doesn't have any attributes, the previous
    // one might have had some and then we need to reset them back to default
    wxDataViewItemAttr attr;
    model->GetAttr(dvItem, colIdx, attr);
    renderer->OSXApplyAttr(attr);

    // set the state (enabled/disabled) of the item
    renderer->OSXApplyEnabled(model->IsEnabled(dvItem, colIdx));

    // and finally do draw it
    renderer->MacRender();
}

//
// notifications
//
-(void) outlineViewColumnDidMove:(NSNotification*)notification
{
    int const newColumnPosition = [[[notification userInfo] objectForKey:@"NSNewColumn"] intValue];

    wxDataViewColumn* const col(static_cast<wxDataViewColumn*>([[[[self tableColumns] objectAtIndex:newColumnPosition] identifier] pointer]));

    wxDataViewCtrl* const dvc = implementation->GetDataViewCtrl();

    wxDataViewEvent event(wxEVT_COMMAND_DATAVIEW_COLUMN_REORDERED,dvc->GetId());


    event.SetEventObject(dvc);
    event.SetColumn(dvc->GetColumnPosition(col));
    event.SetDataViewColumn(col);
    dvc->GetEventHandler()->ProcessEvent(event);
}

-(void) outlineViewItemDidCollapse:(NSNotification*)notification
{
    wxDataViewCtrl* const dvc = implementation->GetDataViewCtrl();

    wxDataViewEvent event(wxEVT_COMMAND_DATAVIEW_ITEM_COLLAPSED,dvc->GetId());


    event.SetEventObject(dvc);
    event.SetItem(wxDataViewItemFromItem(
                    [[notification userInfo] objectForKey:@"NSObject"]));
    dvc->GetEventHandler()->ProcessEvent(event);
}

-(void) outlineViewItemDidExpand:(NSNotification*)notification
{
    wxDataViewCtrl* const dvc = implementation->GetDataViewCtrl();

    wxDataViewEvent event(wxEVT_COMMAND_DATAVIEW_ITEM_EXPANDED,dvc->GetId());


    event.SetEventObject(dvc);
    event.SetItem(wxDataViewItemFromItem(
                    [[notification userInfo] objectForKey:@"NSObject"]));
    dvc->GetEventHandler()->ProcessEvent(event);
}

-(void) outlineViewSelectionDidChange:(NSNotification*)notification
{
    wxUnusedVar(notification);

    wxDataViewCtrl* const dvc = implementation->GetDataViewCtrl();

    wxDataViewEvent event(wxEVT_COMMAND_DATAVIEW_SELECTION_CHANGED,dvc->GetId());

    event.SetEventObject(dvc);
    event.SetModel(dvc->GetModel());
    event.SetItem(dvc->GetSelection());
    dvc->GetEventHandler()->ProcessEvent(event);
}

-(void) textDidBeginEditing:(NSNotification*)notification
{
    // this notification is only sent if the user started modifying the cell
    // (not when the user clicked into the cell and the cell's editor is
    // called!)

    // call method of superclass (otherwise editing does not work correctly -
    // the outline data source class is not informed about a change of data):
    [super textDidBeginEditing:notification];

    // remember the column being edited, it will be used in textDidEndEditing:
    currentlyEditedColumn = [self editedColumn];
    currentlyEditedRow = [self editedRow];

    wxDataViewColumn* const col =
        static_cast<wxDataViewColumn*>(
                [[[[self tableColumns] objectAtIndex:currentlyEditedColumn] identifier] pointer]);

    wxDataViewCtrl* const dvc = implementation->GetDataViewCtrl();


    // stop editing of a custom item first (if necessary)
    dvc->FinishCustomItemEditing();

    // now, send the event:
    wxDataViewEvent
        event(wxEVT_COMMAND_DATAVIEW_ITEM_EDITING_STARTED,dvc->GetId());

    event.SetEventObject(dvc);
    event.SetItem(
            wxDataViewItemFromItem([self itemAtRow:currentlyEditedRow]));
    event.SetColumn(dvc->GetColumnPosition(col));
    event.SetDataViewColumn(col);
    dvc->GetEventHandler()->ProcessEvent(event);
}

-(void) textDidEndEditing:(NSNotification*)notification
{
    // call method of superclass (otherwise editing does not work correctly -
    // the outline data source class is not informed about a change of data):
    [super textDidEndEditing:notification];

    // under OSX an event indicating the end of an editing session can be sent
    // even if no event indicating a start of an editing session has been sent
    // (see Documentation for NSControl controlTextDidEndEditing:); this is
    // not expected by a user of the wxWidgets library and therefore an
    // wxEVT_COMMAND_DATAVIEW_ITEM_EDITING_DONE event is only sent if a
    // corresponding wxEVT_COMMAND_DATAVIEW_ITEM_EDITING_STARTED has been sent
    // before; to check if a wxEVT_COMMAND_DATAVIEW_ITEM_EDITING_STARTED has
    // been sent the last edited column/row are valid:
    if ( currentlyEditedColumn != -1 && currentlyEditedRow != -1 )
    {
        wxDataViewColumn* const col =
            static_cast<wxDataViewColumn*>(
                    [[[[self tableColumns] objectAtIndex:currentlyEditedColumn] identifier] pointer]);

        wxDataViewCtrl* const dvc = implementation->GetDataViewCtrl();

        // send event to wxWidgets:
        wxDataViewEvent
            event(wxEVT_COMMAND_DATAVIEW_ITEM_EDITING_DONE,dvc->GetId());

        event.SetEventObject(dvc);
        event.SetItem(
                wxDataViewItemFromItem([self itemAtRow:currentlyEditedRow]));
        event.SetColumn(dvc->GetColumnPosition(col));
        event.SetDataViewColumn(col);
        dvc->GetEventHandler()->ProcessEvent(event);


        // we're not editing any more
        currentlyEditedColumn =
            currentlyEditedRow = -1;
    }
}

@end

// ============================================================================
// wxCocoaDataViewControl
// ============================================================================

wxCocoaDataViewControl::wxCocoaDataViewControl(wxWindow* peer,
                                               const wxPoint& pos,
                                               const wxSize& size,
                                               long style)
    : wxWidgetCocoaImpl
      (
        peer,
        [[NSScrollView alloc] initWithFrame:wxOSXGetFrameForControl(peer,pos,size)]
      ),
      m_DataSource(NULL),
      m_OutlineView([[wxCocoaOutlineView alloc] init])
{
    // initialize scrollview (the outline view is part of a scrollview):
    NSScrollView* scrollview = (NSScrollView*) GetWXWidget();

    [scrollview setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [scrollview setBorderType:NSNoBorder];
    [scrollview setHasVerticalScroller:YES];
    [scrollview setHasHorizontalScroller:YES];
    [scrollview setAutohidesScrollers:YES];
    [scrollview setDocumentView:m_OutlineView];

    // initialize the native control itself too
    InitOutlineView(style);
}

void wxCocoaDataViewControl::InitOutlineView(long style)
{
    [m_OutlineView setImplementation:this];
    [m_OutlineView setColumnAutoresizingStyle:NSTableViewSequentialColumnAutoresizingStyle];
    [m_OutlineView setIndentationPerLevel:GetDataViewCtrl()->GetIndent()];
    NSUInteger maskGridStyle(NSTableViewGridNone);
    if (style & wxDV_HORIZ_RULES)
        maskGridStyle |= NSTableViewSolidHorizontalGridLineMask;
    if (style & wxDV_VERT_RULES)
        maskGridStyle |= NSTableViewSolidVerticalGridLineMask;
    [m_OutlineView setGridStyleMask:maskGridStyle];
    [m_OutlineView setAllowsMultipleSelection:           (style & wxDV_MULTIPLE)  != 0];
    [m_OutlineView setUsesAlternatingRowBackgroundColors:(style & wxDV_ROW_LINES) != 0];

    if ( style & wxDV_NO_HEADER )
        [m_OutlineView setHeaderView:nil];
}

wxCocoaDataViewControl::~wxCocoaDataViewControl()
{
    [m_DataSource  release];
    [m_OutlineView release];
}

//
// column related methods (inherited from wxDataViewWidgetImpl)
//
bool wxCocoaDataViewControl::ClearColumns()
{
    // as there is a bug in NSOutlineView version (OSX 10.5.6 #6555162) the
    // columns cannot be deleted if there is an outline column in the view;
    // therefore, the whole view is deleted and newly constructed:
    [m_OutlineView release];
    m_OutlineView = [[wxCocoaOutlineView alloc] init];
    [((NSScrollView*) GetWXWidget()) setDocumentView:m_OutlineView];
    [m_OutlineView setDataSource:m_DataSource];

    InitOutlineView(GetDataViewCtrl()->GetWindowStyle());

    return true;
}

bool wxCocoaDataViewControl::DeleteColumn(wxDataViewColumn* columnPtr)
{
    if ([m_OutlineView outlineTableColumn] == columnPtr->GetNativeData()->GetNativeColumnPtr())
        [m_OutlineView setOutlineTableColumn:nil]; // due to a bug this does not work
    [m_OutlineView removeTableColumn:columnPtr->GetNativeData()->GetNativeColumnPtr()]; // due to a confirmed bug #6555162 the deletion does not work for
    // outline table columns (... and there is no workaround)
    return (([m_OutlineView columnWithIdentifier:[[[wxPointerObject alloc] initWithPointer:columnPtr] autorelease]]) == -1);
}

void wxCocoaDataViewControl::DoSetExpanderColumn(const wxDataViewColumn *columnPtr)
{
    [m_OutlineView setOutlineTableColumn:columnPtr->GetNativeData()->GetNativeColumnPtr()];
}

wxDataViewColumn* wxCocoaDataViewControl::GetColumn(unsigned int pos) const
{
    return static_cast<wxDataViewColumn*>([[[[m_OutlineView tableColumns] objectAtIndex:pos] identifier] pointer]);
}

int wxCocoaDataViewControl::GetColumnPosition(const wxDataViewColumn *columnPtr) const
{
    return [m_OutlineView columnWithIdentifier:[[[wxPointerObject alloc] initWithPointer:const_cast<wxDataViewColumn*>(columnPtr)] autorelease]];
}

bool wxCocoaDataViewControl::InsertColumn(unsigned int pos, wxDataViewColumn* columnPtr)
{
    // create column and set the native data of the dataview column:
    NSTableColumn *nativeColumn = ::CreateNativeColumn(columnPtr);
    columnPtr->GetNativeData()->SetNativeColumnPtr(nativeColumn);
    // as the native control does not allow the insertion of a column at a
    // specified position the column is first appended and - if necessary -
    // moved to its final position:
    [m_OutlineView addTableColumn:nativeColumn];
    if (pos != static_cast<unsigned int>([m_OutlineView numberOfColumns]-1))
        [m_OutlineView moveColumn:[m_OutlineView numberOfColumns]-1 toColumn:pos];

    // set columns width now that it can be computed even for autosized columns:
    columnPtr->SetWidth(columnPtr->GetWidthVariable());

    // done:
    return true;
}

void wxCocoaDataViewControl::FitColumnWidthToContent(unsigned int pos)
{
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
    const int count = GetCount();
    NSTableColumn *column = GetColumn(pos)->GetNativeData()->GetNativeColumnPtr();

    class MaxWidthCalculator
    {
    public:
        MaxWidthCalculator(wxCocoaOutlineView *view,
                           NSTableColumn *column, unsigned columnIndex)
            : m_width(0),
              m_view(view),
              m_column(columnIndex),
              m_indent(0)
        {
            // account for indentation in the column with expander
            if ( column == [m_view outlineTableColumn] )
                m_indent = [m_view indentationPerLevel];
        }

        void UpdateWithWidth(int width)
        {
            m_width = wxMax(m_width, width);
        }

        void UpdateWithRow(int row)
        {
            NSCell *cell = [m_view preparedCellAtColumn:m_column row:row];
            unsigned cellWidth = [cell cellSize].width + 1/*round the float up*/;

            if ( m_indent )
                cellWidth += m_indent * ([m_view levelForRow:row] + 1);

            m_width = wxMax(m_width, cellWidth);
        }

        int GetMaxWidth() const { return m_width; }

    private:
        int m_width;
        wxCocoaOutlineView *m_view;
        unsigned m_column;
        int m_indent;
    };

    MaxWidthCalculator calculator(m_OutlineView, column, pos);

    if ( [column headerCell] )
    {
        calculator.UpdateWithWidth([[column headerCell] cellSize].width + 1/*round the float up*/);
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
            calculator.UpdateWithRow(row);

        // finally, include currently visible items in the calculation:
        const NSRange visible = [m_OutlineView rowsInRect:[m_OutlineView visibleRect]];
        const int first_visible = wxMax(visible.location, top_part_end);
        const int last_visible = wxMin(first_visible + visible.length, bottom_part_start);

        for ( row = first_visible; row < last_visible; row++ )
            calculator.UpdateWithRow(row);

        wxLogTrace("dataview",
                   "determined best size from %d top, %d bottom plus %d more visible items out of %d total",
                   top_part_end,
                   count - bottom_part_start,
                   wxMax(0, last_visible - first_visible),
                   count);
    }

    [column setWidth:calculator.GetMaxWidth()];
#endif // MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
}

//
// item related methods (inherited from wxDataViewWidgetImpl)
//
bool wxCocoaDataViewControl::Add(const wxDataViewItem& parent, const wxDataViewItem& WXUNUSED(item))
{
    if (parent.IsOk())
        [m_OutlineView reloadItem:[m_DataSource getDataViewItemFromBuffer:parent] reloadChildren:YES];
    else
        [m_OutlineView reloadData];
    return true;
}

bool wxCocoaDataViewControl::Add(const wxDataViewItem& parent, const wxDataViewItemArray& WXUNUSED(items))
{
    if (parent.IsOk())
        [m_OutlineView reloadItem:[m_DataSource getDataViewItemFromBuffer:parent] reloadChildren:YES];
    else
        [m_OutlineView reloadData];
    return true;
}

void wxCocoaDataViewControl::Collapse(const wxDataViewItem& item)
{
    [m_OutlineView collapseItem:[m_DataSource getDataViewItemFromBuffer:item]];
}

void wxCocoaDataViewControl::EnsureVisible(const wxDataViewItem& item, const wxDataViewColumn *columnPtr)
{
    if (item.IsOk())
    {
        [m_OutlineView scrollRowToVisible:[m_OutlineView rowForItem:[m_DataSource getDataViewItemFromBuffer:item]]];
        if (columnPtr)
            [m_OutlineView scrollColumnToVisible:GetColumnPosition(columnPtr)];
    }
}

void wxCocoaDataViewControl::Expand(const wxDataViewItem& item)
{
    [m_OutlineView expandItem:[m_DataSource getDataViewItemFromBuffer:item]];
}

unsigned int wxCocoaDataViewControl::GetCount() const
{
    return [m_OutlineView numberOfRows];
}

wxRect wxCocoaDataViewControl::GetRectangle(const wxDataViewItem& item, const wxDataViewColumn *columnPtr)
{
    return wxFromNSRect([m_osxView superview],[m_OutlineView frameOfCellAtColumn:GetColumnPosition(columnPtr)
            row:[m_OutlineView rowForItem:[m_DataSource getDataViewItemFromBuffer:item]]]);
}

bool wxCocoaDataViewControl::IsExpanded(const wxDataViewItem& item) const
{
    return [m_OutlineView isItemExpanded:[m_DataSource getDataViewItemFromBuffer:item]];
}

bool wxCocoaDataViewControl::Reload()
{
    [m_DataSource clearBuffers];
    [m_OutlineView scrollColumnToVisible:0];
    [m_OutlineView scrollRowToVisible:0];
    [m_OutlineView reloadData];
    return true;
}

bool wxCocoaDataViewControl::Remove(const wxDataViewItem& parent, const wxDataViewItem& WXUNUSED(item))
{
    if (parent.IsOk())
        [m_OutlineView reloadItem:[m_DataSource getDataViewItemFromBuffer:parent] reloadChildren:YES];
    else
        [m_OutlineView reloadData];
    return true;
}

bool wxCocoaDataViewControl::Remove(const wxDataViewItem& parent, const wxDataViewItemArray& WXUNUSED(item))
{
    if (parent.IsOk())
        [m_OutlineView reloadItem:[m_DataSource getDataViewItemFromBuffer:parent] reloadChildren:YES];
    else
        [m_OutlineView reloadData];
    return true;
}

bool wxCocoaDataViewControl::Update(const wxDataViewColumn *columnPtr)
{
    wxUnusedVar(columnPtr);
    
    return false;
}

bool wxCocoaDataViewControl::Update(const wxDataViewItem& WXUNUSED(parent), const wxDataViewItem& item)
{
    [m_OutlineView reloadItem:[m_DataSource getDataViewItemFromBuffer:item]];
    return true;
}

bool wxCocoaDataViewControl::Update(const wxDataViewItem& WXUNUSED(parent), const wxDataViewItemArray& items)
{
    for (size_t i=0; i<items.GetCount(); ++i)
        [m_OutlineView reloadItem:[m_DataSource getDataViewItemFromBuffer:items[i]]];
    return true;
}

//
// model related methods
//
bool wxCocoaDataViewControl::AssociateModel(wxDataViewModel* model)
{
    [m_DataSource release];
    if (model)
    {
        m_DataSource = [[wxCocoaOutlineDataSource alloc] init];
        [m_DataSource setImplementation:this];
        [m_DataSource setModel:model];
    }
    else
        m_DataSource = NULL;
    [m_OutlineView setDataSource:m_DataSource]; // if there is a data source the data is immediately going to be requested
    return true;
}

//
// selection related methods (inherited from wxDataViewWidgetImpl)
//

wxDataViewItem wxCocoaDataViewControl::GetCurrentItem() const
{
    return wxDataViewItem([[m_OutlineView itemAtRow:[m_OutlineView selectedRow]] pointer]);
}

void wxCocoaDataViewControl::SetCurrentItem(const wxDataViewItem& item)
{
    // We can't have unselected current item in a NSTableView, as the
    // documentation of its deselectRow method explains, the control will
    // automatically change the current item to the closest still selected item
    // if the current item is deselected. So we have no choice but to select
    // the item in the same time as making it current.
    Select(item);
}

int wxCocoaDataViewControl::GetSelections(wxDataViewItemArray& sel) const
{
    NSIndexSet* selectedRowIndexes([m_OutlineView selectedRowIndexes]);

    NSUInteger indexRow;


    sel.Empty();
    sel.Alloc([selectedRowIndexes count]);
    indexRow = [selectedRowIndexes firstIndex];
    while (indexRow != NSNotFound)
    {
        sel.Add(wxDataViewItem([[m_OutlineView itemAtRow:indexRow] pointer]));
        indexRow = [selectedRowIndexes indexGreaterThanIndex:indexRow];
    }
    return sel.GetCount();
}

bool wxCocoaDataViewControl::IsSelected(const wxDataViewItem& item) const
{
    return [m_OutlineView isRowSelected:[m_OutlineView rowForItem:[m_DataSource getDataViewItemFromBuffer:item]]];
}

void wxCocoaDataViewControl::Select(const wxDataViewItem& item)
{
    if (item.IsOk())
        [m_OutlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:[m_OutlineView rowForItem:[m_DataSource getDataViewItemFromBuffer:item]]]
            byExtendingSelection:GetDataViewCtrl()->HasFlag(wxDV_MULTIPLE) ? YES : NO];
}

void wxCocoaDataViewControl::SelectAll()
{
    [m_OutlineView selectAll:m_OutlineView];
}

void wxCocoaDataViewControl::Unselect(const wxDataViewItem& item)
{
    if (item.IsOk())
        [m_OutlineView deselectRow:[m_OutlineView rowForItem:[m_DataSource getDataViewItemFromBuffer:item]]];
}

void wxCocoaDataViewControl::UnselectAll()
{
    [m_OutlineView deselectAll:m_OutlineView];
}

//
// sorting related methods
//
wxDataViewColumn* wxCocoaDataViewControl::GetSortingColumn() const
{
    NSArray* const columns = [m_OutlineView tableColumns];

    UInt32 const noOfColumns = [columns count];


    for (UInt32 i=0; i<noOfColumns; ++i)
        if ([[columns objectAtIndex:i] sortDescriptorPrototype] != nil)
            return static_cast<wxDataViewColumn*>([[[columns objectAtIndex:i] identifier] pointer]);
    return NULL;
}

void wxCocoaDataViewControl::Resort()
{
    [m_DataSource clearChildren];
    [m_OutlineView reloadData];
}

//
// other methods (inherited from wxDataViewWidgetImpl)
//
void wxCocoaDataViewControl::DoSetIndent(int indent)
{
    [m_OutlineView setIndentationPerLevel:static_cast<CGFloat>(indent)];
}

void wxCocoaDataViewControl::HitTest(const wxPoint& point, wxDataViewItem& item, wxDataViewColumn*& columnPtr) const
{
    NSPoint const nativePoint = wxToNSPoint((NSScrollView*) GetWXWidget(),point);

    int indexColumn;
    int indexRow;


    indexColumn = [m_OutlineView columnAtPoint:nativePoint];
    indexRow    = [m_OutlineView rowAtPoint:   nativePoint];
    if ((indexColumn >= 0) && (indexRow >= 0))
    {
        columnPtr = static_cast<wxDataViewColumn*>([[[[m_OutlineView tableColumns] objectAtIndex:indexColumn] identifier] pointer]);
        item      = wxDataViewItem([[m_OutlineView itemAtRow:indexRow] pointer]);
    }
    else
    {
        columnPtr = NULL;
        item      = wxDataViewItem();
    }
}

void wxCocoaDataViewControl::SetRowHeight(const wxDataViewItem& WXUNUSED(item), unsigned int WXUNUSED(height))
    // Not supported by the native control
{
}

void wxCocoaDataViewControl::OnSize()
{
    if ([m_OutlineView numberOfColumns] == 1)
        [m_OutlineView sizeLastColumnToFit];
}

//
// drag & drop helper methods
//
wxDataFormat wxCocoaDataViewControl::GetDnDDataFormat(wxDataObjectComposite* dataObjects)
{
    wxDataFormat resultFormat;
    if ( !dataObjects )
        return resultFormat;

    bool compatible(true);

    size_t const noOfFormats = dataObjects->GetFormatCount();
    size_t       indexFormat;

    wxDataFormat* formats;

    // get all formats and check afterwards if the formats are compatible; if
    // they are compatible the preferred format is returned otherwise
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
                resultFormat.SetType(formats[indexFormat].GetType()); // first format (should only be reached if indexFormat == 0)
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
    }

    delete[] formats;

    return resultFormat;
}

wxDataObjectComposite* wxCocoaDataViewControl::GetDnDDataObjects(NSData* dataObject) const
{
    wxDataFormatId dataFormatID;


    [dataObject getBytes:&dataFormatID length:sizeof(wxDataFormatId)];
    switch (dataFormatID)
    {
        case wxDF_TEXT:
        case wxDF_UNICODETEXT:
            {
                wxTextDataObject* textDataObject(new wxTextDataObject());

                if (textDataObject->SetData(wxDataFormat(dataFormatID),[dataObject length]-sizeof(wxDataFormatId),static_cast<char const*>([dataObject bytes])+sizeof(wxDataFormatId)))
                {
                    wxDataObjectComposite* dataObjectComposite(new wxDataObjectComposite());

                    dataObjectComposite->Add(textDataObject);
                    return dataObjectComposite;
                }
                else
                {
                    delete textDataObject;
                    return NULL;
                }
            }
            break;
        default:
            return NULL;
    }
}

id wxCocoaDataViewControl::GetItemAtRow(int row) const
{
    return [m_OutlineView itemAtRow:row];
}

// ----------------------------------------------------------------------------
// wxDataViewRendererNativeData
// ----------------------------------------------------------------------------

void wxDataViewRendererNativeData::Init()
{
    m_origFont = NULL;
    m_origTextColour = NULL;
    m_ellipsizeMode = wxELLIPSIZE_MIDDLE;

    if ( m_ColumnCell )
        ApplyLineBreakMode(m_ColumnCell);
}

void wxDataViewRendererNativeData::ApplyLineBreakMode(NSCell *cell)
{
    NSLineBreakMode nsMode = NSLineBreakByWordWrapping;
    switch ( m_ellipsizeMode )
    {
        case wxELLIPSIZE_NONE:
            nsMode = NSLineBreakByClipping;
            break;

        case wxELLIPSIZE_START:
            nsMode = NSLineBreakByTruncatingHead;
            break;

        case wxELLIPSIZE_MIDDLE:
            nsMode = NSLineBreakByTruncatingMiddle;
            break;

        case wxELLIPSIZE_END:
            nsMode = NSLineBreakByTruncatingTail;
            break;
    }

    wxASSERT_MSG( nsMode != NSLineBreakByWordWrapping, "unknown wxEllipsizeMode" );

    [cell setLineBreakMode: nsMode];
}

// ---------------------------------------------------------
// wxDataViewRenderer
// ---------------------------------------------------------

wxDataViewRenderer::wxDataViewRenderer(const wxString& varianttype,
                                       wxDataViewCellMode mode,
                                       int align)
    : wxDataViewRendererBase(varianttype, mode, align),
      m_alignment(align),
      m_mode(mode),
      m_NativeDataPtr(NULL)
{
}

wxDataViewRenderer::~wxDataViewRenderer()
{
    delete m_NativeDataPtr;
}

void wxDataViewRenderer::SetAlignment(int align)
{
    m_alignment = align;
    [GetNativeData()->GetColumnCell() setAlignment:ConvertToNativeHorizontalTextAlignment(align)];
}

void wxDataViewRenderer::SetMode(wxDataViewCellMode mode)
{
    m_mode = mode;
    if ( GetOwner() )
        [GetOwner()->GetNativeData()->GetNativeColumnPtr() setEditable:(mode == wxDATAVIEW_CELL_EDITABLE)];
}

void wxDataViewRenderer::SetNativeData(wxDataViewRendererNativeData* newNativeDataPtr)
{
    delete m_NativeDataPtr;
    m_NativeDataPtr = newNativeDataPtr;
}

void wxDataViewRenderer::EnableEllipsize(wxEllipsizeMode mode)
{
    // we need to store this value to apply it to the columns headerCell in
    // CreateNativeColumn()
    GetNativeData()->SetEllipsizeMode(mode);

    // but we may already apply it to the column cell which will be used for
    // this column
    GetNativeData()->ApplyLineBreakMode(GetNativeData()->GetColumnCell());
}

wxEllipsizeMode wxDataViewRenderer::GetEllipsizeMode() const
{
    return GetNativeData()->GetEllipsizeMode();
}

void
wxDataViewRenderer::OSXOnCellChanged(NSObject *object,
                                     const wxDataViewItem& item,
                                     unsigned col)
{
    // TODO: we probably should get rid of this code entirely and make this
    //       function pure virtual, but currently we still have some native
    //       renderers (wxDataViewChoiceRenderer) which don't override it and
    //       there is also wxDataViewCustomRenderer for which it's not obvious
    //       how it should be implemented so keep this "auto-deduction" of
    //       variant type from NSObject for now

    wxVariant value;
    if ( [object isKindOfClass:[NSString class]] )
        value = ObjectToString(object);
    else if ( [object isKindOfClass:[NSNumber class]] )
        value = ObjectToLong(object);
    else if ( [object isKindOfClass:[NSDate class]] )
        value = ObjectToDate(object);
    else
    {
        wxFAIL_MSG( wxString::Format
                    (
                     "unknown value type %s",
                     wxCFStringRef::AsString([object className])
                    ));
        return;
    }

    wxDataViewModel *model = GetOwner()->GetOwner()->GetModel();
    model->ChangeValue(value, item, col);
}

void wxDataViewRenderer::OSXApplyAttr(const wxDataViewItemAttr& attr)
{
    wxDataViewRendererNativeData * const data = GetNativeData();
    NSCell * const cell = data->GetItemCell();

    // set the font and text colour to use: we need to do it if we had ever
    // changed them before, even if this item itself doesn't have any special
    // attributes as otherwise it would reuse the attributes from the previous
    // cell rendered using the same renderer
    NSFont *font = NULL;
    NSColor *colText = NULL;

    if ( attr.HasFont() )
    {
        font = data->GetOriginalFont();
        if ( !font )
        {
            // this is the first time we're setting the font, remember the
            // original one before changing it
            font = [cell font];
            data->SaveOriginalFont(font);
        }

        if ( font )
        {
            // FIXME: using wxFont methods here doesn't work for some reason
            NSFontManager * const fm = [NSFontManager sharedFontManager];
            if ( attr.GetBold() )
                font = [fm convertFont:font toHaveTrait:NSBoldFontMask];
            if ( attr.GetItalic() )
                font = [fm convertFont:font toHaveTrait:NSItalicFontMask];
        }
        //else: can't change font if the cell doesn't have any
    }

    if ( attr.HasColour() )
    {
        // we can set font for any cell but only NSTextFieldCell provides
        // a method for setting text colour so check that this method is
        // available before using it
        if ( [cell respondsToSelector:@selector(setTextColor:)] &&
                [cell respondsToSelector:@selector(textColor)] )
        {
            if ( !data->GetOriginalTextColour() )
            {
                // the cast to (untyped) id is safe because of the check above
                data->SaveOriginalTextColour([(id)cell textColor]);
            }

            const wxColour& c = attr.GetColour();
            colText = [NSColor colorWithDeviceRed:c.Red() / 255.
                green:c.Green() / 255.
                blue:c.Blue() / 255.
                alpha:c.Alpha() / 255.];
        }
    }

    if ( !font )
        font = data->GetOriginalFont();
    if ( !colText )
        colText = data->GetOriginalTextColour();

    if ( font )
        [cell setFont:font];

    if ( colText )
        [(id)cell setTextColor:colText];
}

void wxDataViewRenderer::OSXApplyEnabled(bool enabled)
{
    [GetNativeData()->GetItemCell() setEnabled:enabled];
}

IMPLEMENT_ABSTRACT_CLASS(wxDataViewRenderer,wxDataViewRendererBase)

// ---------------------------------------------------------
// wxDataViewCustomRenderer
// ---------------------------------------------------------
wxDataViewCustomRenderer::wxDataViewCustomRenderer(const wxString& varianttype,
                                                   wxDataViewCellMode mode,
                                                   int align)
    : wxDataViewCustomRendererBase(varianttype, mode, align),
      m_editorCtrlPtr(NULL),
      m_DCPtr(NULL)
{
    SetNativeData(new wxDataViewRendererNativeData([[wxCustomCell alloc] init]));
}

bool wxDataViewCustomRenderer::MacRender()
{
    [GetNativeData()->GetItemCell() setObjectValue:[[[wxCustomRendererObject alloc] initWithRenderer:this] autorelease]];
    return true;
}

void wxDataViewCustomRenderer::OSXApplyAttr(const wxDataViewItemAttr& attr)
{
    // simply save the attribute so that it could be reused from our Render()
    SetAttr(attr);

    // it's not necessary to call the base class version which sets the cell
    // properties to correspond to this attribute because we currently don't
    // use any NSCell methods in custom renderers anyhow but if we ever start
    // doing this (e.g. override RenderText() here to use NSTextFieldCell
    // methods), then we should pass it on to wxDataViewRenderer here
}

IMPLEMENT_ABSTRACT_CLASS(wxDataViewCustomRenderer, wxDataViewRenderer)

// ---------------------------------------------------------
// wxDataViewTextRenderer
// ---------------------------------------------------------
wxDataViewTextRenderer::wxDataViewTextRenderer(const wxString& varianttype,
                                               wxDataViewCellMode mode,
                                               int align)
    : wxDataViewRenderer(varianttype,mode,align)
{
    NSTextFieldCell* cell;


    cell = [[NSTextFieldCell alloc] init];
    [cell setAlignment:ConvertToNativeHorizontalTextAlignment(align)];
    SetNativeData(new wxDataViewRendererNativeData(cell));
    [cell release];
}

bool wxDataViewTextRenderer::MacRender()
{
    if (GetValue().GetType() == GetVariantType())
    {
        [GetNativeData()->GetItemCell() setObjectValue:wxCFStringRef(GetValue().GetString()).AsNSString()];
        return true;
    }
    else
    {
        wxFAIL_MSG(wxString("Text renderer cannot render value because of wrong value type; value type: ") << GetValue().GetType());
        return false;
    }
}

void
wxDataViewTextRenderer::OSXOnCellChanged(NSObject *value,
                                         const wxDataViewItem& item,
                                         unsigned col)
{
    wxDataViewModel *model = GetOwner()->GetOwner()->GetModel();
    model->ChangeValue(ObjectToString(value), item, col);
}

IMPLEMENT_CLASS(wxDataViewTextRenderer,wxDataViewRenderer)

// ---------------------------------------------------------
// wxDataViewBitmapRenderer
// ---------------------------------------------------------
wxDataViewBitmapRenderer::wxDataViewBitmapRenderer(const wxString& varianttype,
                                                   wxDataViewCellMode mode,
                                                   int align)
    : wxDataViewRenderer(varianttype,mode,align)
{
    NSImageCell* cell;


    cell = [[NSImageCell alloc] init];
    SetNativeData(new wxDataViewRendererNativeData(cell));
    [cell release];
}

// This method returns 'true' if
//  - the passed bitmap is valid and it could be assigned to the native data
//  browser;
//  - the passed bitmap is invalid (or is not initialized); this case
//  simulates a non-existing bitmap.
// In all other cases the method returns 'false'.
bool wxDataViewBitmapRenderer::MacRender()
{
    wxCHECK_MSG(GetValue().GetType() == GetVariantType(),false,wxString("Bitmap renderer cannot render value; value type: ") << GetValue().GetType());

    wxBitmap bitmap;

    bitmap << GetValue();
    if (bitmap.IsOk())
        [GetNativeData()->GetItemCell() setObjectValue:[[bitmap.GetNSImage() retain] autorelease]];
    return true;
}

IMPLEMENT_CLASS(wxDataViewBitmapRenderer,wxDataViewRenderer)

// -------------------------------------
// wxDataViewChoiceRenderer
// -------------------------------------
wxDataViewChoiceRenderer::wxDataViewChoiceRenderer(const wxArrayString& choices,
                                                   wxDataViewCellMode mode,
                                                   int alignment)
    : wxDataViewRenderer(wxT("string"), mode, alignment),
      m_choices(choices)
{
    NSPopUpButtonCell* cell;


    cell = [[NSPopUpButtonCell alloc] init];
    [cell setControlSize:NSMiniControlSize];
    [cell setFont:[[NSFont fontWithName:[[cell font] fontName] size:[NSFont systemFontSizeForControlSize:NSMiniControlSize]] autorelease]];
    for (size_t i=0; i<choices.GetCount(); ++i)
        [cell addItemWithTitle:[[wxCFStringRef(choices[i]).AsNSString() retain] autorelease]];
    SetNativeData(new wxDataViewRendererNativeData(cell));
    [cell release];
}

bool wxDataViewChoiceRenderer::MacRender()
{
    if (GetValue().GetType() == GetVariantType())
    {
        [((NSPopUpButtonCell*) GetNativeData()->GetItemCell()) selectItemWithTitle:[[wxCFStringRef(GetValue().GetString()).AsNSString() retain] autorelease]];
        return true;
    }
    else
    {
        wxFAIL_MSG(wxString("Choice renderer cannot render value because of wrong value type; value type: ") << GetValue().GetType());
        return false;
    }
}

IMPLEMENT_CLASS(wxDataViewChoiceRenderer,wxDataViewRenderer)

// ---------------------------------------------------------
// wxDataViewDateRenderer
// ---------------------------------------------------------

wxDataViewDateRenderer::wxDataViewDateRenderer(const wxString& varianttype,
                                               wxDataViewCellMode mode,
                                               int align)
    : wxDataViewRenderer(varianttype,mode,align)
{
    NSTextFieldCell* cell;

    NSDateFormatter* dateFormatter;


    dateFormatter = [[NSDateFormatter alloc] init];
    [dateFormatter setFormatterBehavior:NSDateFormatterBehavior10_4];
    [dateFormatter setDateStyle:NSDateFormatterShortStyle];
    cell = [[NSTextFieldCell alloc] init];
    [cell setFormatter:dateFormatter];
    SetNativeData(new wxDataViewRendererNativeData(cell,[NSDate dateWithString:@"2000-12-30 20:00:00 +0000"]));
    [cell          release];
    [dateFormatter release];
}

bool wxDataViewDateRenderer::MacRender()
{
    if (GetValue().GetType() == GetVariantType())
    {
        if (GetValue().GetDateTime().IsValid())
        {
            // -- find best fitting style to show the date --
            // as the style should be identical for all cells a reference date
            // instead of the actual cell's date value is used for all cells;
            // this reference date is stored in the renderer's native data
            // section for speed purposes; otherwise, the reference date's
            // string has to be recalculated for each item that may become
            // timewise long if a lot of rows using dates exist; the algorithm
            // has the preference to display as much information as possible
            // in the first instance; but as this is often impossible due to
            // space restrictions the style is shortened per loop; finally, if
            // the shortest time and date format does not fit into the cell
            // the time part is dropped; remark: the time part itself is not
            // modified per iteration loop and only uses the short style,
            // means that only the hours and minutes are being shown

            // GetObject() returns a date for testing the size of a date object
            [GetNativeData()->GetItemCell() setObjectValue:GetNativeData()->GetObject()];
            [[GetNativeData()->GetItemCell() formatter] setTimeStyle:NSDateFormatterShortStyle];
            for (int dateFormatterStyle=4; dateFormatterStyle>0; --dateFormatterStyle)
            {
                [[GetNativeData()->GetItemCell() formatter] setDateStyle:(NSDateFormatterStyle)dateFormatterStyle];
                if (dateFormatterStyle == 1)
                {
                    // if the shortest style for displaying the date and time
                    // is too long to be fully visible remove the time part of
                    // the date:
                    if ([GetNativeData()->GetItemCell() cellSize].width > [GetNativeData()->GetColumnPtr() width])
                        [[GetNativeData()->GetItemCell() formatter] setTimeStyle:NSDateFormatterNoStyle];
                    {
                        // basically not necessary as the loop would end anyway
                        // but let's save the last comparison
                        break;
                    }
                }
                else if ([GetNativeData()->GetItemCell() cellSize].width <= [GetNativeData()->GetColumnPtr() width])
                    break;
            }
            // set data (the style is set by the previous loop); on OSX the
            // date has to be specified with respect to UTC; in wxWidgets the
            // date is always entered in the local timezone; so, we have to do
            // a conversion from the local to UTC timezone when adding the
            // seconds to 1970-01-01 UTC:
            [GetNativeData()->GetItemCell() setObjectValue:[NSDate dateWithTimeIntervalSince1970:GetValue().GetDateTime().ToUTC().Subtract(wxDateTime(1,wxDateTime::Jan,1970)).GetSeconds().ToDouble()]];
        }
        return true;
    }
    else
    {
        wxFAIL_MSG(wxString("Date renderer cannot render value because of wrong value type; value type: ") << GetValue().GetType());
        return false;
    }
}

void
wxDataViewDateRenderer::OSXOnCellChanged(NSObject *value,
                                         const wxDataViewItem& item,
                                         unsigned col)
{
    wxDataViewModel *model = GetOwner()->GetOwner()->GetModel();
    model->ChangeValue(ObjectToDate(value), item, col);
}

IMPLEMENT_ABSTRACT_CLASS(wxDataViewDateRenderer,wxDataViewRenderer)

// ---------------------------------------------------------
// wxDataViewIconTextRenderer
// ---------------------------------------------------------
wxDataViewIconTextRenderer::wxDataViewIconTextRenderer(const wxString& varianttype,
                                                       wxDataViewCellMode mode,
                                                       int align)
     : wxDataViewRenderer(varianttype,mode)
{
    wxImageTextCell* cell;


    cell = [[wxImageTextCell alloc] init];
    [cell setAlignment:ConvertToNativeHorizontalTextAlignment(align)];
    SetNativeData(new wxDataViewRendererNativeData(cell));
    [cell release];
}

bool wxDataViewIconTextRenderer::MacRender()
{
    if (GetValue().GetType() == GetVariantType())
    {
        wxDataViewIconText iconText;

        wxImageTextCell* cell;

        cell = (wxImageTextCell*) GetNativeData()->GetItemCell();
        iconText << GetValue();
        if (iconText.GetIcon().IsOk())
            [cell setImage:[[wxBitmap(iconText.GetIcon()).GetNSImage() retain] autorelease]];
        [cell setStringValue:[[wxCFStringRef(iconText.GetText()).AsNSString() retain] autorelease]];
        return true;
    }
    else
    {
        wxFAIL_MSG(wxString("Icon & text renderer cannot render value because of wrong value type; value type: ") << GetValue().GetType());
        return false;
    }
}

void
wxDataViewIconTextRenderer::OSXOnCellChanged(NSObject *value,
                                             const wxDataViewItem& item,
                                             unsigned col)
{
    wxVariant valueIconText;
    valueIconText << wxDataViewIconText(ObjectToString(value));

    wxDataViewModel *model = GetOwner()->GetOwner()->GetModel();
    model->ChangeValue(valueIconText, item, col);
}

IMPLEMENT_ABSTRACT_CLASS(wxDataViewIconTextRenderer,wxDataViewRenderer)

// ---------------------------------------------------------
// wxDataViewToggleRenderer
// ---------------------------------------------------------
wxDataViewToggleRenderer::wxDataViewToggleRenderer(const wxString& varianttype,
                                                   wxDataViewCellMode mode,
                                                   int align)
    : wxDataViewRenderer(varianttype,mode)
{
    NSButtonCell* cell;


    cell = [[NSButtonCell alloc] init];
    [cell setAlignment:ConvertToNativeHorizontalTextAlignment(align)];
    [cell setButtonType:NSSwitchButton];
    [cell setImagePosition:NSImageOnly];
    SetNativeData(new wxDataViewRendererNativeData(cell));
    [cell release];
}

bool wxDataViewToggleRenderer::MacRender()
{
    if (GetValue().GetType() == GetVariantType())
    {
        [GetNativeData()->GetItemCell() setIntValue:GetValue().GetLong()];
        return true;
    }
    else
    {
        wxFAIL_MSG(wxString("Toggle renderer cannot render value because of wrong value type; value type: ") << GetValue().GetType());
        return false;
    }
}

void
wxDataViewToggleRenderer::OSXOnCellChanged(NSObject *value,
                                           const wxDataViewItem& item,
                                           unsigned col)
{
    wxDataViewModel *model = GetOwner()->GetOwner()->GetModel();
    model->ChangeValue(ObjectToBool(value), item, col);
}

IMPLEMENT_ABSTRACT_CLASS(wxDataViewToggleRenderer,wxDataViewRenderer)

// ---------------------------------------------------------
// wxDataViewProgressRenderer
// ---------------------------------------------------------
wxDataViewProgressRenderer::wxDataViewProgressRenderer(const wxString& label,
                                                       const wxString& varianttype,
                                                       wxDataViewCellMode mode,
                                                       int align)
    : wxDataViewRenderer(varianttype,mode,align)
{
    wxUnusedVar(label);
    
    NSLevelIndicatorCell* cell;

    cell = [[NSLevelIndicatorCell alloc] initWithLevelIndicatorStyle:NSContinuousCapacityLevelIndicatorStyle];
    [cell setMinValue:0];
    [cell setMaxValue:100];
    SetNativeData(new wxDataViewRendererNativeData(cell));
    [cell release];
}

bool wxDataViewProgressRenderer::MacRender()
{
    if (GetValue().GetType() == GetVariantType())
    {
        [GetNativeData()->GetItemCell() setIntValue:GetValue().GetLong()];
        return true;
    }
    else
    {
        wxFAIL_MSG(wxString("Progress renderer cannot render value because of wrong value type; value type: ") << GetValue().GetType());
        return false;
    }
}

void
wxDataViewProgressRenderer::OSXOnCellChanged(NSObject *value,
                                             const wxDataViewItem& item,
                                             unsigned col)
{
    wxDataViewModel *model = GetOwner()->GetOwner()->GetModel();
    model->ChangeValue(ObjectToLong(value), item, col);
}

IMPLEMENT_ABSTRACT_CLASS(wxDataViewProgressRenderer,wxDataViewRenderer)

// ---------------------------------------------------------
// wxDataViewColumn
// ---------------------------------------------------------

wxDataViewColumn::wxDataViewColumn(const wxString& title,
                                   wxDataViewRenderer* renderer,
                                   unsigned int model_column,
                                   int width,
                                   wxAlignment align,
                                   int flags)
     : wxDataViewColumnBase(renderer, model_column),
       m_NativeDataPtr(new wxDataViewColumnNativeData()),
       m_title(title)
{
    InitCommon(width, align, flags);
    if (renderer && !renderer->IsCustomRenderer() &&
        (renderer->GetAlignment() == wxDVR_DEFAULT_ALIGNMENT))
        renderer->SetAlignment(align);
}

wxDataViewColumn::wxDataViewColumn(const wxBitmap& bitmap,
                                   wxDataViewRenderer* renderer,
                                   unsigned int model_column,
                                   int width,
                                   wxAlignment align,
                                   int flags)
    : wxDataViewColumnBase(bitmap, renderer, model_column),
      m_NativeDataPtr(new wxDataViewColumnNativeData())
{
    InitCommon(width, align, flags);
    if (renderer && !renderer->IsCustomRenderer() &&
        (renderer->GetAlignment() == wxDVR_DEFAULT_ALIGNMENT))
        renderer->SetAlignment(align);
}

wxDataViewColumn::~wxDataViewColumn()
{
    delete m_NativeDataPtr;
}

int wxDataViewColumn::GetWidth() const
{
    return [m_NativeDataPtr->GetNativeColumnPtr() width];
}

bool wxDataViewColumn::IsSortKey() const
{
    NSTableColumn *nsCol = GetNativeData()->GetNativeColumnPtr();
    return nsCol && ([nsCol sortDescriptorPrototype] != nil);
}

void wxDataViewColumn::SetAlignment(wxAlignment align)
{
    m_alignment = align;
    [[m_NativeDataPtr->GetNativeColumnPtr() headerCell] setAlignment:ConvertToNativeHorizontalTextAlignment(align)];
    if (m_renderer && !m_renderer->IsCustomRenderer() &&
        (m_renderer->GetAlignment() == wxDVR_DEFAULT_ALIGNMENT))
        m_renderer->SetAlignment(align);
}

void wxDataViewColumn::SetBitmap(const wxBitmap& bitmap)
{
    // bitmaps and titles cannot exist at the same time - if the bitmap is set
    // the title is removed:
    m_title = wxEmptyString;
    wxDataViewColumnBase::SetBitmap(bitmap);
    [[m_NativeDataPtr->GetNativeColumnPtr() headerCell] setImage:[[bitmap.GetNSImage() retain] autorelease]];
}

void wxDataViewColumn::SetMaxWidth(int maxWidth)
{
    m_maxWidth = maxWidth;
    [m_NativeDataPtr->GetNativeColumnPtr() setMaxWidth:maxWidth];
}

void wxDataViewColumn::SetMinWidth(int minWidth)
{
    m_minWidth = minWidth;
    [m_NativeDataPtr->GetNativeColumnPtr() setMinWidth:minWidth];
}

void wxDataViewColumn::SetReorderable(bool reorderable)
{
    wxUnusedVar(reorderable);
}

void wxDataViewColumn::SetHidden(bool hidden)
{
    // How to set flag here?

    [m_NativeDataPtr->GetNativeColumnPtr() setHidden:hidden];
}

bool wxDataViewColumn::IsHidden() const
{
    return [m_NativeDataPtr->GetNativeColumnPtr() isHidden];
}

void wxDataViewColumn::SetResizeable(bool resizeable)
{
    wxDataViewColumnBase::SetResizeable(resizeable);
    if (resizeable)
        [m_NativeDataPtr->GetNativeColumnPtr() setResizingMask:NSTableColumnUserResizingMask];
    else
        [m_NativeDataPtr->GetNativeColumnPtr() setResizingMask:NSTableColumnNoResizing];
}

void wxDataViewColumn::SetSortable(bool sortable)
{
    // wxDataViewColumnBase::SetSortable(sortable);
    // Avoid endless recursion and just set the flag here
    if (sortable)
        m_flags |= wxDATAVIEW_COL_SORTABLE;
    else
        m_flags &= ~wxDATAVIEW_COL_SORTABLE;
}

void wxDataViewColumn::SetSortOrder(bool ascending)
{
    if (m_ascending != ascending)
    {
        m_ascending = ascending;
        if (IsSortKey())
        {
            // change sorting order:
            NSArray*          sortDescriptors;
            NSSortDescriptor* sortDescriptor;
            NSTableColumn*    tableColumn;

            tableColumn     = m_NativeDataPtr->GetNativeColumnPtr();
            sortDescriptor  = [[NSSortDescriptor alloc] initWithKey:[[tableColumn sortDescriptorPrototype] key] ascending:m_ascending];
            sortDescriptors = [NSArray arrayWithObject:sortDescriptor];
            [tableColumn setSortDescriptorPrototype:sortDescriptor];
            [[tableColumn tableView] setSortDescriptors:sortDescriptors];
            [sortDescriptor release];
        }
    }
}

void wxDataViewColumn::SetTitle(const wxString& title)
{
    // bitmaps and titles cannot exist at the same time - if the title is set
    // the bitmap is removed:
    wxDataViewColumnBase::SetBitmap(wxBitmap());
    m_title = title;
    [[m_NativeDataPtr->GetNativeColumnPtr() headerCell] setStringValue:[[wxCFStringRef(title).AsNSString() retain] autorelease]];
}

void wxDataViewColumn::SetWidth(int width)
{
    m_width = width;

    switch ( width )
    {
        case wxCOL_WIDTH_AUTOSIZE:
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
            if ( GetOwner() )
            {
                wxCocoaDataViewControl *peer = static_cast<wxCocoaDataViewControl*>(GetOwner()->GetPeer());
                peer->FitColumnWidthToContent(GetOwner()->GetColumnPosition(this));
                break;
            }
#endif
            // fall through if unsupported (OSX < 10.5) or not yet settable

        case wxCOL_WIDTH_DEFAULT:
            width = wxDVC_DEFAULT_WIDTH;
            // fall through

        default:
            [m_NativeDataPtr->GetNativeColumnPtr() setWidth:width];
            break;
    }
}

void wxDataViewColumn::SetAsSortKey(bool WXUNUSED(sort))
{
    // see wxGTK native wxDataViewColumn implementation
    wxFAIL_MSG("not implemented");
}

void wxDataViewColumn::SetNativeData(wxDataViewColumnNativeData* newNativeDataPtr)
{
    delete m_NativeDataPtr;
    m_NativeDataPtr = newNativeDataPtr;
}

#endif // (wxUSE_DATAVIEWCTRL == 1) && !defined(wxUSE_GENERICDATAVIEWCTRL)
