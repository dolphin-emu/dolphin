/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/window.mm
// Purpose:     widgets (non tlw) for cocoa
// Author:      Stefan Csomor
// Modified by:
// Created:     2008-06-20
// RCS-ID:      $Id: window.mm 67243 2011-03-19 08:36:23Z SC $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/dcclient.h"
    #include "wx/frame.h"
    #include "wx/log.h"
    #include "wx/textctrl.h"
    #include "wx/combobox.h"
#endif

#ifdef __WXMAC__
    #include "wx/osx/private.h"
#endif

#include "wx/evtloop.h"

#if wxUSE_CARET
    #include "wx/caret.h"
#endif

#if wxUSE_DRAG_AND_DROP
    #include "wx/dnd.h"
#endif

#if wxUSE_TOOLTIPS
    #include "wx/tooltip.h"
#endif

#include <objc/objc-runtime.h>

// Get the window with the focus

NSView* GetViewFromResponder( NSResponder* responder )
{
    NSView* view = nil;
    if ( [responder isKindOfClass:[NSTextView class]] )
    {
        NSView* delegate = (NSView*) [(NSTextView*)responder delegate];
        if ( [delegate isKindOfClass:[NSTextField class] ] )
            view = delegate;
        else
            view =  (NSView*) responder;
    }
    else
    {
        if ( [responder isKindOfClass:[NSView class]] )
            view = (NSView*) responder;
    }
    return view;
}

NSView* GetFocusedViewInWindow( NSWindow* keyWindow )
{
    NSView* focusedView = nil;
    if ( keyWindow != nil )
        focusedView = GetViewFromResponder([keyWindow firstResponder]);

    return focusedView;
}

WXWidget wxWidgetImpl::FindFocus()
{
    return GetFocusedViewInWindow( [NSApp keyWindow] );
}

NSRect wxOSXGetFrameForControl( wxWindowMac* window , const wxPoint& pos , const wxSize &size , bool adjustForOrigin )
{
    int x, y, w, h ;

    window->MacGetBoundsForControl( pos , size , x , y, w, h , adjustForOrigin ) ;
    wxRect bounds(x,y,w,h);
    NSView* sv = (window->GetParent()->GetHandle() );

    return wxToNSRect( sv, bounds );
}

@interface wxNSView : NSView
{
    NSTrackingRectTag rectTag;
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
    NSTrackingArea* _trackingArea;
#endif
}

// the tracking tag is needed to track mouse enter / exit events
- (void) setTrackingTag: (NSTrackingRectTag)tag;
- (NSTrackingRectTag) trackingTag;
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
// under 10.5 we can also track mouse moved events on non-focused windows if
// we use the new NSTrackingArea APIs. 
- (void) updateTrackingArea;
- (NSTrackingArea*) trackingArea;
#endif
@end // wxNSView

@interface NSView(PossibleMethods)
- (void)setTitle:(NSString *)aString;
- (void)setStringValue:(NSString *)aString;
- (void)setIntValue:(int)anInt;
- (void)setFloatValue:(float)aFloat;
- (void)setDoubleValue:(double)aDouble;

- (double)minValue;
- (double)maxValue;
- (void)setMinValue:(double)aDouble;
- (void)setMaxValue:(double)aDouble;

- (void)sizeToFit;

- (BOOL)isEnabled;
- (void)setEnabled:(BOOL)flag;

- (void)setImage:(NSImage *)image;
- (void)setControlSize:(NSControlSize)size;

- (void)setFont:(NSFont *)fontObject;

- (id)contentView;

- (void)setTarget:(id)anObject;
- (void)setAction:(SEL)aSelector;
- (void)setDoubleAction:(SEL)aSelector;
- (void)setBackgroundColor:(NSColor*)aColor;
- (void)setOpaque:(BOOL)opaque;
- (void)setTextColor:(NSColor *)color;
- (void)setImagePosition:(NSCellImagePosition)aPosition;
@end

long wxOSXTranslateCocoaKey( NSEvent* event )
{
    long retval = 0;

    if ([event type] != NSFlagsChanged)
    {
        NSString* s = [event charactersIgnoringModifiers];
        // backspace char reports as delete w/modifiers for some reason
        if ([s length] == 1)
        {
            switch ( [s characterAtIndex:0] )
            {
                // backspace key
                case 0x7F :
                case 8 :
                    retval = WXK_BACK;
                    break;
                case NSUpArrowFunctionKey :
                    retval = WXK_UP;
                    break;
                case NSDownArrowFunctionKey :
                    retval = WXK_DOWN;
                    break;
                case NSLeftArrowFunctionKey :
                    retval = WXK_LEFT;
                    break;
                case NSRightArrowFunctionKey :
                    retval = WXK_RIGHT;
                    break;
                case NSInsertFunctionKey  :
                    retval = WXK_INSERT;
                    break;
                case NSDeleteFunctionKey  :
                    retval = WXK_DELETE;
                    break;
                case NSHomeFunctionKey  :
                    retval = WXK_HOME;
                    break;
        //        case NSBeginFunctionKey  :
        //            retval = WXK_BEGIN;
        //            break;
                case NSEndFunctionKey  :
                    retval = WXK_END;
                    break;
                case NSPageUpFunctionKey  :
                    retval = WXK_PAGEUP;
                    break;
               case NSPageDownFunctionKey  :
                    retval = WXK_PAGEDOWN;
                    break;
               case NSHelpFunctionKey  :
                    retval = WXK_HELP;
                    break;
                default:
                    int intchar = [s characterAtIndex: 0];
                    if ( intchar >= NSF1FunctionKey && intchar <= NSF24FunctionKey )
                        retval = WXK_F1 + (intchar - NSF1FunctionKey );
                    break;
            }
        }
    }

    // Some keys don't seem to have constants. The code mimics the approach
    // taken by WebKit. See:
    // http://trac.webkit.org/browser/trunk/WebCore/platform/mac/KeyEventMac.mm
    switch( [event keyCode] )
    {
        // command key
        case 54:
        case 55:
            retval = WXK_COMMAND;
            break;
        // caps locks key
        case 57: // Capslock
            retval = WXK_CAPITAL;
            break;
        // shift key
        case 56: // Left Shift
        case 60: // Right Shift
            retval = WXK_SHIFT;
            break;
        // alt key
        case 58: // Left Alt
        case 61: // Right Alt
            retval = WXK_ALT;
            break;
        // ctrl key
        case 59: // Left Ctrl
        case 62: // Right Ctrl
            retval = WXK_CONTROL;
            break;
        // clear key
        case 71:
            retval = WXK_CLEAR;
            break;
        // tab key
        case 48:
            retval = WXK_TAB;
            break;

        case 75: // /
            retval = WXK_NUMPAD_DIVIDE;
            break;
        case 67: // *
            retval = WXK_NUMPAD_MULTIPLY;
            break;
        case 78: // -
            retval = WXK_NUMPAD_SUBTRACT;
            break;
        case 69: // +
            retval = WXK_NUMPAD_ADD;
            break;
        case 76: // Enter
            retval = WXK_NUMPAD_ENTER;
            break;
        case 65: // .
            retval = WXK_NUMPAD_DECIMAL;
            break;
        case 82: // 0
            retval = WXK_NUMPAD0;
            break;
        case 83: // 1
            retval = WXK_NUMPAD1;
            break;
        case 84: // 2
            retval = WXK_NUMPAD2;
            break;
        case 85: // 3
            retval = WXK_NUMPAD3;
            break;
        case 86: // 4
            retval = WXK_NUMPAD4;
            break;
        case 87: // 5
            retval = WXK_NUMPAD5;
            break;
        case 88: // 6
            retval = WXK_NUMPAD6;
            break;
        case 89: // 7
            retval = WXK_NUMPAD7;
            break;
        case 91: // 8
            retval = WXK_NUMPAD8;
            break;
        case 92: // 9
            retval = WXK_NUMPAD9;
            break;
        default:
            //retval = [event keyCode];
            break;
    }
    return retval;
}

