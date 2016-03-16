///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/listbox.mm
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
#include "wx/dnd.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/utils.h"
    #include "wx/settings.h"
    #include "wx/arrstr.h"
    #include "wx/dcclient.h"
#endif

#include "wx/osx/private.h"

// forward decls

class wxListWidgetCocoaImpl;

@interface wxNSTableDataSource : NSObject <NSTableViewDataSource>
{
    wxListWidgetCocoaImpl* impl;
}

- (id)tableView:(NSTableView *)aTableView
        objectValueForTableColumn:(NSTableColumn *)aTableColumn
        row:(NSInteger)rowIndex;

- (void)tableView:(NSTableView *)aTableView
        setObjectValue:(id)value forTableColumn:(NSTableColumn *)aTableColumn
        row:(NSInteger)rowIndex;

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView;

- (void)setImplementation: (wxListWidgetCocoaImpl *) theImplementation;
- (wxListWidgetCocoaImpl*) implementation;

@end

@interface wxNSTableView : NSTableView <NSTableViewDelegate>
{
}

@end

//
// table column
//

class wxCocoaTableColumn;

@interface wxNSTableColumn : NSTableColumn
{
    wxCocoaTableColumn* column;
}

- (void) setColumn: (wxCocoaTableColumn*) col;

- (wxCocoaTableColumn*) column;

@end

class WXDLLIMPEXP_CORE wxCocoaTableColumn : public wxListWidgetColumn
{
public :
    wxCocoaTableColumn( wxNSTableColumn* column, bool editable )
        : m_column( column ), m_editable(editable)
    {
    }

    ~wxCocoaTableColumn()
    {
    }

    wxNSTableColumn* GetNSTableColumn() const { return m_column ; }

    bool IsEditable() const { return m_editable; }

protected :
    wxNSTableColumn* m_column;
    bool m_editable;
} ;

NSString* column1 = @"1";

class wxListWidgetCocoaImpl : public wxWidgetCocoaImpl, public wxListWidgetImpl
{
public :
    wxListWidgetCocoaImpl( wxWindowMac* peer, NSScrollView* view, wxNSTableView* tableview, wxNSTableDataSource* data );

    ~wxListWidgetCocoaImpl();

    virtual wxListWidgetColumn*     InsertTextColumn( unsigned pos, const wxString& title, bool editable = false,
                                wxAlignment just = wxALIGN_LEFT , int defaultWidth = -1) wxOVERRIDE  ;
    virtual wxListWidgetColumn*     InsertCheckColumn( unsigned pos , const wxString& title, bool editable = false,
                                wxAlignment just = wxALIGN_LEFT , int defaultWidth =  -1) wxOVERRIDE  ;

    // add and remove

    virtual void            ListDelete( unsigned int n ) wxOVERRIDE ;
    virtual void            ListInsert( unsigned int n ) wxOVERRIDE ;
    virtual void            ListClear() wxOVERRIDE ;

    // selecting

    virtual void            ListDeselectAll() wxOVERRIDE;

    virtual void            ListSetSelection( unsigned int n, bool select, bool multi ) wxOVERRIDE ;
    virtual int             ListGetSelection() const wxOVERRIDE ;

    virtual int             ListGetSelections( wxArrayInt& aSelections ) const wxOVERRIDE ;

    virtual bool            ListIsSelected( unsigned int n ) const wxOVERRIDE ;

    // display

    virtual void            ListScrollTo( unsigned int n ) wxOVERRIDE ;

    virtual int             ListGetTopItem() const wxOVERRIDE;

    // accessing content

    virtual unsigned int    ListGetCount() const wxOVERRIDE ;
    virtual int             DoListHitTest( const wxPoint& inpoint ) const wxOVERRIDE;

    int                     ListGetColumnType( int col )
    {
        return col;
    }
    virtual void            UpdateLine( unsigned int n, wxListWidgetColumn* col = NULL ) wxOVERRIDE ;
    virtual void            UpdateLineToEnd( unsigned int n) wxOVERRIDE;

    virtual void            controlDoubleAction(WXWidget slf, void* _cmd, void *sender) wxOVERRIDE;

    
protected :
    wxNSTableView*          m_tableView ;

    wxNSTableDataSource*    m_dataSource;
} ;

//
// implementations
//

@implementation wxNSTableColumn

- (id) init
{
    self = [super init];
    column = nil;
    return self;
}

- (void) setColumn: (wxCocoaTableColumn*) col
{
    column = col;
}

- (wxCocoaTableColumn*) column
{
    return column;
}

@end

