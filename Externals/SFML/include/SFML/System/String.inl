////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Laurent Gomila (laurent@sfml-dev.org)
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
template <typename T>
String String::fromUtf8(T begin, T end)
{
    String string;
    Utf8::toUtf32(begin, end, std::back_inserter(string.m_string));
    return string;
}


////////////////////////////////////////////////////////////
template <typename T>
String String::fromUtf16(T begin, T end)
{
    String string;
    Utf16::toUtf32(begin, end, std::back_inserter(string.m_string));
    return string;
}


////////////////////////////////////////////////////////////
template <typename T>
String String::fromUtf32(T begin, T end)
{
    String string;
    string.m_string.assign(begin, end);
    return string;
}
