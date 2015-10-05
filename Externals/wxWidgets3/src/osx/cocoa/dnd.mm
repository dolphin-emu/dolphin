///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/dnd.mm
// Purpose:     wxDropTarget, wxDropSource implementations
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) 1998 Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/object.h"
#endif

#if wxUSE_DRAG_AND_DROP

#include "wx/dnd.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/toplevel.h"
    #include "wx/gdicmn.h"
    #include "wx/wx.h"
#endif // WX_PRECOMP

#include "wx/evtloop.h"

#include "wx/osx/private.h"

wxDropSource* gCurrentSource = NULL;

wxDragResult NSDragOperationToWxDragResult(NSDragOperation code)
{
    switch (code)
    {
        case NSDragOperationGeneric:
            return wxDragCopy;
        case NSDragOperationCopy:
            return wxDragCopy;
        case NSDragOperationMove:
            return wxDragMove;
        case NSDragOperationLink:
            return wxDragLink;
        case NSDragOperationNone:
            return wxDragNone;
        default:
            wxFAIL_MSG("Unexpected result code");
    }
    return wxDragNone;
}

@interface DropSourceDelegate : NSObject
{
    BOOL dragFinished;
    int resultCode;
    wxDropSource* impl;

    // Flags for drag and drop operations (wxDrag_* ).
    int m_dragFlags;
}

- (void)setImplementation:(wxDropSource *)dropSource flags:(int)flags;
- (BOOL)finished;
- (NSDragOperation)code;
- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)forLocal;
- (void)draggedImage:(NSImage *)anImage movedTo:(NSPoint)aPoint;
- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation;
@end

@implementation DropSourceDelegate

- (id)init
{
    self = [super init];
    dragFinished = NO;
    resultCode = NSDragOperationNone;
    impl = 0;
    m_dragFlags = wxDrag_CopyOnly;
    return self;
}

- (void)setImplementation:(wxDropSource *)dropSource flags:(int)flags
{
    impl = dropSource;
    m_dragFlags = flags;
}

- (BOOL)finished
{
    return dragFinished;
}

- (NSDragOperation)code
{
    return resultCode;
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)forLocal
{
    /*
    By default drag targets receive a mask of NSDragOperationAll (0xf)
    which, despite its name, does not include the later added
    NSDragOperationMove (0x10) that sometimes is wanted.
    Use NSDragOperationEvery instead because it includes all flags.

    Note that this, compared to the previous behaviour, adds
    NSDragOperationDelete to the mask which seems harmless.

    We are also keeping NSDragOperationLink and NSDragOperationPrivate
    in to preserve previous behaviour.
    */

    NSDragOperation allowedDragOperations = NSDragOperationEvery;

    if (m_dragFlags == wxDrag_CopyOnly)
    {
        allowedDragOperations &= ~NSDragOperationMove;
    }

    return allowedDragOperations;
}

- (void)draggedImage:(NSImage *)anImage movedTo:(NSPoint)aPoint
{
    wxUnusedVar( anImage );
    wxUnusedVar( aPoint );
    
    bool optionDown = GetCurrentKeyModifiers() & optionKey;
    wxDragResult result = optionDown ? wxDragCopy : wxDragMove;
    
    if (wxDropSource* source = impl)
    {
        if (!source->GiveFeedback(result))
        {
            wxStockCursor cursorID = wxCURSOR_NONE;

            switch (result)
            {
                case wxDragCopy:
                    cursorID = wxCURSOR_COPY_ARROW;
                    break;

                case wxDragMove:
                    cursorID = wxCURSOR_ARROW;
                    break;

                case wxDragNone:
                    cursorID = wxCURSOR_NO_ENTRY;
                    break;

                case wxDragError:
                case wxDragLink:
                case wxDragCancel:
                default:
                    // put these here to make gcc happy
                    ;
            }

            if (cursorID != wxCURSOR_NONE)
            {
                // TODO under 10.6 the os itself deals with the cursor, remove if things
                // work properly everywhere
#if 0
                wxCursor cursor( cursorID );
                cursor.MacInstall();
#endif
            }
        }
    }
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
    wxUnusedVar( anImage );
    wxUnusedVar( aPoint );
    
    resultCode = operation;
    dragFinished = YES;
}