void wxWidgetCocoaImpl::SetupKeyEvent(wxKeyEvent &wxevent , NSEvent * nsEvent, NSString* charString)
{
    UInt32 modifiers = [nsEvent modifierFlags] ;
    int eventType = [nsEvent type];

    wxevent.m_shiftDown = modifiers & NSShiftKeyMask;
    wxevent.m_controlDown = modifiers & NSControlKeyMask;
    wxevent.m_altDown = modifiers & NSAlternateKeyMask;
    wxevent.m_metaDown = modifiers & NSCommandKeyMask;

    wxevent.m_rawCode = [nsEvent keyCode];
    wxevent.m_rawFlags = modifiers;

    wxevent.SetTimestamp( (int)([nsEvent timestamp] * 1000) ) ;

    wxString chars;
    if ( eventType != NSFlagsChanged )
    {
        NSString* nschars = [nsEvent charactersIgnoringModifiers];
        if ( charString )
        {
            // if charString is set, it did not come from key up / key down
            wxevent.SetEventType( wxEVT_CHAR );
            chars = wxCFStringRef::AsString(charString);
        }
        else if ( nschars )
        {
            chars = wxCFStringRef::AsString(nschars);
        }
    }

    int aunichar = chars.Length() > 0 ? chars[0] : 0;
    long keyval = 0;

    if (wxevent.GetEventType() != wxEVT_CHAR)
    {
        keyval = wxOSXTranslateCocoaKey(nsEvent) ;
        switch (eventType)
        {
            case NSKeyDown :
                wxevent.SetEventType( wxEVT_KEY_DOWN )  ;
                break;
            case NSKeyUp :
                wxevent.SetEventType( wxEVT_KEY_UP )  ;
                break;
            case NSFlagsChanged :
                switch (keyval)
                {
                    case WXK_CONTROL:
                        wxevent.SetEventType( wxevent.m_controlDown ? wxEVT_KEY_DOWN : wxEVT_KEY_UP);
                        break;
                    case WXK_SHIFT:
                        wxevent.SetEventType( wxevent.m_shiftDown ? wxEVT_KEY_DOWN : wxEVT_KEY_UP);
                        break;
                    case WXK_ALT:
                        wxevent.SetEventType( wxevent.m_altDown ? wxEVT_KEY_DOWN : wxEVT_KEY_UP);
                        break;
                    case WXK_COMMAND:
                        wxevent.SetEventType( wxevent.m_metaDown ? wxEVT_KEY_DOWN : wxEVT_KEY_UP);
                        break;
                }
                break;
            default :
                break ;
        }
    }

    if ( !keyval )
    {
        if ( wxevent.GetEventType() == wxEVT_KEY_UP || wxevent.GetEventType() == wxEVT_KEY_DOWN )
            keyval = wxToupper( aunichar ) ;
        else
            keyval = aunichar;
    }

#if wxUSE_UNICODE
    wxevent.m_uniChar = aunichar;
#endif
    wxevent.m_keyCode = keyval;

    wxWindowMac* peer = GetWXPeer();
    if ( peer )
    {
        wxevent.SetEventObject(peer);
        wxevent.SetId(peer->GetId()) ;
    }
}

UInt32 g_lastButton = 0 ;
bool g_lastButtonWasFakeRight = false ;

// better scroll wheel support 
// see http://lists.apple.com/archives/cocoa-dev/2007/Feb/msg00050.html

@interface NSEvent (DeviceDelta)
- (CGFloat)deviceDeltaX;
- (CGFloat)deviceDeltaY;
@end

void wxWidgetCocoaImpl::SetupMouseEvent( wxMouseEvent &wxevent , NSEvent * nsEvent )
{
    int eventType = [nsEvent type];
    UInt32 modifiers = [nsEvent modifierFlags] ;

    NSPoint locationInWindow = [nsEvent locationInWindow];
    
    // adjust coordinates for the window of the target view
    if ( [nsEvent window] != [m_osxView window] )
    {
        if ( [nsEvent window] != nil )
            locationInWindow = [[nsEvent window] convertBaseToScreen:locationInWindow];

        if ( [m_osxView window] != nil )
            locationInWindow = [[m_osxView window] convertScreenToBase:locationInWindow];
    }

    NSPoint locationInView = [m_osxView convertPoint:locationInWindow fromView:nil];
    wxPoint locationInViewWX = wxFromNSPoint( m_osxView, locationInView );

    // these parameters are not given for all events
    UInt32 button = [nsEvent buttonNumber];
    UInt32 clickCount = 0;

    wxevent.m_x = locationInViewWX.x;
    wxevent.m_y = locationInViewWX.y;
    wxevent.m_shiftDown = modifiers & NSShiftKeyMask;
    wxevent.m_controlDown = modifiers & NSControlKeyMask;
    wxevent.m_altDown = modifiers & NSAlternateKeyMask;
    wxevent.m_metaDown = modifiers & NSCommandKeyMask;
    wxevent.SetTimestamp( (int)([nsEvent timestamp] * 1000) ) ;

    UInt32 mouseChord = 0;

    switch (eventType)
    {
        case NSLeftMouseDown :
        case NSLeftMouseDragged :
            mouseChord = 1U;
            break;
        case NSRightMouseDown :
        case NSRightMouseDragged :
            mouseChord = 2U;
            break;
        case NSOtherMouseDown :
        case NSOtherMouseDragged :
            mouseChord = 4U;
            break;
    }

    // a control click is interpreted as a right click
    bool thisButtonIsFakeRight = false ;
    if ( button == 0 && (modifiers & NSControlKeyMask) )
    {
        button = 1 ;
        thisButtonIsFakeRight = true ;
    }

    // otherwise we report double clicks by connecting a left click with a ctrl-left click
    if ( clickCount > 1 && button != g_lastButton )
        clickCount = 1 ;

    // we must make sure that our synthetic 'right' button corresponds in
    // mouse down, moved and mouse up, and does not deliver a right down and left up
    switch (eventType)
    {
        case NSLeftMouseDown :
        case NSRightMouseDown :
        case NSOtherMouseDown :
            g_lastButton = button ;
            g_lastButtonWasFakeRight = thisButtonIsFakeRight ;
            break;
     }

    if ( button == 0 )
    {
        g_lastButton = 0 ;
        g_lastButtonWasFakeRight = false ;
    }
    else if ( g_lastButton == 1 && g_lastButtonWasFakeRight )
        button = g_lastButton ;

    // Adjust the chord mask to remove the primary button and add the
    // secondary button.  It is possible that the secondary button is
    // already pressed, e.g. on a mouse connected to a laptop, but this
    // possibility is ignored here:
    if( thisButtonIsFakeRight && ( mouseChord & 1U ) )
        mouseChord = ((mouseChord & ~1U) | 2U);

    if(mouseChord & 1U)
                wxevent.m_leftDown = true ;
    if(mouseChord & 2U)
                wxevent.m_rightDown = true ;
    if(mouseChord & 4U)
                wxevent.m_middleDown = true ;

    // translate into wx types
    switch (eventType)
    {
        case NSLeftMouseDown :
        case NSRightMouseDown :
        case NSOtherMouseDown :
            clickCount = [nsEvent clickCount];
            switch ( button )
            {
                case 0 :
                    wxevent.SetEventType( clickCount > 1 ? wxEVT_LEFT_DCLICK : wxEVT_LEFT_DOWN )  ;
                    break ;

                case 1 :
                    wxevent.SetEventType( clickCount > 1 ? wxEVT_RIGHT_DCLICK : wxEVT_RIGHT_DOWN ) ;
                    break ;

                case 2 :
                    wxevent.SetEventType( clickCount > 1 ? wxEVT_MIDDLE_DCLICK : wxEVT_MIDDLE_DOWN ) ;
                    break ;

                default:
                    break ;
            }
            break ;

        case NSLeftMouseUp :
        case NSRightMouseUp :
        case NSOtherMouseUp :
            clickCount = [nsEvent clickCount];
            switch ( button )
            {
                case 0 :
                    wxevent.SetEventType( wxEVT_LEFT_UP )  ;
                    break ;

                case 1 :
                    wxevent.SetEventType( wxEVT_RIGHT_UP ) ;
                    break ;

                case 2 :
                    wxevent.SetEventType( wxEVT_MIDDLE_UP ) ;
                    break ;

                default:
                    break ;
            }
            break ;

     case NSScrollWheel :
        {
            float deltaX = 0.0;
            float deltaY = 0.0;

            wxevent.SetEventType( wxEVT_MOUSEWHEEL ) ;

            // see http://developer.apple.com/qa/qa2005/qa1453.html
            // for more details on why we have to look for the exact type
            
            const EventRef cEvent = (EventRef) [nsEvent eventRef];
            bool isMouseScrollEvent = false;
            if ( cEvent )
                isMouseScrollEvent = ::GetEventKind(cEvent) == kEventMouseScroll;
                
            if ( isMouseScrollEvent )
            {
                deltaX = [nsEvent deviceDeltaX];
                deltaY = [nsEvent deviceDeltaY];
            }
            else
            {
                deltaX = ([nsEvent deltaX] * 10);
                deltaY = ([nsEvent deltaY] * 10);
            }
            
            wxevent.m_wheelDelta = 10;
            wxevent.m_linesPerAction = 1;
                
            if ( fabs(deltaX) > fabs(deltaY) )
            {
                wxevent.m_wheelAxis = 1;
                wxevent.m_wheelRotation = (int)deltaX;
            }
            else
            {
                wxevent.m_wheelRotation = (int)deltaY;
            }

        }
        break ;

        case NSMouseEntered :
            wxevent.SetEventType( wxEVT_ENTER_WINDOW ) ;
            break;
        case NSMouseExited :
            wxevent.SetEventType( wxEVT_LEAVE_WINDOW ) ;
            break;
        case NSLeftMouseDragged :
        case NSRightMouseDragged :
        case NSOtherMouseDragged :
        case NSMouseMoved :
            wxevent.SetEventType( wxEVT_MOTION ) ;
            break;
        default :
            break ;
    }

    wxevent.m_clickCount = clickCount;
    wxWindowMac* peer = GetWXPeer();
    if ( peer )
    {
        wxevent.SetEventObject(peer);
        wxevent.SetId(peer->GetId()) ;
    }
}

@implementation wxNSView

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
    }
}

- (void) setTrackingTag: (NSTrackingRectTag)tag
{
    rectTag = tag;
}

- (NSTrackingRectTag) trackingTag
{
    return rectTag;
}

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
- (void) updateTrackingArea
{
    if (_trackingArea)
    {
        [self removeTrackingArea: _trackingArea];
        [_trackingArea release];
    }
    
    NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited|NSTrackingMouseMoved|NSTrackingActiveAlways;
        
    NSTrackingArea* area = [[NSTrackingArea alloc] initWithRect: [self bounds] options: options owner: self userInfo: nil];
    [self addTrackingArea: area];

    _trackingArea = area;
}

