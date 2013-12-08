/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/nonownedwnd.cpp
// Purpose:     implementation of wxNonOwnedWindow
// Author:      Stefan Csomor
// Created:     2008-03-24
// Copyright:   (c) Stefan Csomor 2008
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
#endif // WX_PRECOMP

#include "wx/hashmap.h"
#include "wx/evtloop.h"
#include "wx/tooltip.h"
#include "wx/nonownedwnd.h"

#include "wx/osx/private.h"
#include "wx/settings.h"
#include "wx/frame.h"

#if wxUSE_SYSTEM_OPTIONS
    #include "wx/sysopt.h"
#endif

// ============================================================================
// wxNonOwnedWindow implementation
// ============================================================================

// unified title and toolbar constant - not in Tiger headers, so we duplicate it here
#define kWindowUnifiedTitleAndToolbarAttribute (1 << 7)

IMPLEMENT_DYNAMIC_CLASS( wxNonOwnedWindowCarbonImpl , wxNonOwnedWindowImpl )

WXWindow wxNonOwnedWindowCarbonImpl::GetWXWindow() const
{
    return (WXWindow) m_macWindow;
}
void wxNonOwnedWindowCarbonImpl::Raise()
{
    ::SelectWindow( m_macWindow ) ;
}

void wxNonOwnedWindowCarbonImpl::Lower()
{
    ::SendBehind( m_macWindow , NULL ) ;
}

void wxNonOwnedWindowCarbonImpl::ShowWithoutActivating()
{
    bool plainTransition = true;

#if wxUSE_SYSTEM_OPTIONS
    if ( wxSystemOptions::HasOption(wxMAC_WINDOW_PLAIN_TRANSITION) )
        plainTransition = ( wxSystemOptions::GetOptionInt( wxMAC_WINDOW_PLAIN_TRANSITION ) == 1 ) ;
#endif

    if ( plainTransition )
       ::ShowWindow( (WindowRef)m_macWindow );
    else
       ::TransitionWindow( (WindowRef)m_macWindow, kWindowZoomTransitionEffect, kWindowShowTransitionAction, NULL );
}

bool wxNonOwnedWindowCarbonImpl::Show(bool show)
{
    bool plainTransition = true;

#if wxUSE_SYSTEM_OPTIONS
    if ( wxSystemOptions::HasOption(wxMAC_WINDOW_PLAIN_TRANSITION) )
        plainTransition = ( wxSystemOptions::GetOptionInt( wxMAC_WINDOW_PLAIN_TRANSITION ) == 1 ) ;
#endif

    if (show)
    {
        ShowWithoutActivating();
        ::SelectWindow( (WindowRef)m_macWindow ) ;
    }
    else
    {
#if wxOSX_USE_CARBON
        if ( plainTransition )
           ::HideWindow( (WindowRef)m_macWindow );
        else
           ::TransitionWindow( (WindowRef)m_macWindow, kWindowZoomTransitionEffect, kWindowHideTransitionAction, NULL );
#endif
    }
    return true;
}

void wxNonOwnedWindowCarbonImpl::Update()
{
    HIWindowFlush(m_macWindow) ;
}

bool wxNonOwnedWindowCarbonImpl::SetTransparent(wxByte alpha)
{
    OSStatus result = SetWindowAlpha((WindowRef)m_macWindow, (CGFloat)((alpha)/255.0));
    return result == noErr;
}

bool wxNonOwnedWindowCarbonImpl::SetBackgroundColour(const wxColour& col )
{
    if ( col == wxColour(wxMacCreateCGColorFromHITheme(kThemeBrushDocumentWindowBackground)) )
    {
        SetThemeWindowBackground( (WindowRef) m_macWindow,  kThemeBrushDocumentWindowBackground, false ) ;
        m_wxPeer->SetBackgroundStyle(wxBG_STYLE_SYSTEM);
        // call directly if object is not yet completely constructed
        if ( m_wxPeer->GetNonOwnedPeer() == NULL )
            SetBackgroundStyle(wxBG_STYLE_SYSTEM);
    }
    else if ( col == wxColour(wxMacCreateCGColorFromHITheme(kThemeBrushDialogBackgroundActive)) )
    {
        SetThemeWindowBackground( (WindowRef) m_macWindow,  kThemeBrushDialogBackgroundActive, false ) ;
        m_wxPeer->SetBackgroundStyle(wxBG_STYLE_SYSTEM);
        // call directly if object is not yet completely constructed
        if ( m_wxPeer->GetNonOwnedPeer() == NULL )
            SetBackgroundStyle(wxBG_STYLE_SYSTEM);
    }
    return true;
}

void wxNonOwnedWindowCarbonImpl::SetExtraStyle( long exStyle )
{
    if ( m_macWindow != NULL )
    {
        bool metal = exStyle & wxFRAME_EX_METAL ;

        if ( MacGetMetalAppearance() != metal )
        {
            if ( MacGetUnifiedAppearance() )
                MacSetUnifiedAppearance( !metal ) ;

            MacSetMetalAppearance( metal ) ;
        }
    }
}

bool wxNonOwnedWindowCarbonImpl::SetBackgroundStyle(wxBackgroundStyle style)
{
    if ( style == wxBG_STYLE_TRANSPARENT )
    {
        OSStatus err = HIWindowChangeFeatures( m_macWindow, 0, kWindowIsOpaque );
        verify_noerr( err );
        err = ReshapeCustomWindow( m_macWindow );
        verify_noerr( err );
    }

    return true ;
}

bool wxNonOwnedWindowCarbonImpl::CanSetTransparent()
{
    return true;
}

void wxNonOwnedWindowCarbonImpl::GetContentArea( int &left , int &top , int &width , int &height ) const
{
    Rect content, structure ;

    GetWindowBounds( m_macWindow, kWindowStructureRgn , &structure ) ;
    GetWindowBounds( m_macWindow, kWindowContentRgn , &content ) ;

    left = content.left - structure.left ;
    top = content.top  - structure.top ;
    width = content.right - content.left ;
    height = content.bottom - content.top ;
}

void wxNonOwnedWindowCarbonImpl::MoveWindow(int x, int y, int width, int height)
{
    Rect bounds = { y , x , y + height , x + width } ;
    verify_noerr(SetWindowBounds( (WindowRef) m_macWindow, kWindowStructureRgn , &bounds )) ;
}

void wxNonOwnedWindowCarbonImpl::GetPosition( int &x, int &y ) const
{
    Rect bounds ;

    verify_noerr(GetWindowBounds((WindowRef) m_macWindow, kWindowStructureRgn , &bounds )) ;

    x = bounds.left ;
    y = bounds.top ;
}

void wxNonOwnedWindowCarbonImpl::GetSize( int &width, int &height ) const
{
    Rect bounds ;

    verify_noerr(GetWindowBounds((WindowRef) m_macWindow, kWindowStructureRgn , &bounds )) ;

    width = bounds.right - bounds.left ;
    height = bounds.bottom - bounds.top ;
}

bool wxNonOwnedWindowCarbonImpl::MacGetUnifiedAppearance() const
{
    return MacGetWindowAttributes() & kWindowUnifiedTitleAndToolbarAttribute ;
}

void wxNonOwnedWindowCarbonImpl::MacChangeWindowAttributes( wxUint32 attributesToSet , wxUint32 attributesToClear )
{
    ChangeWindowAttributes( m_macWindow, attributesToSet, attributesToClear ) ;
}

wxUint32 wxNonOwnedWindowCarbonImpl::MacGetWindowAttributes() const
{
    UInt32 attr = 0 ;
    GetWindowAttributes( m_macWindow, &attr ) ;
    return attr ;
}

