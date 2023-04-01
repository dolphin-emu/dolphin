/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/window.cpp
// Purpose:     wxWindowMac
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/window.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/panel.h"
    #include "wx/frame.h"
    #include "wx/dc.h"
    #include "wx/dcclient.h"
    #include "wx/button.h"
    #include "wx/menu.h"
    #include "wx/dialog.h"
    #include "wx/settings.h"
    #include "wx/msgdlg.h"
    #include "wx/scrolbar.h"
    #include "wx/statbox.h"
    #include "wx/textctrl.h"
    #include "wx/toolbar.h"
    #include "wx/layout.h"
    #include "wx/statusbr.h"
    #include "wx/menuitem.h"
    #include "wx/treectrl.h"
    #include "wx/listctrl.h"
#endif

#include "wx/tooltip.h"
#include "wx/spinctrl.h"
#include "wx/geometry.h"

#if wxUSE_LISTCTRL
    #include "wx/listctrl.h"
#endif

#if wxUSE_TREECTRL
    #include "wx/treectrl.h"
#endif

#if wxUSE_CARET
    #include "wx/caret.h"
#endif

#if wxUSE_POPUPWIN
    #include "wx/popupwin.h"
#endif

#if wxUSE_DRAG_AND_DROP
#include "wx/dnd.h"
#endif

#if wxOSX_USE_CARBON
#include "wx/osx/uma.h"
#else
#include "wx/osx/private.h"
// bring in theming
#include <Carbon/Carbon.h>
#endif

#define MAC_SCROLLBAR_SIZE 15
#define MAC_SMALL_SCROLLBAR_SIZE 11

#include <string.h>

#define wxMAC_DEBUG_REDRAW 0
#ifndef wxMAC_DEBUG_REDRAW
#define wxMAC_DEBUG_REDRAW 0
#endif

// Get the window with the focus
WXWidget wxWidgetImpl::FindFocus()
{
    ControlRef control = NULL ;
    GetKeyboardFocus( GetUserFocusWindow() , &control ) ;
    return control;
}

// no compositing to take into account under carbon
wxWidgetImpl* wxWidgetImpl::FindBestFromWXWidget(WXWidget control)
{
    return FindFromWXWidget(control);
}

// ---------------------------------------------------------------------------
// Carbon Events
// ---------------------------------------------------------------------------

static const EventTypeSpec eventList[] =
{
    { kEventClassCommand, kEventProcessCommand } ,
    { kEventClassCommand, kEventCommandUpdateStatus } ,

    { kEventClassControl , kEventControlGetClickActivation } ,
    { kEventClassControl , kEventControlHit } ,

    { kEventClassTextInput, kEventTextInputUnicodeForKeyEvent } ,
    { kEventClassTextInput, kEventTextInputUpdateActiveInputArea } ,

    { kEventClassControl , kEventControlDraw } ,

    { kEventClassControl , kEventControlVisibilityChanged } ,
    { kEventClassControl , kEventControlEnabledStateChanged } ,
    { kEventClassControl , kEventControlHiliteChanged } ,

    { kEventClassControl , kEventControlActivate } ,
    { kEventClassControl , kEventControlDeactivate } ,

    { kEventClassControl , kEventControlSetFocusPart } ,
    { kEventClassControl , kEventControlFocusPartChanged } ,

    { kEventClassService , kEventServiceGetTypes },
    { kEventClassService , kEventServiceCopy },
    { kEventClassService , kEventServicePaste },

//    { kEventClassControl , kEventControlInvalidateForSizeChange } , // 10.3 only
//    { kEventClassControl , kEventControlBoundsChanged } ,
} ;

