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
#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>
#include <SFML/Network/SocketImpl.hpp>
#include <SFML/System/Err.hpp>


namespace sf
{
////////////////////////////////////////////////////////////
TcpListener::TcpListener() :
Socket(Tcp)
{

}


////////////////////////////////////////////////////////////
unsigned short TcpListener::getLocalPort() const
{
    if (getHandle() != priv::SocketImpl::invalidSocket())
    {
        // Retrieve informations about the local end of the socket
        sockaddr_in address;
        priv::SocketImpl::AddrLength size = sizeof(address);
        if (getsockname(getHandle(), reinterpret_cast<sockaddr*>(&address), &size) != -1)
        {
            return ntohs(address.sin_port);
        }
    }

    // We failed to retrieve the port
    return 0;
}


////////////////////////////////////////////////////////////
Socket::Status TcpListener::listen(unsigned short port)
{
    // Create the internal socket if it doesn't exist
    create();

    // Bind the socket to the specified port
    sockaddr_in address = priv::SocketImpl::createAddress(INADDR_ANY, port);
    if (bind(getHandle(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1)
    {
        // Not likely to happen, but...
        err() << "Failed to bind listener socket to port " << port << std::endl;
        return Error;
    }

    // Listen to the bound port
    if (::listen(getHandle(), 0) == -1)
    {
        // Oops, socket is deaf
        err() << "Failed to listen to port " << port << std::endl;
        return Error;
    }

    return Done;
}


////////////////////////////////////////////////////////////
void TcpListener::close()
{
    // Simply close the socket
    Socket::close();
}


////////////////////////////////////////////////////////////
Socket::Status TcpListener::accept(TcpSocket& socket)
{
    // Make sure that we're listening
    if (getHandle() == priv::SocketImpl::invalidSocket())
    {
        err() << "Failed to accept a new connection, the socket is not listening" << std::endl;
        return Error;
    }

    // Accept a new connection
    sockaddr_in address;
    priv::SocketImpl::AddrLength length = sizeof(address);
    SocketHandle remote = ::accept(getHandle(), reinterpret_cast<sockaddr*>(&address), &length);

    // Check for errors
    if (remote == priv::SocketImpl::invalidSocket())
        return priv::SocketImpl::getErrorStatus();

    // Initialize the new connected socket
    socket.close();
    socket.create(remote);

    return Done;
}

} // namespace sf