void wxNonOwnedWindowCarbonImpl::MacSetMetalAppearance( bool set )
{
    if ( MacGetUnifiedAppearance() )
        MacSetUnifiedAppearance( false ) ;

    MacChangeWindowAttributes( set ? kWindowMetalAttribute : kWindowNoAttributes ,
        set ? kWindowNoAttributes : kWindowMetalAttribute ) ;
}

bool wxNonOwnedWindowCarbonImpl::MacGetMetalAppearance() const
{
    return MacGetWindowAttributes() & kWindowMetalAttribute ;
}

void wxNonOwnedWindowCarbonImpl::MacSetUnifiedAppearance( bool set )
{
    if ( MacGetMetalAppearance() )
        MacSetMetalAppearance( false ) ;

    MacChangeWindowAttributes( set ? kWindowUnifiedTitleAndToolbarAttribute : kWindowNoAttributes ,
        set ? kWindowNoAttributes : kWindowUnifiedTitleAndToolbarAttribute) ;

    // For some reason, Tiger uses white as the background color for this appearance,
    // while most apps using it use the typical striped background. Restore that behaviour
    // for wx.
    // TODO: Determine if we need this on Leopard as well. (should be harmless either way,
    // though)
    // since when creating the peering is not yet completely set-up we call both setters
    // explicitly
    m_wxPeer->SetBackgroundColour( wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW) ) ;
    SetBackgroundColour( wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW) ) ;
}


// ----------------------------------------------------------------------------
// globals
// ----------------------------------------------------------------------------

static pascal long wxShapedMacWindowDef(short varCode, WindowRef window, SInt16 message, SInt32 param);

WXDLLEXPORT void SetupMouseEvent( wxMouseEvent &wxevent , wxMacCarbonEvent &cEvent );

// ---------------------------------------------------------------------------
// Carbon Events
// ---------------------------------------------------------------------------

static const EventTypeSpec eventList[] =
{
    // TODO: remove control related event like key and mouse (except for WindowLeave events)

    { kEventClassKeyboard, kEventRawKeyDown } ,
    { kEventClassKeyboard, kEventRawKeyRepeat } ,
    { kEventClassKeyboard, kEventRawKeyUp } ,
    { kEventClassKeyboard, kEventRawKeyModifiersChanged } ,

    { kEventClassTextInput, kEventTextInputUnicodeForKeyEvent } ,
    { kEventClassTextInput, kEventTextInputUpdateActiveInputArea } ,

    { kEventClassWindow , kEventWindowShown } ,
    { kEventClassWindow , kEventWindowActivated } ,
    { kEventClassWindow , kEventWindowDeactivated } ,
    { kEventClassWindow , kEventWindowBoundsChanging } ,
    { kEventClassWindow , kEventWindowBoundsChanged } ,
    { kEventClassWindow , kEventWindowClose } ,
    { kEventClassWindow , kEventWindowGetRegion } ,

    // we have to catch these events on the toplevel window level,
    // as controls don't get the raw mouse events anymore

    { kEventClassMouse , kEventMouseDown } ,
    { kEventClassMouse , kEventMouseUp } ,
    { kEventClassMouse , kEventMouseWheelMoved } ,
    { kEventClassMouse , kEventMouseMoved } ,
    { kEventClassMouse , kEventMouseDragged } ,
} ;

static pascal OSStatus KeyboardEventHandler( EventHandlerCallRef handler , EventRef event , void *data )
{
    OSStatus result = eventNotHandledErr ;
    // call DoFindFocus instead of FindFocus, because for Composite Windows(like WxGenericListCtrl)
    // FindFocus does not return the actual focus window, but the enclosing window
    wxWindow* focus = wxWindow::DoFindFocus();
    if ( focus == NULL )
        focus = data ? ((wxNonOwnedWindowImpl*) data)->GetWXPeer() : NULL ;

    unsigned char charCode ;
    wxChar uniChar[2] ;
    uniChar[0] = 0;
    uniChar[1] = 0;

    UInt32 keyCode ;
    UInt32 modifiers ;
    UInt32 when = EventTimeToTicks( GetEventTime( event ) ) ;

#if wxUSE_UNICODE
    ByteCount dataSize = 0 ;
    if ( GetEventParameter( event, kEventParamKeyUnicodes, typeUnicodeText, NULL, 0 , &dataSize, NULL ) == noErr )
    {
        UniChar buf[2] ;
        int numChars = dataSize / sizeof( UniChar) + 1;

        UniChar* charBuf = buf ;

        if ( numChars * 2 > 4 )
            charBuf = new UniChar[ numChars ] ;
        GetEventParameter( event, kEventParamKeyUnicodes, typeUnicodeText, NULL, dataSize , NULL , charBuf ) ;
        charBuf[ numChars - 1 ] = 0;

        wxMBConvUTF16 converter ;
        converter.MB2WC( uniChar , (const char*)charBuf , 2 ) ;

        if ( numChars * 2 > 4 )
            delete[] charBuf ;
    }
#endif // wxUSE_UNICODE

    GetEventParameter( event, kEventParamKeyMacCharCodes, typeChar, NULL, 1, NULL, &charCode );
    GetEventParameter( event, kEventParamKeyCode, typeUInt32, NULL, sizeof(UInt32), NULL, &keyCode );
    GetEventParameter( event, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(UInt32), NULL, &modifiers );

    UInt32 message = (keyCode << 8) + charCode;
    switch ( GetEventKind( event ) )
    {
        case kEventRawKeyRepeat :
        case kEventRawKeyDown :
            {
                WXEVENTREF formerEvent = wxTheApp->MacGetCurrentEvent() ;
                WXEVENTHANDLERCALLREF formerHandler = wxTheApp->MacGetCurrentEventHandlerCallRef() ;
                wxTheApp->MacSetCurrentEvent( event , handler ) ;
                if ( /* focus && */ wxTheApp->MacSendKeyDownEvent(
                    focus , message , modifiers , when , uniChar[0] ) )
                {
                    result = noErr ;
                }
                wxTheApp->MacSetCurrentEvent( formerEvent , formerHandler ) ;
            }
            break ;

        case kEventRawKeyUp :
            if ( /* focus && */ wxTheApp->MacSendKeyUpEvent(
                focus , message , modifiers , when , uniChar[0] ) )
            {
                result = noErr ;
            }
            break ;

        case kEventRawKeyModifiersChanged :
            {
                wxKeyEvent event(wxEVT_KEY_DOWN);

                event.m_shiftDown = modifiers & shiftKey;
                event.m_rawControlDown = modifiers & controlKey;
                event.m_altDown = modifiers & optionKey;
                event.m_controlDown = event.m_metaDown = modifiers & cmdKey;

#if wxUSE_UNICODE
                event.m_uniChar = uniChar[0] ;
#endif

                event.SetTimestamp(when);
                event.SetEventObject(focus);

                if ( /* focus && */ (modifiers ^ wxApp::s_lastModifiers ) & controlKey )
                {
                    event.m_keyCode = WXK_RAW_CONTROL  ;
                    event.SetEventType( ( modifiers & controlKey ) ? wxEVT_KEY_DOWN : wxEVT_KEY_UP ) ;
                    focus->HandleWindowEvent( event ) ;
                }
                if ( /* focus && */ (modifiers ^ wxApp::s_lastModifiers ) & shiftKey )
                {
                    event.m_keyCode = WXK_SHIFT ;
                    event.SetEventType( ( modifiers & shiftKey ) ? wxEVT_KEY_DOWN : wxEVT_KEY_UP ) ;
                    focus->HandleWindowEvent( event ) ;
                }
                if ( /* focus && */ (modifiers ^ wxApp::s_lastModifiers ) & optionKey )
                {
                    event.m_keyCode = WXK_ALT ;
                    event.SetEventType( ( modifiers & optionKey ) ? wxEVT_KEY_DOWN : wxEVT_KEY_UP ) ;
                    focus->HandleWindowEvent( event ) ;
                }
                if ( /* focus && */ (modifiers ^ wxApp::s_lastModifiers ) & cmdKey )
                {
                    event.m_keyCode = WXK_CONTROL;
                    event.SetEventType( ( modifiers & cmdKey ) ? wxEVT_KEY_DOWN : wxEVT_KEY_UP ) ;
                    focus->HandleWindowEvent( event ) ;
                }

                wxApp::s_lastModifiers = modifiers ;
            }
            break ;

        default:
            break;
    }

    return result ;
}