static pascal OSStatus wxMacWindowControlEventHandler( EventHandlerCallRef handler , EventRef event , void *data )
{
    OSStatus result = eventNotHandledErr ;
    static wxWindowMac* targetFocusWindow = NULL;
    static wxWindowMac* formerFocusWindow = NULL;

    wxMacCarbonEvent cEvent( event ) ;

    ControlRef controlRef ;
    wxWindowMac* thisWindow = (wxWindowMac*) data ;

    cEvent.GetParameter( kEventParamDirectObject , &controlRef ) ;

    switch ( GetEventKind( event ) )
    {
        case kEventControlDraw :
            {
                HIShapeRef updateRgn = NULL ;
                HIMutableShapeRef allocatedRgn = NULL ;
                wxRegion visRegion = thisWindow->MacGetVisibleRegion() ;

                // according to the docs: redraw entire control if param not present
                if ( cEvent.GetParameter<HIShapeRef>(kEventParamShape, &updateRgn) != noErr )
                {
                    updateRgn = visRegion.GetWXHRGN();
                }
                else
                {
                    if ( thisWindow->MacGetLeftBorderSize() != 0 || thisWindow->MacGetTopBorderSize() != 0 )
                    {
                        // as this update region is in native window locals we must adapt it to wx window local
                        allocatedRgn = HIShapeCreateMutableCopy(updateRgn);
                        HIShapeOffset(allocatedRgn, thisWindow->MacGetLeftBorderSize() , thisWindow->MacGetTopBorderSize());
                        // hide the given region by the new region that must be shifted
                        updateRgn = allocatedRgn ;
                    }
                }

#if wxMAC_DEBUG_REDRAW
                if ( thisWindow->MacIsUserPane() )
                {
                    static float color = 0.5 ;
                    static int channel = 0 ;
                    HIRect bounds;
                    CGContextRef cgContext = cEvent.GetParameter<CGContextRef>(kEventParamCGContextRef) ;

                    HIViewGetBounds( controlRef, &bounds );
                    CGContextSetRGBFillColor( cgContext, channel == 0 ? color : 0.5 ,
                        channel == 1 ? color : 0.5 , channel == 2 ? color : 0.5 , 1 );
                    CGContextFillRect( cgContext, bounds );
                    color += 0.1 ;
                    if ( color > 0.9 )
                    {
                        color = 0.5 ;
                        channel++ ;
                        if ( channel == 3 )
                            channel = 0 ;
                    }
                }
#endif

                {
                    CGContextRef cgContext = NULL ;
                    OSStatus err = cEvent.GetParameter<CGContextRef>(kEventParamCGContextRef, &cgContext) ;
                    if ( err != noErr )
                    {
                        // for non-composite drawing, since we don't support it ourselves, send it through the
                        // the default handler
                        // CallNextEventHandler( handler,event ) ;
                        // result = noErr ;
                        if ( allocatedRgn )
                            CFRelease( allocatedRgn ) ;
                        break;
                    }

                    thisWindow->MacSetCGContextRef( cgContext ) ;

                    {
                        wxMacCGContextStateSaver sg( cgContext ) ;
                        CGFloat alpha = (CGFloat)1.0 ;
                        {
                            wxWindow* iter = thisWindow ;
                            while ( iter )
                            {
                                alpha *= (CGFloat)( iter->GetTransparent()/255.0 ) ;
                                if ( iter->IsTopLevel() )
                                    iter = NULL ;
                                else
                                    iter = iter->GetParent() ;
                            }
                        }
                        CGContextSetAlpha( cgContext, alpha ) ;

                        if ( thisWindow->GetBackgroundStyle() == wxBG_STYLE_TRANSPARENT )
                        {
                            HIRect bounds;
                            HIViewGetBounds( controlRef, &bounds );
                            CGContextClearRect( cgContext, bounds );
                        }

                        if ( !HIShapeIsEmpty(updateRgn) )
                        {
                            // refcount increase because wxRegion constructor takes ownership of the native region
                            CFRetain(updateRgn);
                            thisWindow->GetUpdateRegion() = wxRegion(updateRgn);
                            if ( !thisWindow->MacDoRedraw( cEvent.GetTicks() ) )
                            {
                               // for native controls: call their native paint method
                                if ( !thisWindow->MacIsUserPane() ||
                                    ( thisWindow->IsTopLevel() && thisWindow->GetBackgroundStyle() == wxBG_STYLE_SYSTEM ) )
                                {
                                    if ( thisWindow->GetBackgroundStyle() != wxBG_STYLE_TRANSPARENT )
                                    {
                                        CallNextEventHandler( handler,event ) ;
                                        result = noErr ;
                                    }
                                }
                            }
                            else
                            {
                                result = noErr ;
                            }
                            thisWindow->MacPaintChildrenBorders();
                        }
                        thisWindow->MacSetCGContextRef( NULL ) ;
                    }
                }

                if ( allocatedRgn )
                    CFRelease( allocatedRgn ) ;
            }
            break ;

        case kEventControlVisibilityChanged :
            // we might have two native controls attributed to the same wxWindow instance
            // eg a scrollview and an embedded textview, make sure we only fire for the 'outer'
            // control, as otherwise native and wx visibility are different
            if ( thisWindow->GetPeer() != NULL && thisWindow->GetPeer()->GetControlRef() == controlRef )
            {
                thisWindow->MacVisibilityChanged() ;
            }
            break ;

        case kEventControlEnabledStateChanged :
            thisWindow->MacEnabledStateChanged();
            break ;

        case kEventControlHiliteChanged :
            thisWindow->MacHiliteChanged() ;
            break ;

        case kEventControlActivate :
        case kEventControlDeactivate :
            // FIXME: we should have a virtual function for this!
#if wxUSE_TREECTRL
            if ( thisWindow->IsKindOf( CLASSINFO( wxTreeCtrl ) ) )
                thisWindow->Refresh();
#endif
#if wxUSE_LISTCTRL
            if ( thisWindow->IsKindOf( CLASSINFO( wxListCtrl ) ) )
                thisWindow->Refresh();
#endif
            break ;

        //
        // focus handling
        // different handling on OS X
        //

        case kEventControlFocusPartChanged :
            // the event is emulated by wxmac for systems lower than 10.5
            {
                ControlPartCode previousControlPart = cEvent.GetParameter<ControlPartCode>(kEventParamControlPreviousPart , typeControlPartCode );
                ControlPartCode currentControlPart = cEvent.GetParameter<ControlPartCode>(kEventParamControlCurrentPart , typeControlPartCode );

                if ( thisWindow->MacGetTopLevelWindow() && thisWindow->GetPeer()->NeedsFocusRect() )
                {
                    thisWindow->MacInvalidateBorders();
                }

                if ( currentControlPart == 0 )
                {
                    // kill focus
#if wxUSE_CARET
                    if ( thisWindow->GetCaret() )
                        thisWindow->GetCaret()->OnKillFocus();
#endif

                    wxLogTrace(wxT("Focus"), wxT("focus lost(%p)"), static_cast<void*>(thisWindow));

                    // remove this as soon as posting the synthesized event works properly
                    static bool inKillFocusEvent = false ;

                    if ( !inKillFocusEvent )
                    {
                        inKillFocusEvent = true ;
                        wxFocusEvent event( wxEVT_KILL_FOCUS, thisWindow->GetId());
                        event.SetEventObject(thisWindow);
                        event.SetWindow(targetFocusWindow);
                        thisWindow->HandleWindowEvent(event) ;
                        inKillFocusEvent = false ;
                        targetFocusWindow = NULL;
                    }
                }
                else if ( previousControlPart == 0 )
                {
                    // set focus
                    // panel wants to track the window which was the last to have focus in it
                    wxLogTrace(wxT("Focus"), wxT("focus set(%p)"), static_cast<void*>(thisWindow));
                    wxChildFocusEvent eventFocus((wxWindow*)thisWindow);
                    thisWindow->HandleWindowEvent(eventFocus);

#if wxUSE_CARET
                    if ( thisWindow->GetCaret() )
                        thisWindow->GetCaret()->OnSetFocus();
#endif

                    wxFocusEvent event(wxEVT_SET_FOCUS, thisWindow->GetId());
                    event.SetEventObject(thisWindow);
                    event.SetWindow(formerFocusWindow);
                    thisWindow->HandleWindowEvent(event) ;
                    formerFocusWindow = NULL;
                }
            }
            break;
        case kEventControlSetFocusPart :
            {
                Boolean focusEverything = false ;
                if ( cEvent.GetParameter<Boolean>(kEventParamControlFocusEverything , &focusEverything ) == noErr )
                {
                    // put a breakpoint here to catch focus everything events
                }
                ControlPartCode controlPart = cEvent.GetParameter<ControlPartCode>(kEventParamControlPart , typeControlPartCode );
                if ( controlPart != kControlFocusNoPart )
                {
                    targetFocusWindow = thisWindow;
                    wxLogTrace(wxT("Focus"), wxT("focus to be set(%p)"), static_cast<void*>(thisWindow));
                }
                else
                {
                    formerFocusWindow = thisWindow;
                    wxLogTrace(wxT("Focus"), wxT("focus to be lost(%p)"), static_cast<void*>(thisWindow));
                }

                ControlPartCode previousControlPart = 0;
                verify_noerr( HIViewGetFocusPart(controlRef, &previousControlPart));

                if ( thisWindow->MacIsUserPane() )
                {
                    if ( controlPart != kControlFocusNoPart )
                        cEvent.SetParameter<ControlPartCode>( kEventParamControlPart, typeControlPartCode, 1 ) ;
                    result = noErr ;
                }
                else
                    result = CallNextEventHandler(handler, event);
            }
            break ;

        case kEventControlHit :
            result = thisWindow->MacControlHit( handler , event ) ;
            break ;

        case kEventControlGetClickActivation :
            {
                // fix to always have a proper activation for DataBrowser controls (stay in bkgnd otherwise)
                WindowRef owner = cEvent.GetParameter<WindowRef>(kEventParamWindowRef);
                if ( !IsWindowActive(owner) )
                {
                    cEvent.SetParameter(kEventParamClickActivation,typeClickActivationResult, (UInt32) kActivateAndIgnoreClick) ;
                    result = noErr ;
                }
            }
            break ;

        default :
            break ;
    }

    return result ;
}

