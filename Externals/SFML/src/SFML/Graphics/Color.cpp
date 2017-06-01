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
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/Color.hpp>
#include <algorithm>


namespace sf
{
////////////////////////////////////////////////////////////
// Static member data
////////////////////////////////////////////////////////////
const Color Color::Black(0, 0, 0);
const Color Color::White(255, 255, 255);
const Color Color::Red(255, 0, 0);
const Color Color::Green(0, 255, 0);
const Color Color::Blue(0, 0, 255);
const Color Color::Yellow(255, 255, 0);
const Color Color::Magenta(255, 0, 255);
const Color Color::Cyan(0, 255, 255);
const Color Color::Transparent(0, 0, 0, 0);


////////////////////////////////////////////////////////////
Color::Color() :
r(0),
g(0),
b(0),
a(255)
{

}


////////////////////////////////////////////////////////////
Color::Color(Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha) :
r(red),
g(green),
b(blue),
a(alpha)
{

}


////////////////////////////////////////////////////////////
Color::Color(Uint32 color) :
r((color & 0xff000000) >> 24),
g((color & 0x00ff0000) >> 16),
b((color & 0x0000ff00) >> 8 ),
a((color & 0x000000ff) >> 0 )
{

}


////////////////////////////////////////////////////////////
Uint32 Color::toInteger() const
{
    return (r << 24) | (g << 16) | (b << 8) | a;
}


////////////////////////////////////////////////////////////
bool operator ==(const Color& left, const Color& right)
{
    return (left.r == right.r) &&
           (left.g == right.g) &&
           (left.b == right.b) &&
           (left.a == right.a);
}


////////////////////////////////////////////////////////////
bool operator !=(const Color& left, const Color& right)
{
    return !(left == right);
}


////////////////////////////////////////////////////////////
Color operator +(const Color& left, const Color& right)
{
    return Color(Uint8(std::min(int(left.r) + right.r, 255)),
                 Uint8(std::min(int(left.g) + right.g, 255)),
                 Uint8(std::min(int(left.b) + right.b, 255)),
                 Uint8(std::min(int(left.a) + right.a, 255)));
}


////////////////////////////////////////////////////////////
Color operator -(const Color& left, const Color& right)
{
    return Color(Uint8(std::max(int(left.r) - right.r, 0)),
                 Uint8(std::max(int(left.g) - right.g, 0)),
                 Uint8(std::max(int(left.b) - right.b, 0)),
                 Uint8(std::max(int(left.a) - right.a, 0)));
}


////////////////////////////////////////////////////////////
Color operator *(const Color& left, const Color& right)
{
    return Color(Uint8(int(left.r) * right.r / 255),
                 Uint8(int(left.g) * right.g / 255),
                 Uint8(int(left.b) * right.b / 255),
                 Uint8(int(left.a) * right.a / 255));
}


////////////////////////////////////////////////////////////
Color& operator +=(Color& left, const Color& right)
{
    return left = left + right;
}


////////////////////////////////////////////////////////////
Color& operator -=(Color& left, const Color& right)
{
    return left = left - right;
}


////////////////////////////////////////////////////////////
Color& operator *=(Color& left, const Color& right)
{
    return left = left * right;
}

} // namespace sf
