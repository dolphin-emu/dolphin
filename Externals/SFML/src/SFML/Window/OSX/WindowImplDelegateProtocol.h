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
#include <SFML/Config.hpp> // for sf::Uint8
#include <SFML/Window/WindowHandle.hpp>

#import <AppKit/AppKit.h>

namespace sf {
    namespace priv {
        class WindowImplCocoa;
    }
}

////////////////////////////////////////////////////////////
/// \brief Interface of the delegate of the window implementation
///
/// We don't create an interface here because Obj-C doesn't allow
/// multiple inheritance (SFViewController and SFWindowController
/// don't have the same parent classes). Unfortunately this means
/// we have to duplicate some code.
///
/// Everything is done via a class that implement this protocol.
/// There are two of these classes:
///
/// SFViewController and SFWindowController
///
/// The requester is a WindowImplCocoa. It's used to send back
/// event via these functions:
///
/// windowClosed, windowResized, windowLostFocus, windowGainedFocus
///
/// mouseDownAt, mouseUpAt, mouseMovedAt, mouseWheelScrolledAt,
/// mouseMovedIn, mouseMovedOut
///
/// keyDown, keyUp, textEntered
///
/// Note: Joysticks are not bound to a view or window
/// thus they're not managed by a class implementing this protocol.
///
////////////////////////////////////////////////////////////
@protocol WindowImplDelegateProtocol

////////////////////////////////////////////////////////////
/// \brief Get the display scale factor
///
/// \return e.g. 1.0 for classic display, 2.0 for retina display
///
////////////////////////////////////////////////////////////
-(CGFloat)displayScaleFactor;

////////////////////////////////////////////////////////////
/// \brief Set the WindowImpl who requested this delegate
///
////////////////////////////////////////////////////////////
-(void)setRequesterTo:(sf::priv::WindowImplCocoa*)requester;

////////////////////////////////////////////////////////////
/// \brief Get the underlying OS specific handle
///
/// \return Return the main view or window.
///
////////////////////////////////////////////////////////////
-(sf::WindowHandle)getSystemHandle;

////////////////////////////////////////////////////////////
/// \brief Determine where the mouse is
///
/// \return true when the mouse is inside the OpenGL view, false otherwise
///
////////////////////////////////////////////////////////////
-(BOOL)isMouseInside;

////////////////////////////////////////////////////////////
/// \brief Grab or release the mouse cursor
///
/// \param grabbed YES to grab, NO to release
///
////////////////////////////////////////////////////////////
-(void)setCursorGrabbed:(BOOL)grabbed;

////////////////////////////////////////////////////////////
/// \brief Get window position
///
/// \return Top left corner of the window or view
///
////////////////////////////////////////////////////////////
-(NSPoint)position;

////////////////////////////////////////////////////////////
/// \brief Move the window
///
/// Doesn't apply if the implementation is 'only' a view.
///
/// \param x x position in SFML coordinates
/// \param y y position in SFML coordinates
///
////////////////////////////////////////////////////////////
-(void)setWindowPositionToX:(int)x Y:(int)y;

////////////////////////////////////////////////////////////
/// \brief Get window/view's size
///
/// \return the size of the rendering area
///
////////////////////////////////////////////////////////////
-(NSSize)size;

////////////////////////////////////////////////////////////
/// \brief Resize the window/view
///
/// \param width new width
/// \param height new height
///
////////////////////////////////////////////////////////////
-(void)resizeTo:(unsigned int)width by:(unsigned int)height;

////////////////////////////////////////////////////////////
/// \brief Set the window's title
///
/// Doesn't apply if the implementation is 'only' a view.
///
/// \param title new title
///
////////////////////////////////////////////////////////////
-(void)changeTitle:(NSString*)title;

////////////////////////////////////////////////////////////
/// \brief Hide the window
///
/// Doesn't apply if the implementation is 'only' a view.
///
////////////////////////////////////////////////////////////
-(void)hideWindow;

////////////////////////////////////////////////////////////
/// \brief Show the window
///
/// Doesn't apply if the implementation is 'only' a view.
///
////////////////////////////////////////////////////////////
-(void)showWindow;

////////////////////////////////////////////////////////////
/// \brief Close the window
///
/// Doesn't apply if the implementation is 'only' a view.
///
////////////////////////////////////////////////////////////
-(void)closeWindow;

////////////////////////////////////////////////////////////
/// \brief Request the current window to be made the active
///        foreground window
///
////////////////////////////////////////////////////////////
-(void)requestFocus;

////////////////////////////////////////////////////////////
/// \brief Check whether the window has the input focus
///
/// \return True if window has focus, false otherwise
///
////////////////////////////////////////////////////////////
-(BOOL)hasFocus;

////////////////////////////////////////////////////////////
/// \brief Enable key repeat
///
////////////////////////////////////////////////////////////
-(void)enableKeyRepeat;

////////////////////////////////////////////////////////////
/// \brief Disable key repeat
///
////////////////////////////////////////////////////////////
-(void)disableKeyRepeat;

////////////////////////////////////////////////////////////
/// \brief Set an icon to the application
///
/// \param width icon's width
/// \param height icon's height
/// \param pixels icon's data
///
////////////////////////////////////////////////////////////
-(void)setIconTo:(unsigned int)width by:(unsigned int)height with:(const sf::Uint8*)pixels;

////////////////////////////////////////////////////////////
/// \brief Fetch new event
///
////////////////////////////////////////////////////////////
-(void)processEvent;

////////////////////////////////////////////////////////////
/// \brief Apply a given context to an OpenGL view
///
/// \param context OpenGL context to attach to the OpenGL view
///
////////////////////////////////////////////////////////////
-(void)applyContext:(NSOpenGLContext*)context;

@end