static pascal OSStatus
wxMacWindowServiceEventHandler(EventHandlerCallRef WXUNUSED(handler),
                               EventRef event,
                               void *data)
{
    OSStatus result = eventNotHandledErr ;

    wxMacCarbonEvent cEvent( event ) ;

    ControlRef controlRef ;
    wxWindowMac* thisWindow = (wxWindowMac*) data ;
    wxTextCtrl* textCtrl = wxDynamicCast( thisWindow , wxTextCtrl ) ;
    cEvent.GetParameter( kEventParamDirectObject , &controlRef ) ;

    switch ( GetEventKind( event ) )
    {
        case kEventServiceGetTypes :
            if ( textCtrl )
            {
                long from, to ;
                textCtrl->GetSelection( &from , &to ) ;

                CFMutableArrayRef copyTypes = 0 , pasteTypes = 0;
                if ( from != to )
                    copyTypes = cEvent.GetParameter< CFMutableArrayRef >( kEventParamServiceCopyTypes , typeCFMutableArrayRef ) ;
                if ( textCtrl->IsEditable() )
                    pasteTypes = cEvent.GetParameter< CFMutableArrayRef >( kEventParamServicePasteTypes , typeCFMutableArrayRef ) ;

                static const OSType textDataTypes[] = { kTXNTextData /* , 'utxt', 'PICT', 'MooV', 'AIFF' */  };
                for ( size_t i = 0 ; i < WXSIZEOF(textDataTypes) ; ++i )
                {
                    CFStringRef typestring = CreateTypeStringWithOSType(textDataTypes[i]);
                    if ( typestring )
                    {
                        if ( copyTypes )
                            CFArrayAppendValue(copyTypes, typestring) ;
                        if ( pasteTypes )
                            CFArrayAppendValue(pasteTypes, typestring) ;

                        CFRelease( typestring ) ;
                    }
                }

                result = noErr ;
            }
            break ;

        case kEventServiceCopy :
            if ( textCtrl )
            {
                long from, to ;

                textCtrl->GetSelection( &from , &to ) ;
                wxString val = textCtrl->GetValue() ;
                val = val.Mid( from , to - from ) ;
                PasteboardRef pasteboard = cEvent.GetParameter<PasteboardRef>( kEventParamPasteboardRef, typePasteboardRef );
                verify_noerr( PasteboardClear( pasteboard ) ) ;
                PasteboardSynchronize( pasteboard );
                // TODO add proper conversion
                CFDataRef data = CFDataCreate( kCFAllocatorDefault, (const UInt8*)val.c_str(), val.length() );
                PasteboardPutItemFlavor( pasteboard, (PasteboardItemID) 1, CFSTR("com.apple.traditional-mac-plain-text"), data, 0);
                CFRelease( data );
                result = noErr ;
            }
            break ;

        case kEventServicePaste :
            if ( textCtrl )
            {
                PasteboardRef pasteboard = cEvent.GetParameter<PasteboardRef>( kEventParamPasteboardRef, typePasteboardRef );
                PasteboardSynchronize( pasteboard );
                ItemCount itemCount;
                verify_noerr( PasteboardGetItemCount( pasteboard, &itemCount ) );
                for( UInt32 itemIndex = 1; itemIndex <= itemCount; itemIndex++ )
                {
                    PasteboardItemID itemID;
                    if ( PasteboardGetItemIdentifier( pasteboard, itemIndex, &itemID ) == noErr )
                    {
                        CFDataRef flavorData = NULL;
                        if ( PasteboardCopyItemFlavorData( pasteboard, itemID, CFSTR("com.apple.traditional-mac-plain-text"), &flavorData ) == noErr )
                        {
                            CFIndex flavorDataSize = CFDataGetLength( flavorData );
                            char *content = new char[flavorDataSize+1] ;
                            memcpy( content, CFDataGetBytePtr( flavorData ), flavorDataSize );
                            content[flavorDataSize]=0;
                            CFRelease( flavorData );
#if wxUSE_UNICODE
                            textCtrl->WriteText( wxString( content , wxConvLocal ) );
#else
                            textCtrl->WriteText( wxString( content ) ) ;
#endif

                            delete[] content ;
                            result = noErr ;
                        }
                    }
                }
            }
            break ;

        default:
            break ;
    }

    return result ;
}

