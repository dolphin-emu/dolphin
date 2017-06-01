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

#ifndef SFML_ALCHECK_HPP
#define SFML_ALCHECK_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Config.hpp>
#ifdef SFML_SYSTEM_IOS
    #include <OpenAl/al.h>
    #include <OpenAl/alc.h>
#else
    #include <al.h>
    #include <alc.h>
#endif


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
/// Let's define a macro to quickly check every OpenAL API call
////////////////////////////////////////////////////////////
#ifdef SFML_DEBUG

    // If in debug mode, perform a test on every call
    // The do-while loop is needed so that alCheck can be used as a single statement in if/else branches
    #define alCheck(expr) do { expr; sf::priv::alCheckError(__FILE__, __LINE__, #expr); } while (false)

#else

    // Else, we don't add any overhead
    #define alCheck(expr) (expr)

#endif


////////////////////////////////////////////////////////////
/// Check the last OpenAL error
///
/// \param file Source file where the call is located
/// \param line Line number of the source file where the call is located
/// \param expression The evaluated expression as a string
///
////////////////////////////////////////////////////////////
void alCheckError(const char* file, unsigned int line, const char* expression);

} // namespace priv

} // namespace sf


#endif // SFML_ALCHECK_HPP
