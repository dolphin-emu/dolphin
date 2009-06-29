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


namespace sf
{
////////////////////////////////////////////////////////////
/// Return the value of the invalid socket
////////////////////////////////////////////////////////////
SocketHelper::SocketType SocketHelper::InvalidSocket()
{
    return INVALID_SOCKET;
}


////////////////////////////////////////////////////////////
/// Close / destroy a socket
////////////////////////////////////////////////////////////
bool SocketHelper::Close(SocketHelper::SocketType Socket)
{
    return closesocket(Socket) != -1;
}


////////////////////////////////////////////////////////////
/// Set a socket as blocking or non-blocking
////////////////////////////////////////////////////////////
void SocketHelper::SetBlocking(SocketHelper::SocketType Socket, bool Block)
{
    unsigned long Blocking = Block ? 0 : 1;
    ioctlsocket(Socket, FIONBIO, &Blocking);
}


////////////////////////////////////////////////////////////
/// Get the last socket error status
////////////////////////////////////////////////////////////
Socket::Status SocketHelper::GetErrorStatus()
{
    switch (WSAGetLastError())
    {
        case WSAEWOULDBLOCK :  return Socket::NotReady;
        case WSAECONNABORTED : return Socket::Disconnected;
        case WSAECONNRESET :   return Socket::Disconnected;
        case WSAETIMEDOUT :    return Socket::Disconnected;
        case WSAENETRESET :    return Socket::Disconnected;
        case WSAENOTCONN :     return Socket::Disconnected;
        default :              return Socket::Error;
    }
}


////////////////////////////////////////////////////////////
// Windows needs some initialization and cleanup to get
// sockets working properly... so let's create a class that will
// do it automatically
////////////////////////////////////////////////////////////
struct SocketInitializer
{
    SocketInitializer()
    {
        WSADATA InitData;
        WSAStartup(MAKEWORD(2,2), &InitData);
    }

    ~SocketInitializer()
    {
        WSACleanup();
    }
};

SocketInitializer GlobalInitializer;

} // namespace sf