// we don't interfere with foreign controls on our toplevel windows, therefore we always give back eventNotHandledErr
// for windows that we didn't create (like eg Scrollbars in a databrowser), or for controls where we did not handle the
// mouse down at all
//
// This handler can also be called from app level where data (ie target window) may be null or a non wx window

EventMouseButton g_lastButton = 0 ;
bool g_lastButtonWasFakeRight = false ;

WXDLLEXPORT void SetupMouseEvent( wxMouseEvent &wxevent , wxMacCarbonEvent &cEvent )
{
    UInt32 modifiers = cEvent.GetParameter<UInt32>(kEventParamKeyModifiers, typeUInt32) ;
    Point screenMouseLocation = cEvent.GetParameter<Point>(kEventParamMouseLocation) ;

    // these parameters are not given for all events
    EventMouseButton button = 0 ;
    UInt32 clickCount = 0 ;
    UInt32 mouseChord = 0;

    cEvent.GetParameter<EventMouseButton>( kEventParamMouseButton, typeMouseButton , &button ) ;
    cEvent.GetParameter<UInt32>( kEventParamClickCount, typeUInt32 , &clickCount ) ;
    // the chord is the state of the buttons pressed currently
    cEvent.GetParameter<UInt32>( kEventParamMouseChord, typeUInt32 , &mouseChord ) ;

    wxevent.m_x = screenMouseLocation.h;
    wxevent.m_y = screenMouseLocation.v;
    wxevent.m_shiftDown = modifiers & shiftKey;
    wxevent.m_rawControlDown = modifiers & controlKey;
    wxevent.m_altDown = modifiers & optionKey;
    wxevent.m_controlDown = wxevent.m_metaDown = modifiers & cmdKey;
    wxevent.m_clickCount = clickCount;
    wxevent.SetTimestamp( cEvent.GetTicks() ) ;

    // a control click is interpreted as a right click
    bool thisButtonIsFakeRight = false ;
    if ( button == kEventMouseButtonPrimary && (modifiers & controlKey) )
    {
        button = kEventMouseButtonSecondary ;
        thisButtonIsFakeRight = true ;
    }

    // otherwise we report double clicks by connecting a left click with a ctrl-left click
    if ( clickCount > 1 && button != g_lastButton )
        clickCount = 1 ;

    // we must make sure that our synthetic 'right' button corresponds in
    // mouse down, moved and mouse up, and does not deliver a right down and left up

    if ( cEvent.GetKind() == kEventMouseDown )
    {
        g_lastButton = button ;
        g_lastButtonWasFakeRight = thisButtonIsFakeRight ;
    }

    if ( button == 0 )
    {
        g_lastButton = 0 ;
        g_lastButtonWasFakeRight = false ;
    }
    else if ( g_lastButton == kEventMouseButtonSecondary && g_lastButtonWasFakeRight )
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
    switch ( cEvent.GetKind() )
    {
        case kEventMouseDown :
            switch ( button )
            {
                case kEventMouseButtonPrimary :
                    wxevent.SetEventType( clickCount > 1 ? wxEVT_LEFT_DCLICK : wxEVT_LEFT_DOWN )  ;
                    break ;

                case kEventMouseButtonSecondary :
                    wxevent.SetEventType( clickCount > 1 ? wxEVT_RIGHT_DCLICK : wxEVT_RIGHT_DOWN ) ;
                    break ;

                case kEventMouseButtonTertiary :
                    wxevent.SetEventType( clickCount > 1 ? wxEVT_MIDDLE_DCLICK : wxEVT_MIDDLE_DOWN ) ;
                    break ;

                default:
                    break ;
            }
            break ;

        case kEventMouseUp :
            switch ( button )
            {
                case kEventMouseButtonPrimary :
                    wxevent.SetEventType( wxEVT_LEFT_UP )  ;
                    break ;

                case kEventMouseButtonSecondary :
                    wxevent.SetEventType( wxEVT_RIGHT_UP ) ;
                    break ;

                case kEventMouseButtonTertiary :
                    wxevent.SetEventType( wxEVT_MIDDLE_UP ) ;
                    break ;

                default:
                    break ;
            }
            break ;
     // TODO http://developer.apple.com/qa/qa2005/qa1453.html
     // add declaration for 10.4 and change to kEventMouseScroll
    case kEventMouseWheelMoved :
        {
            wxevent.SetEventType( wxEVT_MOUSEWHEEL ) ;

            EventMouseWheelAxis axis = cEvent.GetParameter<EventMouseWheelAxis>(kEventParamMouseWheelAxis, typeMouseWheelAxis) ;
            SInt32 delta = cEvent.GetParameter<SInt32>(kEventParamMouseWheelDelta, typeSInt32) ;

            wxevent.m_wheelRotation = delta;
            wxevent.m_wheelDelta = 1;
            wxevent.m_linesPerAction = 1;
            wxevent.m_columnsPerAction = 1;
            if ( axis == kEventMouseWheelAxisX )
                wxevent.m_wheelAxis = wxMOUSE_WHEEL_HORIZONTAL;
        }
        break ;

        case kEventMouseEntered :
        case kEventMouseExited :
        case kEventMouseDragged :
        case kEventMouseMoved :
            wxevent.SetEventType( wxEVT_MOTION ) ;
            break;
        default :
            break ;
    }
}

#define NEW_CAPTURE_HANDLING 1

