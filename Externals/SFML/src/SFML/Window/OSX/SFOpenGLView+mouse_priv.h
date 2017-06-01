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
#include <SFML/Window/Mouse.hpp>

#import <AppKit/AppKit.h>


////////////////////////////////////////////////////////////
/// Here are defined a few private messages for mouse
/// handling in SFOpenGLView.
///
////////////////////////////////////////////////////////////


@interface SFOpenGLView (mouse_priv)

////////////////////////////////////////////////////////////
/// \brief Update the mouse state (in or out)
///
/// Fire an event if its state has changed.
///
////////////////////////////////////////////////////////////
-(void)updateMouseState;

////////////////////////////////////////////////////////////
/// \brief handle mouse down event
///
////////////////////////////////////////////////////////////
-(void)handleMouseDown:(NSEvent*)theEvent;

////////////////////////////////////////////////////////////
/// \brief handle mouse up event
///
////////////////////////////////////////////////////////////
-(void)handleMouseUp:(NSEvent*)theEvent;

////////////////////////////////////////////////////////////
/// \brief handle mouse move event
///
////////////////////////////////////////////////////////////
-(void)handleMouseMove:(NSEvent*)theEvent;

////////////////////////////////////////////////////////////
/// \brief Check whether the cursor is grabbed or not
///
/// The cursor is grabbed if the window is active (key) and
/// either it is in fullscreen mode or the user wants to
/// grab it.
///
////////////////////////////////////////////////////////////
-(BOOL)isCursorCurrentlyGrabbed;

////////////////////////////////////////////////////////////
/// \brief (Dis)connect the cursor's movements from/to the system
///        and project the cursor into the view
///
////////////////////////////////////////////////////////////
-(void)updateCursorGrabbed;

////////////////////////////////////////////////////////////
/// \brief Move the cursor to the given location
///
/// \param loc location expressed in SFML coordinate system
///
////////////////////////////////////////////////////////////
-(void)moveCursorTo:(NSPoint)loc;

////////////////////////////////////////////////////////////
/// \brief Get the display identifier on which the view is
///
////////////////////////////////////////////////////////////
-(CGDirectDisplayID)displayId;

////////////////////////////////////////////////////////////
/// \brief Convert the NSEvent mouse button type to SFML type
///
/// \param event a mouse button event
///
/// \return Left, Right, ..., or ButtonCount if the button is unknown
///
////////////////////////////////////////////////////////////
+(sf::Mouse::Button)mouseButtonFromEvent:(NSEvent*)event;

@end
