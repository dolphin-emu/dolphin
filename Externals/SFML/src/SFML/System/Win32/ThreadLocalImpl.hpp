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

#ifndef SFML_THREADLOCALIMPL_HPP
#define SFML_THREADLOCALIMPL_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/System/NonCopyable.hpp>
#include <windows.h>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Windows implementation of thread-local storage
////////////////////////////////////////////////////////////
class ThreadLocalImpl : NonCopyable
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor -- allocate the storage
    ///
    ////////////////////////////////////////////////////////////
    ThreadLocalImpl();

    ////////////////////////////////////////////////////////////
    /// \brief Destructor -- free the storage
    ///
    ////////////////////////////////////////////////////////////
    ~ThreadLocalImpl();

    ////////////////////////////////////////////////////////////
    /// \brief Set the thread-specific value of the variable
    ///
    /// \param value Value of the variable for this thread
    ///
    ////////////////////////////////////////////////////////////
    void setValue(void* value);

    ////////////////////////////////////////////////////////////
    /// \brief Retrieve the thread-specific value of the variable
    ///
    /// \return Value of the variable for this thread
    ///
    ////////////////////////////////////////////////////////////
    void* getValue() const;

private:

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    DWORD m_index; ///< Index of our thread-local storage slot
};

} // namespace priv

} // namespace sf


#endif // SFML_THREADLOCALIMPL_HPP
