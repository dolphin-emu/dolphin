////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2018 Laurent Gomila (laurent@sfml-dev.org)
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
#include <SFML/Network/Unix/SocketImpl.hpp>
#include <SFML/System/Err.hpp>
#include <errno.h>
#include <fcntl.h>
#include <cstring>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
sockaddr_in SocketImpl::createAddress(Uint32 address, unsigned short port)
{
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = htonl(address);
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);

#if defined(SFML_SYSTEM_MACOS)
    addr.sin_len = sizeof(addr);
#endif

    return addr;
}


////////////////////////////////////////////////////////////
SocketHandle SocketImpl::invalidSocket()
{
    return -1;
}


////////////////////////////////////////////////////////////
void SocketImpl::close(SocketHandle sock)
{
    ::close(sock);
}


////////////////////////////////////////////////////////////
void SocketImpl::setBlocking(SocketHandle sock, bool block)
{
    int status = fcntl(sock, F_GETFL);
    if (block)
    {
        if (fcntl(sock, F_SETFL, status & ~O_NONBLOCK) == -1)
            err() << "Failed to set file status flags: " << errno << std::endl;
    }
    else
    {
        if (fcntl(sock, F_SETFL, status | O_NONBLOCK) == -1)
            err() << "Failed to set file status flags: " << errno << std::endl;

    }
}


////////////////////////////////////////////////////////////
Socket::Status SocketImpl::getErrorStatus()
{
    // The followings are sometimes equal to EWOULDBLOCK,
    // so we have to make a special case for them in order
    // to avoid having double values in the switch case
    if ((errno == EAGAIN) || (errno == EINPROGRESS))
        return Socket::NotReady;

    switch (errno)
    {
        case EWOULDBLOCK:  return Socket::NotReady;
        case ECONNABORTED: return Socket::Disconnected;
        case ECONNRESET:   return Socket::Disconnected;
        case ETIMEDOUT:    return Socket::Disconnected;
        case ENETRESET:    return Socket::Disconnected;
        case ENOTCONN:     return Socket::Disconnected;
        case EPIPE:        return Socket::Disconnected;
        default:           return Socket::Error;
    }
}

} // namespace priv

} // namespace sf