class wxNSTableViewCellValue : public wxListWidgetCellValue
{
public :
    wxNSTableViewCellValue( id &v ) : value(v)
    {
    }

    virtual ~wxNSTableViewCellValue() {}

    virtual void Set( CFStringRef v ) wxOVERRIDE
    {
        value = [[(NSString*)v retain] autorelease];
    }
    virtual void Set( const wxString& value ) wxOVERRIDE
    {
        Set( (CFStringRef) wxCFStringRef( value ) );
    }
    virtual void Set( int v ) wxOVERRIDE
    {
        value = [NSNumber numberWithInt:v];
    }

    virtual int GetIntValue() const wxOVERRIDE
    {
        if ( [value isKindOfClass:[NSNumber class]] )
            return [ (NSNumber*) value intValue ];

        return 0;
    }

    virtual wxString GetStringValue() const wxOVERRIDE
    {
        if ( [value isKindOfClass:[NSString class]] )
            return wxCFStringRef::AsString( (NSString*) value );

        return wxEmptyString;
    }

protected:
    id& value;
} ;

@implementation wxNSTableDataSource

- (id) init
{
    self = [super init];
    impl = nil;
    return self;
}

- (void)setImplementation: (wxListWidgetCocoaImpl *) theImplementation
{
    impl = theImplementation;
}

- (wxListWidgetCocoaImpl*) implementation
{
    return impl;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView
{
    wxUnusedVar(aTableView);
    if ( impl )
        return impl->ListGetCount();
    return 0;
}

- (id)tableView:(NSTableView *)aTableView
        objectValueForTableColumn:(NSTableColumn *)aTableColumn
        row:(NSInteger)rowIndex
{
    wxUnusedVar(aTableView);
    wxNSTableColumn* tablecol = (wxNSTableColumn *)aTableColumn;
    wxListBox* lb = dynamic_cast<wxListBox*>(impl->GetWXPeer());
    wxCocoaTableColumn* col = [tablecol column];
    id value = nil;
    wxNSTableViewCellValue cellvalue(value);
    lb->GetValueCallback(rowIndex, col, cellvalue);
    return value;
}

- (void)tableView:(NSTableView *)aTableView
        setObjectValue:(id)value forTableColumn:(NSTableColumn *)aTableColumn
        row:(NSInteger)rowIndex
{
    wxUnusedVar(aTableView);
    wxNSTableColumn* tablecol = (wxNSTableColumn *)aTableColumn;
    wxListBox* lb = dynamic_cast<wxListBox*>(impl->GetWXPeer());
    wxCocoaTableColumn* col = [tablecol column];
    wxNSTableViewCellValue cellvalue(value);
    lb->SetValueCallback(rowIndex, col, cellvalue);
}

@end

@implementation wxNSTableView

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
    }
}

- (void) tableViewSelectionDidChange: (NSNotification *) notification
{
    wxUnusedVar(notification);
    
    int row = [self selectedRow];
    
    if (row == -1) 
    {
        // no row selected
    } 
    else 
    {
        wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
        wxListBox *list = static_cast<wxListBox*> ( impl->GetWXPeer());
        wxCHECK_RET( list != NULL , wxT("Listbox expected"));
        
        if ((row < 0) || (row > (int) list->GetCount()))  // OS X can select an item below the last item
            return;
        
        if ( !list->MacGetBlockEvents() )
            list->HandleLineEvent( row, false );
    }
    
} 

- (void)setFont:(NSFont *)aFont
{
    NSArray *tableColumns = [self tableColumns];
    unsigned int columnIndex = [tableColumns count];
    while (columnIndex--)
        [[(NSTableColumn *)[tableColumns objectAtIndex:columnIndex] dataCell] setFont:aFont];

    [self setRowHeight:[gNSLayoutManager defaultLineHeightForFont:aFont]+2];
}

- (void) setControlSize:(NSControlSize) size
{
    NSArray *tableColumns = [self tableColumns];
    unsigned int columnIndex = [tableColumns count];
    while (columnIndex--)
        [[(NSTableColumn *)[tableColumns objectAtIndex:columnIndex] dataCell] setControlSize:size];
}

@end

//
//
//

wxListWidgetCocoaImpl::wxListWidgetCocoaImpl( wxWindowMac* peer, NSScrollView* view, wxNSTableView* tableview, wxNSTableDataSource* data ) :
    wxWidgetCocoaImpl( peer, view ), m_tableView(tableview), m_dataSource(data)
{
    InstallEventHandler( tableview );
}

wxListWidgetCocoaImpl::~wxListWidgetCocoaImpl()
{
    [m_dataSource release];
}