pascal OSStatus
wxMacTopLevelMouseEventHandler(EventHandlerCallRef WXUNUSED(handler),
                               EventRef event,
                               void *data)
{
    wxNonOwnedWindow* toplevelWindow = data ? ((wxNonOwnedWindowImpl*) data)->GetWXPeer() : NULL ;

    OSStatus result = eventNotHandledErr ;

    wxMacCarbonEvent cEvent( event ) ;

    Point screenMouseLocation = cEvent.GetParameter<Point>(kEventParamMouseLocation) ;
    Point windowMouseLocation = screenMouseLocation ;

    WindowRef window = NULL;
    short windowPart = ::FindWindow(screenMouseLocation, &window);

    wxWindow* currentMouseWindow = NULL ;
    ControlRef control = NULL ;

#if NEW_CAPTURE_HANDLING
    if ( wxApp::s_captureWindow )
    {
        window = (WindowRef) wxApp::s_captureWindow->MacGetTopLevelWindowRef() ;
        windowPart = inContent ;
    }
#endif

    if ( window )
    {
        QDGlobalToLocalPoint( GetWindowPort( window ), &windowMouseLocation );

        if ( wxApp::s_captureWindow
#if !NEW_CAPTURE_HANDLING
             && wxApp::s_captureWindow->MacGetTopLevelWindowRef() == (WXWindow) window && windowPart == inContent
#endif
             )
        {
            currentMouseWindow = wxApp::s_captureWindow ;
        }
        else if ( (IsWindowActive(window) && windowPart == inContent) )
        {
            ControlPartCode part ;
            control = FindControlUnderMouse( windowMouseLocation , window , &part ) ;
            // if there is no control below the mouse position, send the event to the toplevel window itself
            if ( control == 0 )
            {
                currentMouseWindow = (wxWindow*) toplevelWindow ;
            }
            else
            {
                currentMouseWindow = (wxWindow*) wxFindWindowFromWXWidget( (WXWidget) control ) ;
#ifndef __WXUNIVERSAL__
                if ( currentMouseWindow == NULL && cEvent.GetKind() == kEventMouseMoved )
                {
#if wxUSE_TOOLBAR
                    // for wxToolBar to function we have to send certaint events to it
                    // instead of its children (wxToolBarTools)
                    ControlRef parent ;
                    GetSuperControl(control, &parent );
                    wxWindow *wxparent = (wxWindow*) wxFindWindowFromWXWidget((WXWidget) parent ) ;
                    if ( wxparent && wxparent->IsKindOf( CLASSINFO( wxToolBar ) ) )
                        currentMouseWindow = wxparent ;
#endif
                }
#endif
            }

            // disabled windows must not get any input messages
            if ( currentMouseWindow && !currentMouseWindow->MacIsReallyEnabled() )
                currentMouseWindow = NULL;
        }
    }

    wxMouseEvent wxevent(wxEVT_LEFT_DOWN);
    SetupMouseEvent( wxevent , cEvent ) ;

    // handle all enter / leave events

    if ( currentMouseWindow != g_MacLastWindow )
    {
        if ( g_MacLastWindow )
        {
            wxMouseEvent eventleave(wxevent);
            eventleave.SetEventType( wxEVT_LEAVE_WINDOW );
            g_MacLastWindow->ScreenToClient( &eventleave.m_x, &eventleave.m_y );
            eventleave.SetEventObject( g_MacLastWindow ) ;
            wxevent.SetId( g_MacLastWindow->GetId() ) ;

#if wxUSE_TOOLTIPS
            wxToolTip::RelayEvent( g_MacLastWindow , eventleave);
#endif

            g_MacLastWindow->HandleWindowEvent(eventleave);
        }

        if ( currentMouseWindow )
        {
            wxMouseEvent evententer(wxevent);
            evententer.SetEventType( wxEVT_ENTER_WINDOW );
            currentMouseWindow->ScreenToClient( &evententer.m_x, &evententer.m_y );
            evententer.SetEventObject( currentMouseWindow ) ;
            wxevent.SetId( currentMouseWindow->GetId() ) ;

#if wxUSE_TOOLTIPS
            wxToolTip::RelayEvent( currentMouseWindow , evententer );
#endif

            currentMouseWindow->HandleWindowEvent(evententer);
        }

        g_MacLastWindow = currentMouseWindow ;
    }

    if ( windowPart == inMenuBar )
    {
        // special case menu bar, as we are having a low-level runloop we must do it ourselves
        if ( cEvent.GetKind() == kEventMouseDown )
        {
            ::MenuSelect( screenMouseLocation ) ;
            ::HiliteMenu(0);
            result = noErr ;
        }
    }
    else if ( window && windowPart == inProxyIcon )
    {
        // special case proxy icon bar, as we are having a low-level runloop we must do it ourselves
        if ( cEvent.GetKind() == kEventMouseDown )
        {
            if ( ::TrackWindowProxyDrag( window, screenMouseLocation ) != errUserWantsToDragWindow )
            {
                // TODO Track change of file path and report back
                result = noErr ;
            }
        }
    }
    else if ( currentMouseWindow )
    {
        wxWindow *currentMouseWindowParent = currentMouseWindow->GetParent();

        currentMouseWindow->ScreenToClient( &wxevent.m_x , &wxevent.m_y ) ;

        wxevent.SetEventObject( currentMouseWindow ) ;
        wxevent.SetId( currentMouseWindow->GetId() ) ;

        // make tooltips current

#if wxUSE_TOOLTIPS
        if ( wxevent.GetEventType() == wxEVT_MOTION )
            wxToolTip::RelayEvent( currentMouseWindow , wxevent );
#endif

        if ( currentMouseWindow->HandleWindowEvent(wxevent) )
        {
            if ( currentMouseWindowParent &&
                 !currentMouseWindowParent->GetChildren().Member(currentMouseWindow) )
                currentMouseWindow = NULL;

            result = noErr;
        }
        else
        {
            // if the user code did _not_ handle the event, then perform the
            // default processing
            if ( wxevent.GetEventType() == wxEVT_LEFT_DOWN )
            {
                // ... that is set focus to this window
                if (currentMouseWindow->CanAcceptFocus() && wxWindow::FindFocus()!=currentMouseWindow)
                    currentMouseWindow->SetFocus();
            }
        }

        if ( cEvent.GetKind() == kEventMouseUp && wxApp::s_captureWindow )
        {
            wxApp::s_captureWindow = NULL ;
            // update cursor ?
         }

        // update cursor

        wxWindow* cursorTarget = currentMouseWindow ;
        wxPoint cursorPoint( wxevent.m_x , wxevent.m_y ) ;

        extern wxCursor gGlobalCursor;

        if (!gGlobalCursor.IsOk())
        {
            while ( cursorTarget && !cursorTarget->MacSetupCursor( cursorPoint ) )
            {
                cursorTarget = cursorTarget->GetParent() ;
                if ( cursorTarget )
                    cursorPoint += cursorTarget->GetPosition();
            }
        }

    }
    else // currentMouseWindow == NULL
    {
        if (toplevelWindow && !control)
        {
           extern wxCursor gGlobalCursor;
           if (!gGlobalCursor.IsOk())
           {
                // update cursor when over toolbar and titlebar etc.
                wxSTANDARD_CURSOR->MacInstall() ;
           }
        }

        // don't mess with controls we don't know about
        // for some reason returning eventNotHandledErr does not lead to the correct behaviour
        // so we try sending them the correct control directly
        if ( cEvent.GetKind() == kEventMouseDown && toplevelWindow && control )
        {
            EventModifiers modifiers = cEvent.GetParameter<EventModifiers>(kEventParamKeyModifiers, typeUInt32) ;
            Point clickLocation = windowMouseLocation ;

            HIPoint hiPoint ;
            hiPoint.x = clickLocation.h ;
            hiPoint.y = clickLocation.v ;
            HIViewConvertPoint( &hiPoint , (ControlRef) toplevelWindow->GetHandle() , control  ) ;
            clickLocation.h = (int)hiPoint.x ;
            clickLocation.v = (int)hiPoint.y ;

            HandleControlClick( control , clickLocation , modifiers , (ControlActionUPP ) -1 ) ;
            result = noErr ;
        }
    }

    return result ;
}

