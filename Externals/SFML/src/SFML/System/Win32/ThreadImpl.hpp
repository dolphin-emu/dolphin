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
#include <SFML/System/NonCopyable.hpp>
#include <windows.h>

// Fix for unaligned stack with clang and GCC on Windows XP 32-bit
#if defined(SFML_SYSTEM_WINDOWS) && (defined(__clang__) || defined(__GNUC__))

    #define ALIGN_STACK __attribute__((__force_align_arg_pointer__))

#else

    #define ALIGN_STACK

#endif


namespace sf
{
class Thread;

namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Windows implementation of threads
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
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~ThreadImpl();

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
    /// \return OS specific error code
    ///
    ////////////////////////////////////////////////////////////
    ALIGN_STACK static unsigned int __stdcall entryPoint(void* userData);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    HANDLE m_thread; ///< Win32 thread handle
    unsigned int m_threadId; ///< Win32 thread identifier
};

} // namespace priv

} // namespace sf


#endif // SFML_THREADIMPL_HPP
