/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/scrolbar.mm
// Purpose:     wxScrollBar
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:       wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/scrolbar.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/settings.h"
#endif

#include "wx/math.h"
#include "wx/osx/private.h"

@interface wxNSScroller : NSScroller
{
}
@end

@implementation wxNSScroller

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods(self);
    }
}

@end

class wxOSXScrollBarCocoaImpl : public wxWidgetCocoaImpl
{
public :
    wxOSXScrollBarCocoaImpl( wxWindowMac* peer, WXWidget w) : wxWidgetCocoaImpl( peer, w )
    {
        m_maximum = 1;
    }

    void SetMaximum(wxInt32 v) wxOVERRIDE
    {
        m_maximum = (v == 0) ? 1 : v;
    }

    void    SetScrollThumb( wxInt32 value, wxInt32 thumbSize ) wxOVERRIDE
    {
        double v = ((double) value)/m_maximum;
        double t = ((double) thumbSize)/(m_maximum+thumbSize);
        [(wxNSScroller*) m_osxView setDoubleValue:v];
        [(wxNSScroller*) m_osxView setKnobProportion:t];
    }

    virtual wxInt32 GetValue() const wxOVERRIDE
    {
        return wxRound([(wxNSScroller*) m_osxView floatValue] * m_maximum);
    }

    virtual wxInt32 GetMaximum() const wxOVERRIDE
    {
        return m_maximum;
    }

    virtual void controlAction(WXWidget slf, void* _cmd, void *sender) wxOVERRIDE;
    virtual void mouseEvent(WX_NSEvent event, WXWidget slf, void* _cmd) wxOVERRIDE;
protected:
    wxInt32 m_maximum;
};

// we will have a mouseDown, then in the native
// implementation of mouseDown the tracking code
// is calling clickedAction, therefore we wire this
// to thumbtrack and only after super mouseDown
// returns we will call the thumbrelease

void wxOSXScrollBarCocoaImpl::controlAction( WXWidget WXUNUSED(slf), void *WXUNUSED(_cmd), void *WXUNUSED(sender))
{
    wxEventType scrollEvent = wxEVT_NULL;
    switch ([(NSScroller*)m_osxView hitPart])
    {
    case NSScrollerIncrementLine:
        scrollEvent = wxEVT_SCROLL_LINEDOWN;
        break;
    case NSScrollerIncrementPage:
        scrollEvent = wxEVT_SCROLL_PAGEDOWN;
        break;
    case NSScrollerDecrementLine:
        scrollEvent = wxEVT_SCROLL_LINEUP;
        break;
    case NSScrollerDecrementPage:
        scrollEvent = wxEVT_SCROLL_PAGEUP;
        break;
    case NSScrollerKnob:
    case NSScrollerKnobSlot:
        scrollEvent = wxEVT_SCROLL_THUMBTRACK;
        break;
    case NSScrollerNoPart:
    default:
        return;
    }

    wxWindow* wxpeer = (wxWindow*) GetWXPeer();
    if ( wxpeer )
        wxpeer->TriggerScrollEvent(scrollEvent);
}

void wxOSXScrollBarCocoaImpl::mouseEvent(WX_NSEvent event, WXWidget slf, void *_cmd)
{
    wxWidgetCocoaImpl::mouseEvent(event, slf, _cmd);

    // send a release event in case we've been tracking the thumb
    if ( strcmp( sel_getName((SEL) _cmd) , "mouseDown:") == 0 )
    {
        NSScrollerPart hit = [(NSScroller*)m_osxView hitPart];
        if ( (hit == NSScrollerKnob || hit == NSScrollerKnobSlot) )
        {
            wxWindow* wxpeer = (wxWindow*) GetWXPeer();
            if ( wxpeer )
                wxpeer->OSXHandleClicked(0);
        }
    }
}

wxWidgetImplType* wxWidgetImpl::CreateScrollBar( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long style,
                                    long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    // the creation rect defines the orientation
    NSRect createRect = ( style & wxSB_HORIZONTAL ) ? NSMakeRect(r.origin.x, r.origin.y , 17, 16) :
        NSMakeRect(r.origin.x, r.origin.y , 16, 17);
    wxNSScroller* v = [[wxNSScroller alloc] initWithFrame:createRect];
    [v setFrame:r];

    wxWidgetCocoaImpl* c = new wxOSXScrollBarCocoaImpl( wxpeer, v );
    [v setEnabled:YES];
    return c;
}