WXDLLEXPORT pascal OSStatus wxMacUnicodeTextEventHandler( EventHandlerCallRef handler , EventRef event , void *data )
{
    OSStatus result = eventNotHandledErr ;
    wxWindowMac* focus = (wxWindowMac*) data ;

    wchar_t* uniChars = NULL ;
    UInt32 when = EventTimeToTicks( GetEventTime( event ) ) ;

    UniChar* charBuf = NULL;
    ByteCount dataSize = 0 ;
    int numChars = 0 ;
    UniChar buf[2] ;
    if ( GetEventParameter( event, kEventParamTextInputSendText, typeUnicodeText, NULL, 0 , &dataSize, NULL ) == noErr )
    {
        numChars = dataSize / sizeof( UniChar) + 1;
        charBuf = buf ;

        if ( (size_t) numChars * 2 > sizeof(buf) )
            charBuf = new UniChar[ numChars ] ;
        else
            charBuf = buf ;

        uniChars = new wchar_t[ numChars ] ;
        GetEventParameter( event, kEventParamTextInputSendText, typeUnicodeText, NULL, dataSize , NULL , charBuf ) ;
        charBuf[ numChars - 1 ] = 0;
        // the resulting string will never have more chars than the utf16 version, so this is safe
        wxMBConvUTF16 converter ;
        numChars = converter.MB2WC( uniChars , (const char*)charBuf , numChars ) ;
    }

    switch ( GetEventKind( event ) )
    {
        case kEventTextInputUpdateActiveInputArea :
            {
                // An IME input event may return several characters, but we need to send one char at a time to
                // EVT_CHAR
                for (int pos=0 ; pos < numChars ; pos++)
                {
                    WXEVENTREF formerEvent = wxTheApp->MacGetCurrentEvent() ;
                    WXEVENTHANDLERCALLREF formerHandler = wxTheApp->MacGetCurrentEventHandlerCallRef() ;
                    wxTheApp->MacSetCurrentEvent( event , handler ) ;

                    UInt32 message = uniChars[pos] < 128 ? (char)uniChars[pos] : '?';
/*
    NB: faking a charcode here is problematic. The kEventTextInputUpdateActiveInputArea event is sent
    multiple times to update the active range during inline input, so this handler will often receive
    uncommited text, which should usually not trigger side effects. It might be a good idea to check the
    kEventParamTextInputSendFixLen parameter and verify if input is being confirmed (see CarbonEvents.h).
    On the other hand, it can be useful for some applications to react to uncommitted text (for example,
    to update a status display), as long as it does not disrupt the inline input session. Ideally, wx
    should add new event types to support advanced text input. For now, I would keep things as they are.

    However, the code that was being used caused additional problems:
                    UInt32 message = (0  << 8) + ((char)uniChars[pos] );
    Since it simply truncated the unichar to the last byte, it ended up causing weird bugs with inline
    input, such as switching to another field when one attempted to insert the character U+4E09 (the kanji
    for "three"), because it was truncated to 09 (kTabCharCode), which was later "converted" to WXK_TAB
    (still 09) in wxMacTranslateKey; or triggering the default button when one attempted to insert U+840D
    (the kanji for "name"), which got truncated to 0D and interpreted as a carriage return keypress.
    Note that even single-byte characters could have been misinterpreted, since MacRoman charcodes only
    overlap with Unicode within the (7-bit) ASCII range.
    But simply passing a NUL charcode would disable text updated events, because wxTextCtrl::OnChar checks
    for codes within a specific range. Therefore I went for the solution seen above, which keeps ASCII
    characters as they are and replaces the rest with '?', ensuring that update events are triggered.
    It would be better to change wxTextCtrl::OnChar to look at the actual unicode character instead, but
    I don't have time to look into that right now.
        -- CL
*/
                    if ( wxTheApp->MacSendCharEvent( focus , message , 0 , when , uniChars[pos] ) )
                    {
                        result = noErr ;
                    }

                    wxTheApp->MacSetCurrentEvent( formerEvent , formerHandler ) ;
                }
            }
            break ;
        case kEventTextInputUnicodeForKeyEvent :
            {
                UInt32 keyCode, modifiers ;
                EventRef rawEvent ;
                unsigned char charCode ;

                GetEventParameter( event, kEventParamTextInputSendKeyboardEvent, typeEventRef, NULL, sizeof(rawEvent), NULL, &rawEvent ) ;
                GetEventParameter( rawEvent, kEventParamKeyMacCharCodes, typeChar, NULL, 1, NULL, &charCode );
                GetEventParameter( rawEvent, kEventParamKeyCode, typeUInt32, NULL, sizeof(UInt32), NULL, &keyCode );
                GetEventParameter( rawEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(UInt32), NULL, &modifiers );

                UInt32 message = (keyCode << 8) + charCode;

                // An IME input event may return several characters, but we need to send one char at a time to
                // EVT_CHAR
                for (int pos=0 ; pos < numChars ; pos++)
                {
                    WXEVENTREF formerEvent = wxTheApp->MacGetCurrentEvent() ;
                    WXEVENTHANDLERCALLREF formerHandler = wxTheApp->MacGetCurrentEventHandlerCallRef() ;
                    wxTheApp->MacSetCurrentEvent( event , handler ) ;

                    if ( wxTheApp->MacSendCharEvent( focus , message , modifiers , when , uniChars[pos] ) )
                    {
                        result = noErr ;
                    }

                    wxTheApp->MacSetCurrentEvent( formerEvent , formerHandler ) ;
                }
            }
            break;
        default:
            break ;
    }

    delete [] uniChars ;
    if ( charBuf != buf )
        delete [] charBuf ;

    return result ;
}

static pascal OSStatus
wxMacWindowCommandEventHandler(EventHandlerCallRef WXUNUSED(handler),
                               EventRef event,
                               void *data)
{
    OSStatus result = eventNotHandledErr ;
    wxWindowMac* focus = (wxWindowMac*) data ;

    HICommand command ;

    wxMacCarbonEvent cEvent( event ) ;
    cEvent.GetParameter<HICommand>(kEventParamDirectObject,typeHICommand,&command) ;

    wxMenuItem* item = NULL ;
    wxMenu* itemMenu = wxFindMenuFromMacCommand( command , item ) ;

    if ( item )
    {
        wxASSERT( itemMenu != NULL ) ;

        switch ( cEvent.GetKind() )
        {
            case kEventProcessCommand :
                if ( itemMenu->HandleCommandProcess( item, focus ) )
                    result = noErr;
            break ;

            case kEventCommandUpdateStatus:
                if ( itemMenu->HandleCommandUpdateStatus( item, focus ) )
                    result = noErr;
                break ;

            default :
                break ;
        }
    }
    return result ;
}

pascal OSStatus wxMacWindowEventHandler( EventHandlerCallRef handler , EventRef event , void *data )
{
    EventRef formerEvent = (EventRef) wxTheApp->MacGetCurrentEvent() ;
    EventHandlerCallRef formerEventHandlerCallRef = (EventHandlerCallRef) wxTheApp->MacGetCurrentEventHandlerCallRef() ;
    wxTheApp->MacSetCurrentEvent( event , handler ) ;
    OSStatus result = eventNotHandledErr ;

    switch ( GetEventClass( event ) )
    {
        case kEventClassCommand :
            result = wxMacWindowCommandEventHandler( handler , event , data ) ;
            break ;

        case kEventClassControl :
            result = wxMacWindowControlEventHandler( handler, event, data ) ;
            break ;

        case kEventClassService :
            result = wxMacWindowServiceEventHandler( handler, event , data ) ;
            break ;

        case kEventClassTextInput :
            result = wxMacUnicodeTextEventHandler( handler , event , data ) ;
            break ;

        default :
            break ;
    }

    wxTheApp->MacSetCurrentEvent( formerEvent, formerEventHandlerCallRef ) ;

    return result ;
}

