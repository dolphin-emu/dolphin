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

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/VideoModeImpl.hpp>
#include <windows.h>
#include <algorithm>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
std::vector<VideoMode> VideoModeImpl::getFullscreenModes()
{
    std::vector<VideoMode> modes;

    // Enumerate all available video modes for the primary display adapter
    DEVMODE win32Mode;
    win32Mode.dmSize = sizeof(win32Mode);
    for (int count = 0; EnumDisplaySettings(NULL, count, &win32Mode); ++count)
    {
        // Convert to sf::VideoMode
        VideoMode mode(win32Mode.dmPelsWidth, win32Mode.dmPelsHeight, win32Mode.dmBitsPerPel);

        // Add it only if it is not already in the array
        if (std::find(modes.begin(), modes.end(), mode) == modes.end())
            modes.push_back(mode);
    }

    return modes;
}


////////////////////////////////////////////////////////////
VideoMode VideoModeImpl::getDesktopMode()
{
    DEVMODE win32Mode;
    win32Mode.dmSize = sizeof(win32Mode);
    EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &win32Mode);

    return VideoMode(win32Mode.dmPelsWidth, win32Mode.dmPelsHeight, win32Mode.dmBitsPerPel);
}

} // namespace priv

} // namespace sf
