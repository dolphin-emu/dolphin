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

#ifndef SFML_LOCK_HPP
#define SFML_LOCK_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/System/Export.hpp>
#include <SFML/System/NonCopyable.hpp>


namespace sf
{
class Mutex;

////////////////////////////////////////////////////////////
/// \brief Automatic wrapper for locking and unlocking mutexes
///
////////////////////////////////////////////////////////////
class SFML_SYSTEM_API Lock : NonCopyable
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Construct the lock with a target mutex
    ///
    /// The mutex passed to sf::Lock is automatically locked.
    ///
    /// \param mutex Mutex to lock
    ///
    ////////////////////////////////////////////////////////////
    explicit Lock(Mutex& mutex);

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    /// The destructor of sf::Lock automatically unlocks its mutex.
    ///
    ////////////////////////////////////////////////////////////
    ~Lock();

private:

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    Mutex& m_mutex; ///< Mutex to lock / unlock
};

} // namespace sf


#endif // SFML_LOCK_HPP


////////////////////////////////////////////////////////////
/// \class sf::Lock
/// \ingroup system
///
/// sf::Lock is a RAII wrapper for sf::Mutex. By unlocking
/// it in its destructor, it ensures that the mutex will
/// always be released when the current scope (most likely
/// a function) ends.
/// This is even more important when an exception or an early
/// return statement can interrupt the execution flow of the
/// function.
///
/// For maximum robustness, sf::Lock should always be used
/// to lock/unlock a mutex.
///
/// Usage example:
/// \code
/// sf::Mutex mutex;
///
/// void function()
/// {
///     sf::Lock lock(mutex); // mutex is now locked
///
///     functionThatMayThrowAnException(); // mutex is unlocked if this function throws
///
///     if (someCondition)
///         return; // mutex is unlocked
///
/// } // mutex is unlocked
/// \endcode
///
/// Because the mutex is not explicitly unlocked in the code,
/// it may remain locked longer than needed. If the region
/// of the code that needs to be protected by the mutex is
/// not the entire function, a good practice is to create a
/// smaller, inner scope so that the lock is limited to this
/// part of the code.
///
/// \code
/// sf::Mutex mutex;
///
/// void function()
/// {
///     {
///       sf::Lock lock(mutex);
///       codeThatRequiresProtection();
///
///     } // mutex is unlocked here
///
///     codeThatDoesntCareAboutTheMutex();
/// }
/// \endcode
///
/// Having a mutex locked longer than required is a bad practice
/// which can lead to bad performances. Don't forget that when
/// a mutex is locked, other threads may be waiting doing nothing
/// until it is released.
///
/// \see sf::Mutex
///
////////////////////////////////////////////////////////////