DEFINE_ONE_SHOT_HANDLER_GETTER( wxMacWindowEventHandler )

// ---------------------------------------------------------------------------
// Scrollbar Tracking for all
// ---------------------------------------------------------------------------

pascal void wxMacLiveScrollbarActionProc( ControlRef control , ControlPartCode partCode ) ;
pascal void wxMacLiveScrollbarActionProc( ControlRef control , ControlPartCode partCode )
{
    if ( partCode != 0)
    {
        wxWindow*  wx = wxFindWindowFromWXWidget(  (WXWidget) control ) ;
        if ( wx )
        {
            wxEventType scrollEvent = wxEVT_NULL;
            switch ( partCode )
            {
            case kControlUpButtonPart:
                scrollEvent = wxEVT_SCROLL_LINEUP;
                break;

            case kControlDownButtonPart:
                scrollEvent = wxEVT_SCROLL_LINEDOWN;
                break;

            case kControlPageUpPart:
                scrollEvent = wxEVT_SCROLL_PAGEUP;
                break;

            case kControlPageDownPart:
                scrollEvent = wxEVT_SCROLL_PAGEDOWN;
                break;

            case kControlIndicatorPart:
                scrollEvent = wxEVT_SCROLL_THUMBTRACK;
                // when this is called as a live proc, mouse is always still down
                // so no need for thumbrelease
                // scrollEvent = wxEVT_SCROLL_THUMBRELEASE;
                break;
            }
            wx->TriggerScrollEvent(scrollEvent) ;
        }
    }
}
wxMAC_DEFINE_PROC_GETTER( ControlActionUPP , wxMacLiveScrollbarActionProc ) ;

wxWidgetImplType* wxWidgetImpl::CreateUserPane( wxWindowMac* wxpeer,
                            wxWindowMac* WXUNUSED(parent),
                            wxWindowID WXUNUSED(id),
                            const wxPoint& pos,
                            const wxSize& size,
                            long WXUNUSED(style),
                            long WXUNUSED(extraStyle))
{
    OSStatus err = noErr;
    Rect bounds = wxMacGetBoundsForControl( wxpeer , pos , size ) ;
    wxMacControl* c = new wxMacControl(wxpeer, false, true) ;
    UInt32 features = 0
        | kControlSupportsEmbedding
        | kControlSupportsLiveFeedback
        | kControlGetsFocusOnClick
//            | kControlHasSpecialBackground
//            | kControlSupportsCalcBestRect
        | kControlHandlesTracking
        | kControlSupportsFocus
        | kControlWantsActivate
        | kControlWantsIdle ;

    err =::CreateUserPaneControl( MAC_WXHWND(wxpeer->GetParent()->MacGetTopLevelWindowRef()) , &bounds, features , c->GetControlRefAddr() );
    verify_noerr( err );
    return c;
}


void wxMacControl::InstallEventHandler( WXWidget control )
{
    wxWidgetImpl::Associate( control ? control : (WXWidget) m_controlRef , this ) ;
    ::InstallControlEventHandler( control ? (ControlRef) control : m_controlRef , GetwxMacWindowEventHandlerUPP(),
        GetEventTypeCount(eventList), eventList, GetWXPeer(), NULL);
}

IMPLEMENT_DYNAMIC_CLASS( wxMacControl , wxWidgetImpl )

wxMacControl::wxMacControl()
{
    Init();
}

wxMacControl::wxMacControl(wxWindowMac* peer , bool isRootControl, bool isUserPane ) :
    wxWidgetImpl( peer, isRootControl, isUserPane )
{
    Init();
}

wxMacControl::~wxMacControl()
{
    if ( m_controlRef && !IsRootControl() )
    {
        wxASSERT_MSG( m_controlRef != NULL , wxT("Control Handle already NULL, Dispose called twice ?") );
        wxASSERT_MSG( IsValidControlHandle(m_controlRef) , wxT("Invalid Control Handle (maybe already released) in Dispose") );

        wxWidgetImpl::RemoveAssociations( this ) ;
        // we cannot check the ref count here anymore, as autorelease objects might delete their refs later
        // we can have situations when being embedded, where the control gets deleted behind our back, so only
        // CFRelease if we are safe
        if ( IsValidControlHandle(m_controlRef) )
            CFRelease(m_controlRef);
    }
    m_controlRef = NULL;
}

void wxMacControl::Init()
{
    m_controlRef = NULL;
    m_macControlEventHandler = NULL;
}

void wxMacControl::RemoveFromParent()
{
    // nothing to do here for carbon
    HIViewRemoveFromSuperview(m_controlRef);
}

void wxMacControl::Embed( wxWidgetImpl *parent )
{
    HIViewAddSubview((ControlRef)parent->GetWXWidget(), m_controlRef);
}

void wxMacControl::SetNeedsDisplay( const wxRect* rect )
{
    if ( !IsVisible() )
        return;

    if ( rect != NULL )
    {
        HIRect updatearea = CGRectMake( rect->x , rect->y , rect->width , rect->height);
        HIViewSetNeedsDisplayInRect( m_controlRef, &updatearea, true );
    }
    else
        HIViewSetNeedsDisplay( m_controlRef , true );
}

void wxMacControl::Raise()
{
    verify_noerr( HIViewSetZOrder( m_controlRef, kHIViewZOrderAbove, NULL ) );
}

void wxMacControl::Lower()
{
    verify_noerr( HIViewSetZOrder( m_controlRef, kHIViewZOrderBelow, NULL ) );
}

void wxMacControl::GetContentArea(int &left , int &top , int &width , int &height) const
{
    HIShapeRef rgn = NULL;
    Rect content ;

    if ( HIViewCopyShape(m_controlRef, kHIViewContentMetaPart, &rgn) == noErr)
    {
        CGRect cgrect;
        HIShapeGetBounds(rgn, &cgrect);
        content = (Rect){ (short)cgrect.origin.y,
                          (short)cgrect.origin.x,
                          (short)(cgrect.origin.y+cgrect.size.height),
                          (short)(cgrect.origin.x+cgrect.size.width) };
        CFRelease(rgn);
    }
    else
    {
        GetControlBounds(m_controlRef, &content);
        content.right -= content.left;
        content.left = 0;
        content.bottom -= content.top;
        content.top = 0;
    }

    left = content.left;
    top = content.top;

    width = content.right - content.left ;
    height = content.bottom - content.top ;
}