- (NSTrackingArea*) trackingArea
{
    return _trackingArea;
}
#endif
@end // wxNSView

//
// event handlers
//

#if wxUSE_DRAG_AND_DROP

// see http://lists.apple.com/archives/Cocoa-dev/2005/Jul/msg01244.html
// for details on the NSPasteboard -> PasteboardRef conversion

NSDragOperation wxOSX_draggingEntered( id self, SEL _cmd, id <NSDraggingInfo>sender )
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if (impl == NULL)
        return NSDragOperationNone;

    return impl->draggingEntered(sender, self, _cmd);
}

void wxOSX_draggingExited( id self, SEL _cmd, id <NSDraggingInfo> sender )
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if (impl == NULL)
        return ;

    return impl->draggingExited(sender, self, _cmd);
}

NSDragOperation wxOSX_draggingUpdated( id self, SEL _cmd, id <NSDraggingInfo>sender )
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if (impl == NULL)
        return NSDragOperationNone;

    return impl->draggingUpdated(sender, self, _cmd);
}

BOOL wxOSX_performDragOperation( id self, SEL _cmd, id <NSDraggingInfo> sender )
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if (impl == NULL)
        return NSDragOperationNone;

    return impl->performDragOperation(sender, self, _cmd) ? YES:NO ;
}

#endif

void wxOSX_mouseEvent(NSView* self, SEL _cmd, NSEvent *event)
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if (impl == NULL)
        return;

    impl->mouseEvent(event, self, _cmd);
}

BOOL wxOSX_acceptsFirstMouse(NSView* WXUNUSED(self), SEL WXUNUSED(_cmd), NSEvent *WXUNUSED(event))
{
    // This is needed to support click through, otherwise the first click on a window
    // will not do anything unless it is the active window already.
    return YES;
}

void wxOSX_keyEvent(NSView* self, SEL _cmd, NSEvent *event)
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if (impl == NULL)
        return;

    impl->keyEvent(event, self, _cmd);
}

void wxOSX_insertText(NSView* self, SEL _cmd, NSString* text)
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if (impl == NULL)
        return;

    impl->insertText(text, self, _cmd);
}

BOOL wxOSX_performKeyEquivalent(NSView* self, SEL _cmd, NSEvent *event)
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if (impl == NULL)
        return NO;

    return impl->performKeyEquivalent(event, self, _cmd);
}

BOOL wxOSX_acceptsFirstResponder(NSView* self, SEL _cmd)
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if (impl == NULL)
        return NO;

    return impl->acceptsFirstResponder(self, _cmd);
}

BOOL wxOSX_becomeFirstResponder(NSView* self, SEL _cmd)
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if (impl == NULL)
        return NO;

    return impl->becomeFirstResponder(self, _cmd);
}

BOOL wxOSX_resignFirstResponder(NSView* self, SEL _cmd)
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if (impl == NULL)
        return NO;

    return impl->resignFirstResponder(self, _cmd);
}

void wxOSX_resetCursorRects(NSView* self, SEL _cmd)
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if (impl == NULL)
        return;

    impl->resetCursorRects(self, _cmd);
}

BOOL wxOSX_isFlipped(NSView* self, SEL _cmd)
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if (impl == NULL)
        return NO;

    return impl->isFlipped(self, _cmd) ? YES:NO;
}

typedef void (*wxOSX_DrawRectHandlerPtr)(NSView* self, SEL _cmd, NSRect rect);

void wxOSX_drawRect(NSView* self, SEL _cmd, NSRect rect)
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if (impl == NULL)
        return;

#ifdef wxUSE_THREADS
    // OS X starts a NSUIHeartBeatThread for animating the default button in a
    // dialog. This causes a drawRect of the active dialog from outside the
    // main UI thread. This causes an occasional crash since the wx drawing
    // objects (like wxPen) are not thread safe.
    //
    // Notice that NSUIHeartBeatThread seems to be undocumented and doing
    // [NSWindow setAllowsConcurrentViewDrawing:NO] does not affect it.
    if ( !wxThread::IsMain() )
    {
      // just call the superclass handler, we don't need any custom wx drawing
      // here and it seems to work fine:
      wxOSX_DrawRectHandlerPtr
          superimpl = (wxOSX_DrawRectHandlerPtr)
                        [[self superclass] instanceMethodForSelector:_cmd];
      superimpl(self, _cmd, rect);
      return;
    }
#endif // wxUSE_THREADS

    return impl->drawRect(&rect, self, _cmd);
}

void wxOSX_controlAction(NSView* self, SEL _cmd, id sender)
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if (impl == NULL)
        return;

    impl->controlAction(self, _cmd, sender);
}

void wxOSX_controlDoubleAction(NSView* self, SEL _cmd, id sender)
{
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if (impl == NULL)
        return;

    impl->controlDoubleAction(self, _cmd, sender);
}

unsigned int wxWidgetCocoaImpl::draggingEntered(void* s, WXWidget WXUNUSED(slf), void *WXUNUSED(_cmd))
{
    id <NSDraggingInfo>sender = (id <NSDraggingInfo>) s;
    NSPasteboard *pboard = [sender draggingPasteboard];
    NSDragOperation sourceDragMask = [sender draggingSourceOperationMask];

    wxWindow* wxpeer = GetWXPeer();
    if ( wxpeer == NULL )
        return NSDragOperationNone;

    wxDropTarget* target = wxpeer->GetDropTarget();
    if ( target == NULL )
        return NSDragOperationNone;

    wxDragResult result = wxDragNone;
    NSPoint nspoint = [m_osxView convertPoint:[sender draggingLocation] fromView:nil];
    wxPoint pt = wxFromNSPoint( m_osxView, nspoint );

    if ( sourceDragMask & NSDragOperationLink )
        result = wxDragLink;
    else if ( sourceDragMask & NSDragOperationCopy )
        result = wxDragCopy;
    else if ( sourceDragMask & NSDragOperationMove )
        result = wxDragMove;

    PasteboardRef pboardRef;
    PasteboardCreate((CFStringRef)[pboard name], &pboardRef);
    target->SetCurrentDragPasteboard(pboardRef);
    result = target->OnEnter(pt.x, pt.y, result);
    CFRelease(pboardRef);

    NSDragOperation nsresult = NSDragOperationNone;
    switch (result )
    {
        case wxDragLink:
            nsresult = NSDragOperationLink;
        case wxDragMove:
            nsresult = NSDragOperationMove;
        case wxDragCopy:
            nsresult = NSDragOperationCopy;
        default :
            break;
    }
    return nsresult;
}

void wxWidgetCocoaImpl::draggingExited(void* s, WXWidget WXUNUSED(slf), void *WXUNUSED(_cmd))
{
    id <NSDraggingInfo>sender = (id <NSDraggingInfo>) s;
    NSPasteboard *pboard = [sender draggingPasteboard];

    wxWindow* wxpeer = GetWXPeer();
    if ( wxpeer == NULL )
        return;

    wxDropTarget* target = wxpeer->GetDropTarget();
    if ( target == NULL )
        return;

    PasteboardRef pboardRef;
    PasteboardCreate((CFStringRef)[pboard name], &pboardRef);
    target->SetCurrentDragPasteboard(pboardRef);
    target->OnLeave();
    CFRelease(pboardRef);
 }

unsigned int wxWidgetCocoaImpl::draggingUpdated(void* s, WXWidget WXUNUSED(slf), void *WXUNUSED(_cmd))
{
    id <NSDraggingInfo>sender = (id <NSDraggingInfo>) s;
    NSPasteboard *pboard = [sender draggingPasteboard];
    NSDragOperation sourceDragMask = [sender draggingSourceOperationMask];

    wxWindow* wxpeer = GetWXPeer();
    if ( wxpeer == NULL )
        return NSDragOperationNone;

    wxDropTarget* target = wxpeer->GetDropTarget();
    if ( target == NULL )
        return NSDragOperationNone;

    wxDragResult result = wxDragNone;
    NSPoint nspoint = [m_osxView convertPoint:[sender draggingLocation] fromView:nil];
    wxPoint pt = wxFromNSPoint( m_osxView, nspoint );

    if ( sourceDragMask & NSDragOperationLink )
        result = wxDragLink;
    else if ( sourceDragMask & NSDragOperationCopy )
        result = wxDragCopy;
    else if ( sourceDragMask & NSDragOperationMove )
        result = wxDragMove;
    
    PasteboardRef pboardRef;
    PasteboardCreate((CFStringRef)[pboard name], &pboardRef);
    target->SetCurrentDragPasteboard(pboardRef);
    result = target->OnDragOver(pt.x, pt.y, result);
    CFRelease(pboardRef);

    NSDragOperation nsresult = NSDragOperationNone;
    switch (result )
    {
        case wxDragLink:
            nsresult = NSDragOperationLink;
        case wxDragMove:
            nsresult = NSDragOperationMove;
        case wxDragCopy:
            nsresult = NSDragOperationCopy;
        default :
            break;
    }
    return nsresult;
}

