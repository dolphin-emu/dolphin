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

#ifndef SFML_CLOCK_HPP
#define SFML_CLOCK_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/System/Export.hpp>
#include <SFML/System/Time.hpp>


namespace sf
{
////////////////////////////////////////////////////////////
/// \brief Utility class that measures the elapsed time
///
////////////////////////////////////////////////////////////
class SFML_SYSTEM_API Clock
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// The clock starts automatically after being constructed.
    ///
    ////////////////////////////////////////////////////////////
    Clock();

    ////////////////////////////////////////////////////////////
    /// \brief Get the elapsed time
    ///
    /// This function returns the time elapsed since the last call
    /// to restart() (or the construction of the instance if restart()
    /// has not been called).
    ///
    /// \return Time elapsed
    ///
    ////////////////////////////////////////////////////////////
    Time getElapsedTime() const;

    ////////////////////////////////////////////////////////////
    /// \brief Restart the clock
    ///
    /// This function puts the time counter back to zero.
    /// It also returns the time elapsed since the clock was started.
    ///
    /// \return Time elapsed
    ///
    ////////////////////////////////////////////////////////////
    Time restart();

private:

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    Time m_startTime; ///< Time of last reset, in microseconds
};

} // namespace sf


#endif // SFML_CLOCK_HPP


////////////////////////////////////////////////////////////
/// \class sf::Clock
/// \ingroup system
///
/// sf::Clock is a lightweight class for measuring time.
///
/// Its provides the most precise time that the underlying
/// OS can achieve (generally microseconds or nanoseconds).
/// It also ensures monotonicity, which means that the returned
/// time can never go backward, even if the system time is
/// changed.
///
/// Usage example:
/// \code
/// sf::Clock clock;
/// ...
/// Time time1 = clock.getElapsedTime();
/// ...
/// Time time2 = clock.restart();
/// \endcode
///
/// The sf::Time value returned by the clock can then be
/// converted to a number of seconds, milliseconds or even
/// microseconds.
///
/// \see sf::Time
///
////////////////////////////////////////////////////////////