void wxMacControl::Move(int x, int y, int width, int height)
{
    UInt32 attr = 0 ;
    GetWindowAttributes( GetControlOwner(m_controlRef) , &attr ) ;

    HIRect hir = CGRectMake(x,y,width,height);
    if ( !(attr & kWindowCompositingAttribute) )
    {
        HIRect parent;
        HIViewGetFrame( HIViewGetSuperview(m_controlRef), &parent );
        hir.origin.x += parent.origin.x;
        hir.origin.y += parent.origin.y;
    }
    HIViewSetFrame ( m_controlRef , &hir );
}

void wxMacControl::GetPosition( int &x, int &y ) const
{
    Rect r;
    GetControlBounds( m_controlRef , &r );
    x = r.left;
    y = r.top;

    UInt32 attr = 0 ;
    GetWindowAttributes( GetControlOwner(m_controlRef) , &attr ) ;

    if ( !(attr & kWindowCompositingAttribute) )
    {
        HIRect parent;
        HIViewGetFrame( HIViewGetSuperview(m_controlRef), &parent );
        x -= (int)parent.origin.x;
        y -= (int)parent.origin.y;
    }

}

void wxMacControl::GetSize( int &width, int &height ) const
{
    Rect r;
    GetControlBounds( m_controlRef , &r );
    width = r.right - r.left;
    height = r.bottom - r.top;
}

void wxMacControl::SetControlSize( wxWindowVariant variant )
{
    ControlSize size ;
    switch ( variant )
    {
        case wxWINDOW_VARIANT_NORMAL :
            size = kControlSizeNormal;
            break ;

        case wxWINDOW_VARIANT_SMALL :
            size = kControlSizeSmall;
            break ;

        case wxWINDOW_VARIANT_MINI :
            // not always defined in the headers
            size = 3 ;
            break ;

        case wxWINDOW_VARIANT_LARGE :
            size = kControlSizeLarge;
            break ;

        default:
            wxFAIL_MSG(wxT("unexpected window variant"));
            break ;
    }

    SetData<ControlSize>(kControlEntireControl, kControlSizeTag, &size ) ;
}

void wxMacControl::ScrollRect( const wxRect *rect, int dx, int dy )
{
    if (GetNeedsDisplay() )
    {
        // because HIViewScrollRect does not scroll the already invalidated area we have two options:
        // in case there is already a pending redraw on that area
        // either immediate redraw or full invalidate
#if 1
        // is the better overall solution, as it does not slow down scrolling
        SetNeedsDisplay() ;
#else
        // this would be the preferred version for fast drawing controls
        HIViewRender(GetControlRef()) ;
#endif
    }

    // note there currently is a bug in OSX (10.3 +?) which makes inefficient refreshes in case an entire control
    // area is scrolled, this does not occur if width and height are 2 pixels less,
    // TODO: write optimal workaround

    HIRect scrollarea = CGRectMake( rect->x , rect->y , rect->width , rect->height);
    HIViewScrollRect ( m_controlRef , &scrollarea , dx ,dy );

#if 0
    // this would be the preferred version for fast drawing controls
    HIViewRender(GetControlRef()) ;
#endif
}

bool wxMacControl::CanFocus() const
{
    // TODO : evaluate performance hits by looking up this value, eventually cache the results for a 1 sec or so
    // CAUTION : the value returned currently is 0 or 2, I've also found values of 1 having the same meaning,
    // but the value range is nowhere documented
    Boolean keyExistsAndHasValidFormat ;
    CFIndex fullKeyboardAccess = CFPreferencesGetAppIntegerValue( CFSTR("AppleKeyboardUIMode" ) ,
        kCFPreferencesCurrentApplication, &keyExistsAndHasValidFormat );

    if ( keyExistsAndHasValidFormat && fullKeyboardAccess > 0 )
    {
        return true ;
    }
    else
    {
        UInt32 features = 0 ;
        GetControlFeatures( m_controlRef, &features ) ;

        return features & ( kControlSupportsFocus | kControlGetsFocusOnClick ) ;
    }
}

bool wxMacControl::GetNeedsDisplay() const
{
    return HIViewGetNeedsDisplay( m_controlRef );
}

void  wxWidgetImpl::Convert( wxPoint *pt , wxWidgetImpl *from , wxWidgetImpl *to )
{
    HIPoint hiPoint;

    hiPoint.x = pt->x;
    hiPoint.y = pt->y;
    HIViewConvertPoint( &hiPoint , (ControlRef) from->GetWXWidget() , (ControlRef) to->GetWXWidget()  );
    pt->x = (int)hiPoint.x;
    pt->y = (int)hiPoint.y;
}

bool wxMacControl::SetFocus()
{
    // as we cannot rely on the control features to find out whether we are in full keyboard mode,
    // we can only leave in case of an error

    OSStatus err = SetKeyboardFocus( GetControlOwner( m_controlRef ), m_controlRef, kControlFocusNextPart );
    if ( err == errCouldntSetFocus )
        return false ;
    SetUserFocusWindow(GetControlOwner( m_controlRef ) );

    return true;
}

bool wxMacControl::HasFocus() const
{
    ControlRef control;
    GetKeyboardFocus( GetUserFocusWindow() , &control );
    return control == m_controlRef;
}

void wxMacControl::SetCursor(const wxCursor& cursor)
{
    wxWindowMac *mouseWin = 0 ;
    WindowRef window = GetControlOwner( m_controlRef ) ;

    wxNonOwnedWindow* tlwwx = wxNonOwnedWindow::GetFromWXWindow( (WXWindow) window ) ;
    if ( tlwwx != NULL )
    {
        ControlPartCode part ;
        ControlRef control ;
        Point pt ;
        HIPoint hiPoint ;
        HIGetMousePosition(kHICoordSpaceWindow, window, &hiPoint);
        pt.h = hiPoint.x;
        pt.v = hiPoint.y;
        control = FindControlUnderMouse( pt , window , &part ) ;
        if ( control )
            mouseWin = wxFindWindowFromWXWidget( (WXWidget) control ) ;
    }

    if ( mouseWin == tlwwx && !wxIsBusy() )
        cursor.MacInstall() ;
}

