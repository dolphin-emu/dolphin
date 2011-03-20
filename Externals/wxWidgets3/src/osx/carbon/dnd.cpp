///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/dnd.cpp
// Purpose:     wxDropTarget, wxDropSource implementations
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// RCS-ID:      $Id: dnd.cpp 63431 2010-02-09 01:24:22Z RD $
// Copyright:   (c) 1998 Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_DRAG_AND_DROP

#include "wx/dnd.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/toplevel.h"
    #include "wx/gdicmn.h"
#endif // WX_PRECOMP

#include "wx/osx/private.h"

// ----------------------------------------------------------------------------
// globals
// ----------------------------------------------------------------------------

typedef struct
{
    wxWindow *m_currentTargetWindow;
    wxDropTarget *m_currentTarget;
    wxDropSource *m_currentSource;
    wxDragResult m_result;
    int m_flags;
} MacTrackingGlobals;

MacTrackingGlobals gTrackingGlobals;

void wxMacEnsureTrackingHandlersInstalled();

OSStatus wxMacPromiseKeeper(PasteboardRef WXUNUSED(inPasteboard),
                            PasteboardItemID WXUNUSED(inItem),
                            CFStringRef WXUNUSED(inFlavorType),
                            void * WXUNUSED(inContext))
{
    OSStatus  err = noErr;

    // we might add promises here later, inContext is the wxDropSource*

    return err;
}

wxDropTarget::wxDropTarget( wxDataObject *data )
            : wxDropTargetBase( data )
{
    wxMacEnsureTrackingHandlersInstalled();
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
    wxMacEnsureTrackingHandlersInstalled();

    m_window = win;
}

wxDropSource* wxDropSource::GetCurrentDropSource()
{
    return gTrackingGlobals.m_currentSource;
}

wxDropSource::wxDropSource(wxDataObject& data,
                           wxWindow *win,
                           const wxCursor &cursorCopy,
                           const wxCursor &cursorMove,
                           const wxCursor &cursorStop)
            : wxDropSourceBase(cursorCopy, cursorMove, cursorStop)
{
    wxMacEnsureTrackingHandlersInstalled();

    SetData( data );
    m_window = win;
}

wxDragResult wxDropSource::DoDragDrop(int flags)
{
    wxASSERT_MSG( m_data, wxT("Drop source: no data") );

    if ((m_data == NULL) || (m_data->GetFormatCount() == 0))
        return (wxDragResult)wxDragNone;

    DragReference theDrag;
    RgnHandle dragRegion;
    OSStatus err = noErr;
    PasteboardRef   pasteboard;

    // add data to drag

    err = PasteboardCreate( kPasteboardUniqueName, &pasteboard );
    if ( err != noErr )
        return wxDragNone;

    // we add a dummy promise keeper because of strange messages when linking against carbon debug
    err = PasteboardSetPromiseKeeper( pasteboard, wxMacPromiseKeeper, this );
    if ( err != noErr )
    {
        CFRelease( pasteboard );
        return wxDragNone;
    }

    err = PasteboardClear( pasteboard );
    if ( err != noErr )
    {
        CFRelease( pasteboard );
        return wxDragNone;
    }
    PasteboardSynchronize( pasteboard );

    m_data->AddToPasteboard( pasteboard, 1 );

    if (NewDragWithPasteboard( pasteboard , &theDrag) != noErr)
    {
        CFRelease( pasteboard );
        return wxDragNone;
    }

    dragRegion = NewRgn();
    RgnHandle tempRgn = NewRgn();

    EventRecord rec;
    ConvertEventRefToEventRecord(  (EventRef) wxTheApp->MacGetCurrentEvent(), &rec );

    const short dragRegionOuterBoundary = 10;
    const short dragRegionInnerBoundary = 9;

    SetRectRgn(
        dragRegion,
        rec.where.h - dragRegionOuterBoundary,
        rec.where.v  - dragRegionOuterBoundary,
        rec.where.h + dragRegionOuterBoundary,
        rec.where.v + dragRegionOuterBoundary );

    SetRectRgn(
        tempRgn,
        rec.where.h - dragRegionInnerBoundary,
        rec.where.v - dragRegionInnerBoundary,
        rec.where.h + dragRegionInnerBoundary,
        rec.where.v + dragRegionInnerBoundary );

    DiffRgn( dragRegion, tempRgn, dragRegion );
    DisposeRgn( tempRgn );

    // TODO: work with promises in order to return data
    // only when drag was successfully completed

    gTrackingGlobals.m_currentSource = this;
    gTrackingGlobals.m_result = wxDragNone;
    gTrackingGlobals.m_flags = flags;

    err = TrackDrag( theDrag, &rec, dragRegion );

    DisposeRgn( dragRegion );
    DisposeDrag( theDrag );
    CFRelease( pasteboard );
    gTrackingGlobals.m_currentSource = NULL;

    return gTrackingGlobals.m_result;
}

