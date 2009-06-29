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

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Network/SocketHelper.hpp>
#include <errno.h>
#include <fcntl.h>


namespace sf
{
////////////////////////////////////////////////////////////
/// Return the value of the invalid socket
////////////////////////////////////////////////////////////
SocketHelper::SocketType SocketHelper::InvalidSocket()
{
    return -1;
}


////////////////////////////////////////////////////////////
/// Close / destroy a socket
////////////////////////////////////////////////////////////
bool SocketHelper::Close(SocketHelper::SocketType Socket)
{
    return close(Socket) != -1;
}


////////////////////////////////////////////////////////////
/// Set a socket as blocking or non-blocking
////////////////////////////////////////////////////////////
void SocketHelper::SetBlocking(SocketHelper::SocketType Socket, bool Block)
{
    int Status = fcntl(Socket, F_GETFL);
    if (Block)
        fcntl(Socket, F_SETFL, Status & ~O_NONBLOCK);
    else
        fcntl(Socket, F_SETFL, Status | O_NONBLOCK);
}


////////////////////////////////////////////////////////////
/// Get the last socket error status
////////////////////////////////////////////////////////////
Socket::Status SocketHelper::GetErrorStatus()
{
    // The followings are sometimes equal to EWOULDBLOCK,
    // so we have to make a special case for them in order
    // to avoid having double values in the switch case
    if ((errno == EAGAIN) || (errno == EINPROGRESS))
        return Socket::NotReady;

    switch (errno)
    {
        case EWOULDBLOCK :  return Socket::NotReady;
        case ECONNABORTED : return Socket::Disconnected;
        case ECONNRESET :   return Socket::Disconnected;
        case ETIMEDOUT :    return Socket::Disconnected;
        case ENETRESET :    return Socket::Disconnected;
        case ENOTCONN :     return Socket::Disconnected;
        default :           return Socket::Error;
    }
}

} // namespace sf