void wxMacControl::CaptureMouse()
{
}

void wxMacControl::ReleaseMouse()
{
}

//
// subclass specifics
//

OSStatus wxMacControl::GetData(ControlPartCode inPartCode , ResType inTag , Size inBufferSize , void * inOutBuffer , Size * outActualSize ) const
{
    return ::GetControlData( m_controlRef , inPartCode , inTag , inBufferSize , inOutBuffer , outActualSize );
}

OSStatus wxMacControl::GetDataSize(ControlPartCode inPartCode , ResType inTag , Size * outActualSize ) const
{
    return ::GetControlDataSize( m_controlRef , inPartCode , inTag , outActualSize );
}

OSStatus wxMacControl::SetData(ControlPartCode inPartCode , ResType inTag , Size inSize , const void * inData)
{
    return ::SetControlData( m_controlRef , inPartCode , inTag , inSize , inData );
}

OSStatus wxMacControl::SendEvent( EventRef event , OptionBits inOptions )
{
    return SendEventToEventTargetWithOptions( event,
        HIObjectGetEventTarget( (HIObjectRef) m_controlRef ), inOptions );
}

OSStatus wxMacControl::SendHICommand( HICommand &command , OptionBits inOptions )
{
    wxMacCarbonEvent event( kEventClassCommand , kEventCommandProcess );

    event.SetParameter<HICommand>(kEventParamDirectObject,command);

    return SendEvent( event , inOptions );
}

OSStatus wxMacControl::SendHICommand( UInt32 commandID , OptionBits inOptions  )
{
    HICommand command;

    memset( &command, 0 , sizeof(command) );
    command.commandID = commandID;
    return SendHICommand( command , inOptions );
}

void wxMacControl::PerformClick()
{
    HIViewSimulateClick (m_controlRef, kControlButtonPart, 0, NULL );
}

wxInt32 wxMacControl::GetValue() const
{
    return ::GetControl32BitValue( m_controlRef );
}

wxInt32 wxMacControl::GetMaximum() const
{
    return ::GetControl32BitMaximum( m_controlRef );
}

wxInt32 wxMacControl::GetMinimum() const
{
    return ::GetControl32BitMinimum( m_controlRef );
}

void wxMacControl::SetValue( wxInt32 v )
{
    ::SetControl32BitValue( m_controlRef , v );
}

void wxMacControl::SetMinimum( wxInt32 v )
{
    ::SetControl32BitMinimum( m_controlRef , v );
}

void wxMacControl::SetMaximum( wxInt32 v )
{
    ::SetControl32BitMaximum( m_controlRef , v );
}

void wxMacControl::SetValueAndRange( SInt32 value , SInt32 minimum , SInt32 maximum )
{
    ::SetControl32BitMinimum( m_controlRef , minimum );
    ::SetControl32BitMaximum( m_controlRef , maximum );
    ::SetControl32BitValue( m_controlRef , value );
}

void wxMacControl::VisibilityChanged(bool WXUNUSED(shown))
{
}

void wxMacControl::SuperChangedPosition()
{
}

void wxMacControl::SetFont( const wxFont & font , const wxColour& foreground , long windowStyle, bool ignoreBlack )
{
    m_font = font;
    HIViewPartCode part = 0;
    HIThemeTextHorizontalFlush flush = kHIThemeTextHorizontalFlushDefault;
    if ( ( windowStyle & wxALIGN_MASK ) & wxALIGN_CENTER_HORIZONTAL )
        flush = kHIThemeTextHorizontalFlushCenter;
    else if ( ( windowStyle & wxALIGN_MASK ) & wxALIGN_RIGHT )
        flush = kHIThemeTextHorizontalFlushRight;
    HIViewSetTextFont( m_controlRef , part , (CTFontRef) font.OSXGetCTFont() );
    HIViewSetTextHorizontalFlush( m_controlRef, part, flush );

    if ( foreground != *wxBLACK || ignoreBlack == false )
    {
        ControlFontStyleRec fontStyle;
        foreground.GetRGBColor( &fontStyle.foreColor );
        fontStyle.flags = kControlUseForeColorMask;
        ::SetControlFontStyle( m_controlRef , &fontStyle );
    }
#if wxOSX_USE_ATSU_TEXT
    ControlFontStyleRec fontStyle;
    if ( font.MacGetThemeFontID() != kThemeCurrentPortFont )
    {
        switch ( font.MacGetThemeFontID() )
        {
            case kThemeSmallSystemFont :
                fontStyle.font = kControlFontSmallSystemFont;
                break;

            case 109 : // mini font
                fontStyle.font = -5;
                break;

            case kThemeSystemFont :
                fontStyle.font = kControlFontBigSystemFont;
                break;

            default :
                fontStyle.font = kControlFontBigSystemFont;
                break;
        }

        fontStyle.flags = kControlUseFontMask;
    }
    else
    {
        fontStyle.font = font.MacGetFontNum();
        fontStyle.style = font.MacGetFontStyle();
        fontStyle.size = font.GetPointSize();
        fontStyle.flags = kControlUseFontMask | kControlUseFaceMask | kControlUseSizeMask;
    }

    fontStyle.just = teJustLeft;
    fontStyle.flags |= kControlUseJustMask;
    if ( ( windowStyle & wxALIGN_MASK ) & wxALIGN_CENTER_HORIZONTAL )
        fontStyle.just = teJustCenter;
    else if ( ( windowStyle & wxALIGN_MASK ) & wxALIGN_RIGHT )
        fontStyle.just = teJustRight;


    // we only should do this in case of a non-standard color, as otherwise 'disabled' controls
    // won't get grayed out by the system anymore

    if ( foreground != *wxBLACK || ignoreBlack == false )
    {
        foreground.GetRGBColor( &fontStyle.foreColor );
        fontStyle.flags |= kControlUseForeColorMask;
    }

    ::SetControlFontStyle( m_controlRef , &fontStyle );
#endif
}

void wxMacControl::SetBackgroundColour( const wxColour &WXUNUSED(col) )
{
//    HITextViewSetBackgroundColor( m_textView , color );
}

bool wxMacControl::SetBackgroundStyle(wxBackgroundStyle style)
{
    if ( style != wxBG_STYLE_PAINT )
    {
        OSStatus err = HIViewChangeFeatures(m_controlRef , 0 , kHIViewIsOpaque);
        verify_noerr( err );
    }
    else
    {
        OSStatus err = HIViewChangeFeatures(m_controlRef , kHIViewIsOpaque , 0);
        verify_noerr( err );
    }

    return true ;
}