static pascal OSStatus
wxNonOwnedWindowEventHandler(EventHandlerCallRef WXUNUSED(handler),
                                EventRef event,
                                void *data)
{
    OSStatus result = eventNotHandledErr ;

    wxMacCarbonEvent cEvent( event ) ;

    // WindowRef windowRef = cEvent.GetParameter<WindowRef>(kEventParamDirectObject) ;
    wxNonOwnedWindow* toplevelWindow = data ? ((wxNonOwnedWindowImpl*) data)->GetWXPeer() : NULL;

    switch ( GetEventKind( event ) )
    {
        case kEventWindowActivated :
        {
            toplevelWindow->HandleActivated( cEvent.GetTicks() , true) ;
            // we still sending an eventNotHandledErr in order to allow for default processing
        }
            break ;

        case kEventWindowDeactivated :
        {
            toplevelWindow->HandleActivated( cEvent.GetTicks() , false) ;
            // we still sending an eventNotHandledErr in order to allow for default processing
        }
            break ;

        case kEventWindowShown :
            toplevelWindow->Refresh() ;
            result = noErr ;
            break ;

        case kEventWindowClose :
            toplevelWindow->Close() ;
            result = noErr ;
            break ;

        case kEventWindowBoundsChanged :
        {
            UInt32 attributes = cEvent.GetParameter<UInt32>(kEventParamAttributes, typeUInt32) ;
            Rect newRect = cEvent.GetParameter<Rect>(kEventParamCurrentBounds) ;
            wxRect r( newRect.left , newRect.top , newRect.right - newRect.left , newRect.bottom - newRect.top ) ;
            if ( attributes & kWindowBoundsChangeSizeChanged )
            {
                toplevelWindow->HandleResized(cEvent.GetTicks() ) ;
            }

            if ( attributes & kWindowBoundsChangeOriginChanged )
            {
                toplevelWindow->HandleMoved(cEvent.GetTicks() ) ;
            }

            result = noErr ;
        }
            break ;

        case kEventWindowBoundsChanging :
        {
            UInt32 attributes = cEvent.GetParameter<UInt32>(kEventParamAttributes,typeUInt32) ;
            Rect newRect = cEvent.GetParameter<Rect>(kEventParamCurrentBounds) ;

            if ( (attributes & kWindowBoundsChangeSizeChanged) || (attributes & kWindowBoundsChangeOriginChanged) )
            {
                // all (Mac) rects are in content area coordinates, all wxRects in structure coordinates
                int left , top , width , height ;
                // structure width
                int swidth, sheight;

                toplevelWindow->GetNonOwnedPeer()->GetContentArea(left, top, width, height);
                toplevelWindow->GetNonOwnedPeer()->GetSize(swidth, sheight);
                int deltawidth = swidth - width;
                int deltaheight = sheight - height;
                wxRect adjustR(
                    newRect.left - left,
                    newRect.top - top,
                    newRect.right - newRect.left + deltawidth,
                    newRect.bottom - newRect.top + deltaheight ) ;

                toplevelWindow->HandleResizing( cEvent.GetTicks(), &adjustR );

                const Rect adjustedRect = { adjustR.y + top  , adjustR.x + left , adjustR.y + top + adjustR.height - deltaheight ,
                    adjustR.x + left + adjustR.width - deltawidth } ;
                if ( !EqualRect( &newRect , &adjustedRect ) )
                    cEvent.SetParameter<Rect>( kEventParamCurrentBounds , &adjustedRect ) ;
            }

            result = noErr ;
        }
            break ;

        case kEventWindowGetRegion :
            {
                if ( toplevelWindow->GetBackgroundStyle() == wxBG_STYLE_TRANSPARENT )
                {
                    WindowRegionCode windowRegionCode ;

                    // Fetch the region code that is being queried
                    GetEventParameter( event,
                                       kEventParamWindowRegionCode,
                                       typeWindowRegionCode, NULL,
                                       sizeof windowRegionCode, NULL,
                                       &windowRegionCode ) ;

                    // If it is the opaque region code then set the
                    // region to empty and return noErr to stop event
                    // propagation
                    if ( windowRegionCode == kWindowOpaqueRgn ) {
                        RgnHandle region;
                        GetEventParameter( event,
                                           kEventParamRgnHandle,
                                           typeQDRgnHandle, NULL,
                                           sizeof region, NULL,
                                           &region) ;
                        SetEmptyRgn(region) ;
                        result = noErr ;
                    }
                }
            }
            break ;

        default :
            break ;
    }

    return result ;
}

// mix this in from window.cpp
pascal OSStatus wxMacUnicodeTextEventHandler( EventHandlerCallRef handler , EventRef event , void *data ) ;

static pascal OSStatus wxNonOwnedEventHandler( EventHandlerCallRef handler , EventRef event , void *data )
{
    OSStatus result = eventNotHandledErr ;

    switch ( GetEventClass( event ) )
    {
        case kEventClassTextInput :
        {
            // TODO remove as soon as all events are on implementation classes only
            wxNonOwnedWindow* toplevelWindow = data ? ((wxNonOwnedWindowImpl*) data)->GetWXPeer() : NULL;

            result = wxMacUnicodeTextEventHandler( handler, event , toplevelWindow ) ;
        }
            break ;

        case kEventClassKeyboard :
            result = KeyboardEventHandler( handler, event , data ) ;
            break ;

        case kEventClassWindow :
            result = wxNonOwnedWindowEventHandler( handler, event , data ) ;
            break ;

        case kEventClassMouse :
            result = wxMacTopLevelMouseEventHandler( handler, event , data ) ;
            break ;

        default :
            break ;
    }

    return result ;
}

DEFINE_ONE_SHOT_HANDLER_GETTER( wxNonOwnedEventHandler )

// ---------------------------------------------------------------------------
// Support functions for shaped windows, based on Apple's CustomWindow sample at
// http://developer.apple.com/samplecode/Sample_Code/Human_Interface_Toolbox/Mac_OS_High_Level_Toolbox/CustomWindow.htm
// ---------------------------------------------------------------------------

static void wxShapedMacWindowGetPos(WindowRef window, Rect* inRect)
{
    GetWindowPortBounds(window, inRect);
    Point pt = { inRect->top ,inRect->left };
    QDLocalToGlobalPoint( GetWindowPort( window ), &pt );
    inRect->bottom += pt.v - inRect->top;
    inRect->right += pt.h - inRect->left;
    inRect->top = pt.v;
    inRect->left = pt.h;
}

static SInt32 wxShapedMacWindowGetFeatures(WindowRef WXUNUSED(window), SInt32 param)
{
    /*------------------------------------------------------
        Define which options your custom window supports.
    --------------------------------------------------------*/
    //just enable everything for our demo
    *(OptionBits*)param =
        //kWindowCanGrow |
        //kWindowCanZoom |
        kWindowCanCollapse |
        //kWindowCanGetWindowRegion |
        //kWindowHasTitleBar |
        //kWindowSupportsDragHilite |
        kWindowCanDrawInCurrentPort |
        //kWindowCanMeasureTitle |
        kWindowWantsDisposeAtProcessDeath |
        kWindowSupportsGetGrowImageRegion |
        kWindowDefSupportsColorGrafPort;

    return 1;
}

// The content region is left as a rectangle matching the window size, this is
// so the origin in the paint event, and etc. still matches what the
// programmer expects.
static void wxShapedMacWindowContentRegion(WindowRef window, RgnHandle rgn)
{
    SetEmptyRgn(rgn);
    wxNonOwnedWindow* win = wxNonOwnedWindow::GetFromWXWindow((WXWindow)window);
    if (win)
    {
        Rect r ;
        wxShapedMacWindowGetPos( window, &r ) ;
        RectRgn( rgn , &r ) ;
    }
}

// The structure region is set to the shape given to the SetShape method.
static void wxShapedMacWindowStructureRegion(WindowRef window, RgnHandle rgn)
{
    RgnHandle cachedRegion = (RgnHandle) GetWRefCon(window);

    SetEmptyRgn(rgn);
    if (cachedRegion)
    {
        Rect windowRect;
        wxShapedMacWindowGetPos(window, &windowRect);    // how big is the window
        CopyRgn(cachedRegion, rgn);                      // make a copy of our cached region
        OffsetRgn(rgn, windowRect.left, windowRect.top); // position it over window
        //MapRgn(rgn, &mMaskSize, &windowRect);          //scale it to our actual window size
    }
}

