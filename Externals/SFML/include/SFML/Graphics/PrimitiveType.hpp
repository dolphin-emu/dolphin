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

#ifndef SFML_PRIMITIVETYPE_HPP
#define SFML_PRIMITIVETYPE_HPP

namespace sf
{
////////////////////////////////////////////////////////////
/// \ingroup graphics
/// \brief Types of primitives that a sf::VertexArray can render
///
/// Points and lines have no area, therefore their thickness
/// will always be 1 pixel, regardless the current transform
/// and view.
///
////////////////////////////////////////////////////////////
enum PrimitiveType
{
    Points,        ///< List of individual points
    Lines,         ///< List of individual lines
    LineStrip,     ///< List of connected lines, a point uses the previous point to form a line
    Triangles,     ///< List of individual triangles
    TriangleStrip, ///< List of connected triangles, a point uses the two previous points to form a triangle
    TriangleFan,   ///< List of connected triangles, a point uses the common center and the previous point to form a triangle
    Quads,         ///< List of individual quads (deprecated, don't work with OpenGL ES)

    // Deprecated names
    LinesStrip     = LineStrip,     ///< \deprecated Use LineStrip instead
    TrianglesStrip = TriangleStrip, ///< \deprecated Use TriangleStrip instead
    TrianglesFan   = TriangleFan    ///< \deprecated Use TriangleFan instead
};

} // namespace sf


#endif // SFML_PRIMITIVETYPE_HPP