void wxMacControl::SetRange( SInt32 minimum , SInt32 maximum )
{
    ::SetControl32BitMinimum( m_controlRef , minimum );
    ::SetControl32BitMaximum( m_controlRef , maximum );
}

short wxMacControl::HandleKey( SInt16 keyCode,  SInt16 charCode, EventModifiers modifiers )
{
    return HandleControlKey( m_controlRef , keyCode , charCode , modifiers );
}

void wxMacControl::SetActionProc( ControlActionUPP   actionProc )
{
    SetControlAction( m_controlRef , actionProc );
}

SInt32 wxMacControl::GetViewSize() const
{
    return GetControlViewSize( m_controlRef );
}

bool wxMacControl::IsVisible() const
{
    return IsControlVisible( m_controlRef );
}

void wxMacControl::SetVisibility( bool visible )
{
    SetControlVisibility( m_controlRef , visible, true );
}

bool wxMacControl::IsEnabled() const
{
    return IsControlEnabled( m_controlRef );
}

bool wxMacControl::IsActive() const
{
    return IsControlActive( m_controlRef );
}

void wxMacControl::Enable( bool enable )
{
    if ( enable )
        EnableControl( m_controlRef );
    else
        DisableControl( m_controlRef );
}

void wxMacControl::SetDrawingEnabled( bool enable )
{
    if ( enable )
    {
        HIViewSetDrawingEnabled( m_controlRef , true );
        HIViewSetNeedsDisplay( m_controlRef, true);
    }
    else
    {
        HIViewSetDrawingEnabled( m_controlRef , false );
    }
}

void wxMacControl::GetRectInWindowCoords( Rect *r )
{
    GetControlBounds( m_controlRef , r ) ;

    WindowRef tlwref = GetControlOwner( m_controlRef ) ;

    wxNonOwnedWindow* tlwwx = wxNonOwnedWindow::GetFromWXWindow( (WXWindow) tlwref ) ;
    if ( tlwwx != NULL )
    {
        ControlRef rootControl = tlwwx->GetPeer()->GetControlRef() ;
        HIPoint hiPoint = CGPointMake( 0 , 0 ) ;
        HIViewConvertPoint( &hiPoint , HIViewGetSuperview(m_controlRef) , rootControl ) ;
        OffsetRect( r , (short) hiPoint.x , (short) hiPoint.y ) ;
    }
}

void wxMacControl::GetBestRect( wxRect *rect ) const
{
    short   baselineoffset;
    Rect r = {0,0,0,0};

    GetBestControlRect( m_controlRef , &r , &baselineoffset );
    *rect = wxRect( r.left, r.top, r.right - r.left, r.bottom-r.top );
}

void wxMacControl::GetBestRect( Rect *r ) const
{
    short   baselineoffset;
    GetBestControlRect( m_controlRef , r , &baselineoffset );
}

void wxMacControl::SetLabel( const wxString &title , wxFontEncoding encoding)
{
    SetControlTitleWithCFString( m_controlRef , wxCFStringRef( title , encoding ) );
}

void wxMacControl::GetFeatures( UInt32 * features )
{
    GetControlFeatures( m_controlRef , features );
}

void wxMacControl::PulseGauge()
{
}

// SetNeedsDisplay would not invalidate the children
static void InvalidateControlAndChildren( HIViewRef control )
{
    HIViewSetNeedsDisplay( control , true );
    UInt16 childrenCount = 0;
    OSStatus err = CountSubControls( control , &childrenCount );
    if ( err == errControlIsNotEmbedder )
        return;

    wxASSERT_MSG( err == noErr , wxT("Unexpected error when accessing subcontrols") );

    for ( UInt16 i = childrenCount; i >=1; --i )
    {
        HIViewRef child;

        err = GetIndexedSubControl( control , i , & child );
        if ( err == errControlIsNotEmbedder )
            return;

        InvalidateControlAndChildren( child );
    }
}

void wxMacControl::InvalidateWithChildren()
{
    InvalidateControlAndChildren( m_controlRef );
}

OSType wxMacCreator = 'WXMC';
OSType wxMacControlProperty = 'MCCT';

void wxMacControl::SetReferenceInNativeControl()
{
    void * data = this;
    verify_noerr( SetControlProperty ( m_controlRef ,
        wxMacCreator,wxMacControlProperty, sizeof(data), &data ) );
}

wxMacControl* wxMacControl::GetReferenceFromNativeControl(ControlRef control)
{
    wxMacControl* ctl = NULL;
    ByteCount actualSize;
    if ( GetControlProperty( control ,wxMacCreator,wxMacControlProperty, sizeof(ctl) ,
        &actualSize , &ctl ) == noErr )
    {
        return ctl;
    }
    return NULL;
}

wxBitmap wxMacControl::GetBitmap() const
{
    return wxNullBitmap;
}

void wxMacControl::SetBitmap( const wxBitmap& WXUNUSED(bmp) )
{
    // implemented in the respective subclasses
}

void wxMacControl::SetBitmapPosition( wxDirection WXUNUSED(dir) )
{
    // implemented in the same subclasses that implement SetBitmap()
}

void wxMacControl::SetScrollThumb( wxInt32 WXUNUSED(pos), wxInt32 WXUNUSED(viewsize) )
{
    // implemented in respective subclass
}

//
// Tab Control
//

OSStatus wxMacControl::SetTabEnabled( SInt16 tabNo , bool enable )
{
    return ::SetTabEnabled( m_controlRef , tabNo , enable );
}



// Control Factory

wxWidgetImplType* wxWidgetImpl::CreateContentView( wxNonOwnedWindow* now )
{
    // There is a bug in 10.2.X for ::GetRootControl returning the window view instead of
    // the content view, so we have to retrieve it explicitly

    wxMacControl* contentview = new wxMacControl(now , true /*isRootControl*/);
    HIViewFindByID( HIViewGetRoot( (WindowRef) now->GetWXWindow() ) , kHIViewWindowContentID ,
        contentview->GetControlRefAddr() ) ;
    if ( !contentview->IsOk() )
    {
        // compatibility mode fallback
        GetRootControl( (WindowRef) now->GetWXWindow() , contentview->GetControlRefAddr() ) ;
    }

    // the root control level handler
    if ( !now->IsNativeWindowWrapper() )
        contentview->InstallEventHandler() ;

    return contentview;
}