static SInt32 wxShapedMacWindowGetRegion(WindowRef window, SInt32 param)
{
    GetWindowRegionPtr rgnRec = (GetWindowRegionPtr)param;

    if (rgnRec == NULL)
        return paramErr;

    switch (rgnRec->regionCode)
    {
        case kWindowStructureRgn:
            wxShapedMacWindowStructureRegion(window, rgnRec->winRgn);
            break;

        case kWindowContentRgn:
            wxShapedMacWindowContentRegion(window, rgnRec->winRgn);
            break;

        default:
            SetEmptyRgn(rgnRec->winRgn);
            break;
    }

    return noErr;
}

// Determine the region of the window which was hit
//
static SInt32 wxShapedMacWindowHitTest(WindowRef window, SInt32 param)
{
    Point hitPoint;
    static RgnHandle tempRgn = NULL;

    if (tempRgn == NULL)
        tempRgn = NewRgn();

    // get the point clicked
    SetPt( &hitPoint, LoWord(param), HiWord(param) );

     // Mac OS 8.5 or later
    wxShapedMacWindowStructureRegion(window, tempRgn);
    if (PtInRgn( hitPoint, tempRgn )) //in window content region?
        return wInContent;

    // no significant area was hit
    return wNoHit;
}

static pascal long wxShapedMacWindowDef(short WXUNUSED(varCode), WindowRef window, SInt16 message, SInt32 param)
{
    switch (message)
    {
        case kWindowMsgHitTest:
            return wxShapedMacWindowHitTest(window, param);

        case kWindowMsgGetFeatures:
            return wxShapedMacWindowGetFeatures(window, param);

        // kWindowMsgGetRegion is sent during CreateCustomWindow and ReshapeCustomWindow
        case kWindowMsgGetRegion:
            return wxShapedMacWindowGetRegion(window, param);

        default:
            break;
    }

    return 0;
}

// implementation

typedef struct
{
    wxPoint m_position ;
    wxSize m_size ;
    bool m_wasResizable ;
} FullScreenData ;

wxNonOwnedWindowCarbonImpl::wxNonOwnedWindowCarbonImpl()
{
}

wxNonOwnedWindowCarbonImpl::wxNonOwnedWindowCarbonImpl( wxNonOwnedWindow* nonownedwnd) : wxNonOwnedWindowImpl( nonownedwnd)
{
    m_macEventHandler = NULL;
    m_macWindow = NULL;
    m_macFullScreenData = NULL ;
}

wxNonOwnedWindowCarbonImpl::~wxNonOwnedWindowCarbonImpl()
{
#if wxUSE_TOOLTIPS
        wxToolTip::NotifyWindowDelete(m_macWindow) ;
#endif

    if ( m_macEventHandler )
    {
        ::RemoveEventHandler((EventHandlerRef) m_macEventHandler);
        m_macEventHandler = NULL ;
    }

    if ( m_macWindow && !m_wxPeer->IsNativeWindowWrapper())
        DisposeWindow( m_macWindow );

    FullScreenData *data = (FullScreenData *) m_macFullScreenData ;
    delete data ;
    m_macFullScreenData = NULL ;

    m_macWindow = NULL;

}

void wxNonOwnedWindowCarbonImpl::WillBeDestroyed()
{
    if ( m_macEventHandler )
    {
        ::RemoveEventHandler((EventHandlerRef) m_macEventHandler);
        m_macEventHandler = NULL ;
    }
}

static void wxNonOwnedWindowInstallTopLevelWindowEventHandler(WindowRef window, EventHandlerRef* handler, void *ref)
{
    InstallWindowEventHandler(window, GetwxNonOwnedEventHandlerUPP(),
        GetEventTypeCount(eventList), eventList, ref, handler );
}

bool wxNonOwnedWindowCarbonImpl::SetShape(const wxRegion& region)
{
    // Make a copy of the region
    RgnHandle  shapeRegion = NewRgn();
    HIShapeGetAsQDRgn( region.GetWXHRGN(), shapeRegion );

    // Dispose of any shape region we may already have
    RgnHandle oldRgn = (RgnHandle)GetWRefCon( (WindowRef) m_wxPeer->GetWXWindow() );
    if ( oldRgn )
        DisposeRgn(oldRgn);

    // Save the region so we can use it later
    SetWRefCon((WindowRef) m_wxPeer->GetWXWindow(), (URefCon)shapeRegion);

    // inform the window manager that the window has changed shape
    ReshapeCustomWindow((WindowRef) m_wxPeer->GetWXWindow());

    return true;
}


void wxNonOwnedWindowCarbonImpl::MacInstallTopLevelWindowEventHandler()
{
    if ( m_macEventHandler != NULL )
    {
        verify_noerr( ::RemoveEventHandler( (EventHandlerRef) m_macEventHandler ) ) ;
    }
    wxNonOwnedWindowInstallTopLevelWindowEventHandler(MAC_WXHWND(m_macWindow),(EventHandlerRef *)&m_macEventHandler,this);
}

void wxNonOwnedWindowCarbonImpl::Create(
    wxWindow* WXUNUSED(parent),
    WXWindow nativeWindow )
{
    m_macWindow = nativeWindow;
}