unsigned int wxListWidgetCocoaImpl::ListGetCount() const
{
    wxListBox* lb = dynamic_cast<wxListBox*> ( GetWXPeer() );
    return lb->GetCount();
}

//
// columns
//

wxListWidgetColumn* wxListWidgetCocoaImpl::InsertTextColumn( unsigned pos, const wxString& WXUNUSED(title), bool editable,
                                wxAlignment WXUNUSED(just), int defaultWidth)
{
    wxNSTableColumn* col1 = [[wxNSTableColumn alloc] init];
    [col1 setEditable:editable];

    unsigned formerColCount = [m_tableView numberOfColumns];

    // there's apparently no way to insert at a specific position
    [m_tableView addTableColumn:col1 ];
    if ( pos < formerColCount )
        [m_tableView moveColumn:formerColCount toColumn:pos];

    if ( defaultWidth >= 0 )
    {
        [col1 setMaxWidth:defaultWidth];
        [col1 setMinWidth:defaultWidth];
        [col1 setWidth:defaultWidth];
    }
    else
    {
        [col1 setMaxWidth:1000];
        [col1 setMinWidth:10];
        // temporary hack, because I cannot get the automatic column resizing
        // to work properly
        [col1 setWidth:1000];
    }
    [col1 setResizingMask: NSTableColumnAutoresizingMask];
    
    wxListBox *list = static_cast<wxListBox*> ( GetWXPeer());
    if ( list != NULL )
        [[col1 dataCell] setFont:list->GetFont().OSXGetNSFont()];
    
    wxCocoaTableColumn* wxcol = new wxCocoaTableColumn( col1, editable );
    [col1 setColumn:wxcol];

    // owned by the tableview
    [col1 release];
    return wxcol;
}

wxListWidgetColumn* wxListWidgetCocoaImpl::InsertCheckColumn( unsigned pos , const wxString& WXUNUSED(title), bool editable,
                                wxAlignment WXUNUSED(just), int defaultWidth )
{
   wxNSTableColumn* col1 = [[wxNSTableColumn alloc] init];
    [col1 setEditable:editable];

    // set your custom cell & set it up
    NSButtonCell* checkbox = [[NSButtonCell alloc] init];
    [checkbox setTitle:@""];
    [checkbox setButtonType:NSSwitchButton];
    [col1 setDataCell:checkbox] ;
    
    wxListBox *list = static_cast<wxListBox*> ( GetWXPeer());
    if ( list != NULL )
    {
        NSControlSize size = NSRegularControlSize;
        
        switch ( list->GetWindowVariant() )
        {
            case wxWINDOW_VARIANT_NORMAL :
                size = NSRegularControlSize;
                break ;
                
            case wxWINDOW_VARIANT_SMALL :
                size = NSSmallControlSize;
                break ;
                
            case wxWINDOW_VARIANT_MINI :
                size = NSMiniControlSize;
                break ;
                
            case wxWINDOW_VARIANT_LARGE :
                size = NSRegularControlSize;
                break ;
                
            default:
                break ;
        }

        [[col1 dataCell] setControlSize:size];
        // although there is no text, it may help to get the correct vertical layout
        [[col1 dataCell] setFont:list->GetFont().OSXGetNSFont()];        
    }

    [checkbox release];

    unsigned formerColCount = [m_tableView numberOfColumns];

    // there's apparently no way to insert at a specific position
    [m_tableView addTableColumn:col1 ];
    if ( pos < formerColCount )
        [m_tableView moveColumn:formerColCount toColumn:pos];

    if ( defaultWidth >= 0 )
    {
        [col1 setMaxWidth:defaultWidth];
        [col1 setMinWidth:defaultWidth];
        [col1 setWidth:defaultWidth];
    }

    [col1 setResizingMask: NSTableColumnNoResizing];
    wxCocoaTableColumn* wxcol = new wxCocoaTableColumn( col1, editable );
    [col1 setColumn:wxcol];

    // owned by the tableview
    [col1 release];
    return wxcol;
}


//
// inserting / removing lines
//

void wxListWidgetCocoaImpl::ListInsert( unsigned int WXUNUSED(n) )
{
    [m_tableView reloadData];
}

void wxListWidgetCocoaImpl::ListDelete( unsigned int WXUNUSED(n) )
{
    [m_tableView reloadData];
}

void wxListWidgetCocoaImpl::ListClear()
{
    [m_tableView reloadData];
}

// selecting

void wxListWidgetCocoaImpl::ListDeselectAll()
{
    [m_tableView deselectAll:nil];
}

