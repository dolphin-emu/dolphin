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
#import <SFML/Window/OSX/WindowImplDelegateProtocol.h>

#import <AppKit/AppKit.h>

namespace sf
{
namespace priv
{

////////////////////////////////////////////////////////////
/// \brief Get the scale factor of the main screen
///
////////////////////////////////////////////////////////////
inline CGFloat getDefaultScaleFactor()
{
    return [[NSScreen mainScreen] backingScaleFactor];
}

////////////////////////////////////////////////////////////
/// \brief Scale SFML coordinates to backing coordinates
///
/// \param in SFML coordinates to be converted
/// \param delegate an object implementing WindowImplDelegateProtocol, or nil for default scale
///
////////////////////////////////////////////////////////////
template <class T>
void scaleIn(T& in, id<WindowImplDelegateProtocol> delegate)
{
    in /= delegate ? [delegate displayScaleFactor] : getDefaultScaleFactor();
}

template <class T>
void scaleInWidthHeight(T& in, id<WindowImplDelegateProtocol> delegate)
{
    scaleIn(in.width, delegate);
    scaleIn(in.height, delegate);
}

template <class T>
void scaleInXY(T& in, id<WindowImplDelegateProtocol> delegate)
{
    scaleIn(in.x, delegate);
    scaleIn(in.y, delegate);
}

////////////////////////////////////////////////////////////
/// \brief Scale backing coordinates to SFML coordinates
///
/// \param out backing coordinates to be converted
/// \param delegate an object implementing WindowImplDelegateProtocol, or nil for default scale
///
////////////////////////////////////////////////////////////
template <class T>
void scaleOut(T& out, id<WindowImplDelegateProtocol> delegate)
{
    out *= delegate ? [delegate displayScaleFactor] : getDefaultScaleFactor();
}

template <class T>
void scaleOutWidthHeight(T& out, id<WindowImplDelegateProtocol> delegate)
{
    scaleOut(out.width, delegate);
    scaleOut(out.height, delegate);
}

template <class T>
void scaleOutXY(T& out, id<WindowImplDelegateProtocol> delegate)
{
    scaleOut(out.x, delegate);
    scaleOut(out.y, delegate);
}

} // namespace priv
} // namespace sf