bool wxWidgetCocoaImpl::performDragOperation(void* s, WXWidget WXUNUSED(slf), void *WXUNUSED(_cmd))
{
    id <NSDraggingInfo>sender = (id <NSDraggingInfo>) s;

    NSPasteboard *pboard = [sender draggingPasteboard];
    NSDragOperation sourceDragMask = [sender draggingSourceOperationMask];

    wxWindow* wxpeer = GetWXPeer();
    wxDropTarget* target = wxpeer->GetDropTarget();
    wxDragResult result = wxDragNone;
    NSPoint nspoint = [m_osxView convertPoint:[sender draggingLocation] fromView:nil];
    wxPoint pt = wxFromNSPoint( m_osxView, nspoint );

    if ( sourceDragMask & NSDragOperationLink )
        result = wxDragLink;
    else if ( sourceDragMask & NSDragOperationCopy )
        result = wxDragCopy;
    else if ( sourceDragMask & NSDragOperationMove )
        result = wxDragMove;

    PasteboardRef pboardRef;
    PasteboardCreate((CFStringRef)[pboard name], &pboardRef);
    target->SetCurrentDragPasteboard(pboardRef);

    if (target->OnDrop(pt.x, pt.y))
        result = target->OnData(pt.x, pt.y, result);

    CFRelease(pboardRef);

    return result != wxDragNone;
}

typedef void (*wxOSX_TextEventHandlerPtr)(NSView* self, SEL _cmd, NSString *event);
typedef void (*wxOSX_EventHandlerPtr)(NSView* self, SEL _cmd, NSEvent *event);
typedef BOOL (*wxOSX_PerformKeyEventHandlerPtr)(NSView* self, SEL _cmd, NSEvent *event);
typedef BOOL (*wxOSX_FocusHandlerPtr)(NSView* self, SEL _cmd);
typedef BOOL (*wxOSX_ResetCursorRectsHandlerPtr)(NSView* self, SEL _cmd);

void wxWidgetCocoaImpl::mouseEvent(WX_NSEvent event, WXWidget slf, void *_cmd)
{
    if ( !DoHandleMouseEvent(event) )
    {
        // for plain NSView mouse events would propagate to parents otherwise
        if (!IsUserPane())
        {
            wxOSX_EventHandlerPtr superimpl = (wxOSX_EventHandlerPtr) [[slf superclass] instanceMethodForSelector:(SEL)_cmd];
            superimpl(slf, (SEL)_cmd, event);
        }
    }
}

void wxWidgetCocoaImpl::keyEvent(WX_NSEvent event, WXWidget slf, void *_cmd)
{
    if ( [event type] == NSKeyDown )
    {
        // there are key equivalents that are not command-combos and therefore not handled by cocoa automatically, 
        // therefore we call the menubar directly here, exit if the menu is handling the shortcut
        if ( [[[NSApplication sharedApplication] mainMenu] performKeyEquivalent:event] )
            return;
    
        m_lastKeyDownEvent = event;
    }
    
    if ( GetFocusedViewInWindow([slf window]) != slf || m_hasEditor || !DoHandleKeyEvent(event) )
    {
        wxOSX_EventHandlerPtr superimpl = (wxOSX_EventHandlerPtr) [[slf superclass] instanceMethodForSelector:(SEL)_cmd];
        superimpl(slf, (SEL)_cmd, event);
    }
    m_lastKeyDownEvent = NULL;
}

void wxWidgetCocoaImpl::insertText(NSString* text, WXWidget slf, void *_cmd)
{
    if ( m_lastKeyDownEvent==NULL || m_hasEditor || !DoHandleCharEvent(m_lastKeyDownEvent, text) )
    {
        wxOSX_TextEventHandlerPtr superimpl = (wxOSX_TextEventHandlerPtr) [[slf superclass] instanceMethodForSelector:(SEL)_cmd];
        superimpl(slf, (SEL)_cmd, text);
    }
}


bool wxWidgetCocoaImpl::performKeyEquivalent(WX_NSEvent event, WXWidget slf, void *_cmd)
{
    bool handled = false;
    
    wxKeyEvent wxevent(wxEVT_KEY_DOWN);
    SetupKeyEvent( wxevent, event );
   
    // because performKeyEquivalent is going up the entire view hierarchy, we don't have to
    // walk up the ancestors ourselves but let cocoa do it
    
    int command = m_wxPeer->GetAcceleratorTable()->GetCommand( wxevent );
    if (command != -1)
    {
        wxEvtHandler * const handler = m_wxPeer->GetEventHandler();
        
        wxCommandEvent command_event( wxEVT_COMMAND_MENU_SELECTED, command );
        handled = handler->ProcessEvent( command_event );
        
        if ( !handled )
        {
            // accelerators can also be used with buttons, try them too
            command_event.SetEventType(wxEVT_COMMAND_BUTTON_CLICKED);
            handled = handler->ProcessEvent( command_event );
        }
    }
    
    if ( !handled )
    {
        wxOSX_PerformKeyEventHandlerPtr superimpl = (wxOSX_PerformKeyEventHandlerPtr) [[slf superclass] instanceMethodForSelector:(SEL)_cmd];
        return superimpl(slf, (SEL)_cmd, event);
    }
    return YES;
}

bool wxWidgetCocoaImpl::acceptsFirstResponder(WXWidget slf, void *_cmd)
{
    if ( IsUserPane() )
        return m_wxPeer->AcceptsFocus();
    else
    {
        wxOSX_FocusHandlerPtr superimpl = (wxOSX_FocusHandlerPtr) [[slf superclass] instanceMethodForSelector:(SEL)_cmd];
        return superimpl(slf, (SEL)_cmd);
    }
}

bool wxWidgetCocoaImpl::becomeFirstResponder(WXWidget slf, void *_cmd)
{
    wxOSX_FocusHandlerPtr superimpl = (wxOSX_FocusHandlerPtr) [[slf superclass] instanceMethodForSelector:(SEL)_cmd];
    // get the current focus before running becomeFirstResponder
    NSView* otherView = FindFocus();

    wxWidgetImpl* otherWindow = FindFromWXWidget(otherView);
    BOOL r = superimpl(slf, (SEL)_cmd);
    if ( r )
    {
        DoNotifyFocusEvent( true, otherWindow );
    }

    return r;
}

bool wxWidgetCocoaImpl::resignFirstResponder(WXWidget slf, void *_cmd)
{
    wxOSX_FocusHandlerPtr superimpl = (wxOSX_FocusHandlerPtr) [[slf superclass] instanceMethodForSelector:(SEL)_cmd];
    BOOL r = superimpl(slf, (SEL)_cmd);
    // get the current focus after running resignFirstResponder
    // note that this value isn't reliable, it might return the same view that
    // is resigning
    NSView* otherView = FindFocus();
    wxWidgetImpl* otherWindow = FindFromWXWidget(otherView);

    // It doesn't make sense to notify about the loss of focus if we're not
    // really losing it and the window which has just gained focus is the same
    // one as this window itself. Of course, this should never happen in the
    // first place but somehow it does in wxGrid code and without this check we
    // enter into an infinite recursion, see #12267.
    if ( otherWindow == this )
        return r;

    // NSTextViews have an editor as true responder, therefore the might get the
    // resign notification if their editor takes over, don't trigger any event then
    if ( r && !m_hasEditor)
    {
        DoNotifyFocusEvent( false, otherWindow );
    }
    return r;
}

void wxWidgetCocoaImpl::resetCursorRects(WXWidget slf, void *_cmd)
{
    wxWindow* wxpeer = GetWXPeer();
    if ( wxpeer )
    {
        NSCursor *cursor = (NSCursor*)wxpeer->GetCursor().GetHCURSOR();
        if (cursor == NULL)
        {
            wxOSX_ResetCursorRectsHandlerPtr superimpl = (wxOSX_ResetCursorRectsHandlerPtr) [[slf superclass] instanceMethodForSelector:(SEL)_cmd];
            superimpl(slf, (SEL)_cmd);
        }
        else
        {
            [slf addCursorRect: [slf bounds]
                cursor: cursor];
        }
    }
}

bool wxWidgetCocoaImpl::isFlipped(WXWidget WXUNUSED(slf), void *WXUNUSED(_cmd))
{
    return m_isFlipped;
}


#define OSX_DEBUG_DRAWING 0