void wxNonOwnedWindowCarbonImpl::Create(
                                        wxWindow* parent,
                                        const wxPoint& pos,
                                        const wxSize& size,
                                        long style, long extraStyle,
                                        const wxString& WXUNUSED(name) )
{

    OSStatus err = noErr ;
    Rect theBoundsRect;

    int x = (int)pos.x;
    int y = (int)pos.y;

    int w = size.x;
    int h = size.y;

    ::SetRect(&theBoundsRect, x, y , x + w, y + h);

    // translate the window attributes in the appropriate window class and attributes
    WindowClass wclass = 0;
    WindowAttributes attr = kWindowNoAttributes ;
    WindowGroupRef group = NULL ;
    bool activationScopeSet = false;
    WindowActivationScope activationScope = kWindowActivationScopeNone;

    if ( style & wxFRAME_TOOL_WINDOW )
    {
        if (
            ( style & wxMINIMIZE_BOX ) || ( style & wxMAXIMIZE_BOX ) ||
            ( style & wxSYSTEM_MENU ) || ( style & wxCAPTION ) ||
            ( style & wxTINY_CAPTION)
            )
        {
            if ( ( style & wxSTAY_ON_TOP ) )
                wclass = kUtilityWindowClass;
            else
                wclass = kFloatingWindowClass ;

            if ( ( style & wxTINY_CAPTION) )
                attr |= kWindowSideTitlebarAttribute ;
        }
        else
        {
            if ( style & wxNO_BORDER )
            {
                wclass = kSimpleWindowClass ;
            }
            else
            {
                wclass = kPlainWindowClass ;
            }
            activationScopeSet = true;
            activationScope = kWindowActivationScopeNone;
        }
    }
    else if ( ( style & wxPOPUP_WINDOW ) )
    {
        if ( ( style & wxBORDER_NONE ) )
        {
            wclass = kHelpWindowClass ;   // has no border
            attr |= kWindowNoShadowAttribute;
        }
        else
        {
            wclass = kPlainWindowClass ;  // has a single line border, it will have to do for now
        }
        group = GetWindowGroupOfClass(kFloatingWindowClass) ;
        // make sure we don't deactivate something
        activationScopeSet = true;
        activationScope = kWindowActivationScopeNone;
    }
    else if ( ( style & wxCAPTION ) )
    {
        wclass = kDocumentWindowClass ;
        attr |= kWindowInWindowMenuAttribute ;
    }
    else if ( ( style & wxFRAME_DRAWER ) )
    {
        wclass = kDrawerWindowClass;
    }
    else
    {
        if ( ( style & wxMINIMIZE_BOX ) || ( style & wxMAXIMIZE_BOX ) ||
            ( style & wxCLOSE_BOX ) || ( style & wxSYSTEM_MENU ) )
        {
            wclass = kDocumentWindowClass ;
        }
        else if ( ( style & wxNO_BORDER ) )
        {
            wclass = kSimpleWindowClass ;
        }
        else
        {
            wclass = kPlainWindowClass ;
        }
    }

    if ( wclass != kPlainWindowClass )
    {
        if ( ( style & wxMINIMIZE_BOX ) )
            attr |= kWindowCollapseBoxAttribute ;

        if ( ( style & wxMAXIMIZE_BOX ) )
            attr |= kWindowFullZoomAttribute ;

        if ( ( style & wxRESIZE_BORDER ) )
            attr |= kWindowResizableAttribute ;

        if ( ( style & wxCLOSE_BOX) )
            attr |= kWindowCloseBoxAttribute ;
    }
    attr |= kWindowLiveResizeAttribute;

    if ( ( style &wxSTAY_ON_TOP) )
        group = GetWindowGroupOfClass(kUtilityWindowClass) ;

    if ( ( style & wxFRAME_FLOAT_ON_PARENT ) )
        group = GetWindowGroupOfClass(kFloatingWindowClass) ;

    if ( group == NULL && parent != NULL )
    {
        WindowRef parenttlw = (WindowRef) parent->MacGetTopLevelWindowRef();
        if( parenttlw )
            group = GetWindowGroupParent( GetWindowGroup( parenttlw ) );
    }

    attr |= kWindowCompositingAttribute;
#if 0 // TODO : decide on overall handling of high dpi screens (pixel vs userscale)
    attr |= kWindowFrameworkScaledAttribute;
#endif

    if ( ( style &wxFRAME_SHAPED) )
    {
        WindowDefSpec customWindowDefSpec;
        customWindowDefSpec.defType = kWindowDefProcPtr;
        customWindowDefSpec.u.defProc =
#ifdef __LP64__
        (WindowDefUPP) wxShapedMacWindowDef;
#else
        NewWindowDefUPP(wxShapedMacWindowDef);
#endif
        err = ::CreateCustomWindow( &customWindowDefSpec, wclass,
                                   attr, &theBoundsRect,
                                   (WindowRef*) &m_macWindow);
    }
    else
    {
        err = ::CreateNewWindow( wclass , attr , &theBoundsRect , (WindowRef*)&m_macWindow ) ;
    }

    if ( err == noErr && m_macWindow != NULL && group != NULL )
        SetWindowGroup( (WindowRef) m_macWindow , group ) ;

    wxCHECK_RET( err == noErr, wxT("Mac OS error when trying to create new window") );

    // setup a separate group for each window, so that overlays can be handled easily

    WindowGroupRef overlaygroup = NULL;
    verify_noerr( CreateWindowGroup( kWindowGroupAttrMoveTogether | kWindowGroupAttrLayerTogether | kWindowGroupAttrHideOnCollapse, &overlaygroup ));
    verify_noerr( SetWindowGroupParent( overlaygroup, GetWindowGroup( (WindowRef) m_macWindow )));
    verify_noerr( SetWindowGroup( (WindowRef) m_macWindow , overlaygroup ));

    if ( activationScopeSet )
    {
        verify_noerr( SetWindowActivationScope( (WindowRef) m_macWindow , activationScope ));
    }

    // the create commands are only for content rect,
    // so we have to set the size again as structure bounds
    SetWindowBounds( m_macWindow , kWindowStructureRgn , &theBoundsRect ) ;

    // Causes the inner part of the window not to be metal
    // if the style is used before window creation.
#if 0 // TARGET_API_MAC_OSX
    if ( m_macUsesCompositing && m_macWindow != NULL )
    {
        if ( GetExtraStyle() & wxFRAME_EX_METAL )
            MacSetMetalAppearance( true ) ;
    }
#endif

    if ( m_macWindow != NULL )
    {
        MacSetUnifiedAppearance( true ) ;
    }

    HIViewRef growBoxRef = 0 ;
    err = HIViewFindByID( HIViewGetRoot( m_macWindow ), kHIViewWindowGrowBoxID, &growBoxRef  );
    if ( err == noErr && growBoxRef != 0 )
        HIGrowBoxViewSetTransparent( growBoxRef, true ) ;

    // the frame window event handler
    InstallStandardEventHandler( GetWindowEventTarget(m_macWindow) ) ;
    MacInstallTopLevelWindowEventHandler() ;

    if ( extraStyle & wxFRAME_EX_METAL)
        MacSetMetalAppearance(true);

    if ( ( style &wxFRAME_SHAPED) )
    {
        // default shape matches the window size
        wxRegion rgn( 0, 0, w, h );
        SetShape( rgn );
    }
}

bool wxNonOwnedWindowCarbonImpl::ShowWithEffect(bool show,
                                         wxShowEffect effect,
                                         unsigned timeout)
{
    WindowTransitionEffect transition = 0 ;
    switch( effect )
    {
        case wxSHOW_EFFECT_ROLL_TO_LEFT:
        case wxSHOW_EFFECT_ROLL_TO_RIGHT:
        case wxSHOW_EFFECT_ROLL_TO_TOP:
        case wxSHOW_EFFECT_ROLL_TO_BOTTOM:
        case wxSHOW_EFFECT_SLIDE_TO_LEFT:
        case wxSHOW_EFFECT_SLIDE_TO_RIGHT:
        case wxSHOW_EFFECT_SLIDE_TO_TOP:
        case wxSHOW_EFFECT_SLIDE_TO_BOTTOM:
            transition = kWindowGenieTransitionEffect;
            break;
        case wxSHOW_EFFECT_BLEND:
            transition = kWindowFadeTransitionEffect;
            break;
        case wxSHOW_EFFECT_EXPAND:
            // having sheets would be fine, but this might lead to a repositioning
#if 0
            if ( GetParent() )
                transition = kWindowSheetTransitionEffect;
            else
#endif
                transition = kWindowZoomTransitionEffect;
            break;

        case wxSHOW_EFFECT_NONE:
            // wxNonOwnedWindow is supposed to call Show() itself in this case
            wxFAIL_MSG( "ShowWithEffect() shouldn't be called" );
            return false;

        case wxSHOW_EFFECT_MAX:
            wxFAIL_MSG( "invalid effect flag" );
            return false;
    }

    TransitionWindowOptions options;
    options.version = 0;
    options.duration = timeout / 1000.0;
    options.window = transition == kWindowSheetTransitionEffect ? (WindowRef) m_wxPeer->GetParent()->MacGetTopLevelWindowRef() :0;
    options.userData = 0;

    wxSize size = wxGetDisplaySize();
    Rect bounds;
    GetWindowBounds( (WindowRef)m_macWindow, kWindowStructureRgn, &bounds );
    CGRect hiBounds = CGRectMake( bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top );

    switch ( effect )
    {
        case wxSHOW_EFFECT_ROLL_TO_RIGHT:
        case wxSHOW_EFFECT_SLIDE_TO_RIGHT:
            hiBounds.origin.x = 0;
            hiBounds.size.width = 0;
            break;

        case wxSHOW_EFFECT_ROLL_TO_LEFT:
        case wxSHOW_EFFECT_SLIDE_TO_LEFT:
            hiBounds.origin.x = size.x;
            hiBounds.size.width = 0;
            break;

        case wxSHOW_EFFECT_ROLL_TO_TOP:
        case wxSHOW_EFFECT_SLIDE_TO_TOP:
            hiBounds.origin.y = size.y;
            hiBounds.size.height = 0;
            break;

        case wxSHOW_EFFECT_ROLL_TO_BOTTOM:
        case wxSHOW_EFFECT_SLIDE_TO_BOTTOM:
            hiBounds.origin.y = 0;
            hiBounds.size.height = 0;
            break;

        default:
            break; // direction doesn't make sense
    }

    ::TransitionWindowWithOptions
    (
        (WindowRef)m_macWindow,
        transition,
        show ? kWindowShowTransitionAction : kWindowHideTransitionAction,
        transition == kWindowGenieTransitionEffect ? &hiBounds : NULL,
        false,
        &options
    );

    if ( show )
    {
        ::SelectWindow( (WindowRef)m_macWindow ) ;
    }

    return true;
}

