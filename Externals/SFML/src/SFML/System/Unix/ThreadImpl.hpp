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

#ifndef SFML_THREADIMPL_HPP
#define SFML_THREADIMPL_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Config.hpp>
#include <SFML/System/NonCopyable.hpp>
#include <pthread.h>


namespace sf
{
class Thread;

namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Unix implementation of threads
////////////////////////////////////////////////////////////
class ThreadImpl : NonCopyable
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor, launch the thread
    ///
    /// \param owner The Thread instance to run
    ///
    ////////////////////////////////////////////////////////////
    ThreadImpl(Thread* owner);

    ////////////////////////////////////////////////////////////
    /// \brief Wait until the thread finishes
    ///
    ////////////////////////////////////////////////////////////
    void wait();

    ////////////////////////////////////////////////////////////
    /// \brief Terminate the thread
    ///
    ////////////////////////////////////////////////////////////
    void terminate();

private:

    ////////////////////////////////////////////////////////////
    /// \brief Global entry point for all threads
    ///
    /// \param userData User-defined data (contains the Thread instance)
    ///
    /// \return Os specific error code
    ///
    ////////////////////////////////////////////////////////////
    static void* entryPoint(void* userData);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    pthread_t m_thread;   ///< pthread thread instance
    bool      m_isActive; ///< Thread state (active or inactive)
};

} // namespace priv

} // namespace sf


#endif // SFML_THREADIMPL_HPP
