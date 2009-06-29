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
#include <SFML/Network/IPAddress.hpp>
#include <SFML/Network/Http.hpp>
#include <SFML/Network/SocketHelper.hpp>
#include <string.h>


namespace sf
{
////////////////////////////////////////////////////////////
/// Static member data
////////////////////////////////////////////////////////////
const IPAddress IPAddress::LocalHost("127.0.0.1");


////////////////////////////////////////////////////////////
/// Default constructor
////////////////////////////////////////////////////////////
IPAddress::IPAddress() :
myAddress(INADDR_NONE)
{

}


////////////////////////////////////////////////////////////
/// Construct the address from a string
////////////////////////////////////////////////////////////
IPAddress::IPAddress(const std::string& Address)
{
    // First try to convert it as a byte representation ("xxx.xxx.xxx.xxx")
    myAddress = inet_addr(Address.c_str());

    // If not successful, try to convert it as a host name
    if (!IsValid())
    {
        hostent* Host = gethostbyname(Address.c_str());
        if (Host)
        {
            // Host found, extract its IP address
            myAddress = reinterpret_cast<in_addr*>(Host->h_addr)->s_addr;
        }
        else
        {
            // Host name not found on the network
            myAddress = INADDR_NONE;
        }
    }
}


////////////////////////////////////////////////////////////
/// Construct the address from a C-style string ;
/// Needed for implicit conversions from literal strings to IPAddress to work
////////////////////////////////////////////////////////////
IPAddress::IPAddress(const char* Address)
{
    // First try to convert it as a byte representation ("xxx.xxx.xxx.xxx")
    myAddress = inet_addr(Address);

    // If not successful, try to convert it as a host name
    if (!IsValid())
    {
        hostent* Host = gethostbyname(Address);
        if (Host)
        {
            // Host found, extract its IP address
            myAddress = reinterpret_cast<in_addr*>(Host->h_addr)->s_addr;
        }
        else
        {
            // Host name not found on the network
            myAddress = INADDR_NONE;
        }
    }
}


////////////////////////////////////////////////////////////
/// Construct the address from 4 bytes
////////////////////////////////////////////////////////////
IPAddress::IPAddress(Uint8 Byte0, Uint8 Byte1, Uint8 Byte2, Uint8 Byte3)
{
    myAddress = htonl((Byte0 << 24) | (Byte1 << 16) | (Byte2 << 8) | Byte3);
}


////////////////////////////////////////////////////////////
/// Construct the address from a 32-bits integer
////////////////////////////////////////////////////////////
IPAddress::IPAddress(Uint32 Address)
{
    myAddress = htonl(Address);
}


////////////////////////////////////////////////////////////
/// Tell if the address is a valid one
////////////////////////////////////////////////////////////
bool IPAddress::IsValid() const
{
    return myAddress != INADDR_NONE;
}


////////////////////////////////////////////////////////////
/// Get a string representation of the address
////////////////////////////////////////////////////////////
std::string IPAddress::ToString() const
{
    in_addr InAddr;
    InAddr.s_addr = myAddress;

    return inet_ntoa(InAddr);
}


////////////////////////////////////////////////////////////
/// Get an integer representation of the address
////////////////////////////////////////////////////////////
Uint32 IPAddress::ToInteger() const
{
    return ntohl(myAddress);
}


////////////////////////////////////////////////////////////
/// Get the computer's local IP address (from the LAN point of view)
////////////////////////////////////////////////////////////
IPAddress IPAddress::GetLocalAddress()
{
    // The method here is to connect a UDP socket to anyone (here to localhost),
    // and get the local socket address with the getsockname function.
    // UDP connection will not send anything to the network, so this function won't cause any overhead.

    IPAddress LocalAddress;

    // Create the socket
    SocketHelper::SocketType Socket = socket(PF_INET, SOCK_DGRAM, 0);
    if (Socket == SocketHelper::InvalidSocket())
        return LocalAddress;

    // Build the host address (use a random port)
    sockaddr_in SockAddr;
    memset(SockAddr.sin_zero, 0, sizeof(SockAddr.sin_zero));
    SockAddr.sin_addr.s_addr = INADDR_LOOPBACK;
    SockAddr.sin_family      = AF_INET;
    SockAddr.sin_port        = htons(4567);

    // Connect the socket
    if (connect(Socket, reinterpret_cast<sockaddr*>(&SockAddr), sizeof(SockAddr)) == -1)
    {
        SocketHelper::Close(Socket);
        return LocalAddress;
    }
 
    // Get the local address of the socket connection
    SocketHelper::LengthType Size = sizeof(SockAddr);
    if (getsockname(Socket, reinterpret_cast<sockaddr*>(&SockAddr), &Size) == -1)
    {
        SocketHelper::Close(Socket);
        return LocalAddress;
    }

    // Close the socket
    SocketHelper::Close(Socket);

    // Finally build the IP address
    LocalAddress.myAddress = SockAddr.sin_addr.s_addr;

    return LocalAddress;
}


////////////////////////////////////////////////////////////
/// Get the computer's public IP address (from the web point of view)
////////////////////////////////////////////////////////////
IPAddress IPAddress::GetPublicAddress(float Timeout)
{
    // The trick here is more complicated, because the only way
    // to get our public IP address is to get it from a distant computer.
    // Here we get the web page from http://www.sfml-dev.org/ip-provider.php
    // and parse the result to extract our IP address
    // (not very hard : the web page contains only our IP address).

    Http Server("www.sfml-dev.org");
    Http::Request Request(Http::Request::Get, "/ip-provider.php");
    Http::Response Page = Server.SendRequest(Request, Timeout);
    if (Page.GetStatus() == Http::Response::Ok)
        return IPAddress(Page.GetBody());

    // Something failed: return an invalid address
    return IPAddress();
}


////////////////////////////////////////////////////////////
/// Comparison operator ==
////////////////////////////////////////////////////////////
bool IPAddress::operator ==(const IPAddress& Other) const
{
    return myAddress == Other.myAddress;
}


////////////////////////////////////////////////////////////
/// Comparison operator !=
////////////////////////////////////////////////////////////
bool IPAddress::operator !=(const IPAddress& Other) const
{
    return myAddress != Other.myAddress;
}


////////////////////////////////////////////////////////////
/// Comparison operator <
////////////////////////////////////////////////////////////
bool IPAddress::operator <(const IPAddress& Other) const
{
    return myAddress < Other.myAddress;
}


////////////////////////////////////////////////////////////
/// Comparison operator >
////////////////////////////////////////////////////////////
bool IPAddress::operator >(const IPAddress& Other) const
{
    return myAddress > Other.myAddress;
}


////////////////////////////////////////////////////////////
/// Comparison operator <=
////////////////////////////////////////////////////////////
bool IPAddress::operator <=(const IPAddress& Other) const
{
    return myAddress <= Other.myAddress;
}


////////////////////////////////////////////////////////////
/// Comparison operator >=
////////////////////////////////////////////////////////////
bool IPAddress::operator >=(const IPAddress& Other) const
{
    return myAddress >= Other.myAddress;
}


////////////////////////////////////////////////////////////
/// Operator >> overload to extract an address from an input stream
////////////////////////////////////////////////////////////
std::istream& operator >>(std::istream& Stream, IPAddress& Address)
{
    std::string Str;
    Stream >> Str;
    Address = IPAddress(Str);

    return Stream;
}


////////////////////////////////////////////////////////////
/// Operator << overload to print an address to an output stream
////////////////////////////////////////////////////////////
std::ostream& operator <<(std::ostream& Stream, const IPAddress& Address)
{
    return Stream << Address.ToString();
}

} // namespace sf
