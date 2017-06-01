////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Marco Antognini (antognini.marco@gmail.com),
//                         Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/WindowHandle.hpp>
#include <SFML/Window/WindowStyle.hpp>
#include <SFML/Window/OSX/WindowImplCocoa.hpp>
#include <SFML/System/Err.hpp>
#include <ApplicationServices/ApplicationServices.h>
#include <algorithm>

#import <SFML/Window/OSX/NSImage+raw.h>
#import <SFML/Window/OSX/Scaling.h>
#import <SFML/Window/OSX/SFApplication.h>
#import <SFML/Window/OSX/SFOpenGLView.h>
#import <SFML/Window/OSX/SFWindow.h>
#import <SFML/Window/OSX/SFWindowController.h>
#import <OpenGL/OpenGL.h>

////////////////////////////////////////////////////////////
/// SFBlackView is a simple view filled with black, nothing more
///
////////////////////////////////////////////////////////////
@interface SFBlackView : NSView
@end

@implementation SFBlackView

////////////////////////////////////////////////////////////
-(void)drawRect:(NSRect)dirtyRect
{
    [[NSColor blackColor] setFill];
    NSRectFill(dirtyRect);
}

@end

////////////////////////////////////////////////////////////
/// SFWindowController class: private interface
///
////////////////////////////////////////////////////////////
@interface SFWindowController ()

////////////////////////////////////////////////////////////
/// \brief Retrieves the screen height
///
/// \return screen height
///
////////////////////////////////////////////////////////////
-(float)screenHeight;

////////////////////////////////////////////////////////////
/// \brief Retrieves the title bar height
///
/// \return title bar height
///
////////////////////////////////////////////////////////////
-(float)titlebarHeight;

@end

@implementation SFWindowController

#pragma mark
#pragma mark SFWindowController's methods

////////////////////////////////////////////////////////
-(id)initWithWindow:(NSWindow*)window
{
    if ((self = [super init]))
    {
        m_window = nil;
        m_oglView = nil;
        m_requester = 0;
        m_fullscreen = NO; // assuming this is the case... too hard to handle anyway.
        m_restoreResize = NO;

        // Retain the window for our own use.
        m_window = [window retain];

        if (m_window == nil)
        {
            sf::err() << "No window was given to -[SFWindowController initWithWindow:]." << std::endl;
            return self;
        }

        // Create the view.
        m_oglView = [[SFOpenGLView alloc] initWithFrame:[[m_window contentView] frame]
                                             fullscreen:NO];

        if (m_oglView == nil)
        {
            sf::err() << "Could not create an instance of NSOpenGLView "
                      << "in -[SFWindowController initWithWindow:]."
                      << std::endl;
            return self;
        }

        // Set the view to the window as its content view.
        [m_window setContentView:m_oglView];
    }

    return self;
}


////////////////////////////////////////////////////////
-(id)initWithMode:(const sf::VideoMode&)mode andStyle:(unsigned long)style
{
    // If we are not on the main thread we stop here and advice the user.
    if ([NSThread currentThread] != [NSThread mainThread])
    {
        /*
         * See http://lists.apple.com/archives/cocoa-dev/2011/Feb/msg00460.html
         * for more information.
         */
        sf::err() << "Cannot create a window from a worker thread. (OS X limitation)" << std::endl;

        return nil;
    }

    if ((self = [super init]))
    {
        m_window = nil;
        m_oglView = nil;
        m_requester = 0;
        m_fullscreen = (style & sf::Style::Fullscreen);
        m_restoreResize = NO;

        if (m_fullscreen)
            [self setupFullscreenViewWithMode:mode];
        else
            [self setupWindowWithMode:mode andStyle:style];

        [m_oglView finishInit];
    }
    return self;
}


