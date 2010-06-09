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

#ifndef SFML_SOCKETTCP_HPP
#define SFML_SOCKETTCP_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Network/SocketHelper.hpp>
#include <vector>


namespace sf
{
class Packet;
class IPAddress;
template <typename> class Selector;

////////////////////////////////////////////////////////////
/// SocketTCP wraps a socket using TCP protocol to
/// send data safely (but a bit slower)
////////////////////////////////////////////////////////////
class SFML_API SocketTCP
{
public :

    ////////////////////////////////////////////////////////////
    /// Default constructor
    ///
    ////////////////////////////////////////////////////////////
    SocketTCP();

    ////////////////////////////////////////////////////////////
    /// Change the blocking state of the socket.
    /// The default behaviour of a socket is blocking
    ///
    /// \param Blocking : Pass true to set the socket as blocking, or false for non-blocking
    ///
    ////////////////////////////////////////////////////////////
    void SetBlocking(bool Blocking);

    ////////////////////////////////////////////////////////////
    /// Connect to another computer on a specified port
    ///
    /// \param Port :        Port to use for transfers (warning : ports < 1024 are reserved)
    /// \param HostAddress : IP Address of the host to connect to
    /// \param Timeout :     Maximum time to wait, in seconds (0 by default : no timeout) (this parameter is ignored for non-blocking sockets)
    ///
    /// \return True if operation has been successful
    ///
    ////////////////////////////////////////////////////////////
    Socket::Status Connect(unsigned short Port, const IPAddress& HostAddress, float Timeout = 0.f);

    ////////////////////////////////////////////////////////////
    /// Listen to a specified port for incoming data or connections
    ///
    /// \param Port : Port to listen to
    ///
    /// \return True if operation has been successful
    ///
    ////////////////////////////////////////////////////////////
    bool Listen(unsigned short Port);

    ////////////////////////////////////////////////////////////
    /// Wait for a connection (must be listening to a port).
    /// This function will block if the socket is blocking
    ///
    /// \param Connected : Socket containing the connection with the connected client
    /// \param Address :   Pointer to an address to fill with client infos (NULL by default)
    ///
    /// \return Status code
    ///
    ////////////////////////////////////////////////////////////
    Socket::Status Accept(SocketTCP& Connected, IPAddress* Address = NULL);

    ////////////////////////////////////////////////////////////
    /// Send an array of bytes to the host (must be connected first)
    ///
    /// \param Data : Pointer to the bytes to send
    /// \param Size : Number of bytes to send
    ///
    /// \return Status code
    ///
    ////////////////////////////////////////////////////////////
    Socket::Status Send(const char* Data, std::size_t Size);

    ////////////////////////////////////////////////////////////
    /// Receive an array of bytes from the host (must be connected first).
    /// This function will block if the socket is blocking
    ///
    /// \param Data :         Pointer to a byte array to fill (make sure it is big enough)
    /// \param MaxSize :      Maximum number of bytes to read
    /// \param SizeReceived : Number of bytes received
    ///
    /// \return Status code
    ///
    ////////////////////////////////////////////////////////////
    Socket::Status Receive(char* Data, std::size_t MaxSize, std::size_t& SizeReceived);

    ////////////////////////////////////////////////////////////
    /// Send a packet of data to the host (must be connected first)
    ///
    /// \param PacketToSend : Packet to send
    ///
    /// \return Status code
    ///
    ////////////////////////////////////////////////////////////
    Socket::Status Send(Packet& PacketToSend);

    ////////////////////////////////////////////////////////////
    /// Receive a packet from the host (must be connected first).
    /// This function will block if the socket is blocking
    ///
    /// \param PacketToReceive : Packet to fill with received data
    ///
    /// \return Status code
    ///
    ////////////////////////////////////////////////////////////
    Socket::Status Receive(Packet& PacketToReceive);

    ////////////////////////////////////////////////////////////
    /// Close the socket
    ///
    /// \return True if operation has been successful
    ///
    ////////////////////////////////////////////////////////////
    bool Close();

    ////////////////////////////////////////////////////////////
    /// Check if the socket is in a valid state ; this function
    /// can be called any time to check if the socket is OK
    ///
    /// \return True if the socket is valid
    ///
    ////////////////////////////////////////////////////////////
    bool IsValid() const;

    ////////////////////////////////////////////////////////////
    /// Comparison operator ==
    ///
    /// \param Other : Socket to compare
    ///
    /// \return True if *this == Other
    ///
    ////////////////////////////////////////////////////////////
    bool operator ==(const SocketTCP& Other) const;

    ////////////////////////////////////////////////////////////
    /// Comparison operator !=
    ///
    /// \param Other : Socket to compare
    ///
    /// \return True if *this != Other
    ///
    ////////////////////////////////////////////////////////////
    bool operator !=(const SocketTCP& Other) const;

    ////////////////////////////////////////////////////////////
    /// Comparison operator <.
    /// Provided for compatibility with standard containers, as
    /// comparing two sockets doesn't make much sense...
    ///
    /// \param Other : Socket to compare
    ///
    /// \return True if *this < Other
    ///
    ////////////////////////////////////////////////////////////
    bool operator <(const SocketTCP& Other) const;

private :

    friend class Selector<SocketTCP>;

    ////////////////////////////////////////////////////////////
    /// Construct the socket from a socket descriptor
    /// (for internal use only)
    ///
    /// \param Descriptor : Socket descriptor
    ///
    ////////////////////////////////////////////////////////////
    SocketTCP(SocketHelper::SocketType Descriptor);

    ////////////////////////////////////////////////////////////
    /// Create the socket
    ///
    /// \param Descriptor : System socket descriptor to use (0 by default -- create a new socket)
    ///
    ////////////////////////////////////////////////////////////
    void Create(SocketHelper::SocketType Descriptor = 0);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    SocketHelper::SocketType mySocket;            ///< Socket descriptor
    Uint32                   myPendingHeader;     ///< Data of the current pending packet header, if any
    Uint32                   myPendingHeaderSize; ///< Size of the current pending packet header, if any
    std::vector<char>        myPendingPacket;     ///< Data of the current pending packet, if any
    Int32                    myPendingPacketSize; ///< Size of the current pending packet, if any
    bool                     myIsBlocking;        ///< Is the socket blocking or non-blocking ?
};

} // namespace sf


#endif // SFML_SOCKETTCP_HPP
