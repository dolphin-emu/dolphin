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
#include <SFML/Graphics/RectangleShape.hpp>
#include <cmath>


namespace sf
{
////////////////////////////////////////////////////////////
RectangleShape::RectangleShape(const Vector2f& size)
{
    setSize(size);
}


////////////////////////////////////////////////////////////
void RectangleShape::setSize(const Vector2f& size)
{
    m_size = size;
    update();
}


////////////////////////////////////////////////////////////
const Vector2f& RectangleShape::getSize() const
{
    return m_size;
}


////////////////////////////////////////////////////////////
std::size_t RectangleShape::getPointCount() const
{
    return 4;
}


////////////////////////////////////////////////////////////
Vector2f RectangleShape::getPoint(std::size_t index) const
{
    switch (index)
    {
        default:
        case 0: return Vector2f(0, 0);
        case 1: return Vector2f(m_size.x, 0);
        case 2: return Vector2f(m_size.x, m_size.y);
        case 3: return Vector2f(0, m_size.y);
    }
}

} // namespace sf