////////////////////////////////////////////////////////
-(void)setupFullscreenViewWithMode:(const sf::VideoMode&)mode
{
    // Create a screen-sized window on the main display
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    sf::priv::scaleInWidthHeight(desktop, nil);
    NSRect windowRect = NSMakeRect(0, 0, desktop.width, desktop.height);
    m_window = [[SFWindow alloc] initWithContentRect:windowRect
                                           styleMask:NSBorderlessWindowMask
                                             backing:NSBackingStoreBuffered
                                               defer:NO];

    if (m_window == nil)
    {
        sf::err() << "Could not create an instance of NSWindow "
                  << "in -[SFWindowController setupFullscreenViewWithMode:]."
                  << std::endl;
        return;
    }

    // Set the window level to be above the menu bar
    [m_window setLevel:NSMainMenuWindowLevel+1];

    // More window configuration...
    [m_window setOpaque:YES];
    [m_window setHidesOnDeactivate:YES];
    [m_window setAutodisplay:YES];
    [m_window setReleasedWhenClosed:NO]; // We own the class, not AppKit

    // Register for event
    [m_window setDelegate:self];
    [m_window setAcceptsMouseMovedEvents:YES];
    [m_window setIgnoresMouseEvents:NO];

    // Create a master view containing our OpenGL view
    NSView* masterView = [[[SFBlackView alloc] initWithFrame:windowRect] autorelease];

    if (masterView == nil)
    {
        sf::err() << "Could not create an instance of SFBlackView "
                  << "in -[SFWindowController setupFullscreenViewWithMode:]."
                  << std::endl;
        return;
    }

    // Create our OpenGL view size and the view
    CGFloat width = std::min(mode.width, desktop.width);
    CGFloat height = std::min(mode.height, desktop.height);
    CGFloat x = (desktop.width - width) / 2.0;
    CGFloat y = (desktop.height - height) / 2.0;
    NSRect oglRect = NSMakeRect(x, y, width, height);

    m_oglView = [[SFOpenGLView alloc] initWithFrame:oglRect
                                         fullscreen:YES];

    if (m_oglView == nil)
    {
        sf::err() << "Could not create an instance of NSOpenGLView "
                  << "in -[SFWindowController setupFullscreenViewWithMode:]."
                  << std::endl;
        return;
    }

    // Populate the window and views
    [masterView addSubview:m_oglView];
    [m_window setContentView:masterView];
}


////////////////////////////////////////////////////////
-(void)setupWindowWithMode:(const sf::VideoMode&)mode andStyle:(unsigned long)style
{
    // We know that style & sf::Style::Fullscreen is false.

    // Create our window size.
    NSRect rect = NSMakeRect(0, 0, mode.width, mode.height);

    // Convert the SFML window style to Cocoa window style.
    unsigned int nsStyle = NSBorderlessWindowMask;
    if (style & sf::Style::Titlebar)
        nsStyle |= NSTitledWindowMask | NSMiniaturizableWindowMask;
    if (style & sf::Style::Resize)
        nsStyle |= NSResizableWindowMask;
    if (style & sf::Style::Close)
        nsStyle |= NSClosableWindowMask;

    // Create the window.
    m_window = [[SFWindow alloc] initWithContentRect:rect
                                           styleMask:nsStyle
                                             backing:NSBackingStoreBuffered
                                               defer:NO]; // Don't defer it!
    /*
     "YES" produces some "invalid drawable".
     See http://www.cocoabuilder.com/archive/cocoa/152482-nsviews-and-nsopenglcontext-invalid-drawable-error.html

     [...]
     As best as I can figure, this is happening because the NSWindow (and
     hence my view) are not visible on screen yet, and the system doesn't like that.
     [...]
     */

    if (m_window == nil)
    {
        sf::err() << "Could not create an instance of NSWindow "
                  << "in -[SFWindowController setupWindowWithMode:andStyle:]."
                  << std::endl;

        return;
    }

    // Create the view.
    m_oglView = [[SFOpenGLView alloc] initWithFrame:[[m_window contentView] frame]
                                         fullscreen:NO];

    if (m_oglView == nil)
    {
        sf::err() << "Could not create an instance of NSOpenGLView "
                  << "in -[SFWindowController setupWindowWithMode:andStyle:]."
                  << std::endl;

        return;
    }

    // Set the view to the window as its content view.
    [m_window setContentView:m_oglView];

    // Register for event.
    [m_window setDelegate:self];
    [m_window setAcceptsMouseMovedEvents:YES];
    [m_window setIgnoresMouseEvents:NO];

    // And some other things...
    [m_window center];
    [m_window setAutodisplay:YES];
    [m_window setReleasedWhenClosed:NO]; // We own the class, not AppKit
}


