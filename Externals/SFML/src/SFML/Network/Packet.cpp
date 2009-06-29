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
#include <SFML/Network/Packet.hpp>
#include <SFML/Network/SocketHelper.hpp>
#include <string.h>


namespace sf
{
////////////////////////////////////////////////////////////
/// Default constructor
////////////////////////////////////////////////////////////
Packet::Packet() :
myReadPos(0),
myIsValid(true)
{

}


////////////////////////////////////////////////////////////
/// Virtual destructor
////////////////////////////////////////////////////////////
Packet::~Packet()
{

}


////////////////////////////////////////////////////////////
/// Append data to the end of the packet
////////////////////////////////////////////////////////////
void Packet::Append(const void* Data, std::size_t SizeInBytes)
{
    if (Data && (SizeInBytes > 0))
    {
        std::size_t Start = myData.size();
        myData.resize(Start + SizeInBytes);
        memcpy(&myData[Start], Data, SizeInBytes);
    }
}


////////////////////////////////////////////////////////////
/// Clear the packet data
////////////////////////////////////////////////////////////
void Packet::Clear()
{
    myData.clear();
    myReadPos = 0;
    myIsValid = true;
}


////////////////////////////////////////////////////////////
/// Get a pointer to the data contained in the packet
/// Warning : the returned pointer may be invalid after you
/// append data to the packet
////////////////////////////////////////////////////////////
const char* Packet::GetData() const
{
    return !myData.empty() ? &myData[0] : NULL;
}


////////////////////////////////////////////////////////////
/// Get the size of the data contained in the packet
////////////////////////////////////////////////////////////
std::size_t Packet::GetDataSize() const
{
    return myData.size();
}


////////////////////////////////////////////////////////////
/// Tell if the reading position has reached the end of the packet
////////////////////////////////////////////////////////////
bool Packet::EndOfPacket() const
{
    return myReadPos >= myData.size();
}


////////////////////////////////////////////////////////////
/// Tell if the packet is valid for reading
////////////////////////////////////////////////////////////
Packet::operator bool() const
{
    return myIsValid;
}


////////////////////////////////////////////////////////////
/// Operator >> overloads to extract data from the packet
////////////////////////////////////////////////////////////
Packet& Packet::operator >>(bool& Data)
{
    Uint8 Value;
    if (*this >> Value)
        Data = (Value != 0);

    return *this;
}
Packet& Packet::operator >>(Int8& Data)
{
    if (CheckSize(sizeof(Data)))
    {
        Data = *reinterpret_cast<const Int8*>(GetData() + myReadPos);
        myReadPos += sizeof(Data);
    }

    return *this;
}
Packet& Packet::operator >>(Uint8& Data)
{
    if (CheckSize(sizeof(Data)))
    {
        Data = *reinterpret_cast<const Uint8*>(GetData() + myReadPos);
        myReadPos += sizeof(Data);
    }

    return *this;
}
Packet& Packet::operator >>(Int16& Data)
{
    if (CheckSize(sizeof(Data)))
    {
        Data = ntohs(*reinterpret_cast<const Int16*>(GetData() + myReadPos));
        myReadPos += sizeof(Data);
    }

    return *this;
}
Packet& Packet::operator >>(Uint16& Data)
{
    if (CheckSize(sizeof(Data)))
    {
        Data = ntohs(*reinterpret_cast<const Uint16*>(GetData() + myReadPos));
        myReadPos += sizeof(Data);
    }

    return *this;
}
Packet& Packet::operator >>(Int32& Data)
{
    if (CheckSize(sizeof(Data)))
    {
        Data = ntohl(*reinterpret_cast<const Int32*>(GetData() + myReadPos));
        myReadPos += sizeof(Data);
    }

    return *this;
}
Packet& Packet::operator >>(Uint32& Data)
{
    if (CheckSize(sizeof(Data)))
    {
        Data = ntohl(*reinterpret_cast<const Uint32*>(GetData() + myReadPos));
        myReadPos += sizeof(Data);
    }

    return *this;
}
Packet& Packet::operator >>(float& Data)
{
    if (CheckSize(sizeof(Data)))
    {
        Data = *reinterpret_cast<const float*>(GetData() + myReadPos);
        myReadPos += sizeof(Data);
    }

    return *this;
}
Packet& Packet::operator >>(double& Data)
{
    if (CheckSize(sizeof(Data)))
    {
        Data = *reinterpret_cast<const double*>(GetData() + myReadPos);
        myReadPos += sizeof(Data);
    }

    return *this;
}
Packet& Packet::operator >>(char* Data)
{
    // First extract string length
    Uint32 Length;
    *this >> Length;

    if ((Length > 0) && CheckSize(Length))
    {
        // Then extract characters
        memcpy(Data, GetData() + myReadPos, Length);
        Data[Length] = '\0';

        // Update reading position
        myReadPos += Length;
    }

    return *this;
}
Packet& Packet::operator >>(std::string& Data)
{
    // First extract string length
    Uint32 Length;
    *this >> Length;

    Data.clear();
    if ((Length > 0) && CheckSize(Length))
    {
        // Then extract characters
        Data.assign(GetData() + myReadPos, Length);

        // Update reading position
        myReadPos += Length;
    }

    return *this;
}
Packet& Packet::operator >>(wchar_t* Data)
{
    // First extract string length
    Uint32 Length;
    *this >> Length;

    if ((Length > 0) && CheckSize(Length * sizeof(Int32)))
    {
        // Then extract characters
        for (Uint32 i = 0; i < Length; ++i)
        {
            Uint32 c;
            *this >> c;
            Data[i] = static_cast<wchar_t>(c);
        }
        Data[Length] = L'\0';
    }

    return *this;
}
Packet& Packet::operator >>(std::wstring& Data)
{
    // First extract string length
    Uint32 Length;
    *this >> Length;

    Data.clear();
    if ((Length > 0) && CheckSize(Length * sizeof(Int32)))
    {
        // Then extract characters
        for (Uint32 i = 0; i < Length; ++i)
        {
            Uint32 c;
            *this >> c;
            Data += static_cast<wchar_t>(c);
        }
    }

    return *this;
}


////////////////////////////////////////////////////////////
/// Operator << overloads to put data into the packet
////////////////////////////////////////////////////////////
Packet& Packet::operator <<(bool Data)
{
    *this << static_cast<Uint8>(Data);
    return *this;
}
Packet& Packet::operator <<(Int8 Data)
{
    Append(&Data, sizeof(Data));
    return *this;
}
Packet& Packet::operator <<(Uint8 Data)
{
    Append(&Data, sizeof(Data));
    return *this;
}
Packet& Packet::operator <<(Int16 Data)
{
    Int16 ToWrite = htons(Data);
    Append(&ToWrite, sizeof(ToWrite));
    return *this;
}
Packet& Packet::operator <<(Uint16 Data)
{
    Uint16 ToWrite = htons(Data);
    Append(&ToWrite, sizeof(ToWrite));
    return *this;
}
Packet& Packet::operator <<(Int32 Data)
{
    Int32 ToWrite = htonl(Data);
    Append(&ToWrite, sizeof(ToWrite));
    return *this;
}
Packet& Packet::operator <<(Uint32 Data)
{
    Uint32 ToWrite = htonl(Data);
    Append(&ToWrite, sizeof(ToWrite));
    return *this;
}
Packet& Packet::operator <<(float Data)
{
    Append(&Data, sizeof(Data));
    return *this;
}
Packet& Packet::operator <<(double Data)
{
    Append(&Data, sizeof(Data));
    return *this;
}
Packet& Packet::operator <<(const char* Data)
{
    // First insert string length
    Uint32 Length = 0;
    for (const char* c = Data; *c != '\0'; ++c)
        ++Length;
    *this << Length;

    // Then insert characters
    Append(Data, Length * sizeof(char));

    return *this;
}
Packet& Packet::operator <<(const std::string& Data)
{
    // First insert string length
    Uint32 Length = static_cast<Uint32>(Data.size());
    *this << Length;

    // Then insert characters
    if (Length > 0)
    {
        Append(Data.c_str(), Length * sizeof(std::string::value_type));
    }

    return *this;
}
Packet& Packet::operator <<(const wchar_t* Data)
{
    // First insert string length
    Uint32 Length = 0;
    for (const wchar_t* c = Data; *c != L'\0'; ++c)
        ++Length;
    *this << Length;

    // Then insert characters
    for (const wchar_t* c = Data; *c != L'\0'; ++c)
        *this << static_cast<Int32>(*c);

    return *this;
}
Packet& Packet::operator <<(const std::wstring& Data)
{
    // First insert string length
    Uint32 Length = static_cast<Uint32>(Data.size());
    *this << Length;

    // Then insert characters
    if (Length > 0)
    {
        for (std::wstring::const_iterator c = Data.begin(); c != Data.end(); ++c)
            *this << static_cast<Int32>(*c);
    }

    return *this;
}


////////////////////////////////////////////////////////////
/// Check if the packet can extract a given size of bytes
////////////////////////////////////////////////////////////
bool Packet::CheckSize(std::size_t Size)
{
    myIsValid = myIsValid && (myReadPos + Size <= myData.size());

    return myIsValid;
}


////////////////////////////////////////////////////////////
/// Called before the packet is sent to the network
////////////////////////////////////////////////////////////
const char* Packet::OnSend(std::size_t& DataSize)
{
    DataSize = GetDataSize();
    return GetData();
}


////////////////////////////////////////////////////////////
/// Called after the packet has been received from the network
////////////////////////////////////////////////////////////
void Packet::OnReceive(const char* Data, std::size_t DataSize)
{
    Append(Data, DataSize);
}

} // namespace sf