void wxWidgetCocoaImpl::drawRect(void* rect, WXWidget slf, void *WXUNUSED(_cmd))
{
    // preparing the update region
    
    wxRegion updateRgn;
    const NSRect *rects;
    NSInteger count;

    [slf getRectsBeingDrawn:&rects count:&count];
    for ( int i = 0 ; i < count ; ++i )
    {
        updateRgn.Union(wxFromNSRect(slf, rects[i]));
    }

    wxWindow* wxpeer = GetWXPeer();

    if ( wxpeer->MacGetLeftBorderSize() != 0 || wxpeer->MacGetTopBorderSize() != 0 )
    {
        // as this update region is in native window locals we must adapt it to wx window local
        updateRgn.Offset( wxpeer->MacGetLeftBorderSize() , wxpeer->MacGetTopBorderSize() );
    }
    
    if ( wxpeer->MacGetTopLevelWindow()->GetWindowStyle() & wxFRAME_SHAPED )
    {
        int xoffset = 0, yoffset = 0;
        wxRegion rgn = wxpeer->MacGetTopLevelWindow()->GetShape();
        wxpeer->MacRootWindowToWindow( &xoffset, &yoffset );
        rgn.Offset( xoffset, yoffset );
        updateRgn.Intersect(rgn);
    }
    
    wxpeer->GetUpdateRegion() = updateRgn;

    // setting up the drawing context
    
    CGContextRef context = (CGContextRef) [[NSGraphicsContext currentContext] graphicsPort];
    CGContextSaveGState( context );
    
#if OSX_DEBUG_DRAWING
    CGContextBeginPath( context );
    CGContextMoveToPoint(context, 0, 0);
    NSRect bounds = [slf bounds];
    CGContextAddLineToPoint(context, 10, 0);
    CGContextMoveToPoint(context, 0, 0);
    CGContextAddLineToPoint(context, 0, 10);
    CGContextMoveToPoint(context, bounds.size.width, bounds.size.height);
    CGContextAddLineToPoint(context, bounds.size.width, bounds.size.height-10);
    CGContextMoveToPoint(context, bounds.size.width, bounds.size.height);
    CGContextAddLineToPoint(context, bounds.size.width-10, bounds.size.height);
    CGContextClosePath( context );
    CGContextStrokePath(context);
#endif
    
    if ( !m_isFlipped )
    {
        CGContextTranslateCTM( context, 0,  [m_osxView bounds].size.height );
        CGContextScaleCTM( context, 1, -1 );
    }
    
    wxpeer->MacSetCGContextRef( context );

    bool handled = wxpeer->MacDoRedraw( 0 );
    CGContextRestoreGState( context );

    CGContextSaveGState( context );
    if ( !handled )
    {
        // call super
        SEL _cmd = @selector(drawRect:);
        wxOSX_DrawRectHandlerPtr superimpl = (wxOSX_DrawRectHandlerPtr) [[slf superclass] instanceMethodForSelector:_cmd];
        superimpl(slf, _cmd, *(NSRect*)rect);
        CGContextRestoreGState( context );
        CGContextSaveGState( context );
    }
    // as we called restore above, we have to flip again if necessary
    if ( !m_isFlipped )
    {
        CGContextTranslateCTM( context, 0,  [m_osxView bounds].size.height );
        CGContextScaleCTM( context, 1, -1 );
    }
    wxpeer->MacPaintChildrenBorders();
    wxpeer->MacSetCGContextRef( NULL );
    CGContextRestoreGState( context );
}

void wxWidgetCocoaImpl::controlAction( WXWidget WXUNUSED(slf), void *WXUNUSED(_cmd), void *WXUNUSED(sender))
{
    wxWindow* wxpeer = (wxWindow*) GetWXPeer();
    if ( wxpeer )
        wxpeer->OSXHandleClicked(0);
}

void wxWidgetCocoaImpl::controlDoubleAction( WXWidget WXUNUSED(slf), void *WXUNUSED(_cmd), void *WXUNUSED(sender))
{
}

void wxWidgetCocoaImpl::controlTextDidChange()
{
    wxWindow* wxpeer = (wxWindow*)GetWXPeer();
    if ( wxpeer ) 
    {
        // since native rtti doesn't have to be enabled and wx' rtti is not aware of the mixin wxTextEntry, workaround is needed
        wxTextCtrl *tc = wxDynamicCast( wxpeer , wxTextCtrl );
        wxComboBox *cb = wxDynamicCast( wxpeer , wxComboBox );
        if ( tc )
            tc->SendTextUpdatedEventIfAllowed();
        else if ( cb )
            cb->SendTextUpdatedEventIfAllowed();
        else 
        {
            wxFAIL_MSG("Unexpected class for controlTextDidChange event");
        }
    }
}

//

#if OBJC_API_VERSION >= 2

#define wxOSX_CLASS_ADD_METHOD( c, s, i, t ) \
    class_addMethod(c, s, i, t );

#else

#define wxOSX_CLASS_ADD_METHOD( c, s, i, t ) \
    { s, (char*) t, i },

#endif

void wxOSXCocoaClassAddWXMethods(Class c)
{

#if OBJC_API_VERSION < 2
    static objc_method wxmethods[] =
    {
#endif

    wxOSX_CLASS_ADD_METHOD(c, @selector(mouseDown:), (IMP) wxOSX_mouseEvent, "v@:@" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(rightMouseDown:), (IMP) wxOSX_mouseEvent, "v@:@" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(otherMouseDown:), (IMP) wxOSX_mouseEvent, "v@:@" )

    wxOSX_CLASS_ADD_METHOD(c, @selector(mouseUp:), (IMP) wxOSX_mouseEvent, "v@:@" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(rightMouseUp:), (IMP) wxOSX_mouseEvent, "v@:@" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(otherMouseUp:), (IMP) wxOSX_mouseEvent, "v@:@" )

    wxOSX_CLASS_ADD_METHOD(c, @selector(mouseMoved:), (IMP) wxOSX_mouseEvent, "v@:@" )

    wxOSX_CLASS_ADD_METHOD(c, @selector(mouseDragged:), (IMP) wxOSX_mouseEvent, "v@:@" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(rightMouseDragged:), (IMP) wxOSX_mouseEvent, "v@:@" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(otherMouseDragged:), (IMP) wxOSX_mouseEvent, "v@:@" )
    
    wxOSX_CLASS_ADD_METHOD(c, @selector(acceptsFirstMouse:), (IMP) wxOSX_acceptsFirstMouse, "v@:@" )

    wxOSX_CLASS_ADD_METHOD(c, @selector(scrollWheel:), (IMP) wxOSX_mouseEvent, "v@:@" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(mouseEntered:), (IMP) wxOSX_mouseEvent, "v@:@" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(mouseExited:), (IMP) wxOSX_mouseEvent, "v@:@" )

    wxOSX_CLASS_ADD_METHOD(c, @selector(keyDown:), (IMP) wxOSX_keyEvent, "v@:@" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(keyUp:), (IMP) wxOSX_keyEvent, "v@:@" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(flagsChanged:), (IMP) wxOSX_keyEvent, "v@:@" )

    wxOSX_CLASS_ADD_METHOD(c, @selector(insertText:), (IMP) wxOSX_insertText, "v@:@" )

    wxOSX_CLASS_ADD_METHOD(c, @selector(performKeyEquivalent:), (IMP) wxOSX_performKeyEquivalent, "c@:@" )

    wxOSX_CLASS_ADD_METHOD(c, @selector(acceptsFirstResponder), (IMP) wxOSX_acceptsFirstResponder, "c@:" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(becomeFirstResponder), (IMP) wxOSX_becomeFirstResponder, "c@:" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(resignFirstResponder), (IMP) wxOSX_resignFirstResponder, "c@:" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(resetCursorRects), (IMP) wxOSX_resetCursorRects, "v@:" )

    wxOSX_CLASS_ADD_METHOD(c, @selector(isFlipped), (IMP) wxOSX_isFlipped, "c@:" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(drawRect:), (IMP) wxOSX_drawRect, "v@:{_NSRect={_NSPoint=ff}{_NSSize=ff}}" )

    wxOSX_CLASS_ADD_METHOD(c, @selector(controlAction:), (IMP) wxOSX_controlAction, "v@:@" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(controlDoubleAction:), (IMP) wxOSX_controlDoubleAction, "v@:@" )

#if wxUSE_DRAG_AND_DROP
    wxOSX_CLASS_ADD_METHOD(c, @selector(draggingEntered:), (IMP) wxOSX_draggingEntered, "I@:@" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(draggingUpdated:), (IMP) wxOSX_draggingUpdated, "I@:@" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(draggingExited:), (IMP) wxOSX_draggingExited, "v@:@" )
    wxOSX_CLASS_ADD_METHOD(c, @selector(performDragOperation:), (IMP) wxOSX_performDragOperation, "c@:@" )
#endif

#if OBJC_API_VERSION < 2
    } ;
    static int method_count = WXSIZEOF( wxmethods );
    static objc_method_list *wxmethodlist = NULL;
    if ( wxmethodlist == NULL )
    {
        wxmethodlist = (objc_method_list*) malloc(sizeof(objc_method_list) + sizeof(wxmethods) );
        memcpy( &wxmethodlist->method_list[0], &wxmethods[0], sizeof(wxmethods) );
        wxmethodlist->method_count = method_count;
        wxmethodlist->obsolete = 0;
    }
    class_addMethods( c, wxmethodlist );
#endif
}

//
// C++ implementation class
//

IMPLEMENT_DYNAMIC_CLASS( wxWidgetCocoaImpl , wxWidgetImpl )

wxWidgetCocoaImpl::wxWidgetCocoaImpl( wxWindowMac* peer , WXWidget w, bool isRootControl, bool isUserPane ) :
    wxWidgetImpl( peer, isRootControl, isUserPane )
{
    Init();
    m_osxView = w;

    // check if the user wants to create the control initially hidden
    if ( !peer->IsShown() )
        SetVisibility(false);

    // gc aware handling
    if ( m_osxView )
        CFRetain(m_osxView);
    [m_osxView release];
}

wxWidgetCocoaImpl::wxWidgetCocoaImpl()
{
    Init();
}

void wxWidgetCocoaImpl::Init()
{
    m_osxView = NULL;
    m_isFlipped = true;
    m_lastKeyDownEvent = NULL;
    m_hasEditor = false;
}