void wxListWidgetCocoaImpl::ListSetSelection( unsigned int n, bool select, bool multi )
{
    // TODO
    if ( select )
        [m_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:n]
		     byExtendingSelection:multi];
    else
        [m_tableView deselectRow: n];

}

int wxListWidgetCocoaImpl::ListGetSelection() const
{
    return [m_tableView selectedRow];
}

int wxListWidgetCocoaImpl::ListGetSelections( wxArrayInt& aSelections ) const
{
    aSelections.Empty();

    int count = ListGetCount();

    for ( int i = 0; i < count; ++i)
    {
        if ([m_tableView isRowSelected:i])
        aSelections.Add(i);
    }

    return aSelections.Count();
}

bool wxListWidgetCocoaImpl::ListIsSelected( unsigned int n ) const
{
    return [m_tableView isRowSelected:n];
}

// display

void wxListWidgetCocoaImpl::ListScrollTo( unsigned int n )
{
    [m_tableView scrollRowToVisible:n];
}

int wxListWidgetCocoaImpl::ListGetTopItem() const
{
     NSScrollView *scrollView = [m_tableView enclosingScrollView];
     NSRect visibleRect = scrollView.contentView.visibleRect;
     NSRange range = [m_tableView rowsInRect:visibleRect];
     return range.location;
}

void wxListWidgetCocoaImpl::UpdateLine( unsigned int WXUNUSED(n), wxListWidgetColumn* WXUNUSED(col) )
{
    // TODO optimize
    [m_tableView reloadData];
}

void wxListWidgetCocoaImpl::UpdateLineToEnd( unsigned int WXUNUSED(n))
{
    // TODO optimize
    [m_tableView reloadData];
}

void wxListWidgetCocoaImpl::controlDoubleAction(WXWidget WXUNUSED(slf),void* WXUNUSED(_cmd), void *WXUNUSED(sender))
{
    wxListBox *list = static_cast<wxListBox*> ( GetWXPeer());
    wxCHECK_RET( list != NULL , wxT("Listbox expected"));

    int sel = [m_tableView clickedRow];
    if ((sel < 0) || (sel > (int) list->GetCount()))  // OS X can select an item below the last item (why?)
       return;

    list->HandleLineEvent( sel, true );
}

// accessing content


wxWidgetImplType* wxWidgetImpl::CreateListBox( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    NSScrollView* scrollview = [[NSScrollView alloc] initWithFrame:r];

    // use same scroll flags logic as msw

    [scrollview setHasVerticalScroller:YES];

    if ( style & wxLB_HSCROLL )
        [scrollview setHasHorizontalScroller:YES];

    [scrollview setAutohidesScrollers: ((style & wxLB_ALWAYS_SB) ? NO : YES)];

    // setting up the true table

    wxNSTableView* tableview = [[wxNSTableView alloc] init];
    [tableview setDelegate:tableview];
    // only one multi-select mode available
    if ( (style & wxLB_EXTENDED) || (style & wxLB_MULTIPLE) )
        [tableview setAllowsMultipleSelection:YES];

    // simple listboxes have no header row
    [tableview setHeaderView:nil];

    if ( style & wxLB_HSCROLL )
        [tableview setColumnAutoresizingStyle:NSTableViewNoColumnAutoresizing];
    else
        [tableview setColumnAutoresizingStyle:NSTableViewLastColumnOnlyAutoresizingStyle];

    wxNSTableDataSource* ds = [[ wxNSTableDataSource alloc] init];
    [tableview setDataSource:ds];
    [scrollview setDocumentView:tableview];
    [tableview release];

    wxListWidgetCocoaImpl* c = new wxListWidgetCocoaImpl( wxpeer, scrollview, tableview, ds );

    // temporary hook for dnd
 //   [tableview registerForDraggedTypes:[NSArray arrayWithObjects:
 //       NSStringPboardType, NSFilenamesPboardType, (NSString*) kPasteboardTypeFileURLPromise, NSTIFFPboardType, NSPICTPboardType, NSPDFPboardType, nil]];

    [ds setImplementation:c];
    return c;
}

int wxListWidgetCocoaImpl::DoListHitTest(const wxPoint& inpoint) const
{
    // translate inpoint to listpoint via scrollview
    NSPoint p = wxToNSPoint( m_osxView, inpoint );
    p = [m_osxView convertPoint:p toView:m_tableView];
    // hittest using new point
    NSInteger i = [m_tableView rowAtPoint:p];
    return i;
}

#endif // wxUSE_LISTBOX
