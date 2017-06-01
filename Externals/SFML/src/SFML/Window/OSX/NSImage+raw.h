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
#include <SFML/Config.hpp>

#import <AppKit/AppKit.h>

////////////////////////////////////////////////////////////
/// Extends NSImage with a convenience method to load images
/// from raw data.
///
////////////////////////////////////////////////////////////

@interface NSImage (raw)

////////////////////////////////////////////////////////////
/// \brief Load an image from raw RGBA pixels
///
/// \param pixels array of 4 * `size` bytes representing the image
/// \param size size of the image
///
/// \return an instance of NSImage that needs to be released by the caller
///
////////////////////////////////////////////////////////////
+(NSImage*)imageWithRawData:(const sf::Uint8*)pixels andSize:(NSSize)size;

@end
