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
#include <SFML/Graphics/RenderTextureImplDefault.hpp>
#include <SFML/Graphics/GLCheck.hpp>
#include <SFML/Graphics/TextureSaver.hpp>
#include <SFML/Window/Context.hpp>
#include <SFML/System/Err.hpp>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
RenderTextureImplDefault::RenderTextureImplDefault() :
m_context(0),
m_width  (0),
m_height (0)
{

}


////////////////////////////////////////////////////////////
RenderTextureImplDefault::~RenderTextureImplDefault()
{
    // Destroy the context
    delete m_context;
}


////////////////////////////////////////////////////////////
bool RenderTextureImplDefault::create(unsigned int width, unsigned int height, unsigned int, bool depthBuffer)
{
    // Store the dimensions
    m_width = width;
    m_height = height;

    // Create the in-memory OpenGL context
    m_context = new Context(ContextSettings(depthBuffer ? 32 : 0), width, height);

    return true;
}


////////////////////////////////////////////////////////////
bool RenderTextureImplDefault::activate(bool active)
{
    return m_context->setActive(active);
}


////////////////////////////////////////////////////////////
void RenderTextureImplDefault::updateTexture(unsigned int textureId)
{
    // Make sure that the current texture binding will be preserved
    priv::TextureSaver save;

    // Copy the rendered pixels to the texture
    glCheck(glBindTexture(GL_TEXTURE_2D, textureId));
    glCheck(glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, m_width, m_height));
}

} // namespace priv

} // namespace sf
