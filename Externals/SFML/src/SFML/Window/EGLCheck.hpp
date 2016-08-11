////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2013 Jonathan De Wachter (dewachter.jonathan@gmail.com)
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

#ifndef SFML_EGLCHECK_HPP
#define SFML_EGLCHECK_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Config.hpp>
#include <EGL/egl.h>
#include <string>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
/// Let's define a macro to quickly check every EGL API call
////////////////////////////////////////////////////////////
#ifdef SFML_DEBUG

    //// In debug mode, perform a test on every EGL call
    #define eglCheck(x) x; sf::priv::eglCheckError(__FILE__, __LINE__);

#else

    // Else, we don't add any overhead
    #define eglCheck(x) (x)

#endif

////////////////////////////////////////////////////////////
/// \brief Check the last EGL error
///
/// \param file Source file where the call is located
/// \param line Line number of the source file where the call is located
///
////////////////////////////////////////////////////////////
void eglCheckError(const char* file, unsigned int line);

} // namespace priv
} // namespace sf


#endif // SFML_EGLCHECK_HPP
