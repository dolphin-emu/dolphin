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
#include <SFML/Graphics/RenderStates.hpp>
#include <cstddef>


namespace sf
{
////////////////////////////////////////////////////////////
// We cannot use the default constructor here, because it accesses BlendAlpha, which is also global (and dynamically
// initialized). Initialization order of global objects in different translation units is not defined.
const RenderStates RenderStates::Default(BlendMode(
    BlendMode::SrcAlpha, BlendMode::OneMinusSrcAlpha, BlendMode::Add,
    BlendMode::One, BlendMode::OneMinusSrcAlpha, BlendMode::Add));


////////////////////////////////////////////////////////////
RenderStates::RenderStates() :
blendMode(BlendAlpha),
transform(),
texture  (NULL),
shader   (NULL)
{
}


////////////////////////////////////////////////////////////
RenderStates::RenderStates(const Transform& theTransform) :
blendMode(BlendAlpha),
transform(theTransform),
texture  (NULL),
shader   (NULL)
{
}


////////////////////////////////////////////////////////////
RenderStates::RenderStates(const BlendMode& theBlendMode) :
blendMode(theBlendMode),
transform(),
texture  (NULL),
shader   (NULL)
{
}


////////////////////////////////////////////////////////////
RenderStates::RenderStates(const Texture* theTexture) :
blendMode(BlendAlpha),
transform(),
texture  (theTexture),
shader   (NULL)
{
}


////////////////////////////////////////////////////////////
RenderStates::RenderStates(const Shader* theShader) :
blendMode(BlendAlpha),
transform(),
texture  (NULL),
shader   (theShader)
{
}


////////////////////////////////////////////////////////////
RenderStates::RenderStates(const BlendMode& theBlendMode, const Transform& theTransform,
                           const Texture* theTexture, const Shader* theShader) :
blendMode(theBlendMode),
transform(theTransform),
texture  (theTexture),
shader   (theShader)
{
}

} // namespace sf
