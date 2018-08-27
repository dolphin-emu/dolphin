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

#ifndef SFML_TCPSOCKET_HPP
#define SFML_TCPSOCKET_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Network/Export.hpp>
#include <SFML/Network/Socket.hpp>
#include <SFML/System/Time.hpp>


namespace sf
{
class TcpListener;
class IpAddress;
class Packet;

////////////////////////////////////////////////////////////
/// \brief Specialized socket using the TCP protocol
///
////////////////////////////////////////////////////////////
class SFML_NETWORK_API TcpSocket : public Socket
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    TcpSocket();

    ////////////////////////////////////////////////////////////
    /// \brief Get the port to which the socket is bound locally
    ///
    /// If the socket is not connected, this function returns 0.
    ///
    /// \return Port to which the socket is bound
    ///
    /// \see connect, getRemotePort
    ///
    ////////////////////////////////////////////////////////////
    unsigned short getLocalPort() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the address of the connected peer
    ///
    /// It the socket is not connected, this function returns
    /// sf::IpAddress::None.
    ///
    /// \return Address of the remote peer
    ///
    /// \see getRemotePort
    ///
    ////////////////////////////////////////////////////////////
    IpAddress getRemoteAddress() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the port of the connected peer to which
    ///        the socket is connected
    ///
    /// If the socket is not connected, this function returns 0.
    ///
    /// \return Remote port to which the socket is connected
    ///
    /// \see getRemoteAddress
    ///
    ////////////////////////////////////////////////////////////
    unsigned short getRemotePort() const;

    ////////////////////////////////////////////////////////////
    /// \brief Connect the socket to a remote peer
    ///
    /// In blocking mode, this function may take a while, especially
    /// if the remote peer is not reachable. The last parameter allows
    /// you to stop trying to connect after a given timeout.
    /// If the socket is already connected, the connection is
    /// forcibly disconnected before attempting to connect again.
    ///
    /// \param remoteAddress Address of the remote peer
    /// \param remotePort    Port of the remote peer
    /// \param timeout       Optional maximum time to wait
    ///
    /// \return Status code
    ///
    /// \see disconnect
    ///
    ////////////////////////////////////////////////////////////
    Status connect(const IpAddress& remoteAddress, unsigned short remotePort, Time timeout = Time::Zero);

    ////////////////////////////////////////////////////////////
    /// \brief Disconnect the socket from its remote peer
    ///
    /// This function gracefully closes the connection. If the
    /// socket is not connected, this function has no effect.
    ///
    /// \see connect
    ///
    ////////////////////////////////////////////////////////////
    void disconnect();

    ////////////////////////////////////////////////////////////
    /// \brief Send raw data to the remote peer
    ///
    /// To be able to handle partial sends over non-blocking
    /// sockets, use the send(const void*, std::size_t, std::size_t&)
    /// overload instead.
    /// This function will fail if the socket is not connected.
    ///
    /// \param data Pointer to the sequence of bytes to send
    /// \param size Number of bytes to send
    ///
    /// \return Status code
    ///
    /// \see receive
    ///
    ////////////////////////////////////////////////////////////
    Status send(const void* data, std::size_t size);

    ////////////////////////////////////////////////////////////
    /// \brief Send raw data to the remote peer
    ///
    /// This function will fail if the socket is not connected.
    ///
    /// \param data Pointer to the sequence of bytes to send
    /// \param size Number of bytes to send
    /// \param sent The number of bytes sent will be written here
    ///
    /// \return Status code
    ///
    /// \see receive
    ///
    ////////////////////////////////////////////////////////////
    Status send(const void* data, std::size_t size, std::size_t& sent);

    ////////////////////////////////////////////////////////////
    /// \brief Receive raw data from the remote peer
    ///
    /// In blocking mode, this function will wait until some
    /// bytes are actually received.
    /// This function will fail if the socket is not connected.
    ///
    /// \param data     Pointer to the array to fill with the received bytes
    /// \param size     Maximum number of bytes that can be received
    /// \param received This variable is filled with the actual number of bytes received
    ///
    /// \return Status code
    ///
    /// \see send
    ///
    ////////////////////////////////////////////////////////////
    Status receive(void* data, std::size_t size, std::size_t& received);

