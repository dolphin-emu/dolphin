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
#include <SFML/System/Unix/ClockImpl.hpp>
#if defined(SFML_SYSTEM_MACOS) || defined(SFML_SYSTEM_IOS)
    #include <mach/mach_time.h>
#else
    #include <time.h>
#endif


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
Time ClockImpl::getCurrentTime()
{
#if defined(SFML_SYSTEM_MACOS) || defined(SFML_SYSTEM_IOS)

    // Mac OS X specific implementation (it doesn't support clock_gettime)
    static mach_timebase_info_data_t frequency = {0, 0};
    if (frequency.denom == 0)
        mach_timebase_info(&frequency);
    Uint64 nanoseconds = mach_absolute_time() * frequency.numer / frequency.denom;
    return sf::microseconds(nanoseconds / 1000);

#else

    // POSIX implementation
    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return sf::microseconds(static_cast<Uint64>(time.tv_sec) * 1000000 + time.tv_nsec / 1000);

#endif
}

} // namespace priv

} // namespace sf