////////////////////////////////////////////////////////
-(void)dealloc
{
    [self closeWindow];
    [NSMenu setMenuBarVisible:YES];

    [m_window release];
    [m_oglView release];

    [super dealloc];
}


#pragma mark
#pragma mark WindowImplDelegateProtocol's methods


////////////////////////////////////////////////////////
-(CGFloat)displayScaleFactor
{
    return [m_oglView displayScaleFactor];
}


////////////////////////////////////////////////////////
-(void)setRequesterTo:(sf::priv::WindowImplCocoa*)requester
{
    // Forward to the view.
    [m_oglView setRequesterTo:requester];
    m_requester = requester;
}


////////////////////////////////////////////////////////
-(sf::WindowHandle)getSystemHandle
{
    return m_window;
}


////////////////////////////////////////////////////////
-(BOOL)isMouseInside
{
    return [m_oglView isMouseInside];
}


////////////////////////////////////////////////////////
-(void)setCursorGrabbed:(BOOL)grabbed
{
    // Remove or restore resizeable style if needed
    BOOL resizeable = [m_window styleMask] & NSResizableWindowMask;
    if (grabbed && resizeable)
    {
        m_restoreResize = YES;
        NSUInteger newStyle = [m_window styleMask] & ~NSResizableWindowMask;
        [m_window setStyleMask:newStyle];
    }
    else if (!grabbed && m_restoreResize)
    {
        m_restoreResize = NO;
        NSUInteger newStyle = [m_window styleMask] | NSResizableWindowMask;
        [m_window setStyleMask:newStyle];
    }

    // Forward to our view
    [m_oglView setCursorGrabbed:grabbed];
}


////////////////////////////////////////////////////////////
-(NSPoint)position
{
    // Note: since 10.7 the conversion API works with NSRect
    // instead of NSPoint. Therefore we use a NSRect but ignore
    // its width and height.

    // Position of the bottom-left corner in the different coordinate systems:
    NSRect corner = [m_oglView frame]; // bottom left; size is ignored
    NSRect view   = [m_oglView convertRectToBacking:corner];
    NSRect window = [m_oglView convertRect:view toView:nil];
    NSRect screen = [[m_oglView window] convertRectToScreen:window];

    // Get the top-left corner in screen coordinates
    CGFloat x = screen.origin.x;
    CGFloat y = screen.origin.y + [m_oglView frame].size.height;

    // Flip y-axis (titlebar was already taken into account above)
    y = [self screenHeight] - y;

    return NSMakePoint(x, y);
}


////////////////////////////////////////////////////////
-(void)setWindowPositionToX:(int)x Y:(int)y
{
    NSPoint point = NSMakePoint(x, y);

    // Flip for SFML window coordinate system and take titlebar into account
    point.y = [self screenHeight] - point.y + [self titlebarHeight];

    // Place the window.
    [m_window setFrameTopLeftPoint:point];

    // In case the cursor was grabbed we need to update its position
    [m_oglView updateCursorGrabbed];
}


////////////////////////////////////////////////////////
-(NSSize)size
{
    return [m_oglView frame].size;
}


////////////////////////////////////////////////////////
-(void)resizeTo:(unsigned int)width by:(unsigned int)height
{
    if (m_fullscreen)
    {
        // Special case when fullscreen: only resize the opengl view
        // and make sure the requested size is not bigger than the window.
        sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
        sf::priv::scaleInWidthHeight(desktop, nil);

        width = std::min(width, desktop.width);
        height = std::min(height, desktop.height);

        CGFloat x = (desktop.width - width) / 2.0;
        CGFloat y = (desktop.height - height) / 2.0;
        NSRect oglRect = NSMakeRect(x, y, width, height);

        [m_oglView setFrame:oglRect];
        [m_oglView setNeedsDisplay:YES];
    }
    else
    {
        // Before resizing, remove resizable mask to be able to resize
        // beyond the desktop boundaries.
        NSUInteger styleMask = [m_window styleMask];

        [m_window setStyleMask:styleMask ^ NSResizableWindowMask];

        // Add titlebar height.
        height += [self titlebarHeight];

        // Corner case: don't set the window height bigger than the screen height
        // or the view will be resized _later_ without generating a resize event.
        NSRect screenFrame = [[NSScreen mainScreen] visibleFrame];
        CGFloat maxVisibleHeight = screenFrame.size.height;
        if (height > maxVisibleHeight)
        {
            height = maxVisibleHeight;

            // The size is not the requested one, we fire an event
            if (m_requester != 0)
                m_requester->windowResized(width, height - [self titlebarHeight]);
        }

        NSRect frame = NSMakeRect([m_window frame].origin.x,
                                  [m_window frame].origin.y,
                                  width,
                                  height);

        [m_window setFrame:frame display:YES];

        // And restore the mask
        [m_window setStyleMask:styleMask];
    }
}


