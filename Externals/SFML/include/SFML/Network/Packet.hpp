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

#ifndef SFML_PACKET_HPP
#define SFML_PACKET_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Config.hpp>
#include <string>
#include <vector>


namespace sf
{
////////////////////////////////////////////////////////////
/// Packet wraps data to send / to receive through the network
////////////////////////////////////////////////////////////
class SFML_API Packet
{
public :

    ////////////////////////////////////////////////////////////
    /// Default constructor
    ///
    ////////////////////////////////////////////////////////////
    Packet();

    ////////////////////////////////////////////////////////////
    /// Virtual destructor
    ///
    ////////////////////////////////////////////////////////////
    virtual ~Packet();

    ////////////////////////////////////////////////////////////
    /// Append data to the end of the packet
    ///
    /// \param Data :        Pointer to the bytes to append
    /// \param SizeInBytes : Number of bytes to append
    ///
    ////////////////////////////////////////////////////////////
    void Append(const void* Data, std::size_t SizeInBytes);

    ////////////////////////////////////////////////////////////
    /// Clear the packet data
    ///
    ////////////////////////////////////////////////////////////
    void Clear();

    ////////////////////////////////////////////////////////////
    /// Get a pointer to the data contained in the packet
    /// Warning : the returned pointer may be invalid after you
    /// append data to the packet
    ///
    /// \return Pointer to the data
    ///
    ////////////////////////////////////////////////////////////
    const char* GetData() const;

    ////////////////////////////////////////////////////////////
    /// Get the size of the data contained in the packet
    ///
    /// \return Data size, in bytes
    ///
    ////////////////////////////////////////////////////////////
    std::size_t GetDataSize() const;

    ////////////////////////////////////////////////////////////
    /// Tell if the reading position has reached the end of the packet
    ///
    /// \return True if all data have been read into the packet
    ///
    ////////////////////////////////////////////////////////////
    bool EndOfPacket() const;

    ////////////////////////////////////////////////////////////
    /// Return the validity of packet
    ///
    /// \return True if last data extraction from packet was successful
    ///
    ////////////////////////////////////////////////////////////
    operator bool() const;

    ////////////////////////////////////////////////////////////
    /// Operator >> overloads to extract data from the packet
    ///
    ////////////////////////////////////////////////////////////
    Packet& operator >>(bool&         Data);
    Packet& operator >>(Int8&         Data);
    Packet& operator >>(Uint8&        Data);
    Packet& operator >>(Int16&        Data);
    Packet& operator >>(Uint16&       Data);
    Packet& operator >>(Int32&        Data);
    Packet& operator >>(Uint32&       Data);
    Packet& operator >>(float&        Data);
    Packet& operator >>(double&       Data);
    Packet& operator >>(char*         Data);
    Packet& operator >>(std::string&  Data);
    Packet& operator >>(wchar_t*      Data);
    Packet& operator >>(std::wstring& Data);

    ////////////////////////////////////////////////////////////
    /// Operator << overloads to put data into the packet
    ///
    ////////////////////////////////////////////////////////////
    Packet& operator <<(bool                Data);
    Packet& operator <<(Int8                Data);
    Packet& operator <<(Uint8               Data);
    Packet& operator <<(Int16               Data);
    Packet& operator <<(Uint16              Data);
    Packet& operator <<(Int32               Data);
    Packet& operator <<(Uint32              Data);
    Packet& operator <<(float               Data);
    Packet& operator <<(double              Data);
    Packet& operator <<(const char*         Data);
    Packet& operator <<(const std::string&  Data);
    Packet& operator <<(const wchar_t*      Data);
    Packet& operator <<(const std::wstring& Data);

private :

    friend class SocketTCP;
    friend class SocketUDP;

    ////////////////////////////////////////////////////////////
    /// Check if the packet can extract a given size of bytes
    ///
    /// \param Size : Size to check
    ///
    /// \return True if Size bytes can be read from the packet's data
    ///
    ////////////////////////////////////////////////////////////
    bool CheckSize(std::size_t Size);

    ////////////////////////////////////////////////////////////
    /// Called before the packet is sent to the network
    ///
    /// \param DataSize : Variable to fill with the size of data to send
    ///
    /// \return Pointer to the array of bytes to send
    ///
    ////////////////////////////////////////////////////////////
    virtual const char* OnSend(std::size_t& DataSize);

    ////////////////////////////////////////////////////////////
    /// Called after the packet has been received from the network
    ///
    /// \param Data :     Pointer to the array of received bytes
    /// \param DataSize : Size of the array of bytes
    ///
    ////////////////////////////////////////////////////////////
    virtual void OnReceive(const char* Data, std::size_t DataSize);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    std::vector<char> myData;    ///< Data stored in the packet
    std::size_t       myReadPos; ///< Current reading position in the packet
    bool              myIsValid; ///< Reading state of the packet
};

} // namespace sf


#endif // SFML_PACKET_HPP
