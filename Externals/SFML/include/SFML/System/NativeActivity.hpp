////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Laurent Gomila (laurent@sfml-dev.org)
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

#ifndef SFML_NATIVEACTIVITY_HPP
#define SFML_NATIVEACTIVITY_HPP


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/System/Export.hpp>


#if !defined(SFML_SYSTEM_ANDROID)
#error NativeActivity.hpp: This header is Android only.
#endif


struct ANativeActivity;

namespace sf
{
////////////////////////////////////////////////////////////
/// \ingroup system
/// \brief Return a pointer to the Android native activity
///
/// You shouldn't have to use this function, unless you want
/// to implement very specific details, that SFML doesn't
/// support, or to use a workaround for a known issue.
///
/// \return Pointer to Android native activity structure
///
/// \sfplatform{Android,SFML/System/NativeActivity.hpp}
///
////////////////////////////////////////////////////////////
SFML_SYSTEM_API ANativeActivity* getNativeActivity();

} // namespace sf


#endif // SFML_NATIVEACTIVITY_HPP
