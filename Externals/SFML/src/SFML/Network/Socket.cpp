////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2013 Laurent Gomila (laurent.gom@gmail.com)
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
#include <SFML/Network/Socket.hpp>
#include <SFML/Network/SocketImpl.hpp>
#include <SFML/System/Err.hpp>


namespace sf
{
////////////////////////////////////////////////////////////
Socket::Socket(Type type) :
m_type      (type),
m_socket    (priv::SocketImpl::invalidSocket()),
m_isBlocking(true)
{

}


////////////////////////////////////////////////////////////
Socket::~Socket()
{
    // Close the socket before it gets destructed
    close();
}


////////////////////////////////////////////////////////////
void Socket::setBlocking(bool blocking)
{
    // Apply if the socket is already created
    if (m_socket != priv::SocketImpl::invalidSocket())
        priv::SocketImpl::setBlocking(m_socket, blocking);

    m_isBlocking = blocking;
}


////////////////////////////////////////////////////////////
bool Socket::isBlocking() const
{
    return m_isBlocking;
}


////////////////////////////////////////////////////////////
SocketHandle Socket::getHandle() const
{
    return m_socket;
}


////////////////////////////////////////////////////////////
void Socket::create()
{
    // Don't create the socket if it already exists
    if (m_socket == priv::SocketImpl::invalidSocket())
    {
        SocketHandle handle = socket(PF_INET, m_type == Tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
        create(handle);
    }
}


////////////////////////////////////////////////////////////
void Socket::create(SocketHandle handle)
{
    // Don't create the socket if it already exists
    if (m_socket == priv::SocketImpl::invalidSocket())
    {
        // Assign the new handle
        m_socket = handle;

        // Set the current blocking state
        setBlocking(m_isBlocking);

        if (m_type == Tcp)
        {
            // Disable the Nagle algorithm (ie. removes buffering of TCP packets)
            int yes = 1;
            if (setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&yes), sizeof(yes)) == -1)
            {
                err() << "Failed to set socket option \"TCP_NODELAY\" ; "
                      << "all your TCP packets will be buffered" << std::endl;
            }

            // On Mac OS X, disable the SIGPIPE signal on disconnection
            #ifdef SFML_SYSTEM_MACOS
                if (setsockopt(m_socket, SOL_SOCKET, SO_NOSIGPIPE, reinterpret_cast<char*>(&yes), sizeof(yes)) == -1)
                {
                    err() << "Failed to set socket option \"SO_NOSIGPIPE\"" << std::endl;
                }
            #endif
        }
        else
        {
            // Enable broadcast by default for UDP sockets
            int yes = 1;
            if (setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char*>(&yes), sizeof(yes)) == -1)
            {
                err() << "Failed to enable broadcast on UDP socket" << std::endl;
            }
        }
    }
}


////////////////////////////////////////////////////////////
void Socket::close()
{
    // Close the socket
    if (m_socket != priv::SocketImpl::invalidSocket())
    {
        priv::SocketImpl::close(m_socket);
        m_socket = priv::SocketImpl::invalidSocket();
    }
}

} // namespace sf
