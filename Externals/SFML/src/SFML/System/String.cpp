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
#include <SFML/System/String.hpp>
#include <SFML/System/Utf.hpp>
#include <iterator>
#include <cstring>


namespace sf
{
////////////////////////////////////////////////////////////
const std::size_t String::InvalidPos = std::basic_string<Uint32>::npos;


////////////////////////////////////////////////////////////
String::String()
{
}


////////////////////////////////////////////////////////////
String::String(char ansiChar, const std::locale& locale)
{
    m_string += Utf32::decodeAnsi(ansiChar, locale);
}


////////////////////////////////////////////////////////////
String::String(wchar_t wideChar)
{
    m_string += Utf32::decodeWide(wideChar);
}


////////////////////////////////////////////////////////////
String::String(Uint32 utf32Char)
{
    m_string += utf32Char;
}


////////////////////////////////////////////////////////////
String::String(const char* ansiString, const std::locale& locale)
{
    if (ansiString)
    {
        std::size_t length = strlen(ansiString);
        if (length > 0)
        {
            m_string.reserve(length + 1);
            Utf32::fromAnsi(ansiString, ansiString + length, std::back_inserter(m_string), locale);
        }
    }
}


////////////////////////////////////////////////////////////
String::String(const std::string& ansiString, const std::locale& locale)
{
    m_string.reserve(ansiString.length() + 1);
    Utf32::fromAnsi(ansiString.begin(), ansiString.end(), std::back_inserter(m_string), locale);
}


////////////////////////////////////////////////////////////
String::String(const wchar_t* wideString)
{
    if (wideString)
    {
        std::size_t length = std::wcslen(wideString);
        if (length > 0)
        {
            m_string.reserve(length + 1);
            Utf32::fromWide(wideString, wideString + length, std::back_inserter(m_string));
        }
    }
}


////////////////////////////////////////////////////////////
String::String(const std::wstring& wideString)
{
    m_string.reserve(wideString.length() + 1);
    Utf32::fromWide(wideString.begin(), wideString.end(), std::back_inserter(m_string));
}


////////////////////////////////////////////////////////////
String::String(const Uint32* utf32String)
{
    if (utf32String)
        m_string = utf32String;
}


////////////////////////////////////////////////////////////
String::String(const std::basic_string<Uint32>& utf32String) :
m_string(utf32String)
{
}


////////////////////////////////////////////////////////////
String::String(const String& copy) :
m_string(copy.m_string)
{
}


////////////////////////////////////////////////////////////
String::operator std::string() const
{
    return toAnsiString();
}


////////////////////////////////////////////////////////////
String::operator std::wstring() const
{
    return toWideString();
}


////////////////////////////////////////////////////////////
std::string String::toAnsiString(const std::locale& locale) const
{
    // Prepare the output string
    std::string output;
    output.reserve(m_string.length() + 1);

    // Convert
    Utf32::toAnsi(m_string.begin(), m_string.end(), std::back_inserter(output), 0, locale);

    return output;
}


////////////////////////////////////////////////////////////
std::wstring String::toWideString() const
{
    // Prepare the output string
    std::wstring output;
    output.reserve(m_string.length() + 1);

    // Convert
    Utf32::toWide(m_string.begin(), m_string.end(), std::back_inserter(output), 0);

    return output;
}


////////////////////////////////////////////////////////////
String& String::operator =(const String& right)
{
    m_string = right.m_string;
    return *this;
}


////////////////////////////////////////////////////////////
String& String::operator +=(const String& right)
{
    m_string += right.m_string;
    return *this;
}


////////////////////////////////////////////////////////////
Uint32 String::operator [](std::size_t index) const
{
    return m_string[index];
}


////////////////////////////////////////////////////////////
Uint32& String::operator [](std::size_t index)
{
    return m_string[index];
}


////////////////////////////////////////////////////////////
void String::clear()
{
    m_string.clear();
}


////////////////////////////////////////////////////////////
std::size_t String::getSize() const
{
    return m_string.size();
}


////////////////////////////////////////////////////////////
bool String::isEmpty() const
{
    return m_string.empty();
}


////////////////////////////////////////////////////////////
void String::erase(std::size_t position, std::size_t count)
{
    m_string.erase(position, count);
}


////////////////////////////////////////////////////////////
void String::insert(std::size_t position, const String& str)
{
    m_string.insert(position, str.m_string);
}


////////////////////////////////////////////////////////////
std::size_t String::find(const String& str, std::size_t start) const
{
    return m_string.find(str.m_string, start);
}


////////////////////////////////////////////////////////////
const Uint32* String::getData() const
{
    return m_string.c_str();
}


////////////////////////////////////////////////////////////
String::Iterator String::begin()
{
    return m_string.begin();
}


////////////////////////////////////////////////////////////
String::ConstIterator String::begin() const
{
    return m_string.begin();
}


////////////////////////////////////////////////////////////
String::Iterator String::end()
{
    return m_string.end();
}


////////////////////////////////////////////////////////////
String::ConstIterator String::end() const
{
    return m_string.end();
}


////////////////////////////////////////////////////////////
bool operator ==(const String& left, const String& right)
{
    return left.m_string == right.m_string;
}


////////////////////////////////////////////////////////////
bool operator !=(const String& left, const String& right)
{
    return !(left == right);
}


////////////////////////////////////////////////////////////
bool operator <(const String& left, const String& right)
{
    return left.m_string < right.m_string;
}


////////////////////////////////////////////////////////////
bool operator >(const String& left, const String& right)
{
    return right < left;
}


////////////////////////////////////////////////////////////
bool operator <=(const String& left, const String& right)
{
    return !(right < left);
}


////////////////////////////////////////////////////////////
bool operator >=(const String& left, const String& right)
{
    return !(left < right);
}


////////////////////////////////////////////////////////////
String operator +(const String& left, const String& right)
{
    String string = left;
    string += right;

    return string;
}

} // namespace sf
