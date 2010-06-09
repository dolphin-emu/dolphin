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

#ifndef SFML_SOCKETHELPER_HPP
#define SFML_SOCKETHELPER_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Config.hpp>


namespace sf
{
namespace Socket
{
    ////////////////////////////////////////////////////////////
    /// Enumeration of status returned by socket functions
    ////////////////////////////////////////////////////////////
    enum Status
    {
        Done,         ///< The socket has sent / received the data
        NotReady,     ///< The socket is not ready to send / receive data yet
        Disconnected, ///< The TCP socket has been disconnected
        Error         ///< An unexpected error happened
    };
}

} // namespace sf


#ifdef SFML_SYSTEM_WINDOWS

    #include <SFML/Network/Win32/SocketHelper.hpp>

#else

    #include <SFML/Network/Unix/SocketHelper.hpp>

#endif


#endif // SFML_SOCKETHELPER_HPP