wxWidgetCocoaImpl::~wxWidgetCocoaImpl()
{
    RemoveAssociations( this );

    if ( !IsRootControl() )
    {
        NSView *sv = [m_osxView superview];
        if ( sv != nil )
            [m_osxView removeFromSuperview];
    }
    // gc aware handling
    if ( m_osxView )
        CFRelease(m_osxView);
}

bool wxWidgetCocoaImpl::IsVisible() const
{
    return [m_osxView isHiddenOrHasHiddenAncestor] == NO;
}

void wxWidgetCocoaImpl::SetVisibility( bool visible )
{
    [m_osxView setHidden:(visible ? NO:YES)];
}

// ----------------------------------------------------------------------------
// window animation stuff
// ----------------------------------------------------------------------------

// define a delegate used to refresh the window during animation
@interface wxNSAnimationDelegate : NSObject wxOSX_10_6_AND_LATER(<NSAnimationDelegate>)
{
    wxWindow *m_win;
    bool m_isDone;
}

- (id)init:(wxWindow *)win;

- (bool)isDone;

// NSAnimationDelegate methods
- (void)animationDidEnd:(NSAnimation*)animation;
- (void)animation:(NSAnimation*)animation
        didReachProgressMark:(NSAnimationProgress)progress;
@end

@implementation wxNSAnimationDelegate

- (id)init:(wxWindow *)win
{
    [super init];

    m_win = win;
    m_isDone = false;

    return self;
}

- (bool)isDone
{
    return m_isDone;
}

- (void)animation:(NSAnimation*)animation
        didReachProgressMark:(NSAnimationProgress)progress
{
    wxUnusedVar(animation);
    wxUnusedVar(progress);

    m_win->SendSizeEvent();
}

- (void)animationDidEnd:(NSAnimation*)animation
{
    wxUnusedVar(animation);
    m_isDone = true;
}

@end

/* static */
bool
wxWidgetCocoaImpl::ShowViewOrWindowWithEffect(wxWindow *win,
                                              bool show,
                                              wxShowEffect effect,
                                              unsigned timeout)
{
    // create the dictionary describing the animation to perform on this view
    NSObject * const
        viewOrWin = static_cast<NSObject *>(win->OSXGetViewOrWindow());
    NSMutableDictionary * const
        dict = [NSMutableDictionary dictionaryWithCapacity:4];
    [dict setObject:viewOrWin forKey:NSViewAnimationTargetKey];

    // determine the start and end rectangles assuming we're hiding the window
    const wxRect rectOrig = win->GetRect();
    wxRect rectStart,
           rectEnd;
    rectStart =
    rectEnd = rectOrig;

    if ( show )
    {
        if ( effect == wxSHOW_EFFECT_ROLL_TO_LEFT ||
                effect == wxSHOW_EFFECT_SLIDE_TO_LEFT )
            effect = wxSHOW_EFFECT_ROLL_TO_RIGHT;
        else if ( effect == wxSHOW_EFFECT_ROLL_TO_RIGHT ||
                    effect == wxSHOW_EFFECT_SLIDE_TO_RIGHT )
            effect = wxSHOW_EFFECT_ROLL_TO_LEFT;
        else if ( effect == wxSHOW_EFFECT_ROLL_TO_TOP ||
                    effect == wxSHOW_EFFECT_SLIDE_TO_TOP )
            effect = wxSHOW_EFFECT_ROLL_TO_BOTTOM;
        else if ( effect == wxSHOW_EFFECT_ROLL_TO_BOTTOM ||
                    effect == wxSHOW_EFFECT_SLIDE_TO_BOTTOM )
            effect = wxSHOW_EFFECT_ROLL_TO_TOP;
    }

    switch ( effect )
    {
        case wxSHOW_EFFECT_ROLL_TO_LEFT:
        case wxSHOW_EFFECT_SLIDE_TO_LEFT:
            rectEnd.width = 0;
            break;

        case wxSHOW_EFFECT_ROLL_TO_RIGHT:
        case wxSHOW_EFFECT_SLIDE_TO_RIGHT:
            rectEnd.x = rectStart.GetRight();
            rectEnd.width = 0;
            break;

        case wxSHOW_EFFECT_ROLL_TO_TOP:
        case wxSHOW_EFFECT_SLIDE_TO_TOP:
            rectEnd.height = 0;
            break;

        case wxSHOW_EFFECT_ROLL_TO_BOTTOM:
        case wxSHOW_EFFECT_SLIDE_TO_BOTTOM:
            rectEnd.y = rectStart.GetBottom();
            rectEnd.height = 0;
            break;

        case wxSHOW_EFFECT_EXPAND:
            rectEnd.x = rectStart.x + rectStart.width / 2;
            rectEnd.y = rectStart.y + rectStart.height / 2;
            rectEnd.width =
            rectEnd.height = 0;
            break;

        case wxSHOW_EFFECT_BLEND:
            [dict setObject:(show ? NSViewAnimationFadeInEffect
                                  : NSViewAnimationFadeOutEffect)
                  forKey:NSViewAnimationEffectKey];
            break;

        case wxSHOW_EFFECT_NONE:
        case wxSHOW_EFFECT_MAX:
            wxFAIL_MSG( "unexpected animation effect" );
            return false;

        default:
            wxFAIL_MSG( "unknown animation effect" );
            return false;
    };

    if ( show )
    {
        // we need to restore it to the original rectangle instead of making it
        // disappear
        wxSwap(rectStart, rectEnd);

        // and as the window is currently hidden, we need to show it for the
        // animation to be visible at all (but don't restore it at its full
        // rectangle as it shouldn't appear immediately)
        win->SetSize(rectStart);
        win->Show();
    }

    NSView * const parentView = [viewOrWin isKindOfClass:[NSView class]]
                                    ? [(NSView *)viewOrWin superview]
                                    : nil;
    const NSRect rStart = wxToNSRect(parentView, rectStart);
    const NSRect rEnd = wxToNSRect(parentView, rectEnd);

    [dict setObject:[NSValue valueWithRect:rStart]
          forKey:NSViewAnimationStartFrameKey];
    [dict setObject:[NSValue valueWithRect:rEnd]
          forKey:NSViewAnimationEndFrameKey];

    // create an animation using the values in the above dictionary
    NSViewAnimation * const
        anim = [[NSViewAnimation alloc]
                initWithViewAnimations:[NSArray arrayWithObject:dict]];

    if ( !timeout )
    {
        // what is a good default duration? Windows uses 200ms, Web frameworks
        // use anything from 250ms to 1s... choose something in the middle
        timeout = 500;
    }

    [anim setDuration:timeout/1000.];   // duration is in seconds here

    // if the window being animated changes its layout depending on its size
    // (which is almost always the case) we need to redo it during animation
    //
    // the number of layouts here is arbitrary, but 10 seems like too few (e.g.
    // controls in wxInfoBar visibly jump around)
    const int NUM_LAYOUTS = 20;
    for ( float f = 1./NUM_LAYOUTS; f < 1.; f += 1./NUM_LAYOUTS )
        [anim addProgressMark:f];

    wxNSAnimationDelegate * const
        animDelegate = [[wxNSAnimationDelegate alloc] init:win];
    [anim setDelegate:animDelegate];
    [anim startAnimation];

    // Cocoa is capable of doing animation asynchronously or even from separate
    // thread but wx API doesn't provide any way to be notified about the
    // animation end and without this we really must ensure that the window has
    // the expected (i.e. the same as if a simple Show() had been used) size
    // when we return, so block here until the animation finishes
    //
    // notice that because the default animation mode is NSAnimationBlocking,
    // no user input events ought to be processed from here
    {
        wxEventLoopGuarantor ensureEventLoopExistence;
        wxEventLoopBase * const loop = wxEventLoopBase::GetActive();
        while ( ![animDelegate isDone] )
            loop->Dispatch();
    }

    if ( !show )
    {
        // NSViewAnimation is smart enough to hide the NSView being animated at
        // the end but we also must ensure that it's hidden for wx too
        win->Hide();

        // and we must also restore its size because it isn't expected to
        // change just because the window was hidden
        win->SetSize(rectOrig);
    }
    else
    {
        // refresh it once again after the end to ensure that everything is in
        // place
        win->SendSizeEvent();
    }

    [anim setDelegate:nil];
    [animDelegate release];
    [anim release];

    return true;
}

bool wxWidgetCocoaImpl::ShowWithEffect(bool show,
                                       wxShowEffect effect,
                                       unsigned timeout)
{
    return ShowViewOrWindowWithEffect(m_wxPeer, show, effect, timeout);
}

void wxWidgetCocoaImpl::Raise()
{
    // Not implemented
}

void wxWidgetCocoaImpl::Lower()
{
    // Not implemented
}

void wxWidgetCocoaImpl::ScrollRect( const wxRect *WXUNUSED(rect), int WXUNUSED(dx), int WXUNUSED(dy) )
{
#if 1
    SetNeedsDisplay() ;
#else
    // We should do something like this, but it wasn't working in 10.4.
    if (GetNeedsDisplay() )
    {
        SetNeedsDisplay() ;
    }
    NSRect r = wxToNSRect( [m_osxView superview], *rect );
    NSSize offset = NSMakeSize((float)dx, (float)dy);
    [m_osxView scrollRect:r by:offset];
#endif
}

