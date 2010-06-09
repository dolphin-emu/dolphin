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

#ifndef SFML_SOCKETHELPERWIN32_HPP
#define SFML_SOCKETHELPERWIN32_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <winsock2.h>


namespace sf
{
////////////////////////////////////////////////////////////
/// This class defines helper functions to do all the
/// non-portable socket stuff. This class is meant for internal
/// use only
////////////////////////////////////////////////////////////
class SFML_API SocketHelper
{
public :

    ////////////////////////////////////////////////////////////
    // Define some socket types
    ////////////////////////////////////////////////////////////
    typedef SOCKET SocketType;
    typedef int    LengthType;

    ////////////////////////////////////////////////////////////
    /// Return the value of the invalid socket
    ///
    /// \return Unique value of the invalid socket
    ///
    ////////////////////////////////////////////////////////////
    static SocketType InvalidSocket();

    ////////////////////////////////////////////////////////////
    /// Close / destroy a socket
    ///
    /// \param Socket : Socket to close
    ///
    /// \return True on success
    ///
    ////////////////////////////////////////////////////////////
    static bool Close(SocketType Socket);

    ////////////////////////////////////////////////////////////
    /// Set a socket as blocking or non-blocking
    ///
    /// \param Socket : Socket to modify
    /// \param Block :  New blocking state of the socket
    ///
    ////////////////////////////////////////////////////////////
    static void SetBlocking(SocketType Socket, bool Block);

    ////////////////////////////////////////////////////////////
    /// Get the last socket error status
    ///
    /// \return Status corresponding to the last socket error
    ///
    ////////////////////////////////////////////////////////////
    static Socket::Status GetErrorStatus();
};

} // namespace sf


#endif // SFML_SOCKETHELPERWIN32_HPP
