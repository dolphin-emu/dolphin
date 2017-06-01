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

#ifndef SFML_RENDERTEXTURE_HPP
#define SFML_RENDERTEXTURE_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/Export.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RenderTarget.hpp>


namespace sf
{
namespace priv
{
    class RenderTextureImpl;
}

////////////////////////////////////////////////////////////
/// \brief Target for off-screen 2D rendering into a texture
///
////////////////////////////////////////////////////////////
class SFML_GRAPHICS_API RenderTexture : public RenderTarget
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// Constructs an empty, invalid render-texture. You must
    /// call create to have a valid render-texture.
    ///
    /// \see create
    ///
    ////////////////////////////////////////////////////////////
    RenderTexture();

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    virtual ~RenderTexture();

    ////////////////////////////////////////////////////////////
    /// \brief Create the render-texture
    ///
    /// Before calling this function, the render-texture is in
    /// an invalid state, thus it is mandatory to call it before
    /// doing anything with the render-texture.
    /// The last parameter, \a depthBuffer, is useful if you want
    /// to use the render-texture for 3D OpenGL rendering that requires
    /// a depth buffer. Otherwise it is unnecessary, and you should
    /// leave this parameter to false (which is its default value).
    ///
    /// \param width       Width of the render-texture
    /// \param height      Height of the render-texture
    /// \param depthBuffer Do you want this render-texture to have a depth buffer?
    ///
    /// \return True if creation has been successful
    ///
    ////////////////////////////////////////////////////////////
    bool create(unsigned int width, unsigned int height, bool depthBuffer = false);

    ////////////////////////////////////////////////////////////
    /// \brief Enable or disable texture smoothing
    ///
    /// This function is similar to Texture::setSmooth.
    /// This parameter is disabled by default.
    ///
    /// \param smooth True to enable smoothing, false to disable it
    ///
    /// \see isSmooth
    ///
    ////////////////////////////////////////////////////////////
    void setSmooth(bool smooth);

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether the smooth filtering is enabled or not
    ///
    /// \return True if texture smoothing is enabled
    ///
    /// \see setSmooth
    ///
    ////////////////////////////////////////////////////////////
    bool isSmooth() const;

    ////////////////////////////////////////////////////////////
    /// \brief Enable or disable texture repeating
    ///
    /// This function is similar to Texture::setRepeated.
    /// This parameter is disabled by default.
    ///
    /// \param repeated True to enable repeating, false to disable it
    ///
    /// \see isRepeated
    ///
    ////////////////////////////////////////////////////////////
    void setRepeated(bool repeated);

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether the texture is repeated or not
    ///
    /// \return True if texture is repeated
    ///
    /// \see setRepeated
    ///
    ////////////////////////////////////////////////////////////
    bool isRepeated() const;

    ////////////////////////////////////////////////////////////
    /// \brief Generate a mipmap using the current texture data
    ///
    /// This function is similar to Texture::generateMipmap and operates
    /// on the texture used as the target for drawing.
    /// Be aware that any draw operation may modify the base level image data.
    /// For this reason, calling this function only makes sense after all
    /// drawing is completed and display has been called. Not calling display
    /// after subsequent drawing will lead to undefined behavior if a mipmap
    /// had been previously generated.
    ///
    /// \return True if mipmap generation was successful, false if unsuccessful
    ///
    ////////////////////////////////////////////////////////////
    bool generateMipmap();

    ////////////////////////////////////////////////////////////
    /// \brief Activate or deactivate the render-texture for rendering
    ///
    /// This function makes the render-texture's context current for
    /// future OpenGL rendering operations (so you shouldn't care
    /// about it if you're not doing direct OpenGL stuff).
    /// Only one context can be current in a thread, so if you
    /// want to draw OpenGL geometry to another render target
    /// (like a RenderWindow) don't forget to activate it again.
    ///
    /// \param active True to activate, false to deactivate
    ///
    /// \return True if operation was successful, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    bool setActive(bool active = true);

