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
/// Here are defined a few private messages for keyboard
/// handling in SFOpenGLView.
///
////////////////////////////////////////////////////////////


@interface SFOpenGLView (keyboard_priv)

////////////////////////////////////////////////////////////
/// \brief Convert a key down/up NSEvent into an SFML key event
///
/// The conversion is based on localizedKeys and nonLocalizedKeys functions.
///
/// \param event a key event
///
/// \return sf::Keyboard::Unknown as Code if the key is unknown
///
////////////////////////////////////////////////////////////
+(sf::Event::KeyEvent)convertNSKeyEventToSFMLEvent:(NSEvent*)event;

////////////////////////////////////////////////////////////
/// \brief Check if the event represent some Unicode text
///
/// The event is assumed to be a key down event.
/// False is returned if the event is either escape or a non text Unicode.
///
/// \param event a key down event
///
/// \return true if event represents a Unicode character, false otherwise
///
////////////////////////////////////////////////////////////
+(BOOL)isValidTextUnicode:(NSEvent*)event;

@end
