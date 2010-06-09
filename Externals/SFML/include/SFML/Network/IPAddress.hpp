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

#ifndef SFML_IPADDRESS_HPP
#define SFML_IPADDRESS_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Config.hpp>
#include <istream>
#include <ostream>
#include <string>


namespace sf
{
////////////////////////////////////////////////////////////
/// IPAddress provides easy manipulation of IP v4 addresses
////////////////////////////////////////////////////////////
class SFML_API IPAddress
{
public :

    ////////////////////////////////////////////////////////////
    /// Default constructor -- constructs an invalid address
    ///
    ////////////////////////////////////////////////////////////
    IPAddress();

    ////////////////////////////////////////////////////////////
    /// Construct the address from a string
    ///
    /// \param Address : IP address ("xxx.xxx.xxx.xxx") or network name
    ///
    ////////////////////////////////////////////////////////////
    IPAddress(const std::string& Address);

    ////////////////////////////////////////////////////////////
    /// Construct the address from a C-style string ;
    /// Needed for implicit conversions from literal strings to IPAddress to work
    ///
    /// \param Address : IP address ("xxx.xxx.xxx.xxx") or network name
    ///
    ////////////////////////////////////////////////////////////
    IPAddress(const char* Address);

    ////////////////////////////////////////////////////////////
    /// Construct the address from 4 bytes
    ///
    /// \param Byte0 : First byte of the address
    /// \param Byte1 : Second byte of the address
    /// \param Byte2 : Third byte of the address
    /// \param Byte3 : Fourth byte of the address
    ///
    ////////////////////////////////////////////////////////////
    IPAddress(Uint8 Byte0, Uint8 Byte1, Uint8 Byte2, Uint8 Byte3);

    ////////////////////////////////////////////////////////////
    /// Construct the address from a 32-bits integer
    ///
    /// \param Address : 4 bytes of the address packed into a 32-bits integer
    ///
    ////////////////////////////////////////////////////////////
    IPAddress(Uint32 Address);

    ////////////////////////////////////////////////////////////
    /// Tell if the address is a valid one
    ///
    /// \return True if address has a valid syntax
    ///
    ////////////////////////////////////////////////////////////
    bool IsValid() const;

    ////////////////////////////////////////////////////////////
    /// Get a string representation of the address
    ///
    /// \return String representation of the IP address ("xxx.xxx.xxx.xxx")
    ///
    ////////////////////////////////////////////////////////////
    std::string ToString() const;

    ////////////////////////////////////////////////////////////
    /// Get an integer representation of the address
    ///
    /// \return 32-bits integer containing the 4 bytes of the address, in system endianness
    ///
    ////////////////////////////////////////////////////////////
    Uint32 ToInteger() const;

    ////////////////////////////////////////////////////////////
    /// Get the computer's local IP address (from the LAN point of view)
    ///
    /// \return Local IP address
    ///
    ////////////////////////////////////////////////////////////
    static IPAddress GetLocalAddress();

    ////////////////////////////////////////////////////////////
    /// Get the computer's public IP address (from the web point of view).
    /// The only way to get a public address is to ask it to a
    /// distant website ; as a consequence, this function may be
    /// very slow -- use it as few as possible !
    ///
    /// \param Timeout : Maximum time to wait, in seconds (0 by default : no timeout)
    ///
    /// \return Public IP address
    ///
    ////////////////////////////////////////////////////////////
    static IPAddress GetPublicAddress(float Timeout = 0.f);

    ////////////////////////////////////////////////////////////
    /// Comparison operator ==
    ///
    /// \param Other : Address to compare
    ///
    /// \return True if *this == Other
    ///
    ////////////////////////////////////////////////////////////
    bool operator ==(const IPAddress& Other) const;

    ////////////////////////////////////////////////////////////
    /// Comparison operator !=
    ///
    /// \param Other : Address to compare
    ///
    /// \return True if *this != Other
    ///
    ////////////////////////////////////////////////////////////
    bool operator !=(const IPAddress& Other) const;

    ////////////////////////////////////////////////////////////
    /// Comparison operator <
    ///
    /// \param Other : Address to compare
    ///
    /// \return True if *this < Other
    ///
    ////////////////////////////////////////////////////////////
    bool operator <(const IPAddress& Other) const;

    ////////////////////////////////////////////////////////////
    /// Comparison operator >
    ///
    /// \param Other : Address to compare
    ///
    /// \return True if *this > Other
    ///
    ////////////////////////////////////////////////////////////
    bool operator >(const IPAddress& Other) const;

    ////////////////////////////////////////////////////////////
    /// Comparison operator <=
    ///
    /// \param Other : Address to compare
    ///
    /// \return True if *this <= Other
    ///
    ////////////////////////////////////////////////////////////
    bool operator <=(const IPAddress& Other) const;

    ////////////////////////////////////////////////////////////
    /// Comparison operator >=
    ///
    /// \param Other : Address to compare
    ///
    /// \return True if *this >= Other
    ///
    ////////////////////////////////////////////////////////////
    bool operator >=(const IPAddress& Other) const;

    ////////////////////////////////////////////////////////////
    // Static member data
    ////////////////////////////////////////////////////////////
    static const IPAddress LocalHost; ///< Local host address (to connect to the same computer)

private :

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    Uint32 myAddress; ///< Address stored as an unsigned 32 bits integer
};

////////////////////////////////////////////////////////////
/// Operator >> overload to extract an address from an input stream
///
/// \param Stream :  Input stream
/// \param Address : Address to extract
///
/// \return Reference to the input stream
///
////////////////////////////////////////////////////////////
SFML_API std::istream& operator >>(std::istream& Stream, IPAddress& Address);

////////////////////////////////////////////////////////////
/// Operator << overload to print an address to an output stream
///
/// \param Stream :  Output stream
/// \param Address : Address to print
///
/// \return Reference to the output stream
///
////////////////////////////////////////////////////////////
SFML_API std::ostream& operator <<(std::ostream& Stream, const IPAddress& Address);

} // namespace sf


#endif // SFML_IPADDRESS_HPP
