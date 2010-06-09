////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2009 Laurent Gomila (laurent.gom@gmail.com)
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

#ifndef SFML_NONCOPYABLE_HPP
#define SFML_NONCOPYABLE_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Config.hpp>


namespace sf
{
////////////////////////////////////////////////////////////
/// Utility base class to easily declare non-copyable classes.
/// Just inherit from NonCopyable to get a non-copyable class
////////////////////////////////////////////////////////////
struct SFML_API NonCopyable
{
protected :

    ////////////////////////////////////////////////////////////
    /// The default constructor won't be generated, so provide it
    ///
    ////////////////////////////////////////////////////////////
    NonCopyable() {}

private :

    ////////////////////////////////////////////////////////////
    /// Copy constructor : declare it private and don't implement
    /// it to prevent from calling it
    ///
    ////////////////////////////////////////////////////////////
    NonCopyable(const NonCopyable&);

    ////////////////////////////////////////////////////////////
    /// Assignment operator : declare it private and don't implement
    /// it to prevent from calling it
    ///
    ////////////////////////////////////////////////////////////
    NonCopyable& operator =(const NonCopyable&);
};

} // namespace sf


#endif // SFML_NONCOPYABLE_HPP