////////////////////////////////////////////////////////
-(void)changeTitle:(NSString*)title
{
    [m_window setTitle:title];
}


////////////////////////////////////////////////////////
-(void)hideWindow
{
    [m_window orderOut:nil];
}


////////////////////////////////////////////////////////
-(void)showWindow
{
    [m_window makeKeyAndOrderFront:nil];
}


////////////////////////////////////////////////////////
-(void)closeWindow
{
    [self applyContext:nil];
    [m_window close];
    [m_window setDelegate:nil];
    [self setRequesterTo:0];
}


////////////////////////////////////////////////////////
-(void)requestFocus
{
    [m_window makeKeyAndOrderFront:nil];

    // In case the app is not active, make its dock icon bounce for one sec
    [NSApp requestUserAttention:NSInformationalRequest];
}


////////////////////////////////////////////////////////////
-(BOOL)hasFocus
{
    return [NSApp keyWindow] == m_window;
}


////////////////////////////////////////////////////////
-(void)enableKeyRepeat
{
    [m_oglView enableKeyRepeat];
}


////////////////////////////////////////////////////////
-(void)disableKeyRepeat
{
    [m_oglView disableKeyRepeat];
}


////////////////////////////////////////////////////////
-(void)setIconTo:(unsigned int)width
              by:(unsigned int)height
            with:(const sf::Uint8*)pixels
{
    // Load image and set app icon.
    NSImage* icon = [NSImage imageWithRawData:pixels
                                      andSize:NSMakeSize(width, height)];

    [[SFApplication sharedApplication] setApplicationIconImage:icon];

    [icon release];
}


////////////////////////////////////////////////////////
-(void)processEvent
{
    // If we are not on the main thread we stop here and advice the user.
    if ([NSThread currentThread] != [NSThread mainThread])
    {
        /*
         * See http://lists.apple.com/archives/cocoa-dev/2011/Feb/msg00460.html
         * for more information.
         */
        sf::err() << "Cannot fetch event from a worker thread. (OS X restriction)" << std::endl;

        return;
    }

    // If we don't have a requester we don't fetch event.
    if (m_requester != 0)
        [SFApplication processEvent];
}


////////////////////////////////////////////////////////
-(void)applyContext:(NSOpenGLContext*)context
{
    [m_oglView setOpenGLContext:context];
    [context setView:m_oglView];
}


#pragma mark
#pragma mark NSWindowDelegate's methods


////////////////////////////////////////////////////////
-(BOOL)windowShouldClose:(id)sender
{
    (void)sender;

    if (m_requester == 0)
        return YES;

    m_requester->windowClosed();
    return NO;
}


#pragma mark
#pragma mark Other methods

////////////////////////////////////////////////////////
-(float)screenHeight
{
    NSDictionary* deviceDescription = [[m_window screen] deviceDescription];
    NSNumber* screenNumber = [deviceDescription valueForKey:@"NSScreenNumber"];
    CGDirectDisplayID screenID = (CGDirectDisplayID)[screenNumber intValue];
    CGFloat height = CGDisplayPixelsHigh(screenID);
    return height;
}


////////////////////////////////////////////////////////
-(float)titlebarHeight
{
    return NSHeight([m_window frame]) - NSHeight([[m_window contentView] frame]);
}

@end

