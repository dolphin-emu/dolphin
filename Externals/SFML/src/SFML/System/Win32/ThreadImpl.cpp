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
#include <SFML/System/Win32/ThreadImpl.hpp>
#include <SFML/System/Thread.hpp>
#include <SFML/System/Err.hpp>
#include <cassert>
#include <process.h>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
ThreadImpl::ThreadImpl(Thread* owner)
{
    m_thread = reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, &ThreadImpl::entryPoint, owner, 0, &m_threadId));

    if (!m_thread)
        err() << "Failed to create thread" << std::endl;
}


////////////////////////////////////////////////////////////
ThreadImpl::~ThreadImpl()
{
    if (m_thread)
        CloseHandle(m_thread);
}


////////////////////////////////////////////////////////////
void ThreadImpl::wait()
{
    if (m_thread)
    {
        assert(m_threadId != GetCurrentThreadId()); // A thread cannot wait for itself!
        WaitForSingleObject(m_thread, INFINITE);
    }
}


////////////////////////////////////////////////////////////
void ThreadImpl::terminate()
{
    if (m_thread)
        TerminateThread(m_thread, 0);
}


////////////////////////////////////////////////////////////
unsigned int __stdcall ThreadImpl::entryPoint(void* userData)
{
    // The Thread instance is stored in the user data
    Thread* owner = static_cast<Thread*>(userData);

    // Forward to the owner
    owner->run();

    // Optional, but it is cleaner
    _endthreadex(0);

    return 0;
}

} // namespace priv

} // namespace sf
