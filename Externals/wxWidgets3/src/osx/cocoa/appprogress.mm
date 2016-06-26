/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/appprogress.mm
// Purpose:     wxAppProgressIndicator OSX implemenation
// Author:      Tobias Taschner
// Created:     2014-10-22
// Copyright:   (c) 2014 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/appprogress.h"
#include "wx/osx/private.h"

@interface wxAppProgressDockIcon : NSObject
{
    NSProgressIndicator* m_progIndicator;
    NSDockTile* m_dockTile;
}

- (id)init;

- (void)setProgress: (double)value;

@end

@implementation wxAppProgressDockIcon

- (id)init
{
    self = [super init];
    if (self) {
        m_dockTile = [NSApplication sharedApplication].dockTile;
        NSImageView* iv = [[NSImageView alloc] init];
        [iv setImage:[NSApplication sharedApplication].applicationIconImage];
        [m_dockTile setContentView:iv];

        m_progIndicator = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(0.0f, 16.0f, m_dockTile.size.width, 24.)];
        m_progIndicator.style = NSProgressIndicatorBarStyle;
        [m_progIndicator setIndeterminate:NO];
        [iv addSubview:m_progIndicator];

        [m_progIndicator setBezeled:YES];
        [m_progIndicator setMinValue:0];
        [m_progIndicator setMaxValue:1];
        [m_progIndicator release];
        [self setProgress:0.0];
    }
    return self;
}

- (void)setProgress: (double)value
{
    [m_progIndicator setHidden:NO];
    [m_progIndicator setDoubleValue:value];

    [m_dockTile display];
}

- (void)setIndeterminate: (bool)indeterminate
{
    [m_progIndicator setIndeterminate:indeterminate];

    [m_dockTile display];
}

- (void)reset
{
    [m_dockTile setContentView:nil];
}

@end

wxAppProgressIndicator::wxAppProgressIndicator(wxWindow* WXUNUSED(parent), int maxValue ):
    m_maxValue(maxValue)
{
    wxAppProgressDockIcon* dockIcon = [[[wxAppProgressDockIcon alloc] init] retain];

    m_dockIcon = dockIcon;
}

wxAppProgressIndicator::~wxAppProgressIndicator()
{
    Reset();

    NSObject* obj = (NSObject*) m_dockIcon;
    [obj release];
}

bool wxAppProgressIndicator::IsAvailable() const
{
    return true;
}

void wxAppProgressIndicator::SetValue(int value)
{
    wxAppProgressDockIcon* dockIcon = (wxAppProgressDockIcon*) m_dockIcon;
    [dockIcon setProgress:(double)value / (double)m_maxValue];
}

void wxAppProgressIndicator::SetRange(int range)
{
    m_maxValue = range;
}

void wxAppProgressIndicator::Pulse()
{
    wxAppProgressDockIcon* dockIcon = (wxAppProgressDockIcon*) m_dockIcon;
    [dockIcon setIndeterminate:true];
}

void wxAppProgressIndicator::Reset()
{
    wxAppProgressDockIcon* dockIcon = (wxAppProgressDockIcon*) m_dockIcon;
    [dockIcon reset];
}