    ////////////////////////////////////////////////////////////
    /// \brief Update the contents of the target texture
    ///
    /// This function updates the target texture with what
    /// has been drawn so far. Like for windows, calling this
    /// function is mandatory at the end of rendering. Not calling
    /// it may leave the texture in an undefined state.
    ///
    ////////////////////////////////////////////////////////////
    void display();

    ////////////////////////////////////////////////////////////
    /// \brief Return the size of the rendering region of the texture
    ///
    /// The returned value is the size that you passed to
    /// the create function.
    ///
    /// \return Size in pixels
    ///
    ////////////////////////////////////////////////////////////
    virtual Vector2u getSize() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get a read-only reference to the target texture
    ///
    /// After drawing to the render-texture and calling Display,
    /// you can retrieve the updated texture using this function,
    /// and draw it using a sprite (for example).
    /// The internal sf::Texture of a render-texture is always the
    /// same instance, so that it is possible to call this function
    /// once and keep a reference to the texture even after it is
    /// modified.
    ///
    /// \return Const reference to the texture
    ///
    ////////////////////////////////////////////////////////////
    const Texture& getTexture() const;

private:

    ////////////////////////////////////////////////////////////
    /// \brief Activate the target for rendering
    ///
    /// This function is called by the base class
    /// everytime it's going to use OpenGL calls.
    ///
    /// \param active True to make the target active, false to deactivate it
    ///
    /// \return True if the function succeeded
    ///
    ////////////////////////////////////////////////////////////
    virtual bool activate(bool active);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    priv::RenderTextureImpl* m_impl;    ///< Platform/hardware specific implementation
    Texture                  m_texture; ///< Target texture to draw on
};

} // namespace sf


#endif // SFML_RENDERTEXTURE_HPP


////////////////////////////////////////////////////////////
/// \class sf::RenderTexture
/// \ingroup graphics
///
/// sf::RenderTexture is the little brother of sf::RenderWindow.
/// It implements the same 2D drawing and OpenGL-related functions
/// (see their base class sf::RenderTarget for more details),
/// the difference is that the result is stored in an off-screen
/// texture rather than being show in a window.
///
/// Rendering to a texture can be useful in a variety of situations:
/// \li precomputing a complex static texture (like a level's background from multiple tiles)
/// \li applying post-effects to the whole scene with shaders
/// \li creating a sprite from a 3D object rendered with OpenGL
/// \li etc.
///
/// Usage example:
///
/// \code
/// // Create a new render-window
/// sf::RenderWindow window(sf::VideoMode(800, 600), "SFML window");
///
/// // Create a new render-texture
/// sf::RenderTexture texture;
/// if (!texture.create(500, 500))
///     return -1;
///
/// // The main loop
/// while (window.isOpen())
/// {
///    // Event processing
///    // ...
///
///    // Clear the whole texture with red color
///    texture.clear(sf::Color::Red);
///
///    // Draw stuff to the texture
///    texture.draw(sprite);  // sprite is a sf::Sprite
///    texture.draw(shape);   // shape is a sf::Shape
///    texture.draw(text);    // text is a sf::Text
///
///    // We're done drawing to the texture
///    texture.display();
///
///    // Now we start rendering to the window, clear it first
///    window.clear();
///
///    // Draw the texture
///    sf::Sprite sprite(texture.getTexture());
///    window.draw(sprite);
///
///    // End the current frame and display its contents on screen
///    window.display();
/// }
/// \endcode
///
/// Like sf::RenderWindow, sf::RenderTexture is still able to render direct
/// OpenGL stuff. It is even possible to mix together OpenGL calls
/// and regular SFML drawing commands. If you need a depth buffer for
/// 3D rendering, don't forget to request it when calling RenderTexture::create.
///
/// \see sf::RenderTarget, sf::RenderWindow, sf::View, sf::Texture
///
////////////////////////////////////////////////////////////