void wxWidgetCocoaImpl::Move(int x, int y, int width, int height)
{
    wxWindowMac* parent = GetWXPeer()->GetParent();
    // under Cocoa we might have a contentView in the wxParent to which we have to
    // adjust the coordinates
    if (parent && [m_osxView superview] != parent->GetHandle() )
    {
        int cx = 0,cy = 0,cw = 0,ch = 0;
        if ( parent->GetPeer() )
        {
            parent->GetPeer()->GetContentArea(cx, cy, cw, ch);
            x -= cx;
            y -= cy;
        }
    }
    [[m_osxView superview] setNeedsDisplayInRect:[m_osxView frame]];
    NSRect r = wxToNSRect( [m_osxView superview], wxRect(x,y,width, height) );
    [m_osxView setFrame:r];
    [[m_osxView superview] setNeedsDisplayInRect:r];

    wxNSView* wxview = (wxNSView*)m_osxView;
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
    if ([wxview respondsToSelector:@selector(updateTrackingArea)] )
        [wxview updateTrackingArea]; 
#else
    if ([m_osxView respondsToSelector:@selector(trackingTag)] )
    {
        if ( [wxview trackingTag] )
            [wxview removeTrackingRect: [wxview trackingTag]];

        [wxview setTrackingTag: [wxview addTrackingRect: [m_osxView bounds] owner: wxview userData: nil assumeInside: NO]];
    }
#endif
}

void wxWidgetCocoaImpl::GetPosition( int &x, int &y ) const
{
    wxRect r = wxFromNSRect( [m_osxView superview], [m_osxView frame] );
    x = r.GetLeft();
    y = r.GetTop();
    
    // under Cocoa we might have a contentView in the wxParent to which we have to
    // adjust the coordinates
    wxWindowMac* parent = GetWXPeer()->GetParent();
    if (parent && [m_osxView superview] != parent->GetHandle() )
    {
        int cx = 0,cy = 0,cw = 0,ch = 0;
        if ( parent->GetPeer() )
        {
            parent->GetPeer()->GetContentArea(cx, cy, cw, ch);
            x += cx;
            y += cy;
        }
    }
}

void wxWidgetCocoaImpl::GetSize( int &width, int &height ) const
{
    NSRect rect = [m_osxView frame];
    width = (int)rect.size.width;
    height = (int)rect.size.height;
}

void wxWidgetCocoaImpl::GetContentArea( int&left, int &top, int &width, int &height ) const
{
    if ( [m_osxView respondsToSelector:@selector(contentView) ] )
    {
        NSView* cv = [m_osxView contentView];

        NSRect bounds = [m_osxView bounds];
        NSRect rect = [cv frame];

        int y = (int)rect.origin.y;
        int x = (int)rect.origin.x;
        if ( ![ m_osxView isFlipped ] )
            y = (int)(bounds.size.height - (rect.origin.y + rect.size.height));
        left = x;
        top = y;
        width = (int)rect.size.width;
        height = (int)rect.size.height;
    }
    else
    {
        left = top = 0;
        GetSize( width, height );
    }
}

void wxWidgetCocoaImpl::SetNeedsDisplay( const wxRect* where )
{
    if ( where )
        [m_osxView setNeedsDisplayInRect:wxToNSRect(m_osxView, *where )];
    else
        [m_osxView setNeedsDisplay:YES];
}

bool wxWidgetCocoaImpl::GetNeedsDisplay() const
{
    return [m_osxView needsDisplay];
}

bool wxWidgetCocoaImpl::CanFocus() const
{
    return [m_osxView canBecomeKeyView] == YES;
}

bool wxWidgetCocoaImpl::HasFocus() const
{
    return ( FindFocus() == m_osxView );
}

bool wxWidgetCocoaImpl::SetFocus()
{
    if ( !CanFocus() )
        return false;

    [[m_osxView window] makeKeyAndOrderFront:nil] ;
    [[m_osxView window] makeFirstResponder: m_osxView] ;
    return true;
}


void wxWidgetCocoaImpl::RemoveFromParent()
{
    [m_osxView removeFromSuperview];
}

void wxWidgetCocoaImpl::Embed( wxWidgetImpl *parent )
{
    NSView* container = parent->GetWXWidget() ;
    wxASSERT_MSG( container != NULL , wxT("No valid mac container control") ) ;
    [container addSubview:m_osxView];
}

void wxWidgetCocoaImpl::SetBackgroundColour( const wxColour &col )
{
    NSView* targetView = m_osxView;
    if ( [m_osxView isKindOfClass:[NSScrollView class] ] )
        targetView = [(NSScrollView*) m_osxView documentView];

    if ( [targetView respondsToSelector:@selector(setBackgroundColor:) ] )
    {
        [targetView setBackgroundColor:[NSColor colorWithCalibratedRed:(CGFloat) (col.Red() / 255.0)
                                                                green:(CGFloat) (col.Green() / 255.0)
                                                                 blue:(CGFloat) (col.Blue() / 255.0)
                                                                alpha:(CGFloat) (col.Alpha() / 255.0)]];
    }
}

bool wxWidgetCocoaImpl::SetBackgroundStyle( wxBackgroundStyle style )
{
    BOOL opaque = ( style == wxBG_STYLE_PAINT );
    
    if ( [m_osxView respondsToSelector:@selector(setOpaque:) ] )
    {
        [m_osxView setOpaque: opaque];
    }
    
    return true ;
}

void wxWidgetCocoaImpl::SetLabel( const wxString& title, wxFontEncoding encoding )
{
    if ( [m_osxView respondsToSelector:@selector(setTitle:) ] )
    {
        wxCFStringRef cf( title , encoding );
        [m_osxView setTitle:cf.AsNSString()];
    }
    else if ( [m_osxView respondsToSelector:@selector(setStringValue:) ] )
    {
        wxCFStringRef cf( title , encoding );
        [m_osxView setStringValue:cf.AsNSString()];
    }
}


void  wxWidgetImpl::Convert( wxPoint *pt , wxWidgetImpl *from , wxWidgetImpl *to )
{
    NSPoint p = wxToNSPoint( from->GetWXWidget(), *pt );
    p = [from->GetWXWidget() convertPoint:p toView:to->GetWXWidget() ];
    *pt = wxFromNSPoint( to->GetWXWidget(), p );
}

wxInt32 wxWidgetCocoaImpl::GetValue() const
{
    return [(NSControl*)m_osxView intValue];
}

void wxWidgetCocoaImpl::SetValue( wxInt32 v )
{
    if (  [m_osxView respondsToSelector:@selector(setIntValue:)] )
    {
        [m_osxView setIntValue:v];
    }
    else if (  [m_osxView respondsToSelector:@selector(setFloatValue:)] )
    {
        [m_osxView setFloatValue:(double)v];
    }
    else if (  [m_osxView respondsToSelector:@selector(setDoubleValue:)] )
    {
        [m_osxView setDoubleValue:(double)v];
    }
}

void wxWidgetCocoaImpl::SetMinimum( wxInt32 v )
{
    if (  [m_osxView respondsToSelector:@selector(setMinValue:)] )
    {
        [m_osxView setMinValue:(double)v];
    }
}

void wxWidgetCocoaImpl::SetMaximum( wxInt32 v )
{
    if (  [m_osxView respondsToSelector:@selector(setMaxValue:)] )
    {
        [m_osxView setMaxValue:(double)v];
    }
}

wxInt32 wxWidgetCocoaImpl::GetMinimum() const
{
    if (  [m_osxView respondsToSelector:@selector(minValue)] )
    {
        return (int)[m_osxView minValue];
    }
    return 0;
}

wxInt32 wxWidgetCocoaImpl::GetMaximum() const
{
    if (  [m_osxView respondsToSelector:@selector(maxValue)] )
    {
        return (int)[m_osxView maxValue];
    }
    return 0;
}

wxBitmap wxWidgetCocoaImpl::GetBitmap() const
{
    wxBitmap bmp;

    // TODO: how to create a wxBitmap from NSImage?
#if 0
    if ( [m_osxView respondsToSelector:@selector(image:)] )
        bmp = [m_osxView image];
#endif

    return bmp;
}

void wxWidgetCocoaImpl::SetBitmap( const wxBitmap& bitmap )
{
    if (  [m_osxView respondsToSelector:@selector(setImage:)] )
    {
        [m_osxView setImage:bitmap.GetNSImage()];
        [m_osxView setNeedsDisplay:YES];
    }
}

void wxWidgetCocoaImpl::SetBitmapPosition( wxDirection dir )
{
    if ( [m_osxView respondsToSelector:@selector(setImagePosition:)] )
    {
        NSCellImagePosition pos;
        switch ( dir )
        {
            case wxLEFT:
                pos = NSImageLeft;
                break;

            case wxRIGHT:
                pos = NSImageRight;
                break;

            case wxTOP:
                pos = NSImageAbove;
                break;

            case wxBOTTOM:
                pos = NSImageBelow;
                break;

            default:
                wxFAIL_MSG( "invalid image position" );
                pos = NSNoImage;
        }

        [m_osxView setImagePosition:pos];
    }
}

void wxWidgetCocoaImpl::SetupTabs( const wxNotebook& WXUNUSED(notebook))
{
    // implementation in subclass
}

void wxWidgetCocoaImpl::GetBestRect( wxRect *r ) const
{
    r->x = r->y = r->width = r->height = 0;

    if (  [m_osxView respondsToSelector:@selector(sizeToFit)] )
    {
        NSRect former = [m_osxView frame];
        [m_osxView sizeToFit];
        NSRect best = [m_osxView frame];
        [m_osxView setFrame:former];
        r->width = (int)best.size.width;
        r->height = (int)best.size.height;
    }
}

