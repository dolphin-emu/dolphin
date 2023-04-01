/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/radiobut.mm
// Purpose:     wxRadioButton
// Author:      Stefan Csomor
// Modified by:
// Created:     ??/??/98
// Copyright:   (c) AUTHOR
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_RADIOBTN

#include "wx/radiobut.h"
#include "wx/osx/private.h"

// ugly workaround for the fact that since 10.8 is treating all subviews of a view
// which share the same action selector as a group and sets their value automatically
// so we are creating <maxAlternateActions> different selectors and iterate through them
// as we are assigning their selectors as actions

#include <objc/objc-runtime.h>

const int maxAlternateActions = 100;
NSString* alternateActionsSelector = @"controlAction%d:";

extern void wxOSX_controlAction(NSView* self, SEL _cmd, id sender);

@interface wxNSRadioButton : NSButton
{
    NSTrackingRectTag rectTag;
}

@end

@implementation wxNSRadioButton
+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
        for ( int i = 1 ; i <= maxAlternateActions ; i++ )
        {
            class_addMethod(self, NSSelectorFromString([NSString stringWithFormat: alternateActionsSelector, i]), (IMP) wxOSX_controlAction, "v@:@" );
        }
    }
}

- (void) setState: (NSInteger) v
{
    [super setState:v];
    //    [[self cell] setState:v];
}

- (int) intValue
{
    switch ( [self state] )
    {
        case NSOnState:
            return 1;
        case NSMixedState:
            return 2;
        default:
            return 0;
    }
}

- (void) setIntValue: (int) v
{
    switch( v )
    {
        case 2:
            [self setState:NSMixedState];
            break;
        case 1:
            [self setState:NSOnState];
            break;
        default :
            [self setState:NSOffState];
            break;
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

@end

wxWidgetImplType* wxWidgetImpl::CreateRadioButton( wxWindowMac* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    const wxString& WXUNUSED(label),
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxNSRadioButton* v = [[wxNSRadioButton alloc] initWithFrame:r];

    [v setButtonType:NSRadioButton];

    static int alternateAction = 1;
 
    [v setAction: NSSelectorFromString([NSString stringWithFormat: alternateActionsSelector, alternateAction])];
    if ( ++alternateAction > maxAlternateActions )
        alternateAction = 1;

    wxWidgetCocoaImpl* c = new wxWidgetCocoaImpl( wxpeer, v );
    return c;
}

#endif
