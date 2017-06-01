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
#include <SFML/Graphics/CircleShape.hpp>
#include <cmath>


namespace sf
{
////////////////////////////////////////////////////////////
CircleShape::CircleShape(float radius, std::size_t pointCount) :
m_radius    (radius),
m_pointCount(pointCount)
{
    update();
}


////////////////////////////////////////////////////////////
void CircleShape::setRadius(float radius)
{
    m_radius = radius;
    update();
}


////////////////////////////////////////////////////////////
float CircleShape::getRadius() const
{
    return m_radius;
}


////////////////////////////////////////////////////////////
void CircleShape::setPointCount(std::size_t count)
{
    m_pointCount = count;
    update();
}

////////////////////////////////////////////////////////////
std::size_t CircleShape::getPointCount() const
{
    return m_pointCount;
}


////////////////////////////////////////////////////////////
Vector2f CircleShape::getPoint(std::size_t index) const
{
    static const float pi = 3.141592654f;

    float angle = index * 2 * pi / m_pointCount - pi / 2;
    float x = std::cos(angle) * m_radius;
    float y = std::sin(angle) * m_radius;

    return Vector2f(m_radius + x, m_radius + y);
}

} // namespace sf