    ////////////////////////////////////////////////////////////
    /// \brief Send a formatted packet of data to the remote peer
    ///
    /// In non-blocking mode, if this function returns sf::Socket::Partial,
    /// you \em must retry sending the same unmodified packet before sending
    /// anything else in order to guarantee the packet arrives at the remote
    /// peer uncorrupted.
    /// This function will fail if the socket is not connected.
    ///
    /// \param packet Packet to send
    ///
    /// \return Status code
    ///
    /// \see receive
    ///
    ////////////////////////////////////////////////////////////
    Status send(Packet& packet);

    ////////////////////////////////////////////////////////////
    /// \brief Receive a formatted packet of data from the remote peer
    ///
    /// In blocking mode, this function will wait until the whole packet
    /// has been received.
    /// This function will fail if the socket is not connected.
    ///
    /// \param packet Packet to fill with the received data
    ///
    /// \return Status code
    ///
    /// \see send
    ///
    ////////////////////////////////////////////////////////////
    Status receive(Packet& packet);

private:

    friend class TcpListener;

    ////////////////////////////////////////////////////////////
    /// \brief Structure holding the data of a pending packet
    ///
    ////////////////////////////////////////////////////////////
    struct PendingPacket
    {
        PendingPacket();

        Uint32            Size;         ///< Data of packet size
        std::size_t       SizeReceived; ///< Number of size bytes received so far
        std::vector<char> Data;         ///< Data of the packet
    };

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    PendingPacket m_pendingPacket; ///< Temporary data of the packet currently being received
};

} // namespace sf


#endif // SFML_TCPSOCKET_HPP


////////////////////////////////////////////////////////////
/// \class sf::TcpSocket
/// \ingroup network
///
/// TCP is a connected protocol, which means that a TCP
/// socket can only communicate with the host it is connected
/// to. It can't send or receive anything if it is not connected.
///
/// The TCP protocol is reliable but adds a slight overhead.
/// It ensures that your data will always be received in order
/// and without errors (no data corrupted, lost or duplicated).
///
/// When a socket is connected to a remote host, you can
/// retrieve informations about this host with the
/// getRemoteAddress and getRemotePort functions. You can
/// also get the local port to which the socket is bound
/// (which is automatically chosen when the socket is connected),
/// with the getLocalPort function.
///
/// Sending and receiving data can use either the low-level
/// or the high-level functions. The low-level functions
/// process a raw sequence of bytes, and cannot ensure that
/// one call to Send will exactly match one call to Receive
/// at the other end of the socket.
///
/// The high-level interface uses packets (see sf::Packet),
/// which are easier to use and provide more safety regarding
/// the data that is exchanged. You can look at the sf::Packet
/// class to get more details about how they work.
///
/// The socket is automatically disconnected when it is destroyed,
/// but if you want to explicitly close the connection while
/// the socket instance is still alive, you can call disconnect.
///
/// Usage example:
/// \code
/// // ----- The client -----
///
/// // Create a socket and connect it to 192.168.1.50 on port 55001
/// sf::TcpSocket socket;
/// socket.connect("192.168.1.50", 55001);
///
/// // Send a message to the connected host
/// std::string message = "Hi, I am a client";
/// socket.send(message.c_str(), message.size() + 1);
///
/// // Receive an answer from the server
/// char buffer[1024];
/// std::size_t received = 0;
/// socket.receive(buffer, sizeof(buffer), received);
/// std::cout << "The server said: " << buffer << std::endl;
///
/// // ----- The server -----
///
/// // Create a listener to wait for incoming connections on port 55001
/// sf::TcpListener listener;
/// listener.listen(55001);
///
/// // Wait for a connection
/// sf::TcpSocket socket;
/// listener.accept(socket);
/// std::cout << "New client connected: " << socket.getRemoteAddress() << std::endl;
///
/// // Receive a message from the client
/// char buffer[1024];
/// std::size_t received = 0;
/// socket.receive(buffer, sizeof(buffer), received);
/// std::cout << "The client said: " << buffer << std::endl;
///
/// // Send an answer
/// std::string message = "Welcome, client";
/// socket.send(message.c_str(), message.size() + 1);
/// \endcode
///
/// \see sf::Socket, sf::UdpSocket, sf::Packet
///
////////////////////////////////////////////////////////////
