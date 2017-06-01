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

#ifndef SFML_RENDERTEXTUREIMPLFBO_HPP
#define SFML_RENDERTEXTUREIMPLFBO_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/RenderTextureImpl.hpp>
#include <SFML/Window/Context.hpp>
#include <SFML/Window/GlResource.hpp>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Specialization of RenderTextureImpl using the
///        FrameBuffer Object OpenGL extension
///
////////////////////////////////////////////////////////////
class RenderTextureImplFBO : public RenderTextureImpl, GlResource
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    RenderTextureImplFBO();

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~RenderTextureImplFBO();

    ////////////////////////////////////////////////////////////
    /// \brief Check whether the system supports FBOs or not
    ///
    /// \return True if FBO render textures are supported
    ///
    ////////////////////////////////////////////////////////////
    static bool isAvailable();

private:

    ////////////////////////////////////////////////////////////
    /// \brief Create the render texture implementation
    ///
    /// \param width       Width of the texture to render to
    /// \param height      Height of the texture to render to
    /// \param textureId   OpenGL identifier of the target texture
    /// \param depthBuffer Is a depth buffer requested?
    ///
    /// \return True if creation has been successful
    ///
    ////////////////////////////////////////////////////////////
    virtual bool create(unsigned int width, unsigned int height, unsigned int textureId, bool depthBuffer);

    ////////////////////////////////////////////////////////////
    /// \brief Activate or deactivate the render texture for rendering
    ///
    /// \param active True to activate, false to deactivate
    ///
    /// \return True on success, false on failure
    ///
    ////////////////////////////////////////////////////////////
    virtual bool activate(bool active);

    ////////////////////////////////////////////////////////////
    /// \brief Update the pixels of the target texture
    ///
    /// \param textureId OpenGL identifier of the target texture
    ///
    ////////////////////////////////////////////////////////////
    virtual void updateTexture(unsigned textureId);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    Context*     m_context;     ///< Needs a separate OpenGL context for not messing up the other ones
    unsigned int m_frameBuffer; ///< OpenGL frame buffer object
    unsigned int m_depthBuffer; ///< Optional depth buffer attached to the frame buffer
};

} // namespace priv

} // namespace sf


#endif // SFML_RENDERTEXTUREIMPLFBO_HPP
