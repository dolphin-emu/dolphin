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

#ifndef SFML_MUTEX_HPP
#define SFML_MUTEX_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/System/Export.hpp>
#include <SFML/System/NonCopyable.hpp>


namespace sf
{
namespace priv
{
    class MutexImpl;
}

////////////////////////////////////////////////////////////
/// \brief Blocks concurrent access to shared resources
///        from multiple threads
///
////////////////////////////////////////////////////////////
class SFML_SYSTEM_API Mutex : NonCopyable
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    Mutex();

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~Mutex();

    ////////////////////////////////////////////////////////////
    /// \brief Lock the mutex
    ///
    /// If the mutex is already locked in another thread,
    /// this call will block the execution until the mutex
    /// is released.
    ///
    /// \see unlock
    ///
    ////////////////////////////////////////////////////////////
    void lock();

    ////////////////////////////////////////////////////////////
    /// \brief Unlock the mutex
    ///
    /// \see lock
    ///
    ////////////////////////////////////////////////////////////
    void unlock();

private:

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    priv::MutexImpl* m_mutexImpl; ///< OS-specific implementation
};

} // namespace sf


#endif // SFML_MUTEX_HPP


////////////////////////////////////////////////////////////
/// \class sf::Mutex
/// \ingroup system
///
/// Mutex stands for "MUTual EXclusion". A mutex is a
/// synchronization object, used when multiple threads are involved.
///
/// When you want to protect a part of the code from being accessed
/// simultaneously by multiple threads, you typically use a
/// mutex. When a thread is locked by a mutex, any other thread
/// trying to lock it will be blocked until the mutex is released
/// by the thread that locked it. This way, you can allow only
/// one thread at a time to access a critical region of your code.
///
/// Usage example:
/// \code
/// Database database; // this is a critical resource that needs some protection
/// sf::Mutex mutex;
///
/// void thread1()
/// {
///     mutex.lock(); // this call will block the thread if the mutex is already locked by thread2
///     database.write(...);
///     mutex.unlock(); // if thread2 was waiting, it will now be unblocked
/// }
///
/// void thread2()
/// {
///     mutex.lock(); // this call will block the thread if the mutex is already locked by thread1
///     database.write(...);
///     mutex.unlock(); // if thread1 was waiting, it will now be unblocked
/// }
/// \endcode
///
/// Be very careful with mutexes. A bad usage can lead to bad problems,
/// like deadlocks (two threads are waiting for each other and the
/// application is globally stuck).
///
/// To make the usage of mutexes more robust, particularly in
/// environments where exceptions can be thrown, you should
/// use the helper class sf::Lock to lock/unlock mutexes.
///
/// SFML mutexes are recursive, which means that you can lock
/// a mutex multiple times in the same thread without creating
/// a deadlock. In this case, the first call to lock() behaves
/// as usual, and the following ones have no effect.
/// However, you must call unlock() exactly as many times as you
/// called lock(). If you don't, the mutex won't be released.
///
/// \see sf::Lock
///
////////////////////////////////////////////////////////////
