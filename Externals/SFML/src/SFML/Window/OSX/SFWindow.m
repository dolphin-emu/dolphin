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
#import <SFML/Window/OSX/SFWindow.h>


@implementation SFWindow

////////////////////////////////////////////////////////
-(BOOL)acceptsFirstResponder
{
    return YES;
}


////////////////////////////////////////////////////////
-(BOOL)canBecomeKeyWindow
{
    return YES;
}


////////////////////////////////////////////////////////
-(void)keyDown:(NSEvent*)theEvent
{
    // Do nothing except preventing a system alert each time a key is pressed
    //
    // Special Consideration :
    // -----------------------
    // Consider overriding NSResponder -keyDown: message in a Cocoa view/window
    // that contains a SFML rendering area. Doing so will prevent a system
    // alert to be thrown every time the user presses a key.
    (void)theEvent;
}


////////////////////////////////////////////////////////
-(void)performClose:(id)sender
{
    // From Apple documentation:
    //
    // > If the window's delegate or the window itself implements windowShouldClose:,
    // > that message is sent with the window as the argument. (Only one such message is sent;
    // > if both the delegate and the NSWindow object implement the method, only the delegate
    // > receives the message.) If the windowShouldClose: method returns NO, the window isn't
    // > closed. If it returns YES, or if it isn't implemented, performClose: invokes the
    // > close method to close the window.
    // >
    // > If the window doesn't have a close button or can't be closed (for example, if the
    // > delegate replies NO to a windowShouldClose: message), the system emits the alert sound.
    //
    // The last paragraph is problematic for SFML fullscreen window since they don't have
    // a close button (style is NSBorderlessWindowMask). So we reimplement this function.

    BOOL shouldClose = NO;

    if ([self delegate] && [[self delegate] respondsToSelector:@selector(windowShouldClose:)])
        shouldClose = [[self delegate] windowShouldClose:sender];
    // else if ([self respondsToSelector:@selector(windowShouldClose:)])
    //     shouldClose = [self windowShouldClose:sender];
    // error: no visible @interface for 'SFWindow' declares the selector 'windowShouldClose:'

    if (shouldClose)
        [self close];
}


////////////////////////////////////////////////////////
-(BOOL)validateMenuItem:(NSMenuItem*)menuItem
{
    return [menuItem action] == @selector(performClose:);
}


@end


@implementation NSWindow (SFML)

////////////////////////////////////////////////////////////
-(id)sfClose
{
    [self performClose:nil];
    return nil;
}

@end