void wxNonOwnedWindowCarbonImpl::SetTitle( const wxString& title, wxFontEncoding encoding )
{
    SetWindowTitleWithCFString( m_macWindow , wxCFStringRef( title , encoding ) ) ;
}

bool wxNonOwnedWindowCarbonImpl::IsMaximized() const
{
    return IsWindowInStandardState( m_macWindow , NULL , NULL ) ;
}

bool wxNonOwnedWindowCarbonImpl::IsIconized() const
{
    return IsWindowCollapsed((WindowRef)GetWXWindow() ) ;
}

void wxNonOwnedWindowCarbonImpl::Iconize( bool iconize )
{
    if ( IsWindowCollapsable( m_macWindow ) )
        CollapseWindow( m_macWindow , iconize ) ;
}

void wxNonOwnedWindowCarbonImpl::Maximize(bool maximize)
{
    Point idealSize = { 0 , 0 } ;
    if ( maximize )
    {
        HIRect bounds ;
        HIWindowGetAvailablePositioningBounds(kCGNullDirectDisplay,kHICoordSpace72DPIGlobal,
            &bounds);
        idealSize.h = bounds.size.width;
        idealSize.v = bounds.size.height;
    }
    ZoomWindowIdeal( (WindowRef)GetWXWindow() , maximize ? inZoomOut : inZoomIn , &idealSize ) ;
}

bool wxNonOwnedWindowCarbonImpl::IsFullScreen() const
{
    return m_macFullScreenData != NULL ;
}

bool wxNonOwnedWindowCarbonImpl::ShowFullScreen(bool show, long style)
{
    if ( show )
    {
        FullScreenData *data = (FullScreenData *)m_macFullScreenData ;
        delete data ;
        data = new FullScreenData() ;

        m_macFullScreenData = data ;
        data->m_position = m_wxPeer->GetPosition() ;
        data->m_size = m_wxPeer->GetSize() ;
#if wxOSX_USE_CARBON
        WindowAttributes attr = 0;
        GetWindowAttributes((WindowRef) GetWXWindow(), &attr);
        data->m_wasResizable = attr & kWindowResizableAttribute;
        if ( style & wxFULLSCREEN_NOMENUBAR )
            HideMenuBar() ;
#endif

        wxRect client = wxGetClientDisplayRect() ;

        int left, top, width, height ;
        int x, y, w, h ;

        x = client.x ;
        y = client.y ;
        w = client.width ;
        h = client.height ;

        GetContentArea( left, top, width, height ) ;
        int outerwidth, outerheight;
        GetSize( outerwidth, outerheight );

        if ( style & wxFULLSCREEN_NOCAPTION )
        {
            y -= top ;
            h += top ;
            // avoid adding the caption twice to the height
            outerheight -= top;
        }

        if ( style & wxFULLSCREEN_NOBORDER )
        {
            x -= left ;
            w += outerwidth - width;
            h += outerheight - height;
        }

        if ( style & wxFULLSCREEN_NOTOOLBAR )
        {
            // TODO
        }

        if ( style & wxFULLSCREEN_NOSTATUSBAR )
        {
            // TODO
        }

        m_wxPeer->SetSize( x , y , w, h ) ;
        if ( data->m_wasResizable )
        {
#if wxOSX_USE_CARBON
            ChangeWindowAttributes( (WindowRef) GetWXWindow() , kWindowNoAttributes , kWindowResizableAttribute ) ;
#endif
        }
    }
    else if ( m_macFullScreenData != NULL )
    {
        FullScreenData *data = (FullScreenData *) m_macFullScreenData ;
#if wxOSX_USE_CARBON
        ShowMenuBar() ;
        if ( data->m_wasResizable )
            ChangeWindowAttributes( (WindowRef) GetWXWindow() , kWindowResizableAttribute ,  kWindowNoAttributes ) ;
#endif
        m_wxPeer->SetPosition( data->m_position ) ;
        m_wxPeer->SetSize( data->m_size ) ;

        delete data ;
        m_macFullScreenData = NULL ;
    }

    return true;
}

// Attracts the users attention to this window if the application is
// inactive (should be called when a background event occurs)

static pascal void wxMacNMResponse( NMRecPtr ptr )
{
    NMRemove( ptr ) ;
    DisposePtr( (Ptr)ptr ) ;
}

void wxNonOwnedWindowCarbonImpl::RequestUserAttention(int WXUNUSED(flags))
{
    NMRecPtr notificationRequest = (NMRecPtr) NewPtr( sizeof( NMRec) ) ;

    memset( notificationRequest , 0 , sizeof(*notificationRequest) ) ;
    notificationRequest->qType = nmType ;
    notificationRequest->nmMark = 1 ;
    notificationRequest->nmIcon = 0 ;
    notificationRequest->nmSound = 0 ;
    notificationRequest->nmStr = NULL ;
    notificationRequest->nmResp = wxMacNMResponse ;

    verify_noerr( NMInstall( notificationRequest ) ) ;
}

void wxNonOwnedWindowCarbonImpl::ScreenToWindow( int *x, int *y )
{
    HIPoint p = CGPointMake(  (x ? *x : 0), (y ? *y : 0) );
    HIViewRef contentView ;
    // TODO check toolbar offset
    HIViewFindByID( HIViewGetRoot( m_macWindow ), kHIViewWindowContentID , &contentView) ;
    HIPointConvert( &p, kHICoordSpace72DPIGlobal, NULL, kHICoordSpaceView, contentView );
    if ( x )
        *x = (int)p.x;
    if ( y )
        *y = (int)p.y;
}

void wxNonOwnedWindowCarbonImpl::WindowToScreen( int *x, int *y )
{
    HIPoint p = CGPointMake(  (x ? *x : 0), (y ? *y : 0) );
    HIViewRef contentView ;
    // TODO check toolbar offset
    HIViewFindByID( HIViewGetRoot( m_macWindow ), kHIViewWindowContentID , &contentView) ;
    HIPointConvert( &p, kHICoordSpaceView, contentView, kHICoordSpace72DPIGlobal, NULL );
    if ( x )
        *x = (int)p.x;
    if ( y )
        *y = (int)p.y;
}

bool wxNonOwnedWindowCarbonImpl::IsActive()
{
    return ActiveNonFloatingWindow() == m_macWindow;
}

wxNonOwnedWindowImpl* wxNonOwnedWindowImpl::CreateNonOwnedWindow( wxNonOwnedWindow* wxpeer, wxWindow* parent, const wxPoint& pos, const wxSize& size,
    long style, long extraStyle, const wxString& name )
{
    wxNonOwnedWindowImpl* now = new wxNonOwnedWindowCarbonImpl( wxpeer );
    now->Create( parent, pos, size, style , extraStyle, name );
    return now;
}

wxNonOwnedWindowImpl* wxNonOwnedWindowImpl::CreateNonOwnedWindow( wxNonOwnedWindow* wxpeer, wxWindow* parent, WXWindow nativeWindow )
{
    wxNonOwnedWindowCarbonImpl* now = new wxNonOwnedWindowCarbonImpl( wxpeer );
    now->Create( parent, nativeWindow );
    return now;
}

