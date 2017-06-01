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
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/VideoModeImpl.hpp>
#include <algorithm>
#include <functional>


namespace sf
{
////////////////////////////////////////////////////////////
VideoMode::VideoMode() :
width       (0),
height      (0),
bitsPerPixel(0)
{

}


////////////////////////////////////////////////////////////
VideoMode::VideoMode(unsigned int modeWidth, unsigned int modeHeight, unsigned int modeBitsPerPixel) :
width       (modeWidth),
height      (modeHeight),
bitsPerPixel(modeBitsPerPixel)
{

}


////////////////////////////////////////////////////////////
VideoMode VideoMode::getDesktopMode()
{
    // Directly forward to the OS-specific implementation
    return priv::VideoModeImpl::getDesktopMode();
}


////////////////////////////////////////////////////////////
const std::vector<VideoMode>& VideoMode::getFullscreenModes()
{
    static std::vector<VideoMode> modes;

    // Populate the array on first call
    if (modes.empty())
    {
        modes = priv::VideoModeImpl::getFullscreenModes();
        std::sort(modes.begin(), modes.end(), std::greater<VideoMode>());
    }

    return modes;
}


////////////////////////////////////////////////////////////
bool VideoMode::isValid() const
{
    const std::vector<VideoMode>& modes = getFullscreenModes();

    return std::find(modes.begin(), modes.end(), *this) != modes.end();
}


////////////////////////////////////////////////////////////
bool operator ==(const VideoMode& left, const VideoMode& right)
{
    return (left.width        == right.width)        &&
           (left.height       == right.height)       &&
           (left.bitsPerPixel == right.bitsPerPixel);
}


////////////////////////////////////////////////////////////
bool operator !=(const VideoMode& left, const VideoMode& right)
{
    return !(left == right);
}


////////////////////////////////////////////////////////////
bool operator <(const VideoMode& left, const VideoMode& right)
{
    if (left.bitsPerPixel == right.bitsPerPixel)
    {
        if (left.width == right.width)
        {
            return left.height < right.height;
        }
        else
        {
            return left.width < right.width;
        }
    }
    else
    {
        return left.bitsPerPixel < right.bitsPerPixel;
    }
}


////////////////////////////////////////////////////////////
bool operator >(const VideoMode& left, const VideoMode& right)
{
    return right < left;
}


////////////////////////////////////////////////////////////
bool operator <=(const VideoMode& left, const VideoMode& right)
{
    return !(right < left);
}


////////////////////////////////////////////////////////////
bool operator >=(const VideoMode& left, const VideoMode& right)
{
    return !(left < right);
}

} // namespace sf
