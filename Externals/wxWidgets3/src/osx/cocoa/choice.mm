/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/choice.mm
// Purpose:     wxChoice
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_CHOICE

#include "wx/choice.h"

#ifndef WX_PRECOMP
    #include "wx/menu.h"
    #include "wx/dcclient.h"
#endif

#include "wx/osx/private.h"

@interface wxNSPopUpButton : NSPopUpButton
{
}

@end

@implementation wxNSPopUpButton

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
    }
}

- (int) intValue
{
   return [self indexOfSelectedItem];
}

- (void) setIntValue: (int) v
{
    [self selectItemAtIndex:v];
}

@end

@interface NSView(PossibleSizeMethods)
- (NSControlSize)controlSize;
@end

class wxChoiceCocoaImpl : public wxWidgetCocoaImpl
{
public:
    wxChoiceCocoaImpl(wxWindowMac *wxpeer, wxNSPopUpButton *v)
    : wxWidgetCocoaImpl(wxpeer, v)
    {
    }
    
    void GetLayoutInset(int &left , int &top , int &right, int &bottom) const wxOVERRIDE
    {
        left = top = right = bottom = 0;
        NSControlSize size = NSRegularControlSize;
        if ( [m_osxView respondsToSelector:@selector(controlSize)] )
            size = [m_osxView controlSize];
        else if ([m_osxView respondsToSelector:@selector(cell)])
        {
            id cell = [(id)m_osxView cell];
            if ([cell respondsToSelector:@selector(controlSize)])
                size = [cell controlSize];
        }
        
        switch( size )
        {
            case NSRegularControlSize:
                left = right = 3;
                top = 2;
                bottom = 3;
                break;
            case NSSmallControlSize:
                left = right = 3;
                top = 1;
                bottom = 3;
                break;
            case NSMiniControlSize:
                left = 1;
                right = 2;
                top = 0;
                bottom = 0;
                break;
        }
    }
};

wxWidgetImplType* wxWidgetImpl::CreateChoice( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    wxMenu* menu,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxNSPopUpButton* v = [[wxNSPopUpButton alloc] initWithFrame:r pullsDown:NO];
    [v setMenu: menu->GetHMenu()];
    [v setAutoenablesItems:NO];
    wxWidgetCocoaImpl* c = new wxChoiceCocoaImpl( wxpeer, v );
    return c;
}

#endif // wxUSE_CHOICE
