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
#include <SFML/System/Time.hpp>


namespace sf
{
////////////////////////////////////////////////////////////
const Time Time::Zero;


////////////////////////////////////////////////////////////
Time::Time() :
m_microseconds(0)
{
}


////////////////////////////////////////////////////////////
float Time::asSeconds() const
{
    return m_microseconds / 1000000.f;
}


////////////////////////////////////////////////////////////
Int32 Time::asMilliseconds() const
{
    return static_cast<Int32>(m_microseconds / 1000);
}


////////////////////////////////////////////////////////////
Int64 Time::asMicroseconds() const
{
    return m_microseconds;
}


////////////////////////////////////////////////////////////
Time::Time(Int64 microseconds) :
m_microseconds(microseconds)
{
}


////////////////////////////////////////////////////////////
Time seconds(float amount)
{
    return Time(static_cast<Int64>(amount * 1000000));
}


////////////////////////////////////////////////////////////
Time milliseconds(Int32 amount)
{
    return Time(static_cast<Int64>(amount) * 1000);
}


////////////////////////////////////////////////////////////
Time microseconds(Int64 amount)
{
    return Time(amount);
}


////////////////////////////////////////////////////////////
bool operator ==(Time left, Time right)
{
    return left.asMicroseconds() == right.asMicroseconds();
}


////////////////////////////////////////////////////////////
bool operator !=(Time left, Time right)
{
    return left.asMicroseconds() != right.asMicroseconds();
}


////////////////////////////////////////////////////////////
bool operator <(Time left, Time right)
{
    return left.asMicroseconds() < right.asMicroseconds();
}


////////////////////////////////////////////////////////////
bool operator >(Time left, Time right)
{
    return left.asMicroseconds() > right.asMicroseconds();
}


////////////////////////////////////////////////////////////
bool operator <=(Time left, Time right)
{
    return left.asMicroseconds() <= right.asMicroseconds();
}


////////////////////////////////////////////////////////////
bool operator >=(Time left, Time right)
{
    return left.asMicroseconds() >= right.asMicroseconds();
}


////////////////////////////////////////////////////////////
Time operator -(Time right)
{
    return microseconds(-right.asMicroseconds());
}


////////////////////////////////////////////////////////////
Time operator +(Time left, Time right)
{
    return microseconds(left.asMicroseconds() + right.asMicroseconds());
}


////////////////////////////////////////////////////////////
Time& operator +=(Time& left, Time right)
{
    return left = left + right;
}


////////////////////////////////////////////////////////////
Time operator -(Time left, Time right)
{
    return microseconds(left.asMicroseconds() - right.asMicroseconds());
}


////////////////////////////////////////////////////////////
Time& operator -=(Time& left, Time right)
{
    return left = left - right;
}


////////////////////////////////////////////////////////////
Time operator *(Time left, float right)
{
    return seconds(left.asSeconds() * right);
}


////////////////////////////////////////////////////////////
Time operator *(Time left, Int64 right)
{
    return microseconds(left.asMicroseconds() * right);
}


////////////////////////////////////////////////////////////
Time operator *(float left, Time right)
{
    return right * left;
}


////////////////////////////////////////////////////////////
Time operator *(Int64 left, Time right)
{
    return right * left;
}


////////////////////////////////////////////////////////////
Time& operator *=(Time& left, float right)
{
    return left = left * right;
}


////////////////////////////////////////////////////////////
Time& operator *=(Time& left, Int64 right)
{
    return left = left * right;
}


////////////////////////////////////////////////////////////
Time operator /(Time left, float right)
{
    return seconds(left.asSeconds() / right);
}


////////////////////////////////////////////////////////////
Time operator /(Time left, Int64 right)
{
    return microseconds(left.asMicroseconds() / right);
}


////////////////////////////////////////////////////////////
Time& operator /=(Time& left, float right)
{
    return left = left / right;
}


////////////////////////////////////////////////////////////
Time& operator /=(Time& left, Int64 right)
{
    return left = left / right;
}

} // namespace sf
