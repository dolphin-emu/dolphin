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

#ifndef SFML_PACKET_HPP
#define SFML_PACKET_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Network/Export.hpp>
#include <string>
#include <vector>


namespace sf
{
class String;
class TcpSocket;
class UdpSocket;

////////////////////////////////////////////////////////////
/// \brief Utility class to build blocks of data to transfer
///        over the network
///
////////////////////////////////////////////////////////////
class SFML_NETWORK_API Packet
{
    // A bool-like type that cannot be converted to integer or pointer types
    typedef bool (Packet::*BoolType)(std::size_t);

public :

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// Creates an empty packet.
    ///
    ////////////////////////////////////////////////////////////
    Packet();

    ////////////////////////////////////////////////////////////
    /// \brief Virtual destructor
    ///
    ////////////////////////////////////////////////////////////
    virtual ~Packet();

    ////////////////////////////////////////////////////////////
    /// \brief Append data to the end of the packet
    ///
    /// \param data        Pointer to the sequence of bytes to append
    /// \param sizeInBytes Number of bytes to append
    ///
    /// \see clear
    ///
    ////////////////////////////////////////////////////////////
    void append(const void* data, std::size_t sizeInBytes);

    ////////////////////////////////////////////////////////////
    /// \brief Clear the packet
    ///
    /// After calling Clear, the packet is empty.
    ///
    /// \see append
    ///
    ////////////////////////////////////////////////////////////
    void clear();

    ////////////////////////////////////////////////////////////
    /// \brief Get a pointer to the data contained in the packet
    ///
    /// Warning: the returned pointer may become invalid after
    /// you append data to the packet, therefore it should never
    /// be stored.
    /// The return pointer is NULL if the packet is empty.
    ///
    /// \return Pointer to the data
    ///
    /// \see getDataSize
    ///
    ////////////////////////////////////////////////////////////
    const void* getData() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the size of the data contained in the packet
    ///
    /// This function returns the number of bytes pointed to by
    /// what getData returns.
    ///
    /// \return Data size, in bytes
    ///
    /// \see getData
    ///
    ////////////////////////////////////////////////////////////
    std::size_t getDataSize() const;

    ////////////////////////////////////////////////////////////
    /// \brief Tell if the reading position has reached the
    ///        end of the packet
    ///
    /// This function is useful to know if there is some data
    /// left to be read, without actually reading it.
    ///
    /// \return True if all data was read, false otherwise
    ///
    /// \see operator bool
    ///
    ////////////////////////////////////////////////////////////
    bool endOfPacket() const;

public:

    ////////////////////////////////////////////////////////////
    /// \brief Test the validity of the packet, for reading
    ///
    /// This operator allows to test the packet as a boolean
    /// variable, to check if a reading operation was successful.
    ///
    /// A packet will be in an invalid state if it has no more
    /// data to read.
    ///
    /// This behaviour is the same as standard C++ streams.
    ///
    /// Usage example:
    /// \code
    /// float x;
    /// packet >> x;
    /// if (packet)
    /// {
    ///    // ok, x was extracted successfully
    /// }
    ///
    /// // -- or --
    ///
    /// float x;
    /// if (packet >> x)
    /// {
    ///    // ok, x was extracted successfully
    /// }
    /// \endcode
    ///
    /// Don't focus on the return type, it's equivalent to bool but
    /// it disallows unwanted implicit conversions to integer or
    /// pointer types.
    ///
    /// \return True if last data extraction from packet was successful
    ///
    /// \see endOfPacket
    ///
    ////////////////////////////////////////////////////////////
    operator BoolType() const;

    ////////////////////////////////////////////////////////////
    /// Overloads of operator >> to read data from the packet
    ///
    ////////////////////////////////////////////////////////////
    Packet& operator >>(bool&         data);
    Packet& operator >>(Int8&         data);
    Packet& operator >>(Uint8&        data);
    Packet& operator >>(Int16&        data);
    Packet& operator >>(Uint16&       data);
    Packet& operator >>(Int32&        data);
    Packet& operator >>(Uint32&       data);
    Packet& operator >>(float&        data);
    Packet& operator >>(double&       data);
    Packet& operator >>(char*         data);
    Packet& operator >>(std::string&  data);
    Packet& operator >>(wchar_t*      data);
    Packet& operator >>(std::wstring& data);
    Packet& operator >>(String&       data);

    ////////////////////////////////////////////////////////////
    /// Overloads of operator << to write data into the packet
    ///
    ////////////////////////////////////////////////////////////
    Packet& operator <<(bool                data);
    Packet& operator <<(Int8                data);
    Packet& operator <<(Uint8               data);
    Packet& operator <<(Int16               data);
    Packet& operator <<(Uint16              data);
    Packet& operator <<(Int32               data);
    Packet& operator <<(Uint32              data);
    Packet& operator <<(float               data);
    Packet& operator <<(double              data);
    Packet& operator <<(const char*         data);
    Packet& operator <<(const std::string&  data);
    Packet& operator <<(const wchar_t*      data);
    Packet& operator <<(const std::wstring& data);
    Packet& operator <<(const String&       data);

protected:

    friend class TcpSocket;
    friend class UdpSocket;

