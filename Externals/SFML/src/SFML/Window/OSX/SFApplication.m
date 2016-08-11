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
#import <SFML/Window/OSX/SFApplication.h>


////////////////////////////////////////////////////////////
@implementation SFApplication


////////////////////////////////////////////////////////////
+(void)processEvent
{
    [SFApplication sharedApplication]; // Make sure NSApp exists
    NSEvent* event = nil;

    while ((event = [NSApp nextEventMatchingMask:NSAnyEventMask
                                       untilDate:[NSDate distantPast]
                                          inMode:NSDefaultRunLoopMode
                                         dequeue:YES])) // Remove the event from the queue
    {
        [NSApp sendEvent:event];
    }
}


////////////////////////////////////////////////////////
+(void)setUpMenuBar
{
    [SFApplication sharedApplication]; // Make sure NSApp exists

    // Set the main menu bar
    NSMenu* mainMenu = [NSApp mainMenu];
    if (mainMenu != nil)
        return;
    mainMenu = [[[NSMenu alloc] initWithTitle:@""] autorelease];
    [NSApp setMainMenu:mainMenu];

    // Application Menu (aka Apple Menu)
    NSMenuItem* appleItem = [mainMenu addItemWithTitle:@"" action:nil keyEquivalent:@""];
    NSMenu* appleMenu = [[SFApplication newAppleMenu] autorelease];
    [appleItem setSubmenu:appleMenu];

    // File Menu
    NSMenuItem* fileItem = [mainMenu addItemWithTitle:@"" action:nil keyEquivalent:@""];
    NSMenu* fileMenu = [[SFApplication newFileMenu] autorelease];
    [fileItem setSubmenu:fileMenu];

    // Window menu
    NSMenuItem* windowItem = [mainMenu addItemWithTitle:@"" action:nil keyEquivalent:@""];
    NSMenu* windowMenu = [[SFApplication newWindowMenu] autorelease];
    [windowItem setSubmenu:windowMenu];
    [NSApp setWindowsMenu:windowMenu];
}


////////////////////////////////////////////////////////
+(NSMenu*)newAppleMenu
{
    // Apple menu is as follow:
    //
    // AppName >
    //    About AppName
    //    --------------------
    //    Preferences...        [greyed]
    //    --------------------
    //    Services >
    //        / default empty menu /
    //    --------------------
    //    Hide AppName      Command+H
    //    Hide Others       Option+Command+H
    //    Show All
    //    --------------------
    //    Quit AppName      Command+Q

    NSString* appName = [SFApplication applicationName];

    // APPLE MENU
    NSMenu* appleMenu = [[NSMenu alloc] initWithTitle:@""];

    // ABOUT
    [appleMenu addItemWithTitle:[@"About " stringByAppendingString:appName]
                         action:@selector(orderFrontStandardAboutPanel:)
                  keyEquivalent:@""];

    // SEPARATOR
    [appleMenu addItem:[NSMenuItem separatorItem]];

    // PREFERENCES
    [appleMenu addItemWithTitle:@"Preferences..."
                         action:nil
                  keyEquivalent:@""];

    // SEPARATOR
    [appleMenu addItem:[NSMenuItem separatorItem]];

    // SERVICES
    NSMenu* serviceMenu = [[[NSMenu alloc] initWithTitle:@""] autorelease];
    NSMenuItem* serviceItem = [appleMenu addItemWithTitle:@"Services"
                                                  action:nil
                                           keyEquivalent:@""];
    [serviceItem setSubmenu:serviceMenu];
    [NSApp setServicesMenu:serviceMenu];

    // SEPARATOR
    [appleMenu addItem:[NSMenuItem separatorItem]];

    // HIDE
    [appleMenu addItemWithTitle:[@"Hide " stringByAppendingString:appName]
                         action:@selector(hide:)
                  keyEquivalent:@"h"];

    // HIDE OTHER
    NSMenuItem* hideOtherItem = [appleMenu addItemWithTitle:@"Hide Others"
                                                     action:@selector(hideOtherApplications:)
                                              keyEquivalent:@"h"];
    [hideOtherItem setKeyEquivalentModifierMask:(NSAlternateKeyMask | NSCommandKeyMask)];

    // SHOW ALL
    [appleMenu addItemWithTitle:@"Show All"
                         action:@selector(unhideAllApplications:)
                  keyEquivalent:@""];

    // SEPARATOR
    [appleMenu addItem:[NSMenuItem separatorItem]];

    // QUIT
    [appleMenu addItemWithTitle:[@"Quit " stringByAppendingString:appName]
                         action:@selector(terminate:)
                  keyEquivalent:@"q"];

    return appleMenu;
}


////////////////////////////////////////////////////////
+(NSMenu*)newFileMenu
{
    // The File menu is as follow:
    //
    // File >
    //    Close             Command+W

    // FILE MENU
    NSMenu* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];

    // CLOSE WINDOW
    NSMenuItem* closeItem = [[NSMenuItem alloc] initWithTitle:@"Close Window"
                                                       action:@selector(performClose:)
                                                keyEquivalent:@"w"];
    [fileMenu addItem:closeItem];
    [closeItem release];

    return fileMenu;
}


////////////////////////////////////////////////////////
+(NSMenu*)newWindowMenu
{
    // The Window menu is as follow:
    //
    // Window >
    //    Minimize          Command+M
    //    Zoom
    //    --------------------
    //    Bring All to Front

    // WINDOW MENU
    NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];

    // MINIMIZE
    NSMenuItem* minimizeItem = [[NSMenuItem alloc] initWithTitle:@"Minimize"
                                                          action:@selector(performMiniaturize:)
                                                   keyEquivalent:@"m"];
    [windowMenu addItem:minimizeItem];
    [minimizeItem release];

    // ZOOM
    [windowMenu addItemWithTitle:@"Zoom"
                          action:@selector(performZoom:)
                   keyEquivalent:@""];

    // SEPARATOR
    [windowMenu addItem:[NSMenuItem separatorItem]];

    // BRING ALL TO FRONT
    [windowMenu addItemWithTitle:@"Bring All to Front"
                          action:@selector(bringAllToFront:)
                   keyEquivalent:@""];

    return windowMenu;
}


////////////////////////////////////////////////////////
+(NSString*)applicationName
{
    // First, try localized name
    NSString* appName = [[[NSBundle mainBundle] localizedInfoDictionary] objectForKey:@"CFBundleDisplayName"];

    // Then, try non-localized name
    if ((appName == nil) || ([appName length] == 0))
        appName = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"];

    // Finally, fallback to the process info
    if ((appName == nil) || ([appName length] == 0))
        appName = [[NSProcessInfo processInfo] processName];

    return appName;
}


////////////////////////////////////////////////////////
-(void)bringAllToFront:(id)sender
{
    (void)sender;
    [[NSApp windows] makeObjectsPerformSelector:@selector(orderFrontRegardless)];
}


////////////////////////////////////////////////////////
-(void)sendEvent:(NSEvent *)anEvent
{
    // Fullscreen windows have a strange behaviour with key up. To make
    // sure the user gets an event we call (if possible) sfKeyUp on our
    // custom OpenGL view. See -[SFOpenGLView sfKeyUp:] for more details.

    id firstResponder = [[anEvent window] firstResponder];
    if (([anEvent type] != NSKeyUp) || (![firstResponder tryToPerform:@selector(sfKeyUp:) with:anEvent]))
    {
        // It's either not a key up event or no responder has a sfKeyUp
        // message implemented.
        [super sendEvent:anEvent];
    }
}


@end


