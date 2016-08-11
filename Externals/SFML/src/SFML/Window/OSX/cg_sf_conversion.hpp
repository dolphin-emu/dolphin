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

#ifndef SFML_CG_SF_CONVERSION_HPP
#define SFML_CG_SF_CONVERSION_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/VideoMode.hpp>
#include <ApplicationServices/ApplicationServices.h>

namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Get bpp of a video mode for OS 10.6 or later
///
/// With OS 10.6 and later, Quartz doesn't use dictionaries any more
/// to represent video mode. Instead it uses a CGDisplayMode opaque type.
///
////////////////////////////////////////////////////////////
size_t modeBitsPerPixel(CGDisplayModeRef mode);

////////////////////////////////////////////////////////////
/// \brief Get bpp for all OS X version
///
/// This function use only non-deprecated way to get the
/// display bits per pixel information for a given display id.
///
////////////////////////////////////////////////////////////
size_t displayBitsPerPixel(CGDirectDisplayID displayId);

////////////////////////////////////////////////////////////
/// \brief Convert a Quartz video mode into a sf::VideoMode object
///
////////////////////////////////////////////////////////////
VideoMode convertCGModeToSFMode(CGDisplayModeRef cgmode);

////////////////////////////////////////////////////////////
/// \brief Convert a sf::VideoMode object into a Quartz video mode
///
////////////////////////////////////////////////////////////
CGDisplayModeRef convertSFModeToCGMode(VideoMode sfmode);

} // namespace priv
} // namespace sf

#endif
