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

#ifndef SFML_SCOPEDXCBPTR_HPP
#define SFML_SCOPEDXCBPTR_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <cstdlib>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Scoped pointer that frees memory returned in XCB replies
///
////////////////////////////////////////////////////////////
template<typename T>
class ScopedXcbPtr
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Constructor
    ///
    /// \param pointer Pointer value to store
    ///
    ////////////////////////////////////////////////////////////
    ScopedXcbPtr(T* pointer);

    ////////////////////////////////////////////////////////////
    /// \brief Destructor, calls std::free() on the stored pointer
    ///
    ////////////////////////////////////////////////////////////
    ~ScopedXcbPtr();

    ////////////////////////////////////////////////////////////
    /// \brief Structure dereference operator
    ///
    /// \return Stored pointer
    ///
    ////////////////////////////////////////////////////////////
    T* operator ->() const;

    ////////////////////////////////////////////////////////////
    /// \brief Address operator.
    ///
    /// \return Address of the stored pointer
    ///
    ////////////////////////////////////////////////////////////
    T** operator &();

    ////////////////////////////////////////////////////////////
    /// \brief Check if stored pointer is valid
    ///
    /// \return true if stored pointer is valid
    ///
    ////////////////////////////////////////////////////////////
    operator bool() const;

    ////////////////////////////////////////////////////////////
    /// \brief Retrieve the stored pointer.
    ///
    /// \return The stored pointer
    ///
    ////////////////////////////////////////////////////////////
    T* get() const;

private:
    T* m_pointer; ///< Stored pointer
};

#include <SFML/Window/Unix/ScopedXcbPtr.inl>

} // namespace priv

} // namespace sf

#endif // SFML_SCOPEDXCBPTR_HPP
