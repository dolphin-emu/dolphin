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

#ifndef SFML_SLEEP_HPP
#define SFML_SLEEP_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/System/Export.hpp>
#include <SFML/System/Time.hpp>


namespace sf
{
////////////////////////////////////////////////////////////
/// \ingroup system
/// \brief Make the current thread sleep for a given duration
///
/// sf::sleep is the best way to block a program or one of its
/// threads, as it doesn't consume any CPU power.
///
/// \param duration Time to sleep
///
////////////////////////////////////////////////////////////
void SFML_SYSTEM_API sleep(Time duration);

} // namespace sf


#endif // SFML_SLEEP_HPP