@end

wxDropTarget::wxDropTarget( wxDataObject *data )
            : wxDropTargetBase( data )
{

}

//-------------------------------------------------------------------------
// wxDropSource
//-------------------------------------------------------------------------

wxDropSource::wxDropSource(wxWindow *win,
                           const wxCursor &cursorCopy,
                           const wxCursor &cursorMove,
                           const wxCursor &cursorStop)
            : wxDropSourceBase(cursorCopy, cursorMove, cursorStop)
{
    m_window = win;
}

wxDropSource::wxDropSource(wxDataObject& data,
                           wxWindow *win,
                           const wxCursor &cursorCopy,
                           const wxCursor &cursorMove,
                           const wxCursor &cursorStop)
            : wxDropSourceBase(cursorCopy, cursorMove, cursorStop)
{
    SetData( data );
    m_window = win;
}

wxDropSource* wxDropSource::GetCurrentDropSource()
{
    return gCurrentSource;
}

wxDragResult wxDropSource::DoDragDrop(int flags)
{
    wxASSERT_MSG( m_data, wxT("Drop source: no data") );

    wxDragResult result = wxDragNone;
    if ((m_data == NULL) || (m_data->GetFormatCount() == 0))
        return result;

    NSView* view = m_window->GetPeer()->GetWXWidget();
    if (view)
    {
        NSPasteboard *pboard;
 
        pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    
        OSStatus err = noErr;
        PasteboardRef pboardRef;
        PasteboardCreate((CFStringRef)[pboard name], &pboardRef);
        
        err = PasteboardClear( pboardRef );
        if ( err != noErr )
        {
            CFRelease( pboardRef );
            return wxDragNone;
        }
        PasteboardSynchronize( pboardRef );
        
        m_data->AddToPasteboard( pboardRef, 1 );
        
        NSEvent* theEvent = (NSEvent*)wxTheApp->MacGetCurrentEvent();
        wxASSERT_MSG(theEvent, "DoDragDrop must be called in response to a mouse down or drag event.");

        NSPoint down = [theEvent locationInWindow];
        NSPoint p = [view convertPoint:down fromView:nil];
                
        gCurrentSource = this;
        
        // add a dummy square as dragged image for the moment, 
        // TODO: proper drag image for data
        NSSize sz = NSMakeSize(16,16);
        NSRect fillRect = NSMakeRect(0, 0, 16, 16);
        NSImage* image = [[NSImage alloc] initWithSize: sz];
 
        [image lockFocus];
        
        [[[NSColor whiteColor] colorWithAlphaComponent:0.8] set];
        NSRectFill(fillRect);
        [[NSColor blackColor] set];
        NSFrameRectWithWidthUsingOperation(fillRect,1.0f,NSCompositeDestinationOver);
        
        [image unlockFocus];        
        
        
        DropSourceDelegate* delegate = [[DropSourceDelegate alloc] init];
        [delegate setImplementation:this flags:flags];
        [view dragImage:image at:p offset:NSMakeSize(0.0,0.0) 
            event: theEvent pasteboard: pboard source:delegate slideBack: NO];
        
        wxEventLoopBase * const loop = wxEventLoop::GetActive();
        while ( ![delegate finished] )
            loop->Dispatch();
        
        result = NSDragOperationToWxDragResult([delegate code]);
        [delegate release];
        [image release];
        
        wxWindow* mouseUpTarget = wxWindow::GetCapture();

        if ( mouseUpTarget == NULL )
        {
            mouseUpTarget = m_window;
        }
        
        if ( mouseUpTarget != NULL )
        {
            wxMouseEvent wxevent(wxEVT_LEFT_DOWN);
            ((wxWidgetCocoaImpl*)mouseUpTarget->GetPeer())->SetupMouseEvent(wxevent , theEvent) ;
            wxevent.SetEventType(wxEVT_LEFT_UP);
        
            mouseUpTarget->HandleWindowEvent(wxevent);
        }
        
        gCurrentSource = NULL;
    }


    return result;
}

#endif // wxUSE_DRAG_AND_DROP