    ////////////////////////////////////////////////////////////
    /// \brief Called before the packet is sent over the network
    ///
    /// This function can be defined by derived classes to
    /// transform the data before it is sent; this can be
    /// used for compression, encryption, etc.
    /// The function must return a pointer to the modified data,
    /// as well as the number of bytes pointed.
    /// The default implementation provides the packet's data
    /// without transforming it.
    ///
    /// \param size Variable to fill with the size of data to send
    ///
    /// \return Pointer to the array of bytes to send
    ///
    /// \see onReceive
    ///
    ////////////////////////////////////////////////////////////
    virtual const void* onSend(std::size_t& size);

    ////////////////////////////////////////////////////////////
    /// \brief Called after the packet is received over the network
    ///
    /// This function can be defined by derived classes to
    /// transform the data after it is received; this can be
    /// used for uncompression, decryption, etc.
    /// The function receives a pointer to the received data,
    /// and must fill the packet with the transformed bytes.
    /// The default implementation fills the packet directly
    /// without transforming the data.
    ///
    /// \param data Pointer to the received bytes
    /// \param size Number of bytes
    ///
    /// \see onSend
    ///
    ////////////////////////////////////////////////////////////
    virtual void onReceive(const void* data, std::size_t size);

private :

    ////////////////////////////////////////////////////////////
    /// Disallow comparisons between packets
    ///
    ////////////////////////////////////////////////////////////
    bool operator ==(const Packet& right) const;
    bool operator !=(const Packet& right) const;

    ////////////////////////////////////////////////////////////
    /// \brief Check if the packet can extract a given number of bytes
    ///
    /// This function updates accordingly the state of the packet.
    ///
    /// \param size Size to check
    ///
    /// \return True if \a size bytes can be read from the packet
    ///
    ////////////////////////////////////////////////////////////
    bool checkSize(std::size_t size);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    std::vector<char> m_data;    ///< Data stored in the packet
    std::size_t       m_readPos; ///< Current reading position in the packet
    bool              m_isValid; ///< Reading state of the packet
};

} // namespace sf


#endif // SFML_PACKET_HPP


////////////////////////////////////////////////////////////
/// \class sf::Packet
/// \ingroup network
///
/// Packets provide a safe and easy way to serialize data,
/// in order to send it over the network using sockets
/// (sf::TcpSocket, sf::UdpSocket).
///
/// Packets solve 2 fundamental problems that arise when
/// transfering data over the network:
/// \li data is interpreted correctly according to the endianness
/// \li the bounds of the packet are preserved (one send == one receive)
///
/// The sf::Packet class provides both input and output modes.
/// It is designed to follow the behaviour of standard C++ streams,
/// using operators >> and << to extract and insert data.
///
/// It is recommended to use only fixed-size types (like sf::Int32, etc.),
/// to avoid possible differences between the sender and the receiver.
/// Indeed, the native C++ types may have different sizes on two platforms
/// and your data may be corrupted if that happens.
///
/// Usage example:
/// \code
/// sf::Uint32 x = 24;
/// std::string s = "hello";
/// double d = 5.89;
///
/// // Group the variables to send into a packet
/// sf::Packet packet;
/// packet << x << s << d;
///
/// // Send it over the network (socket is a valid sf::TcpSocket)
/// socket.send(packet);
///
/// -----------------------------------------------------------------
///
/// // Receive the packet at the other end
/// sf::Packet packet;
/// socket.receive(packet);
///
/// // Extract the variables contained in the packet
/// sf::Uint32 x;
/// std::string s;
/// double d;
/// if (packet >> x >> s >> d)
/// {
///     // Data extracted successfully...
/// }
/// \endcode
///
/// Packets have built-in operator >> and << overloads for
/// standard types:
/// \li bool
/// \li fixed-size integer types (sf::Int8/16/32, sf::Uint8/16/32)
/// \li floating point numbers (float, double)
/// \li string types (char*, wchar_t*, std::string, std::wstring, sf::String)
///
/// Like standard streams, it is also possible to define your own
/// overloads of operators >> and << in order to handle your
/// custom types.
///
/// \code
/// struct MyStruct
/// {
///     float       number;
///     sf::Int8    integer;
///     std::string str;
/// };
///
/// sf::Packet& operator <<(sf::Packet& packet, const MyStruct& m)
/// {
///     return packet << m.number << m.integer << m.str;
/// }
///
/// sf::Packet& operator >>(sf::Packet& packet, MyStruct& m)
/// {
///     return packet >> m.number >> m.integer >> m.str;
/// }
/// \endcode
///
/// Packets also provide an extra feature that allows to apply
/// custom transformations to the data before it is sent,
/// and after it is received. This is typically used to
/// handle automatic compression or encryption of the data.
/// This is achieved by inheriting from sf::Packet, and overriding
/// the onSend and onReceive functions.
///
/// Here is an example:
/// \code
/// class ZipPacket : public sf::Packet
/// {
///     virtual const void* onSend(std::size_t& size)
///     {
///         const void* srcData = getData();
///         std::size_t srcSize = getDataSize();
///
///         return MySuperZipFunction(srcData, srcSize, &size);
///     }
///
///     virtual void onReceive(const void* data, std::size_t size)
///     {
///         std::size_t dstSize;
///         const void* dstData = MySuperUnzipFunction(data, size, &dstSize);
///
///         append(dstData, dstSize);
///     }
/// };
///
/// // Use like regular packets:
/// ZipPacket packet;
/// packet << x << s << d;
/// ...
/// \endcode
///
/// \see sf::TcpSocket, sf::UdpSocket
///
////////////////////////////////////////////////////////////