bool wxWidgetCocoaImpl::IsEnabled() const
{
    NSView* targetView = m_osxView;
    if ( [m_osxView isKindOfClass:[NSScrollView class] ] )
        targetView = [(NSScrollView*) m_osxView documentView];

    if ( [targetView respondsToSelector:@selector(isEnabled) ] )
        return [targetView isEnabled];
    return true;
}

void wxWidgetCocoaImpl::Enable( bool enable )
{
    NSView* targetView = m_osxView;
    if ( [m_osxView isKindOfClass:[NSScrollView class] ] )
        targetView = [(NSScrollView*) m_osxView documentView];

    if ( [targetView respondsToSelector:@selector(setEnabled:) ] )
        [targetView setEnabled:enable];
}

void wxWidgetCocoaImpl::PulseGauge()
{
}

void wxWidgetCocoaImpl::SetScrollThumb( wxInt32 WXUNUSED(val), wxInt32 WXUNUSED(view) )
{
}

void wxWidgetCocoaImpl::SetControlSize( wxWindowVariant variant )
{
    NSControlSize size = NSRegularControlSize;

    switch ( variant )
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
            wxFAIL_MSG(wxT("unexpected window variant"));
            break ;
    }
    if ( [m_osxView respondsToSelector:@selector(setControlSize:)] )
        [m_osxView setControlSize:size];
    else if ([m_osxView respondsToSelector:@selector(cell)])
    {
        id cell = [(id)m_osxView cell];
        if ([cell respondsToSelector:@selector(setControlSize:)])
            [cell setControlSize:size];
    }
}

void wxWidgetCocoaImpl::SetFont(wxFont const& font, wxColour const&col, long, bool)
{
    if ([m_osxView respondsToSelector:@selector(setFont:)])
        [m_osxView setFont: font.OSXGetNSFont()];
    if ([m_osxView respondsToSelector:@selector(setTextColor:)])
        [m_osxView setTextColor:[NSColor colorWithCalibratedRed:(CGFloat) (col.Red() / 255.0)
                                                                 green:(CGFloat) (col.Green() / 255.0)
                                                                  blue:(CGFloat) (col.Blue() / 255.0)
                                                                 alpha:(CGFloat) (col.Alpha() / 255.0)]];
}

void wxWidgetCocoaImpl::SetToolTip(wxToolTip* tooltip)
{
    if (tooltip)
    {
        wxCFStringRef cf( tooltip->GetTip() , m_wxPeer->GetFont().GetEncoding() );
        [m_osxView setToolTip: cf.AsNSString()];
    }
    else 
        [m_osxView setToolTip: nil];

}

void wxWidgetCocoaImpl::InstallEventHandler( WXWidget control )
{
    WXWidget c =  control ? control : (WXWidget) m_osxView;
    wxWidgetImpl::Associate( c, this ) ;
    if ([c respondsToSelector:@selector(setAction:)])
    {
        [c setTarget: c];
        [c setAction: @selector(controlAction:)];
        if ([c respondsToSelector:@selector(setDoubleAction:)])
        {
            [c setDoubleAction: @selector(controlDoubleAction:)];
        }

    }
}

bool wxWidgetCocoaImpl::DoHandleCharEvent(NSEvent *event, NSString *text)
{
    wxKeyEvent wxevent(wxEVT_CHAR);
    SetupKeyEvent( wxevent, event, text );

    return GetWXPeer()->OSXHandleKeyEvent(wxevent);
}

bool wxWidgetCocoaImpl::DoHandleKeyEvent(NSEvent *event)
{
    wxKeyEvent wxevent(wxEVT_KEY_DOWN);
    SetupKeyEvent( wxevent, event );
    bool result = GetWXPeer()->OSXHandleKeyEvent(wxevent);

    // this will fire higher level events, like insertText, to help
    // us handle EVT_CHAR, etc.

    if ( IsUserPane() && [event type] == NSKeyDown)
    {
        if ( !result )
        {
            if ( wxevent.GetKeyCode() < WXK_SPACE || wxevent.GetKeyCode() == WXK_DELETE || wxevent.GetKeyCode() >= WXK_START )
            {
                // eventually we could setup a doCommandBySelector catcher and retransform this into the wx key chars
                wxKeyEvent wxevent2(wxevent) ;
                wxevent2.SetEventType(wxEVT_CHAR);
                result = GetWXPeer()->OSXHandleKeyEvent(wxevent2);
            }
            else
            {
                if ( !wxevent.CmdDown() )
                {
                    if ( [m_osxView isKindOfClass:[NSScrollView class] ] )
                        [[(NSScrollView*)m_osxView documentView] interpretKeyEvents:[NSArray arrayWithObject:event]];
                    else
                        [m_osxView interpretKeyEvents:[NSArray arrayWithObject:event]];
                    result = true;
                }
            }
        }
    }

    return result;
}

bool wxWidgetCocoaImpl::DoHandleMouseEvent(NSEvent *event)
{
    wxMouseEvent wxevent(wxEVT_LEFT_DOWN);
    SetupMouseEvent(wxevent , event) ;

    return GetWXPeer()->HandleWindowEvent(wxevent);
}

void wxWidgetCocoaImpl::DoNotifyFocusEvent(bool receivedFocus, wxWidgetImpl* otherWindow)
{
    wxWindow* thisWindow = GetWXPeer();
    if ( thisWindow->MacGetTopLevelWindow() && NeedsFocusRect() )
    {
        thisWindow->MacInvalidateBorders();
    }

    if ( receivedFocus )
    {
        wxLogTrace(wxT("Focus"), wxT("focus set(%p)"), static_cast<void*>(thisWindow));
        wxChildFocusEvent eventFocus((wxWindow*)thisWindow);
        thisWindow->HandleWindowEvent(eventFocus);

#if wxUSE_CARET
        if ( thisWindow->GetCaret() )
            thisWindow->GetCaret()->OnSetFocus();
#endif

        wxFocusEvent event(wxEVT_SET_FOCUS, thisWindow->GetId());
        event.SetEventObject(thisWindow);
        if (otherWindow)
            event.SetWindow(otherWindow->GetWXPeer());
        thisWindow->HandleWindowEvent(event) ;
    }
    else // !receivedFocuss
    {
#if wxUSE_CARET
        if ( thisWindow->GetCaret() )
            thisWindow->GetCaret()->OnKillFocus();
#endif

        wxLogTrace(wxT("Focus"), wxT("focus lost(%p)"), static_cast<void*>(thisWindow));

        wxFocusEvent event( wxEVT_KILL_FOCUS, thisWindow->GetId());
        event.SetEventObject(thisWindow);
        if (otherWindow)
            event.SetWindow(otherWindow->GetWXPeer());
        thisWindow->HandleWindowEvent(event) ;
    }
}

void wxWidgetCocoaImpl::SetCursor(const wxCursor& cursor)
{
    if ( !wxIsBusy() )
    {
        NSPoint location = [NSEvent mouseLocation];
        location = [[m_osxView window] convertScreenToBase:location];
        NSPoint locationInView = [m_osxView convertPoint:location fromView:nil];

        if( NSMouseInRect(locationInView, [m_osxView bounds], YES) )
        {
            [(NSCursor*)cursor.GetHCURSOR() set];
        }
    }
    [[m_osxView window] invalidateCursorRectsForView:m_osxView];
}

void wxWidgetCocoaImpl::CaptureMouse()
{
    [[m_osxView window] disableCursorRects];
}

void wxWidgetCocoaImpl::ReleaseMouse()
{
    [[m_osxView window] enableCursorRects];
}

void wxWidgetCocoaImpl::SetFlipped(bool flipped)
{
    m_isFlipped = flipped;
}

//
// Factory methods
//

wxWidgetImpl* wxWidgetImpl::CreateUserPane( wxWindowMac* wxpeer, wxWindowMac* WXUNUSED(parent),
    wxWindowID WXUNUSED(id), const wxPoint& pos, const wxSize& size,
    long WXUNUSED(style), long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxNSView* v = [[wxNSView alloc] initWithFrame:r];

    // temporary hook for dnd
    [v registerForDraggedTypes:[NSArray arrayWithObjects:
        NSStringPboardType, NSFilenamesPboardType, NSTIFFPboardType, NSPICTPboardType, NSPDFPboardType, nil]];

    wxWidgetCocoaImpl* c = new wxWidgetCocoaImpl( wxpeer, v, false, true );
    return c;
}

wxWidgetImpl* wxWidgetImpl::CreateContentView( wxNonOwnedWindow* now )
{
    NSWindow* tlw = now->GetWXWindow();
    
    wxWidgetCocoaImpl* c = NULL;
    if ( now->IsNativeWindowWrapper() )
    {
        NSView* cv = [tlw contentView];
        c = new wxWidgetCocoaImpl( now, cv, true );
        // increase ref count, because the impl destructor will decrement it again
        CFRetain(cv);
        if ( !now->IsShown() )
            [cv setHidden:NO];
        
    }
    else
    {
        wxNSView* v = [[wxNSView alloc] initWithFrame:[[tlw contentView] frame]];
        c = new wxWidgetCocoaImpl( now, v, true );
        c->InstallEventHandler();
        [tlw setContentView:v];
    }
    return c;
}