bool gTrackingGlobalsInstalled = false;

// passing the globals via refcon is not needed by the CFM and later architectures anymore
// but I'll leave it in there, just in case...

pascal OSErr wxMacWindowDragTrackingHandler(
    DragTrackingMessage theMessage, WindowPtr theWindow,
    void *handlerRefCon, DragReference theDrag );
pascal OSErr wxMacWindowDragReceiveHandler(
    WindowPtr theWindow, void *handlerRefCon,
    DragReference theDrag );

void wxMacEnsureTrackingHandlersInstalled()
{
    if ( !gTrackingGlobalsInstalled )
    {
        OSStatus err;

        err = InstallTrackingHandler( NewDragTrackingHandlerUPP(wxMacWindowDragTrackingHandler), 0L, &gTrackingGlobals );
        verify_noerr( err );

        err = InstallReceiveHandler( NewDragReceiveHandlerUPP(wxMacWindowDragReceiveHandler), 0L, &gTrackingGlobals );
        verify_noerr( err );

        gTrackingGlobalsInstalled = true;
    }
}

pascal OSErr wxMacWindowDragTrackingHandler(
    DragTrackingMessage theMessage, WindowPtr theWindow,
    void *handlerRefCon, DragReference theDrag )
{
    MacTrackingGlobals* trackingGlobals = (MacTrackingGlobals*) handlerRefCon;

    Point mouse, localMouse;
    DragAttributes attributes;

    GetDragAttributes( theDrag, &attributes );
    PasteboardRef   pasteboard = 0;
    GetDragPasteboard( theDrag, &pasteboard );
    wxNonOwnedWindow* toplevel = wxNonOwnedWindow::GetFromWXWindow( (WXWindow) theWindow );

    bool optionDown = GetCurrentKeyModifiers() & optionKey;
    wxDragResult result = optionDown ? wxDragCopy : wxDragMove;

    switch (theMessage)
    {
        case kDragTrackingEnterHandler:
        case kDragTrackingLeaveHandler:
            break;

        case kDragTrackingEnterWindow:
            if (trackingGlobals != NULL)
            {
                trackingGlobals->m_currentTargetWindow = NULL;
                trackingGlobals->m_currentTarget = NULL;
            }
            break;

        case kDragTrackingInWindow:
            if (trackingGlobals == NULL)
                break;
            if (toplevel == NULL)
                break;

            GetDragMouse( theDrag, &mouse, 0L );
            {
                int x = mouse.h ;
                int y = mouse.v ;
                toplevel->GetNonOwnedPeer()->ScreenToWindow( &x, &y );
                localMouse.h = x;
                localMouse.v = y;

                {
                    wxWindow *win = NULL;
                    ControlPartCode controlPart;
                    ControlRef control = FindControlUnderMouse( localMouse, theWindow, &controlPart );
                    if ( control )
                        win = wxFindWindowFromWXWidget( (WXWidget) control );
                    else
                        win = toplevel;

                    int localx, localy;
                    localx = localMouse.h;
                    localy = localMouse.v;

                    if ( win )
                        win->MacRootWindowToWindow( &localx, &localy );
                    if ( win != trackingGlobals->m_currentTargetWindow )
                    {
                        if ( trackingGlobals->m_currentTargetWindow )
                        {
                            // this window is left
                            if ( trackingGlobals->m_currentTarget )
                            {
                                HideDragHilite( theDrag );
                                trackingGlobals->m_currentTarget->SetCurrentDragPasteboard( pasteboard );
                                trackingGlobals->m_currentTarget->OnLeave();
                                trackingGlobals->m_currentTarget = NULL;
                                trackingGlobals->m_currentTargetWindow = NULL;
                            }
                        }

                        if ( win )
                        {
                            // this window is entered
                            trackingGlobals->m_currentTargetWindow = win;
                            trackingGlobals->m_currentTarget = win->GetDropTarget();
                            {
                                if ( trackingGlobals->m_currentTarget )
                                {
                                    trackingGlobals->m_currentTarget->SetCurrentDragPasteboard( pasteboard );
                                    result = trackingGlobals->m_currentTarget->OnEnter( localx, localy, result );
                                }

                                if ( result != wxDragNone )
                                {
                                    int x, y;

                                    x = y = 0;
                                    win->MacWindowToRootWindow( &x, &y );
                                    RgnHandle hiliteRgn = NewRgn();
                                    Rect r = { y, x, y + win->GetSize().y, x + win->GetSize().x };
                                    RectRgn( hiliteRgn, &r );
                                    ShowDragHilite( theDrag, hiliteRgn, true );
                                    DisposeRgn( hiliteRgn );
                                }
                            }
                        }
                    }
                    else
                    {
                        if ( trackingGlobals->m_currentTarget )
                        {
                            trackingGlobals->m_currentTarget->SetCurrentDragPasteboard( pasteboard );
                            result = trackingGlobals->m_currentTarget->OnDragOver( localx, localy, result );
                        }
                    }

                    // set cursor for OnEnter and OnDragOver
                    if ( trackingGlobals->m_currentSource && !trackingGlobals->m_currentSource->GiveFeedback( result ) )
                    {
                        if ( !trackingGlobals->m_currentSource->MacInstallDefaultCursor( result ) )
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
                                wxCursor cursor( cursorID );
                                cursor.MacInstall();
                            }
                        }
                    }
                }
            }
            break;

        case kDragTrackingLeaveWindow:
            if (trackingGlobals == NULL)
                break;

            if (trackingGlobals->m_currentTarget)
            {
                trackingGlobals->m_currentTarget->SetCurrentDragPasteboard( pasteboard );
                trackingGlobals->m_currentTarget->OnLeave();
                HideDragHilite( theDrag );
                trackingGlobals->m_currentTarget = NULL;
            }
            trackingGlobals->m_currentTargetWindow = NULL;
            break;

        default:
            break;
    }

    return noErr;
}

