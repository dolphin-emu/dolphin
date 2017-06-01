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
#import <AppKit/AppKit.h>

////////////////////////////////////////////////////////////
/// \brief Here we redefine some methods to allow grabbing fullscreen events
///
////////////////////////////////////////////////////////////
@interface SFWindow : NSWindow

////////////////////////////////////////////////////////////
/// \brief Allow to grab fullscreen events
///
/// acceptsFirstResponder and canBecomeKeyWindow messages must
/// return YES to grab fullscreen events.
///
/// See http://stackoverflow.com/questions/999464/fullscreen-key-down-actions
///
/// \return YES
///
////////////////////////////////////////////////////////////
-(BOOL)acceptsFirstResponder;

////////////////////////////////////////////////////////////
/// \brief Allow to grab fullscreen events
///
/// See acceptsFirstResponder documentation above.
///
/// \return YES
///
////////////////////////////////////////////////////////////
-(BOOL)canBecomeKeyWindow;

////////////////////////////////////////////////////////////
/// \brief Prevent system alert
///
/// \param theEvent a Cocoa event
///
////////////////////////////////////////////////////////////
-(void)keyDown:(NSEvent*)theEvent;

////////////////////////////////////////////////////////////
/// \brief This action method simulates the user clicking the close button
///
/// Override NSWindow implementation, see implementation for details
///
/// \param sender The message's sender
///
////////////////////////////////////////////////////////////
-(void)performClose:(id)sender;

////////////////////////////////////////////////////////////
/// \brief Enabling or disabling a specific menu item
///
/// \param menuItem An NSMenuItem object that represents the menu item
///
/// \return YES to enable menuItem, NO to disable it.
///
////////////////////////////////////////////////////////////
-(BOOL)validateMenuItem:(NSMenuItem*)menuItem;

@end


////////////////////////////////////////////////////////////
/// \brief Extension of NSWindow
///
/// Add some extra messages for SFML internal usage.
///
////////////////////////////////////////////////////////////
@interface NSWindow (SFML)

////////////////////////////////////////////////////////////
/// Proxy for performClose: for the app delegate
///
/// \return nil
///
////////////////////////////////////////////////////////////
-(id)sfClose;

@end
