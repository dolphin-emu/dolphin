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
#include <SFML/Network/SocketUDP.hpp>
#include <SFML/Network/IPAddress.hpp>
#include <SFML/Network/Packet.hpp>
#include <algorithm>
#include <iostream>
#include <string.h>


namespace sf
{
////////////////////////////////////////////////////////////
/// Default constructor
////////////////////////////////////////////////////////////
SocketUDP::SocketUDP()
{
    Create();
}


////////////////////////////////////////////////////////////
/// Change the blocking state of the socket
////////////////////////////////////////////////////////////
void SocketUDP::SetBlocking(bool Blocking)
{
    // Make sure our socket is valid
    if (!IsValid())
        Create();

    SocketHelper::SetBlocking(mySocket, Blocking);
    myIsBlocking = Blocking;
}


////////////////////////////////////////////////////////////
/// Bind the socket to a specific port
////////////////////////////////////////////////////////////
bool SocketUDP::Bind(unsigned short Port)
{
    // Check if the socket is already bound to the specified port
    if (myPort != Port)
    {
        // If the socket was previously bound to another port, we need to unbind it first
        Unbind();

        if (Port != 0)
        {
            // Build an address with the specified port
            sockaddr_in Addr;
            Addr.sin_family      = AF_INET;
            Addr.sin_port        = htons(Port);
            Addr.sin_addr.s_addr = INADDR_ANY;
            memset(Addr.sin_zero, 0, sizeof(Addr.sin_zero));

            // Bind the socket to the port
            if (bind(mySocket, reinterpret_cast<sockaddr*>(&Addr), sizeof(Addr)) == -1)
            {
                std::cerr << "Failed to bind the socket to port " << Port << std::endl;
                myPort = 0;
                return false;
            }
        }

        // Save the new port
        myPort = Port;
    }

    return true;
}


////////////////////////////////////////////////////////////
/// Unbind the socket to its previous port
////////////////////////////////////////////////////////////
bool SocketUDP::Unbind()
{
    // To unbind the socket, we just recreate it
    if (myPort != 0)
    {
        Close();
        Create();
        myPort = 0;
    }

    return true;
}


////////////////////////////////////////////////////////////
/// Send an array of bytes
////////////////////////////////////////////////////////////
Socket::Status SocketUDP::Send(const char* Data, std::size_t Size, const IPAddress& Address, unsigned short Port)
{
    // Make sure the socket is valid
    if (!IsValid())
        Create();

    // Check parameters
    if (Data && Size)
    {
        // Build the target address
        sockaddr_in Target;
        Target.sin_family      = AF_INET;
        Target.sin_port        = htons(Port);
        Target.sin_addr.s_addr = inet_addr(Address.ToString().c_str());
        memset(Target.sin_zero, 0, sizeof(Target.sin_zero));

        // Loop until every byte has been sent
        int Sent = 0;
        int SizeToSend = static_cast<int>(Size);
        for (int Length = 0; Length < SizeToSend; Length += Sent)
        {
            // Send a chunk of data
            Sent = sendto(mySocket, Data + Length, SizeToSend - Length, 0, reinterpret_cast<sockaddr*>(&Target), sizeof(Target));

            // Check errors
            if (Sent <= 0)
                return SocketHelper::GetErrorStatus();
        }

        return Socket::Done;
    }
    else
    {
        // Error...
        std::cerr << "Cannot send data over the network (invalid parameters)" << std::endl;
        return Socket::Error;
    }
}


////////////////////////////////////////////////////////////
/// Receive an array of bytes.
/// This function will block if the socket is blocking
////////////////////////////////////////////////////////////
Socket::Status SocketUDP::Receive(char* Data, std::size_t MaxSize, std::size_t& SizeReceived, IPAddress& Address, unsigned short& Port)
{
    // First clear the size received
    SizeReceived = 0;

    // Make sure the socket is bound to a port
    if (myPort == 0)
    {
        std::cerr << "Failed to receive data ; the UDP socket first needs to be bound to a port" << std::endl;
        return Socket::Error;
    }

    // Make sure the socket is valid
    if (!IsValid())
        Create();

    // Check parameters
    if (Data && MaxSize)
    {
        // Data that will be filled with the other computer's address
        sockaddr_in Sender;
        Sender.sin_family      = AF_INET;
        Sender.sin_port        = 0;
        Sender.sin_addr.s_addr = INADDR_ANY;
        memset(Sender.sin_zero, 0, sizeof(Sender.sin_zero));
        SocketHelper::LengthType SenderSize = sizeof(Sender);

        // Receive a chunk of bytes
        int Received = recvfrom(mySocket, Data, static_cast<int>(MaxSize), 0, reinterpret_cast<sockaddr*>(&Sender), &SenderSize);

        // Check the number of bytes received
        if (Received > 0)
        {
            Address = IPAddress(inet_ntoa(Sender.sin_addr));
            Port = ntohs(Sender.sin_port);
            SizeReceived = static_cast<std::size_t>(Received);
            return Socket::Done;
        }
        else
        {
            Address = IPAddress();
            Port = 0;
            return Received == 0 ? Socket::Disconnected : SocketHelper::GetErrorStatus();
        }
    }
    else
    {
        // Error...
        std::cerr << "Cannot receive data from the network (invalid parameters)" << std::endl;
        return Socket::Error;
    }
}


////////////////////////////////////////////////////////////
/// Send a packet of data
////////////////////////////////////////////////////////////
Socket::Status SocketUDP::Send(Packet& PacketToSend, const IPAddress& Address, unsigned short Port)
{
    // Get the data to send from the packet
    std::size_t DataSize = 0;
    const char* Data = PacketToSend.OnSend(DataSize);

    // Send the packet size
    Uint32 PacketSize = htonl(static_cast<unsigned long>(DataSize));
    Send(reinterpret_cast<const char*>(&PacketSize), sizeof(PacketSize), Address, Port);

    // Send the packet data
    if (PacketSize > 0)
    {
        return Send(Data, DataSize, Address, Port);
    }
    else
    {
        return Socket::Done;
    }
}


////////////////////////////////////////////////////////////
/// Receive a packet.
/// This function will block if the socket is blocking
////////////////////////////////////////////////////////////
Socket::Status SocketUDP::Receive(Packet& PacketToReceive, IPAddress& Address, unsigned short& Port)
{
    // We start by getting the size of the incoming packet
    Uint32      PacketSize = 0;
    std::size_t Received   = 0;
    if (myPendingPacketSize < 0)
    {
        // Loop until we've received the entire size of the packet
        // (even a 4 bytes variable may be received in more than one call)
        while (myPendingHeaderSize < sizeof(myPendingHeader))
        {
            char* Data = reinterpret_cast<char*>(&myPendingHeader) + myPendingHeaderSize;
            Socket::Status Status = Receive(Data, sizeof(myPendingHeader) - myPendingHeaderSize, Received, Address, Port);
            myPendingHeaderSize += Received;

            if (Status != Socket::Done)
                return Status;
        }

        PacketSize = ntohl(myPendingHeader);
        myPendingHeaderSize = 0;
    }
    else
    {
        // There is a pending packet : we already know its size
        PacketSize = myPendingPacketSize;
    }

    // Use another address instance for receiving the packet data ;
    // chunks of data coming from a different sender will be discarded (and lost...)
    IPAddress Sender;
    unsigned short SenderPort;

    // Then loop until we receive all the packet data
    char Buffer[1024];
    while (myPendingPacket.size() < PacketSize)
    {
        // Receive a chunk of data
        std::size_t SizeToGet = std::min(static_cast<std::size_t>(PacketSize - myPendingPacket.size()), sizeof(Buffer));
        Socket::Status Status = Receive(Buffer, SizeToGet, Received, Sender, SenderPort);
        if (Status != Socket::Done)
        {
            // We must save the size of the pending packet until we can receive its content
            if (Status == Socket::NotReady)
                myPendingPacketSize = PacketSize;
            return Status;
        }

        // Append it into the packet
        if ((Sender == Address) && (SenderPort == Port) && (Received > 0))
        {
            myPendingPacket.resize(myPendingPacket.size() + Received);
            char* Begin = &myPendingPacket[0] + myPendingPacket.size() - Received;
            memcpy(Begin, Buffer, Received);
        }
    }

    // We have received all the datas : we can copy it to the user packet, and clear our internal packet
    PacketToReceive.Clear();
    if (!myPendingPacket.empty())
        PacketToReceive.OnReceive(&myPendingPacket[0], myPendingPacket.size());
    myPendingPacket.clear();
    myPendingPacketSize = -1;

    return Socket::Done;
}


////////////////////////////////////////////////////////////
/// Close the socket
////////////////////////////////////////////////////////////
bool SocketUDP::Close()
{
    if (IsValid())
    {
        if (!SocketHelper::Close(mySocket))
        {
            std::cerr << "Failed to close socket" << std::endl;
            return false;
        }

        mySocket = SocketHelper::InvalidSocket();
    }

    myPort       = 0;
    myIsBlocking = true;

    return true;
}


////////////////////////////////////////////////////////////
/// Check if the socket is in a valid state ; this function
/// can be called any time to check if the socket is OK
////////////////////////////////////////////////////////////
bool SocketUDP::IsValid() const
{
    return mySocket != SocketHelper::InvalidSocket();
}


////////////////////////////////////////////////////////////
/// Get the port the socket is currently bound to
////////////////////////////////////////////////////////////
unsigned short SocketUDP::GetPort() const
{
    return myPort;
}


////////////////////////////////////////////////////////////
/// Comparison operator ==
////////////////////////////////////////////////////////////
bool SocketUDP::operator ==(const SocketUDP& Other) const
{
    return mySocket == Other.mySocket;
}


////////////////////////////////////////////////////////////
/// Comparison operator !=
////////////////////////////////////////////////////////////
bool SocketUDP::operator !=(const SocketUDP& Other) const
{
    return mySocket != Other.mySocket;
}


////////////////////////////////////////////////////////////
/// Comparison operator <.
/// Provided for compatibility with standard containers, as
/// comparing two sockets doesn't make much sense...
////////////////////////////////////////////////////////////
bool SocketUDP::operator <(const SocketUDP& Other) const
{
    return mySocket < Other.mySocket;
}


////////////////////////////////////////////////////////////
/// Construct the socket from a socket descriptor
/// (for internal use only)
////////////////////////////////////////////////////////////
SocketUDP::SocketUDP(SocketHelper::SocketType Descriptor)
{
    Create(Descriptor);
}


////////////////////////////////////////////////////////////
/// Create the socket
////////////////////////////////////////////////////////////
void SocketUDP::Create(SocketHelper::SocketType Descriptor)
{
    // Use the given socket descriptor, or get a new one
    mySocket = Descriptor ? Descriptor : socket(PF_INET, SOCK_DGRAM, 0);
    myIsBlocking = true;

    // Clear the last port used
    myPort = 0;

    // Reset the pending packet
    myPendingHeaderSize = 0;
    myPendingPacket.clear();
    myPendingPacketSize = -1;

    // Setup default options
    if (IsValid())
    {
        // To avoid the "Address already in use" error message when trying to bind to the same port
        int Yes = 1;
        if (setsockopt(mySocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&Yes), sizeof(Yes)) == -1)
        {
            std::cerr << "Failed to set socket option \"reuse address\" ; "
                      << "binding to a same port may fail if too fast" << std::endl;
        }

        // Enable broadcast by default
        if (setsockopt(mySocket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char*>(&Yes), sizeof(Yes)) == -1)
        {
            std::cerr << "Failed to enable broadcast on UDP socket" << std::endl;
        }

        // Set blocking by default (should always be the case anyway)
        SetBlocking(true);
    }
}

} // namespace sf