pascal OSErr wxMacWindowDragReceiveHandler(
    WindowPtr theWindow,
    void *handlerRefCon,
    DragReference theDrag)
{
    MacTrackingGlobals* trackingGlobals = (MacTrackingGlobals*)handlerRefCon;
    if ( trackingGlobals->m_currentTarget )
    {
        Point mouse, localMouse;
        int localx, localy;

        PasteboardRef   pasteboard = 0;
        GetDragPasteboard( theDrag, &pasteboard );
        trackingGlobals->m_currentTarget->SetCurrentDragPasteboard( pasteboard );
        GetDragMouse( theDrag, &mouse, 0L );
        localMouse = mouse;
        localx = localMouse.h;
        localy = localMouse.v;
        wxNonOwnedWindow* tlw = wxNonOwnedWindow::GetFromWXWindow((WXWindow) theWindow);
        if ( tlw )
            tlw->GetNonOwnedPeer()->ScreenToWindow( &localx, &localy );

        // TODO : should we use client coordinates?
        if ( trackingGlobals->m_currentTargetWindow )
            trackingGlobals->m_currentTargetWindow->MacRootWindowToWindow( &localx, &localy );
        if ( trackingGlobals->m_currentTarget->OnDrop( localx, localy ) )
        {
            // the option key indicates copy in Mac UI, if it's not pressed do
            // move by default if it's allowed at all
            wxDragResult
                result = !(trackingGlobals->m_flags & wxDrag_AllowMove) ||
                            (GetCurrentKeyModifiers() & optionKey)
                            ? wxDragCopy
                            : wxDragMove;
            trackingGlobals->m_result =
                trackingGlobals->m_currentTarget->OnData( localx, localy, result );
        }
    }

    return noErr;
}

#endif // wxUSE_DRAG_AND_DROP

